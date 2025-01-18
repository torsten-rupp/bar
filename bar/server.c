/***********************************************************************\
*
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

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
#include "crypt.h"
#include "archive.h"
#include "storage.h"
#include "index/index.h"
#include "index/index_storages.h"
#include "continuous.h"
#include "jobs.h"
#include "server_io.h"
#include "connector.h"
#include "bar.h"

#include "commands_create.h"
#include "commands_test.h"
#include "commands_restore.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define _NO_SESSION_ID
#define _SIMULATOR
#define _SIMULATE_PURGE            // simulate purge entities/storages only

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE                         1024                     // number of session key bits

#define MAX_NETWORK_CLIENT_THREADS               3                        // number of threads for a client
// TODO:
//#define LOCK_TIMEOUT                             (10L*60L*MS_PER_SECOND)  // general lock timeout [ms]
#define LOCK_TIMEOUT                             (30L*MS_PER_SECOND)  // general lock timeout [ms]
#define CLIENT_TIMEOUT                           (30L*MS_PER_SECOND)      // client timeout [ms]

#define SLAVE_DEBUG_LEVEL                        1
#define SLAVE_COMMAND_TIMEOUT                    (10L*MS_PER_SECOND)

#define AUTHORIZATION_PENALITY_TIME              500                      // delay processing by failCount^2*n [ms]
#define MAX_AUTHORIZATION_PENALITY_TIME          30000                    // max. penality time [ms]
#define MAX_AUTHORIZATION_HISTORY_KEEP_TIME      30000                    // max. time to keep entries in authorization fail history [ms]
#define MAX_AUTHORIZATION_FAIL_HISTORY           64                       // max. length of history of authorization fail clients
#define MAX_ABORT_COMMAND_IDS                    512                      // max. aborted command ids history

#define DEFAULT_PAIRING_MASTER_TIMEOUT           (10*S_PER_MINUTE)        // default timeout pairing new master [s]

// sleep times [s]
#define SLEEP_TIME_PAIRING_THREAD                ( 1*S_PER_MINUTE)
#define SLEEP_TIME_SCHEDULER_THREAD              ( 1*S_PER_MINUTE)
#define SLEEP_TIME_PAUSE_THREAD                  ( 1*S_PER_MINUTE)
#define SLEEP_TIME_INDEX_THREAD                  ( 1*S_PER_MINUTE)
#define SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD      (10*S_PER_MINUTE)
#define SLEEP_TIME_PERSISTENCE_THREAD            (10*S_PER_MINUTE)

// id none
#define ID_NONE                                  0

// keep all
#define KEEP_ALL                                 -1

// forever age
#define AGE_FOREVER                              -1

/***************************** Datatypes *******************************/

// server states
typedef enum
{
  SERVER_STATE_RUNNING,
  SERVER_STATE_PAUSE,
  SERVER_STATE_SUSPENDED,
} ServerStates;

// pairing modes
typedef enum
{
  PAIRING_MODE_NONE,
  PAIRING_MODE_AUTO,
  PAIRING_MODE_MANUAL
} PairingModes;

// entity
typedef struct EntityNode
{
  LIST_NODE_HEADER(struct EntityNode);

  IndexId               entityId;
  String                jobUUID;
  ArchiveTypes          archiveType;
  uint64                createdDateTime;
  uint64                size;
  ulong                 totalEntryCount;
  uint64                totalEntrySize;
  bool                  lockedFlag;
  const PersistenceNode *persistenceNode;
} EntityNode;

typedef struct
{
  LIST_HEADER(EntityNode);
} EntityList;

// aggregate info
typedef struct
{
  struct
  {
    uint normal;
    uint full;
    uint incremental;
    uint differential;
    uint continuous;
  }      executionCount;
  struct
  {
    uint64 normal;
    uint64 full;
    uint64 incremental;
    uint64 differential;
    uint64 continuous;
  }      averageDuration;
  uint   totalEntityCount;
  uint   totalStorageCount;
  uint64 totalStorageSize;
  uint   totalEntryCount;
  uint64 totalEntrySize;
} AggregateInfo;

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

// authorization states
typedef enum
{
  AUTHORIZATION_STATE_WAITING = 1 << 0,
  AUTHORIZATION_STATE_CLIENT  = 1 << 1,
  AUTHORIZATION_STATE_MASTER  = 1 << 2,
  AUTHORIZATION_STATE_FAIL    = 1 << 3,
} AuthorizationStates;
typedef ulong AuthorizationStateSet;

// authorization fail node
typedef struct AuthorizationFailNode
{
  LIST_NODE_HEADER(struct AuthorizationFailNode);

  String clientName;                                    // client nam
  uint   count;                                         // number of authentification failures
  uint64 lastTimestamp;                                 // timestamp last failture [us]
} AuthorizationFailNode;

// authorization fail list
typedef struct
{
  LIST_HEADER(AuthorizationFailNode);
} AuthorizationFailList;

// server command info list
typedef struct CommandInfoNode
{
  LIST_NODE_HEADER(struct CommandInfoNode);

  uint        id;
  IndexHandle *indexHandle;
} CommandInfoNode;

typedef struct
{
  LIST_HEADER(CommandInfoNode);
} CommandInfoList;

// client info
typedef struct
{
  Semaphore             lock;

  uint64                connectTimestamp;
  bool                  quitFlag;

  // authorization data
  AuthorizationStates   authorizationState;
  AuthorizationFailNode *authorizationFailNode;

  // commands
  CommandInfoList       commandInfoList;                   // running command list
  RingBuffer            abortedCommandIds;                 // aborted command ids
  uint                  abortedCommandIdStart;

  // i/o
  ServerIO              io;

  // command processing
  ThreadPoolNode        *threads[MAX_NETWORK_CLIENT_THREADS];
  MsgQueue              commandQueue;

  // current list settings
  EntryList             includeEntryList;
  PatternList           excludePatternList;
  JobOptions            jobOptions;
  DirectoryInfoList     directoryInfoList;

  // current index id settings
  Array                 indexIdArray;                      // ids of uuid/entity/storage ids to list/restore
  Array                 entryIdArray;                      // ids of entries to restore
} ClientInfo;

// client node
typedef struct ClientNode
{
  LIST_NODE_HEADER(struct ClientNode);

  ClientInfo clientInfo;
} ClientNode;

// client list
typedef struct
{
  LIST_HEADER(ClientNode);

  Semaphore lock;
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
  AuthorizationStateSet authorizationStateSet;
  uint                  id;
  StringMap             argumentMap;
} Command;

/***********************************************************************\
* Name   : TransferInfoFunction
* Purpose: move info call-back
* Input  : error       - error code
*          storageId   - storage id
*          storageName - storage name
*          n           - bytes moved [bytes]
*          size        - storage size [bytes]
*          doneCount   - storage count done
*          doneSize    - moved size done [bytes]
*          totalCount  - total number of storages
*          totalSize   - total size of storages [bytes]
*          userData    - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*TransferInfoFunction)(IndexId     storageId,
                                    ConstString storageName,
                                    uint64      n,
                                    uint64      size,
                                    uint        doneCount,
                                    uint64      doneSize,
                                    uint        totalCount,
                                    uint64      totalSize,
                                    void        *userData
                                   );

typedef struct JobScheduleNode
{
  LIST_NODE_HEADER(struct JobScheduleNode);

  // job
  String             jobName;
  String             jobUUID;

  // settings
  String             scheduleUUID;
  ScheduleDate       date;
  ScheduleWeekDaySet weekDaySet;
  ScheduleTime       time;
  ArchiveTypes       archiveType;
  uint               interval;
  ScheduleTime       beginTime,endTime;
  String             customText;
  bool               testCreatedArchives;
  bool               noStorage;
  bool               enabled;
  uint64             lastExecutedDateTime;

  // running info
  bool               active;
  uint64             lastScheduleCheckDateTime;
} JobScheduleNode;

typedef struct
{
  LIST_HEADER(JobScheduleNode);
} JobScheduleList;

/***************************** Variables *******************************/
LOCAL String                hostName;

LOCAL ClientList            clientList;                  // list with clients
LOCAL AuthorizationFailList authorizationFailList;       // list with failed client authorizations
LOCAL Semaphore             delayThreadTrigger;
LOCAL Thread                jobThread;                   // thread executing jobs create/restore
LOCAL Thread                schedulerThread;             // thread for scheduling jobs
LOCAL Semaphore             schedulerThreadTrigger;
LOCAL Thread                pauseThread;
LOCAL Thread                pairingThread;               // thread for pairing master/slaves
LOCAL Semaphore             pairingThreadTrigger;
LOCAL Semaphore             updateIndexThreadTrigger;
LOCAL Thread                updateIndexThread;           // thread to add/update index
LOCAL Semaphore             autoIndexThreadTrigger;
LOCAL Thread                autoIndexThread;             // thread to collect BAR files for auto-index
LOCAL Thread                persistenceThread;           // thread to purge expired/move archive files
LOCAL Semaphore             persistenceThreadTrigger;

LOCAL Semaphore             serverStateLock;
LOCAL ServerStates          serverState;                 // current server state
LOCAL struct
      {
        bool create;
        bool storage;
        bool restore;
        bool indexUpdate;
        bool indexMaintenance;
      } pauseFlags;                                      // TRUE iff pause
LOCAL uint64                pauseEndDateTime;            // pause end date/time [s]
LOCAL struct
{
  Semaphore    lock;
  PairingModes pairingMode;
  TimeoutInfo  pairingTimeoutInfo;                       // master pairing timeout info
  String       name;                                     // new master name
  CryptHash    uuidHash;                                 // new master UUID hash
} newMaster;
LOCAL IndexHandle           indexHandle;                 // index handle
LOCAL uint64                intermediateMaintenanceDateTime;  // intermediate maintenance date/time
LOCAL bool                  quitFlag;                    // TRUE iff quit requested

#ifdef SIMULATE_PURGE
  Array simulatedPurgeEntityIdArray;
#endif /* SIMULATE_PURGE */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

LOCAL void deleteClient(ClientNode *clientNode);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : setQuit
* Purpose: quit application
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void setQuit(void)
{
  quitFlag = TRUE;
  Semaphore_signalModified(&delayThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : isQuit
* Purpose: check if quit application
* Input  : -
* Output : -
* Return : TRUE iff quit
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isQuit(void)
{
  return quitFlag;
}

/***********************************************************************\
* Name   : delayThread
* Purpose: delay thread and check quit flag
* Input  : sleepTime - sleep time [s]
*          trigger   - trigger semaphore (can be NULL for delay default
*                      trigger)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayThread(uint sleepTime, Semaphore *trigger)
{
  TimeoutInfo timeoutInfo;

  // use delay trigger as a default
  if (trigger == NULL) trigger = &delayThreadTrigger;

  Misc_initTimeout(&timeoutInfo,sleepTime*MS_PER_SECOND);
  SEMAPHORE_LOCKED_DO(trigger,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    while (   !isQuit()
           && !Misc_isTimeout(&timeoutInfo)
           && !Semaphore_waitModified(trigger,Misc_getRestTimeout(&timeoutInfo,5000))
          )
    {
    }
  }
  Misc_doneTimeout(&timeoutInfo);
}

/***********************************************************************\
* Name   : parseMaintenanceDateTime
* Purpose: parse schedule date/time
* Input  : maintenanceNode - maintenance node variable
*          date            - date string (<year|*>-<month|*>-<day|*>)
*          weekDays        - week days string (<day>,...)
*          beginTime       - begin time string <hour|*>:<minute|*>
*          endTime         - end time string <hour|*>:<minute|*>
* Output : maintenanceNode - maintenance node
* Return : ERROR_NONE or error code
* Notes  : month names: jan, feb, mar, apr, may, jun, jul, aug, sep, oct
*          nov, dec
*          week day names: mon, tue, wed, thu, fri, sat, sun
\***********************************************************************/

LOCAL Errors parseMaintenanceDateTime(MaintenanceNode *maintenanceNode,
                                      ConstString     date,
                                      ConstString     weekDays,
                                      ConstString     beginTime,
                                      ConstString     endTime
                                     )
{
  Errors error;
  String s0,s1,s2;

  assert(maintenanceNode != NULL);
  assert(date != NULL);
  assert(weekDays != NULL);
  assert(beginTime != NULL);
  assert(endTime != NULL);

  error = ERROR_NONE;

  // init variables
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();

  // parse date
  if      (String_parse(date,STRING_BEGIN,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (   !Configuration_parseDateNumber(s0,&maintenanceNode->date.year )
        || !Configuration_parseDateMonth (s1,&maintenanceNode->date.month)
        || !Configuration_parseDateNumber(s2,&maintenanceNode->date.day  )
       )
    {
      error = ERROR_PARSE_DATE;
    }
  }
  else
  {
    error = ERROR_PARSE_DATE;
  }

  // parse week days
  if (!Configuration_parseWeekDaySet(String_cString(weekDays),&maintenanceNode->weekDaySet))
  {
    error = ERROR_PARSE_WEEKDAYS;
  }

  // parse begin/end time
  if (String_parse(beginTime,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (   !Configuration_parseTimeNumber(s0,&maintenanceNode->beginTime.hour  )
        || !Configuration_parseTimeNumber(s1,&maintenanceNode->beginTime.minute)
       )
    {
      error = ERROR_PARSE_TIME;
    }
  }
  else
  {
    error = ERROR_PARSE_TIME;
  }
  if (String_parse(endTime,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (   !Configuration_parseTimeNumber(s0,&maintenanceNode->endTime.hour  )
        || !Configuration_parseTimeNumber(s1,&maintenanceNode->endTime.minute)
       )
    {
      error = ERROR_PARSE_TIME;
    }
  }
  else
  {
    error = ERROR_PARSE_TIME;
  }

  // free resources
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  return error;
}


/***********************************************************************\
* Name   : parseServerType
* Purpose: parse server type
* Input  : name     - file|ftp|ssh|webdav
*          userData - user data (not used)
* Output : serverType - server type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseServerType(const char *name, ServerTypes *serverType, void *userData)
{
  assert(name != NULL);
  assert(serverType != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase(name,"FILE"  )) (*serverType) = SERVER_TYPE_FILE;
  else if (stringEqualsIgnoreCase(name,"FTP"   )) (*serverType) = SERVER_TYPE_FTP;
  else if (stringEqualsIgnoreCase(name,"SSH"   )) (*serverType) = SERVER_TYPE_SSH;
  else if (stringEqualsIgnoreCase(name,"WEBDAV")) (*serverType) = SERVER_TYPE_WEBDAV;
  else return FALSE;

  return TRUE;
}

/***********************************************************************\
* Name   : parseScheduleTime
* Purpose: parse schedule time
* Input  : scheduleTime - schedule time variable
*          string       - time string <hour|*>:<minute|*>
* Output : scheduleTime - schedule time
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors parseScheduleTime(ScheduleTime *scheduleTime,
                               ConstString  string
                              )
{
  Errors error;
  String s0,s1;

  assert(scheduleTime != NULL);
  assert(string != NULL);

  error = ERROR_NONE;

  // init variables
  s0 = String_new();
  s1 = String_new();

  // parse time
  if (String_parse(string,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (   !Configuration_parseTimeNumber(s0,&scheduleTime->hour  )
        || !Configuration_parseTimeNumber(s1,&scheduleTime->minute)
       )
    {
      error = ERROR_PARSE_TIME;
    }
  }
  else
  {
    error = ERROR_PARSE_TIME;
  }

  // free resources
  String_delete(s1);
  String_delete(s0);

  return error;
}

/***********************************************************************\
* Name   : parseScheduleDateTime
* Purpose: parse schedule date/time
* Input  : scheduleDate       - schedule date variable
*          scheduleWeekDaySet - schedule week day set variable
*          scheduleTime       - schedule time variable
*          dateString         - date string (<year|*>-<month|*>-<day|*>)
*          weekDaysString     - week days string (<day>,...)
*          timeString         - time string <hour|*>:<minute|*>
* Output : scheduleDate       - schedule date
*          scheduleWeekDaySet - schedule week day set
*          scheduleTime       - schedule time
* Return : ERROR_NONE or error code
* Notes  : month names: jan, feb, mar, apr, may, jun, jul, aug, sep, oct
*          nov, dec
*          week day names: mon, tue, wed, thu, fri, sat, sun
\***********************************************************************/

LOCAL Errors parseScheduleDateTime(ScheduleDate       *scheduleDate,
                                   ScheduleWeekDaySet *scheduleWeekDaySet,
                                   ScheduleTime       *scheduleTime,
                                   ConstString        dateString,
                                   ConstString        weekDaysString,
                                   ConstString        timeString
                                  )
{
  Errors error;
  String s0,s1,s2;

  assert(scheduleDate != NULL);
  assert(scheduleWeekDaySet != NULL);
  assert(scheduleTime != NULL);
  assert(dateString != NULL);
  assert(weekDaysString != NULL);
  assert(timeString != NULL);

  error = ERROR_NONE;

  // init variables
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();

  // parse date
  if (error == ERROR_NONE)
  {
    if      (String_parse(dateString,STRING_BEGIN,"%S-%S-%S",NULL,s0,s1,s2))
    {
      if (   !Configuration_parseDateNumber(s0,&scheduleDate->year )
          || !Configuration_parseDateMonth (s1,&scheduleDate->month)
          || !Configuration_parseDateNumber(s2,&scheduleDate->day  )
         )
      {
        error = ERROR_PARSE_DATE;
      }
    }
    else
    {
      error = ERROR_PARSE_DATE;
    }
  }

  // parse week days
  if (error == ERROR_NONE)
  {
    if (!Configuration_parseWeekDaySet(String_cString(weekDaysString),scheduleWeekDaySet))
    {
      error = ERROR_PARSE_WEEKDAYS;
    }
  }

  // parse time
  if (error == ERROR_NONE)
  {
    error = parseScheduleTime(scheduleTime,timeString);
  }

  // free resources
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  return error;
}

#if 0
not used
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
#endif

/***********************************************************************\
* Name   : getClientInfoString
* Purpose: get client info string
* Input  : clientInfo - client info
*          buffer     - info string variable
*          bufferSize - buffer size
* Output : buffer - info string
* Return : buffer - info string
* Notes  : -
\***********************************************************************/

LOCAL const char *getClientInfoString(ClientInfo *clientInfo, char *buffer, uint bufferSize)
{
  assert(clientInfo != NULL);
  assert(buffer != NULL);
  assert(bufferSize > 0);

  switch (clientInfo->io.type)
  {
    case SERVER_IO_TYPE_NONE:
      stringFormat(buffer,bufferSize,"'unknown'");
      break;
    case SERVER_IO_TYPE_BATCH:
      stringFormat(buffer,bufferSize,"'batch'");
      break;
    case SERVER_IO_TYPE_NETWORK:
      stringFormat(buffer,bufferSize,"'%s:%d'",String_cString(clientInfo->io.network.name),clientInfo->io.network.port);
      if (clientInfo->authorizationState == AUTHORIZATION_STATE_MASTER)
      {
        stringAppend(buffer,bufferSize," (master)");
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return buffer;
}

/***********************************************************************\
* Name   : storageVolumeRequest
* Purpose: volume request call-back
* Input  : type         - storage volume request type
*          volumeNumber - volume number
*          message      - message or NULL
*          userData     - user data: job node
* Output : -
* Return : volume request result; see StorageVolumeRequestResults
* Notes  : -
\***********************************************************************/

LOCAL StorageVolumeRequestResults storageVolumeRequest(StorageVolumeRequestTypes type,
                                                       uint                      volumeNumber,
                                                       const char                *message,
                                                       void                      *userData
                                                      )
{
  JobNode                     *jobNode = (JobNode*)userData;
  StorageVolumeRequestResults storageVolumeRequestResult;

  assert(jobNode != NULL);

//TODO: use type?
  UNUSED_VARIABLE(type);
//TODO: use message
  UNUSED_VARIABLE(message);

  storageVolumeRequestResult = STORAGE_VOLUME_REQUEST_RESULT_NONE;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // request volume
    assert(jobNode->jobState == JOB_STATE_RUNNING);
    jobNode->runningInfo.volumeRequest       = VOLUME_REQUEST_INITIAL;
    jobNode->runningInfo.volumeRequestNumber = volumeNumber;
    jobNode->volumeUnloadFlag    = FALSE;
    jobNode->volumeNumber        = 0;
    Job_listSignalModifed();

    // wait until volume is available or job is aborted
    storageVolumeRequestResult = STORAGE_VOLUME_REQUEST_RESULT_NONE;
    do
    {
      Job_listWaitModifed(LOCK_TIMEOUT);

      if      (jobNode->requestedAbortFlag)
      {
        storageVolumeRequestResult = STORAGE_VOLUME_REQUEST_RESULT_ABORTED;
      }
      else if (jobNode->volumeUnloadFlag)
      {
        storageVolumeRequestResult = STORAGE_VOLUME_REQUEST_RESULT_UNLOAD;
        jobNode->volumeUnloadFlag = FALSE;
      }
      else if (jobNode->volumeNumber == jobNode->runningInfo.volumeRequestNumber)
      {
        storageVolumeRequestResult = STORAGE_VOLUME_REQUEST_RESULT_OK;
      }
    }
    while (   !isQuit()
           && (storageVolumeRequestResult == STORAGE_VOLUME_REQUEST_RESULT_NONE)
          );

    // clear request volume
    jobNode->runningInfo.volumeRequest = VOLUME_REQUEST_NONE;
    Job_listSignalModifed();
  }

  return storageVolumeRequestResult;
}

/***********************************************************************\
* Name   : beginPairingMaster
* Purpose: begin pairing master (if not already started)
* Input  : timeout - timeout [s]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void beginPairingMaster(uint timeout, PairingModes pairingMode)
{
  ClientNode *clientNode,*disconnectClientNode;

  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (newMaster.pairingMode == PAIRING_MODE_NONE)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 (pairingMode == PAIRING_MODE_AUTO)
                   ? "Initialize auto pairing master"
                   : "Initialize pairing master"
                );
    }

    // start pairing new master
    newMaster.pairingMode = pairingMode;
    String_clear(newMaster.name);
    Misc_restartTimeout(&newMaster.pairingTimeoutInfo,timeout*MS_PER_S);
  }

  // disconnect all currently connected masters for re-pairing
  SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (IS_SET(clientNode->clientInfo.authorizationState,AUTHORIZATION_STATE_MASTER))
      {
        disconnectClientNode = clientNode;
        clientNode = List_remove(&clientList,disconnectClientNode);
        deleteClient(disconnectClientNode);
      }
      else
      {
        clientNode = clientNode->next;
      }
    }
  }
}

/***********************************************************************\
* Name   : endPairingMaster
* Purpose: end pairing master (if started)
* Input  : name     - master name
*          uuidHash - master UUID hash
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors endPairingMaster(ConstString name, const CryptHash *uuidHash)
{
  bool   modifiedFlag;
  Errors error;

  assert(name != NULL);
  assert(uuidHash != NULL);

  modifiedFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // set/clear paired master
    if (!String_isEmpty(name))
    {
      modifiedFlag =    !String_equals(globalOptions.masterInfo.name,name)
                     || !Configuration_equalsHash(&globalOptions.masterInfo.uuidHash,uuidHash);
      String_set(globalOptions.masterInfo.name,name);
      if (!Configuration_setHash(&globalOptions.masterInfo.uuidHash,uuidHash))
      {
        Semaphore_unlock(&newMaster.lock);
        return ERROR_INSUFFICIENT_MEMORY;
      }
      if (modifiedFlag)
      {
        error = Configuration_update();
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&newMaster.lock);
          return error;
        }
      }
      logMessage(NULL,  // logHandle,
                LOG_TYPE_ALWAYS,
                "Paired master '%s'",
                String_cString(globalOptions.masterInfo.name)
               );
    }
    else
    {
      modifiedFlag = !String_isEmpty(globalOptions.masterInfo.name);
      String_clear(globalOptions.masterInfo.name);
      Configuration_clearHash(&globalOptions.masterInfo.uuidHash);
      if (modifiedFlag)
      {
        error = Configuration_update();
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&newMaster.lock);
          return error;
        }
      }
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Cleared paired master"
                );
    }

    // stop pairing
    newMaster.pairingMode = PAIRING_MODE_NONE;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : abortPairingMaster
* Purpose: abort pairing master (if started)
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void abortPairingMaster(void)
{
  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (newMaster.pairingMode != PAIRING_MODE_NONE)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 (newMaster.pairingMode == PAIRING_MODE_AUTO)
                   ? "Aborted auto pairing master"
                   : "Aborted pairing master"
                );

      newMaster.pairingMode = PAIRING_MODE_NONE;
    }
  }
}

/***********************************************************************\
* Name   : clearPairedMaster
* Purpose: clear paired master
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearPairedMaster(void)
{
  bool       clearedFlag;
  ClientNode *clientNode,*disconnectClientNode;
  Errors     error;

  // clear paired master
  clearedFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    assert(newMaster.pairingMode == PAIRING_MODE_NONE);

    if (!String_isEmpty(globalOptions.masterInfo.name))
    {
      String_clear(globalOptions.masterInfo.name);
      Configuration_clearHash(&globalOptions.masterInfo.uuidHash);
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Cleared paired master"
                );

      clearedFlag = TRUE;
    }
  }

  if (clearedFlag)
  {
    // disconnect all currently connected masters
    SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      clientNode = clientList.head;
      while (clientNode != NULL)
      {
        if (IS_SET(clientNode->clientInfo.authorizationState,AUTHORIZATION_STATE_MASTER))
        {
          disconnectClientNode = clientNode;
          clientNode = List_remove(&clientList,disconnectClientNode);
          deleteClient(disconnectClientNode);
        }
        else
        {
          clientNode = clientNode->next;
        }
      }
    }

    // update config file
    error = Configuration_update();
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : isSlavePaired
* Purpose: check if a slave is paired
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff slave is paired
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isSlavePaired(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return (jobNode->slaveState == SLAVE_STATE_PAIRED);
}

/***********************************************************************\
* Name   : getSlaveStateText
* Purpose: get slave state text
* Input  : slaveState - slave state
* Output : -
* Return : slave state text
* Notes  : -
\***********************************************************************/

LOCAL const char *getSlaveStateText(SlaveStates slaveState)
{
  const char *stateText;

  stateText = "UNKNOWN";
  switch (slaveState)
  {
    case SLAVE_STATE_OFFLINE:
      stateText = "OFFLINE";
      break;
    case SLAVE_STATE_ONLINE:
      stateText = "ONLINE";
      break;
    case SLAVE_STATE_WRONG_MODE:
      stateText = "WRONG_MODE";
      break;
    case SLAVE_STATE_WRONG_PROTOCOL_VERSION:
      stateText = "WRONG_PROTOCOL_VERSION";
      break;
    case SLAVE_STATE_PAIRED:
      stateText = "PAIRED";
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
* Name   : initAggregateInfo
* Purpose: init aggregate info
* Input  : aggregateInfo - aggregate info variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initAggregateInfo(AggregateInfo *aggregateInfo)
{
  assert(aggregateInfo != NULL);

  UNUSED_VARIABLE(aggregateInfo);
}

/***********************************************************************\
* Name   : doneAggregateInfo
* Purpose: done aggregate info
* Input  : aggregateInfo - aggregate info variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneAggregateInfo(AggregateInfo *aggregateInfo)
{
  assert(aggregateInfo != NULL);

  UNUSED_VARIABLE(aggregateInfo);
}

/***********************************************************************\
* Name   : getAggregateInfo
* Purpose: get aggregate info for job/sched7ule
* Input  : aggregateInfo - aggregate info variable
*          jobUUID    - job UUID
*          entityUUID - entity UUID or NULL
* Output : aggregateInfo - aggregate info
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getAggregateInfo(AggregateInfo *aggregateInfo,
                            ConstString   jobUUID,
                            ConstString   entityUUID
                           )
{
  assert(aggregateInfo != NULL);
  assert(jobUUID != NULL);

  // init variables
  aggregateInfo->executionCount.normal        = 0L;
  aggregateInfo->executionCount.full          = 0L;
  aggregateInfo->executionCount.incremental   = 0L;
  aggregateInfo->executionCount.differential  = 0L;
  aggregateInfo->executionCount.continuous    = 0L;
  aggregateInfo->averageDuration.normal       = 0LL;
  aggregateInfo->averageDuration.full         = 0LL;
  aggregateInfo->averageDuration.incremental  = 0LL;
  aggregateInfo->averageDuration.differential = 0LL;
  aggregateInfo->averageDuration.continuous   = 0LL;
  aggregateInfo->totalEntityCount             = 0L;
  aggregateInfo->totalStorageCount            = 0L;
  aggregateInfo->totalStorageSize             = 0LL;
  aggregateInfo->totalEntryCount              = 0L;
  aggregateInfo->totalEntrySize               = 0LL;

  // get job info (if possible)
  if (Index_isAvailable())
  {
    (void)Index_findUUID(&indexHandle,
                         String_cString(jobUUID),
                         String_cString(entityUUID),
                         NULL,  // uuidId
                         &aggregateInfo->executionCount.normal,
                         &aggregateInfo->executionCount.full,
                         &aggregateInfo->executionCount.incremental,
                         &aggregateInfo->executionCount.differential,
                         &aggregateInfo->executionCount.continuous,
                         &aggregateInfo->averageDuration.normal,
                         &aggregateInfo->averageDuration.full,
                         &aggregateInfo->averageDuration.incremental,
                         &aggregateInfo->averageDuration.differential,
                         &aggregateInfo->averageDuration.continuous,
                         &aggregateInfo->totalEntityCount,
                         &aggregateInfo->totalStorageCount,
                         &aggregateInfo->totalStorageSize,
                         &aggregateInfo->totalEntryCount,
                         &aggregateInfo->totalEntrySize
                        );
  }
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : pairingThreadCode
* Purpose: master/slave pairing thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pairingThreadCode(void)
{
  /***********************************************************************\
  * Name   : updateSlaveState
  * Purpose: update slave state in job
  * Input  : slaveNode        - slave node
  *          slaveState       - slave state
  *          slaveTLS         - TRUE iff slave TLS connection
  *          slaveInsecureTLS - TRUE iff insecure slave TLS connection
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void updateSlaveState(const SlaveNode *slaveNode, SlaveStates slaveState, bool slaveTLS, bool slaveInsecureTLS);
  void updateSlaveState(const SlaveNode *slaveNode, SlaveStates slaveState, bool slaveTLS, bool slaveInsecureTLS)
  {
    JobNode *jobNode;

    // update slave state in job
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      JOB_LIST_ITERATE(jobNode)
      {
        if (   (jobNode->job.slaveHost.port == slaveNode->port)
            && String_equals(jobNode->job.slaveHost.name,slaveNode->name)
           )
        {
          jobNode->slaveState       = slaveState;
          jobNode->slaveTLS         = slaveTLS;
          jobNode->slaveInsecureTLS = slaveInsecureTLS;
        }
      }
    }
  }

  JobNode     *jobNode;
  SlaveNode   *slaveNode;
  Errors      error;
  bool        anyOfflineFlag,anyUnpairedFlag;
  uint        slaveProtocolVersionMajor;
  ServerModes slaveServerMode;
  FileHandle  fileHandle;
  FileInfo    fileInfo;
  String      line;
  uint64      pairingStopDateTime;
  bool        clearPairing;

  line = String_new();
  while (!isQuit())
  {
    switch (globalOptions.serverMode)
    {
      case SERVER_MODE_MASTER:
        // try pairing all slaves
        anyOfflineFlag  = FALSE;
        anyUnpairedFlag = FALSE;
        JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          // disconnect shutdown slaves
          JOB_SLAVE_LIST_ITERATE(slaveNode)
          {
            if (Connector_isShutdown(&slaveNode->connectorInfo))
            {
              // disconnect slave
              Connector_disconnect(&slaveNode->connectorInfo);

              // update slave state in job
              updateSlaveState(slaveNode,SLAVE_STATE_OFFLINE,FALSE,FALSE);

              // log info
              if (slaveNode->authorizedFlag)
              {
                logMessage(NULL,  // logHandle,
                           LOG_TYPE_INFO,
                           "Slave %s:%d disconnected",
                           String_cString(slaveNode->name),slaveNode->port
                          );
              }

              slaveNode->authorizedFlag = FALSE;
            }
          }

          // update slave list
          JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
          {
            // collect new slaves
            JOB_LIST_ITERATE(jobNode)
            {
              if (Job_isRemote(jobNode))
              {
                slaveNode = JOB_SLAVE_LIST_FIND(slaveNode,
                                                   (slaveNode->port == jobNode->job.slaveHost.port)
                                                && String_equals(slaveNode->name,jobNode->job.slaveHost.name)
                                               );
                if (slaveNode == NULL)
                {
                  slaveNode = Job_addSlave(jobNode->job.slaveHost.name,
                                           jobNode->job.slaveHost.port,
                                           jobNode->job.slaveHost.tlsMode
                                          );
                }
                else
                {
                  slaveNode->tlsMode = jobNode->job.slaveHost.tlsMode;
                }
              }
            }

            // discard obsolete slaves
            slaveNode = JOB_SLAVE_LIST_HEAD();
            while (slaveNode != NULL)
            {
              if (   !Job_isSlaveLocked(slaveNode)
                  && !JOB_LIST_CONTAINS(jobNode,
                                           (jobNode->job.slaveHost.port == slaveNode->port)
                                        && String_equals(jobNode->job.slaveHost.name,slaveNode->name)
                                       )
                 )
              {
                slaveNode = Job_removeSlave(slaveNode);
              }
              else
              {
                slaveNode = slaveNode->next;
              }
            }
          }  // JOB_LIST_LOCKED_DO

          // try connect new slaves, authorize
          JOB_SLAVE_LIST_ITERATE(slaveNode)
          {
            assert(slaveNode->name != NULL);

            if (!Connector_isShutdown(&slaveNode->connectorInfo))
            {
              // try connect to slave
              if (!Connector_isConnected(&slaveNode->connectorInfo))
              {
                error = Connector_connect(&slaveNode->connectorInfo,
                                          slaveNode->name,
                                          slaveNode->port,
                                          slaveNode->tlsMode,
                                          globalOptions.serverCA.data,
                                          globalOptions.serverCA.length,
                                          globalOptions.serverCert.data,
                                          globalOptions.serverCert.length,
                                          globalOptions.serverKey.data,
                                          globalOptions.serverKey.length
                                         );
                if (error == ERROR_NONE)
                {
                  // log info
                  if (Misc_getCurrentDateTime() > (slaveNode->lastOnlineDateTime+10*S_PER_MINUTE))
                  {
                    logMessage(NULL,  // logHandle,
                               LOG_TYPE_INFO,
                               "Slave %s:%d online",
                               String_cString(slaveNode->name),slaveNode->port
                              );
                  }
                  slaveNode->lastOnlineDateTime = Misc_getCurrentDateTime();
                }
                else
                {
                  anyOfflineFlag = TRUE;
                }
              }

              // try authorize on slave
              if (   Connector_isConnected(&slaveNode->connectorInfo)
                  && !Connector_isAuthorized(&slaveNode->connectorInfo)
                 )
              {
                error = Connector_authorize(&slaveNode->connectorInfo,30*MS_PER_SECOND);
                if (error == ERROR_NONE)
                {
                  slaveNode->authorizedFlag = TRUE;

                  // log info
                  logMessage(NULL,  // logHandle,
                             LOG_TYPE_INFO,
                             "Slave %s:%d authorized",
                             String_cString(slaveNode->name),slaveNode->port
                            );
                }
                else
                {
                  anyUnpairedFlag = TRUE;
                }
              }

              // update slave state in job
              if      (Connector_isAuthorized(&slaveNode->connectorInfo))
              {
                error = Connector_getVersion(&slaveNode->connectorInfo,
                                             &slaveProtocolVersionMajor,
                                             NULL,  // slaveProtocolVersionMinor
                                             &slaveServerMode
                                            );
                if (error == ERROR_NONE)
                {
                  if (slaveServerMode == SERVER_MODE_SLAVE)
                  {
                    if (slaveProtocolVersionMajor == SERVER_PROTOCOL_VERSION_MAJOR)
                    {
                      updateSlaveState(slaveNode,
                                       SLAVE_STATE_PAIRED,
                                       Connector_isTLS(&slaveNode->connectorInfo),
                                       Connector_isInsecureTLS(&slaveNode->connectorInfo)
                                      );
                    }
                    else
                    {
                      updateSlaveState(slaveNode,
                                       SLAVE_STATE_WRONG_PROTOCOL_VERSION,
                                       Connector_isTLS(&slaveNode->connectorInfo),
                                       Connector_isInsecureTLS(&slaveNode->connectorInfo)
                                      );
                    }
                  }
                  else
                  {
                    updateSlaveState(slaveNode,
                                     SLAVE_STATE_WRONG_MODE,
                                     Connector_isTLS(&slaveNode->connectorInfo),
                                     Connector_isInsecureTLS(&slaveNode->connectorInfo)
                                    );
                  }
                }
              }
              else if (Connector_isConnected(&slaveNode->connectorInfo))
              {
                updateSlaveState(slaveNode,
                                 SLAVE_STATE_ONLINE,
                                 Connector_isTLS(&slaveNode->connectorInfo),
                                 Connector_isInsecureTLS(&slaveNode->connectorInfo)
                                );
              }
              else
              {
                updateSlaveState(slaveNode,
                                 SLAVE_STATE_OFFLINE,
                                 FALSE,  // slaveTLS
                                 FALSE  // slaveInsecureTLS
                                );
              }

#if 0
fprintf(stderr,"%s, %d: checked %s:%d : slavestate=%d slaveNode=%p connectstate=%d Connector_isConnected=%d\n",__FILE__,__LINE__,
String_cString(slaveNode->name),
slaveNode->port,
jobNode->slaveState,
slaveNode,
slaveNode->connectorInfo.state,
Connector_isConnected(&slaveNode->connectorInfo)
);
#endif
            }
          }
        }

        if (!anyOfflineFlag && !anyUnpairedFlag)
        {
          // sleep and check quit flag
          delayThread(SLEEP_TIME_PAIRING_THREAD,(globalOptions.serverMode == SERVER_MODE_MASTER) ? &pairingThreadTrigger : NULL);
        }
        else
        {
          // short sleep
          delayThread(30,(globalOptions.serverMode == SERVER_MODE_MASTER) ? &pairingThreadTrigger : NULL);
        }
        break;
      case SERVER_MODE_SLAVE:
        // check if pairing/clear master requested
        pairingStopDateTime = 0LL;
        if (File_open(&fileHandle,globalOptions.masterInfo.pairingFileName,FILE_OPEN_READ) == ERROR_NONE)
        {
          clearPairing = FALSE;

          // get modified time
          if (File_getInfo(&fileInfo,globalOptions.masterInfo.pairingFileName) == ERROR_NONE)
          {
            pairingStopDateTime = fileInfo.timeModified+DEFAULT_PAIRING_MASTER_TIMEOUT;
          }

          // read file
          if (File_readLine(&fileHandle,line) == ERROR_NONE)
          {
            clearPairing = String_equalsIgnoreCaseCString(line,"0") || String_equalsIgnoreCaseCString(line,"clear");
          }

          // close and delete file
          File_close(&fileHandle);
          (void)File_delete(globalOptions.masterInfo.pairingFileName,FALSE);

          // check if clear/start/stop pairing
          if (!clearPairing)
          {
            if (Misc_getCurrentDateTime() < pairingStopDateTime)
            {
              beginPairingMaster(DEFAULT_PAIRING_MASTER_TIMEOUT,PAIRING_MODE_AUTO);
            }
            else
            {
              abortPairingMaster();
            }
          }
          else
          {
            clearPairedMaster();
          }
        }
        else
        {
          if (Misc_isTimeout(&newMaster.pairingTimeoutInfo))
          {
            abortPairingMaster();
          }
        }

        if (   (newMaster.pairingMode != PAIRING_MODE_NONE)
            || String_isEmpty(globalOptions.masterInfo.name)
           )
        {
          // short sleep
          delayThread(5,(globalOptions.serverMode == SERVER_MODE_MASTER) ? &pairingThreadTrigger : NULL);
        }
        else
        {
          // sleep and check quit flag
          delayThread(SLEEP_TIME_PAIRING_THREAD,(globalOptions.serverMode == SERVER_MODE_MASTER) ? &pairingThreadTrigger : NULL);
        }
        break;
    }
  }
  String_delete(line);

  switch (globalOptions.serverMode)
  {
    case SERVER_MODE_MASTER:
      // disconnect slaves
      JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        JOB_SLAVE_LIST_ITERATE(slaveNode)
        {
          if (   Connector_isConnected(&slaveNode->connectorInfo)
              || Connector_isShutdown(&slaveNode->connectorInfo)
             )
          {
            Connector_disconnect(&slaveNode->connectorInfo);
          }
        }
      }
      break;
    case SERVER_MODE_SLAVE:
      break;
  }
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : freeJobScheduleNode
* Purpose: free job schedule node
* Input  : jobScheduleNode - job schedule node
*          userData        - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeJobScheduleNode(JobScheduleNode *jobScheduleNode, void *userData)
{
  assert(jobScheduleNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(jobScheduleNode->customText);
  String_delete(jobScheduleNode->scheduleUUID);
  String_delete(jobScheduleNode->jobUUID);
  String_delete(jobScheduleNode->jobName);
}

/***********************************************************************\
* Name   : getJobScheduleList
* Purpose: get and append job schedules
* Input  : jobScheduleList - job schedule list
*          jobNode         - job node
*          jobStateSet     - job states
*          enabled         - TRUE for only enabled schedules
*          archiveTypeSet  - archive types
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getJobScheduleList(JobScheduleList *jobScheduleList,
                              const JobNode   *jobNode,
                              JobStateSet     jobStateSet,
                              bool            enabled,
                              ArchiveTypeSet  archiveTypeSet
                             )
{
  assert(jobScheduleList != NULL);
  assert(jobNode != NULL);

  if (IN_SET(jobStateSet,jobNode->jobState))
  {
    const ScheduleNode *scheduleNode;
    LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
    {
      if (   (!enabled || scheduleNode->enabled)
          && IN_SET(archiveTypeSet,scheduleNode->archiveType)
         )
      {
        JobScheduleNode *jobScheduleNode = LIST_NEW_NODE(JobScheduleNode);
        if (jobScheduleNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        jobScheduleNode->jobName                   = String_duplicate(jobNode->name);
        jobScheduleNode->jobUUID                   = String_duplicate(jobNode->job.uuid);
        jobScheduleNode->lastScheduleCheckDateTime = jobNode->lastScheduleCheckDateTime;

        jobScheduleNode->scheduleUUID              = String_duplicate(scheduleNode->uuid);
        jobScheduleNode->date                      = scheduleNode->date;
        jobScheduleNode->weekDaySet                = scheduleNode->weekDaySet;
        jobScheduleNode->time                      = scheduleNode->time;
        jobScheduleNode->archiveType               = scheduleNode->archiveType;
        jobScheduleNode->interval                  = scheduleNode->interval;
        jobScheduleNode->beginTime                 = scheduleNode->beginTime;
        jobScheduleNode->endTime                   = scheduleNode->endTime;
        jobScheduleNode->customText                = String_duplicate(scheduleNode->customText);
        jobScheduleNode->testCreatedArchives       = scheduleNode->testCreatedArchives;
        jobScheduleNode->noStorage                 = scheduleNode->noStorage;
        jobScheduleNode->enabled                   = scheduleNode->enabled;
        jobScheduleNode->lastExecutedDateTime      = scheduleNode->lastExecutedDateTime;

        jobScheduleNode->active                    = scheduleNode->active;

        List_append(jobScheduleList,jobScheduleNode);
      }
    }
  }
}

/***********************************************************************\
* Name   : getJobSchedule
* Purpose: get job schedule
* Input  : jobScheduleNode          - job schedule
*          scheduleStartDateTime    - schedule search start date/time
*          continuousDatabaseHandle - continuous database handle
* Output : -
* Return : schedule date/time or MAX_UINT64
* Notes  : -
\***********************************************************************/

LOCAL uint64 getJobSchedule(const JobScheduleNode *jobScheduleNode,
                            uint64                scheduleStartDateTime,
                            DatabaseHandle        *continuousDatabaseHandle
                           )
{
  assert(jobScheduleNode != NULL);
  assert(continuousDatabaseHandle != NULL);

  uint64 jobScheduleDateTime = MAX_UINT64;

  // get search start date/time
  uint year,month,day;
  uint hour,minute;
  Misc_splitDateTime(scheduleStartDateTime,
                     TIME_TYPE_LOCAL,
                     &year,
                     &month,
                     &day,
                     &hour,
                     &minute,
                     NULL,  // second
                     NULL,  // &weekDay
                     NULL  // dayLightSavingMode
                    );

  // get search end year
  uint lastScheduleCheckYear;
  Misc_splitDateTime(jobScheduleNode->lastScheduleCheckDateTime,
                     TIME_TYPE_LOCAL,
                     &lastScheduleCheckYear,
                     NULL,  // month
                     NULL,  // day
                     NULL,  // hour
                     NULL,  // minute
                     NULL,  // second
                     NULL,  // weekDay
                     NULL  // dayLightSavingMode
                    );

  // check if job have to be executed by regular schedule (check backward in time)
//fprintf(stderr,"%s:%d: currentDateTime=%"PRIu64"\n",__FILE__,__LINE__,currentDateTime);
//fprintf(stderr,"%s:%d: dateTime=%d %d %d - %d %d\n",__FILE__,__LINE__,dateTime.year,dateTime.month,dateTime.day,dateTime.hour,dateTime.minute);
//fprintf(stderr,"%s:%d: lastScheduleCheckYear %d: %d %d\n",__FILE__,__LINE__,lastScheduleCheckYear);
  int checkYear   = (int)year;
  int checkMonth  = (int)month;
  int checkDay    = (int)day;
  int checkHour   = (int)hour;
  int checkMinute = (int)minute;
  while (checkYear >= (int)lastScheduleCheckYear)
  {
//fprintf(stderr,"%s:%d: year=%d\n",__FILE__,__LINE__,year);
    if ((jobScheduleNode->date.year == DATE_ANY) || (jobScheduleNode->date.year == checkYear))
    {
      while ((checkMonth >= 1) && !isQuit())
      {
//fprintf(stderr,"%s:%d: month=%d\n",__FILE__,__LINE__,month);
        if ((jobScheduleNode->date.month == DATE_ANY) || (jobScheduleNode->date.month == checkMonth))
        {
          while ((checkDay >= 1) && !isQuit())
          {
            WeekDays weekDay = Misc_getWeekDay(checkYear,checkMonth,checkDay);
//const char *W[7]={"Mo","Di","Mi","Do","Fr","Sa","So"}; fprintf(stderr,"%s:%d: month=%d day=%d weekday=%d %s %d\n",__FILE__,__LINE__,month,day,weekDay,W[weekDay],jobScheduleNode->date.day);

            if (   ((jobScheduleNode->date.day   == DATE_ANY       ) || (jobScheduleNode->date.day == checkDay)    )
                && ((jobScheduleNode->weekDaySet == WEEKDAY_SET_ANY) || IN_SET(jobScheduleNode->weekDaySet,weekDay))
               )
            {
              while ((checkHour >= 0) && !isQuit())
              {
//fprintf(stderr,"%s:%d: hour=%d\n",__FILE__,__LINE__,hour);
                if (   (jobScheduleNode->time.hour == TIME_ANY)
                    || (jobScheduleNode->time.hour == checkHour)
                    || (jobScheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
                   )
                {
                  while ((checkMinute >= 0) && !isQuit())
                  {
                    if (   (jobScheduleNode->time.minute == TIME_ANY)
                        || (jobScheduleNode->time.minute == checkMinute)
                        || (jobScheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
                       )
                    {
                      uint64 scheduleDateTime = Misc_makeDateTime(TIME_TYPE_LOCAL,
                                                                  (uint)checkYear,
                                                                  (uint)checkMonth,
                                                                  (uint)checkDay,
                                                                  (uint)checkHour,
                                                                  (uint)checkMinute,
                                                                  0,  // second
                                                                  DAY_LIGHT_SAVING_MODE_AUTO
                                                                 );
                      assert(scheduleDateTime <= scheduleStartDateTime);

                      if (scheduleDateTime > jobScheduleNode->lastExecutedDateTime)
                      {
                        if      (   (jobScheduleNode->archiveType == ARCHIVE_TYPE_FULL)
                                 && (   (jobScheduleDateTime == MAX_UINT64)
// TODO: obsolete
                                     || (   (jobScheduleNode->archiveType == jobScheduleNode->archiveType)
                                         && (jobScheduleNode->lastExecutedDateTime < jobScheduleNode->lastExecutedDateTime)
                                        )
                                    )
                                )
                        {
                          // schedule full job
                          jobScheduleDateTime = scheduleDateTime;
                        }
                        else if (   (jobScheduleNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
                                 && (   (jobScheduleDateTime == MAX_UINT64)
// TODO: obsolete
                                     || (   (jobScheduleNode->archiveType == jobScheduleNode->archiveType)
                                         && (jobScheduleNode->lastExecutedDateTime < jobScheduleNode->lastExecutedDateTime)
                                        )
                                    )
                                )
                        {
                          // schedule normal/differential/incremental job
                          jobScheduleDateTime = scheduleDateTime;
                        }
                        else if (   (jobScheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
                                 && (   (jobScheduleDateTime == MAX_UINT64)
// TODO: obsolete
                                     || (   (jobScheduleNode->archiveType == jobScheduleNode->archiveType)
                                         && (jobScheduleNode->lastExecutedDateTime < jobScheduleNode->lastExecutedDateTime)
                                        )
                                    )
                                 && (scheduleDateTime >= (  jobScheduleNode->lastExecutedDateTime
                                                          + (uint64)jobScheduleNode->interval*S_PER_MINUTE)
                                                         )
                                 && Continuous_isEntryAvailable(continuousDatabaseHandle,
                                                                String_cString(jobScheduleNode->jobUUID),
                                                                String_cString(jobScheduleNode->scheduleUUID)
                                                               )
                                )
                        {
                          // schedule continuous job
                          jobScheduleDateTime = scheduleDateTime;
                        }
                      }
                    }
                    if (jobScheduleDateTime != MAX_UINT64) break;
                    checkMinute--;
                  } // minute
                  if (jobScheduleDateTime != MAX_UINT64) break;
                }
                if (jobScheduleDateTime != MAX_UINT64) break;
                checkHour--;
                checkMinute = 59;
              } // hour
              if (jobScheduleDateTime != MAX_UINT64) break;
            }
            if (jobScheduleDateTime != MAX_UINT64) break;
            checkDay--;
            checkHour = 23;
          } // day
          if (jobScheduleDateTime != MAX_UINT64) break;
        }
        if (jobScheduleDateTime != MAX_UINT64) break;
        checkMonth--;
        if (checkMonth > 0) checkDay = Misc_getLastDayOfMonth(checkYear,checkMonth);
      } // month
      if (jobScheduleDateTime != MAX_UINT64) break;
    }
    if (jobScheduleDateTime != MAX_UINT64) break;
    checkYear--;
    checkMonth = 12;
  }

  return jobScheduleDateTime;
}

/***********************************************************************\
* Name   : getNextJobSchedule
* Purpose: get next job schedule
* Input  : jobScheduleNode       - job schedule
*          scheduleStartDateTime - schedule search start date/time
* Output : -
* Return : next schedule date/time or MAX_UINT64
* Notes  : -
\***********************************************************************/

LOCAL uint64 getNextJobSchedule(const JobScheduleNode *jobScheduleNode,
                                uint64                scheduleStartDateTime
                               )
{
  #define MAX_NEXT_SCHEDULE (7*24*60)  // max. next schedule check minutes [min]

  uint64 nextScheduleDateTime = MAX_UINT64;

  if (jobScheduleNode->enabled)
  {
    // get search start date/time
    uint year,month,day;
    uint hour,minute;
    uint weekDay;
    Misc_splitDateTime(scheduleStartDateTime,
                       TIME_TYPE_LOCAL,
                       &year,
                       &month,
                       &day,
                       &hour,
                       &minute,
                       NULL,  // second
                       &weekDay,
                       NULL  // dayLightSavingMode
                      );

    // search for next schedule
    uint   i = 0;
    while ((i < MAX_NEXT_SCHEDULE) && (nextScheduleDateTime >= MAX_UINT64))
    {
      // check matching schedules
      if ((jobScheduleNode->date.year == DATE_ANY) || (jobScheduleNode->date.year == (int)year))
      {
//fprintf(stderr,"%s:%d: schedule month=%d month=%d\n",__FILE__,__LINE__,jobScheduleNode->date.month,month);
        if ((jobScheduleNode->date.month == DATE_ANY) || (jobScheduleNode->date.month == (int)month))
        {
          uint weekDay = Misc_getWeekDay(year,month,day);
//const char *W[7]={"Mo","Di","Mi","Do","Fr","Sa","So"}; fprintf(stderr,"%s:%d: %d-%d-%d -> weekday=%d schedule day=%s day=%d\n",__FILE__,__LINE__,year,month,day,weekDay,W[weekDay],jobScheduleNode->date.day,day);
          if (   ((jobScheduleNode->date.day   == DATE_ANY       ) || (jobScheduleNode->date.day == (int)day)    )
              && ((jobScheduleNode->weekDaySet == WEEKDAY_SET_ANY) || IN_SET(jobScheduleNode->weekDaySet,weekDay))
             )
          {
//fprintf(stderr,"%s:%d: schedule hour=%d hour=%d\n",__FILE__,__LINE__,jobScheduleNode->time.hour,hour);
            if (   (jobScheduleNode->time.hour == TIME_ANY)
                || (jobScheduleNode->time.hour == (int)hour)
               )
            {
//fprintf(stderr,"%s:%d: schedule minute=%d minute=%d\n",__FILE__,__LINE__,jobScheduleNode->time.minute,minute);
              if (   (jobScheduleNode->time.minute == TIME_ANY)
                  || (jobScheduleNode->time.minute == (int)minute)
                 )
              {
                uint64 dateTime = Misc_makeDateTime(TIME_TYPE_LOCAL,
                                                    year,
                                                    month,
                                                    day,
                                                    hour,
                                                    minute,
                                                    0,  // second
                                                    DAY_LIGHT_SAVING_MODE_AUTO
                                                   );
                if (   (dateTime > scheduleStartDateTime)
                    && (dateTime > jobScheduleNode->lastExecutedDateTime)
                    && ((nextScheduleDateTime >= MAX_UINT64) || (dateTime < nextScheduleDateTime))
                   )
                {
                  nextScheduleDateTime = dateTime;
                }
              } // minute
            } // hour
          } // day
        } // month
      } // year

      // next date/time
      if (minute < 60)
      {
        minute++;
      }
      else
      {
        minute = 0;

        if (hour < 24)
        {
          hour++;
        }
        else
        {
          hour = 0;

          uint lastDayOfMonth = Misc_getLastDayOfMonth(year,month);
          if (day < lastDayOfMonth)
          {
            day++;
          }
          else
          {
            day = 1;

            if (month < 12)
            {
              month++;
            }
            else
            {
              month = 1;
              year++;
            }
          }
        }
      }

      i++;
    }
  }

  return nextScheduleDateTime;

  #undef MAX_NEXT_SCHEDULE
}

/***********************************************************************\
* Name   : schedulerThreadCode
* Purpose: scheduler thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void schedulerThreadCode(void)
{
  // open continuous database
  DatabaseHandle continuousDatabaseHandle;
  if (Continuous_isAvailable())
  {
    Errors error = Continuous_open(&continuousDatabaseHandle);
    if (error != ERROR_NONE)
    {
      printError("cannot initialize continuous database (error: %s)!",
                 Error_getText(error)
                );
      return;
    }
  }

  // init resources
  TimeoutInfo     rereadJobTimeout;
  JobScheduleList jobScheduleList;
  Misc_initTimeout(&rereadJobTimeout,SLEEP_TIME_SCHEDULER_THREAD);
  List_init(&jobScheduleList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeJobScheduleNode,NULL));

  while (!isQuit())
  {
    // write all modified jobs, re-read all job config files
    if (Misc_isTimeout(&rereadJobTimeout))
    {
      Job_flushAll();
      Job_rereadAll(globalOptions.jobsDirectory);

      Misc_restartTimeout(&rereadJobTimeout,0);
    }

    // get jobs schedule list (Note: avoid long locking of job list)
    List_clear(&jobScheduleList);
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      JobNode *jobNode;
      JOB_LIST_ITERATE(jobNode)
      {
        getJobScheduleList(&jobScheduleList,
                           jobNode,
                             SET_VALUE(JOB_STATE_NONE)
                           | SET_VALUE(JOB_STATE_DONE)
                           | SET_VALUE(JOB_STATE_ERROR)
                           | SET_VALUE(JOB_STATE_ABORTED)
                           | SET_VALUE(JOB_STATE_DISCONNECTED),
                           TRUE,  // enabled schedules only
                           ARCHIVE_TYPESET_ALL
                          );
      }
    }

    // check for jobs triggers
    uint64 now = (Misc_getCurrentDateTime()/S_PER_MINUTE)*S_PER_MINUTE;  // round to full minutes
    JobScheduleNode *jobScheduleNode;
    LIST_ITERATEX(&jobScheduleList,jobScheduleNode,!isQuit())
    {
      JobScheduleNode *executeScheduleNode = NULL;
      uint64          executeScheduleDateTime;
      if (jobScheduleNode->enabled)
      {
        executeScheduleDateTime = getJobSchedule(jobScheduleNode,
                                                 now,
                                                 &continuousDatabaseHandle
                                                );
        if (executeScheduleDateTime != MAX_UINT64)
        {
          executeScheduleNode = jobScheduleNode;
        }
      }

      if (executeScheduleNode != NULL)
      {
        JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
        {
          JobNode      *jobNode      = Job_findByUUID(executeScheduleNode->jobUUID);
          ScheduleNode *scheduleNode = Job_findScheduleByUUID(jobNode,executeScheduleNode->scheduleUUID);

          // trigger job schedule
          if (   (jobNode != NULL)
              && (scheduleNode != NULL)
              && !Job_isActive(jobNode->jobState)
             )
          {
            scheduleNode->active = TRUE;

            Job_trigger(jobNode,
                        scheduleNode->uuid,
                        scheduleNode->archiveType,
                        scheduleNode->customText,
                        scheduleNode->testCreatedArchives,
                        scheduleNode->noStorage,
                        FALSE,  // dryRun
                        executeScheduleDateTime,
                        "scheduler"
                       );

            char buffer[64];
            logMessage(NULL,  // logHandle,
                       LOG_TYPE_WARNING,
                       "Scheduled job '%s' %s for execution at %s",
                       String_cString(jobNode->name),
                       Archive_archiveTypeToString(scheduleNode->archiveType),
                       Misc_formatDateTimeCString(buffer,sizeof(buffer),executeScheduleDateTime,TIME_TYPE_LOCAL,NULL)
                      );

            // store last schedule check time
            jobNode->lastScheduleCheckDateTime = now;
          }
        }
      }
    }

    // sleep and check quit flag
    delayThread(SLEEP_TIME_SCHEDULER_THREAD,&schedulerThreadTrigger);
  }

  // free resources
  List_done(&jobScheduleList);
  Misc_doneTimeout(&rereadJobTimeout);

  // close continuous database
  if (Continuous_isAvailable()) Continuous_close(&continuousDatabaseHandle);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : pauseThreadCode
* Purpose: pause thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pauseThreadCode(void)
{
  SlaveNode *slaveNode;
  Errors    error;

  while (!isQuit())
  {
    // decrement pause time, continue
    SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      if (serverState == SERVER_STATE_PAUSE)
      {
        if (Misc_getCurrentDateTime() > pauseEndDateTime)
        {
          // clear pause flags
          pauseFlags.create           = FALSE;
          pauseFlags.storage          = FALSE;
          pauseFlags.restore          = FALSE;
          pauseFlags.indexUpdate      = FALSE;
          pauseFlags.indexMaintenance = FALSE;

          // continue all slaves
          if (globalOptions.serverMode == SERVER_MODE_MASTER)
          {
            JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
            {
              JOB_SLAVE_LIST_ITERATE(slaveNode)
              {
                if (Connector_isAuthorized(&slaveNode->connectorInfo))
                {
                  error = Connector_executeCommand(&slaveNode->connectorInfo,
                                                   1,
                                                   10*MS_PER_SECOND,
                                                   CALLBACK_(NULL,NULL),
                                                   "CONTINUE"
                                                  );
                  if (error != ERROR_NONE)
                  {
                    logMessage(NULL,  // logHandle,
                               LOG_TYPE_WARNING,
                               "Continue slave '%s:%u' fail (error: %s)",
                               String_cString(slaveNode->name),
                               slaveNode->port,
                               Error_getText(error)
                              );
                  }
                }
              }
            }
          }

          // set running state
          serverState = SERVER_STATE_RUNNING;
        }
      }
    }

    // sleep, check quit flag
    delayThread(SLEEP_TIME_PAUSE_THREAD,NULL);
  }
}

/***********************************************************************\
* Name   : isMaintenanceTime
* Purpose: check if date/time is maintence time
* Input  : dateTime - date/time
*          userData - user data (unused)
* Output : -
* Return : TRUE iff maintenance time or no maintenance defined at all
* Notes  : -
\***********************************************************************/

LOCAL bool isMaintenanceTime(uint64 dateTime, void *userData)
{
  bool                  maintenanceTimeFlag;
  uint                  year;
  uint                  month;
  uint                  day;
  uint                  hour;
  uint                  minute;
  WeekDays              weekDay;
  const MaintenanceNode *maintenanceNode;

  UNUSED_VARIABLE(userData);

  if      (pauseFlags.indexMaintenance)
  {
    maintenanceTimeFlag = FALSE;
  }
  else if (dateTime < intermediateMaintenanceDateTime)
  {
    maintenanceTimeFlag = TRUE;
  }
  else
  {
    maintenanceTimeFlag = FALSE;

    Misc_splitDateTime(dateTime,
                       TIME_TYPE_LOCAL,
                       &year,
                       &month,
                       &day,
                       &hour,
                       &minute,
                       NULL,  // second,
                       &weekDay,
                       NULL  // isDayLightSaving
                      );

    SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (!List_isEmpty(&globalOptions.maintenanceList))
      {
        LIST_ITERATE(&globalOptions.maintenanceList,maintenanceNode)
        {
          if (   (   (maintenanceNode->date.year == DATE_ANY)
                  || ((uint)maintenanceNode->date.year == year )
                 )
              && (   (maintenanceNode->date.month == DATE_ANY)
                  || ((uint)maintenanceNode->date.month == month)
                 )
              && (   (maintenanceNode->date.day == DATE_ANY)
                  || ((uint)maintenanceNode->date.day == day  )
                 )
              && (   (maintenanceNode->weekDaySet == WEEKDAY_SET_ANY)
                  || IN_SET(maintenanceNode->weekDaySet,weekDay)
                 )
              && (TIME_BEGIN(maintenanceNode->beginTime.hour,maintenanceNode->beginTime.minute) <= TIME(hour,minute))
              && (TIME_END  (maintenanceNode->endTime.hour,  maintenanceNode->endTime.minute  ) >= TIME(hour,minute))
             )
          {
            maintenanceTimeFlag = TRUE;
          }
        }
      }
      else
      {
        maintenanceTimeFlag = TRUE;
      }
    }
  }
//fprintf(stderr,"%s, %d: isMaintenanceTime %d %"PRIu64" %"PRIu64" -> %d\n",__FILE__,__LINE__,pauseFlags.indexMaintenance,dateTime,intermediateMaintenanceDateTime,maintenanceTimeFlag);

  return maintenanceTimeFlag;

  #undef TIME_END
  #undef TIME_BEGIN
  #undef TIME
}

/***********************************************************************\
* Name   : getNextSchedule
* Purpose: get next schedule
* Input  : jobNode               - current active job
*          scheduleUUID          - current active schedule UUID
*          scheduleStartDateTime - schedule start date/time
*          nextJobName           - next job name variable or NULL
*          nextJobUUID           - next job UUID variable or NULL
*          nextScheduleUUID      - next schedule UUID variable or NULL
* Output : nextJobName      - next job name or ""
*          nextJobUUID      - next job UUID or ""
*          nextScheduleUUID - next schedule UUID or ""
* Return : next schedule date/time or MAX_UINT64
* Notes  : -
\***********************************************************************/

LOCAL uint64 getNextSchedule(const JobNode *jobNode,
                             ConstString   scheduleUUID,
                             uint64        scheduleStartDateTime,
                             String        nextJobName,
                             String        nextJobUUID,
                             String        nextScheduleUUID
                            )
{
  assert(nextScheduleUUID != NULL);

  String_clear(nextJobName);
  String_clear(nextJobUUID);
  String_clear(nextScheduleUUID);

  uint64 nextScheduleDateTime = MAX_UINT64;

  // init variables
  JobScheduleList jobScheduleList;
  List_init(&jobScheduleList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeJobScheduleNode,NULL));

  // check schedules/get schedule list of all jobs (Note: avoid long locking of job list)
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    const JobNode *checkJobNode;
    JOB_LIST_ITERATEX(checkJobNode,nextScheduleDateTime != 0LL)
    {
      // check if some other job is already activated (by scheduler or manually)
      if (   (checkJobNode != jobNode)
          && (checkJobNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
          && (!Job_isRemote(checkJobNode) || isSlavePaired(checkJobNode))
          && Job_isActive(checkJobNode->jobState)
         )
      {
        nextScheduleDateTime = 0LL;
      }

      // get schedules for job
      if (Job_isLocal(checkJobNode))
      {
        // local job
        getJobScheduleList(&jobScheduleList,
                           checkJobNode,
                           JOB_STATESET_ALL,
                           TRUE,  // enabled schedules only
                             SET_VALUE(ARCHIVE_TYPE_NORMAL)
                           | SET_VALUE(ARCHIVE_TYPE_FULL)
                           | SET_VALUE(ARCHIVE_TYPE_INCREMENTAL)
                           | SET_VALUE(ARCHIVE_TYPE_DIFFERENTIAL)
                          );
      }
      else
      {
        // remote job: only if connected
        ConnectorInfo *connectorInfo;
        JOB_CONNECTOR_LOCKED_DO(connectorInfo,checkJobNode,LOCK_TIMEOUT)
        {
          if (Connector_isConnected(connectorInfo))
          {
            getJobScheduleList(&jobScheduleList,
                               checkJobNode,
                               JOB_STATESET_ALL,
                               TRUE,  // enabled schedules only
                                 SET_VALUE(ARCHIVE_TYPE_NORMAL)
                               | SET_VALUE(ARCHIVE_TYPE_FULL)
                               | SET_VALUE(ARCHIVE_TYPE_INCREMENTAL)
                               | SET_VALUE(ARCHIVE_TYPE_DIFFERENTIAL)
                              );
          }
        }
      }
    }
  }

  // find next schedule (ordering: running/active/scheduled)
  JobScheduleNode *jobScheduleNode;
  LIST_ITERATEX(&jobScheduleList,jobScheduleNode,(nextScheduleDateTime != 0LL) && !isQuit())
  {
    if (   !String_equals(jobScheduleNode->scheduleUUID,scheduleUUID)
        && jobScheduleNode->active
       )
    {
      if (nextJobName      != NULL) String_set(nextJobName,     jobScheduleNode->jobName);
      if (nextJobUUID      != NULL) String_set(nextJobUUID,     jobScheduleNode->jobUUID);
      if (nextScheduleUUID != NULL) String_set(nextScheduleUUID,jobScheduleNode->scheduleUUID);
      nextScheduleDateTime = 0LL;
    }
  }
  LIST_ITERATEX(&jobScheduleList,jobScheduleNode,(nextScheduleDateTime != 0LL) && !isQuit())
  {
    if (!String_equals(jobScheduleNode->scheduleUUID,scheduleUUID))
    {
      uint64 dateTime = getNextJobSchedule(jobScheduleNode,scheduleStartDateTime);
      if (dateTime < nextScheduleDateTime)
      {
        if (nextJobName      != NULL) String_set(nextJobName,     jobScheduleNode->jobName);
        if (nextJobUUID      != NULL) String_set(nextJobUUID,     jobScheduleNode->jobUUID);
        if (nextScheduleUUID != NULL) String_set(nextScheduleUUID,jobScheduleNode->scheduleUUID);
        nextScheduleDateTime = dateTime;
      }
    }
  }

  // free resources
  List_done(&jobScheduleList);

  return nextScheduleDateTime;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : testStorages
* Purpose: test storages
* Input  : indexHandle         - index handle
*          storageNameList     - storage name list
*          jobOptions          - job options
*          runningInfoFunction - running info function
*          runningInfoUserData - running info user data
*          isAbortedFunction   - check for aborted (can be NULL)
*          isAbortedUserData   - user data for check for aborted
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testStorages(IndexHandle             *indexHandle,
                          const StringList        *storageNameList,
                          JobOptions              *jobOptions,
                          TestRunningInfoFunction runningInfoFunction,
                          void                    *runningInfoUserData,
                          IsAbortedFunction       isAbortedFunction,
                          void                    *isAbortedUserData
                         )
{
  Errors error;

  assert(indexHandle != NULL);

// TODO: remove indexHandle
  UNUSED_VARIABLE(indexHandle);

  // test storage file
  error = Command_test(storageNameList,
                       NULL,  // includeEntryList,
                       NULL,  // excludePatternList,
                       jobOptions,
                       CALLBACK_(runningInfoFunction,runningInfoUserData),
// TODO:
                       CALLBACK_(NULL,NULL),  // getNamePassword
                       CALLBACK_(isAbortedFunction,isAbortedUserData),
                       NULL   // logHandle
                      );

  if (isQuit())
  {
    return ERROR_INTERRUPTED;
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : testStorages
* Purpose: test storage
* Input  : indexHandle             - index handle
*          storageId               - storage to delete
*          testRunningInfoFunction - running info function
*          testRunningInfoUserData - running info user data
*          isAbortedFunction       - check for aborted (can be NULL)
*          isAbortedUserData       - user data for check for aborted
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testStorage(IndexHandle             *indexHandle,
                         IndexId                 storageId,
                         TestRunningInfoFunction testRunningInfoFunction,
                         void                    *testRunningInfoUserData,
                         IsAbortedFunction       isAbortedFunction,
                         void                    *isAbortedUserData
                        )
{
  Errors error;

  assert(indexHandle != NULL);

  // find storage
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String storageName = String_new();
  if (!Index_findStorageById(indexHandle,
                             storageId,
                             jobUUID,
                             NULL,  // scheduleUUID
                             NULL,  // uuidId
                             NULL,  // entityId
                             storageName,
                             NULL,  // dateTime
                             NULL,  // size,
                             NULL,  // indexState
                             NULL,  // indexMode
                             NULL,  // lastCheckedDateTime
                             NULL,  // errorMessage
                             NULL,  // totalEntryCount
                             NULL   // totalEntrySize
                            )
     )
  {
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  // find job name, job options (if possible)
  String     jobName = String_new();
  JobOptions jobOptions;
  Job_getOptions(jobName,&jobOptions,jobUUID);

  // test storage
  StringList storageNameList;
  StringList_init(&storageNameList);
  StringList_append(&storageNameList,storageName);
  error = testStorages(indexHandle,
                       &storageNameList,
                       &jobOptions,
                       CALLBACK_(testRunningInfoFunction,testRunningInfoUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData)
                      );
  if (isQuit())
  {
    StringList_done(&storageNameList);
    Job_doneOptions(&jobOptions);
    String_delete(jobName);
    String_delete(storageName);
    return ERROR_INTERRUPTED;
  }
  StringList_done(&storageNameList);

  // log
  if (error == ERROR_NONE)
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Tested storage #%"PRIi64": job '%s'",
               INDEX_DATABASE_ID(storageId),
               String_cString(jobName)
              );
  }
  else
  {
    if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Test storage #%"PRIi64": job '%s' fail (error: %s)",
                 INDEX_DATABASE_ID(storageId),
                 String_cString(jobName),
                 Error_getText(error)
                );
    }
  }

  // free resources
  StringList_done(&storageNameList);
  Job_doneOptions(&jobOptions);
  String_delete(jobName);
  String_delete(storageName);

  return error;
}

/***********************************************************************\
* Name   : testEntity
* Purpose: test all storage files of entity
* Input  : indexHandle             - index handle
*          entityId                - index id of entity
*          testRunningInfoFunction - running info function
*          testRunningInfoUserData - funning info user data
*          isAbortedFunction       - check for aborted (can be NULL)
*          isAbortedUserData       - user data for check for aborted
* Output : -
* Return : ERROR_NONE or error code
* Notes  : No error is reported if a storage file cannot be deleted
\***********************************************************************/

LOCAL Errors testEntity(IndexHandle             *indexHandle,
                        IndexId                 entityId,
                        TestRunningInfoFunction testRunningInfoFunction,
                        void                    *testRunningInfoUserData,
                        IsAbortedFunction       isAbortedFunction,
                        void                    *isAbortedUserData
                       )
{
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors           error;
  IndexQueryHandle indexQueryHandle;

  assert(indexHandle != NULL);

  // find entity
  if (!Index_findEntity(indexHandle,
                        entityId,
                        NULL,  // findJobUUID,
                        NULL,  // findScheduleUUID
                        NULL,  // findHostName
                        ARCHIVE_TYPE_ANY,
                        0LL,  // findCreatedDate
                        0L,  // findCreatedTime
                        jobUUID,
                        NULL,  // scheduleUUID
                        NULL,  // uuidId
                        NULL,  // entityId
                        NULL,  // archiveType
                        NULL,  // createdDateTime,
                        NULL,  // lastErrorMessage
                        NULL,  // totalEntryCount
                        NULL  // totalEntrySize
                       )
     )
  {
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  // find job name, job options (if possible)
  String     jobName = String_new();
  JobOptions jobOptions;
  Job_getOptions(jobName,&jobOptions,jobUUID);

  // get all storages of entity
  StringList storageNameList;
  StringList_init(&storageNameList);
  String storageName = String_new();
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount
                                 INDEX_TYPESET_ALL,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // hostName
                                 NULL,  // userName
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    String_delete(storageName);
    StringList_done(&storageNameList);
    Job_doneOptions(&jobOptions);
    String_delete(jobName);
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              NULL,  // uuidId
                              NULL,  // jobUUID
                              NULL,  // entityId
                              NULL,  // scheduleUUID
                              NULL,  // hostName
                              NULL,  // userName
                              NULL,  // comment
                              NULL,  // createdDateTime
                              NULL,  // archiveType
                              NULL,  // storageId
                              storageName,
                              NULL,  // createdDateTime
                              NULL,  // size
                              NULL,  // indexState
                              NULL,  // indexMode
                              NULL,  // lastCheckedDateTime
                              NULL,  // errorMessage
                              NULL,  // totalEntryCount
                              NULL  // totalEntrySize
                             )
        )
  {
    StringList_append(&storageNameList,storageName);
  }
  Index_doneList(&indexQueryHandle);

  // test all storages of entity
  error = testStorages(indexHandle,
                       &storageNameList,
                       &jobOptions,
                       CALLBACK_(testRunningInfoFunction,testRunningInfoUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData)
                      );
  if (isQuit())
  {
    String_delete(storageName);
    StringList_done(&storageNameList);
    Job_doneOptions(&jobOptions);
    String_delete(jobName);
    return ERROR_INTERRUPTED;
  }

  // log
  if (error == ERROR_NONE)
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Tested entity #%"PRIi64": job '%s'",
               INDEX_DATABASE_ID(entityId),
               String_cString(jobName)
              );
  }
  else
  {
    if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Test entity #%"PRIi64": job '%s' fail (error: %s)",
                 INDEX_DATABASE_ID(entityId),
                 String_cString(jobName),
                 Error_getText(error)
                );
    }
  }

  // free resources
  String_delete(storageName);
  StringList_done(&storageNameList);
  Job_doneOptions(&jobOptions);
  String_delete(jobName);

  return error;
}

/***********************************************************************\
* Name   : testUUID
* Purpose: deltestete all storage files ofUUID
* Input  : indexHandle             - index handle
*          jobUUID                 - job UUID
*          testRunningInfoFunction - running info function
*          testRunningInfoUserData - running info user data
*          isAbortedFunction       - check for aborted (can be NULL)
*          isAbortedUserData       - user data for check for aborted
* Output : -
* Return : ERROR_NONE or error code
* Notes  : No error is reported if a storage file cannot be deleted
\***********************************************************************/

LOCAL Errors testUUID(IndexHandle             *indexHandle,
                      ConstString             jobUUID,
                      TestRunningInfoFunction testRunningInfoFunction,
                      void                    *testRunningInfoUserData,
                      IsAbortedFunction       isAbortedFunction,
                      void                    *isAbortedUserData
                     )
{
  Errors           error;
  IndexId          uuidId;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);

  // find UUID
  if (!Index_findUUID(indexHandle,
                      String_cString(jobUUID),
                      NULL,  // findScheduleUUID
                      &uuidId,
                      NULL,  // executionCountNormal,
                      NULL,  // executionCountFull,
                      NULL,  // executionCountIncremental,
                      NULL,  // executionCountDifferential,
                      NULL,  // executionCountContinuous,
                      NULL,  // averageDurationNormal,
                      NULL,  // averageDurationFull,
                      NULL,  // averageDurationIncremental,
                      NULL,  // averageDurationDifferential,
                      NULL,  // averageDurationContinuous,
                      NULL,  // totalEntityCount,
                      NULL,  // totalStorageCount,
                      NULL,  // totalStorageSize,
                      NULL,  // totalEntryCount,
                      NULL  // totalEntrySize
                     )
     )
  {
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  // get all entities with uuid id
  Array entityIdArray;
  Array_init(&entityIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 uuidId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleId,
                                 ARCHIVE_TYPE_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_ENTITY_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    Array_done(&entityIdArray);
    return error;
  }
  while (   (error == ERROR_NONE)
         && !isQuit()
         && !Job_isSomeActive()
         && Index_getNextEntity(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // scheduleUUID
                                &entityId,
                                NULL,  // archiveType
                                NULL,  // createdDateTime
                                NULL,  // lastErrorCode
                                NULL,  // lastErrorData
                                NULL,  // totalSize
                                NULL,  // totalEntryCount
                                NULL,  // totalEntrySize
                                NULL  // lockedCount
                               )
        )
  {
    Array_append(&entityIdArray,&entityId);
  }
  Index_doneList(&indexQueryHandle);

  // test all entities of uuid
  ArrayIterator arrayIterator;
  ARRAY_ITERATEX(&entityIdArray,
                 arrayIterator,
                 entityId,
                    (error == ERROR_NONE)
                 && ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
                 && !isQuit()
                )
  {
    error = testEntity(indexHandle,
                       entityId,
                       CALLBACK_(testRunningInfoFunction,testRunningInfoUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData)
                      );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&entityIdArray);
    return error;
  }
  if (isQuit() || Job_isSomeActive())
  {
    Array_done(&entityIdArray);
    return ERROR_INTERRUPTED;
  }

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Tested UUID '%s'",
             String_cString(jobUUID)
            );

  // free resources
  Array_done(&entityIdArray);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteStorage
* Purpose: delete storage
* Input  : indexHandle - index handle
*          storageId   - storage to delete
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors           error;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           storageName;
  String           string;
  uint64           createdDateTime;
  StorageSpecifier storageSpecifier;
  StorageInfo      storageInfo;

  assert(indexHandle != NULL);

  // init variables
  storageName = String_new();
  string      = String_new();

  // find storage
  if (!Index_findStorageById(indexHandle,
                             storageId,
                             jobUUID,
                             NULL,  // scheduleUUID
                             NULL,  // uuidId
                             NULL,  // entityId
                             storageName,
                             &createdDateTime,
                             NULL,  // size
                             NULL,  // indexState
                             NULL,  // indexMode
                             NULL,  // lastCheckedDateTime
                             NULL,  // errorMessage
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
     )
  {
    String_delete(string);
    String_delete(storageName);
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  error = ERROR_NONE;
  if (!String_isEmpty(storageName))
  {
    Storage_initSpecifier(&storageSpecifier);
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      // get storage specified job options (if possible)
      JobOptions jobOptions;
      Job_getOptions(NULL,&jobOptions,jobUUID);

//TODO
#ifndef WERROR
#warning NYI: move this special handling of limited scp into Storage_delete()?
#endif
      // init storage
      if (storageSpecifier.type == STORAGE_TYPE_SCP)
      {
        // try to init scp-storage first with sftp
        storageSpecifier.type = STORAGE_TYPE_SFTP;
        error = Storage_init(&storageInfo,
                             NULL,  // masterIO
                             &storageSpecifier,
                             &jobOptions,
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK_(NULL,NULL),  // storageUpdateProgress
                             CALLBACK_(NULL,NULL),  // getNamePassword
                             CALLBACK_(NULL,NULL),  // requestVolume
                             CALLBACK_(NULL,NULL),  // isPause
                             CALLBACK_(NULL,NULL),  // isAborted
                             NULL  // logHandle
                            );
        if (error != ERROR_NONE)
        {
          // init scp-storage
          storageSpecifier.type = STORAGE_TYPE_SCP;
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &storageSpecifier,
                               &jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
        }
      }
      else
      {
        // init other storage types
        error = Storage_init(&storageInfo,
                             NULL,  // masterIO
                             &storageSpecifier,
                             &jobOptions,
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK_(NULL,NULL),  // storageUpdateProgress
                             CALLBACK_(NULL,NULL),  // getNamePassword
                             CALLBACK_(NULL,NULL),  // requestVolume
                             CALLBACK_(NULL,NULL),  // isPause
                             CALLBACK_(NULL,NULL),  // isAborted
                             NULL  // logHandle
                            );
      }

      if (error !=  ERROR_NONE)
      {
        // init storage with job settings
        if (storageSpecifier.type == STORAGE_TYPE_SCP)
        {
          // try to init scp-storage first with sftp
          storageSpecifier.type = STORAGE_TYPE_SFTP;
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &storageSpecifier,
                               &jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
          if (error != ERROR_NONE)
          {
            // init scp-storage
            storageSpecifier.type = STORAGE_TYPE_SCP;
            error = Storage_init(&storageInfo,
                                 NULL,  // masterIO
                                 &storageSpecifier,
                                 &jobOptions,
                                 &globalOptions.indexDatabaseMaxBandWidthList,
                                 SERVER_CONNECTION_PRIORITY_HIGH,
                                 CALLBACK_(NULL,NULL),  // storageUpdateProgress
                                 CALLBACK_(NULL,NULL),  // getNamePassword
                                 CALLBACK_(NULL,NULL),  // requestVolume
                                 CALLBACK_(NULL,NULL),  // isPause
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );
          }
        }
        else
        {
          // init other storage types
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &storageSpecifier,
                               &jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
        }
      }

      // delete storage file
      if (error == ERROR_NONE)
      {
        // delete storage
        if (Storage_exists(&storageInfo,
                           NULL  // archiveName
                          )
           )
        {
          error = Storage_delete(&storageInfo,
                                 NULL  // archiveName
                                );
          if (error == ERROR_NONE)
          {
            if (createdDateTime > 0LL)
            {
              logMessage(NULL,  // logHandle,
                         LOG_TYPE_ALWAYS,
                         "Deleted storage #%"PRIi64": '%s', created at %s",
                         INDEX_DATABASE_ID(storageId),
                         String_cString(storageName),
                         String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,TIME_TYPE_LOCAL,NULL))
                        );
            }
            else
            {
              logMessage(NULL,  // logHandle,
                         LOG_TYPE_ALWAYS,
                         "Deleted storage #%"PRIi64": '%s'",
                         INDEX_DATABASE_ID(storageId),
                         String_cString(storageName)
                        );
            }
          }
          else
          {
            if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
            {
              logMessage(NULL,  // logHandle,
                         LOG_TYPE_ALWAYS,
                         "Delete storage #%"PRIi64": '%s' fail (error: %s)",
                         INDEX_DATABASE_ID(storageId),
                         String_cString(storageName),
                         Error_getText(error)
                        );
            }
          }
        }

        // prune empty directories
        Storage_pruneDirectories(&storageInfo,
                                 NULL  // archiveName
                                );

        // close storage
        Storage_done(&storageInfo);
      }

      Job_doneOptions(&jobOptions);
    }
    Storage_doneSpecifier(&storageSpecifier);
    if (error != ERROR_NONE)
    {
      String_delete(string);
      String_delete(storageName);
      return error;
    }
    if (isQuit())
    {
      String_delete(string);
      String_delete(storageName);
      return ERROR_INTERRUPTED;
    }

    // purge index
    error = IndexStorage_purge(indexHandle,
                               storageId,
                               NULL  // progressInfo
                              );
    if (error != ERROR_NONE)
    {
      String_delete(string);
      String_delete(storageName);
      return error;
    }
  }

  // free resources
  String_delete(string);
  String_delete(storageName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteEntity
* Purpose: delete entity index and all attached storage files
* Input  : indexHandle - index handle
*          entityId    - index id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : No error is reported if a storage file cannot be deleted
\***********************************************************************/

LOCAL Errors deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  String           jobName;
  String           string;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64           createdDateTime;
  Errors           error;
  const JobNode    *jobNode;
  IndexId          deleteStorageId;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // init variables
  jobName = String_new();
  string  = String_new();

  // find entity
  if (!Index_findEntity(indexHandle,
                        entityId,
                        NULL,  // findJobUUID,
                        NULL,  // findScheduleUUID
                        NULL,  // findHostName
                        ARCHIVE_TYPE_ANY,
                        0LL,  // findCreatedDate
                        0L,  // findCreatedTime
                        jobUUID,
                        NULL,  // scheduleUUID
                        NULL,  // uuidId
                        NULL,  // entityId
                        NULL,  // archiveType
                        &createdDateTime,
                        NULL,  // lastErrorMessage
                        NULL,  // totalEntryCount
                        NULL  // totalEntrySize
                       )
     )
  {
    String_delete(string);
    String_delete(jobName);
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  // find job name (if possible)
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode != NULL)
    {
      String_set(jobName,jobNode->name);
    }
    else
    {
      String_set(jobName,jobUUID);
    }
  }

  // delete all storages of entity
  do
  {
    deleteStorageId = INDEX_ID_NONE;

    // get next storage to delete
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      String_delete(string);
      String_delete(jobName);
      return error;
    }
    if (Index_getNextStorage(&indexQueryHandle,
                             NULL,  // uuidId
                             NULL,  // jobUUID
                             NULL,  // entityId
                             NULL,  // scheduleUUID
                             NULL,  // hostName
                             NULL,  // userName
                             NULL,  // comment
                             NULL,  // createdDateTime
                             NULL,  // archiveType
                             &storageId,
                             NULL,  // storageName
                             NULL,  // createdDateTime
                             NULL,  // size
                             NULL,  // indexState
                             NULL,  // indexMode
                             NULL,  // lastCheckedDateTime
                             NULL,  // errorMessage
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
          )
    {
      deleteStorageId = storageId;
    }
    Index_doneList(&indexQueryHandle);

    // delete storage
    if (!INDEX_ID_IS_NONE(deleteStorageId))
    {
      error = deleteStorage(indexHandle,deleteStorageId);
      if (error == ERROR_NONE)
      {
        if (createdDateTime > 0LL)
        {
          logMessage(NULL,  // logHandle,
                     LOG_TYPE_ALWAYS,
                     "Deleted entity #%"PRIi64": job '%s', created at %s",
                     INDEX_DATABASE_ID(entityId),
                     String_cString(jobName),
                     String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,TIME_TYPE_LOCAL,NULL))
                    );
        }
        else
        {
          logMessage(NULL,  // logHandle,
                     LOG_TYPE_ALWAYS,
                     "Deleted entity #%"PRIi64": job '%s'",
                     INDEX_DATABASE_ID(entityId),
                     String_cString(jobName)
                    );
        }
      }
      else
      {
        if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
        {
          logMessage(NULL,  // logHandle,
                     LOG_TYPE_ALWAYS,
                     "Delete entity #%"PRIi64": job '%s' fail (error: %s)",
                     INDEX_DATABASE_ID(entityId),
                     String_cString(jobName),
                     Error_getText(error)
                    );
        }
      }
    }
  }
  while (   !INDEX_ID_IS_NONE(deleteStorageId)
         && (error == ERROR_NONE)
         && !isQuit()
        );
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(jobName);
    return error;
  }
  if (isQuit())
  {
    String_delete(string);
    String_delete(jobName);
    return ERROR_INTERRUPTED;
  }

  // delete entity index
  error = Index_deleteEntity(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(jobName);
    return error;
  }

  // free resources
  String_delete(string);
  String_delete(jobName);

  return error;
}

/***********************************************************************\
* Name   : deleteUUID
* Purpose: delete all entities of UUID and all attached storage files
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
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
  if (!Index_findUUID(indexHandle,
                      String_cString(jobUUID),
                      NULL,  // findScheduleUUID
                      &uuidId,
                      NULL,  // executionCountNormal,
                      NULL,  // executionCountFull,
                      NULL,  // executionCountIncremental,
                      NULL,  // executionCountDifferential,
                      NULL,  // executionCountContinuous,
                      NULL,  // averageDurationNormal,
                      NULL,  // averageDurationFull,
                      NULL,  // averageDurationIncremental,
                      NULL,  // averageDurationDifferential,
                      NULL,  // averageDurationContinuous,
                      NULL,  // totalEntityCount,
                      NULL,  // totalStorageCount,
                      NULL,  // totalStorageSize,
                      NULL,  // totalEntryCount,
                      NULL  // totalEntrySize
                     )
     )
  {
    return ERROR_DATABASE_ENTRY_NOT_FOUND;
  }

  // delete all entities with uuid id
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 uuidId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleId,
                                 ARCHIVE_TYPE_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_ENTITY_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (   (error == ERROR_NONE)
         && !isQuit()
         && !Job_isSomeActive()
         && Index_getNextEntity(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // scheduleUUID
                                &entityId,
                                NULL,  // archiveType
                                NULL,  // createdDateTime
                                NULL,  // lastErrorCode
                                NULL,  // lastErrorData
                                NULL,  // totalSize
                                NULL,  // totalEntryCount
                                NULL,  // totalEntrySize
                                NULL  // lockedCount
                               )
        )
  {
    error = deleteEntity(indexHandle,entityId);
  }
  Index_doneList(&indexQueryHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (isQuit() || Job_isSomeActive())
  {
    return ERROR_INTERRUPTED;
  }

  // delete UUID
  error = Index_deleteUUID(indexHandle,uuidId);
  if (error == ERROR_NONE)
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Deleted UUID '%s'",
               String_cString(jobUUID)
              );
  }
  else
  {
    if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Delete UUID '%s' fail (error: %s)",
                 String_cString(jobUUID),
                 Error_getText(error)
                );
    }
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : freeEntityNode
* Purpose: free entity node
* Input  : entityNode - entity node
*          userData   - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeEntityNode(EntityNode *entityNode, void *userData)
{
  assert(entityNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(entityNode->jobUUID);
}

/***********************************************************************\
* Name   : newExpirationNode
* Purpose: allocate new entity node
* Input  : entityId        - entity index id
*          jobUUID         - job UUID
*          archiveType     - archive type; see ArchiveTypes
*          createdDateTime - create date/time
*          size            - size [bytes]
*          totalEntryCount - total entry count
*          totalEntrySize  - total entry size [bytes]
* Output : -
* Return : new entity node
* Notes  : -
\***********************************************************************/

LOCAL EntityNode *newExpirationNode(IndexId      entityId,
                                    ConstString  jobUUID,
                                    ArchiveTypes archiveType,
                                    uint64       createdDateTime,
                                    uint64       size,
                                    ulong        totalEntryCount,
                                    uint64       totalEntrySize,
                                    bool         lockedFlag
                                   )
{
  EntityNode *entityNode;

  entityNode = LIST_NEW_NODE(EntityNode);
  if (entityNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  entityNode->entityId        = entityId;
  entityNode->jobUUID         = String_duplicate(jobUUID);
  entityNode->archiveType     = archiveType;
  entityNode->createdDateTime = createdDateTime;
  entityNode->size            = size;
  entityNode->totalEntryCount = totalEntryCount;
  entityNode->totalEntrySize  = totalEntrySize;
  entityNode->lockedFlag      = lockedFlag;
  entityNode->persistenceNode = NULL;

  return entityNode;
}

/***********************************************************************\
* Name   : duplicateEntityNode
* Purpose: duplicate entity node
* Input  : fromEntityNode - from entity node
* Output : -
* Return : new entity node
* Notes  : -
\***********************************************************************/

LOCAL EntityNode *duplicateEntityNode(const EntityNode *fromEntityNode)
{
  EntityNode *entityNode;

  assert(fromEntityNode != NULL);

  entityNode = LIST_NEW_NODE(EntityNode);
  if (entityNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  entityNode->entityId        = fromEntityNode->entityId;
  entityNode->jobUUID         = String_duplicate(fromEntityNode->jobUUID);
  entityNode->archiveType     = fromEntityNode->archiveType;
  entityNode->createdDateTime = fromEntityNode->createdDateTime;
  entityNode->size            = fromEntityNode->size;
  entityNode->totalEntryCount = fromEntityNode->totalEntryCount;
  entityNode->totalEntrySize  = fromEntityNode->totalEntrySize;
  entityNode->lockedFlag      = fromEntityNode->lockedFlag;
  entityNode->persistenceNode = NULL;

  return entityNode;
}

/***********************************************************************\
* Name   : getEntityList
* Purpose: get list of all entities
* Input  : entityList     - entity list variale
*          indexHandle    - index handle (can be NULL)
* Output : entityList - entity list (descend ordering)
* Return : TRUE iff got entity list
* Notes  : -
\***********************************************************************/

LOCAL bool getEntityList(EntityList  *entityList,
                         IndexHandle *indexHandle
                        )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes     archiveType;
  uint64           createdDateTime;
  uint64           totalSize;
  uint             totalEntryCount;
  uint64           totalEntrySize;
  uint             lockedCount;
  EntityNode       *entityNode;

  assert(entityList != NULL);

  // init variables
  List_init(entityList,
            CALLBACK_(NULL,NULL),
            CALLBACK_((ListNodeFreeFunction)freeEntityNode,NULL)
           );

  if (indexHandle != NULL)
  {
    // get entities
    error = Index_initListEntities(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   NULL,  // jobUUID,
                                   NULL,  // scheduldUUID
                                   ARCHIVE_TYPE_ANY,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_ENTITY_SORT_MODE_CREATED,
                                   DATABASE_ORDERING_DESCENDING,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      return FALSE;
    }

    while (Index_getNextEntity(&indexQueryHandle,
                               NULL,  // uuidId,
                               jobUUID,
                               NULL,  // scheduleUUID,
                               &entityId,
                               &archiveType,
                               &createdDateTime,
                               NULL,  // lastErrorCode
                               NULL,  // lastErrorData
                               &totalSize,
                               &totalEntryCount,
                               &totalEntrySize,
                               &lockedCount
                              )
          )
    {
//fprintf(stderr,"%s, %d: %"PRIu64" entityId=%"PRIi64" archiveType=%d totalSize=%"PRIu64" now=%"PRIu64" createdDateTime=%"PRIu64" -> age=%"PRIu64"\n",__FILE__,__LINE__,Misc_getTimestamp()/1000,entityId,archiveType,totalSize,Misc_getCurrentDateTime(),createdDateTime,(Misc_getCurrentDateTime()-createdDateTime)/S_PER_DAY);
      // create expiration node
      entityNode = newExpirationNode(entityId,
                                     jobUUID,
                                     archiveType,
                                     createdDateTime,
                                     totalSize,
                                     totalEntryCount,
                                     totalEntrySize,
                                     (lockedCount > 0)
                                    );
      assert(entityNode != NULL);

      // add to list
      List_append(entityList,entityNode);
    }

    Index_doneList(&indexQueryHandle);
  }

  // check if list is sorted descending by create date/time
  #ifndef NDEBUG
    {
      uint64 lastCreatedDateTime = MAX_UINT64;
      LIST_ITERATE(entityList,entityNode)
      {
        assert(entityNode->createdDateTime <= lastCreatedDateTime);
        lastCreatedDateTime = entityNode->createdDateTime;
      }
    }
  #endif

  return TRUE;
}

/***********************************************************************\
* Name   : getJobEntityList
* Purpose: get job entity list with assigned peristence
* Input  : jobEntityList - job expiration list variable
*          entityList    - expiration list
*          jobUUID                 - job UUID
*          persistenceList         - job persistence list
* Output : jobEntityList - job expiration entity list with
                                     all entities with
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getJobEntityList(EntityList            *jobEntityList,
                            const EntityList      *entityList,
                            ConstString           jobUUID,
                            const PersistenceList *persistenceList
                           )
{
  uint64                 now;
  const EntityNode      *entityNode;
  EntityNode            *jobEntityNode;
  int                   age;
  const PersistenceNode *persistenceNode,*nextPersistenceNode;

  assert(jobEntityList != NULL);
  assert(entityList != NULL);
  assert(jobUUID != NULL);
  assert(Job_isListLocked());

  // check if list is sorted descending by create date/time
  #ifndef NDEBUG
    {
      uint64 lastCreatedDateTime = MAX_UINT64;
      LIST_ITERATE(entityList,entityNode)
      {
        assert(entityNode->createdDateTime <= lastCreatedDateTime);
        lastCreatedDateTime = entityNode->createdDateTime;
      }
    }
  #endif

  // init variables
  List_init(jobEntityList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeEntityNode,NULL));

  now = Misc_getCurrentDateTime();
  LIST_ITERATE(entityList,entityNode)
  {
//fprintf(stderr,"%s:%d: uuid=%s id=%"PRIu64"\n",__FILE__,__LINE__,String_cString(entityNode->jobUUID),INDEX_DATABASE_ID(entityNode->entityId));
    if (   String_equals(entityNode->jobUUID,jobUUID)
        #ifdef SIMULATE_PURGE
        && !Array_contains(&simulatedPurgeEntityIdArray,&entityNode->entityId)
        #endif /* SIMULATE_PURGE */
       )
    {
      // create expiration node
      jobEntityNode = duplicateEntityNode(entityNode);
      assert(jobEntityNode != NULL);

      // find persistence node for entity
      age             = (now-entityNode->createdDateTime)/S_PER_DAY;
      persistenceNode = LIST_HEAD(persistenceList);
      do
      {
        // find persistence node for archive type
        while (   (persistenceNode != NULL)
               && (persistenceNode->archiveType != entityNode->archiveType)
              )
        {
          persistenceNode = persistenceNode->next;
        }

        // find next persistence node for archive type
        nextPersistenceNode = persistenceNode;
        if (nextPersistenceNode != NULL)
        {
          do
          {
            nextPersistenceNode = nextPersistenceNode->next;
          }
          while (   (nextPersistenceNode != NULL)
                 && (nextPersistenceNode->archiveType != entityNode->archiveType)
                );
        }

        if (persistenceNode != NULL)
        {
          if (   (persistenceNode->maxAge == AGE_FOREVER)           // assign if persistence is forever
              || (age <= persistenceNode->maxAge)                   // assign if age is in persistence age range
              || (nextPersistenceNode == NULL)                      // assign to last existing persistence
             )
          {
            jobEntityNode->persistenceNode = persistenceNode;
            break;
          }
        }

        persistenceNode = nextPersistenceNode;
      }
      while (persistenceNode != NULL);

      // add to list
      List_append(jobEntityList,jobEntityNode);
    }
  }

  // check if list is sorted descending by create date/time
  #ifndef NDEBUG
    {
      uint64 lastCreatedDateTime = MAX_UINT64;
      LIST_ITERATE(jobEntityList,entityNode)
      {
        assert(entityNode->createdDateTime <= lastCreatedDateTime);
        lastCreatedDateTime = entityNode->createdDateTime;
      }
    }
  #endif
}

/***********************************************************************\
* Name   : hasPersistence
* Purpose: has persistence for specified archive type
* Input  : jobNode     - job node
*          archiveType - archive type
* Output : -
* Return : TRUE iff persistence for archive type
* Notes  : -
\***********************************************************************/

LOCAL bool hasPersistence(const JobNode *jobNode,
                          ArchiveTypes  archiveType
                         )
{
  const PersistenceNode *persistenceNode;

  assert(jobNode != NULL);
  assert(Job_isListLocked());

  return LIST_CONTAINS(&jobNode->job.options.persistenceList,
                       persistenceNode,
                       persistenceNode->archiveType == archiveType
                      );
}

/***********************************************************************\
* Name   : isInTransit
* Purpose: check if entity is "in-transit"
* Input  : entityNode - expiration entity node
* Output : -
* Return : TRUE iff in transit
* Notes  : -
\***********************************************************************/

LOCAL bool isInTransit(const EntityNode *entityNode)
{
  const PersistenceNode *nextPersistenceNode;
  const EntityNode      *nextEntityNode;

  assert(entityNode != NULL);

  nextPersistenceNode = LIST_FIND_NEXT(entityNode->persistenceNode,
                                       nextPersistenceNode,
                                       nextPersistenceNode->archiveType == entityNode->archiveType
                                      );
  nextEntityNode      = LIST_FIND_NEXT(entityNode,
                                       nextEntityNode,
                                       nextEntityNode->archiveType == entityNode->archiveType
                                      );

  return (   (nextPersistenceNode != NULL)
          && (   (nextEntityNode == NULL)
              || (   (nextEntityNode != NULL)
                  && (entityNode->persistenceNode != nextEntityNode->persistenceNode)
                 )
             )
         );
}

/***********************************************************************\
* Name   : purgeExpiredEntities
* Purpose: purge expired entities
* Input  : indexHandle    - index handle
*          jobUUID        - job UUID or NULL
*          newArchiveType - new archive type which will be created for
*                           job or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : if new created archive is given it is respected in expiration
*          of job entities
\***********************************************************************/

LOCAL Errors purgeExpiredEntities(IndexHandle  *indexHandle,
                                  ConstString  jobUUID,
                                  ArchiveTypes newArchiveType
                                 )
{
  Array            entityIdArray;
  String           expiredJobName;
  EntityList       entityList;
  Errors           failError;
  uint64           now;
  IndexId          expiredEntityId;
  ArchiveTypes     expiredArchiveType;
  uint64           expiredCreatedDateTime;
  ulong            expiredTotalEntryCount;
  uint64           expiredTotalEntrySize;
  String           expiredReason;
  MountList        mountList;
  Errors           error;
  const JobNode    *jobNode;
  EntityList       jobEntityList;
  const EntityNode *jobEntityNode,*otherJobEntityNode;
  const EntityNode *nextJobEntityNode;
  uint             age;
  uint             totalEntityCount;
  uint64           totalEntitySize;
  AutoFreeList     autoFreeList;
  char             string[64];

  assert(indexHandle != NULL);

  // init variables
  Array_init(&entityIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  expiredJobName = String_new();
  expiredReason  = String_new();
  List_init(&entityList,
            CALLBACK_(NULL,NULL),
            CALLBACK_((ListNodeFreeFunction)freeEntityNode,NULL)
           );

  failError = ERROR_NONE;
  do
  {
    // get entity list
    getEntityList(&entityList,indexHandle);

    // init variables
    now                    = Misc_getCurrentDateTime();
    expiredEntityId        = INDEX_ID_NONE;
    expiredArchiveType     = ARCHIVE_TYPE_NONE;
    expiredCreatedDateTime = 0LL;
    expiredTotalEntryCount = 0;
    expiredTotalEntrySize  = 0LL;
    List_init(&mountList,
              CALLBACK_((ListNodeDuplicateFunction)Configuration_duplicateMountNode,NULL),
              CALLBACK_((ListNodeFreeFunction)Configuration_freeMountNode,NULL)
             );

    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      // find expired/surpluse entity
      JOB_LIST_ITERATEX(jobNode,INDEX_ID_IS_NONE(expiredEntityId))
      {
        // get entity list for job with assigned persistence
        getJobEntityList(&jobEntityList,
                         &entityList,
                         jobNode->job.uuid,
                         &jobNode->job.options.persistenceList
                        );

        if (   (   (Misc_getCurrentDateTime() > (jobNode->job.options.persistenceList.lastModificationDateTime+10*S_PER_MINUTE))  // wait 10min after change expiration setting before purge
                || (newArchiveType != ARCHIVE_TYPE_NONE)  // if new entity is created: purge immediately
               )
            && !List_isEmpty(&jobEntityList)  // only expire if persistence list is not empty
           )
        {
          // find expired entity
          LIST_ITERATEX(&jobEntityList,jobEntityNode,INDEX_ID_IS_NONE(expiredEntityId))
          {
            totalEntityCount = 0;
            totalEntitySize  = 0LL;

            if (   !jobEntityNode->lockedFlag
                && (jobEntityNode->persistenceNode != NULL)
               )
            {
              // calculate number/total size of entities in persistence periode
              LIST_ITERATE(&jobEntityList,otherJobEntityNode)
              {
                if (   (otherJobEntityNode->persistenceNode == jobEntityNode->persistenceNode)  // same periode
                    && !otherJobEntityNode->lockedFlag                                          // not locked
                    && !isInTransit(otherJobEntityNode)                                         // not "in-intransit"
                   )
                {
                  totalEntityCount++;
                  totalEntitySize += otherJobEntityNode->size;
                }
              }

              // if new entity for job UUID: increment number of total entities
              if (   String_equals(jobNode->job.uuid,jobUUID)
                  && (jobEntityNode->archiveType == newArchiveType)
                 )
              {
                totalEntityCount++;
              }

              // get age
              age = (now-jobEntityNode->createdDateTime)/S_PER_DAY;

              // check if expired and not "in-transit"
              if (   !isInTransit(jobEntityNode)
                  && hasPersistence(jobNode,jobEntityNode->archiveType)
                  && (   (jobEntityNode->persistenceNode != NULL)
                      && (jobEntityNode->persistenceNode->minKeep != KEEP_ALL)
                      && (totalEntityCount > (uint)jobEntityNode->persistenceNode->minKeep)
                     )
                  && !Index_isLockedEntity(indexHandle,jobEntityNode->entityId)
                  && !Array_contains(&entityIdArray,&jobEntityNode->entityId)
                 )
              {
                // persistence defined, expired and not "in-transit"
                if       (   (jobEntityNode->persistenceNode->maxKeep != KEEP_ALL)
                          && (jobEntityNode->persistenceNode->maxKeep >= jobEntityNode->persistenceNode->minKeep)
                          && (totalEntityCount > (uint)jobEntityNode->persistenceNode->maxKeep)
                         )
                {
                  // over max-keep limit -> find oldest entry of same type and persistence

                  // mark expired entity+oldest entity as processed
                  Array_append(&entityIdArray,&jobEntityNode->entityId);
                  do
                  {
                    nextJobEntityNode = jobEntityNode->next;
                    while (   (nextJobEntityNode != NULL)
                           && (   (nextJobEntityNode->archiveType != jobEntityNode->archiveType)
                               || (nextJobEntityNode->persistenceNode != jobEntityNode->persistenceNode)
                               || isInTransit(nextJobEntityNode)
                              )
                          )
                    {
                      nextJobEntityNode = nextJobEntityNode->next;
                    }

                    if (nextJobEntityNode != NULL)
                    {
                      jobEntityNode = nextJobEntityNode;
                      Array_append(&entityIdArray,&jobEntityNode->entityId);
                    }
                  }
                  while (nextJobEntityNode != NULL);

                  // get expired entity
                  expiredEntityId        = jobEntityNode->entityId;
                  String_set(expiredJobName,jobNode->name);
                  expiredArchiveType     = jobEntityNode->archiveType;
                  expiredCreatedDateTime = jobEntityNode->createdDateTime;
                  expiredTotalEntryCount = jobEntityNode->totalEntryCount;
                  expiredTotalEntrySize  = jobEntityNode->totalEntrySize;
                  String_format(expiredReason,"max. keep limit reached (%u)",(uint)jobEntityNode->persistenceNode->maxKeep);

                  // get mount list
                  List_copy(&mountList,
                            NULL,
                            &jobNode->job.options.mountList,
                            NULL,
                            NULL
                           );
                }
                else if  (   (jobEntityNode->persistenceNode->maxAge != AGE_FOREVER)
                          && (age > (uint)jobEntityNode->persistenceNode->maxAge)
                         )
                {
                  // older than max-age

                  // mark expired entity as processed
                  Array_append(&entityIdArray,&jobEntityNode->entityId);

                  // get expired entity
                  expiredEntityId        = jobEntityNode->entityId;
                  String_set(expiredJobName,jobNode->name);
                  expiredArchiveType     = jobEntityNode->archiveType;
                  expiredCreatedDateTime = jobEntityNode->createdDateTime;
                  expiredTotalEntryCount = jobEntityNode->totalEntryCount;
                  expiredTotalEntrySize  = jobEntityNode->totalEntrySize;
                  String_format(expiredReason,"max. age reached (%u days)",jobEntityNode->persistenceNode->maxAge);

                  // get mount list
                  List_copy(&mountList,
                            NULL,
                            &jobNode->job.options.mountList,
                            NULL,
                            NULL
                           );
                }
              }
            }
          }
        }

        List_done(&jobEntityList);
      }
    } // jobList

    // delete expired entity
    if (!INDEX_ID_IS_NONE(expiredEntityId))
    {
      AutoFree_init(&autoFreeList);
      error = ERROR_NONE;

      // lock entity
      if (error == ERROR_NONE)
      {
        error = Index_lockEntity(indexHandle,expiredEntityId);
        if (error == ERROR_NONE)
        {
          AUTOFREE_ADD(&autoFreeList,&expiredEntityId,{ (void)Index_unlockEntity(indexHandle,expiredEntityId); });
        }
      }

      // mount devices
      if (error == ERROR_NONE)
      {
        error = mountAll(&mountList);
        if (error == ERROR_NONE)
        {
          AUTOFREE_ADD(&autoFreeList,&mountList,{ (void)unmountAll(&mountList); });
        }
      }

      // delete expired entity
      if (error == ERROR_NONE)
      {
        #ifndef SIMULATE_PURGE
          error = deleteEntity(indexHandle,expiredEntityId);
        #else /* not SIMULATE_PURGE */
          Array_append(&simulatedPurgeEntityIdArray,&expiredEntityId);
          error = ERROR_NONE;
        #endif /* SIMULATE_PURGE */
      }

      // unmount devices
      if (error == ERROR_NONE)
      {
        AUTOFREE_REMOVE(&autoFreeList,&mountList);
        (void)unmountAll(&mountList);
      }

      // unlock entity
      if (error == ERROR_NONE)
      {
        AUTOFREE_REMOVE(&autoFreeList,&expiredEntityId);
        (void)Index_unlockEntity(indexHandle,expiredEntityId);
      }

      if (error == ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle,
                    LOG_TYPE_INDEX,
                    "INDEX",
                    #ifdef SIMULATE_PURGE
                      "Purged expired entity of job '%s': '%s', created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes) (simulated): %s",
                    #else /* not SIMULATE_PURGE */
                      "Purged expired entity of job '%s': '%s', created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes): %s",
                    #endif /* SIMULATE_PURGE */
                    String_cString(expiredJobName),
                    Archive_archiveTypeToString(expiredArchiveType),
                    Misc_formatDateTimeCString(string,sizeof(string),expiredCreatedDateTime,TIME_TYPE_LOCAL,NULL),
                    expiredTotalEntryCount,
                    BYTES_SHORT(expiredTotalEntrySize),
                    BYTES_UNIT(expiredTotalEntrySize),
                    expiredTotalEntrySize,
                    String_cString(expiredReason)
                   );
      }
      else
      {
        if (Error_getCode(error) != ERROR_CODE_CONNECT_FAIL)
        {
          plogMessage(NULL,  // logHandle,
                      LOG_TYPE_INDEX,
                      "INDEX",
                      #ifdef SIMULATE_PURGE
                        "Purge expired entity of job '%s': '%s', created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes) (simulated) failed: %s",
                      #else /* not SIMULATE_PURGE */
                        "Purge expired entity of job '%s': '%s', created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes) failed: %s",
                      #endif /* SIMULATE_PURGE */
                      String_cString(expiredJobName),
                      Archive_archiveTypeToString(expiredArchiveType),
                      Misc_formatDateTimeCString(string,sizeof(string),expiredCreatedDateTime,TIME_TYPE_LOCAL,NULL),
                      expiredTotalEntryCount,
                      BYTES_SHORT(expiredTotalEntrySize),
                      BYTES_UNIT(expiredTotalEntrySize),
                      expiredTotalEntrySize,
                      Error_getText(error)
                     );
        }
      }

      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        if (failError == ERROR_NONE)
        {
          failError = error;
        }
      }

      AutoFree_done(&autoFreeList);
    }

    // free resources
    List_done(&mountList);
    List_done(&entityList);
  }
  while (   !INDEX_ID_IS_NONE(expiredEntityId)
         && !isQuit()
        );

  // free resources
  String_delete(expiredReason);
  String_delete(expiredJobName);
  Array_done(&entityIdArray);

  return failError;
}

/***********************************************************************\
* Name   : moveEntity
* Purpose: move storages of entity to new destination
* Input  : indexHandle            - index handle
*          jobOptions             - job options
*          entityId               - entity id
*          moveToStorageSpecifier - move-to storage specifier
*          moveToPath             - move-to path
*          transferInfoFunction   - transfer info function (can be NULL)
*          transferInfoUserData   - user data for transfer info function
*          isAbortedFunction      - is abort check callback (can be NULL)
*          isAbortedUserData      - user data for is aborted check
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors moveEntity(IndexHandle            *indexHandle,
                        const JobOptions       *jobOptions,
                        IndexId                entityId,
                        const StorageSpecifier *moveToStorageSpecifier,
                        ConstString            moveToPath,
                        TransferInfoFunction   transferInfoFunction,
                        void                   *transferInfoUserData,
                        IsAbortedFunction      isAbortedFunction,
                        void                   *isAbortedUserData
                       )
{
  String           storageName;
  StorageSpecifier storageSpecifier;
  String           directoryPath,baseName;
  String           moveToArchivePath;
  BandWidthList    maxFromBandWidthList,maxToBandWidthList;
  Errors           error;
  uint             totalStorageCount;
  uint64           totalStorageSize;
  IndexQueryHandle indexQueryHandle;
  uint             doneStorageCount;
  uint64           doneStorageSize;
  IndexId          storageId;
  uint64           size;
  IndexStates      indexState;
  StorageInfo      fromStorageInfo,toStorageInfo;

  assert(indexHandle != NULL);
  assert(jobOptions != NULL);
  assert(moveToStorageSpecifier != NULL);
  assert(moveToPath != NULL);
  assert(Index_isLockedEntity(indexHandle,entityId));

  // init variables
  storageName       = String_new();
  Storage_initSpecifier(&storageSpecifier);
  directoryPath     = String_new();
  baseName          = String_new();
  moveToArchivePath = String_new();
// TODO: bandwidht limitor NYI
  List_init(&maxFromBandWidthList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)Configuration_freeBandWidthNode,NULL));
  List_init(&maxToBandWidthList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)Configuration_freeBandWidthNode,NULL));

  // get storage infos
  error = Index_getStoragesInfos(indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID
                                 NULL,  // indexIds
                                 0,  // indexIdCount
                                 INDEX_TYPESET_ALL,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 &totalStorageCount,
                                 &totalStorageSize,
                                 NULL,  // totalEntryCount
                                 NULL  // totalEntrySize
                                );
  if (error == ERROR_NONE)
  {
    // move storages
    doneStorageCount = 0;
    doneStorageSize  = 0LL;
    do
    {
      // find next storage to move (Note: do not iterate with index list to avoid long running lock)
      storageId = INDEX_ID_NONE;
      error = Index_initListStorages(&indexQueryHandle,
                                     indexHandle,
                                     INDEX_ID_ANY,  // uuidId
                                     entityId,
                                     NULL,  // jobUUID
                                     NULL,  // scheduleUUID,
                                     NULL,  // indexIds
                                     0,  // indexIdCount
                                     INDEX_TYPESET_ALL,
                                       SET_VALUE(INDEX_STATE_NONE)
                                     | SET_VALUE(INDEX_STATE_OK)
                                     | SET_VALUE(INDEX_STATE_CREATE)
                                     | SET_VALUE(INDEX_STATE_UPDATE_REQUESTED)
                                     | SET_VALUE(INDEX_STATE_UPDATE),
                                     INDEX_MODE_SET_ALL,
                                     NULL,  // hostName
                                     NULL,  // userName
                                     NULL,  // name
                                     INDEX_STORAGE_SORT_MODE_NONE,
                                     DATABASE_ORDERING_NONE,
                                     0LL,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error == ERROR_NONE)
      {
        while (   INDEX_ID_IS_NONE(storageId)
               && Index_getNextStorage(&indexQueryHandle,
                                       NULL,  // uuidId
                                       NULL,  // jobUUID
                                       NULL,  // entityId
                                       NULL,  // scheduleUUID
                                       NULL,  // hostName
                                       NULL,  // userName
                                       NULL,  // comment
                                       NULL,  // createdDateTime
                                       NULL,  // archiveType
                                       &storageId,
                                       storageName,
                                       NULL,  // createdDateTime,
                                       &size,  // size
                                       &indexState,
                                       NULL,  // indexMode,
                                       NULL,  // lastCheckedDateTime,
                                       NULL,  // errorMessage
                                       NULL,  // totalEntryCount
                                       NULL  // totalEntrySize
                                      )
               && (   (isAbortedFunction == NULL)
                   || !isAbortedFunction(isAbortedUserData)
                  )
              )
      {
        if (error == ERROR_NONE)
        {
          // parse name
          error = Storage_parseName(&storageSpecifier,storageName);
        }

        if (error == ERROR_NONE)
        {
          // get new archive name
          File_splitFileName(storageSpecifier.archiveName,directoryPath,baseName,NULL);
          File_setFileName(moveToArchivePath,moveToPath);
          File_appendFileName(moveToArchivePath,baseName);

          // check if path or storage type is different
          if (Storage_equalSpecifiers(&storageSpecifier,
                                      storageSpecifier.archiveName,
                                      moveToStorageSpecifier,
                                      moveToArchivePath
                                     )
             )
          {
            storageId = INDEX_ID_NONE;
          }
        }
      }
      Index_doneList(&indexQueryHandle);
    }

      // move storage
      if (!INDEX_ID_IS_NONE(storageId))
      {
        // update info
        if (transferInfoFunction != NULL)
        {
          transferInfoFunction(storageId,
                               storageName,
                               0LL,  // n
                               0LL,  // size
                               doneStorageCount,
                               doneStorageSize,
                               totalStorageCount,
                               totalStorageSize,
                               transferInfoUserData
                              );
        }

        if (error == ERROR_NONE)
        {
          // init storages
          if (error == ERROR_NONE)
          {
            error = Storage_init(&fromStorageInfo,
                                 NULL,  // masterIO
                                 &storageSpecifier,
                                 jobOptions,
                                 &maxFromBandWidthList,
                                 SERVER_CONNECTION_PRIORITY_LOW,
                                 CALLBACK_(NULL,NULL),  // storageUpdateProgress
                                 CALLBACK_(NULL,NULL),  // getNamePassword
                                 CALLBACK_(NULL,NULL),  // storageVolumeRequest
                                 CALLBACK_(NULL,NULL),  // isPause
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );
            if (error == ERROR_NONE)
            {
              if (Storage_exists(&fromStorageInfo,NULL))
              {
                error = Storage_init(&toStorageInfo,
                                     NULL,  // masterIO
                                     moveToStorageSpecifier,
                                     jobOptions,
                                     &maxToBandWidthList,
                                     SERVER_CONNECTION_PRIORITY_LOW,
                                     CALLBACK_(NULL,NULL),  // storageUpdateProgress
                                     CALLBACK_(NULL,NULL),  // getNamePassword
                                     CALLBACK_(NULL,NULL),  // storageVolumeRequest
                                     CALLBACK_(NULL,NULL),  // isPause
                                     CALLBACK_(NULL,NULL),  // isAborted
                                     NULL  // logHandle
                                    );
                if (error == ERROR_NONE)
                {
                  // get unique name
                  if (Storage_exists(&toStorageInfo,moveToArchivePath))
                  {
                    String prefixFileName,postfixFileName;
                    long   index;
                    uint   n;

                    // rename to new archive
                    prefixFileName  = String_new();
                    postfixFileName = String_new();
                    index = String_findLastChar(baseName,STRING_END,'.');
                    if (index >= 0)
                    {
                      String_sub(prefixFileName,baseName,STRING_BEGIN,index);
                      String_sub(postfixFileName,baseName,index,STRING_END);
                    }
                    else
                    {
                      String_set(prefixFileName,baseName);
                    }
                    n = 0;
                    do
                    {
                      File_setFileName(moveToArchivePath,moveToPath);
                      File_appendFileName(moveToArchivePath,prefixFileName);
                      String_appendFormat(moveToArchivePath,"-%u",n);
                      String_append(moveToArchivePath,postfixFileName);
                      n++;
                    }
                    while (Storage_exists(&toStorageInfo,moveToArchivePath));
                    String_delete(postfixFileName);
                    String_delete(prefixFileName);
                  }

                  // set new storage name
                  if (error == ERROR_NONE)
                  {
                    error = Index_updateStorage(indexHandle,
                                                storageId,
                                                NULL,  // hostName,
                                                NULL,  // userName,
                                                Storage_getPrintableName(NULL,moveToStorageSpecifier,moveToArchivePath),
                                                0LL,  // dateTime,
                                                size,
                                                NULL,  // comment,
                                                FALSE  // updateNewest
                                               );
                  }

                  // copy storage
                  if (error == ERROR_NONE)
                  {
                    error = Storage_copy(&fromStorageInfo,
                                         storageSpecifier.archiveName,
                                         &toStorageInfo,
                                         moveToArchivePath,
                                         size,
                                         CALLBACK_INLINE(bool,(uint64 doneBytes, uint64 totalBytes, void *userData),
                                         {
                                           bool result;

                                           UNUSED_VARIABLE(userData);

                                           if (transferInfoFunction != NULL)
                                           {
                                             transferInfoFunction(storageId,
                                                                  storageName,
                                                                  doneBytes,
                                                                  totalBytes,
                                                                  doneStorageCount,
                                                                  doneStorageSize,
                                                                  totalStorageCount,
                                                                  totalStorageSize,
                                                                  transferInfoUserData
                                                                 );
                                           }

                                           if (isAbortedFunction != NULL)
                                           {
                                             result = !isAbortedFunction(isAbortedUserData);
                                           }
                                           else
                                           {
                                             result = TRUE;
                                           }

                                           return result;
                                         },NULL),
                                         CALLBACK_(isAbortedFunction,isAbortedUserData)
                                        );
                  }

                  // delete original storage
                  if (error == ERROR_NONE)
                  {
                    error = Storage_delete(&fromStorageInfo,
                                           storageSpecifier.archiveName
                                          );
                    if (error != ERROR_NONE)
                    {
                      (void)Storage_delete(&toStorageInfo,moveToArchivePath);
                    }
                  }

                  // set last checked date/time or revert
                  if (error == ERROR_NONE)
                  {
                    // set last checked date/time (ignore error)
                    (void)Index_setStorageState(indexHandle,
                                                storageId,
                                                indexState,
                                                Misc_getCurrentDateTime(),
                                                NULL  // errorMessage
                                               );
                  }
                  else
                  {
                    // revert storage name
                    (void)Index_updateStorage(indexHandle,
                                              storageId,
                                              NULL,  // hostName,
                                              NULL,  // userName,
                                              Storage_getName(NULL,&storageSpecifier,NULL),
                                              0LL,  // dateTime,
                                              size,
                                              NULL,  // comment,
                                              FALSE  // updateNewest
                                             );

                    // set index state: error
                    (void)Index_setStorageState(indexHandle,
                                                storageId,
                                                INDEX_STATE_ERROR,
                                                0LL,  // lastCheckedDateTime
                                                Error_getText(error)
                                               );
                  }

                  // done storage
                  Storage_done(&toStorageInfo);
                }
              }
              else
              {
                // set index state: error
                (void)Index_setStorageState(indexHandle,
                                            storageId,
                                            INDEX_STATE_ERROR,
                                            0LL,  // lastCheckedDateTime
                                            "file not found"
                                           );
              }

              // done storage
              Storage_done(&fromStorageInfo);
            }
          }
        }

        // update info
        doneStorageCount++;
        doneStorageSize += size;
        if (transferInfoFunction != NULL)
        {
          transferInfoFunction(storageId,
                               storageName,
                               size,
                               size,
                               doneStorageCount,
                               doneStorageSize,
                               totalStorageCount,
                               totalStorageSize,
                               transferInfoUserData
                              );
        }
      }
    }
    while (   !INDEX_ID_IS_NONE(storageId)
           && (   (isAbortedFunction == NULL)
               || !isAbortedFunction(isAbortedUserData)
              )
           && (error == ERROR_NONE)
          );
  }

  // free resources
  List_done(&maxToBandWidthList);
  List_done(&maxFromBandWidthList);
  String_delete(moveToArchivePath);
  String_delete(baseName);
  String_delete(directoryPath);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);

  return error;
}

/***********************************************************************\
* Name   : moveAllEntities
* Purpose: move all entities to requested destination
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors moveAllEntities(IndexHandle *indexHandle)
{
  Array            entityIdArray;
  StorageSpecifier moveToStorageSpecifier;
  StorageSpecifier storageSpecifier;
  String           directoryPath,baseName;
  String           moveToPath;
  String           moveToArchivePath;
  String           storageName;
  EntityList       entityList;
  IndexId          moveToEntityId;
  const JobNode    *jobNode;
  EntityList       jobEntityList;
  const EntityNode *jobEntityNode;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  String           moveToJobUUID,moveToJobName;
  ArchiveTypes     moveToArchiveType;
  uint64           moveToCreatedDateTime;
  JobOptions       jobOptions;
  AutoFreeList     autoFreeList;
  char             string[64];

  // init variables
  Array_init(&entityIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Storage_initSpecifier(&moveToStorageSpecifier);
  Storage_initSpecifier(&storageSpecifier);
  directoryPath     = String_new();
  baseName          = String_new();
  moveToPath        = String_new();
  moveToArchivePath = String_new();
  storageName       = String_new();
  moveToJobUUID     = String_new();
  moveToJobName     = String_new();
  Job_initOptions(&jobOptions);

  // get entity list
  getEntityList(&entityList,indexHandle);

  error = ERROR_NONE;
  do
  {
    // init variables
    moveToEntityId        = INDEX_ID_NONE;
    moveToArchiveType     = ARCHIVE_TYPE_NONE;
    moveToCreatedDateTime = 0LL;

    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      JOB_LIST_ITERATE(jobNode)
      {
        // get entity list for job with assigned persistence
        getJobEntityList(&jobEntityList,
                         &entityList,
                         jobNode->job.uuid,
                         &jobNode->job.options.persistenceList
                        );

        // find entity to move
        LIST_ITERATE(&jobEntityList,jobEntityNode)
        {
          if (   !jobEntityNode->lockedFlag
              && (jobEntityNode->persistenceNode != NULL)
              && !String_isEmpty(jobEntityNode->persistenceNode->moveTo)
              && !Array_contains(&entityIdArray,&jobEntityNode->entityId)
             )
          {
            // parse move-to name
            error = Storage_parseName(&moveToStorageSpecifier,jobEntityNode->persistenceNode->moveTo);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // check if some storages have to be moved
            error = Index_initListStorages(&indexQueryHandle,
                                           indexHandle,
                                           INDEX_ID_ANY,  // uuidId
                                           jobEntityNode->entityId,
                                           NULL,  // jobUUID
                                           NULL,  // scheduleUUID,
                                           NULL,  // indexIds
                                           0,  // indexIdCount
                                           INDEX_TYPESET_ALL,
                                             SET_VALUE(INDEX_STATE_NONE)
                                           | SET_VALUE(INDEX_STATE_OK)
                                           | SET_VALUE(INDEX_STATE_CREATE)
                                           | SET_VALUE(INDEX_STATE_UPDATE_REQUESTED)
                                           | SET_VALUE(INDEX_STATE_UPDATE),
                                           INDEX_MODE_SET_ALL,
                                           NULL,  // hostName
                                           NULL,  // userName
                                           NULL,  // name
                                           INDEX_STORAGE_SORT_MODE_NONE,
                                           DATABASE_ORDERING_NONE,
                                           0LL,  // offset
                                           INDEX_UNLIMITED
                                          );
            if (error == ERROR_NONE)
            {
              while (   INDEX_ID_IS_NONE(moveToEntityId)
                     && Index_getNextStorage(&indexQueryHandle,
                                             NULL,  // uuidId
                                             NULL,  // jobUUID
                                             NULL,  // entityId
                                             NULL,  // scheduleUUID
                                             NULL,  // hostName
                                             NULL,  // userName
                                             NULL,  // comment
                                             NULL,  // createdDateTime
                                             NULL,  // archiveType
                                             &storageId,
                                             storageName,
                                             NULL,  // createdDateTime,
                                             NULL,  // size
                                             NULL,  // indexState,
                                             NULL,  // indexMode,
                                             NULL,  // lastCheckedDateTime,
                                             NULL,  // errorMessage
                                             NULL,  // totalEntryCount
                                             NULL  // totalEntrySize
                                            )
                    )
              {
                // parse storage name
                error = Storage_parseName(&storageSpecifier,storageName);
                if (error != ERROR_NONE)
                {
                  continue;
                }

                // get path
                File_splitFileName(storageSpecifier.archiveName,directoryPath,baseName,NULL);

                // get move-to path name (expand macros)
                error = Archive_formatName(moveToPath,
                                           moveToStorageSpecifier.archiveName,
                                           EXPAND_MACRO_MODE_STRING,
                                           jobEntityNode->archiveType,
                                           NULL,  // scheduleTitle,
                                           NULL,  // customText,
                                           jobEntityNode->createdDateTime,
                                           NAME_PART_NUMBER_NONE
                                          );
                if (error != ERROR_NONE)
                {
                  continue;
                }

                // check if path is or storage type are different
                File_setFileName(moveToArchivePath,moveToPath);
                File_appendFileName(moveToArchivePath,baseName);
                if (Storage_equalSpecifiers(&storageSpecifier,
                                            storageName,
                                            &moveToStorageSpecifier,
                                            moveToArchivePath
                                           )
                   )
                {
                  continue;
                }

                // found enity to move
                moveToEntityId = jobEntityNode->entityId;
                String_set(moveToJobUUID,jobNode->job.uuid);
                String_set(moveToJobName,jobNode->name);
                moveToArchiveType     = jobEntityNode->archiveType;
                moveToCreatedDateTime = jobEntityNode->createdDateTime;
              }
              Index_doneList(&indexQueryHandle);
            }
          }
        }

        List_done(&jobEntityList);
      }
    }

    // move entity
    if (!INDEX_ID_IS_NONE(moveToEntityId))
    {
      Array_append(&entityIdArray,&moveToEntityId);

      AutoFree_init(&autoFreeList);
      error = ERROR_NONE;

      // lock entity
      if (error == ERROR_NONE)
      {
        error = Index_lockEntity(indexHandle,moveToEntityId);
        if (error == ERROR_NONE)
        {
          AUTOFREE_ADD(&autoFreeList,&moveToEntityId,{ (void)Index_unlockEntity(indexHandle,moveToEntityId); });
        }
      }

      // mount devices
      if (error == ERROR_NONE)
      {
        JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          jobNode = Job_findByUUID(moveToJobUUID);
          if (jobNode != NULL)
          {
            error = mountAll(&jobNode->job.options.mountList);
          }
        }
        if (error == ERROR_NONE)
        {
          AUTOFREE_ADD(&autoFreeList,&jobNode->job.options.mountList,{ (void)unmountAll(&jobNode->job.options.mountList); });
        }
      }

      // move entity
      if (error == ERROR_NONE)
      {
        error = moveEntity(indexHandle,
                           &jobOptions,
                           moveToEntityId,
                           &moveToStorageSpecifier,
                           moveToPath,
                           CALLBACK_(NULL,NULL),  // moveInfoFunction
                           CALLBACK_(NULL,NULL)  // isAbortedFunction
                          );
      }

      // unmount devices
      if (error == ERROR_NONE)
      {
        AUTOFREE_REMOVE(&autoFreeList,&jobNode->job.options.mountList);
        JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          jobNode = Job_findByUUID(moveToJobUUID);
          if (jobNode != NULL)
          {
            (void)unmountAll(&jobNode->job.options.mountList);
          }
        }
      }

      // unlock entity
      if (error == ERROR_NONE)
      {
        AUTOFREE_REMOVE(&autoFreeList,&moveToEntityId);
        (void)Index_unlockEntity(indexHandle,moveToEntityId);
      }

      if (error == ERROR_NONE)
      {
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_INDEX,
                   "Moved archives of entity #%"PRIi64" '%s': %s, created at %s to '%s'",
                   INDEX_DATABASE_ID(moveToEntityId),
                   String_cString(moveToJobName),
                   Archive_archiveTypeToString(moveToArchiveType),
                   Misc_formatDateTimeCString(string,sizeof(string),moveToCreatedDateTime,TIME_TYPE_LOCAL,NULL),
                   String_cString(Storage_getPrintableName(NULL,&moveToStorageSpecifier,moveToPath))
                   );
      }
      else
      {
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_ERROR,
                   "Failed to move archives of entity #%"PRIi64" '%s': %s, created at %s to '%s': %s",
                   INDEX_DATABASE_ID(moveToEntityId),
                   String_cString(moveToJobName),
                   Archive_archiveTypeToString(moveToArchiveType),
                   Misc_formatDateTimeCString(string,sizeof(string),moveToCreatedDateTime,TIME_TYPE_LOCAL,NULL),
                   String_cString(Storage_getPrintableName(NULL,&moveToStorageSpecifier,moveToPath)),
                   Error_getText(error)
                   );
      }

      AutoFree_done(&autoFreeList);
    }
  }
  while (   !INDEX_ID_IS_NONE(moveToEntityId)
         && !isQuit()
        );

  // free resources
  List_done(&entityList);
  Job_doneOptions(&jobOptions);
  String_delete(moveToJobName);
  String_delete(moveToJobUUID);
  String_delete(storageName);
  String_delete(moveToArchivePath);
  String_delete(moveToPath);
  String_delete(baseName);
  String_delete(directoryPath);
  Storage_doneSpecifier(&storageSpecifier);
  Storage_doneSpecifier(&moveToStorageSpecifier);
  Array_done(&entityIdArray);

  return error;
}

/***********************************************************************\
* Name   : persistenceThreadCode
* Purpose: purge expired entities/move entities thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void persistenceThreadCode(void)
{
  Errors      error;
  IndexHandle indexHandle;

  if (Index_isAvailable())
  {
    // open index
    do
    {
      error = Index_open(&indexHandle,NULL,10*MS_PER_SECOND);
    }
    while (!isQuit() && (error != ERROR_NONE));
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle,
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Cannot open index database (error: %s)",
                  Error_getText(error)
                 );
      return;
    }

    // run persistence thread
    while (!isQuit())
    {
      if (Index_isInitialized())
      {
        error = ERROR_NONE;

        // purge expired entities
        if (error == ERROR_NONE)
        {
          error = purgeExpiredEntities(&indexHandle,
                                       NULL,  // jobUUID
                                       ARCHIVE_TYPE_NONE
                                      );
        }

        // move all entities to destination
        if (error == ERROR_NONE)
        {
          error = moveAllEntities(&indexHandle);
        }

        // purge expired mounts
        purgeMounts(FALSE);

        // sleep
        if ((error == ERROR_NONE) || (Error_getCode(error) == ERROR_CODE_CONNECT_FAIL))
        {
          // sleep and check quit flag
          delayThread(SLEEP_TIME_PERSISTENCE_THREAD,&persistenceThreadTrigger);
        }
        else
        {
          // wait a short time and try again
          delayThread(30,NULL);
        }
      }
      else
      {
        // sleep and check quit flag
        delayThread(SLEEP_TIME_PERSISTENCE_THREAD,&persistenceThreadTrigger);
      }
    }

    // close index
    Index_close(&indexHandle);
  }
  else
  {
    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Index database not available - disabled purge expired"
               );
  }
}

/*---------------------------------------------------------------------*/

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

  return pauseFlags.indexUpdate || Job_isSomeActive();
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

  return isQuit();
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
         && !isQuit()
        )
  {
    Misc_udelay(500L*US_PER_MS);
  }
}

/***********************************************************************\
* Name   : updateIndexThreadCode
* Purpose: index update thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateIndexThreadCode(void)
{
  typedef struct CryptPasswordNode
  {
    LIST_NODE_HEADER(struct CryptPasswordNode);

    Password *cryptPassword;
    Key      cryptPrivateKey;
  } CryptPasswordNode;

  // list with index decrypt passwords
  typedef struct
  {
    LIST_HEADER(CryptPasswordNode);
  } CryptPasswordList;

  /***********************************************************************\
  * Name   : addCryptPassword
  * Purpose: add crypt password to index crypt password list
  * Input  : cryptPasswordList  - index crypt password list
  *          cryptPassword           - crypt password
  *          cryptPrivateKeyFileName - crypt private key file name
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void addCryptPassword(CryptPasswordList *cryptPasswordList, const Password *cryptPassword, const Key *cryptPrivateKey);
  auto void addCryptPassword(CryptPasswordList *cryptPasswordList, const Password *cryptPassword, const Key *cryptPrivateKey)
  {
    CryptPasswordNode *cryptPasswordNode;

    if (!LIST_CONTAINS(cryptPasswordList,
                       cryptPasswordNode,
                          Password_equals(cryptPasswordNode->cryptPassword,cryptPassword)
                       && Configuration_keyEquals(&cryptPasswordNode->cryptPrivateKey,cryptPrivateKey)
                      )
       )
    {
      cryptPasswordNode = LIST_NEW_NODE(CryptPasswordNode);
      if (cryptPasswordNode == NULL)
      {
        return;
      }

      cryptPasswordNode->cryptPassword = Password_duplicate(cryptPassword);
      if (cryptPrivateKey != NULL)
      {
        Configuration_duplicateKey(&cryptPasswordNode->cryptPrivateKey,cryptPrivateKey);
      }
      else
      {
        Configuration_initKey(&cryptPasswordNode->cryptPrivateKey);
      }

      List_append(cryptPasswordList,cryptPasswordNode);
    }
  }

  /***********************************************************************\
  * Name   : freeCryptPasswordNode
  * Purpose: free index crypt password
  * Input  : cryptPasswordNode - crypt password node
  *          userData               - user data (ignored)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeCryptPasswordNode(CryptPasswordNode *cryptPasswordNode, void *userData);
  auto void freeCryptPasswordNode(CryptPasswordNode *cryptPasswordNode, void *userData)
  {
    assert(cryptPasswordNode != NULL);

    UNUSED_VARIABLE(userData);

    Configuration_doneKey(&cryptPasswordNode->cryptPrivateKey);
    if (cryptPasswordNode->cryptPassword != NULL) Password_delete(cryptPasswordNode->cryptPassword);
  }

  // login list
  typedef struct
  {
    String   name;
    Password *password;
  } Login;

  typedef struct LoginNode
  {
    LIST_NODE_HEADER(struct LoginNode);

    String   name;
    Password *password;
  } LoginNode;

  typedef struct
  {
    LIST_HEADER(LoginNode);
  } LoginList;

  /***********************************************************************\
  * Name   : addLogin
  * Purpose: add login to login list
  * Input  : name     - login name
  *          password - login password
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void addLogin(LoginList *loginList, ConstString name, const Password *password);
  auto void addLogin(LoginList *loginList, ConstString name, const Password *password)
  {
    LoginNode *loginNode = LIST_NEW_NODE(LoginNode);
    assert(loginNode != NULL);

    loginNode->name     = String_duplicate(name);
    loginNode->password = Password_duplicate(password);
    List_append(loginList,loginNode);
  }

  /***********************************************************************\
  * Name   : freeLoginNode
  * Purpose: free login node
  * Input  : loginNode - login node
  *          userData  - user data (not used)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeLoginNode(LoginNode *loginNode, void *userData);
  auto void freeLoginNode(LoginNode *loginNode, void *userData)
  {
    assert(loginNode != NULL);

    UNUSED_VARIABLE(userData);

    Password_delete(loginNode->password);
    String_delete(loginNode->name);
  }

  /***********************************************************************\
  * Name   : loginEquals
  * Purpose:
  * Input  : -
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto bool loginEquals(const LoginNode *loginNode, const void *userData);
  auto bool loginEquals(const LoginNode *loginNode, const void *userData)
  {
    const Login *login = (const Login*)userData;

    assert(loginNode != NULL);
    assert(login != NULL);

    return    String_equals(loginNode->name,login->name)
           && Password_equals(loginNode->password,login->password);
  }

  IndexHandle       indexHandle;
  IndexId           uuidId,entityId,storageId;
  StorageSpecifier  addStorageSpecifier,storageSpecifier;
  String            storageName,printableStorageName;
  Login             login;
  LoginList         loginList;
  LoginNode         *loginNode;
  StorageInfo       storageInfo;
  CryptPasswordList cryptPasswordList;
  JobOptions        jobOptions;
  uint64            startTimestamp,endTimestamp;
  bool              loginFlag;
  Errors            error;
  const JobNode     *jobNode;
  CryptPasswordNode *cryptPasswordNode;
  ulong             totalEntryCount;
  uint64            totalEntrySize;

  // initialize variables
  List_init(&cryptPasswordList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeCryptPasswordNode,NULL));
  List_init(&loginList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeLoginNode,NULL));
  Storage_initSpecifier(&addStorageSpecifier);
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  if (Index_isAvailable())
  {
    // open index
    do
    {
      error = Index_open(&indexHandle,NULL,10*MS_PER_SECOND);
    }
    while (!isQuit() && (error != ERROR_NONE));
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle,
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Cannot open index database (error: %s)",
                  Error_getText(error)
                 );
      String_delete(printableStorageName);
      String_delete(storageName);
      Storage_doneSpecifier(&storageSpecifier);
      Storage_doneSpecifier(&addStorageSpecifier);
      List_done(&loginList);
      List_done(&cryptPasswordList);
      return;
    }

    // add/update index database
    while (!isQuit())
    {
      // pause
      pauseIndexUpdate();
      if (isQuit()) break;

      storageId = INDEX_ID_NONE;
      if (   Index_isInitialized()
          && globalOptions.indexDatabaseUpdateFlag
          && isMaintenanceTime(Misc_getCurrentDateTime(),NULL)
         )
      {
        // collect all job crypt passwords and crypt private keys (including no password and default crypt password)
        JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          JOB_LIST_ITERATE(jobNode)
          {
            if (!Password_isEmpty(&jobNode->job.options.cryptPassword))
            {
              addCryptPassword(&cryptPasswordList,&jobNode->job.options.cryptPassword,&jobNode->job.options.cryptPrivateKey);
            }
          }
        }
        addCryptPassword(&cryptPasswordList,&globalOptions.cryptPassword,NULL);
        addCryptPassword(&cryptPasswordList,NULL,NULL);  // no password

// TODO: lock via entityLock?
        // update index entries
        storageId = INDEX_ID_NONE;
        if (Index_findStorageByState(&indexHandle,
                                     INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED),
                                     &uuidId,
                                     NULL,  // jobUUID
                                     &entityId,
                                     NULL,  // scheduleUUID
                                     &storageId,
                                     storageName,
                                     NULL,  // createdDateTime
                                     NULL,  // size
                                     NULL,  // indexMode
                                     NULL,  // lastCheckedDateTime
                                     NULL,  // errorMessage
                                     NULL,  // totalEntryCount
                                     NULL  // totalEntrySize
                                    )
           )
        {
          // pause
          pauseIndexUpdate();
          if (isQuit())
          {
            break;
          }

          // parse storage name, get printable name
          error = Storage_parseName(&addStorageSpecifier,storageName);
          if (error == ERROR_NONE)
          {
            Storage_getPrintableName(printableStorageName,&addStorageSpecifier,NULL);
          }
          else
          {
            addStorageSpecifier.type = STORAGE_TYPE_NONE;
            String_clear(addStorageSpecifier.hostName);
            String_set(printableStorageName,storageName);
          }

          // collect possible login passwords for server
          List_clear(&loginList);
          login.name     = storageSpecifier.userName;
          login.password = &storageSpecifier.password;
          JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
          {
            JOB_LIST_ITERATE(jobNode)
            {
              error = Storage_parseName(&storageSpecifier,jobNode->job.storageName);
              if (   (error == ERROR_NONE)
                  && (storageSpecifier.type == addStorageSpecifier.type)
                  && String_equals(storageSpecifier.hostName,addStorageSpecifier.hostName)
                  && !Password_isEmpty(&storageSpecifier.password)
                  && !List_contains(&loginList,NULL,CALLBACK_((ListNodeEqualsFunction)loginEquals,&login))
                 )
              {
                addLogin(&loginList,storageSpecifier.userName,&storageSpecifier.password);
              }
            }
          }

          // init storage
          startTimestamp = 0LL;
          endTimestamp   = 0LL;
          Job_initOptions(&jobOptions);
          loginFlag      = FALSE;
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &addStorageSpecifier,
                               &jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_LOW,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
          loginFlag = (error == ERROR_NONE);
          LIST_ITERATEX(&loginList,loginNode,!loginFlag)
          {
            String_set(addStorageSpecifier.userName,loginNode->name);
            Password_set(&addStorageSpecifier.password,loginNode->password);
            error = Storage_init(&storageInfo,
                                 NULL,  // masterIO
                                 &addStorageSpecifier,
                                 &jobOptions,
                                 &globalOptions.indexDatabaseMaxBandWidthList,
                                 SERVER_CONNECTION_PRIORITY_LOW,
                                 CALLBACK_(NULL,NULL),  // storageUpdateProgress
                                 CALLBACK_(NULL,NULL),  // getNamePassword
                                 CALLBACK_(NULL,NULL),  // requestVolume
                                 CALLBACK_(NULL,NULL),  // isPause
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );
            loginFlag = (error == ERROR_NONE);
          }
          if (loginFlag)
          {
            // index archive contents
            printInfo(4,"Start create index for '%s'\n",String_cString(printableStorageName));
            plogMessage(NULL,  // logHandle
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Start create index for '%s'",
                        String_cString(printableStorageName)
                       );

            // lock entity (if exists)
            if (!INDEX_ID_IS_NONE(entityId))
            {
              // lock
              Index_lockEntity(&indexHandle,entityId);
            }

            // set state 'update'
            (void)Index_setStorageState(&indexHandle,
                                        storageId,
                                        INDEX_STATE_UPDATE,
                                        0LL,  // lastCheckedDateTime
                                        NULL  // errorMessage
                                       );

            // try to create index
            LIST_ITERATE(&cryptPasswordList,cryptPasswordNode)
            {
              // set password/key
              Password_set(&jobOptions.cryptPassword,cryptPasswordNode->cryptPassword);
              Configuration_copyKey(&jobOptions.cryptPrivateKey,&cryptPasswordNode->cryptPrivateKey);

              // index update
              startTimestamp = Misc_getTimestamp();
              error = Archive_updateIndex(&indexHandle,
                                          uuidId,
                                          entityId,
                                          storageId,
                                          &storageInfo,
                                          &totalEntryCount,
                                          &totalEntrySize,
                                          CALLBACK_(indexPauseCallback,NULL),
                                          CALLBACK_(indexAbortCallback,NULL),
                                          NULL  // logHandle
                                         );
              endTimestamp = Misc_getTimestamp();

              // stop if done, interrupted, or quit
              if (   (error == ERROR_NONE)
                  || (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
                  || isQuit()
                 )
              {
                break;
              }
            }

            // set index state
            if      (error == ERROR_NONE)
            {
              // done
              printInfo(4,"Done create index for '%s'\n",String_cString(printableStorageName));
              (void)Index_setStorageState(&indexHandle,
                                          storageId,
                                          INDEX_STATE_OK,
                                          Misc_getCurrentDateTime(),
                                          NULL  // errorMessage
                                         );
              plogMessage(NULL,  // logHandle,
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Done index for '%s', %"PRIu64" entries/%.1f%s (%"PRIu64" bytes), %lumin:%02lus",
                          String_cString(printableStorageName),
                          totalEntryCount,
                          BYTES_SHORT(totalEntrySize),
                          BYTES_UNIT(totalEntrySize),
                          totalEntrySize,
                          ((endTimestamp-startTimestamp)/US_PER_SECOND)/60,
                          ((endTimestamp-startTimestamp)/US_PER_SECOND)%60
                         );
            }
            else if (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
            {
              // interrupt
              (void)Index_setStorageState(&indexHandle,
                                          storageId,
                                          INDEX_STATE_UPDATE_REQUESTED,
                                          0LL,  // lastCheckedTimestamp
                                          NULL  // errorMessage
                                         );
            }
            else
            {
              // error
              printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));
              (void)Index_setStorageState(&indexHandle,
                                          storageId,
                                          INDEX_STATE_ERROR,
                                          0LL,  // lastCheckedDateTime
                                          "%s (error code: %d)",
                                          Error_getText(error),
                                          Error_getCode(error)
                                         );
              plogMessage(NULL,  // logHandle
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Failed to create index for '%s' (error: %s)",
                          String_cString(printableStorageName),
                          Error_getText(error)
                         );
            }

            // unlock entity (if exists)
            if (!INDEX_ID_IS_NONE(entityId))
            {
              // unlock
              (void)Index_unlockEntity(&indexHandle,entityId);
            }

            // done storage
            (void)Storage_done(&storageInfo);
          }
          else
          {
            (void)Index_setStorageState(&indexHandle,
                                        storageId,
                                        INDEX_STATE_ERROR,
                                        0LL,
                                        "Cannot initialize storage (error: %s)",
                                        Error_getText(error)
                                       );
          }
          Job_doneOptions(&jobOptions);
        }

        // free resources
        List_done(&cryptPasswordList);
      }
      if (isQuit())
      {
        break;
      }

      // sleep and check quit flag/trigger
      if (INDEX_ID_IS_NONE(storageId))
      {
        delayThread(SLEEP_TIME_INDEX_THREAD,&updateIndexThreadTrigger);
      }
    }

    // close index
    Index_close(&indexHandle);
  }
  else
  {
    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Index database not available - disabled index update"
               );
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);
  Storage_doneSpecifier(&addStorageSpecifier);
  List_done(&loginList);
  List_done(&cryptPasswordList);
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
  StorageSpecifier      storageSpecifier;
  String                directoryName;
  String                storageDirectoryName;
  const JobNode         *jobNode;
  const PersistenceNode *persistenceNode;

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  directoryName        = String_new();
  storageDirectoryName = String_new();

  // collect storage locations to check for BAR files
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      if (Storage_parseName(&storageSpecifier,jobNode->job.storageName) == ERROR_NONE)
      {
        // get directory part without macros
        File_getDirectoryName(directoryName,storageSpecifier.archiveName);
        while (   !String_isEmpty(directoryName)
               && Misc_hasMacros(directoryName)
              )
        {
          File_getDirectoryName(directoryName,directoryName);
        }

        // add to list
        Storage_getName(storageDirectoryName,&storageSpecifier,directoryName);
        if (   !String_isEmpty(storageDirectoryName)
            && !StringList_contains(storageDirectoryList,storageDirectoryName)
           )
        {
          StringList_append(storageDirectoryList,storageDirectoryName);
        }
      }

      LIST_ITERATE(&jobNode->job.options.persistenceList,persistenceNode)
      {
        if (   !String_isEmpty(persistenceNode->moveTo)
            && Storage_parseName(&storageSpecifier,persistenceNode->moveTo) == ERROR_NONE)
        {
          // get directory part without macros
          String_set(directoryName,storageSpecifier.archiveName);
          while (   !String_isEmpty(directoryName)
                 && Misc_hasMacros(directoryName)
                )
          {
            File_getDirectoryName(directoryName,directoryName);
          }

          // add to list
          Storage_getName(storageDirectoryName,&storageSpecifier,directoryName);
          if (   !String_isEmpty(storageDirectoryName)
              && !StringList_contains(storageDirectoryList,storageDirectoryName)
             )
          {
            StringList_append(storageDirectoryList,storageDirectoryName);
          }
        }
      }
    }
  }

  // free resources
  String_delete(storageDirectoryName);
  String_delete(directoryName);
  Storage_doneSpecifier(&storageSpecifier);
}

/***********************************************************************\
* Name   : autoAddUpdateIndex
* Purpose: add missing/update outdated indizes
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void autoAddUpdateIndex(IndexHandle *indexHandle)
{
  StringList                 storageDirectoryList;
  StorageSpecifier           storageSpecifier;
  String                     baseName;
  String                     pattern;
  String                     printableStorageName;
  String                     storageDirectoryName;
  JobOptions                 jobOptions;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  IndexId                    storageId;
  IndexStates                indexState;
  uint64                     lastCheckedDateTime;

  // init variables
  StringList_init(&storageDirectoryList);
  Storage_initSpecifier(&storageSpecifier);
  baseName             = String_new();
  pattern              = String_new();
  printableStorageName = String_new();
  Job_initOptions(&jobOptions);

  // collect storage locations to check for BAR files
  getStorageDirectories(&storageDirectoryList);

  // check storage locations for BAR files, send index update request
  while (!StringList_isEmpty(&storageDirectoryList))
  {
    storageDirectoryName = StringList_removeFirst(&storageDirectoryList,NULL);

    error = Storage_parseName(&storageSpecifier,storageDirectoryName);
    if (error == ERROR_NONE)
    {
      if (   (storageSpecifier.type == STORAGE_TYPE_FILESYSTEM)
          || (storageSpecifier.type == STORAGE_TYPE_FTP       )
          || (storageSpecifier.type == STORAGE_TYPE_SSH       )
          || (storageSpecifier.type == STORAGE_TYPE_SCP       )
          || (storageSpecifier.type == STORAGE_TYPE_SFTP      )
          || (storageSpecifier.type == STORAGE_TYPE_WEBDAV    )
          || (storageSpecifier.type == STORAGE_TYPE_WEBDAVS   )
          || (storageSpecifier.type == STORAGE_TYPE_SMB       )
         )
      {
        // get base directory
        File_setFileName(baseName,storageSpecifier.archiveName);
        do
        {
          error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                            &storageSpecifier,
                                            baseName,
                                            &jobOptions,
                                            SERVER_CONNECTION_PRIORITY_LOW
                                           );
          if (error == ERROR_NONE)
          {
            Storage_closeDirectoryList(&storageDirectoryListHandle);
          }
          else
          {
            File_getDirectoryName(baseName,baseName);
          }
        }
        while ((error != ERROR_NONE) && !String_isEmpty(baseName));

        if (!String_isEmpty(baseName))
        {
          // read directory and scan all sub-directories for .bar files if possible
          pprintInfo(4,
                     "INDEX: ",
                     "Auto-index scan '%s'\n",
                     String_cString(Storage_getPrintableName(printableStorageName,&storageSpecifier,baseName))
                    );
          File_appendFileNameCString(File_setFileName(pattern,baseName),"*.bar");
          (void)Storage_forAll(&storageSpecifier,
                               baseName,
                               String_cString(pattern),
                               TRUE,  // skipUnreadableFlag
                               CALLBACK_INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                               {
                                 Errors error;
                                 uint64 now;

                                 assert(fileInfo != NULL);

                                 UNUSED_VARIABLE(userData);

                                 now = Misc_getCurrentDateTime();

                                 // to avoid add/update on currently created archive, wait for min. 30min after creation
                                 if (now > (fileInfo->timeLastChanged+30*60))
                                 {
                                   error = Storage_parseName(&storageSpecifier,storageName);
                                   if (error == ERROR_NONE)
                                   {
                                     // check entry type and file name
                                     switch (fileInfo->type)
                                     {
                                       case FILE_TYPE_FILE:
                                       case FILE_TYPE_LINK:
                                       case FILE_TYPE_HARDLINK:
                                         // get printable name
                                         Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

                                         // wait until index is unused
                                         while (   Index_isIndexInUse()
                                                && !isQuit()
                                               )
                                         {
                                           Misc_mdelay(5*MS_PER_SECOND);
                                         }
                                         if (isQuit())
                                         {
                                           return ERROR_NONE;
                                         }

                                         // get index id, request index update
                                         if (Index_findStorageByName(indexHandle,
                                                                     &storageSpecifier,
                                                                     NULL,  // archiveName
                                                                     NULL,  // uuidId
                                                                     NULL,  // entityId
                                                                     NULL,  // jobUUID
                                                                     NULL,  // scheduleUUID
                                                                     &storageId,
                                                                     NULL,  // createdDateTime
                                                                     NULL,  // size
                                                                     &indexState,
                                                                     NULL,  // indexMode
                                                                     &lastCheckedDateTime,
                                                                     NULL,  // errorMessage
                                                                     NULL,  // totalEntryCount
                                                                     NULL  // totalEntrySize
                                                                    )
                                            )
                                         {
                                           // already in index -> check if modified/state

                                           if      (fileInfo->timeModified > lastCheckedDateTime)
                                           {
                                             // modified -> request update index
                                             error = Index_setStorageState(indexHandle,
                                                                           storageId,
                                                                           INDEX_STATE_UPDATE_REQUESTED,
                                                                           now,
                                                                           NULL  // errorMessage
                                                                          );
                                             if (error == ERROR_NONE)
                                             {
                                               plogMessage(NULL,  // logHandle,
                                                           LOG_TYPE_INDEX,
                                                           "INDEX",
                                                           "Auto requested update index for '%s'",
                                                           String_cString(printableStorageName)
                                                          );
                                             }
                                           }
                                           else if (indexState == INDEX_STATE_OK)
                                           {
                                             // set last checked date/time
                                             error = Index_setStorageState(indexHandle,
                                                                           storageId,
                                                                           INDEX_STATE_OK,
                                                                           now,
                                                                           NULL  // errorMessage
                                                                          );
                                           }
                                         }
                                         else
                                         {
                                           // add to index
                                           error = Index_newStorage(indexHandle,
                                                                    INDEX_ID_NONE, // uuidId
                                                                    INDEX_ID_NONE, // entityId
                                                                    NULL,  // hostName
                                                                    NULL,  // userName
                                                                    printableStorageName,
                                                                    0LL,  // createdDateTime
                                                                    0LL,  // size
                                                                    INDEX_STATE_UPDATE_REQUESTED,
                                                                    INDEX_MODE_AUTO,
                                                                    NULL  // storageId
                                                                   );
                                           if (error == ERROR_NONE)
                                           {
                                             plogMessage(NULL,  // logHandle,
                                                         LOG_TYPE_INDEX,
                                                         "INDEX",
                                                         "Auto requested add index for '%s'",
                                                         String_cString(printableStorageName)
                                                        );
                                           }
                                         }
                                         break;
                                       default:
                                         break;
                                     }
                                   }
                                 }
                                 else
                                 {
                                   error = ERROR_NONE;
                                 }

                                 if (!isQuit())
                                 {
                                   return error;
                                 }
                                 else
                                 {
                                   return ERROR_ABORTED;
                                 }
                               },NULL),
                               CALLBACK_(NULL,NULL)
                              );
        }
      }
    }

    String_delete(storageDirectoryName);
  }

  // free resources
  Job_doneOptions(&jobOptions);
  String_delete(printableStorageName);
  String_delete(pattern);
  String_delete(baseName);
  Storage_doneSpecifier(&storageSpecifier);
  StringList_done(&storageDirectoryList);
}

/***********************************************************************\
* Name   : autoCleanIndex
* Purpose: purge expired indizes
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void autoCleanIndex(IndexHandle *indexHandle)
{
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  Errors           error;
  IndexId          purgeStorageId;
  uint64           now;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  uint64           createdDateTime;
  IndexStates      indexState;
  IndexModes       indexMode;
  uint64           lastCheckedDateTime;
  char             buffer[64];

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  do
  {
    purgeStorageId = INDEX_ID_NONE;

    // get not existing and expired storage index to purge
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error == ERROR_NONE)
    {
      now = Misc_getCurrentDateTime();
      while (   !isQuit()
             && !INDEX_ID_IS_NONE(purgeStorageId)
             && Index_getNextStorage(&indexQueryHandle,
                                     NULL,  // uuidId
                                     NULL,  // jobUUID
                                     NULL,  // entityId
                                     NULL,  // scheduleUUID
                                     NULL,  // hostName
                                     NULL,  // userName
                                     NULL,  // comment
                                     NULL,  // createdDateTime
                                     NULL,  // archiveType
                                     &storageId,
                                     storageName,
                                     &createdDateTime,
                                     NULL,  // size
                                     &indexState,
                                     &indexMode,
                                     &lastCheckedDateTime,
                                     NULL,  // errorMessage
                                     NULL,  // totalEntryCount
                                     NULL  // totalEntrySize
                                    )
            )
      {
        // get printable name (if possible)
        error = Storage_parseName(&storageSpecifier,storageName);
        if (error == ERROR_NONE)
        {
          Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
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
          purgeStorageId = storageId;
        }
      }
      Index_doneList(&indexQueryHandle);
    }

    // purge expired storage index
    if (!INDEX_ID_IS_NONE(purgeStorageId))
    {
      IndexStorage_purge(indexHandle,
                         purgeStorageId,
                         NULL  // progressInfo
                        );

      plogMessage(NULL,  // logHandle,
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Auto deleted index for '%s', last checked %s",
                  String_cString(printableStorageName),
                  Misc_formatDateTimeCString(buffer,sizeof(buffer),lastCheckedDateTime,TIME_TYPE_LOCAL,NULL)
                 );
    }
  }
  while (   !INDEX_ID_IS_NONE(purgeStorageId)
         && isMaintenanceTime(Misc_getCurrentDateTime(),NULL)
        );

  // free resources
  String_delete(storageName);
  String_delete(printableStorageName);
  Storage_doneSpecifier(&storageSpecifier);
}

/***********************************************************************\
* Name   : autoIndexThreadCode
* Purpose: auto index request thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void autoIndexThreadCode(void)
{
  IndexHandle indexHandle;
  Errors      error;

  // initialize variables

  if (Index_isAvailable())
  {
    // open index
    do
    {
      error = Index_open(&indexHandle,NULL,10*MS_PER_SECOND);
    }
    while (!isQuit() && (error != ERROR_NONE));
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle,
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Cannot open index database (error: %s)",
                  Error_getText(error)
                 );
      return;
    }

    // run continuous check for auto index
    while (!isQuit())
    {
      if (isMaintenanceTime(Misc_getCurrentDateTime(),NULL))
      {
      // pause
      pauseIndexUpdate();
      if (isQuit())
      {
        break;
      }

      if (   Index_isInitialized()
          && globalOptions.indexDatabaseAutoUpdateFlag
         )
      {
        autoAddUpdateIndex(&indexHandle);
        if (isQuit())
        {
          break;
        }

        autoCleanIndex(&indexHandle);
        if (isQuit())
        {
          break;
        }
      }
      }
      if (isQuit())
      {
        break;
      }

      // sleep and check quit flag
      delayThread(SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD,&autoIndexThreadTrigger);
    }

    // done index
    Index_close(&indexHandle);
  }
  else
  {
    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Index database not available - disabled auto-index"
               );
  }

  // free resources
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getCryptPasswordFromConfig
* Purpose: get crypt password from config call-back
* Input  : name          - name variable (not used)
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

LOCAL Errors getCryptPasswordFromConfig(String        name,
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

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(passwordType);
  UNUSED_VARIABLE(text);
  UNUSED_VARIABLE(validateFlag);
  UNUSED_VARIABLE(weakCheckFlag);

  if (Password_isEmpty(&jobNode->job.options.cryptPassword))
  {
    Password_set(password,&jobNode->job.options.cryptPassword);
    return ERROR_NONE;
  }
  else
  {
    return ERROR_NO_CRYPT_PASSWORD;
  }
}

/***********************************************************************\
* Name   : createRunningInfo
* Purpose: create running info
* Input  : error       - error code
*          runningInfo - running info data
*          userData    - user data: job node
* Output : -
* Return :
* Notes  : -
\***********************************************************************/

LOCAL void createRunningInfo(Errors      error,
                             RunningInfo *runningInfo,
                             void        *userData
                            )
{
  JobNode *jobNode = (JobNode*)userData;

  assert(jobNode != NULL);
  assert(runningInfo != NULL);

  UNUSED_VARIABLE(error);

  // Note: update progress info has low priority; only try for 2s
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,2*MS_PER_SECOND)
  {
    double  entriesPerSecondAverage,bytesPerSecondAverage,storageBytesPerSecondAverage;
    ulong   restFiles;
    uint64  restBytes;
    uint64  restStorageBytes;
    ulong   estimatedRestTime;

    setRunningInfo(&jobNode->runningInfo,runningInfo);

    // calculate statics values
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     jobNode->runningInfo.progress.done.count      );
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       jobNode->runningInfo.progress.done.size       );
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,jobNode->runningInfo.progress.storage.doneSize);
    entriesPerSecondAverage      = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.entriesPerSecondFilter     );
    bytesPerSecondAverage        = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.bytesPerSecondFilter       );
    storageBytesPerSecondAverage = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.storageBytesPerSecondFilter);

    // calculate rest values
    restFiles         = (jobNode->runningInfo.progress.total.count       > jobNode->runningInfo.progress.done.count      ) ? jobNode->runningInfo.progress.total.count      -jobNode->runningInfo.progress.done.count       : 0L;
    restBytes         = (jobNode->runningInfo.progress.total.size        > jobNode->runningInfo.progress.done.size       ) ? jobNode->runningInfo.progress.total.size       -jobNode->runningInfo.progress.done.size        : 0LL;
    restStorageBytes  = (jobNode->runningInfo.progress.storage.totalSize > jobNode->runningInfo.progress.storage.doneSize) ? jobNode->runningInfo.progress.storage.totalSize-jobNode->runningInfo.progress.storage.doneSize : 0LL;

    // calculate estimated rest time
    estimatedRestTime = 0L;
    if (entriesPerSecondAverage      > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restFiles       /entriesPerSecondAverage     )); }
    if (bytesPerSecondAverage        > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restBytes       /bytesPerSecondAverage       )); }
    if (storageBytesPerSecondAverage > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restStorageBytes/storageBytesPerSecondAverage)); }

    // calulcate performance values
    jobNode->runningInfo.entriesPerSecond      = Misc_performanceFilterGetValue(&jobNode->runningInfo.entriesPerSecondFilter     ,10);
    jobNode->runningInfo.bytesPerSecond        = Misc_performanceFilterGetValue(&jobNode->runningInfo.bytesPerSecondFilter       ,10);
    jobNode->runningInfo.storageBytesPerSecond = Misc_performanceFilterGetValue(&jobNode->runningInfo.storageBytesPerSecondFilter,10);
    jobNode->runningInfo.estimatedRestTime     = estimatedRestTime;
  }
}

/***********************************************************************\
* Name   : restoreRunningInfo
* Purpose: restore running info
* Input  : runningInfo - running info data
*          userData    - user data: job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void restoreRunningInfo(const RunningInfo *runningInfo,
                              void              *userData
                             )
{
  JobNode *jobNode = (JobNode*)userData;

  assert(jobNode != NULL);
  assert(runningInfo != NULL);

  // Note: update progress info has low priority; only try for 2s
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,2*MS_PER_SECOND)
  {
    setRunningInfo(&jobNode->runningInfo,runningInfo);

    // calculate estimated rest time
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     runningInfo->progress.done.count      );
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       runningInfo->progress.done.size       );
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,runningInfo->progress.storage.doneSize);

    jobNode->runningInfo.estimatedRestTime = 0;
  }
}

/***********************************************************************\
* Name   : isPauseCreate
* Purpose: check if pause create
* Input  : userData - user data (not used)
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isPauseCreate(void *userData)
{
  UNUSED_VARIABLE(userData);

  return pauseFlags.create;
}

/***********************************************************************\
* Name   : isPauseStorage
* Purpose: check if pause storage
* Input  : userData - user data (not used)
* Output : -
* Return : TRUE iff pause storage
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isPauseStorage(void *userData)
{
  UNUSED_VARIABLE(userData);

  return pauseFlags.storage;
}

/***********************************************************************\
* Name   : isAborted
* Purpose: check if job is aborted
* Input  : userData - job node
* Output : -
* Return : TRUE iff aborted
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isAborted(void *userData)
{
  const JobNode *jobNode = (const JobNode*)userData;

  assert(jobNode != NULL);

  return jobNode->requestedAbortFlag;
}

/***********************************************************************\
* Name   : jobThreadCode
* Purpose: job execution (create/restore) thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobThreadCode(void)
{
  StorageSpecifier storageSpecifier;
  String           jobName;
  String           storageName;
  String           directory;
  EntryList        includeEntryList;
  PatternList      excludePatternList;
  String           customText;
  String           byName;
  ConnectorInfo    *connectorInfo;
  AggregateInfo    jobAggregateInfo,scheduleAggregateInfo;
  StringMap        resultMap;
  JobNode          *jobNode;
  ArchiveTypes     archiveType;
  uint64           startDateTime;
  JobOptions       jobOptions;
  String           nextJobName;
  StaticString     (nextJobUUID,MISC_UUID_STRING_LENGTH);
  StaticString     (nextScheduleUUID,MISC_UUID_STRING_LENGTH);
  uint64           nextScheduleDateTime;
  LogHandle        logHandle;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  StaticString     (entityUUID,MISC_UUID_STRING_LENGTH);
  uint64           executeStartDateTime,executeEndDateTime;
  StringList       storageNameList;
  TextMacros       (textMacros,14);
  StaticString     (s,64);
  IndexHandle      indexHandle;
  bool             isIndexOpened;
  uint             n;
  Errors           error;
  ScheduleNode     *scheduleNode;

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  jobName            = String_new();
  storageName        = String_new();
  directory          = String_new();
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  customText         = String_new();
  byName             = String_new();
  connectorInfo      = NULL;
  initAggregateInfo(&jobAggregateInfo);
  initAggregateInfo(&scheduleAggregateInfo);
  resultMap          = StringMap_new();
  if (resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  jobNode            = NULL;
  archiveType        = ARCHIVE_TYPE_UNKNOWN;
  startDateTime      = 0LL;
  nextJobName        = String_new();

  while (!isQuit())
  {
    // wait and get next job to run
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // wait and get next job to execute
      do
      {
        // first check for a continuous job to run
        jobNode = jobList.head;
        while (   !isQuit()
               && (jobNode != NULL)
               && (   (jobNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
                   || !Job_isWaiting(jobNode->jobState)
                   || (Job_isRemote(jobNode) && !isSlavePaired(jobNode))
                  )
              )
        {
          jobNode = jobNode->next;
        }

        if (jobNode == NULL)
        {
          // next check for other job types to run
          jobNode = jobList.head;
          while (   !isQuit()
                 && (jobNode != NULL)
                 && (   !Job_isWaiting(jobNode->jobState)
                     || (Job_isRemote(jobNode) && !isSlavePaired(jobNode))
                    )
                )
          {
            jobNode = jobNode->next;
          }
        }

        // if no job to execute -> wait
        if (!isQuit() && (jobNode == NULL)) Job_listWaitModifed(LOCK_TIMEOUT);
      }
      while (!isQuit() && (jobNode == NULL));
      if (isQuit())
      {
        Job_listUnlock();
        break;
      }
      assert(jobNode != NULL);

      // get copy of mandatory job data
      String_set(jobName,jobNode->name);
      String_set(storageName,jobNode->job.storageName);
      String_set(jobUUID,jobNode->job.uuid);
      String_set(scheduleUUID,jobNode->scheduleUUID);
      EntryList_clear(&includeEntryList); EntryList_copy(&includeEntryList,&jobNode->job.includeEntryList,CALLBACK_(NULL,NULL));
      PatternList_clear(&excludePatternList); PatternList_copy(&excludePatternList,&jobNode->job.excludePatternList,CALLBACK_(NULL,NULL));
      Job_copyOptions(&jobOptions,&jobNode->job.options);
      archiveType         = jobNode->archiveType;
      String_set(customText,jobNode->customText);
      startDateTime       = jobNode->startDateTime;
      String_set(byName,jobNode->byName);

      jobOptions.testCreatedArchivesFlag = jobNode->testCreatedArchives;
      jobOptions.noStorage               = jobNode->noStorage;
      jobOptions.dryRun                  = jobNode->dryRun;

      // get and lock connector (if remote job)
      connectorInfo = Job_connectorLock(jobNode,LOCK_TIMEOUT);

      // start job
      Job_start(jobNode);
    }
    if (jobNode == NULL)
    {
      break;
    }

    // Note: job is now protected by running state from being deleted

    // init log
    initLog(&logHandle);

    // get info string
    String_clear(s);
    if (Job_isRemote(jobNode))
    {
      String_appendFormat(s," on '%S'",jobNode->job.slaveHost.name);
    }
    if (jobOptions.noStorage || jobOptions.dryRun)
    {
      String_appendCString(s," (");
      n = 0;
      if (jobOptions.noStorage)
      {
        if (n > 0) String_appendCString(s,", ");
        String_appendCString(s,"no-storage");
        n++;
      }
      if (jobOptions.dryRun)
      {
        if (n > 0) String_appendCString(s,", ");
        String_appendCString(s,"dry-run");
        n++;
      }
      String_appendCString(s,")");
    }

    // log
    switch (jobNode->jobType)
    {
      case JOB_TYPE_NONE:
        break;
      case JOB_TYPE_CREATE:
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Start job '%s'%s %s%s%s",
                   String_cString(jobName),
                   !String_isEmpty(s) ? String_cString(s) : "",
                   Archive_archiveTypeToString(archiveType),
                   !String_isEmpty(byName) ? " by " : "",
                   String_cString(byName)
                  );
        break;
      case JOB_TYPE_RESTORE:
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Start restore%s%s%s",
                   !String_isEmpty(s) ? String_cString(s) : "",
                   !String_isEmpty(byName) ? " by " : "",
                   String_cString(byName)
                  );
        break;
    }

    // open index (depending on local/remote job)
    if (Index_isAvailable())
    {
      do
      {
        error = Index_open(&indexHandle,jobNode->masterIO,INDEX_TIMEOUT);
      }
      while (isQuit() && (error != ERROR_NONE));

      isIndexOpened = (error == ERROR_NONE);
    }
    else
    {
      isIndexOpened = FALSE;
    }

    // parse storage name
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      jobNode->runningInfo.error = Storage_parseName(&storageSpecifier,storageName);
      if (jobNode->runningInfo.error != ERROR_NONE)
      {
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Aborted job '%s': invalid storage '%s' (error: %s)",
                   String_cString(jobName),
                   String_cString(storageName),
                   Error_getText(jobNode->runningInfo.error)
                  );
      }
    }

    // get start date/time
    executeStartDateTime = Misc_getCurrentDateTime();

    // get next schedule
    nextScheduleDateTime = getNextSchedule(jobNode,
                                           scheduleUUID,
                                           executeStartDateTime,
                                           nextJobName,
                                           nextJobUUID,
                                           nextScheduleUUID
                                          );

    // job pre-process command
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      if (!String_isEmpty(jobNode->job.options.preProcessScript))
      {
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_STRING ("%name",                jobName,                                                      NULL);
          TEXT_MACRO_X_STRING ("%archive",             storageName,                                                  NULL);
          TEXT_MACRO_X_CSTRING("%type",                Archive_archiveTypeToString(archiveType),                     NULL);
          TEXT_MACRO_X_CSTRING("%T",                   Archive_archiveTypeToShortString(archiveType),                NULL);
          TEXT_MACRO_X_STRING ("%directory",           File_getDirectoryName(directory,storageSpecifier.archiveName),NULL);
          TEXT_MACRO_X_STRING ("%file",                storageSpecifier.archiveName,                                 NULL);
          if (nextScheduleDateTime < MAX_UINT64)
          {
            TEXT_MACRO_X_STRING ("%nextJobName",         nextJobName,                                                NULL);
            TEXT_MACRO_X_STRING ("%nextJobUUID",         nextJobUUID,                                                NULL);
            TEXT_MACRO_X_STRING ("%nextScheduleUUID",    nextScheduleUUID,                                           NULL);
            TEXT_MACRO_X_UINT64 ("%nextSchedule",        (nextScheduleDateTime >= executeStartDateTime)
                                                           ? nextScheduleDateTime-executeStartDateTime
                                                           : 0,                                                      NULL);
            TEXT_MACRO_X_UINT64 ("%nextScheduleDateTime",nextScheduleDateTime,                                       NULL);
          }
          else
          {
            TEXT_MACRO_X_CSTRING("%nextJobName",         "",                                                         NULL);
            TEXT_MACRO_X_CSTRING("%nextJobUUID",         "",                                                         NULL);
            TEXT_MACRO_X_CSTRING("%nextScheduleUUID",    "",                                                         NULL);
            TEXT_MACRO_X_CSTRING("%nextSchedule",        "",                                                         NULL);
            TEXT_MACRO_X_CSTRING("%nextScheduleDateTime","",                                                         NULL);
          }
        }
        error = executeTemplate(String_cString(jobNode->job.options.preProcessScript),
                                executeStartDateTime,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        if (error == ERROR_NONE)
        {
          logMessage(&logHandle,
                     LOG_TYPE_INFO,
                     "Executed pre-command for '%s'",
                     String_cString(jobName)
                    );
        }
        else
        {
          if (jobNode->runningInfo.error == ERROR_NONE) jobNode->runningInfo.error = error;
          logMessage(&logHandle,
                     LOG_TYPE_ALWAYS,
                     "Aborted job '%s': pre-command fail (error: %s)",
                     String_cString(jobName),
                     Error_getText(jobNode->runningInfo.error)
                    );
        }
      }
    }

    // execute job
    if      (!Job_isRemote(jobNode))
    {
      // local job -> run on this machine

      // get include/excluded entries from commands
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.includeFileCommand))
        {
          jobNode->runningInfo.error = addIncludeListFromCommand(ENTRY_TYPE_FILE,&includeEntryList,String_cString(jobNode->job.options.includeFileCommand));
        }
      }
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.includeImageCommand))
        {
          jobNode->runningInfo.error = addIncludeListFromCommand(ENTRY_TYPE_IMAGE,&includeEntryList,String_cString(jobNode->job.options.includeImageCommand));
        }
      }
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.excludeCommand))
        {
          jobNode->runningInfo.error = addExcludeListFromCommand(&excludePatternList,String_cString(jobNode->job.options.excludeCommand));
        }
      }

      // get include/excluded entries from file
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.includeFileListFileName))
        {
          jobNode->runningInfo.error = addIncludeListFromFile(ENTRY_TYPE_FILE,&includeEntryList,String_cString(jobNode->job.options.includeFileListFileName));
        }
      }
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.includeImageListFileName))
        {
          jobNode->runningInfo.error = addIncludeListFromFile(ENTRY_TYPE_IMAGE,&includeEntryList,String_cString(jobNode->job.options.includeImageListFileName));
        }
      }
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (!String_isEmpty(jobNode->job.options.excludeListFileName))
        {
          jobNode->runningInfo.error = addExcludeListFromFile(&excludePatternList,String_cString(jobNode->job.options.excludeListFileName));
        }
      }

      // purge expired entities (only on master)
      if (   (globalOptions.serverMode == SERVER_MODE_MASTER)
          && (jobNode->jobType == JOB_TYPE_CREATE)
         )
      {
//TODO: work-around: delete oldest entity if number of entities+1 > max. entities
// TODO: locking
//        (void)purgeExpiredEntities(indexHandle,jobUUID,archiveType);
      }

      // create/restore operation
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

              jobNode->runningInfo.progress.doneCount++;
              jobNode->runningInfo.progress.doneSize += 100;
//                jobNode->runningInfo.totalEntryCount += 3;
//                jobNode->runningInfo.totalEntrySize += 181;
              jobNode->runningInfo.estimatedRestTime=120-z;
              String_format(jobNode->runningInfo.fileName,"file %d",z);
              String_format(jobNode->runningInfo.storageName,"storage %d%d",z,z);
            }
          }
        #else
          switch (jobNode->jobType)
          {
            case JOB_TYPE_NONE:
              break;
            case JOB_TYPE_CREATE:
              // create new entity UUID
              Misc_getUUID(entityUUID);

              // create archive
              jobNode->runningInfo.error = Command_create(jobNode->masterIO,
                                                          String_cString(jobUUID),
                                                          String_cString(scheduleUUID),
//TODO:
                                                          NULL,  // scheduleTitle,
                                                          String_cString(entityUUID),
                                                          archiveType,
                                                          storageName,
                                                          &includeEntryList,
                                                          &excludePatternList,
                                                          String_cString(customText),
                                                          &jobOptions,
                                                          startDateTime,
                                                          CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                          CALLBACK_(createRunningInfo,jobNode),
                                                          CALLBACK_(storageVolumeRequest,jobNode),
                                                          CALLBACK_(isPauseCreate,NULL),
                                                          CALLBACK_(isPauseStorage,NULL),
//TODO access jobNode?
                                                          CALLBACK_(isAborted,jobNode),
                                                          &logHandle
                                                         );
              break;
            case JOB_TYPE_RESTORE:
              // restore archive
              StringList_init(&storageNameList);
              StringList_append(&storageNameList,storageName);
              jobNode->runningInfo.error = Command_restore(&storageNameList,
                                                           &includeEntryList,
                                                           &excludePatternList,
                                                           &jobOptions,
                                                           CALLBACK_(restoreRunningInfo,jobNode),
                                                           CALLBACK_(NULL,NULL),  // restoreErrorHandler
                                                           CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                           CALLBACK_INLINE(bool,(void *userData),{ UNUSED_VARIABLE(userData); return pauseFlags.restore; },NULL),
// TODO: use isCommandAborted9)
                                                           CALLBACK_INLINE(bool,(void *userData),{ UNUSED_VARIABLE(userData); return jobNode->requestedAbortFlag; },NULL),
                                                           &logHandle
                                                          );
              StringList_done(&storageNameList);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break;
            #endif /* NDEBUG */
          }
        #endif /* SIMULATOR */
      }
    }
    else
    {
      // slave job -> send to slave and run on slave machine

      // create new entity UUID
      Misc_getUUID(entityUUID);

      // check if connected
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if (connectorInfo == NULL)
        {
          jobNode->runningInfo.error = ERROR_SLAVE_DISCONNECTED;
        }
      }

      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        if      (!Connector_isConnected(connectorInfo))
        {
          jobNode->slaveState = SLAVE_STATE_OFFLINE;
          jobNode->runningInfo.error = ERROR_SLAVE_DISCONNECTED;
        }
        else if (!Connector_isAuthorized(connectorInfo))
        {
          jobNode->slaveState = SLAVE_STATE_OFFLINE;
          jobNode->runningInfo.error = ERROR_NOT_PAIRED;
        }
      }

      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        // init storage
        jobNode->runningInfo.error = Connector_initStorage(connectorInfo,
                                                           jobNode->job.storageName,
                                                           &jobNode->job.options
                                                          );
        if (jobNode->runningInfo.error == ERROR_NONE)
        {
          // run create job
          jobNode->runningInfo.error = Connector_create(connectorInfo,
                                                        jobName,
                                                        jobUUID,
                                                        scheduleUUID,
                                                        storageName,
                                                        &includeEntryList,
                                                        &excludePatternList,
                                                        &jobOptions,
                                                        archiveType,
                                                        NULL,  // scheduleTitle,
                                                        NULL,  // scheduleCustomText,
                                                        CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                        CALLBACK_(createRunningInfo,jobNode),
                                                        CALLBACK_(storageVolumeRequest,jobNode)
                                                       );

          // done storage
          Connector_doneStorage(connectorInfo);
        }
      }
    }

    // update next schedule
    nextScheduleDateTime = getNextSchedule(jobNode,
                                           scheduleUUID,
                                           executeStartDateTime,
                                           nextJobName,
                                           nextJobUUID,
                                           nextScheduleUUID
                                          );

    // job post-process command
    if (!String_isEmpty(jobNode->job.options.postProcessScript))
    {
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%name",                jobName,                                                               NULL);
        TEXT_MACRO_X_STRING ("%archive",             storageName,                                                           NULL);
        TEXT_MACRO_X_CSTRING("%type",                Archive_archiveTypeToString(archiveType),                              NULL);
        TEXT_MACRO_X_CSTRING("%T",                   Archive_archiveTypeToShortString(archiveType),                         NULL);
        TEXT_MACRO_X_STRING ("%directory",           File_getDirectoryName(directory,storageSpecifier.archiveName),         NULL);
        TEXT_MACRO_X_STRING ("%file",                storageSpecifier.archiveName,                                          NULL);
        TEXT_MACRO_X_CSTRING("%state",               Job_getStateText(jobNode->jobState,jobNode->noStorage,jobNode->dryRun),NULL);
        TEXT_MACRO_X_UINT   ("%error",               Error_getCode(jobNode->runningInfo.error),                             NULL);
        TEXT_MACRO_X_CSTRING("%message",             Error_getText(jobNode->runningInfo.error),                             NULL);
        if (nextScheduleDateTime < MAX_UINT64)
        {
          TEXT_MACRO_X_STRING ("%nextJobName",         nextJobName,                                                         NULL);
          TEXT_MACRO_X_STRING ("%nextJobUUID",         nextJobUUID,                                                         NULL);
          TEXT_MACRO_X_STRING ("%nextScheduleUUID",    nextScheduleUUID,                                                    NULL);
          TEXT_MACRO_X_UINT64 ("%nextSchedule",        (nextScheduleDateTime >= executeStartDateTime)
                                                         ? nextScheduleDateTime-executeStartDateTime
                                                         : 0,                                                               NULL);
          TEXT_MACRO_X_UINT64 ("%nextScheduleDateTime",nextScheduleDateTime,                                                NULL);
        }
        else
        {
          TEXT_MACRO_X_CSTRING("%nextJobName",         "",                                                                  NULL);
          TEXT_MACRO_X_CSTRING("%nextJobUUID",         "",                                                                  NULL);
          TEXT_MACRO_X_CSTRING("%nextScheduleUUID",    "",                                                                  NULL);
          TEXT_MACRO_X_CSTRING("%nextSchedule",        "",                                                                  NULL);
          TEXT_MACRO_X_CSTRING("%nextScheduleDateTime","",                                                                  NULL);
        }
      }
      error = executeTemplate(String_cString(jobNode->job.options.postProcessScript),
                              executeStartDateTime,
                              textMacros.data,
                              textMacros.count,
                              CALLBACK_(executeIOOutput,NULL)
                             );
      if (error == ERROR_NONE)
      {
        logMessage(&logHandle,
                   LOG_TYPE_INFO,
                   "Executed post-command for '%s'",
                   String_cString(jobName)
                  );
      }
      else
      {
        if (jobNode->runningInfo.error == ERROR_NONE) jobNode->runningInfo.error = error;
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Aborted job '%s': post-command fail (error: %s)",
                   String_cString(jobName),
                   Error_getText(jobNode->runningInfo.error)
                  );
      }
    }

    // get end date/time
    executeEndDateTime = Misc_getCurrentDateTime();

    // store final compress ratio: 100%-totalSum/totalCompressedSum
    jobNode->runningInfo.progress.compressionRatio = (!jobOptions.dryRun && (jobNode->runningInfo.progress.total.size > 0))
                                             ? 100.0-(jobNode->runningInfo.progress.storage.totalSize*100.0)/jobNode->runningInfo.progress.total.size
                                             : 0.0;

    // store last error code/message, last executed date/time
    jobNode->runningInfo.lastErrorCode        = Error_getCode(jobNode->runningInfo.error);
    jobNode->runningInfo.lastErrorNumber      = Error_getErrno(jobNode->runningInfo.error);
    String_setCString(jobNode->runningInfo.lastErrorData,Error_getData(jobNode->runningInfo.error));
    jobNode->runningInfo.lastExecutedDateTime = executeEndDateTime;

    // log job result
    switch (jobNode->jobType)
    {
      case JOB_TYPE_NONE:
        break;
      case JOB_TYPE_CREATE:
        {
          if      (jobNode->requestedAbortFlag)
          {
            // aborted
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Aborted job '%s'%s%s",
                       String_cString(jobName),
                       !String_isEmpty(jobNode->abortedByInfo) ? " by " : "",
                       String_cString(jobNode->abortedByInfo)
                      );
          }
          else if (jobNode->runningInfo.error != ERROR_NONE)
          {
            // error
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Done job '%s' (error: %s)",
                       String_cString(jobName),
                       Error_getText(jobNode->runningInfo.error)
                      );
          }
          else
          {
            // success
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Done job '%s' (duration: %"PRIu64"h:%02umin:%02us)",
                       String_cString(jobName),
                       (executeEndDateTime-executeStartDateTime) / (60LL*60LL),
                       (uint)((executeEndDateTime-executeStartDateTime) / 60LL) % 60LL,
                       (uint)((executeEndDateTime-executeStartDateTime) % 60LL)
                      );
          }

// TODO: nextScheduleDateTime could be 0
          if (nextScheduleDateTime < MAX_UINT64)
          {
            char buffer[64];
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Next schedule job '%s' at %s",
                       !String_isEmpty(nextJobName) ? String_cString(nextJobName) : String_cString(nextJobUUID),
                       Misc_formatDateTimeCString(buffer,sizeof(buffer),nextScheduleDateTime,TIME_TYPE_LOCAL,NULL)
                      );
          }
        }
        break;
      case JOB_TYPE_RESTORE:
        if (jobNode->runningInfo.error != ERROR_NONE)
        {
          logMessage(&logHandle,
                     LOG_TYPE_ALWAYS,
                     "Done restore archive (error: %s)",
                     Error_getText(jobNode->runningInfo.error)
                    );
        }
        else
        {
          logMessage(&logHandle,
                     LOG_TYPE_ALWAYS,
                     "Done restore archive"
                    );
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    // add index history information
    switch (jobNode->jobType)
    {
      case JOB_TYPE_NONE:
        break;
      case JOB_TYPE_CREATE:
        if (jobNode->requestedAbortFlag)
        {
          if (isIndexOpened)
          {
            error = Index_newHistory(&indexHandle,
                                     jobUUID,
                                     entityUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     "aborted",
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->runningInfo.progress.total.count,
                                     jobNode->runningInfo.progress.total.size,
                                     jobNode->runningInfo.progress.skipped.count,
                                     jobNode->runningInfo.progress.skipped.size,
                                     jobNode->runningInfo.progress.error.count,
                                     jobNode->runningInfo.progress.error.size,
                                     NULL  // historyId
                                    );
            if (error != ERROR_NONE)
            {
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Cannot insert history information for '%s' (error: %s)",
                         String_cString(jobName),
                         Error_getText(error)
                        );
            }
          }
        }
        else if (jobNode->runningInfo.error != ERROR_NONE)
        {
          if (isIndexOpened)
          {
            error = Index_newHistory(&indexHandle,
                                     jobUUID,
                                     entityUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     Error_getText(jobNode->runningInfo.error),
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->runningInfo.progress.total.count,
                                     jobNode->runningInfo.progress.total.size,
                                     jobNode->runningInfo.progress.skipped.count,
                                     jobNode->runningInfo.progress.skipped.size,
                                     jobNode->runningInfo.progress.error.count,
                                     jobNode->runningInfo.progress.error.size,
                                     NULL  // historyId
                                    );
            if (error != ERROR_NONE)
            {
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Cannot insert history information for '%s' (error: %s)",
                         String_cString(jobName),
                         Error_getText(error)
                        );
            }
          }
        }
        else
        {
          if (isIndexOpened)
          {
            error = Index_newHistory(&indexHandle,
                                     jobUUID,
                                     entityUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     NULL,  // errorMessage
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->runningInfo.progress.total.count,
                                     jobNode->runningInfo.progress.total.size,
                                     jobNode->runningInfo.progress.skipped.count,
                                     jobNode->runningInfo.progress.skipped.size,
                                     jobNode->runningInfo.progress.error.count,
                                     jobNode->runningInfo.progress.error.size,
                                     NULL  // historyId
                                    );
            if (error != ERROR_NONE)
            {
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Warning: cannot insert history information for '%s' (error: %s)",
                         String_cString(jobName),
                         Error_getText(error)
                        );
            }
          }
        }
        break;
      case JOB_TYPE_RESTORE:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }

    // close index
    if (isIndexOpened)
    {
      Index_close(&indexHandle);
    }

    // done log
    if      (!Job_isRemote(jobNode))
    {
      logPostProcess(&logHandle,
                     &jobNode->job.options,
                     archiveType,
                     customText,
                     jobName,
                     jobNode->jobState,
                     jobNode->noStorage,
                     jobNode->dryRun,
                     jobNode->runningInfo.message.text
                    );
    }
    doneLog(&logHandle);

    // get statistics data
    getAggregateInfo(&jobAggregateInfo,
                     jobNode->job.uuid,
                     NULL  // scheduleUUID
                    );
    getAggregateInfo(&scheduleAggregateInfo,
                     jobNode->job.uuid,
                     entityUUID
                    );

    // done job
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // end job
      Job_end(jobNode);

      // update statistics data
      jobNode->executionCount.normal        = jobAggregateInfo.executionCount.normal;
      jobNode->executionCount.full          = jobAggregateInfo.executionCount.full;
      jobNode->executionCount.incremental   = jobAggregateInfo.executionCount.incremental;
      jobNode->executionCount.differential  = jobAggregateInfo.executionCount.differential;
      jobNode->executionCount.continuous    = jobAggregateInfo.executionCount.continuous;
      jobNode->averageDuration.normal       = jobAggregateInfo.averageDuration.normal;
      jobNode->averageDuration.full         = jobAggregateInfo.averageDuration.full;
      jobNode->averageDuration.incremental  = jobAggregateInfo.averageDuration.incremental;
      jobNode->averageDuration.differential = jobAggregateInfo.averageDuration.differential;
      jobNode->averageDuration.continuous   = jobAggregateInfo.averageDuration.continuous;
      jobNode->totalEntityCount             = jobAggregateInfo.totalEntityCount;
      jobNode->totalStorageCount            = jobAggregateInfo.totalStorageCount;
      jobNode->totalStorageSize             = jobAggregateInfo.totalStorageSize;
      jobNode->totalEntryCount              = jobAggregateInfo.totalEntryCount;
      jobNode->totalEntrySize               = jobAggregateInfo.totalEntrySize;

      scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
      if (scheduleNode != NULL)
      {
        scheduleNode->active               = FALSE;
        scheduleNode->lastExecutedDateTime = executeEndDateTime;

        scheduleNode->totalEntityCount     = scheduleAggregateInfo.totalEntityCount;
        scheduleNode->totalStorageCount    = scheduleAggregateInfo.totalStorageCount;
        scheduleNode->totalStorageSize     = scheduleAggregateInfo.totalStorageSize;
        scheduleNode->totalEntryCount      = scheduleAggregateInfo.totalEntryCount;
        scheduleNode->totalEntrySize       = scheduleAggregateInfo.totalEntrySize;
      }

      if (!jobOptions.dryRun)
      {
        // store schedule info
        Job_writeScheduleInfo(jobNode,archiveType,executeEndDateTime);
      }

      // free resources
      if (connectorInfo != NULL)
      {
        Job_connectorUnlock(connectorInfo,LOCK_TIMEOUT);
      }
      Job_doneOptions(&jobOptions);
      PatternList_clear(&excludePatternList);
      EntryList_clear(&includeEntryList);
    }
  } // while (!quit)

  // free resources
  String_delete(nextJobName);
  StringMap_delete(resultMap);
  doneAggregateInfo(&scheduleAggregateInfo);
  doneAggregateInfo(&jobAggregateInfo);
  String_delete(byName);
  String_delete(customText);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  String_delete(directory);
  String_delete(storageName);
  String_delete(jobName);
  Storage_doneSpecifier(&storageSpecifier);
}

/*---------------------------------------------------------------------*/

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
  uint *abortedCommandId;

  assert(clientInfo != NULL);

  abortedFlag = FALSE;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    if (!abortedFlag)
    {
      // check for quit
      if (clientInfo->quitFlag) abortedFlag = TRUE;
    }

    if (!abortedFlag)
    {
      if (commandId >= clientInfo->abortedCommandIdStart)
      {
        // check if command aborted
        RINGBUFFER_ITERATEX(&clientInfo->abortedCommandIds,abortedCommandId,!abortedFlag)
        {
          abortedFlag = (commandId == (*abortedCommandId));
        }
      }
    }
  }

  return abortedFlag;
}

/*---------------------------------------------------------------------*/

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
  while (   !isQuit()
         && (   !StringList_isEmpty(&directoryInfoNode->pathNameList)
             || directoryInfoNode->directoryOpenFlag
            )
         && ((timedOut == NULL) || !(*timedOut))
        )
  {
    if (!directoryInfoNode->directoryOpenFlag)
    {
      // process FIFO for deep-first search; this keep the directory list shorter
      StringList_removeLast(&directoryInfoNode->pathNameList,pathName);

      // open directory for reading
      error = File_openDirectoryList(&directoryInfoNode->directoryListHandle,pathName);
      if (error != ERROR_NONE)
      {
        continue;
      }
      directoryInfoNode->directoryOpenFlag = TRUE;
    }

    // read directory content
    while (   !isQuit()
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
      error = File_getInfo(&fileInfo,fileName);
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
        (*timedOut) = (Misc_getTimestamp() >= (startTimestamp+timeout*US_PER_MS));
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
      (*timedOut) = (Misc_getTimestamp() >= (startTimestamp+timeout*US_PER_MS));
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
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  if (!StringMap_getUInt64(argumentMap,"error",&n,(uint64)ERROR_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"error=<n>");
    return;
  }
  error = (Errors)n;

  // format result
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                      "errorMessage=%'s",
                      Error_getText(error)
                     );
}

/***********************************************************************\
* Name   : serverCommand_startTLS
* Purpose: start TLS connection
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_startTLS(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  #ifdef HAVE_GNU_TLS
    Errors error;
    char   buffer[64];
  #endif /* HAVE_GNU_TLS */

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  #ifdef HAVE_GNU_TLS
    if (!Configuration_isCertificateAvailable(&globalOptions.serverCert))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NO_TLS_CERTIFICATE,"no server certificate data");
      return;
    }
    if (!Configuration_isKeyAvailable(&globalOptions.serverKey))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NO_TLS_KEY,"no server key data");
      return;
    }

    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

    SEMAPHORE_LOCKED_DO(&clientInfo->io.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      error = Network_startTLS(&clientInfo->io.network.socketHandle,
                               NETWORK_TLS_TYPE_SERVER,
                               globalOptions.serverCA.data,
                               globalOptions.serverCA.length,
                               globalOptions.serverCert.data,
                               globalOptions.serverCert.length,
                               globalOptions.serverKey.data,
                               globalOptions.serverKey.length,
                               30*MS_PER_SECOND
                              );
      if (error != ERROR_NONE)
      {
        Network_disconnect(&clientInfo->io.network.socketHandle);
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_ALWAYS,
                   "Start TLS failed - disconnected %s (error: %s)",
                   getClientInfoString(clientInfo,buffer,sizeof(buffer)),
                   Error_getText(error)
                  );
      }
    }

    // Note: on error connection is dead!
  #else /* not HAVE_GNU_TLS */
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_FUNCTION_NOT_SUPPORTED,"not available");
  #endif /* HAVE_GNU_TLS */

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_authorize
* Purpose: user authorization: check password
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          or
*            encryptType=<type>
*            name=<text>
*            encryptedUUID=<encrypted uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_authorize(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  ServerIOEncryptTypes encryptType;
  String               name;
  String               encryptedPassword;
  String               encryptedUUID;
  Errors               error;
  void                 *buffer;
  uint                 bufferLength;
  CryptHash            uuidHash;
  char                 s[256];
  AuthorizationStates  authorizationState;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password/UUID
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);
  encryptedPassword = String_new();
  encryptedUUID     = String_new();
  if (   !StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL)
      && !StringMap_getString(argumentMap,"encryptedUUID",encryptedUUID,NULL)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password> or encryptedUUID=<encrypted key>");
    String_delete(encryptedUUID);
    String_delete(encryptedPassword);
    String_delete(name);
    return;
  }
//fprintf(stderr,"%s, %d: encryptType=%d\n",__FILE__,__LINE__,encryptType);
//fprintf(stderr,"%s, %d: encryptedPassword='%s' %lu\n",__FILE__,__LINE__,String_cString(encryptedPassword),String_length(encryptedPassword));
//fprintf(stderr,"%s, %d: encryptedUUID='%s' %lu\n",__FILE__,__LINE__,String_cString(encryptedUUID),String_length(encryptedUUID));

  error              = ERROR_UNKNOWN;
  authorizationState = AUTHORIZATION_STATE_FAIL;
  if      (!String_isEmpty(encryptedPassword))
  {
    // client => verify password
    #ifndef NDEBUG
      if (globalOptions.debug.serverLevel == 0)
    #else
      if (TRUE)
    #endif
    {
      if (ServerIO_verifyPassword(&clientInfo->io,
                                  encryptedPassword,
                                  encryptType,
                                  &globalOptions.serverPasswordHash
                                 )
         )
      {
        error = ERROR_NONE;
      }
      else
      {
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_ALWAYS,
                   "Authorization of client %s failed - invalid password",
                   getClientInfoString(clientInfo,s,sizeof(s))
                  );
        error = ERROR_INVALID_PASSWORD_;
      }
    }
    else
    {
      // Note: server in debug mode -> no password check
      error = ERROR_NONE;
    }

    authorizationState = AUTHORIZATION_STATE_CLIENT;
  }
  else if (!String_isEmpty(encryptedUUID))
  {
    // master => verify/pair new master
    if (globalOptions.serverMode == SERVER_MODE_SLAVE)
    {
      // decrypt UUID
      error = ServerIO_decryptData(&clientInfo->io,
                                   &buffer,
                                   &bufferLength,
                                   encryptType,
                                   encryptedUUID
                                  );
      if (error == ERROR_NONE)
      {
        assert(buffer != NULL);
        assert(bufferLength > 0);

        switch (newMaster.pairingMode)
        {
          case PAIRING_MODE_NONE:
            // not pairing -> verify master UUID

            // calculate hash from UUID
            (void)Crypt_initHash(&uuidHash,PASSWORD_HASH_ALGORITHM);
            Crypt_updateHash(&uuidHash,Misc_getMachineId(),MISC_MACHINE_ID_LENGTH);
            Crypt_updateHash(&uuidHash,buffer,bufferLength);

            // verify master UUID (UUID hash)
            if (!Configuration_equalsHash(&globalOptions.masterInfo.uuidHash,&uuidHash))
            {
              error = ((globalOptions.serverMode == SERVER_MODE_SLAVE) && String_isEmpty(globalOptions.masterInfo.name))
                        ? ERROR_NOT_PAIRED
                        : ERROR_INVALID_PASSWORD_;
              logMessage(NULL,  // logHandle,
                         LOG_TYPE_ALWAYS,
                         "Authorization of master %s failed (error: %s)",
                         getClientInfoString(clientInfo,s,sizeof(s)),
                         Error_getText(error)
                        );
            }

            // free resources
            Crypt_doneHash(&uuidHash);
            break;
          case PAIRING_MODE_AUTO:
            // auto pairing -> done pairing
            SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              // store name
              String_set(newMaster.name,name);

              // calculate hash from UUID
              (void)Crypt_resetHash(&newMaster.uuidHash);
              Crypt_updateHash(&newMaster.uuidHash,Misc_getMachineId(),MISC_MACHINE_ID_LENGTH);
              Crypt_updateHash(&newMaster.uuidHash,buffer,bufferLength);
            }

            error = endPairingMaster(newMaster.name,&newMaster.uuidHash);
            break;
          case PAIRING_MODE_MANUAL:
            // manual pairing -> just store new master name+UUID hash
            SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              // store name
              String_set(newMaster.name,name);

              // calculate hash from UUID
              (void)Crypt_resetHash(&newMaster.uuidHash);
              Crypt_updateHash(&newMaster.uuidHash,Misc_getMachineId(),MISC_MACHINE_ID_LENGTH);
              Crypt_updateHash(&newMaster.uuidHash,buffer,bufferLength);
            }

            // still not paired: new master must be confirmed
            error = ERROR_NOT_PAIRED;
            break;
        }

        // free resources
        freeSecure(buffer);
      }
      else
      {
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_ALWAYS,
                   "Authorization of master %s failed (error: %s)",
                   getClientInfoString(clientInfo,s,sizeof(s)),
                   Error_getText(error)
                  );
      }
    }
    else
    {
      error = ERROR_NOT_A_SLAVE;

      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Authorization of master %s failed (error: %s)",
                 getClientInfoString(clientInfo,s,sizeof(s)),
                 Error_getText(error)
                );
    }

    authorizationState = AUTHORIZATION_STATE_MASTER;
  }

  // set authorization state
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    if (error == ERROR_NONE)
    {
      clientInfo->authorizationState = authorizationState;
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"authorization failure");
    }
  }

  // free resources
  String_delete(encryptedUUID);
  String_delete(encryptedPassword);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_version
* Purpose: get protocol version
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            major=<major version>
*            minor=<minor version>
*            mode=MASTER|SLAVE
\***********************************************************************/

LOCAL void serverCommand_version(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  const char *s;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  s = NULL;
  switch (globalOptions.serverMode)
  {
    case SERVER_MODE_MASTER: s = "MASTER"; break;
    case SERVER_MODE_SLAVE:  s = "SLAVE";  break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  ServerIO_sendResult(&clientInfo->io,
                      id,
                      TRUE,
                      ERROR_NONE,
                      "major=%u minor=%u mode=%s",
                      SERVER_PROTOCOL_VERSION_MAJOR,
                      SERVER_PROTOCOL_VERSION_MINOR,
                      s
                     );
}

/***********************************************************************\
* Name   : serverCommand_quit
* Purpose: quit server
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_quit(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
  #else
    if (FALSE)
  #endif
  {
    setQuit();
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_FUNCTION_NOT_SUPPORTED,"not in debug mode");
  }

  // abort all jobs
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      if (Job_isActive(jobNode->jobState))
      {
        Job_abort(jobNode,NULL);
      }
    }
  }
}

/***********************************************************************\
* Name   : serverCommand_actionResult
* Purpose: set action result
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            error=<n>
*            ...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_actionResult(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
Errors error;
//  uint           stringMapIterator;
  uint64         n;
//  const char     *name;
//  StringMapTypes type;
//  StringMapValue value;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);
//TODO
#ifndef WERROR
#warning TODO
#endif
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); StringMap_debugPrint(0,argumentMap);

//TODO
#ifndef WERROR
#warning TODO
#endif
#if 1
  SEMAPHORE_LOCKED_DO(&clientInfo->io.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // get error
    StringMap_getUInt64(argumentMap,"errorCode",&n,ERROR_CODE_UNKNOWN);
    error = Error_(n,0);

    // set action result
    ServerIO_clientActionResult(&clientInfo->io,id,error,argumentMap);
  }

#else
  SEMAPHORE_LOCKED_DO(&clientInfo->io.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // get error
    StringMap_getUInt64(argumentMap,"errorCode",&n,ERROR_UNKNOWN);
    clientInfo->io.action.error = Error_(n,0);

    // get arguments
    StringMap_clear(clientInfo->io.action.resultMap);
    STRINGMAP_ITERATE(argumentMap,stringMapIterator,name,type,value)
    {
      if (!stringEquals(name,"errorCode"))
      {
        StringMap_putValue(clientInfo->io.action.resultMap,name,type,&value);
      }
    }
  }
#endif

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_get
* Purpose: get setting from server
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  // send value
  if (String_equalsCString(name,"PATH_SEPARATOR"))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%c",FILE_PATH_SEPARATOR_CHAR);
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown server config '%S'",name);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionGet
* Purpose: get server option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  uint              i;
  String            value;
  ConfigValueFormat configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  // find config value
  i = ConfigValue_valueIndex(BAR_CONFIG_VALUES,NULL,String_cString(name));
  if (i == CONFIG_VALUE_INDEX_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"server config '%S'",name);
    String_delete(name);
    return;
  }
  assert(BAR_CONFIG_VALUES[i].type != CONFIG_VALUE_TYPE_DEPRECATED);

  // send value
  value = String_new();
  ConfigValue_formatInit(&configValueFormat,
                         &BAR_CONFIG_VALUES[i],
                         CONFIG_VALUE_FORMAT_MODE_VALUE,
                         &globalOptions
                        );
  ConfigValue_format(&configValueFormat,value);
  ConfigValue_formatDone(&configValueFormat);
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%S",value);
  String_delete(value);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionSet
* Purpose: set server option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  uint   i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, value
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"value=<value>");
    String_delete(value);
    String_delete(name);
    return;
  }

  // parse
  i = ConfigValue_find(BAR_CONFIG_VALUES,
                       CONFIG_VALUE_INDEX_NONE,
                       CONFIG_VALUE_INDEX_NONE,
                       String_cString(name)
                      );
  if (i == CONFIG_VALUE_INDEX_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown server config '%S'",name);
    String_delete(value);
    String_delete(name);
    return;
  }

  if (ConfigValue_parse(BAR_CONFIG_VALUES,
                        &BAR_CONFIG_VALUES[i],
                        NULL, // sectionName
                        String_cString(value),
                        CALLBACK_(NULL,NULL),  // errorFunction
                        CALLBACK_(NULL,NULL),  // warningFunction
                        NULL,
                        NULL // commentLineList  //variable
                       )
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_VALUE,"invalid server config '%S'",name);
    String_delete(value);
    String_delete(name);
    return;
  }

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionFlush
* Purpose: flush server options to config file
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

  error = Configuration_update();
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_masterGet
* Purpose: get master name
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            name=<name>
\***********************************************************************/

LOCAL void serverCommand_masterGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"name=%'S",globalOptions.masterInfo.name);
}

/***********************************************************************\
* Name   : serverCommand_masterClear
* Purpose: clear master
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_masterClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  error = clearPairedMaster();
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_masterPairingStart
* Purpose: start pairing new master
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            timeout=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_masterPairingStart(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint timeout;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get timeout
  StringMap_getUInt(argumentMap,"timeout",&timeout,DEFAULT_PAIRING_MASTER_TIMEOUT);

  // start pairing
  beginPairingMaster(timeout,PAIRING_MODE_MANUAL);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_masterPairingStop
* Purpose: stop pairing new master
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            pair=yes|no
*          Result:
*            name=<name>
\***********************************************************************/

LOCAL void serverCommand_masterPairingStop(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  bool   pairFlag;
  Errors error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get pair flag
  if (!StringMap_getBool(argumentMap,"pair",&pairFlag,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pair=yes|no");
    return;
  }

  if (pairFlag)
  {
    error = endPairingMaster(newMaster.name,&newMaster.uuidHash);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }
  else
  {
    abortPairingMaster();
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"name=%'S",globalOptions.masterInfo.name);
}

/***********************************************************************\
* Name   : serverCommand_masterPairingStatus
* Purpose: get pairing status new master
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            name=<name>
*            restTime=<n> [s]
*            totalTime=<n> [s]
\***********************************************************************/

LOCAL void serverCommand_masterPairingStatus(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&clientInfo->io,
                      id,
                      TRUE,
                      ERROR_NONE,
                      "name=%'S restTime=%lu totalTime=%lu",
                      newMaster.name,
                      Misc_getRestTimeout(&newMaster.pairingTimeoutInfo,MAX_ULONG)/MS_PER_S,
                      Misc_getTotalTimeout(&newMaster.pairingTimeoutInfo)/MS_PER_S
                     );
}

/***********************************************************************\
* Name   : serverCommand_maintenanceList
* Purpose: get maintenance list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            id=<n> \
*            date=<year>|*-<month>|*-<day>|* \
*            weekDays=<week day>|* \
*            beginTime=<hour>|*:<minute>|* \
*            endTime=<hour>|*:<minute>|*
*            ...
\***********************************************************************/

LOCAL void serverCommand_maintenanceList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          date,weekDays,beginTime,endTime;
  MaintenanceNode *maintenanceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // init variables
  date      = String_new();
  weekDays  = String_new();
  beginTime = String_new();
  endTime   = String_new();

  SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&globalOptions.maintenanceList,maintenanceNode)
    {
      // get date string
      String_clear(date);
      if (maintenanceNode->date.year != DATE_ANY)
      {
        String_appendFormat(date,"%d",maintenanceNode->date.year);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (maintenanceNode->date.month != DATE_ANY)
      {
        String_appendFormat(date,"%02d",maintenanceNode->date.month);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (maintenanceNode->date.day != DATE_ANY)
      {
        String_appendFormat(date,"%02d",maintenanceNode->date.day);
      }
      else
      {
        String_appendCString(date,"*");
      }

      // get weekdays string
      String_clear(weekDays);
      if (maintenanceNode->weekDaySet != WEEKDAY_SET_ANY)
      {
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_MON)) { String_joinCString(weekDays,"Mon",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_TUE)) { String_joinCString(weekDays,"Tue",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_WED)) { String_joinCString(weekDays,"Wed",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_THU)) { String_joinCString(weekDays,"Thu",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_FRI)) { String_joinCString(weekDays,"Fri",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_SAT)) { String_joinCString(weekDays,"Sat",','); }
        if (IN_SET(maintenanceNode->weekDaySet,WEEKDAY_SUN)) { String_joinCString(weekDays,"Sun",','); }
      }
      else
      {
        String_appendCString(weekDays,"*");
      }

      // get begin/end time string
      String_clear(beginTime);
      if (maintenanceNode->beginTime.hour != TIME_ANY)
      {
        String_appendFormat(beginTime,"%02d",maintenanceNode->beginTime.hour);
      }
      else
      {
        String_appendCString(beginTime,"*");
      }
      String_appendChar(beginTime,':');
      if (maintenanceNode->beginTime.minute != TIME_ANY)
      {
        String_appendFormat(beginTime,"%02d",maintenanceNode->beginTime.minute);
      }
      else
      {
        String_appendCString(beginTime,"*");
      }

      String_clear(endTime);
      if (maintenanceNode->endTime.hour != TIME_ANY)
      {
        String_appendFormat(endTime,"%02d",maintenanceNode->endTime.hour);
      }
      else
      {
        String_appendCString(endTime,"*");
      }
      String_appendChar(endTime,':');
      if (maintenanceNode->endTime.minute != TIME_ANY)
      {
        String_appendFormat(endTime,"%02d",maintenanceNode->endTime.minute);
      }
      else
      {
        String_appendCString(endTime,"*");
      }

      // send schedule info
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "id=%u date=%S weekDays=%S beginTime=%S endTime=%S",
                          maintenanceNode->id,
                          date,
                          weekDays,
                          beginTime,
                          endTime
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(endTime);
  String_delete(beginTime);
  String_delete(weekDays);
  String_delete(date);
}

/***********************************************************************\
* Name   : serverCommand_maintenanceListAdd
* Purpose: add maintenance to list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            date=<year>|*-<month>|*-<day>|* \
*            weekDays=<week day>|* \
*            beginTime=<hour>|*:<minute>|* \
*            endTime=<hour>|*:<minute>|*
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_maintenanceListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          date;
  String          weekDays;
  String          beginTime,endTime;
  MaintenanceNode *maintenanceNode;
  Errors          error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get date, weekday, begin time, end time
  date = String_new();
  if (!StringMap_getString(argumentMap,"date",date,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"date=<date>|*");
    String_delete(date);
    return;
  }
  weekDays = String_new();
  if (!StringMap_getString(argumentMap,"weekDays",weekDays,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"weekDays=<names>|*");
    String_delete(weekDays);
    String_delete(date);
    return;
  }
  beginTime = String_new();
  if (!StringMap_getString(argumentMap,"beginTime",beginTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"beginTime=<time>|*");
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }
  endTime = String_new();
  if (!StringMap_getString(argumentMap,"endTime",endTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"endTime=<time>|*");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }

  // create new mainteance
  maintenanceNode = Configuration_newMaintenanceNode();
  assert(maintenanceNode != NULL);

  // parse maintenance
  error = parseMaintenanceDateTime(maintenanceNode,
                                   date,
                                   weekDays,
                                   beginTime,
                                   endTime
                                  );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,
                        id,
                        TRUE,
                        ERROR_PARSE_MAINTENANCE,
                        "%S %S %S %S",
                        date,
                        weekDays,
                        beginTime,
                        endTime
                       );
    Configuration_deleteMaintenanceNode(maintenanceNode);
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }

  SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    List_append(&globalOptions.maintenanceList,maintenanceNode);
  }

  // update config file
  error = Configuration_update();
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",maintenanceNode->id);

  // free resources
  String_delete(endTime);
  String_delete(beginTime);
  String_delete(weekDays);
  String_delete(date);
}

/***********************************************************************\
* Name   : serverCommand_maintenanceListUpdate
* Purpose: update maintenance in list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*            date=<year>|*-<month>|*-<day>|* \
*            weekDays=<week day>|* \
*            beginTime=<hour>|*:<minute>|* \
*            endTime=<hour>|*:<minute>|*
*          Result:
\***********************************************************************/

LOCAL void serverCommand_maintenanceListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            maintenanceId;
  String          date;
  String          weekDays;
  String          beginTime,endTime;
  MaintenanceNode *maintenanceNode;
  Errors          error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get maintenance id, date, weekday, begin time, end time
  if (!StringMap_getUInt(argumentMap,"id",&maintenanceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  date = String_new();
  if (!StringMap_getString(argumentMap,"date",date,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"date=<date>|*");
    String_delete(date);
    return;
  }
  weekDays = String_new();
  if (!StringMap_getString(argumentMap,"weekDays",weekDays,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"weekDays=<names>|*");
    String_delete(weekDays);
    String_delete(date);
    return;
  }
  beginTime = String_new();
  if (!StringMap_getString(argumentMap,"beginTime",beginTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"beginTime=<time>|*");
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }
  endTime = String_new();
  if (!StringMap_getString(argumentMap,"endTime",endTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"endTime=<time>|*");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }

  SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find maintenance
    maintenanceNode = LIST_FIND(&globalOptions.maintenanceList,maintenanceNode,maintenanceNode->id == maintenanceId);
    if (maintenanceNode == NULL)
    {
      Semaphore_unlock(&globalOptions.maintenanceList.lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_MAINTENANCE_ID_NOT_FOUND,"%u",maintenanceId);
      String_delete(endTime);
      String_delete(beginTime);
      String_delete(weekDays);
      String_delete(date);
      return;
    }

    // parse maintenance
    error = parseMaintenanceDateTime(maintenanceNode,
                                     date,
                                     weekDays,
                                     beginTime,
                                     endTime
                                    );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,
                          id,
                          TRUE,
                          ERROR_PARSE_MAINTENANCE,
                          "%S %S %S %S",
                          date,
                          weekDays,
                          beginTime,
                          endTime
                         );
      String_delete(endTime);
      String_delete(beginTime);
      String_delete(weekDays);
      String_delete(date);
      return;
    }
  }

  // update config file
  error = Configuration_update();
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(weekDays);
    String_delete(date);
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(endTime);
  String_delete(beginTime);
  String_delete(weekDays);
  String_delete(date);
}

/***********************************************************************\
* Name   : serverCommand_maintenanceListRemove
* Purpose: delete maintenance from list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_maintenanceListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            maintenanceId;
  MaintenanceNode *maintenanceNode;
  Errors          error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get maintenance id
  if (!StringMap_getUInt(argumentMap,"id",&maintenanceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(&globalOptions.maintenanceList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find maintenance
    maintenanceNode = LIST_FIND(&globalOptions.maintenanceList,maintenanceNode,maintenanceNode->id == maintenanceId);
    if (maintenanceNode == NULL)
    {
      Semaphore_unlock(&globalOptions.maintenanceList.lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_MAINTENANCE_ID_NOT_FOUND,"%u",maintenanceId);
      return;
    }

    // delete mainteance
    List_remove(&globalOptions.maintenanceList,maintenanceNode);
    Configuration_deleteMaintenanceNode(maintenanceNode);
  }

  // update config file
  error = Configuration_update();
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_serverList
* Purpose: get server list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  ServerNode *serverNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      switch (serverNode->server.type)
      {
        case SERVER_TYPE_NONE:
          break;
        case SERVER_TYPE_FILE:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "FILE",
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_FTP:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "FTP",
                              serverNode->server.ftp.userName,
                              serverNode->server.maxConnectionCount,
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_SSH:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s port=%d loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "SSH",
                              serverNode->server.ssh.port,
                              serverNode->server.ssh.userName,
                              serverNode->server.maxConnectionCount,
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_WEBDAV:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s serverPort=%u loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "WEBDAV",
                              serverNode->server.webDAV.port,
                              serverNode->server.webDAV.userName,
                              serverNode->server.maxConnectionCount,
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_WEBDAVS:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s serverPort=%u loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "WEBDAVS",
                              serverNode->server.webDAV.port,
                              serverNode->server.webDAV.userName,
                              serverNode->server.maxConnectionCount,
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_SMB:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s loginName=%'S share=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->server.id,
                              serverNode->server.name,
                              "SMB",
                              serverNode->server.smb.userName,
                              serverNode->server.smb.shareName,
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

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_serverListAdd
* Purpose: add entry to server list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            serverType=file|ftp|ssh|webdav
*            path=<path|URL>
*            port=<n>
*            loginName=<name>
*            [password=<password>]
*            [publicKey=<data>]
*            [privateKey=<data>]
*            [maxConnectionCount=<n>]
*            [maxStorageSize=<size>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_serverListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String      name;
  ServerTypes serverType;
  uint        port;
  String      userName;
  String      password;
// TODO:
  String      share;
  String      publicKey;
  String      privateKey;
  uint        maxConnectionCount;
  uint64      maxStorageSize;
  ServerNode  *serverNode;
  Errors      error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
// TODO: use share
UNUSED_VARIABLE(share);

  // get name, server type, login name, port, password, public/private key, max. connections, max. storage size
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,CALLBACK_((StringMapParseEnumFunction)parseServerType,NULL),SERVER_TYPE_FILE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"serverType=<FILE|FTP|SSH|WEBDAV>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"port",&port,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(name);
    return;
  }
  userName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",userName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(userName);
    String_delete(name);
    return;
  }
  password = String_new();
  StringMap_getString(argumentMap,"password",password,NULL);
  publicKey = String_new();
  StringMap_getString(argumentMap,"publicKey",publicKey,NULL);
  privateKey = String_new();
  StringMap_getString(argumentMap,"privateKey",privateKey,NULL);
  StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,MAX_UINT);
  StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,MAX_UINT64);

  // allocate storage server node
  serverNode = Configuration_newServerNode(name,serverType);
  assert(serverNode != NULL);

  // init storage server settings
  switch (serverType)
  {
    case SERVER_TYPE_NONE:
      break;
    case SERVER_TYPE_FILE:
      break;
    case SERVER_TYPE_FTP:
      String_set(serverNode->server.ftp.userName,userName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.ftp.password,password);
      }
      break;
    case SERVER_TYPE_SSH:
      serverNode->server.ssh.port = port;
      String_set(serverNode->server.ssh.userName,userName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.ssh.password,password);
      }
      Configuration_setKeyString(&serverNode->server.ssh.publicKey,NULL,publicKey);
      Configuration_setKeyString(&serverNode->server.ssh.privateKey,NULL,privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
    case SERVER_TYPE_WEBDAVS:
      serverNode->server.webDAV.port = port;
      String_set(serverNode->server.webDAV.userName,userName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.webDAV.password,password);
      }
      Configuration_setKeyString(&serverNode->server.webDAV.publicKey,NULL,publicKey);
      Configuration_setKeyString(&serverNode->server.webDAV.privateKey,NULL,privateKey);
      break;
    case SERVER_TYPE_SMB:
      String_set(serverNode->server.smb.userName,userName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.smb.password,password);
      }
      break;
  }
  serverNode->server.maxConnectionCount = maxConnectionCount;
  serverNode->server.maxStorageSize     = maxStorageSize;

  // add to server list
  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    List_append(&globalOptions.serverList,serverNode);
  }

  // update config file
  error = Configuration_update();
  if (error != ERROR_NONE)
  {
    Semaphore_unlock(&globalOptions.serverList.lock);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(userName);
    String_delete(name);
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",serverNode->server.id);

  // free resources
  String_delete(privateKey);
  String_delete(publicKey);
  String_delete(password);
  String_delete(userName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverListUpdate
* Purpose: update entry in server list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*            name=<name>
*            serverType=filesystem|ftp|ssh|webdav
*            port=<n>
*            loginName=<name>
*            [password=<password>]
*            [publicKey=<data>]
*            [privateKey=<data>]
*            [maxConnectionCount=<n>]
*            [maxStorageSize=<size>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint        serverId;
  String      name;
  ServerTypes serverType;
  uint        port;
  String      userName;
  String      password;
  String      publicKey;
  String      privateKey;
  uint        maxConnectionCount;
  uint64      maxStorageSize;
  ServerNode  *serverNode;
  Errors      error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get id, name, server type, login name, port, password, public/private key, max. connections, max. storage size
  if (!StringMap_getUInt(argumentMap,"id",&serverId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,CALLBACK_((StringMapParseEnumFunction)parseServerType,NULL),SERVER_TYPE_FILE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"serverType=<FILE|FTP|SSH|WEBDAV>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"port",&port,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(name);
    return;
  }
  userName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",userName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(userName);
    String_delete(name);
    return;
  }
  password = String_new();
  StringMap_getString(argumentMap,"password",password,NULL);
  publicKey = String_new();
  StringMap_getString(argumentMap,"publicKey",publicKey,NULL);
  privateKey = String_new();
  StringMap_getString(argumentMap,"privateKey",privateKey,NULL);
  StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,MAX_UINT);
  StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,MAX_UINT64);

  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find storage server
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->server.id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,serverId,TRUE,ERROR_SERVER_ID_NOT_FOUND,"%u",serverId);
      String_delete(privateKey);
      String_delete(publicKey);
      String_delete(password);
      String_delete(userName);
      String_delete(name);
      return;
    }

    // update storage server settings
    Configuration_setServerNode(serverNode,name,serverType);
    switch (serverType)
    {
      case SERVER_TYPE_NONE:
        break;
      case SERVER_TYPE_FILE:
        break;
      case SERVER_TYPE_FTP:
        String_set(serverNode->server.ftp.userName,userName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.ftp.password,password);
        }
        break;
      case SERVER_TYPE_SSH:
        serverNode->server.ssh.port = port;
        String_set(serverNode->server.ssh.userName,userName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.ssh.password,password);
        }
        Configuration_setKeyString(&serverNode->server.ssh.publicKey,NULL,publicKey);
        Configuration_setKeyString(&serverNode->server.ssh.privateKey,NULL,privateKey);
        break;
      case SERVER_TYPE_WEBDAV:
      case SERVER_TYPE_WEBDAVS:
        serverNode->server.webDAV.port = port;
        String_set(serverNode->server.webDAV.userName,userName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.webDAV.password,password);
        }
        Configuration_setKeyString(&serverNode->server.webDAV.publicKey,NULL,publicKey);
        Configuration_setKeyString(&serverNode->server.webDAV.privateKey,NULL,privateKey);
        break;
      case SERVER_TYPE_SMB:
        String_set(serverNode->server.smb.userName,userName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.smb.password,password);
        }
// TODO:share
        break;
    }
    serverNode->server.maxConnectionCount = maxConnectionCount;
    serverNode->server.maxStorageSize     = maxStorageSize;

    // update config file
    error = Configuration_update();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
      String_delete(privateKey);
      String_delete(publicKey);
      String_delete(password);
      String_delete(userName);
      String_delete(name);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(privateKey);
  String_delete(publicKey);
  String_delete(password);
  String_delete(userName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverListRemove
* Purpose: delete entry from server list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint       serverId;
  ServerNode *serverNode;
  Errors     error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get storage server id
  if (!StringMap_getUInt(argumentMap,"id",&serverId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find storage server
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->server.id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,serverId,TRUE,ERROR_SERVER_ID_NOT_FOUND,"%u",serverId);
      return;
    }

    // delete storage server
    List_remove(&globalOptions.serverList,serverNode);
    Configuration_deleteServerNode(serverNode);

    // update config file
    error = Configuration_update();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_abort
* Purpose: abort command execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            commandId=<command id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_abort(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            commandId;
  CommandInfoNode *commandInfoNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get command id
  if (!StringMap_getUInt(argumentMap,"commandId",&commandId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"commandId=<command id>");
    return;
  }

  // abort command
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    commandInfoNode = LIST_FIND(&clientInfo->commandInfoList,commandInfoNode,commandInfoNode->id == commandId);
    if ((commandInfoNode != NULL) && (commandInfoNode->indexHandle != NULL))
    {
      Index_interrupt(commandInfoNode->indexHandle);
    }

    // store command id
    if (RingBuffer_isFull(&clientInfo->abortedCommandIds))
    {
      // discard first entry
      RingBuffer_discard(&clientInfo->abortedCommandIds,1,CALLBACK_(NULL,NULL));

      // get new start command id
      RingBuffer_first(&clientInfo->abortedCommandIds,&clientInfo->abortedCommandIdStart);
    }
    RingBuffer_put(&clientInfo->abortedCommandIds,&commandId,1);
  }

  // format result
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_status
* Purpose: get status
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            state=RUNNING|PAUSED|SUSPENDED
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
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"state=RUNNING");
        break;
      case SERVER_STATE_PAUSE:
        now = Misc_getCurrentDateTime();
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"state=PAUSED time=%"PRIu64,(pauseEndDateTime > now) ? pauseEndDateTime-now : 0LL);
        break;
      case SERVER_STATE_SUSPENDED:
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"state=SUSPENDED");
        break;
    }
  }
  Semaphore_unlock(&serverStateLock);
}

/***********************************************************************\
* Name   : serverCommand_pause
* Purpose: pause job execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            time=<pause time [s]>
*            [modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE,INDEX_MAINTENANCE]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_pause(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            pauseTime;
  String          modeMask;
  StringTokenizer stringTokenizer;
  ConstString     token;
  SlaveNode       *slaveNode;
  Errors          error;
  char            s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get pause time, mode mask
  if (!StringMap_getUInt(argumentMap,"time",&pauseTime,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"time=<time>");
    return;
  }
  modeMask = String_new();
  StringMap_getString(argumentMap,"modeMask",modeMask,NULL);

  // set pause time
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // set pause flags
    if (String_isEmpty(modeMask))
    {
      pauseFlags.create           = TRUE;
      pauseFlags.storage          = TRUE;
      pauseFlags.restore          = TRUE;
      pauseFlags.indexUpdate      = TRUE;
      pauseFlags.indexMaintenance = TRUE;
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
          pauseFlags.storage          = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"RESTORE"))
        {
          pauseFlags.restore          = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_UPDATE"))
        {
          pauseFlags.indexUpdate      = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_MAINTENANCE"))
        {
          pauseFlags.indexMaintenance = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"ALL"))
        {
          pauseFlags.create           = TRUE;
          pauseFlags.storage          = TRUE;
          pauseFlags.restore          = TRUE;
          pauseFlags.indexUpdate      = TRUE;
          pauseFlags.indexMaintenance = TRUE;
        }
      }
      String_doneTokenizer(&stringTokenizer);
    }

    // get pause end time
    pauseEndDateTime = Misc_getCurrentDateTime()+(uint64)pauseTime;

    // suspend all slaves
    if (globalOptions.serverMode == SERVER_MODE_MASTER)
    {
      error = ERROR_NONE;
      JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        JOB_SLAVE_LIST_ITERATEX(slaveNode,error == ERROR_NONE)
        {
          if (Connector_isAuthorized(&slaveNode->connectorInfo))
          {
            error = Connector_executeCommand(&slaveNode->connectorInfo,
                                             1,
                                             10*MS_PER_SECOND,
                                             CALLBACK_(NULL,NULL),
                                             "SUSPEND modeMask=%S",
                                             modeMask
                                            );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&serverStateLock);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
        return;
      }
    }

    // set pause state
    serverState = SERVER_STATE_PAUSE;

    // log info
    String_clear(modeMask);
    if (pauseFlags.create          ) String_joinCString(modeMask,"create",           ',');
    if (pauseFlags.storage         ) String_joinCString(modeMask,"storage",          ',');
    if (pauseFlags.restore         ) String_joinCString(modeMask,"restore",          ',');
    if (pauseFlags.indexUpdate     ) String_joinCString(modeMask,"index update",     ',');
    if (pauseFlags.indexMaintenance) String_joinCString(modeMask,"index maintenance",',');
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Pause by %s for %dmin: %s",
               getClientInfoString(clientInfo,s,sizeof(s)),
               pauseTime/60,
               String_cString(modeMask)
              );
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(modeMask);
}

/***********************************************************************\
* Name   : serverCommand_suspend
* Purpose: suspend job execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_suspend(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          modeMask;
  StringTokenizer stringTokenizer;
  ConstString     token;
  SlaveNode       *slaveNode;
  Errors          error;
  char            s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get mode
  modeMask = String_new();
  StringMap_getString(argumentMap,"modeMask",modeMask,NULL);

  // set suspend
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // set pause flags
    if (String_isEmpty(modeMask))
    {
      pauseFlags.create           = TRUE;
      pauseFlags.storage          = TRUE;
      pauseFlags.restore          = TRUE;
      pauseFlags.indexUpdate      = TRUE;
      pauseFlags.indexMaintenance = TRUE;
    }
    else
    {
      String_initTokenizer(&stringTokenizer,modeMask,STRING_BEGIN,",",NULL,TRUE);
      while (String_getNextToken(&stringTokenizer,&token,NULL))
      {
        if (String_equalsIgnoreCaseCString(token,"CREATE"))
        {
          pauseFlags.create           = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"STORAGE"))
        {
          pauseFlags.storage          = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"RESTORE"))
        {
          pauseFlags.restore          = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_UPDATE"))
        {
          pauseFlags.indexUpdate      = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_MAINTENANCE"))
        {
          pauseFlags.indexMaintenance = TRUE;
        }
      }
      String_doneTokenizer(&stringTokenizer);
    }

    // suspend all slaves
    if (globalOptions.serverMode == SERVER_MODE_MASTER)
    {
      error = ERROR_NONE;
      JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        JOB_SLAVE_LIST_ITERATEX(slaveNode,error == ERROR_NONE)
        {
          if (Connector_isAuthorized(&slaveNode->connectorInfo))
          {
            error = Connector_executeCommand(&slaveNode->connectorInfo,
                                             1,
                                             10*MS_PER_SECOND,
                                             CALLBACK_(NULL,NULL),
                                             "SUSPEND modeMask=%S",
                                             modeMask
                                            );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&serverStateLock);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
        return;
      }
    }

    // set suspend state
    serverState = SERVER_STATE_SUSPENDED;

    // log info
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Suspended by %s",
               getClientInfoString(clientInfo,s,sizeof(s))
              );
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(modeMask);
}

/***********************************************************************\
* Name   : serverCommand_continue
* Purpose: continue job execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_continue(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  SlaveNode *slaveNode;
  Errors    error;
  char      s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear pause/suspend
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // clear pause flags
    pauseFlags.create           = FALSE;
    pauseFlags.storage          = FALSE;
    pauseFlags.restore          = FALSE;
    pauseFlags.indexUpdate      = FALSE;
    pauseFlags.indexMaintenance = FALSE;

    // set running state on slaves
    if (globalOptions.serverMode == SERVER_MODE_MASTER)
    {
      error = ERROR_NONE;
      JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        JOB_SLAVE_LIST_ITERATEX(slaveNode,error == ERROR_NONE)
        {
          if (Connector_isAuthorized(&slaveNode->connectorInfo))
          {
            error = Connector_executeCommand(&slaveNode->connectorInfo,
                                             1,
                                             10*MS_PER_SECOND,
                                             CALLBACK_(NULL,NULL),
                                             "CONTINUE"
                                            );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&serverStateLock);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
        return;
      }
    }

    // set running state
    serverState = SERVER_STATE_RUNNING;

    // log info
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Continued by %s",
               getClientInfoString(clientInfo,s,sizeof(s))
              );
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_maintenace
* Purpose: set intermediate maintenance time slot
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            time=<maintenance time [s]>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_maintenance(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint maintenanceTime;
  char s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get pause time
  if (!StringMap_getUInt(argumentMap,"time",&maintenanceTime,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"time=<time>");
    return;
  }

  // set intermediate maintenance time
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    intermediateMaintenanceDateTime = Misc_getCurrentDateTime()+maintenanceTime;

    // log info
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Set intermediate maintenance time by %s for %dmin",
               getClientInfoString(clientInfo,s,sizeof(s)),
               maintenanceTime/60
              );
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_deviceList
* Purpose: get device list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>]
*          Result:
*            name=<name>
*            size=<n [bytes]>
*            mounted=yes|no
\***********************************************************************/

LOCAL void serverCommand_deviceList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode    *jobNode;
  Errors           error;
  DeviceListHandle deviceListHandle;
  String           deviceName;
  DeviceInfo       deviceInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get job UUID
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote device list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,FALSE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "DEVICE_LIST"
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get device list fail");
        Job_listUnlock();
        return;
      }
    }
    else
    {
      // local device list

      // open device list
      error = Device_openDeviceList(&deviceListHandle);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open device list fail");
        Job_listUnlock();
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
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"read device list fail");
          Device_closeDeviceList(&deviceListHandle);
          String_delete(deviceName);
          Job_listUnlock();
          return;
        }

        // try get device info
        error = Device_getInfo(&deviceInfo,deviceName,FALSE);
        if (error != ERROR_NONE)
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"read device info fail");
          Device_closeDeviceList(&deviceListHandle);
          String_delete(deviceName);
          Job_listUnlock();
          return;
        }

        if (deviceInfo.type == DEVICE_TYPE_BLOCK)
        {
          ServerIO_sendResult(&clientInfo->io,
                              id,FALSE,ERROR_NONE,
                              "name=%'S size=%"PRIu64" mounted=%y",
                              deviceName,
                              deviceInfo.size,
                              deviceInfo.mounted
                             );
        }
      }
      String_delete(deviceName);

      // close device
      Device_closeDeviceList(&deviceListHandle);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_rootList
* Purpose: root list (local+remote)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>]
*            mounts=yes|no
*          Result:
*            name=<name> size=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_rootList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString   (jobUUID,MISC_UUID_STRING_LENGTH);
  bool           allMountsFlag;
  const JobNode  *jobNode;
  Errors         error;
  RootListHandle rootListHandle;
  String         name;
  DeviceInfo     deviceInfo;
  uint64         size;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, mounts flag
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  if (!StringMap_getBool(argumentMap,"allMounts",&allMountsFlag,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"allMounts=yes|no");
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote root list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,FALSE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "ROOT_LIST allMounts=%y",
                                           allMountsFlag
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get root list fail");
        Job_listUnlock();
        return;
      }
    }
    else
    {
      // local root list

      // open root list
      error = File_openRootList(&rootListHandle,allMountsFlag);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open root list fail");
        Job_listUnlock();
        return;
      }

      // read root list entries
      name = String_new();
      while (!File_endOfRootList(&rootListHandle) && (error == ERROR_NONE))
      {
        error = File_readRootList(&rootListHandle,name);
        if (error == ERROR_NONE)
        {
          if (Device_getInfo(&deviceInfo,name,FALSE) == ERROR_NONE)
          {
            size = deviceInfo.size;
          }
          else
          {
            size = 0;
          }

          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "name=%'S size=%"PRIu64,
                              name,
                              size
                             );
        }
      }
      String_delete(name);

      // close root list
      File_closeRootList(&rootListHandle);

      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"read root list fail");
        Job_listUnlock();
        return;
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_fileInfo
* Purpose: get file info (local+remote)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>]
*            name=<name>
*          Result:
*            fileType=FILE name=<absolute path> size=<n [bytes]> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=DIRECTORY name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noBackup=yes|no noDump=yes|no
*            fileType=LINK destinationFileType=<type> name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=HARDLINK name=<absolute pat> size=<n [bytes]> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=DEVICE CHARACTER name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=DEVICE BLOCK name=<absolute pat> size=<n [bytes]> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=FIFO name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=SOCKET name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
*            fileType=SPECIAL name=<absolute pat> dateTime=<time stamp> hidden=<yes|no> noDump=yes|no
\***********************************************************************/

LOCAL void serverCommand_fileInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  const JobNode *jobNode;
  Errors        error;
  FileInfo      fileInfo;
  bool          noBackupExists;
  FileTypes     destinationFileType;

  assert(clientInfo != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file info
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_INFO name=%'S",
                                           name
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file info fail for '%S'",name);
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      // local file info

      // read file info
      error = File_getInfo(&fileInfo,name);
      if (error == ERROR_NONE)
      {
        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" hidden=%y noDump=%y",
                                name,
                                fileInfo.size,
                                fileInfo.timeModified,
                                File_isHidden(name),
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_DIRECTORY:
            // check if .nobackup exists
            noBackupExists = hasNoBackup(name);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=DIRECTORY name=%'S dateTime=%"PRIu64" hidden=%y noBackup=%y noDump=%y",
                                name,
                                fileInfo.timeModified,
                                File_isHidden(name),
                                noBackupExists,
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_LINK:
            destinationFileType = File_getRealType(name);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=LINK destinationFileType=%s name=%'S dateTime=%"PRIu64" hidden=%y noDump=%y",
                                File_fileTypeToString(destinationFileType,NULL),
                                name,
                                fileInfo.timeModified,
                                File_isHidden(name),
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_HARDLINK:
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=HARDLINK name=%'S size=%"PRIu64" dateTime=%"PRIu64" hidden=%y noDump=%y",
                                name,
                                fileInfo.size,
                                fileInfo.timeModified,
                                File_isHidden(name),
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_SPECIAL:
            switch (fileInfo.specialType)
            {
              case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S size=%"PRIu64" specialType=DEVICE_BLOCK dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_SPECIAL_TYPE_FIFO:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                   );
                break;
              case FILE_SPECIAL_TYPE_SOCKET:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              default:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
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
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file info fail for '%S'",name);
      }
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileList
* Purpose: file list (local+remote)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            directory=<name>
*          Result:
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> hidden=yes|no noBackup=yes|no noDump=yes|no
*            fileType=LINK destinationFileType=<type> name=<name> dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=SPECIAL name=<name> specialType=DEVICE_CHARACTER dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=SPECIAL name=<name> specialType=DEVICE_BLOCK dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=SPECIAL name=<name> specialType=FIFO dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=SPECIAL name=<name> specialType=SOCKET dateTime=<time stamp> hidden=yes|no noDump=yes|no
*            fileType=SPECIAL name=<name> specialType=OTHER dateTime=<time stamp> hidden=yes|no noDump=yes|no
\***********************************************************************/

LOCAL void serverCommand_fileList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString        (jobUUID,MISC_UUID_STRING_LENGTH);
  String              directory;
  const JobNode       *jobNode;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              name;
  FileInfo            fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, directoy
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  directory = String_new();
  if (!StringMap_getString(argumentMap,"directory",directory,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"directory=<name>");
    String_delete(directory);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(directory);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,FALSE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_LIST directory=%'S",
                                           directory
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open directory '%S' fail",directory);
        Job_listUnlock();
        String_delete(directory);
        return;
      }
    }
    else
    {
      // local file list

      // open directory
      error = File_openDirectoryList(&directoryListHandle,
                                     directory
                                    );
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open directory '%S' fail",directory);
        Job_listUnlock();
        String_delete(directory);
        return;
      }

      // read directory entries
      name = String_new();
      while (!File_endOfDirectoryList(&directoryListHandle) && (error == ERROR_NONE))
      {
        error = File_readDirectoryList(&directoryListHandle,name);
        if (error == ERROR_NONE)
        {
          if (File_getInfo(&fileInfo,name) == ERROR_NONE)
          {
            switch (fileInfo.type)
            {
              case FILE_TYPE_FILE:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_DIRECTORY:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=DIRECTORY name=%'S dateTime=%"PRIu64" hidden=%y noBackup=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    hasNoBackup(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_LINK:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=LINK destinationFileType=%s name=%'S dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    File_fileTypeToString(File_getRealType(name),NULL),
                                    name,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                   );
                break;
              case FILE_TYPE_HARDLINK:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=HARDLINK name=%'S size=%"PRIu64" dateTime=%"PRIu64" hidden=%y noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_isHidden(name),
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_SPECIAL:
                switch (fileInfo.specialType)
                {
                  case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%"PRIu64" hidden=%y noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_isHidden(name),
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S size=%"PRIu64" specialType=DEVICE_BLOCK dateTime=%"PRIu64" hidden=%y noDump=%y",
                                        name,
                                        fileInfo.size,
                                        fileInfo.timeModified,
                                        File_isHidden(name),
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_FIFO:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%"PRIu64" hidden=%y noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_isHidden(name),
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_SOCKET:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%"PRIu64" hidden=%y noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_isHidden(name),
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  default:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%"PRIu64" hidden=%y noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_isHidden(name),
                                        File_hasAttributeNoDump(&fileInfo)
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
            ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                "fileType=UNKNOWN name=%'S",
                                name
                               );
          }
        }
      }
      String_delete(name);

      // close directory
      File_closeDirectoryList(&directoryListHandle);

      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"read directory '%S' fail",directory);
        Job_listUnlock();
        String_delete(directory);
        return;
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(directory);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeGet
* Purpose: get file attribute
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<name>
*            attribute=NOBACKUP|NODUMP
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_fileAttributeGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString   (jobUUID,MISC_UUID_STRING_LENGTH);
  String         name;
  String         attribute;
  const JobNode  *jobNode;
  Errors         error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, attribute
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"attribute=<name>");
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_ATTRIBUTE_GET name=%'S attribute=%S",
                                           name,
                                           attribute
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file attribute fail");
        Job_listUnlock();
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      // get attribute value
      if      (String_equalsCString(attribute,"NOBACKUP"))
      {
        // .nobackup
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%y",hasNoBackup(name));
      }
      else if (String_equalsCString(attribute,"NODUMP"))
      {
        // nodump attribute
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%y",hasNoDumpAttribute(name));
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute '%S' of '%S'",attribute,name);
      }
    }
  }

  // free resources
  String_delete(attribute);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeSet
* Purpose: set file attribute
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<name>
*            attribute=NOBACKUP|NODUMP
*            value=<value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileAttributeSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString   (jobUUID,MISC_UUID_STRING_LENGTH);
  String         name;
  String         attribute;
  String         value;
  const JobNode  *jobNode;
  Errors         error;
  String         noBackupFileName;
  FileAttributes fileAttributes;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, value
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"attribute=<name>");
    String_delete(name);
    return;
  }
  value = String_new();
  StringMap_getString(argumentMap,"value",value,NULL);
//TODO: value still not used
UNUSED_VARIABLE(value);

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(value);
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_ATTRIBUTE_SET name=%'S attribute=%S value=%'S",
                                           name,
                                           attribute,
                                           value
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"set file attribute fail");
        Job_listUnlock();
        String_delete(value);
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      // set attribute
      if      (String_equalsCString(attribute,"NOBACKUP"))
      {
        if (!hasNoBackup(name))
        {
          noBackupFileName = File_appendFileNameCString(File_setFileName(String_new(),name),".nobackup");
          error = File_touch(noBackupFileName);
          if (error != ERROR_NONE)
          {
            String_delete(noBackupFileName);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot create .nobackup file");
            Job_listUnlock();
            String_delete(value);
            String_delete(attribute);
            String_delete(name);
            return;
          }
          String_delete(noBackupFileName);
        }

        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else if (String_equalsCString(attribute,"NODUMP"))
      {
        error = File_getAttributes(&fileAttributes,name);
        if (error != ERROR_NONE)
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file attributes fail for '%S'",name);
          Job_listUnlock();
          String_delete(value);
          String_delete(attribute);
          String_delete(name);
          return;
        }

        fileAttributes |= FILE_ATTRIBUTE_NO_DUMP;
        error = File_setAttributes(fileAttributes,name);
        if (error != ERROR_NONE)
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"set attribute no-dump fail for '%S'",name);
          Job_listUnlock();
          String_delete(value);
          String_delete(attribute);
          String_delete(name);
          return;
        }

        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute '%S' of '%S'",attribute,name);
      }
    }
  }

  // free resources
  String_delete(value);
  String_delete(attribute);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeClear
* Purpose: clear file attribute
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<name>
*            attribute=NOBACKUP|NODUMP
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileAttributeClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString   (jobUUID,MISC_UUID_STRING_LENGTH);
  String         name;
  String         attribute;
  const JobNode  *jobNode;
  Errors         error;
  String         noBackupFileName1,noBackupFileName2;
  Errors         tmpError;
  FileAttributes fileAttributes;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, attribute
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"attribute=<name>");
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_ATTRIBUTE_CLEAR name=%'S attribute=%S",
                                           name,
                                           attribute
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"clear file attribute fail");
        Job_listUnlock();
        String_delete(attribute);
        String_delete(name);
        return;
      }
    }
    else
    {
      // clear attribute
      if      (String_equalsCString(attribute,"NOBACKUP"))
      {
        if (hasNoBackup(name))
        {
          noBackupFileName1 = File_appendFileNameCString(File_setFileName(String_new(),name),".nobackup");
          noBackupFileName2 = File_appendFileNameCString(File_setFileName(String_new(),name),".NOBACKUP");

          error = ERROR_NONE;
          tmpError = File_delete(noBackupFileName1,FALSE);
          if (tmpError != ERROR_NONE) error = tmpError;
          tmpError = File_delete(noBackupFileName2,FALSE);
          if (tmpError != ERROR_NONE) error = tmpError;
          if (error != ERROR_NONE)
          {
            String_delete(noBackupFileName2);
            String_delete(noBackupFileName1);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot delete .nobackup file");
            Job_listUnlock();
            String_delete(attribute);
            String_delete(name);
            return;
          }

          String_delete(noBackupFileName2);
          String_delete(noBackupFileName1);
        }

        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else if (String_equalsCString(attribute,"NODUMP"))
      {
        error = File_getAttributes(&fileAttributes,name);
        if (error == ERROR_NONE)
        {
          if ((fileAttributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
          {
            fileAttributes &= ~FILE_ATTRIBUTE_NO_DUMP;
            error = File_setAttributes(fileAttributes,name);
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"xxx1set attribute no-dump fail for '%S'",name);
              Job_listUnlock();
              String_delete(attribute);
              String_delete(name);
              return;
            }
          }
        }

        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute '%S' of '%S'",attribute,name);
      }
    }
  }

  // free resources
  String_delete(attribute);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileMkdir
* Purpose: create directory
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileMkdir(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  const JobNode *jobNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote create directory
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_DELETE name=%'S",
                                           name
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file attribute fail");
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      // create directory
      error = File_makeDirectory(name,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSIONS,
                                 TRUE
                                );
      if (error == ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"create directory '%S' fail",name);
      }
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileDelete
* Purpose: delete file
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  const JobNode *jobNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote delete file/directory
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_DELETE name=%'S",
                                           name
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
    }
    else
    {
      // delete file/directory
      error = File_delete(name,TRUE);
      if (error == ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"delete file '%S' fail",name);
      }
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_directoryInfo
* Purpose: get directory info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|""
*            name=<directory>
*            timeout=<n [ms]>|-1
*          Result:
*            count=<file count> size=<total file size> timedOut=yes|no
\***********************************************************************/

LOCAL void serverCommand_directoryInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString      (jobUUID,MISC_UUID_STRING_LENGTH);
  String            name;
  int64             timeout;
  const JobNode     *jobNode;
  Errors            error;
  DirectoryInfoNode *directoryInfoNode;
  uint64            fileCount;
  uint64            fileSize;
  bool              timedOut;
  FileInfo          fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, path/file name, timeout
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  StringMap_getInt64(argumentMap,"timeout",&timeout,0LL);

//TODO: avoid long running lock
  fileCount = 0LL;
  fileSize  = 0LL;
  timedOut  = FALSE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "DIRECTORY_INFO name=%'S timeout=%"PRIi64"",
                                           name,
                                           timeout
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get directory info fail");
        Job_listUnlock();
        String_delete(name);
        return;
      }
    }
    else
    {
      if (File_isDirectory(name))
      {
    //TODO: avoid lock with getDirectoryInfo inside
        SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
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
      }
      else
      {
        // get file size
        if (File_getInfo(&fileInfo,name) == ERROR_NONE)
        {
          fileCount = 1LL;
          fileSize  = fileInfo.size;
        }
      }

      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"count=%"PRIu64" size=%"PRIu64" timedOut=%y",fileCount,fileSize,timedOut);
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_testScript
* Purpose: test script
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            script=<script>
*          Result:
*            line=<text>
\***********************************************************************/

LOCAL void serverCommand_testScript(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  String        script;
  const JobNode *jobNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, script
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  script = String_new();
  if (!StringMap_getString(argumentMap,"script",script,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"script=<script>");
    String_delete(script);
    String_delete(name);
    return;
  }

//TODO: avoid long running lock
  error = ERROR_UNKNOWN;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    if (!String_isEmpty(jobUUID))
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(script);
        String_delete(name);
        return;
      }
    }
    else
    {
      jobNode = NULL;
    }

    if ((jobNode != NULL) && Job_isRemote(jobNode))
    {
      // remote file list
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "TEST_SCRIPT name=%'S script=%'S",
                                           name,
                                           script
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"test script fail");
        Job_listUnlock();
        String_delete(script);
        String_delete(name);
        return;
      }
    }
    else
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK_INLINE(void,(ConstString line, void *userData),
                                 {
                                   UNUSED_VARIABLE(userData);

                                   ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"line=%'S",line);
                                 },NULL),
                                 CALLBACK_INLINE(void,(ConstString line, void *userData),
                                 {
                                   UNUSED_VARIABLE(userData);

                                   ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"line=%'S",line);
                                 },NULL)
                                );
    }
  }
  if (error == ERROR_NONE)
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Test script '%s' OK",
               String_cString(name)
              );
  }
  else
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Test script '%s' fail (error: %s)",
               String_cString(name),
               Error_getText(error)
              );
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");

  // free resources
  String_delete(script);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionGet
* Purpose: get job option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode     *jobNode;
  uint              i;
  String            s;
  ConfigValueFormat configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // find config value
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,NULL,String_cString(name));
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config '%S'",name);
      Job_listUnlock();
      String_delete(name);
      return;
    }
    assert(BAR_CONFIG_VALUES[i].type != CONFIG_VALUE_TYPE_DEPRECATED);

    if (BAR_CONFIG_VALUES[i].type != CONFIG_VALUE_TYPE_DEPRECATED)
    {
      // send value
      s = String_new();
      ConfigValue_formatInit(&configValueFormat,
                             &JOB_CONFIG_VALUES[i],
                             CONFIG_VALUE_FORMAT_MODE_VALUE,
                             jobNode
                            );
      ConfigValue_format(&configValueFormat,s);
      ConfigValue_formatDone(&configValueFormat);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%S",s);
      String_delete(s);
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DEPRECATED_OR_IGNORED_VALUE,"%S",name);
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionSet
* Purpose: set job option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       name,value;
  JobNode      *jobNode;
  uint         i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, value
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"value=<value>");
    String_delete(value);
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    // parse
    i = ConfigValue_find(JOB_CONFIG_VALUES,
                         CONFIG_VALUE_INDEX_NONE,
                         CONFIG_VALUE_INDEX_NONE,
                         String_cString(name)
                        );
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_VALUE,"invalid job config '%S' value: '%S'",name,value);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    if (ConfigValue_parse(JOB_CONFIG_VALUES,
                          &JOB_CONFIG_VALUES[i],
                          NULL, // sectionName
                          String_cString(value),
                          CALLBACK_(NULL,NULL),  // errorFunction
                          CALLBACK_(NULL,NULL),  // warningFunction
                          jobNode,
                          NULL // commentLineList
                         )
       )
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

      // set modified, trigger re-pairing
      Job_setModified(jobNode);
      if (globalOptions.serverMode == SERVER_MODE_MASTER)
      {
        Semaphore_signalModified(&pairingThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
      }
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_VALUE,"invalid job config '%S' value: '%S'",name,value);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }
  }

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionDelete
* Purpose: delete job option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobOptionDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       name;
  JobNode      *jobNode;
  uint         i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // find config value
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,NULL,String_cString(name));
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config '%S'",name);
      Job_listUnlock();
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
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            jobUUID=<uuid> \
*            master=<name> \
*            name=<name> \
*            state=<state> \
*            slaveHostName=<name> \
*            slaveHostPort=<n> \
*            slaveHostForceSSL=yes|no
*            slaveState=<slave state>
*            archiveType=<type> \
*            archivePartSize=<n> \
*            deltaCompressAlgorithm=<delta compress algorithm> \
*            byteCompressAlgorithm=<byte compress algorithm> \
*            cryptAlgorithm=<crypt algorithm> \
*            cryptType=<crypt type> \
*            cryptPasswordMode=<password mode> \
*            lastExecutedDateTime=<timestamp> \
*            lastErrorMessage=<text> \
*            estimatedRestTime=<n [s]>
\***********************************************************************/

LOCAL void serverCommand_jobList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  const JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    JOB_LIST_ITERATEX(jobNode,!isQuit() && !isCommandAborted(clientInfo,id))
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "jobUUID=%S master=%'S name=%'S state=%s slaveHostName=%'S slaveHostPort=%d slaveTLSMode=%s slaveState=%'s slaveTLS=%y slaveInsecureTLS=%y archiveType=%s archivePartSize=%"PRIu64" deltaCompressAlgorithm=%s byteCompressAlgorithm=%s cryptAlgorithm=%'s cryptType=%'s cryptPasswordMode=%'s lastExecutedDateTime=%"PRIu64" lastErrorCode=%u lastErrorData=%'S estimatedRestTime=%lu volumeRequest=%s volumeRequestNumber=%u messageCode=%s messageText=%'S",
                          jobNode->job.uuid,
                          (jobNode->masterIO != NULL) ? jobNode->masterIO->network.name : NULL,
                          jobNode->name,
                          Job_getStateText(jobNode->jobState,jobNode->noStorage,jobNode->dryRun),
                          jobNode->job.slaveHost.name,
                          jobNode->job.slaveHost.port,
                          ConfigValue_selectToString(CONFIG_VALUE_TLS_MODES,
                                                     jobNode->job.slaveHost.tlsMode,
                                                     NULL
                                                    ),
                          getSlaveStateText(jobNode->slaveState),
                          jobNode->slaveTLS,
                          jobNode->slaveInsecureTLS,
                          ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,
                                                     (   (jobNode->archiveType == ARCHIVE_TYPE_FULL        )
                                                      || (jobNode->archiveType == ARCHIVE_TYPE_INCREMENTAL )
                                                      || (jobNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
                                                     )
                                                       ? jobNode->archiveType
                                                       : jobNode->job.options.archiveType,
                                                     NULL
                                                    ),
                          jobNode->job.options.archivePartSize,
                          Compress_algorithmToString(jobNode->job.options.compressAlgorithms.delta,NULL),
                          Compress_algorithmToString(jobNode->job.options.compressAlgorithms.byte,NULL),
                          Crypt_algorithmToString(jobNode->job.options.cryptAlgorithms[0],"unknown"),
                          (jobNode->job.options.cryptAlgorithms[0] != CRYPT_ALGORITHM_NONE) ? Crypt_typeToString(jobNode->job.options.cryptType) : "none",
                          ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobNode->job.options.cryptPasswordMode,NULL),
                          jobNode->runningInfo.lastExecutedDateTime,
                          jobNode->runningInfo.lastErrorCode,
                          jobNode->runningInfo.lastErrorData,
                          jobNode->runningInfo.estimatedRestTime,
                          volumeRequestToString(jobNode->runningInfo.volumeRequest),
                          jobNode->runningInfo.volumeRequestNumber,
                          messageCodeToString(jobNode->runningInfo.message.code),
                          jobNode->runningInfo.message.text
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobInfo
* Purpose: get job info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            lastExecutedDateTime=<time stamp>
*            lastErrorCode=<n>
*            lastErrorData=<text>
*            executionCountNormal=<n>
*            executionCountFull=<n>
*            executionCountIncremental=<n>
*            executionCountDifferential=<n>
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

LOCAL void serverCommand_jobInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode *jobNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    if (Job_isRemote(jobNode) && Job_isRunning(jobNode->jobState))
    {
      // remote job state
      error = ERROR_UNKNOWN;
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "JOB_INFO jobUUID=%S",
                                           jobUUID
                                          );
        }
        else
        {
          error = ERROR_SLAVE_DISCONNECTED;
        }
      }
      assert(error != ERROR_UNKNOWN);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get job status fail");
        Job_listUnlock();
        return;
      }
    }
    else
    {
      // local job: format and send result
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                          "lastExecutedDateTime=%"PRIu64" lastErrorCode=%u lastErrorData=%'S executionCountNormal=%lu executionCountFull=%lu executionCountIncremental=%lu executionCountDifferential=%lu executionCountContinuous=%lu averageDurationNormal=%"PRIu64" averageDurationFull=%"PRIu64" averageDurationIncremental=%"PRIu64" averageDurationDifferential=%"PRIu64" averageDurationContinuous=%"PRIu64" totalEntityCount=%lu totalStorageCount=%lu totalStorageSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64,
                          jobNode->runningInfo.lastExecutedDateTime,
                          jobNode->runningInfo.lastErrorCode,
                          jobNode->runningInfo.lastErrorData,
                          jobNode->executionCount.normal,
                          jobNode->executionCount.full,
                          jobNode->executionCount.incremental,
                          jobNode->executionCount.differential,
                          jobNode->executionCount.continuous,
                          jobNode->averageDuration.normal,
                          jobNode->averageDuration.full,
                          jobNode->averageDuration.incremental,
                          jobNode->averageDuration.differential,
                          jobNode->averageDuration.continuous,
                          jobNode->totalEntityCount,
                          jobNode->totalStorageCount,
                          jobNode->totalStorageSize,
                          jobNode->totalEntryCount,
                          jobNode->totalEntrySize
                         );
    }
  }
}

/***********************************************************************\
* Name   : serverCommand_jobStart
* Purpose: start job execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            [scheduleUUID=<text>]
*            [customText=<text>]
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [testCreatedArchives=yes|no]
*            [noStorage=yes|no]
*            [dryRun=yes|no]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobStart(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       customText;
  ArchiveTypes archiveType;
  bool         testCreatedArchives;
  bool         noStorage;
  bool         dryRun;
  JobNode      *jobNode;
  char         s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, schedule custom text, no-storage, archive type, dry-run
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL);
  customText = String_new();
  StringMap_getString(argumentMap,"customText",customText,NULL);
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(customText);
    return;
  }
  StringMap_getBool(argumentMap,"testCreatedArchives",&testCreatedArchives,FALSE);
  StringMap_getBool(argumentMap,"noStorage",&noStorage,FALSE);
  StringMap_getBool(argumentMap,"dryRun",&dryRun,FALSE);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      String_delete(customText);
      Job_listUnlock();
      return;
    }

    // run job
    if  (!Job_isActive(jobNode->jobState))
    {
      // trigger job
      Job_trigger(jobNode,
                  scheduleUUID,
                  archiveType,
                  customText,
                  testCreatedArchives,
                  noStorage,
                  dryRun,
                  Misc_getCurrentDateTime(),
                  getClientInfoString(clientInfo,s,sizeof(s))
                 );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(customText);
}

/***********************************************************************\
* Name   : serverCommand_jobAbort
* Purpose: abort job execution
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobAbort(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;
  char         s[64];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // abort job
    if (Job_isActive(jobNode->jobState))
    {
      Job_abort(jobNode,getClientInfoString(clientInfo,s,sizeof(s)));
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobReset
* Purpose: reset job error and running info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobReset(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // reset job
    if (!Job_isActive(jobNode->jobState))
    {
      Job_reset(jobNode);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobNew
* Purpose: create new job
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            [jobUUID=<uuid>]
*            [master=<name>]
*          Result:
*            jobUUID=<uuid>
\***********************************************************************/

LOCAL void serverCommand_jobNew(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String       name;
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       master;
  String       fileName;
  FileHandle   fileHandle;
  Errors       error;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, job UUID, master
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  master = String_new();
  StringMap_getString(argumentMap,"master",master,NULL);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    jobNode = NULL;

    if (globalOptions.serverMode == SERVER_MODE_MASTER)
    {
      // add new local job

      // check if job already exists
      if (Job_exists(name))
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_ALREADY_EXISTS,"%S",name);
        Job_listUnlock();
        String_delete(master);
        String_delete(name);
        return;
      }

      // create empty job file
      fileName = File_appendFileName(File_setFileName(String_new(),globalOptions.jobsDirectory),name);
      error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
      if (error != ERROR_NONE)
      {
        String_delete(fileName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_CREATE_JOB,"%S",name);
        Job_listUnlock();
        String_delete(master);
        String_delete(name);
        return;
      }
      File_close(&fileHandle);
      (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

      // create new job
      jobNode = Job_new(JOB_TYPE_CREATE,
                        name,
                        jobUUID,
                        fileName
                       );
      assert(jobNode != NULL);

      // write job to file
      error = Job_write(jobNode);
      if (error != ERROR_NONE)
      {
        printWarning("cannot create job '%s' (error: %s)",String_cString(jobNode->fileName),Error_getText(error));
      }

      // add new job to list
      Job_listAppend(jobNode);

      // free resources
      String_delete(fileName);
    }
    else
    {
      // temporary add remote job from master

      // find/create temporary job
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        // create temporary job
        jobNode = Job_new(JOB_TYPE_CREATE,
                          name,
                          jobUUID,
                          NULL // fileName
                         );
        assert(jobNode != NULL);

        // add new job to list
        Job_listAppend(jobNode);
      }

      // set master i/o
      jobNode->masterIO = &clientInfo->io;
    }

    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"jobUUID=%S",jobNode->job.uuid);
  }

  // free resources
  String_delete(master);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobClone
* Purpose: copy job
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode *jobNode;
  String        fileName;
  FileHandle    fileHandle;
  Errors        error;
  JobNode       *newJobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // check if mew job already exists
    if (Job_exists(name))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_ALREADY_EXISTS,"%s",name);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // create empty job file
    fileName = File_appendFileName(File_setFileName(String_new(),globalOptions.jobsDirectory),name);
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_CREATE_JOB,"%S",name);
      Job_listUnlock();
      String_delete(name);
      return;
    }
    File_close(&fileHandle);
    (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

    // copy job
    newJobNode = Job_copy(jobNode,fileName);
    assert(newJobNode != NULL);

    // get new UUID
    Misc_getUUID(newJobNode->job.uuid);

    // free resources
    String_delete(fileName);

    // write job to file
    error = Job_write(newJobNode);
    if (error != ERROR_NONE)
    {
      printWarning("cannot update job '%s' (error: %s)",String_cString(jobNode->fileName),Error_getText(error));
    }

    // write initial schedule info
    Job_writeScheduleInfo(newJobNode,ARCHIVE_TYPE_NONE,0LL);

    // add new job to list
    Job_listAppend(newJobNode);

    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"jobUUID=%S",newJobNode->job.uuid);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobRename
* Purpose: rename job
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<new job name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobRename(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       newName;
  JobNode      *jobNode;
  String       fileName;
  Errors       error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  newName = String_new();
  if (!StringMap_getString(argumentMap,"newName",newName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"newName=<name>");
    String_delete(newName);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // check if job already exists
    if (Job_exists(newName))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_ALREADY_EXISTS,"%S",newName);
      Job_listUnlock();
      String_delete(newName);
      return;
    }

    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(newName);
      return;
    }

    // rename job
    fileName = File_appendFileName(File_setFileName(String_new(),globalOptions.jobsDirectory),newName);
    error = File_rename(jobNode->fileName,fileName,NULL);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_RENAME_JOB,"%S",jobUUID);
      Job_listUnlock();
      String_delete(newName);
      return;
    }

    // free resources
    String_delete(fileName);

    // store new file name
    File_appendFileName(File_setFileName(jobNode->fileName,globalOptions.jobsDirectory),newName);
    String_set(jobNode->name,newName);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(newName);
}

/***********************************************************************\
* Name   : serverCommand_jobDelete
* Purpose: delete job
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;
  Errors       error;
  String       fileName,baseName;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // remove job in list if not running, has not requested a password and has not requested a new volume
    if (Job_isRunning(jobNode->jobState))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_RUNNING,"%S",jobNode->name);
      Job_listUnlock();
      return;
    }

    if (jobNode->fileName != NULL)
    {
      // delete job file
      error = File_delete(jobNode->fileName,FALSE);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DELETE_JOB,"%S",jobNode->fileName);
        Job_listUnlock();
        return;
      }

      // delete job schedule state file
      fileName = String_new();
      baseName = String_new();
      File_splitFileName(jobNode->fileName,fileName,baseName,NULL);
      File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
      (void)File_delete(fileName,FALSE);
      String_delete(baseName);
      String_delete(fileName);
    }

    // remove from list and delete
    Job_listRemove(jobNode);
    Job_delete(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobFlush
* Purpose: flush all job data (write to disk)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid id>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobFlush(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get job UUID
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);

  if (!String_isEmpty(jobUUID))
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      JobNode *jobNode;

      // find job
      if (!String_isEmpty(jobUUID))
      {
        jobNode = Job_findByUUID(jobUUID);
        if (jobNode == NULL)
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
          Job_listUnlock();
          return;
        }

        Job_flush(jobNode);
      }
    }
  }
  else
  {
    Job_flushAll();
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobStatus
* Purpose: get job status
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            state=<state>
*            errorCode=<n>
*            errorNumber=<n>
*            errorData=<text>
*            doneCount=<n>
*            doneSize=<n [bytes]>
*            totalEntryCount=<n>
*            totalEntrySize=<n [bytes]>
*            collectTotalSumDone=yes|no
*            skippedEntryCount=<n>
*            skippedEntrySize=<n [bytes]>
*            errorEntryCount=<n>
*            errorEntrySize=<n [bytes]>
*            archiveSize=<n [bytes]>
*            compressionRatio=<ratio>
*            entryName=<name>
*            entryDoneSize=<n [bytes]>
*            entryTotalSize=<n [bytes]>
*            storageName=<name>
*            storageDoneSize=<n [bytes]>
*            storageTotalSize=<n [bytes]>
*            volumeNumber=<number>
*            volumeProgress=<n [0..100]>
*            volumeRequest=<text>
*            volumeRequestNumber=<n>
*            messageCode=<text>
*            message=<text>
*            entriesPerSecond=<n [1/s]>
*            bytesPerSecond=<n [bytes/s]>
*            storageBytesPerSecond=<n [bytes/s]>
*            estimatedRestTime=<n [s]>
\***********************************************************************/

LOCAL void serverCommand_jobStatus(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }
// TODO:fprintf(stderr,"%s:%d: %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,jobNode->statusInfo.entry.doneSize,jobNode->statusInfo.entry.totalSize);

    // format and send result
    // Note: remote jobs status is updated in jobThreadRun->Connector_create()
// TODO: rename totalEntry* -> total*
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                        "state=%s errorCode=%u errorNumber=%d errorData=%'S doneCount=%lu doneSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" collectTotalSumDone=%y skippedEntryCount=%lu skippedEntrySize=%"PRIu64" errorEntryCount=%lu errorEntrySize=%"PRIu64" archiveSize=%"PRIu64" compressionRatio=%lf entryName=%'S entryDoneSize=%"PRIu64" entryTotalSize=%"PRIu64" storageName=%'S storageDoneSize=%"PRIu64" storageTotalSize=%"PRIu64" volumeNumber=%d volumeDone=%lf entriesPerSecond=%lf bytesPerSecond=%lf storageBytesPerSecond=%lf estimatedRestTime=%lu volumeRequest=%s volumeRequestNumber=%d messageCode=%s messageText=%'S",
                        Job_getStateText(jobNode->jobState,jobNode->noStorage,jobNode->dryRun),
                        jobNode->runningInfo.lastErrorCode,
                        jobNode->runningInfo.lastErrorNumber,
                        jobNode->runningInfo.lastErrorData,
                        jobNode->runningInfo.progress.done.count,
                        jobNode->runningInfo.progress.done.size,
                        jobNode->runningInfo.progress.total.count,
                        jobNode->runningInfo.progress.total.size,
                        jobNode->runningInfo.progress.collectTotalSumDone,
                        jobNode->runningInfo.progress.skipped.count,
                        jobNode->runningInfo.progress.skipped.size,
                        jobNode->runningInfo.progress.error.count,
                        jobNode->runningInfo.progress.error.size,
                        jobNode->runningInfo.progress.archiveSize,
                        jobNode->runningInfo.progress.compressionRatio,
                        jobNode->runningInfo.progress.entry.name,
                        jobNode->runningInfo.progress.entry.doneSize,
                        jobNode->runningInfo.progress.entry.totalSize,
                        jobNode->runningInfo.progress.storage.name,
                        jobNode->runningInfo.progress.storage.doneSize,
                        jobNode->runningInfo.progress.storage.totalSize,
                        jobNode->runningInfo.progress.volume.number,
                        jobNode->runningInfo.progress.volume.done,
                        jobNode->runningInfo.entriesPerSecond,
                        jobNode->runningInfo.bytesPerSecond,
                        jobNode->runningInfo.storageBytesPerSecond,
                        jobNode->runningInfo.estimatedRestTime,
                        volumeRequestToString(jobNode->runningInfo.volumeRequest),
                        jobNode->runningInfo.volumeRequestNumber,
                        messageCodeToString(jobNode->runningInfo.message.code),
                        jobNode->runningInfo.message.text
                       );
  }
}

/***********************************************************************\
* Name   : serverCommand_includeList
* Purpose: get job include list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode *jobNode;
  EntryNode     *entryNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // send include list
    LIST_ITERATE(&jobNode->job.includeEntryList,entryNode)
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "id=%u entryType=%s pattern=%'S patternType=%s",
                          entryNode->id,
                          EntryList_entryTypeToString(entryNode->type,"unknown"),
                          entryNode->string,
                          Pattern_patternTypeToString(entryNode->pattern.type,"unknown")
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeListClear
* Purpose: clear job include list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear include list
    EntryList_clear(&jobNode->job.includeEntryList);

    // notify modfiied include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeListAdd
* Purpose: add entry to job include list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  EntryTypes   entryType;
  String       patternString;
  PatternTypes patternType;
  JobNode      *jobNode;
  uint         entryId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, entry type, pattern, pattern type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,CALLBACK_((StringMapParseEnumFunction)EntryList_parseEntryType,NULL),ENTRY_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entryType=FILE|IMAGE");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // add to include list
    EntryList_append(&jobNode->job.includeEntryList,entryType,patternString,patternType,&entryId);

    // notify modified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",entryId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_includeListUpdate
* Purpose: update entry to job include list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         entryId;
  EntryTypes   entryType;
  PatternTypes patternType;
  String       patternString;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, entry type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&entryId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,CALLBACK_((StringMapParseEnumFunction)EntryList_parseEntryType,NULL),ENTRY_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entryType=FILE|IMAGE");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // update include list
    EntryList_update(&jobNode->job.includeEntryList,entryId,entryType,patternString,patternType);

    // notify modified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_includeListRemove
* Purpose: remove entry from job include list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         entryId;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&entryId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // remove from include list
    if (!EntryList_remove(&jobNode->job.includeEntryList,entryId))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_ENTRY_NOT_FOUND,"%u",entryId);
      Job_listUnlock();
      return;
    }

    // notify modified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeList
* Purpose: get job exclude list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode *jobNode;
  PatternNode   *patternNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // send exclude list
    LIST_ITERATE(&jobNode->job.excludePatternList,patternNode)
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "id=%u pattern=%'S patternType=%s",
                          patternNode->id,
                          patternNode->string,
                          Pattern_patternTypeToString(patternNode->pattern.type,"unknown")
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListClear
* Purpose: clear job exclude list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->job.excludePatternList);

    // notify modified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListAdd
* Purpose: add entry to job exclude list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  PatternTypes patternType;
  String       patternString;
  JobNode      *jobNode;
  uint         patternId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // add to exclude list
    #ifndef NDEBUG
      patternId = globalOptions.debug.serverFixedIdsFlag ? 1 : MISC_ID_NONE;
    #else
      patternId = MISC_ID_NONE;
    #endif
    PatternList_append(&jobNode->job.excludePatternList,patternString,patternType,&patternId);

    // notify mpdified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",patternId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeListUpdate
* Purpose: update entry in job exclude list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         patternId;
  PatternTypes patternType;
  String       patternString;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, pattern id, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // update exclude list
    PatternList_update(&jobNode->job.excludePatternList,patternId,patternString,patternType);

    // notify modified include/exclude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeListRemove
* Purpose: remove entry from job exclude list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         patternId;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // remove from exclude list
    if (!PatternList_remove(&jobNode->job.excludePatternList,patternId))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PATTERN_ID_NOT_FOUND,"%u",patternId);
      Job_listUnlock();
      return;
    }

    // notify modified include/execlude lists
    Job_setIncludeExcludeModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_mountList
* Purpose: get job mount list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            id=<n> \
*            name=<name> \
*            device=<name> \
*            ...
\***********************************************************************/

LOCAL void serverCommand_mountList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode *jobNode;
  MountNode     *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // send mount list
    LIST_ITERATE(&jobNode->job.options.mountList,mountNode)
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "id=%u name=%'S device=%'S",
                          mountNode->id,
                          mountNode->name,
                          mountNode->device
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_mountListClear
* Purpose: clear job mount list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_mountListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear mount list
    List_clear(&jobNode->job.options.mountList);

    // notify modified mounts
    Job_setMountModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_mountListAdd
* Purpose: add entry to job mountlist
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*            device=<name>
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_mountListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       name;
  String       device;
  JobNode      *jobNode;
  MountNode    *mountNode;
  uint         mountId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, name, device
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  device = String_new();
  if (!StringMap_getString(argumentMap,"device",device,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"device=<name>");
    String_delete(device);
    String_delete(name);
    return;
  }

  mountId = 0;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(device);
      String_delete(name);
      return;
    }

    // add to mount list
    mountNode = Configuration_newMountNode(name,device);
    if (mountNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,
                          id,
                          TRUE,
                          ERROR_INSUFFICIENT_MEMORY,
                          "cannot add mount node"
                         );
      Job_listUnlock();
      String_delete(device);
      String_delete(name);
      return;
    }
    List_append(&jobNode->job.options.mountList,mountNode);

    // get id
    mountId = mountNode->id;

    // notify modified mounts
    Job_setMountModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",mountId);

  // free resources
  String_delete(device);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_mountListUpdate
* Purpose: update entry to job mount list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            name=<name>
*            device=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_mountListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         mountId;
  String       name;
  String       device;
  JobNode      *jobNode;
  MountNode    *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, mount id, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&mountId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  device = String_new();
  if (!StringMap_getString(argumentMap,"device",device,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"device=<name>");
    String_delete(device);
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(device);
      String_delete(name);
      return;
    }

    // get mount
    mountNode = LIST_FIND(&jobNode->job.options.mountList,mountNode,mountNode->id == mountId);
    if (mountNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_MOUNT_ID_NOT_FOUND,"%u",mountId);
      Job_listUnlock();
      String_delete(device);
      String_delete(name);
      return;
    }

    // update mount list
    String_set(mountNode->name,name);
    String_set(mountNode->device,device);

    // notify modified mounts
    Job_setMountModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(device);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_mountListRemove
* Purpose: remove entry from job mount list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_mountListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         mountId;
  JobNode      *jobNode;
  MountNode    *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, mount id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&mountId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // get mount
    mountNode = LIST_FIND(&jobNode->job.options.mountList,mountNode,mountNode->id == mountId);
    if (mountNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_MOUNT_ID_NOT_FOUND,"%u",mountId);
      Job_listUnlock();
      return;
    }

    // remove from mount list and free
    List_remove(&jobNode->job.options.mountList,mountNode);
    Configuration_deleteMountNode(mountNode);

    // notify modified mounts
    Job_setMountModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceList
* Purpose: get soource list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode   *jobNode;
  DeltaSourceNode *deltaSourceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // send delta source list
    LIST_ITERATE(&jobNode->job.options.deltaSourceList,deltaSourceNode)
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "id=%u pattern=%'S patternType=%s",
                          deltaSourceNode->id,
//TODO
                          deltaSourceNode->storageName,
                          Pattern_patternTypeToString(PATTERN_TYPE_GLOB,"unknown")
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListClear
* Purpose: clear source list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear source list
    DeltaSourceList_clear(&jobNode->job.options.deltaSourceList);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListAdd
* Purpose: add entry to source list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  PatternTypes patternType;
  String       patternString;
  JobNode      *jobNode;
  uint         deltaSourceId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // add to source list
    DeltaSourceList_append(&jobNode->job.options.deltaSourceList,patternString,patternType,&deltaSourceId);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",deltaSourceId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_sourceListUpdate
* Purpose: update entry in source list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         deltaSourceId;
  String       patternString;
  PatternTypes patternType;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&deltaSourceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // update source list
    DeltaSourceList_update(&jobNode->job.options.deltaSourceList,deltaSourceId,patternString,patternType);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_sourceListRemove
* Purpose: remove entry from source list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         deltaSourceId;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&deltaSourceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // remove from source list
    if (!DeltaSourceList_remove(&jobNode->job.options.deltaSourceList,deltaSourceId))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DELTA_SOURCE_ID_NOT_FOUND,"%u",deltaSourceId);
      Job_listUnlock();
      return;
    }

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressList
* Purpose: get job exclude compress list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode *jobNode;
  PatternNode   *patternNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // send exclude list
    LIST_ITERATE(&jobNode->job.options.compressExcludePatternList,patternNode)
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "pattern=%'S patternType=%s",
                          patternNode->string,
                          Pattern_patternTypeToString(patternNode->pattern.type,"unknown")
                         );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListClear
* Purpose: clear job exclude compress list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->job.options.compressExcludePatternList);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListAdd
* Purpose: add entry to job exclude compress list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  String       patternString;
  PatternTypes patternType;
  JobNode      *jobNode;
  uint         patternId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // add to exclude list
    #ifndef NDEBUG
      patternId = globalOptions.debug.serverFixedIdsFlag ? 1 : MISC_ID_NONE;
    #else
      patternId = MISC_ID_NONE;
    #endif
    PatternList_append(&jobNode->job.options.compressExcludePatternList,patternString,patternType,&patternId);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",patternId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListUpdate
* Purpose: update entry in job exclude compress list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         patternId;
  String       patternString;
  PatternTypes patternType;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(patternString);
      return;
    }

    // update exclude list
    PatternList_update(&jobNode->job.options.compressExcludePatternList,patternId,patternString,patternType);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListRemove
* Purpose: remove entry from job exclude compress list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         patternId;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // remove from exclude list
    if (!PatternList_remove(&jobNode->job.options.compressExcludePatternList,patternId))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PATTERN_ID_NOT_FOUND,"%u",patternId);
      Job_listUnlock();
      return;
    }

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_scheduleList
* Purpose: get job schedule list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>]
*            [archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS]
*          Result:
*            jobName=<name> \
*            jobUUID=<uuid> \
*            scheduleUUID=<uuid> \
*            archiveType=normal|full|incremental|differential
*            date=<year>|*-<month>|*-<day>|* \
*            weekDays=<week day>|* \
*            time=<hour>|*:<minute>|* \
*            interval=<n>
*            customText=<text> \
*            beginTime=<hour>|*:<minute>|* \
*            endTime=<hour>|*:<minute>|* \
*            testCreatedArchives=yes|no \
*            noStorage=yes|no \
*            enabled=yes|no \
*            lastExecutedDateTime=<timestamp>|0 \
*            nextExecutedDateTime=<timestamp>|0 \
*            totalEntities=<n>|0 \
*            totalEntryCount=<n>|0 \
*            totalEntrySize=<n>|0 \
*            ...
\***********************************************************************/

LOCAL void serverCommand_scheduleList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes          archiveType;
  JobScheduleList       jobScheduleList;
  const JobScheduleNode *jobScheduleNode;
  String                date,weekDays,time;
  String                beginTime,endTime;
  uint64                now;
  uint64                nextScheduleDateTime;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, archive type
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_NONE);

  List_init(&jobScheduleList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeJobScheduleNode,NULL));

  // get job schedule list (Note: avoid long locking of job list)
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    if (!String_isEmpty(jobUUID))
    {
      // find job
      const JobNode *jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        return;
      }

      // get schedule list of job
      getJobScheduleList(&jobScheduleList,
                         jobNode,
                         JOB_STATESET_ALL,
                         FALSE,  // all schedules
                         ARCHIVE_TYPESET_ALL
                        );
    }
    else
    {
      // get schedule list for all jobs
      const JobNode *jobNode;
      JOB_LIST_ITERATE(jobNode)
      {
        getJobScheduleList(&jobScheduleList,
                           jobNode,
                           JOB_STATESET_ALL,
                           FALSE,  // all schedules
                           ARCHIVE_TYPESET_ALL
                          );
      }
    }
  }

  // send schedule info
  date      = String_new();
  weekDays  = String_new();
  time      = String_new();
  beginTime = String_new();
  endTime   = String_new();
  now       = Misc_getCurrentDateTime();
  LIST_ITERATE(&jobScheduleList,jobScheduleNode)
  {
    if ((archiveType == ARCHIVE_TYPE_NONE) || (jobScheduleNode->archiveType == archiveType))
    {
      // get date string
      String_clear(date);
      if (jobScheduleNode->date.year != DATE_ANY)
      {
        String_appendFormat(date,"%d",jobScheduleNode->date.year);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (jobScheduleNode->date.month != DATE_ANY)
      {
        String_appendFormat(date,"%02d",jobScheduleNode->date.month);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (jobScheduleNode->date.day != DATE_ANY)
      {
        String_appendFormat(date,"%02d",jobScheduleNode->date.day);
      }
      else
      {
        String_appendCString(date,"*");
      }

      // get weekdays string
      String_clear(weekDays);
      if (jobScheduleNode->weekDaySet != WEEKDAY_SET_ANY)
      {
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_MON)) { String_joinCString(weekDays,"Mon",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_TUE)) { String_joinCString(weekDays,"Tue",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_WED)) { String_joinCString(weekDays,"Wed",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_THU)) { String_joinCString(weekDays,"Thu",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_FRI)) { String_joinCString(weekDays,"Fri",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_SAT)) { String_joinCString(weekDays,"Sat",','); }
        if (IN_SET(jobScheduleNode->weekDaySet,WEEKDAY_SUN)) { String_joinCString(weekDays,"Sun",','); }
      }
      else
      {
        String_appendCString(weekDays,"*");
      }

      // get time string
      String_clear(time);
      if (jobScheduleNode->time.hour != TIME_ANY)
      {
        String_appendFormat(time,"%02d",jobScheduleNode->time.hour);
      }
      else
      {
        String_appendCString(time,"*");
      }
      String_appendChar(time,':');
      if (jobScheduleNode->time.minute != TIME_ANY)
      {
        String_appendFormat(time,"%02d",jobScheduleNode->time.minute);
      }
      else
      {
        String_appendCString(time,"*");
      }

      // get begin/end time string
      String_clear(beginTime);
      if (jobScheduleNode->beginTime.hour != TIME_ANY)
      {
        String_appendFormat(beginTime,"%02d",jobScheduleNode->beginTime.hour);
      }
      else
      {
        String_appendCString(beginTime,"*");
      }
      String_appendChar(beginTime,':');
      if (jobScheduleNode->beginTime.minute != TIME_ANY)
      {
        String_appendFormat(beginTime,"%02d",jobScheduleNode->beginTime.minute);
      }
      else
      {
        String_appendCString(beginTime,"*");
      }

      String_clear(endTime);
      if (jobScheduleNode->endTime.hour != TIME_ANY)
      {
        String_appendFormat(endTime,"%02d",jobScheduleNode->endTime.hour);
      }
      else
      {
        String_appendCString(endTime,"*");
      }
      String_appendChar(endTime,':');
      if (jobScheduleNode->endTime.minute != TIME_ANY)
      {
        String_appendFormat(endTime,"%02d",jobScheduleNode->endTime.minute);
      }
      else
      {
        String_appendCString(endTime,"*");
      }
      nextScheduleDateTime = getNextJobSchedule(jobScheduleNode, now);

      // send schedule info
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "jobName=%'S jobUUID=%S scheduleUUID=%S archiveType=%s date=%S weekDays=%S time=%S interval=%u customText=%'S beginTime=%S endTime=%S testCreatedArchives=%y noStorage=%y enabled=%y lastExecutedDateTime=%"PRIu64" nextExecutedDateTime=%"PRIu64" totalEntities=%lu totalStorageCount=%lu totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                          jobScheduleNode->jobName,
                          jobScheduleNode->jobUUID,
                          jobScheduleNode->scheduleUUID,
                          (jobScheduleNode->archiveType != ARCHIVE_TYPE_UNKNOWN) ? Archive_archiveTypeToString(jobScheduleNode->archiveType) : "*",
                          date,
                          weekDays,
                          time,
                          jobScheduleNode->interval,
                          jobScheduleNode->customText,
                          beginTime,
                          endTime,
                          jobScheduleNode->testCreatedArchives,
                          jobScheduleNode->noStorage,
                          jobScheduleNode->enabled,
                          jobScheduleNode->lastExecutedDateTime,
                          (nextScheduleDateTime < MAX_UINT64) ? nextScheduleDateTime : 0LL,
// TODO: remove
0LL,//                          jobScheduleNode->totalEntityCount,
0LL,//                          jobScheduleNode->totalStorageCount,
0LL,//                          jobScheduleNode->totalEntryCount,
0LL//                          jobScheduleNode->totalEntrySize
                         );
    }
  }
  String_delete(endTime);
  String_delete(beginTime);
  String_delete(time);
  String_delete(weekDays);
  String_delete(date);
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  List_done(&jobScheduleList);
}

/***********************************************************************\
* Name   : serverCommand_persistenceListClear
* Purpose: clear job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear schedule list
    List_clear(&jobNode->job.options.scheduleList);

    // notify modified schedule list
    Job_setScheduleModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&schedulerThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : serverCommand_scheduleListAdd
* Purpose: add entry to job schedule list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            [scheduleUUID=<uuid>]
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            date=<year>|*-<month>|*-<day>|*
*            weekDays=<week day>,...|*
*            time=<hour>|*:<minute>|*
*            interval=<n>
*            customText=<text>
*            beginTime=<hour>|*:<minute>|*
*            endTime=<hour>|*:<minute>|*
*            testCreatedArchives=yes|no
*            noStorage=yes|no
*            enabled=yes|no
*          Result:
*            scheduleUUID=<uuid>
\***********************************************************************/

LOCAL void serverCommand_scheduleListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       title;
  String       date;
  String       weekDays;
  String       time;
  ArchiveTypes archiveType;
  uint         interval;
  String       customText;
  String       beginTime,endTime;
  bool         testCreatedArchives;
  bool         noStorage;
  bool         enabled;
  ScheduleNode *scheduleNode;
  Errors       error;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, date, weekday, time, archive type, custome text, min./max keep, max. age, enabled
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL);
  title = String_new();
  StringMap_getString(argumentMap,"title",title,NULL);
  date = String_new();
  if (!StringMap_getString(argumentMap,"date",date,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"date=<date>|*");
    String_delete(date);
    String_delete(title);
    return;
  }
  weekDays = String_new();
  if (!StringMap_getString(argumentMap,"weekDays",weekDays,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"weekDays=<names>|*");
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  time = String_new();
  if (!StringMap_getString(argumentMap,"time",time,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"time=<time>|*");
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"archiveType",NULL),"*"))
  {
    archiveType = ARCHIVE_TYPE_NORMAL;
  }
  else if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  StringMap_getUInt(argumentMap,"interval",&interval,0);
  customText = String_new();
  StringMap_getString(argumentMap,"customText",customText,NULL);
  beginTime = String_new();
  if (!StringMap_getString(argumentMap,"beginTime",beginTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"beginTime=<time>|*");
    String_delete(beginTime);
    String_delete(time);
    String_delete(customText);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  endTime = String_new();
  if (!StringMap_getString(argumentMap,"endTime",endTime,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"endTime=<time>|*");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getBool(argumentMap,"testCreatedArchives",&testCreatedArchives,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"testCreatedArchives=yes|no");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getBool(argumentMap,"noStorage",&noStorage,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"noStorage=yes|no");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getBool(argumentMap,"enabled",&enabled,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"enabled=yes|no");
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }

  // create new schedule
  scheduleNode = Job_newScheduleNode(scheduleUUID);
  assert(scheduleNode != NULL);

  // parse schedule
  error = parseScheduleDateTime(&scheduleNode->date,
                                &scheduleNode->weekDaySet,
                                &scheduleNode->time,
                                date,
                                weekDays,
                                time
                               );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,
                        id,
                        TRUE,
                        ERROR_PARSE_SCHEDULE,
                        "%S %S %S",
                        date,
                        weekDays,
                        time
                       );
    Job_deleteScheduleNode(scheduleNode);
    String_delete(endTime);
    String_delete(beginTime);
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
  error = parseScheduleTime(&scheduleNode->beginTime,
                            beginTime
                           );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,
                        id,
                        TRUE,
                        ERROR_PARSE_SCHEDULE,
                        "%S",
                        beginTime
                       );
    Job_deleteScheduleNode(scheduleNode);
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  error = parseScheduleTime(&scheduleNode->endTime,
                            endTime
                           );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,
                        id,
                        TRUE,
                        ERROR_PARSE_SCHEDULE,
                        "%S",
                        endTime
                       );
    Job_deleteScheduleNode(scheduleNode);
    String_delete(endTime);
    String_delete(beginTime);
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  scheduleNode->testCreatedArchives = testCreatedArchives;
  scheduleNode->noStorage           = noStorage;
  scheduleNode->enabled             = enabled;
  scheduleNode->minKeep             = 0;
  scheduleNode->maxKeep             = 0;
  scheduleNode->maxAge              = 0;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      Job_deleteScheduleNode(scheduleNode);
      String_delete(endTime);
      String_delete(beginTime);
      String_delete(customText);
      String_delete(time);
      String_delete(weekDays);
      String_delete(date);
      String_delete(title);
      return;
    }

    // add to schedule list
    List_append(&jobNode->job.options.scheduleList,scheduleNode);

    // notify modified schedule list
    Job_setScheduleModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"scheduleUUID=%S",scheduleNode->uuid);

  Semaphore_signalModified(&schedulerThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // free resources
  String_delete(endTime);
  String_delete(beginTime);
  String_delete(customText);
  String_delete(time);
  String_delete(weekDays);
  String_delete(date);
  String_delete(title);
}

/***********************************************************************\
* Name   : serverCommand_scheduleListRemove
* Purpose: remove entry from job schedule list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;
  ScheduleNode *scheduleNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // find schedule
    scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_SCHEDULE_NOT_FOUND,"%S",scheduleUUID);
      Job_listUnlock();
      return;
    }

    // remove from list
    List_removeAndFree(&jobNode->job.options.scheduleList,scheduleNode);

    // notify modified schedule list
    Job_setScheduleModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&schedulerThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionGet
* Purpose: get schedule options
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  const JobNode      *jobNode;
  const ScheduleNode *scheduleNode;
  uint               i;
  String             s;
  ConfigValueFormat  configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // find schedule
    scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_SCHEDULE_NOT_FOUND,"%S",scheduleUUID);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // find config value
#ifndef WERROR
#warning todo
#endif
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,"schedule",String_cString(name));
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"XXXunknown schedule config '%S'",name);
      Job_listUnlock();
      String_delete(name);
      return;
    }
    assert(BAR_CONFIG_VALUES[i].type != CONFIG_VALUE_TYPE_DEPRECATED);

    if (BAR_CONFIG_VALUES[i].type != CONFIG_VALUE_TYPE_DEPRECATED)
    {
      // send value
      s = String_new();
      ConfigValue_formatInit(&configValueFormat,
                             &JOB_CONFIG_VALUES[i],
                             CONFIG_VALUE_FORMAT_MODE_VALUE,
                             jobNode
                            );
      ConfigValue_format(&configValueFormat,s);
      ConfigValue_formatDone(&configValueFormat);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%S",s);
      String_delete(s);
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DEPRECATED_OR_IGNORED_VALUE,"%S",name);
    }
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionSet
* Purpose: set schedule option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            name=<name>
*            value=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleOptionSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       name,value;
  JobNode      *jobNode;
  ScheduleNode *scheduleNode;
  uint         i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name, value
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"value=<value>");
    String_delete(value);
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    // find schedule
    scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_SCHEDULE_NOT_FOUND,"%S",scheduleUUID);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    // parse
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,"schedule",String_cString(name));
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config '%S'",name);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    if (ConfigValue_parse(JOB_CONFIG_VALUES,
                          &JOB_CONFIG_VALUES[i],
                          "schedule",
                          String_cString(value),
                          CALLBACK_(NULL,NULL),  // errorFunction
                          CALLBACK_(NULL,NULL),  // warningFunction
                          scheduleNode,
                          NULL // commentLineList
                         )
       )
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_VALUE,"invalid schedule config '%S'",name);
      Job_listUnlock();
      String_delete(value);
      String_delete(name);
      return;
    }

    // notify modified schedule list
    Job_setScheduleModified(jobNode);
  }

  Semaphore_signalModified(&schedulerThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionDelete
* Purpose: delete schedule option
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       name;
  JobNode      *jobNode;
  ScheduleNode *scheduleNode;
  uint         i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // find schedule
    scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_SCHEDULE_NOT_FOUND,"%S",scheduleUUID);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // find config value
#ifndef WERROR
#warning todo
#endif
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,"schedule",String_cString(name));
    if (i == CONFIG_VALUE_INDEX_NONE)
    {
// TODO:
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config '%S'",name);
      Job_listUnlock();
      String_delete(name);
      return;
    }

    // delete value
#ifndef WERROR
#warning todo?
#endif
//    ConfigValue_reset(&JOB_CONFIG_VALUES[z],jobNode);

    // notify modified schedule list
    Job_setScheduleModified(jobNode);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleTrigger
* Purpose: trigger job schedule
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleTrigger(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;
  ScheduleNode *scheduleNode;
  uint64       executeScheduleDateTime;
  uint64       dateTime;
  uint         year,month,day,hour,minute;
  WeekDays     weekDay;
  char         s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // find schedule
    scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_SCHEDULE_NOT_FOUND,"%S",scheduleUUID);
      Job_listUnlock();
      return;
    }

    // get matching time for schedule
    executeScheduleDateTime = 0LL;
    dateTime                = (Misc_getCurrentDateTime()/S_PER_MINUTE)*S_PER_MINUTE;  // round to full minutes
    while (   (executeScheduleDateTime == 0LL)
           && (dateTime >= 60LL)
          )
    {
      // get date/time values
      Misc_splitDateTime(dateTime,
                         TIME_TYPE_LOCAL,
                         &year,
                         &month,
                         &day,
                         &hour,
                         &minute,
                         NULL,  // second
                         &weekDay,
                         NULL  // isDayLightSaving
                        );

      // check if date/time is matching with schedule node
      if (   ((scheduleNode->date.year   == DATE_ANY       ) || (scheduleNode->date.year   == (int)year  ))
          && ((scheduleNode->date.month  == DATE_ANY       ) || (scheduleNode->date.month  == (int)month ))
          && ((scheduleNode->date.day    == DATE_ANY       ) || (scheduleNode->date.day    == (int)day   ))
          && ((scheduleNode->weekDaySet  == WEEKDAY_SET_ANY) || IN_SET(scheduleNode->weekDaySet,weekDay)  )
          && ((scheduleNode->time.hour   == TIME_ANY       ) || (scheduleNode->time.hour   == (int)hour  ))
          && ((scheduleNode->time.minute == TIME_ANY       ) || (scheduleNode->time.minute == (int)minute))
         )
      {
        executeScheduleDateTime = dateTime;
      }

      // next time
      dateTime -= 60LL;
    }
    if (executeScheduleDateTime == 0LL)
    {
      executeScheduleDateTime = (Misc_getCurrentDateTime()/S_PER_MINUTE)*S_PER_MINUTE;  // round to full minutes
    }

    // trigger job
    if (!Job_isActive(jobNode->jobState))
    {
      Job_trigger(jobNode,
                  scheduleNode->uuid,
                  scheduleNode->archiveType,
                  scheduleNode->customText,
                  scheduleNode->testCreatedArchives,
                  scheduleNode->noStorage,
                  FALSE,  // dryRun
                  executeScheduleDateTime,
                  getClientInfoString(clientInfo,s,sizeof(s))
                 );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&schedulerThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : serverCommand_persistenceList
* Purpose: get job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            persistenceId=<n> \
*            archiveType=<type> \
*            minKeep=<n>|* \
*            maxKeep=<n>|* \
*            maxAge=<n>|* \
*            moveTo=<uri> #
*            entityId=<n> \
*            createdDateTime=<time stamp [s]> \
*            size=<n> \
*            totalEntrySize=<n> \
*            totalEntryCount=<n> \
*            inTransit=yes|no
*            ...
\***********************************************************************/

LOCAL void serverCommand_persistenceList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode         *jobNode;
  const PersistenceNode *persistenceNode;
  char                  s1[16],s2[16],s3[16];
  EntityList            entityList;
  EntityList            jobEntityList;
  const EntityNode      *jobEntityNode;
  bool                  inTransit;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get job UUID, archive type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  // get expiration entity list
  getEntityList(&entityList,
                indexHandle
               );

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // get expiration entity list of job
    getJobEntityList(&jobEntityList,
                     &entityList,
                     jobNode->job.uuid,
                     &jobNode->job.options.persistenceList
                    );

//TODO: totalStorageCount, totalStorageSize
    LIST_ITERATE(&jobNode->job.options.persistenceList,persistenceNode)
    {
      // send persistence info
      if (persistenceNode->minKeep != KEEP_ALL   ) stringFormat(s1,sizeof(s1),"%d",persistenceNode->minKeep); else stringSet(s1,sizeof(s1),"*");
      if (persistenceNode->maxKeep != KEEP_ALL   ) stringFormat(s2,sizeof(s2),"%d",persistenceNode->maxKeep); else stringSet(s2,sizeof(s2),"*");
      if (persistenceNode->maxAge  != AGE_FOREVER) stringFormat(s3,sizeof(s3),"%d",persistenceNode->maxAge ); else stringSet(s3,sizeof(s3),"*");
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "persistenceId=%u archiveType=%s minKeep=%s maxKeep=%s maxAge=%s moveTo=%'S size=%"PRIu64"",
                          persistenceNode->id,
                          ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,persistenceNode->archiveType,NULL),
                          s1,
                          s2,
                          s3,
                          persistenceNode->moveTo,
//TODO
0LL//                            persistenceNode->totalEntitySize
                         );
      LIST_ITERATE(&jobEntityList,jobEntityNode)
      {
        if (jobEntityNode->persistenceNode == persistenceNode)
        {
          inTransit = isInTransit(jobEntityNode);

          // send entity info
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "persistenceId=%u entityId=%"PRIindexId" createdDateTime=%"PRIu64" size=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" inTransit=%y",
                              jobEntityNode->persistenceNode->id,
                              jobEntityNode->entityId,
                              jobEntityNode->createdDateTime,
                              jobEntityNode->size,
                              jobEntityNode->totalEntryCount,
                              jobEntityNode->totalEntrySize,
                              inTransit
                             );
        }
      }
    }

    // free resources
    List_done(&jobEntityList);
  }

  // free resources
  List_done(&entityList);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_persistenceListClear
* Purpose: clear job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_persistenceListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // clear persistence list
    List_clear(&jobNode->job.options.persistenceList);
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify modified persistence list
    Job_setPersistenceModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&persistenceThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : serverCommand_persistenceListAdd
* Purpose: add entry to job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            archiveType=<type>
*            minKeep=<n>|*
*            maxKeep=<n>|*
*            maxAge=<n>|*
*            moveTo=<uri>
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_persistenceListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString    (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes    archiveType;
  int             minKeep,maxKeep;
  int             maxAge;
  String          moveTo;
  JobNode         *jobNode;
  PersistenceNode *persistenceNode;
  uint            persistenceId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, archive type, min./max keep, max. age
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"minKeep",NULL),"*"))
  {
    minKeep = KEEP_ALL;
  }
  else if (!StringMap_getInt(argumentMap,"minKeep",&minKeep,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"minKeep=<n>|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"maxKeep",NULL),"*"))
  {
    maxKeep = KEEP_ALL;
  }
  else if (!StringMap_getInt(argumentMap,"maxKeep",&maxKeep,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxKeep=<n>|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"maxAge",NULL),"*"))
  {
    maxAge = AGE_FOREVER;
  }
  else if (!StringMap_getInt(argumentMap,"maxAge",&maxAge,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxAge=<n>|*");
    return;
  }
  moveTo = String_new();
  StringMap_getString(argumentMap,"moveTo",moveTo,"");

  persistenceId = ID_NONE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(moveTo);
      return;
    }

    if (!LIST_CONTAINS(&jobNode->job.options.persistenceList,
                       persistenceNode,
                          (persistenceNode->archiveType == archiveType)
                       && (persistenceNode->minKeep     == minKeep    )
                       && (persistenceNode->maxKeep     == maxKeep    )
                       && (persistenceNode->maxAge      == maxAge     )
                      )
       )
    {
      // insert into persistence list
      persistenceNode = Job_newPersistenceNode(archiveType,minKeep,maxKeep,maxAge,moveTo);
      assert(persistenceNode != NULL);
      Job_insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);

      // set last-modified timestamp
      jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

      // get id
      persistenceId = persistenceNode->id;

      // notify modified persistence list
      Job_setPersistenceModified(jobNode);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",persistenceId);

  Semaphore_signalModified(&persistenceThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // free resources
  String_delete(moveTo);
}

/***********************************************************************\
* Name   : serverCommand_persistenceListUpdate
* Purpose: update entry to job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            archiveType=<type>
*            minKeep=<n>|*
*            maxKeep=<n>|*
*            maxAge=<n>|*
*            moveTo=<uri>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_persistenceListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  uint                  persistenceId;
  ArchiveTypes          archiveType;
  int                   minKeep,maxKeep;
  int                   maxAge;
  String                moveTo;
  JobNode               *jobNode;
  PersistenceNode       *persistenceNode;
  const PersistenceNode *existingPersistenceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, persistence id, archive type, min./max keep, max. age
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&persistenceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"minKeep",NULL),"*"))
  {
    minKeep = KEEP_ALL;
  }
  else if (!StringMap_getInt(argumentMap,"minKeep",&minKeep,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"minKeep=<n>|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"maxKeep",NULL),"*"))
  {
    maxKeep = KEEP_ALL;
  }
  else if (!StringMap_getInt(argumentMap,"maxKeep",&maxKeep,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxKeep=<n>|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"maxAge",NULL),"*"))
  {
    maxAge = AGE_FOREVER;
  }
  else if (!StringMap_getInt(argumentMap,"maxAge",&maxAge,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxAge=<n>|*");
    return;
  }
  moveTo = String_new();
  StringMap_getString(argumentMap,"moveTo",moveTo,"");

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      String_delete(moveTo);
      return;
    }

    // find persistence node
    persistenceNode = LIST_FIND(&jobNode->job.options.persistenceList,persistenceNode,persistenceNode->id == persistenceId);
    if (persistenceNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PERSISTENCE_ID_NOT_FOUND,"%u",persistenceId);
      Job_listUnlock();
      String_delete(moveTo);
      return;
    }

    // remove from persistence list
    List_remove(&jobNode->job.options.persistenceList,persistenceNode);

    // update persistence
    persistenceNode->archiveType = archiveType;
    persistenceNode->minKeep     = minKeep;
    persistenceNode->maxKeep     = maxKeep;
    persistenceNode->maxAge      = maxAge;
    if (persistenceNode->moveTo != NULL)
    {
      String_set(persistenceNode->moveTo,moveTo);
    }
    else
    {
      persistenceNode->moveTo = String_duplicate(moveTo);
    }

    if (!LIST_CONTAINS(&jobNode->job.options.persistenceList,
                       existingPersistenceNode,
                          (existingPersistenceNode->archiveType == persistenceNode->archiveType)
                       && (existingPersistenceNode->minKeep     == persistenceNode->minKeep    )
                       && (existingPersistenceNode->maxKeep     == persistenceNode->maxKeep    )
                       && (existingPersistenceNode->maxAge      == persistenceNode->maxAge     )
                       && String_equals(existingPersistenceNode->moveTo,persistenceNode->moveTo)
                      )
       )
    {
      // re-insert updated node into persistence list
      Job_insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
    }
    else
    {
      // duplicate -> discard
      Job_deletePersistenceNode(persistenceNode);
    }

//TODO: remove
    // update "forever"-nodes
//    insertForeverPersistenceNodes(&jobNode->job.options.persistenceList);

    // set last-modified timestamp
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify modified persistence list
    Job_setPersistenceModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&persistenceThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // free resources
  String_delete(moveTo);
}

/***********************************************************************\
* Name   : serverCommand_persistenceListRemove
* Purpose: remove entry from job persistence list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_persistenceListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString    (jobUUID,MISC_UUID_STRING_LENGTH);
  uint            persistenceId;
  JobNode         *jobNode;
  PersistenceNode *persistenceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, persistence id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&persistenceId,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"id=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // find persistence
    persistenceNode = LIST_FIND(&jobNode->job.options.persistenceList,persistenceNode,persistenceNode->id == persistenceId);
    if (persistenceNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PERSISTENCE_ID_NOT_FOUND,"%u",persistenceId);
      Job_listUnlock();
      return;
    }

    // remove from list
    List_removeAndFree(&jobNode->job.options.persistenceList,persistenceNode);

//TODO: remove
    // update "forever"-nodes
//    insertForeverPersistenceNodes(&jobNode->job.options.persistenceList);

    // set last-modified timestamp
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify modified persistence list
    Job_setPersistenceModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  Semaphore_signalModified(&persistenceThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordAdd
* Purpose: add password to internal list of decrypt passwords
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_decryptPasswordAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  ServerIOEncryptTypes encryptType;
  String               encryptedPassword;
  Errors               error;
  Password             password;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get encrypt type, encrypted password
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    return;
  }

  // decrypt password and add to list
  Password_init(&password);
  error = ServerIO_decryptPassword(&clientInfo->io,
                                   &password,
                                   encryptType,
                                   encryptedPassword
                                  );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
    Password_done(&password);
    String_delete(encryptedPassword);
    return;
  }

  // add to list
  Archive_appendDecryptPassword(&password);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  Password_done(&password);
  String_delete(encryptedPassword);
}

/***********************************************************************\
* Name   : serverCommand_ftpPassword
* Purpose: set job FTP password
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_ftpPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  ServerIOEncryptTypes encryptType;
  String               encryptedPassword;
  Errors               error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    return;
  }

  // decrypt password
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    error = ServerIO_decryptPassword(&clientInfo->io,
                                     &clientInfo->jobOptions.ftpServer.password,
                                     encryptType,
                                     encryptedPassword
                                    );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&clientInfo->lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_FTP_PASSWORD,"");
      String_delete(encryptedPassword);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
}

/***********************************************************************\
* Name   : serverCommand_sshPassword
* Purpose: set job SSH password
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sshPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  ServerIOEncryptTypes encryptType;
  String               encryptedPassword;
  Errors               error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    return;
  }

  // decrypt password
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    error = ServerIO_decryptPassword(&clientInfo->io,
                                     &clientInfo->jobOptions.sshServer.password,
                                     encryptType,
                                     encryptedPassword
                                    );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&clientInfo->lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_SSH_PASSWORD,"");
      String_delete(encryptedPassword);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
}

/***********************************************************************\
* Name   : serverCommand_webdavPassword
* Purpose: set job Webdav password
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_webdavPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  ServerIOEncryptTypes encryptType;
  String               encryptedPassword;
  Errors               error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    return;
  }

  // decrypt password
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    error = ServerIO_decryptPassword(&clientInfo->io,
                                     &clientInfo->jobOptions.webDAVServer.password,
                                     encryptType,
                                     encryptedPassword
                                    );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&clientInfo->lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_WEBDAV_PASSWORD,"");
      String_delete(encryptedPassword);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
}

/***********************************************************************\
* Name   : serverCommand_cryptPassword
* Purpose: set job encryption password
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString         (jobUUID,MISC_UUID_STRING_LENGTH);
  ServerIOEncryptTypes encryptType;
  String               encryptedPassword;
  JobNode              *jobNode;
  Errors               error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, get encrypt type, encrypted password
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptType=NONE|RSA");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    return;
  }

  if (!String_equalsCString(jobUUID,"*"))
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // find job
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode == NULL)
      {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
        Job_listUnlock();
        String_delete(encryptedPassword);
        return;
      }

      // decrypt password
//TODO: remove
//      if (jobNode->job.options.cryptPassword == NULL) jobNode->job.options.cryptPassword = Password_new();
      error = ServerIO_decryptPassword(&clientInfo->io,
                                       &jobNode->job.options.cryptPassword,
                                       encryptType,
                                       encryptedPassword
                                      );
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
        Job_listUnlock();
        String_delete(encryptedPassword);
        return;
      }
    }
  }
  else
  {
    // decrypt password
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      error = ServerIO_decryptPassword(&clientInfo->io,
                                       &clientInfo->jobOptions.cryptPassword,
                                       encryptType,
                                       encryptedPassword
                                      );
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&clientInfo->lock);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
        String_delete(encryptedPassword);
        return;
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
}

/***********************************************************************\
* Name   : serverCommand_passwordsClear
* Purpose: clear ssh/ftp/crypt/decrypt passwords stored in memory
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

  // clear ftp/ssh/crypt passwords
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Password_clear(&clientInfo->jobOptions.ftpServer.password);
    Password_clear(&clientInfo->jobOptions.sshServer.password);
    Password_clear(&clientInfo->jobOptions.cryptPassword);
  }

  // clear decrypt passwords
  Archive_clearDecryptPasswords();

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeLoad
* Purpose: load volume
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            volumeNnumber=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeLoad(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint         volumeNumber;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, volume number
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"volumeNumber",&volumeNumber,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"volumeNumber=<n>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // set volume number
    jobNode->volumeNumber = volumeNumber;
    Job_listSignalModifed();
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeUnload
* Purpose: unload volumne
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeUnload(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      return;
    }

    // set unload flag
    jobNode->volumeUnloadFlag = TRUE;
    Job_listSignalModifed();
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_archiveList
* Purpose: list content of archive
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
*            deltaSourceSize=<n [bytes]> fileSystemType=<name> blockSize=<n [bytes]> fragmentOffset=<n> fragmentSize=<n>
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
  StorageInfo       storageInfo;
  Errors            error;
  ArchiveHandle     archiveHandle;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get storage name, pattern
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"name",storageName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<storage name>");
    String_delete(storageName);
    return;
  }

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"%S",storageName);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // init storage
  error = Storage_init(&storageInfo,
                       NULL,  // masterIO
                       &storageSpecifier,
                       &clientInfo->jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(NULL,NULL),  // storageUpdateProgress
                       CALLBACK_(NULL,NULL),  // getNamePassword
                       CALLBACK_(NULL,NULL),  // requestVolume
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"%S",storageName);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // open archive
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       NULL,  // archive name
                       NULL,  // deltaSourceList
                       ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS,
                       CALLBACK_(NULL,NULL),
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"%S",storageName);
    (void)Storage_done(&storageInfo);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // list contents
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveHandle)
         && (error == ERROR_NONE)
         && !isCommandAborted(clientInfo,id)
        )
  {
    // get next file type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo
                                        NULL,  // offset
                                        NULL  // size
                                       );
    if (error == ERROR_NONE)
    {
      // read entry
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_NONE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
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
                                          &archiveHandle,
                                          &deltaCompressAlgorithm,
                                          &byteCompressAlgorithm,
                                          &cryptType,
                                          &cryptAlgorithm,
                                          NULL,  // cryptSalt
                                          NULL,  // cryptKey
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
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(deltaSourceName);
              String_delete(fileName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                  "fileType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" archiveSize=%"PRIu64" deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%"PRIu64" fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"",
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
            uint64             fragmentOffset,fragmentSize;

            // open archive file
            imageName       = String_new();
            deltaSourceName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveHandle,
                                           &deltaCompressAlgorithm,
                                           &byteCompressAlgorithm,
                                           &cryptType,
                                           &cryptAlgorithm,
                                           NULL,  // cryptSalt
                                           NULL,  // cryptKey
                                           imageName,
                                           &deviceInfo,
                                           &fileSystemType,
                                           deltaSourceName,
                                           &deltaSourceSize,
                                           &fragmentOffset,
                                           &fragmentSize
                                          );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(deltaSourceName);
              String_delete(imageName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                  "fileType=IMAGE name=%'S size=%"PRIu64" archiveSize=%"PRIu64" deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%"PRIu64" fileSystemType=%s blockSize=%u fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"",
                                  imageName,
                                  deviceInfo.size,
                                  archiveEntryInfo.image.chunkImageData.info.size,
                                  deltaCompressAlgorithm,
                                  byteCompressAlgorithm,
                                  cryptAlgorithm,
                                  cryptType,
                                  deltaSourceName,
                                  deltaSourceSize,
                                  FileSystem_fileSystemTypeToString(fileSystemType,NULL),
                                  deviceInfo.blockSize,
                                  fragmentOffset,
                                  fragmentSize
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
                                               &archiveHandle,
                                               &cryptType,
                                               &cryptAlgorithm,
                                               NULL,  // cryptSalt
                                               NULL,  // cryptKey
                                               directoryName,
                                               &fileInfo,
                                               NULL   // fileExtendedAttributeList
                                              );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(directoryName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                  "fileType=DIRECTORY name=%'S dateTime=%"PRIu64" cryptAlgorithm=%d cryptType=%d",
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
                                          &archiveHandle,
                                          &cryptType,
                                          &cryptAlgorithm,
                                          NULL,  // cryptSalt
                                          NULL,  // cryptKey
                                          linkName,
                                          name,
                                          NULL,  // fileInfo
                                          NULL   // fileExtendedAttributeList
                                         );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(name);
              String_delete(linkName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
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
                                              &archiveHandle,
                                              &deltaCompressAlgorithm,
                                              &byteCompressAlgorithm,
                                              &cryptType,
                                              &cryptAlgorithm,
                                              NULL,  // cryptSalt
                                              NULL,  // cryptKey
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
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(deltaSourceName);
              StringList_done(&fileNameList);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_matchStringList(&clientInfo->includeEntryList,&fileNameList,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_matchStringList(&clientInfo->excludePatternList,&fileNameList,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                  "fileType=HARDLINK name=%'S size=%"PRIu64" dateTime=%"PRIu64" archiveSize=%"PRIu64" deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%"PRIu64" fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"",
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
                                             &archiveHandle,
                                             &cryptType,
                                             &cryptAlgorithm,
                                             NULL,  // cryptSalt
                                             NULL,  // cryptKey
                                             name,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
              String_delete(name);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,name,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,name,PATTERN_MATCH_MODE_EXACT)
               )
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
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
        case ARCHIVE_ENTRY_TYPE_META:
        case ARCHIVE_ENTRY_TYPE_SALT:
        case ARCHIVE_ENTRY_TYPE_KEY:
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
          error = Archive_skipNextEntry(&archiveHandle);
          if (error != ERROR_NONE)
          {
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read content of storage '%S'",storageName);
            break;
          }
          break;
        case ARCHIVE_ENTRY_TYPE_UNKNOWN:
          (void)Archive_skipNextEntry(&archiveHandle);
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read next entry of storage '%S'",storageName);
      break;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // close archive
  Archive_close(&archiveHandle,FALSE);

  // done storage
  (void)Storage_done(&storageInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : serverCommand_entityMoveTo
* Purpose: move storages of entity to another path
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>
*            moveTo=<path>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_entityMoveTo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          entityId;
  String           moveTo;
  Errors           error;
  JobOptions       jobOptions;
  StorageSpecifier storageSpecifier;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entityId, moveTo
  if (!StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<id>");
    return;
  }
  moveTo = String_new();
  if (!StringMap_getString(argumentMap,"moveTo",moveTo,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"moveTo=<path>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    String_delete(moveTo);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // init variables
  Job_initOptions(&jobOptions);
  Storage_initSpecifier(&storageSpecifier);
// TODO: init specifier, support other types
  storageSpecifier.type = STORAGE_TYPE_FILESYSTEM;

  error = ERROR_NONE;

  // lock entity
  error = Index_lockEntity(indexHandle,entityId);
  if (error == ERROR_NONE)
  {
    // move storages of entity to new path
    error = moveEntity(indexHandle,
                       &jobOptions,
                       entityId,
                       &storageSpecifier,
                       moveTo,
                       CALLBACK_INLINE(void,
                                       (IndexId     storageId,
                                        ConstString storageName,
                                        uint64      n,
                                        uint64      size,
                                        uint        doneCount,
                                        uint64      doneSize,
                                        uint        totalCount,
                                        uint64      totalSize,
                                        void        *userData
                                       ),
                       {
                         UNUSED_VARIABLE(userData);

                         ServerIO_sendResult(&clientInfo->io,
                                             id,
                                             FALSE,
                                             ERROR_NONE,
                                             "storageId=%"PRIi64" name=%S n=%"PRIu64" size=%"PRIu64" doneCount=%u doneSize=%"PRIu64" totalCount=%u totalSize=%"PRIu64,
                                             storageId,
                                             storageName,
                                             n,
                                             size,
                                             doneCount,
                                             doneSize,
                                             totalCount,
                                             totalSize
                                            );
                        },NULL),
                        CALLBACK_INLINE(bool,(void *userData),
                        {
                          UNUSED_VARIABLE(userData);

                          return isCommandAborted(clientInfo,id);
                        },NULL)
                       );

    // unlock entity
    (void)Index_unlockEntity(indexHandle,entityId);
  }
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    Job_doneOptions(&jobOptions);
    String_delete(moveTo);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"%s",Error_getText(error));
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  Job_doneOptions(&jobOptions);
  String_delete(moveTo);
}

/***********************************************************************\
* Name   : serverCommand_storageTest
* Purpose: test storage
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|"" and/or
*            entityId=<id>|0 and/or
*            storageId=<id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageTest(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  typedef struct
  {
    ClientInfo *clientInfo;
    uint       id;
    bool       abortFlag;
  } TestCommandInfo;

  /***********************************************************************\
  * Name   : testRunningInfo
  * Purpose: update test running info
  * Input  : runningInfo - running info data,
  *          userData    - user data
  * Output : -
  * Return : TRUE to continue, FALSE to abort
  * Notes  : -
  \***********************************************************************/

  auto void testRunningInfo(const RunningInfo *runningInfo,
                            void              *userData
                           );
  void testRunningInfo(const RunningInfo *runningInfo,
                       void              *userData
                      )
  {
    TestCommandInfo *testCommandInfo = (TestCommandInfo*)userData;

    assert(testCommandInfo != NULL);
    assert(runningInfo != NULL);
    assert(runningInfo->progress.storage.name != NULL);
    assert(runningInfo->progress.entry.name != NULL);

    ServerIO_sendResult(&testCommandInfo->clientInfo->io,
                        testCommandInfo->id,
                        FALSE,
                        ERROR_NONE,
                        "doneCount=%lu doneSize=%"PRIu64" totalCount=%lu totalSize=%"PRIu64" entryName=%'S entryDoneSize=%"PRIu64" entryTotalSize=%"PRIu64" storageName=%'S storageDoneSize=%"PRIu64" storageTotalSize=%"PRIu64"",
                        runningInfo->progress.done.count,
                        runningInfo->progress.done.size,
                        runningInfo->progress.total.count,
                        runningInfo->progress.total.size,
                        runningInfo->progress.entry.name,
                        runningInfo->progress.entry.doneSize,
                        runningInfo->progress.entry.totalSize,
                        runningInfo->progress.storage.name,
                        runningInfo->progress.storage.doneSize,
                        runningInfo->progress.storage.totalSize
                       );
  }

  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  IndexId      entityId;
  IndexId      storageId;
  Errors       error;
  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get job uuid, entity id, and/or storage id
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getIndexId(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // mount devices
  error = ERROR_NONE;
  String_clear(uuid);
  if      (!String_isEmpty(jobUUID))
  {
    String_set(uuid,jobUUID);
  }
  else if (!INDEX_ID_IS_NONE(entityId))
  {
    if (!Index_findEntity(indexHandle,
                          entityId,
                          NULL,  // findJobUUID
                          NULL,  // findScheduleUUID
                          NULL,  // findHostName
                          ARCHIVE_TYPE_ANY,
                          0LL,  // findCreatedDate
                          0L,  // findCreatedTime
                          uuid,
                          NULL,  // scheduleUUID
                          NULL,  // uuidId
                          NULL,  // entityId
                          NULL,  // archiveType
                          NULL,  // createdDateTime
                          NULL,  // lastErrorMessage
                          NULL,  // totalEntryCount
                          NULL  // totalEntrySize
                         )
       )
    {
      error = ERROR_DATABASE_ENTRY_NOT_FOUND;
    }
  }
  else if (!INDEX_ID_IS_NONE(storageId))
  {
    if (!Index_findStorageById(indexHandle,
                               storageId,
                               uuid,
                               NULL,  // scheduleUUID
                               NULL,  // uuidId
                               NULL,  // entityId
                               NULL,  // storageName
                               NULL,  // createdDateTime
                               NULL,  // size
                               NULL,  // indexState
                               NULL,  // indexMode
                               NULL,  // lastCheckedDateTime
                               NULL,  // errorMessage
                               NULL,  // totalEntryCount
                               NULL  // totalEntrySize
                              )
      )
    {
      error = ERROR_DATABASE_ENTRY_NOT_FOUND;
    }
  }
  if (error == ERROR_NONE)
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = Job_findByUUID(uuid);
      if (jobNode != NULL)
      {
        error = mountAll(&jobNode->job.options.mountList);
      }
    }
  }
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

  // test storages
  TestCommandInfo testCommandInfo;
  testCommandInfo.clientInfo = clientInfo;
  testCommandInfo.id         = id;
  if (!String_isEmpty(jobUUID))
  {
    // test all storage files with job UUID
    error = testUUID(indexHandle,
                     jobUUID,
                     CALLBACK_(testRunningInfo,&testCommandInfo),
                     CALLBACK_INLINE(bool,(void *userData),
                     {
                       UNUSED_VARIABLE(userData);
                       return isCommandAborted(clientInfo,id);
                     },NULL)
                    );
  }

  if (!INDEX_ID_IS_NONE(entityId))
  {
    // test all storage files of entity
    error = testEntity(indexHandle,
                       entityId,
                       CALLBACK_(testRunningInfo,&testCommandInfo),
                       CALLBACK_INLINE(bool,(void *userData),
                       {
                         UNUSED_VARIABLE(userData);
                         return isCommandAborted(clientInfo,id);
                       },NULL)
                      );
  }

  if (!INDEX_ID_IS_NONE(storageId))
  {
    // test storage file
    error = testStorage(indexHandle,
                        storageId,
                        CALLBACK_(testRunningInfo,&testCommandInfo),
                        CALLBACK_INLINE(bool,(void *userData),
                        {
                          UNUSED_VARIABLE(userData);
                          return isCommandAborted(clientInfo,id);
                        },NULL)
                       );
  }
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

  // unmount devices
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    jobNode = Job_findByUUID(uuid);
    if (jobNode != NULL)
    {
      error = unmountAll(&jobNode->job.options.mountList);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageDelete
* Purpose: delete storage and remove database index
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get job uuid, entity id, and/or storage id
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getIndexId(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // mount devices
  error = ERROR_NONE;
  String_clear(uuid);
  if      (!String_isEmpty(jobUUID))
  {
    String_set(uuid,jobUUID);
  }
  else if (!INDEX_ID_IS_NONE(entityId))
  {
    if (!Index_findEntity(indexHandle,
                          entityId,
                          NULL,  // findJobUUID
                          NULL,  // findScheduleUUID
                          NULL,  // findHostName
                          ARCHIVE_TYPE_ANY,
                          0LL,  // findCreatedDate
                          0L,  // findCreatedTime
                          uuid,
                          NULL,  // scheduleUUID
                          NULL,  // uuidId
                          NULL,  // entityId
                          NULL,  // archiveType
                          NULL,  // createdDateTime
                          NULL,  // lastErrorMessage
                          NULL,  // totalEntryCount
                          NULL  // totalEntrySize
                         )
       )
    {
      error = ERROR_DATABASE_ENTRY_NOT_FOUND;
    }
  }
  else if (!INDEX_ID_IS_NONE(storageId))
  {
    if (!Index_findStorageById(indexHandle,
                               storageId,
                               uuid,
                               NULL,  // scheduleUUID
                               NULL,  // uuidId
                               NULL,  // entityId
                               NULL,  // storageName
                               NULL,  // createdDateTime
                               NULL,  // size
                               NULL,  // indexState
                               NULL,  // indexMode
                               NULL,  // lastCheckedDateTime
                               NULL,  // errorMessage
                               NULL,  // totalEntryCount
                               NULL  // totalEntrySize
                              )
       )
    {
      error = ERROR_DATABASE_ENTRY_NOT_FOUND;
    }
  }
  if (error == ERROR_NONE)
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = Job_findByUUID(uuid);
      if (jobNode != NULL)
      {
        error = mountAll(&jobNode->job.options.mountList);
      }
    }
  }
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

  // delete storages
  if (!String_isEmpty(jobUUID))
  {
    // delete all storage files with job UUID
    error = deleteUUID(indexHandle,jobUUID);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }

  if (!INDEX_ID_IS_NONE(entityId))
  {
    // delete entity
    error = deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }

  if (!INDEX_ID_IS_NONE(storageId))
  {
    // delete storage file
    error = deleteStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }

  // unmount devices
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    jobNode = Job_findByUUID(uuid);
    if (jobNode != NULL)
    {
      error = unmountAll(&jobNode->job.options.mountList);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexInfo
* Purpose: get index info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            totalNormalEntityCount=<n> \
*            totalFullEntityCount=<n> \
*            totalIncrementalEntityCount=<n> \
*            totalDifferentialEntityCount=<n> \
*            totalContinuousEntityCount=<n> \
*            totalLockedEntityCount=<n> \
*            totalDeletedEntityCount=<n> \
*            \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            totalEntryContentSize=<n> \
*            totalFileCount=<n> \
*            totalFileSize=<n> \
*            totalImageCount=<n> \
*            totalImageSize=<n> \
*            totalDirectoryCount=<n> \
*            totalLinkCount=<n> \
*            totalHardlinkCount=<n> \
*            totalHardlinkSize=<n> \
*            totalSpecialCount=<n> \
*            \
*            totalEntryCountNewest=<n> \
*            totalEntrySizeNewest=<n> \
*            totalEntryContentSizeNewest=<n> \
*            totalFileCountNewest=<n> \
*            totalFileSizeNewest=<n> \
*            totalImageCountNewest=<n> \
*            totalImageSizeNewest=<n> \
*            totalDirectoryCountNewest=<n> \
*            totalLinkCountNewest=<n> \
*            totalHardlinkCountNewest=<n> \
*            totalHardlinkSizeNewest=<n> \
*            totalSpecialCountNewest=<n> \
*            \
*            totalSkippedEntryCount=<n> \
*            \
*            totalStorageCount=<n> \
*            totalStorageSize=<n> \
*            totalDeletedStorageCount=<n>
\***********************************************************************/

LOCAL void serverCommand_indexInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors error;
  uint   totalNormalEntityCount;
  uint   totalFullEntityCount;
  uint   totalIncrementalEntityCount;
  uint   totalDifferentialEntityCount;
  uint   totalContinuousEntityCount;
  uint   totalLockedEntityCount;
  uint   totalDeletedEntityCount;

  uint   totalEntryCount;
  uint64 totalEntrySize;
  uint   totalFileCount;
  uint64 totalFileSize;
  uint   totalImageCount;
  uint64 totalImageSize;
  uint   totalDirectoryCount;
  uint   totalLinkCount;
  uint   totalHardlinkCount;
  uint64 totalHardlinkSize;
  uint   totalSpecialCount;

  uint   totalEntryCountNewest;
  uint64 totalEntrySizeNewest;
  uint   totalFileCountNewest;
  uint64 totalFileSizeNewest;
  uint   totalImageCountNewest;
  uint64 totalImageSizeNewest;
  uint   totalDirectoryCountNewest;
  uint   totalLinkCountNewest;
  uint   totalHardlinkCountNewest;
  uint64 totalHardlinkSizeNewest;
  uint   totalSpecialCountNewest;

  uint   totalSkippedEntryCount;

  uint   totalStorageCount;
  uint64 totalStorageSize;
  uint   totalOKStorageCount;
  uint   totalUpdateRequestedStorageCount;
  uint   totalErrorStorageCount;
  uint   totalDeletedStorageCount;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // init variables

  // get infos
  error = Index_getInfos(indexHandle,
                         &totalNormalEntityCount,
                         &totalFullEntityCount,
                         &totalIncrementalEntityCount,
                         &totalDifferentialEntityCount,
                         &totalContinuousEntityCount,
                         &totalLockedEntityCount,
                         &totalDeletedEntityCount,

                         &totalEntryCount,
                         &totalEntrySize,
                         &totalFileCount,
                         &totalFileSize,
                         &totalImageCount,
                         &totalImageSize,
                         &totalDirectoryCount,
                         &totalLinkCount,
                         &totalHardlinkCount,
                         &totalHardlinkSize,
                         &totalSpecialCount,

                         &totalEntryCountNewest,
                         &totalEntrySizeNewest,
                         &totalFileCountNewest,
                         &totalFileSizeNewest,
                         &totalImageCountNewest,
                         &totalImageSizeNewest,
                         &totalDirectoryCountNewest,
                         &totalLinkCountNewest,
                         &totalHardlinkCountNewest,
                         &totalHardlinkSizeNewest,
                         &totalSpecialCountNewest,

                         &totalSkippedEntryCount,

                         &totalStorageCount,
                         &totalStorageSize,
                         &totalOKStorageCount,
                         &totalUpdateRequestedStorageCount,
                         &totalErrorStorageCount,
                         &totalDeletedStorageCount
                        );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get index infos fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                      "totalEntityCount=%u \
                       totalNormalEntityCount=%u \
                       totalFullEntityCount=%u \
                       totalIncrementalEntityCount=%u \
                       totalDifferentialEntityCount=%u \
                       totalContinuousEntityCount=%u \
                       totalLockedEntityCount=%u \
                       totalDeletedEntityCount=%u \
                       \
                       totalEntryCount=%u \
                       totalEntrySize=%"PRIu64" \
                       totalFileCount=%u \
                       totalFileSize=%"PRIu64" \
                       totalImageCount=%u \
                       totalImageSize=%"PRIu64" \
                       totalDirectoryCount=%u \
                       totalLinkCount=%u \
                       totalHardlinkCount=%u \
                       totalHardlinkSize=%"PRIu64" \
                       totalSpecialCount=%u \
                       \
                       totalEntryCountNewest=%u \
                       totalEntrySizeNewest=%"PRIu64" \
                       totalFileCountNewest=%u \
                       totalFileSizeNewest=%"PRIu64" \
                       totalImageCountNewest=%u \
                       totalImageSizeNewest=%"PRIu64" \
                       totalDirectoryCountNewest=%u \
                       totalLinkCountNewest=%u \
                       totalHardlinkCountNewest=%u \
                       totalHardlinkSizeNewest=%"PRIu64" \
                       totalSpecialCountNewest=%u \
                       \
                       totalSkippedEntryCount=%u \
                       \
                       totalStorageCount=%u \
                       totalStorageSize=%"PRIu64" \
                       totalOKStorageCount=%u \
                       totalUpdateRequestedStorageCount=%u \
                       totalErrorStorageCount=%u \
                       totalDeletedStorageCount=%u \
                      ",
                      totalNormalEntityCount+totalFullEntityCount+totalIncrementalEntityCount+totalDifferentialEntityCount+totalContinuousEntityCount,
                      totalNormalEntityCount,
                      totalFullEntityCount,
                      totalIncrementalEntityCount,
                      totalDifferentialEntityCount,
                      totalContinuousEntityCount,
                      totalLockedEntityCount,
                      totalDeletedEntityCount,

                      totalEntryCount,
                      totalEntrySize,
                      totalFileCount,
                      totalFileSize,
                      totalImageCount,
                      totalImageSize,
                      totalDirectoryCount,
                      totalLinkCount,
                      totalHardlinkCount,
                      totalHardlinkSize,
                      totalSpecialCount,

                      totalEntryCountNewest,
                      totalEntrySizeNewest,
                      totalFileCountNewest,
                      totalFileSizeNewest,
                      totalImageCountNewest,
                      totalImageSizeNewest,
                      totalDirectoryCountNewest,
                      totalLinkCountNewest,
                      totalHardlinkCountNewest,
                      totalHardlinkSizeNewest,
                      totalSpecialCountNewest,

                      totalSkippedEntryCount,

                      totalStorageCount,
                      totalStorageSize,
                      totalOKStorageCount,
                      totalUpdateRequestedStorageCount,
                      totalErrorStorageCount,
                      totalDeletedStorageCount
                     );

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_indexUUIDList
* Purpose: get index database UUID list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [indexStateSet=<state set>|*]
*            [indexModeSet=<mode set>|*]
*            [name=<text>]
*          Result:
*            uuidId=<n>
*            jobUUID=<uuid> \
*            name=<name> \
*            lastExecutedDateTime=<time stamp [s]> \
*            lastErrorCode=<n> \
*            lastErrorData=<text> \
*            totalSize=<n> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexUUIDList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  typedef struct UUIDNode
  {
    LIST_NODE_HEADER(struct UUIDNode);

    IndexId uuidId;
    String  jobUUID;
    uint64  lastExecutedDateTime;
    uint    lastErrorCode;
    String  lastErrorData;
    uint64  totalSize;
    ulong   totalEntryCount;
    uint64  totalEntrySize;
  } UUIDNode;

  typedef struct
  {
    LIST_HEADER(UUIDNode);
  } UUIDList;

  /***********************************************************************\
  * Name   : freeUUIDNode
  * Purpose: free UUID node
  * Input  : uuidNode - UUID node
  *          userData - not used
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeUUIDNode(UUIDNode *uuidNode, void *userData);
  void freeUUIDNode(UUIDNode *uuidNode, void *userData)
  {
    assert(uuidNode != NULL);
    assert(uuidNode->jobUUID != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(uuidNode->lastErrorData);
    String_delete(uuidNode->jobUUID);
  }

  bool             indexStateAny;
  IndexStateSet    indexStateSet;
  bool             indexModeAny;
  IndexModeSet     indexModeSet;
  String           name;
  UUIDList         uuidList;
  uint             lastErrorCode;
  String           lastErrorData;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64           lastExecutedDateTime;
  uint64           totalSize;
  uint             totalEntryCount;
  uint64           totalEntrySize;
  UUIDNode         *uuidNode;
  const JobNode    *jobNode;
  bool             exitsFlag;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter index state set, index mode set, name
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet",NULL),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    indexStateAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet",NULL),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseMode,NULL),INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    indexModeAny = TRUE;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // init variables
  List_init(&uuidList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeUUIDNode,NULL));
  lastErrorData = String_new();

  // get UUIDs from database (Note: store into list to avoid dead-lock in job list)
  error = Index_initListUUIDs(&indexQueryHandle,
                              indexHandle,
                              indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                              indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                              name,
                              0LL,  // offset
                              INDEX_UNLIMITED
                             );
  if (error != ERROR_NONE)
  {
    String_delete(lastErrorData);
    List_done(&uuidList);

    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init uuid list fail");

    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextUUID(&indexQueryHandle,
                              &uuidId,
                              jobUUID,
                              &lastExecutedDateTime,
                              &lastErrorCode,
                              lastErrorData,
                              &totalSize,
                              &totalEntryCount,
                              &totalEntrySize
                             )
        )
  {
    uuidNode = LIST_NEW_NODE(UUIDNode);
    if (uuidNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    uuidNode->uuidId               = uuidId;
    uuidNode->jobUUID              = String_duplicate(jobUUID);
    uuidNode->lastExecutedDateTime = lastExecutedDateTime;
    uuidNode->lastErrorCode        = lastErrorCode;
    uuidNode->lastErrorData        = String_duplicate(lastErrorData);
    uuidNode->totalSize            = totalSize;
    uuidNode->totalEntryCount      = totalEntryCount;
    uuidNode->totalEntrySize       = totalEntrySize;

    List_append(&uuidList,uuidNode);
  }
  Index_doneList(&indexQueryHandle);

  // get job names and send list
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&uuidList,uuidNode)
    {
      // get job name
      jobNode = Job_findByUUID(uuidNode->jobUUID);
      if (jobNode != NULL)
      {
        String_set(name,jobNode->name);
      }
      else
      {
        String_set(name,uuidNode->jobUUID);
      }

      // send result
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "uuidId=%"PRIu64" jobUUID=%S name=%'S lastExecutedDateTime=%"PRIu64" lastErrorCode=%u lastErrorData=%'S totalSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                          uuidNode->uuidId,
                          uuidNode->jobUUID,
                          name,
                          uuidNode->lastExecutedDateTime,
                          uuidNode->lastErrorCode,
                          uuidNode->lastErrorData,
                          uuidNode->totalSize,
                          uuidNode->totalEntryCount,
                          uuidNode->totalEntrySize
                         );
    }
  }

  // send job UUIDs without database entry
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      // check if exists in database
      exitsFlag = FALSE;
      LIST_ITERATEX(&uuidList,uuidNode,!exitsFlag)
      {
        exitsFlag = String_equals(jobNode->job.uuid,uuidNode->jobUUID);
      }

      if (!exitsFlag)
      {
        ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                            "uuidId=0 jobUUID=%S name=%'S lastExecutedDateTime=0 lastErrorCode=0 lastErrorData='' totalSize=0 totalEntryCount=0 totalEntrySize=0",
                            jobNode->job.uuid,
                            jobNode->name
                           );
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorData);
  List_done(&uuidList);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntityList
* Purpose: get index database entity list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>|""]
*            [indexStateSet=<state set>|*]
*            [indexModeSet=<mode set>|*]
*            [name=<text>]
*          Result:
*            jobUUID=<uuid> \
*            entityId=<id> \
*            scheduleUUID=<uuid> \
*            archiveType=<type> \
*            createdDateTime=<time stamp [s]> \
*            lastErrorCode=<n>
*            lastErrorData=<text>
*            totalSize=<n> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            expireDateTime=<time stamp [s]>
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexEntityList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString         (jobUUID,MISC_UUID_STRING_LENGTH);
  bool                 indexStateAny;
  IndexStateSet        indexStateSet;
  bool                 indexModeAny;
  IndexModeSet         indexModeSet;
  String               name;
  IndexEntitySortModes sortMode;
  DatabaseOrdering     ordering;
  Errors               error;
  IndexQueryHandle     indexQueryHandle;
  IndexId              uuidId;
  String               jobName;
  IndexId              entityId;
  StaticString         (scheduleUUID,MISC_UUID_STRING_LENGTH);
  uint64               createdDateTime;
  ArchiveTypes         archiveType;
  uint                 lastErrorCode;
  String               lastErrorData;
  uint64               totalSize;
  uint                 totalEntryCount;
  uint64               totalEntrySize;
  int                  maxAge;
  const JobNode        *jobNode;
  const ScheduleNode   *scheduleNode;
  uint64               expireDateTime;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get uuid, index state set, index mode set, name, name
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet",NULL),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    indexStateAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet",NULL),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseMode,NULL),INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    indexModeAny = TRUE;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);
  StringMap_getEnum(argumentMap,"sortMode",&sortMode,CALLBACK_((StringMapParseEnumFunction)Index_parseEntitySortMode,NULL),INDEX_ENTITY_SORT_MODE_JOB_UUID);
  StringMap_getEnum(argumentMap,"ordering",&ordering,CALLBACK_((StringMapParseEnumFunction)Index_parseOrdering,NULL),DATABASE_ORDERING_NONE);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  jobName       = String_new();
  lastErrorData = String_new();

  // get entities
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 NULL,  // scheduldUUID
                                 ARCHIVE_TYPE_ANY,
                                 indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                 indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                 name,
                                 sortMode,
                                 ordering,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init entity list fail");
    String_delete(lastErrorData);
    String_delete(jobName);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextEntity(&indexQueryHandle,
                                &uuidId,
                                jobUUID,
                                scheduleUUID,
                                &entityId,
                                &archiveType,
                                &createdDateTime,
                                &lastErrorCode,
                                lastErrorData,
                                &totalSize,
                                &totalEntryCount,
                                &totalEntrySize,
                                NULL  // lockedCount
                               )
        )
  {
    // get job name, expire date/time
    String_clear(jobName);
    maxAge = AGE_FOREVER;
    if (!String_isEmpty(jobUUID))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        jobNode = Job_findByUUID(jobUUID);
        if (jobNode != NULL)
        {
          String_set(jobName,jobNode->name);
          scheduleNode = Job_findScheduleByUUID(jobNode,scheduleUUID);
          if (scheduleNode != NULL)
          {
            maxAge = scheduleNode->maxAge;
          }
        }
      }
    }
    if (maxAge != AGE_FOREVER)
    {
      expireDateTime = createdDateTime+maxAge*S_PER_DAY;
    }
    else
    {
      expireDateTime = 0LL;
    }

    // send result
    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "uuid=%"PRIu64" jobUUID=%S jobName=%'S scheduleUUID=%S entityId=%"PRIindexId" archiveType=%s createdDateTime=%"PRIu64" lastErrorCode=%u lastErrorData=%'S totalSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" expireDateTime=%"PRIu64"",
                        uuidId,
                        jobUUID,
                        jobName,
                        scheduleUUID,
                        entityId,
                        Archive_archiveTypeToString(archiveType),
                        createdDateTime,
                        lastErrorCode,
                        lastErrorData,
                        totalSize,
                        totalEntryCount,
                        totalEntrySize,
                        expireDateTime
                       );
  }
  Index_doneList(&indexQueryHandle);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorData);
  String_delete(jobName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageList
* Purpose: get index database storage list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [entityId=<id>|0|*|NONE]
*            [jobUUID=<uuid>|*|""]
*            [scheduleUUID=<uuid>|*|""]
*            indexTypeSet=<type set>|*
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*            [name=<text>]
*            [offset=<n>]
*            [limit=<n>]
*            [sortMode=NAME|SIZE|CREATED|STATE]
*          Result:
*            uuidId=<id> \
*            jobUUID=<uuid> \
*            jobName=<name> \
*            entityId=<id> \
*            scheduleUUID=<uuid> \
*            hostName=<name> \
*            createdDateTime=<date/time> \
*            archiveType=<type> \
*            storageId=<id> \
*            name=<name> \
*            dateTime=<date/time> \
*            size=<n> \
*            indexState=<state> \
*            indexMode=<mode> \
*            lastCheckedDateTime=<date/time> \
*            errorMessage=<text> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n>
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexStorageList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId               entityId;
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  bool                  jobUUIDAny;
  StaticString          (scheduleUUID,MISC_UUID_STRING_LENGTH);
  bool                  scheduleUUIDAny;
  bool                  indexTypeAny;
  IndexTypeSet          indexTypeSet;
  bool                  indexStateAny;
  IndexStateSet         indexStateSet;
  bool                  indexModeAny;
  IndexModeSet          indexModeSet;
  String                name;
  uint64                offset;
  uint64                limit;
  IndexStorageSortModes sortMode;
  DatabaseOrdering      ordering;
  Errors                error;
  IndexQueryHandle      indexQueryHandle;
  StorageSpecifier      storageSpecifier;
  String                jobName;
  String                hostName;
  String                storageName;
  String                printableStorageName;
  String                errorMessage;
  IndexId               uuidId,storageId;
  uint64                createdDateTime;
  ArchiveTypes          archiveType;
  uint64                dateTime;
  uint64                size;
  IndexStates           indexState;
  IndexModes            indexMode;
  uint64                lastCheckedDateTime;
  uint                  totalEntryCount;
  uint64                totalEntrySize;
  const JobNode         *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entity id, jobUUID, scheduleUUID, index type set, index state set, index mode set, name, offset, limit
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entityId",NULL),"*"))
  {
    entityId = INDEX_ID_ANY;
  }
  else if (stringEquals(StringMap_getTextCString(argumentMap,"entityId",NULL),"NONE"))
  {
    entityId = INDEX_ID_ENTITY_NONE;
  }
  else if (StringMap_getUInt64(argumentMap,"entityId",&entityId.data,INDEX_ID_ANY.data))
  {
  }
  else
  {
    entityId = INDEX_ID_ANY;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"jobUUID",NULL),"*"))
  {
    jobUUIDAny = TRUE;
  }
  else if (StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    jobUUIDAny = FALSE;
  }
  else
  {
    jobUUIDAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"scheduleUUID",NULL),"*"))
  {
    scheduleUUIDAny = TRUE;
  }
  else if (StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    scheduleUUIDAny = FALSE;
  }
  else
  {
    scheduleUUIDAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexTypeSet",NULL),"*"))
  {
    indexTypeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexTypeSet",(uint64*)&indexTypeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseType,NULL),INDEX_TYPESET_ALL,"|",INDEX_TYPE_NONE))
  {
    indexTypeAny = FALSE;
  }
  else
  {
    indexTypeAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet",NULL),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    indexStateAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet",NULL),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseMode,NULL),INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    indexModeAny = TRUE;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);
  StringMap_getEnum(argumentMap,"sortMode",&sortMode,CALLBACK_((StringMapParseEnumFunction)Index_parseStorageSortMode,NULL),INDEX_STORAGE_SORT_MODE_NAME);
  StringMap_getEnum(argumentMap,"ordering",&ordering,CALLBACK_((StringMapParseEnumFunction)Index_parseOrdering,NULL),DATABASE_ORDERING_NONE);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  jobName              = String_new();
  hostName             = String_new();
  storageName          = String_new();
  errorMessage         = String_new();
  printableStorageName = String_new();

  // list index
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 !jobUUIDAny ? String_cString(jobUUID) : NULL,
                                 !scheduleUUIDAny ? String_cString(scheduleUUID) : NULL,
                                 Array_cArray(&clientInfo->indexIdArray),
                                 Array_length(&clientInfo->indexIdArray),
                                 indexTypeAny ? INDEX_TYPESET_ALL : indexTypeSet,
                                 indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                 indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                 NULL,  // hostName
                                 NULL,  // userName
                                 name,
                                 sortMode,
                                 ordering,
                                 offset,
                                 limit
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init storage list fail");
    String_delete(printableStorageName);
    String_delete(errorMessage);
    String_delete(storageName);
    String_delete(hostName);
    String_delete(jobName);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextStorage(&indexQueryHandle,
                                 &uuidId,
                                 jobUUID,
                                 &entityId,
                                 scheduleUUID,
                                 hostName,
                                 NULL,  // userName
                                 NULL,  // comment
                                 &createdDateTime,
                                 &archiveType,
                                 &storageId,
                                 storageName,
                                 &dateTime,
                                 &size,
                                 &indexState,
                                 &indexMode,
                                 &lastCheckedDateTime,
                                 errorMessage,
                                 &totalEntryCount,
                                 &totalEntrySize
                                )
        )
  {
    // get job name
    String_set(jobName,jobUUID);
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode != NULL)
      {
        String_set(jobName,jobNode->name);
      }
    }

    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "uuidId=%"PRIu64" jobUUID=%S jobName=%'S entityId=%"PRIu64" scheduleUUID=%S hostName=%'S createdDateTime=%"PRIu64" archiveType='%s' storageId=%"PRIu64" name=%'S dateTime=%"PRIu64" size=%"PRIu64" indexState=%'s indexMode=%'s lastCheckedDateTime=%"PRIu64" errorMessage=%'S totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                        uuidId,
                        jobUUID,
                        jobName,
                        entityId,
                        scheduleUUID,
                        hostName,
                        createdDateTime,
                        Archive_archiveTypeToString(archiveType),
                        storageId,
                        printableStorageName,
                        dateTime,
                        size,
                        Index_stateToString(indexState,"unknown"),
                        Index_modeToString(indexMode,"unknown"),
                        lastCheckedDateTime,
                        errorMessage,
                        totalEntryCount,
                        totalEntrySize
                       );
  }
  Index_doneList(&indexQueryHandle);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(printableStorageName);
  String_delete(errorMessage);
  String_delete(storageName);
  String_delete(hostName);
  String_delete(jobName);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageListClear
* Purpose: clear selected storage index list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear index ids
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Array_clear(&clientInfo->indexIdArray);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexStorageListAdd
* Purpose: add to selected storage index list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageIds=<id>,...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          storageIds;
  IndexId         storageId;
  StringTokenizer stringTokenizer;
  ConstString     token;
  long            nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index ids
  storageIds = String_new();
  if (!StringMap_getString(argumentMap,"storageIds",storageIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageIds=<id>,...");
    String_delete(storageIds);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(storageIds);
    return;
  }

  // add to id array
  String_initTokenizer(&stringTokenizer,storageIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      // get id
      storageId.data = String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex != STRING_END)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_PARSE_ID,"'%S'",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(storageIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }

      // add id
      Array_append(&clientInfo->indexIdArray,&storageId);
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(storageIds);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageListRemove
* Purpose: remove from selected storage list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageIds=<id>,..
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          storageIds;
  IndexId         storageId;
  StringTokenizer stringTokenizer;
  ConstString     token;
  long            nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index ids
  storageIds = String_new();
  if (!StringMap_getString(argumentMap,"storageIds",storageIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageIds=<id>,...");
    String_delete(storageIds);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(storageIds);
    return;
  }

  // remove from id array
  String_initTokenizer(&stringTokenizer,storageIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      // get id
      storageId.data = String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex != STRING_END)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_PARSE_ID,"'%S'",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(storageIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }

      // remove id
      Array_removeAll(&clientInfo->indexIdArray,&storageId,CALLBACK_(NULL,NULL));
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(storageIds);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageListInfo
* Purpose: get index database storage info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [entityId=<id>|0|*|NONE]
*            [jobUUID=<uuid>|*|""]
*            [scheduleUUID=<uuid>|*|""]
*            indexTypeSet=<type set>|*
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*            [name=<text>]
*          Result:
*            totalStorageCount=<n> \
*            totalStorageSize=<n> [bytes] \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> [bytes]
\***********************************************************************/

LOCAL void serverCommand_indexStorageListInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId       entityId;
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  bool          jobUUIDAny;
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  bool          scheduleUUIDAny;
  bool          indexTypeAny;
  IndexTypeSet  indexTypeSet;
  bool          indexStateAny;
  IndexStateSet indexStateSet;
  bool          indexModeAny;
  IndexModeSet  indexModeSet;
  String        name;
  Errors        error;
  uint          totalStorageCount,totalEntryCount;
  uint64        totalStorageSize,totalEntrySize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // get entityId, jobUUID, scheduleUUID, index type set, index state set, index mode set, name
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entityId",NULL),"*"))
  {
    entityId = INDEX_ID_ANY;
  }
  else if (stringEquals(StringMap_getTextCString(argumentMap,"entityId",NULL),"NONE"))
  {
    entityId = INDEX_ID_NONE;
  }
  else if (StringMap_getUInt64(argumentMap,"entityId",&entityId.data,INDEX_ID_ANY.data))
  {
  }
  else
  {
    entityId = INDEX_ID_ANY;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"jobUUID",NULL),"*"))
  {
    jobUUIDAny = TRUE;
  }
  else if (StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    jobUUIDAny = FALSE;
  }
  else
  {
    jobUUIDAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"scheduleUUID",NULL),"*"))
  {
    scheduleUUIDAny = TRUE;
  }
  else if (StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    scheduleUUIDAny = FALSE;
  }
  else
  {
    scheduleUUIDAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexTypeSet",NULL),"*"))
  {
    indexTypeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexTypeSet",&indexTypeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseType,NULL),INDEX_TYPESET_ALL,"|",INDEX_TYPE_NONE))
  {
    indexTypeAny = FALSE;
  }
  else
  {
    indexTypeAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet",NULL),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    indexStateAny = TRUE;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet",NULL),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,CALLBACK_((StringMapParseEnumFunction)Index_parseMode,NULL),INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    indexModeAny = TRUE;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // get index info
  error = Index_getStoragesInfos(indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 !jobUUIDAny ? jobUUID : NULL,
                                 !scheduleUUIDAny ? scheduleUUID : NULL,
                                 Array_cArray(&clientInfo->indexIdArray),
                                 Array_length(&clientInfo->indexIdArray),
                                 indexTypeAny ? INDEX_TYPESET_ALL : indexTypeSet,
                                 indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                 indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                 name,
                                 &totalStorageCount,
                                 &totalStorageSize,
                                 &totalEntryCount,
                                 &totalEntrySize
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get storages info from index database fail");
    String_delete(name);
    return;
  }
//fprintf(stderr,"%s, %d: %ld %"PRIi64" %ld %"PRIi64"\n",__FILE__,__LINE__,totalStorageCount,totalStorageSize,totalEntryCount,totalEntrySize);

  // send data
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                      "totalStorageCount=%lu totalStorageSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                      totalStorageCount,
                      totalStorageSize,
                      totalEntryCount,
                      totalEntrySize
                     );

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntryList
* Purpose: get index database entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            [entryType=*|FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL]
*            [newestOnly=yes|no]
*            [selectedOnly=yes|no]
*            [fragments=yes|no]
*            [offset=<n>]
*            [limit=<n>]
*            [sortMode=ARCHIVE|NAME|TYPE|SIZE|LAST_CHANGED]
*            [ordering=ASCENDING|DESCENDING]
*          Result:
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
*            entryId=<n> entryType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n> fragmentCount=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            entryId=<n> entryType=IMAGE name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            fileSystemType=<name> fragmentCount=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            entryId=<n> entryType=DIRECTORY name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            entryId=<n> entryType=LINK name=<name> \
*            destinationName=<name> dateTime=<time stamp> userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            entryId=<n> entryType=HARDLINK name=<name> dateTime=<time stamp>
*            userId=<n> groupId=<n> permission=<n> fragmentCount=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            entryId=<n> entryType=SPECIAL name=<name> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
\***********************************************************************/

LOCAL void serverCommand_indexEntryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  #define SEND_FILE_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,size,dateTime,userId,groupId,permission,fragmentCount) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" userId=%u groupId=%u permission=%u fragmentCount=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
                          entryId, \
                          name, \
                          size, \
                          dateTime, \
                          userId, \
                          groupId, \
                          permission, \
                          fragmentCount \
                         ); \
    } \
    while (0)
  #define SEND_IMAGE_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,fileSystemType,size,fragmentCount) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=IMAGE name=%'S fileSystemType=%s size=%"PRIu64" fragmentCount=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
                          entryId, \
                          name, \
                          FileSystem_fileSystemTypeToString(fileSystemType,NULL), \
                          size, \
                          fragmentCount \
                         ); \
    } \
    while (0)
  #define SEND_DIRECTORY_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,size,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=DIRECTORY name=%'S size=%"PRIu64" dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
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
  #define SEND_LINK_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,destinationName,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=LINK name=%'S destinationName=%'S dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
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
  #define SEND_HARDLINK_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,size,dateTime,userId,groupId,permission,fragmentCount) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=HARDLINK name=%'S size=%"PRIu64" dateTime=%"PRIu64" userId=%u groupId=%u permission=%u fragmentCount=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
                          entryId, \
                          name, \
                          size, \
                          dateTime, \
                          userId, \
                          groupId, \
                          permission, \
                          fragmentCount \
                         ); \
    } \
    while (0)
  #define SEND_SPECIAL_ENTRY(jobName,archiveType,hostName,storageName,entryId,name,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S entryId=%"PRIindexId" entryType=SPECIAL name=%'S dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
                          entryId, \
                          name, \
                          dateTime, \
                          userId, \
                          groupId, \
                          permission \
                         ); \
    } \
    while (0)

  String              name;
  IndexTypes          entryType;
  bool                newestOnly;
  bool                selectedOnly;
  bool                fragmentsCount;
  uint64              offset;
  uint64              limit;
  IndexEntrySortModes sortMode;
  DatabaseOrdering    ordering;
  IndexId             prevUUIDId;
  String              jobName;
  String              hostName;
  String              storageName;
  String              entryName;
  FileSystemTypes     fileSystemType;
  String              destinationName;
  Errors              error;
  IndexQueryHandle    indexQueryHandle;
  IndexId             uuidId,entityId,entryId;
  StaticString        (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes        archiveType;
  uint64              size;
  uint64              timeModified;
  uint32              userId,groupId;
  uint32              permission;
  uint                fragmentCount;
  const JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter name, entry type, new entries only, fragments, offset, limit
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entryType",NULL),"*"))
  {
    entryType = INDEX_TYPE_ANY;
  }
  else if (StringMap_getEnum(argumentMap,"entryType",&entryType,CALLBACK_((StringMapParseEnumFunction)Index_parseType,NULL),INDEX_TYPE_ANY))
  {
    // ok
  }
  else
  {
    entryType = INDEX_TYPE_ANY;
  }
  StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE);
  StringMap_getBool(argumentMap,"selectedOnly",&selectedOnly,FALSE);
  StringMap_getBool(argumentMap,"fragmentsCount",&fragmentsCount,FALSE);
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);
  StringMap_getEnum(argumentMap,"sortMode",&sortMode,CALLBACK_((StringMapParseEnumFunction)Index_parseEntrySortMode,NULL),INDEX_ENTRY_SORT_MODE_NAME);
  StringMap_getEnum(argumentMap,"ordering",&ordering,CALLBACK_((StringMapParseEnumFunction)Index_parseOrdering,NULL),DATABASE_ORDERING_NONE);

  // check if index database is available+ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  prevUUIDId      = INDEX_ID_NONE;
  jobName         = String_new();
  hostName        = String_new();
  storageName     = String_new();
  entryName       = String_new();
  destinationName = String_new();
  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                !selectedOnly ? Array_cArray(&clientInfo->indexIdArray) : NULL,
                                !selectedOnly ? Array_length(&clientInfo->indexIdArray) : 0,
                                selectedOnly ? Array_cArray(&clientInfo->entryIdArray) : NULL,
                                selectedOnly ? Array_length(&clientInfo->entryIdArray) : 0L,
                                entryType,
                                name,
                                newestOnly,
                                fragmentsCount,
                                sortMode,
                                ordering,
                                offset,
                                limit
                               );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list entries fail");
    String_delete(destinationName);
    String_delete(entryName);
    String_delete(storageName);
    String_delete(hostName);
    String_delete(jobName);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextEntry(&indexQueryHandle,
                               &uuidId,
                               jobUUID,
                               &entityId,
                               NULL,  // scheduleUUID,
                               hostName,
                               NULL,  // userName
                               &archiveType,
                               &entryId,
                               entryName,
                               NULL,  // storageId
                               storageName,
                               &size,
                               &timeModified,
                               &userId,
                               &groupId,
                               &permission,
                               &fragmentCount,
                               destinationName,
                               &fileSystemType,
                               NULL  // blockSize
                              )
        )
  {
    // get job name
    if (!INDEX_ID_EQUALS(uuidId,prevUUIDId))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        jobNode = Job_findByUUID(jobUUID);
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
    switch (INDEX_TYPE(entryId))
    {
      case INDEX_TYPE_FILE:
        SEND_FILE_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentCount);
        break;
      case INDEX_TYPE_IMAGE:
        SEND_IMAGE_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,fileSystemType,size,fragmentCount);
        break;
      case INDEX_TYPE_DIRECTORY:
        SEND_DIRECTORY_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,size,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_LINK:
        SEND_LINK_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,destinationName,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_HARDLINK:
        SEND_HARDLINK_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentCount);
        break;
      case INDEX_TYPE_SPECIAL:
        SEND_SPECIAL_ENTRY(jobName,archiveType,hostName,storageName,entryId,entryName,timeModified,userId,groupId,permission);
        break;
      default:
        // ignored
        break;
    }
  }
  Index_doneList(&indexQueryHandle);
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(destinationName);
  String_delete(entryName);
  String_delete(storageName);
  String_delete(hostName);
  String_delete(jobName);
  String_delete(name);

  #undef SEND_SPECIAL_ENTRY
  #undef SEND_HARDLINK_ENTRY
  #undef SEND_LINK_ENTRY
  #undef SEND_DIRECTORY_ENTRY
  #undef SEND_IMAGE_ENTRY
  #undef SEND_FILE_ENTRY
}

/***********************************************************************\
* Name   : serverCommand_indexEntryListClear
* Purpose: clear selected index entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexEntryListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Array_clear(&clientInfo->entryIdArray);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexEntryListAdd
* Purpose: add to selected index entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entryIds=<id>,...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexEntryListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           entryIds;
  IndexId          entryId;
  StringTokenizer  stringTokenizer;
  ConstString      token;
  long             nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry ids
  entryIds = String_new();
  if (!StringMap_getString(argumentMap,"entryIds",entryIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entryIds=<id>,...");
    String_delete(entryIds);
    return;
  }

  // check if index database is available+ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(entryIds);
    return;
  }

  // add to ids and all ids of fragments to array
  String_initTokenizer(&stringTokenizer,entryIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      // get id
      entryId.data = String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex != STRING_END)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_PARSE_ID,"'%S'",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(entryIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }

      Array_append(&clientInfo->entryIdArray,&entryId);
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(entryIds);
}

/***********************************************************************\
* Name   : serverCommand_indexEntryListRemove
* Purpose: remove from selected index entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entryId=<id>,...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexEntryListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          entryIds;
  IndexId         entryId;
  StringTokenizer stringTokenizer;
  ConstString     token;
  long            nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry ids
  entryIds = String_new();
  if (!StringMap_getString(argumentMap,"entryIds",entryIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entryIds=<id>,...");
    String_delete(entryIds);
    return;
  }

  // check if index database is available+ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(entryIds);
    return;
  }

  // remove from id array
  String_initTokenizer(&stringTokenizer,entryIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      // get id
      entryId.data = String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex != STRING_END)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_PARSE_ID,"'%S'",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(entryIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }

      // remove id
      Array_removeAll(&clientInfo->entryIdArray,&entryId,CALLBACK_(NULL,NULL));
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(entryIds);
}

/***********************************************************************\
* Name   : serverCommand_indexEntryListInfo
* Purpose: get index database entries info of currently selected index
*          entries
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            [entryType=*|FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL]
*            [newestOnly=yes|no]
*            [selectedOnly=yes|no]
*          Result:
*            totalStorageCount<n>
*            totalStorageSize<n [bytes]>
*            totalEntryCount=<n>
*            totalEntrySize=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_indexEntryListInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String     name;
  IndexTypes entryType;
  bool       newestOnly;
  bool       selectedOnly;
  Errors     error;
  uint       totalStorageCount,totalEntryCount;
  uint64     totalStorageSize,totalEntrySize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry pattern, index type, new entries only
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<text>");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entryType",NULL),"*"))
  {
    entryType = INDEX_TYPE_ANY;
  }
  else if (StringMap_getEnum(argumentMap,"entryType",&entryType,CALLBACK_((StringMapParseEnumFunction)Index_parseType,NULL),INDEX_TYPE_ANY))
  {
    // ok
  }
  else
  {
    entryType = INDEX_TYPE_ANY;
  }
  StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE);
  StringMap_getBool(argumentMap,"selectedOnly",&selectedOnly,FALSE);

  // check if index database is available+ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // get index info
  error = Index_getEntriesInfo(indexHandle,
                               !selectedOnly ? Array_cArray(&clientInfo->indexIdArray) : NULL,
                               !selectedOnly ? Array_length(&clientInfo->indexIdArray) : 0L,
                               selectedOnly ? Array_cArray(&clientInfo->entryIdArray) : NULL,
                               selectedOnly ? Array_length(&clientInfo->entryIdArray) : 0L,
                               entryType,
                               name,
                               newestOnly,
                               &totalStorageCount,
                               &totalStorageSize,
                               &totalEntryCount,
                               &totalEntrySize
                              );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get entries info index database fail");
    String_delete(name);
    return;
  }

  // send data
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                      "totalStorageCount=%lu totalStorageSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                      totalStorageCount,
                      totalStorageSize,
                      totalEntryCount,
                      totalEntrySize
                     );

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntryFragmentList
* Purpose: get index database entry fragment list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [entryId=<n>]
*          Result:
*            storageName=<name> \
*            storageDateTime=<n> \
*            fragmentOffset=<n [bytes]> fragmentSize=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_indexEntryFragmentList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entryId;
  uint64  offset;
  uint64  limit;
  Errors  error;
  IndexQueryHandle indexQueryHandle;
  IndexId storageId;
  String  storageName;
  uint64  storageDateTime;
  uint64  fragmentOffset,fragmentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entity id, offset, limit
  if (!StringMap_getIndexId(argumentMap,"entryId",&entryId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entryId=<id>");
    return;
  }
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);

  // check if index database is available+ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // initialize variables
  storageName = String_new();

  // list entry fragments
  error = Index_initListEntryFragments(&indexQueryHandle,
                                       indexHandle,
                                       entryId,
                                       offset,
                                       limit
                                      );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list entries fail");
    String_delete(storageName);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextEntryFragment(&indexQueryHandle,
                                       NULL,  // entryFragmentId
                                       &storageId,
                                       storageName,
                                       &storageDateTime,
                                       &fragmentOffset,
                                       &fragmentSize
                                      )
        )
  {
    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "storageId=%"PRIu64" storageName=%'S storageDateTime=%"PRIu64" fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"",
                        storageId,
                        storageName,
                        storageDateTime,
                        fragmentOffset,
                        fragmentSize
                       ); \
  }
  Index_doneList(&indexQueryHandle);
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : serverCommand_indexHistoryList
* Purpose: get index database entity list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [jobUUID=<uuid>]
*          Result:
*            jobUUID=<uuid> \
*            scheduleUUID=<uuid> \
*            entityId=<id> \
*            archiveType=<type> \
*            lastCreatedDateTime=<time stamp [s]> \
*            lastErrorMessage=<error message>
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            expireDateTime=<time stamp [s]>
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexHistoryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  String           jobName;
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String           hostName;
  uint64           createdDateTime;
  ArchiveTypes     archiveType;
  String           errorMessage;
  uint64           duration;
  uint             totalEntryCount;
  uint64           totalEntrySize;
  uint             skippedEntryCount;
  uint64           skippedEntrySize;
  uint             errorEntryCount;
  uint64           errorEntrySize;
  const JobNode    *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get job UUID
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // initialize variables
  jobName      = String_new();
  hostName     = String_new();
  errorMessage = String_new();

  // get entities
  error = Index_initListHistory(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init history list fail");
    String_delete(errorMessage);
    String_delete(hostName);
    String_delete(jobName);
    return;
  }

  while (   !isCommandAborted(clientInfo,id)
         && !isQuit()
         && Index_getNextHistory(&indexQueryHandle,
                                 NULL,  // historyId
                                 &uuidId,
                                 jobUUID,
                                 scheduleUUID,
                                 hostName,
                                 NULL,  // userName
                                 &archiveType,
                                 &createdDateTime,
                                 errorMessage,
                                 &duration,
                                 &totalEntryCount,
                                 &totalEntrySize,
                                 &skippedEntryCount,
                                 &skippedEntrySize,
                                 &errorEntryCount,
                                 &errorEntrySize
                                )
        )
  {
    // get job name
    String_clear(jobName);
    if (!String_isEmpty(jobUUID))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        jobNode = Job_findByUUID(jobUUID);
        if (jobNode != NULL)
        {
          String_set(jobName,jobNode->name);
        }
      }
    }

    // send result
    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "uuid=%"PRIu64" jobUUID=%S jobName=%'S scheduleUUID=%S hostName=%'S archiveType=%s createdDateTime=%"PRIu64" errorMessage=%'S duration=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" skippedEntryCount=%lu skippedEntrySize=%"PRIu64" errorEntryCount=%lu errorEntrySize=%"PRIu64"",
                        uuidId,
                        jobUUID,
                        jobName,
                        scheduleUUID,
                        hostName,
                        Archive_archiveTypeToString(archiveType),
                        createdDateTime,
                        errorMessage,
                        duration,
                        totalEntryCount,
                        totalEntrySize,
                        skippedEntryCount,
                        skippedEntrySize,
                        errorEntryCount,
                        errorEntrySize
                       );
  }
  Index_doneList(&indexQueryHandle);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(errorMessage);
  String_delete(hostName);
  String_delete(jobName);
}

/***********************************************************************\
* Name   : serverCommand_indexEntityAdd
* Purpose: add entity to index database
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            [scheduleUUID=<uuid>]
*            [hostName=<name>]
*            [userName=<name>]
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [createdDateTime=<time stamp [s]>]
*          Result:
*            entityId=<id>
\***********************************************************************/

LOCAL void serverCommand_indexEntityAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       hostName,userName;
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  IndexId      entityId;
  Errors       error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get jobUUID, schedule UUID, hostName, userName, archive type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
// TODO: entityUUID?
  StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL);
  hostName = String_new();
  StringMap_getString(argumentMap,"hostName",hostName,NULL);
  userName = String_new();
  StringMap_getString(argumentMap,"userName",userName,NULL);
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_UNKNOWN))
  {
    String_delete(userName);
    String_delete(hostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    String_delete(hostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // create new entity
  error = Index_newEntity(indexHandle,
                          String_cString(jobUUID),
                          String_cString(scheduleUUID),
                          String_cString(hostName),
                          String_cString(userName),
                          archiveType,
                          createdDateTime,
                          FALSE,  // not locked
                          &entityId
                         );
  if (error != ERROR_NONE)
  {
    String_delete(userName);
    String_delete(hostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"add entity fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"entityId=%"PRIu64"",entityId);

  // free resources
  String_delete(userName);
  String_delete(hostName);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageAdd
* Purpose: add storages to index database (if not already exists)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            [patternType=<type>]
*            [forceRefresh=yes|no]
*            [progressSteps=<n>]
*          Result:
*            storageId=<id>
\***********************************************************************/

LOCAL void serverCommand_indexStorageAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           name;
  PatternTypes     patternType;
  bool             forceRefresh;
  int              progressSteps;
  StorageSpecifier nameStorageSpecifier;
  JobOptions       jobOptions;
  bool             foundFlag;
  String           printableStorageName;
  bool             updateRequestedFlag;
  StorageInfo      storageInfo;
  IndexId          storageId;
  Errors           error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get name, pattern type, force refresh, progress steps
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<text>");
    String_delete(name);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,CALLBACK_((StringMapParseEnumFunction)Pattern_parsePatternType,NULL),PATTERN_TYPE_GLOB);
  StringMap_getBool(argumentMap,"forceRefresh",&forceRefresh,FALSE);
  StringMap_getInt(argumentMap,"progressSteps",&progressSteps,1000);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // init variables
  Storage_initSpecifier(&nameStorageSpecifier);
  Job_initOptions(&jobOptions);

  // parse storage specifier
  error = Storage_parseName(&nameStorageSpecifier,name);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"invalid storage specifier");
    Job_doneOptions(&jobOptions);
    Storage_doneSpecifier(&nameStorageSpecifier);
    String_delete(name);
    return;
  }

  foundFlag           = FALSE;
  updateRequestedFlag = FALSE;

  // try to open as storage file
  if (!foundFlag)
  {
    if (!Storage_isPatternSpecifier(&nameStorageSpecifier))
    {
      if (Storage_init(&storageInfo,
                       NULL, // masterIO
                       &nameStorageSpecifier,
                       &jobOptions,
                       &globalOptions.indexDatabaseMaxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_LOW,
                       CALLBACK_(NULL,NULL),  // storageUpdateProgress
                       CALLBACK_(NULL,NULL),  // getNamePassword
                       CALLBACK_(NULL,NULL),  // requestVolume
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      ) == ERROR_NONE
         )
      {
        if (   Storage_exists(&storageInfo,NULL)
            && Storage_isFile(&storageInfo,NULL)
            && Storage_isReadable(&storageInfo,NULL)
           )
        {
          printableStorageName = Storage_getPrintableName(NULL,&nameStorageSpecifier,NULL);

          if (Index_findStorageByName(indexHandle,
                                      &nameStorageSpecifier,
                                      NULL,  // archiveName
                                      NULL,  // uuidId
                                      NULL,  // entityId
                                      NULL,  // jobUUID
                                      NULL,  // scheduleUUID
                                      &storageId,
                                      NULL,  // createdDateTime
                                      NULL,  // size
                                      NULL,  // indexState
                                      NULL,  // indexMode
                                      NULL,  // lastCheckedDateTime
                                      NULL,  // errorMessage
                                      NULL,  // totalEntryCount
                                      NULL  // totalEntrySize
                                     )
             )
          {
            if (forceRefresh)
            {
              error = Index_setStorageState(indexHandle,
                                            storageId,
                                            INDEX_STATE_UPDATE_REQUESTED,
                                            Misc_getCurrentDateTime(),
                                            NULL  // errorMessage
                                           );
              if (error == ERROR_NONE)
              {
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                    storageId,
                                    printableStorageName
                                   );
              }

              updateRequestedFlag = TRUE;
            }
          }
          else
          {
            error = Index_newStorage(indexHandle,
                                     INDEX_ID_NONE, // uuidId
// TODO: id correct?
//INDEX_DEFAULT_ENTITY_ID,//                                     INDEX_ID_NONE, // entityId
                                     INDEX_ID_NONE, // entityId
                                     NULL,  // hostName
                                     NULL,  // userName
                                     printableStorageName,
                                     0LL,  // createdDateTime
                                     0LL,  // size
                                     INDEX_STATE_UPDATE_REQUESTED,
                                     INDEX_MODE_MANUAL,
                                     &storageId
                                    );
            if (error == ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                  storageId,
                                  printableStorageName
                                 );
            }

            updateRequestedFlag = TRUE;
          }

          foundFlag = TRUE;
        }

        Storage_done(&storageInfo);
      }
    }
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      Job_doneOptions(&jobOptions);
      Storage_doneSpecifier(&nameStorageSpecifier);
      String_delete(name);
      return;
    }
  }

  // try to open as directory: add all matching entries
  if (!foundFlag)
  {
    error = Storage_forAll(&nameStorageSpecifier,
                           NULL,  // directory
                           "*" FILE_NAME_EXTENSION_ARCHIVE_FILE,
                           TRUE,  // skipUnreadableFlag
                           CALLBACK_INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                           {
                             StorageSpecifier storageSpecifier;
                             ConstString      printableStorageName;

                             UNUSED_VARIABLE(fileInfo);
                             UNUSED_VARIABLE(userData);

                             Storage_initSpecifier(&storageSpecifier);

                             error = Storage_parseName(&storageSpecifier,storageName);
                             if (error == ERROR_NONE)
                             {
                               if (   (   !Storage_isPatternSpecifier(&nameStorageSpecifier)
                                       || Pattern_match(&nameStorageSpecifier.archivePattern,storageSpecifier.archiveName,0,PATTERN_MATCH_MODE_EXACT,NULL,NULL)
                                      )
                                   && String_endsWithCString(storageSpecifier.archiveName,FILE_NAME_EXTENSION_ARCHIVE_FILE)
                                  )
                               {
                                 printableStorageName = Storage_getPrintableName(NULL,&storageSpecifier,NULL);

                                 if (Index_findStorageByName(indexHandle,
                                                             &storageSpecifier,
                                                             NULL,  // findArchiveName
                                                             NULL,  // uuidId
                                                             NULL,  // entityId
                                                             NULL,  // jobUUID
                                                             NULL,  // scheduleUUID
                                                             &storageId,
                                                             NULL,  // createdDateTime
                                                             NULL,  // size
                                                             NULL,  // indexState
                                                             NULL,  // indexMode
                                                             NULL,  // lastCheckedDateTime
                                                             NULL,  // errorMessage
                                                             NULL,  // totalEntryCount
                                                             NULL  // totalEntrySize
                                                            )
                                    )
                                 {
                                   if (forceRefresh)
                                   {
                                     error = Index_setStorageState(indexHandle,
                                                                   storageId,
                                                                   INDEX_STATE_UPDATE_REQUESTED,
                                                                   Misc_getCurrentDateTime(),
                                                                   NULL  // errorMessage
                                                                  );
                                     if (error == ERROR_NONE)
                                     {
                                       ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                                           storageId,
                                                           printableStorageName
                                                          );

                                       updateRequestedFlag = TRUE;
                                     }
                                   }
                                 }
                                 else
                                 {
                                   if (Storage_init(&storageInfo,
                                                    NULL, // masterIO
                                                    &storageSpecifier,
                                                    &jobOptions,
                                                    &globalOptions.indexDatabaseMaxBandWidthList,
                                                    SERVER_CONNECTION_PRIORITY_LOW,
                                                    CALLBACK_(NULL,NULL),  // storageUpdateProgress
                                                    CALLBACK_(NULL,NULL),  // getNamePassword
                                                    CALLBACK_(NULL,NULL),  // requestVolume
                                                    CALLBACK_(NULL,NULL),  // isPause
                                                    CALLBACK_(NULL,NULL),  // isAborted
                                                    NULL  // logHandle
                                                   ) == ERROR_NONE
                                      )
                                   {
                                     if (   Storage_isFile(&storageInfo,NULL)
                                         && Storage_isReadable(&storageInfo,NULL)
                                        )
                                     {
                                       error = Index_newStorage(indexHandle,
                                                                INDEX_ID_NONE, // uuidId
                                                                INDEX_ID_NONE, // entityId
                                                                NULL,  // hostName
                                                                NULL,  // userName
                                                                printableStorageName,
                                                                0LL,  // createdDateTime
                                                                0LL,  // size
                                                                INDEX_STATE_UPDATE_REQUESTED,
                                                                INDEX_MODE_MANUAL,
                                                                &storageId
                                                               );
                                       if (error == ERROR_NONE)
                                       {
                                         ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                                             storageId,
                                                             printableStorageName
                                                            );
                                         updateRequestedFlag = TRUE;
                                       }
                                     }
                                     Storage_done(&storageInfo);
                                   }
                                 }
                               }

                               if (error == ERROR_NONE)
                               {
                                 foundFlag = TRUE;
                               }
                             }

                             Storage_doneSpecifier(&storageSpecifier);

                             return !isCommandAborted(clientInfo,id) ? ERROR_NONE : ERROR_ABORTED;
                           },NULL),
                           CALLBACK_INLINE(void,(ulong doneCount, ulong totalCount, void *userData),
                           {
                             static ulong lastProgressDoneCount = 0L;
                             UNUSED_VARIABLE(userData);

                             if (doneCount >= (lastProgressDoneCount+(ulong)progressSteps))
                             {
                               ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"doneCount=%lu totalCount=%lu",doneCount,totalCount);
                               lastProgressDoneCount = doneCount;
                             }
                           },NULL)
                          );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      Job_doneOptions(&jobOptions);
      Storage_doneSpecifier(&nameStorageSpecifier);
      String_delete(name);
      return;
    }
  }

  // trigger index thread
  if (updateRequestedFlag)
  {
    Semaphore_signalModified(&updateIndexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }

  if (error == ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
  }

  // free resources
  Job_doneOptions(&jobOptions);
  Storage_doneSpecifier(&nameStorageSpecifier);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexAssign
* Purpose: assign index database for storage; create entity if requested
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            toJobUUID=<uuid>|""
*            toScheduleUUID=<uuid>|""
*            toHostName=<name>|""
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
  StaticString     (toScheduleUUID,MISC_UUID_STRING_LENGTH);
  IndexId          toEntityId;
  String           toHostName;
  ArchiveTypes     archiveType;
  uint64           createdDateTime;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  IndexId          entityId;
  IndexId          storageId;
  Errors           error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get toJobUUID/toEntityId, toScheduleUUID, toHostName, archive type, createdDateTime, jobUUID/entityId/storageId
  String_clear(toJobUUID);
  toEntityId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"toJobUUID",toJobUUID,NULL)
      && !StringMap_getIndexId(argumentMap,"toEntityId",&toEntityId,INDEX_ID_NONE)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"toJobUUID=<uuid> or toEntityId=<id>");
    return;
  }
  StringMap_getString(argumentMap,"toScheduleUUID",toScheduleUUID,NULL);
  toHostName = String_new();
  StringMap_getString(argumentMap,"toHostName",toHostName,NULL);
  StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_NONE);
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getIndexId(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    String_delete(toHostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    String_delete(toHostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  if (!String_isEmpty(jobUUID))
  {
    // assign all storages/entities of job
    if      (!INDEX_ID_IS_NONE(toEntityId))
    {
      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityId
                             INDEX_ID_NONE,  // storageId
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign job fail");
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              String_cString(toJobUUID),
                              String_cString(toScheduleUUID),
                              String_cString(toHostName),
                              NULL,  // userName
                              archiveType,
                              createdDateTime,
                              TRUE,  // locked
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot create entity for %S",toJobUUID);
        return;
      }

      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityId
                             INDEX_ID_NONE,  // storageId
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign job fail");
        return;
      }

      // unlock
      error = Index_unlockEntity(indexHandle,
                                 entityId
                                );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"unlock entity fail");
        return;
      }
    }
  }

  if (!INDEX_ID_IS_NONE(entityId))
  {
    // assign all storages/entity
    if      (!INDEX_ID_IS_NONE(toEntityId))
    {
      // assign all storages of entity to other entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             entityId,
                             INDEX_ID_NONE,  // storageId
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign entity fail");
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // assign entity to other job
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             entityId,
                             INDEX_ID_NONE,  // storageId
                             toJobUUID,
                             INDEX_ID_NONE,  // toEntityId
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign enttity fail");
        return;
      }
    }
  }

  if (!INDEX_ID_IS_NONE(storageId))
  {
    // assign storage
    if      (!INDEX_ID_IS_NONE(toEntityId))
    {
      // assign storage to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityId
                             storageId,
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign storage to entity fail");
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              String_cString(toJobUUID),
                              String_cString(toScheduleUUID),
                              String_cString(toHostName),
                              NULL, // userName
                              archiveType,
                              createdDateTime,
                              TRUE,  // locked
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot create entity for %S",toJobUUID);
        return;
      }

      // assign storage to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityId
                             storageId,
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"assign storage to entity fail");
        return;
      }

      // unlock
      error = Index_unlockEntity(indexHandle,
                                 entityId
                                );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"unlock entity fail");
        return;
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(toHostName);
}

/***********************************************************************\
* Name   : serverCommand_indexRefresh
* Purpose: refresh index database for storage
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            [state=<state>|*]
*            uuidId=<id>|0 and/or
*            entityId=<id>|0 and/or
*            storageId=<id>|0 and/or
*            jobUUID=<uuid>|"" and/or
*            name=<text>
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
  Array            storageIdArray;
  String           storageName;
  ulong            i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // state, uuidId/entityId/storageId/jobUUID/name
  if      (stringEquals(StringMap_getTextCString(argumentMap,"state",NULL),"*"))
  {
    stateAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"state",&state,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_NONE))
  {
    stateAny = FALSE;
  }
  else
  {
    stateAny = TRUE;
  }
  name      = String_new();
  uuidId    = INDEX_ID_NONE;
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  String_clear(jobUUID);
  if (   !StringMap_getIndexId(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE)
      && !StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getIndexId(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
      && !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getString(argumentMap,"name",name,NULL)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<id> or entityId=<id> or storageId=<id> or jobUUID=<uuid> or name=<text>");
    String_delete(name);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // init variables
  Array_init(&storageIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  storageName = String_new();

  // collect all storage ids (Note: do this to avoid infinite loop or database-busy-error when states are changed in another thread, too)
  if (   INDEX_ID_IS_NONE(uuidId)
      && INDEX_ID_IS_NONE(storageId)
      && INDEX_ID_IS_NONE(entityId)
      && String_isEmpty(jobUUID)
      && String_isEmpty(name)
     )
  {
    // refresh all storage with specific state
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        Array_append(&storageIdArray,&storageId);
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (!INDEX_ID_IS_NONE(uuidId))
  {
    // refresh all storage of uuid
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   uuidId,
                                   INDEX_ID_ANY,  // entityId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        Array_append(&storageIdArray,&storageId);
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (!INDEX_ID_IS_NONE(entityId))
  {
    // refresh all storage of entity
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        Array_append(&storageIdArray,&storageId);
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (!INDEX_ID_IS_NONE(storageId))
  {
    Array_append(&storageIdArray,&storageId);
  }

  if (!String_isEmpty(jobUUID))
  {
    // refresh all storage of all entities of job
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   String_cString(jobUUID),
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        Array_append(&storageIdArray,&storageId);
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
                                   INDEX_ID_ANY,  // entityId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   name,
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        Array_append(&storageIdArray,&storageId);
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  // set state for collected storage ids
  ARRAY_ITERATE(&storageIdArray,i,storageId)
  {
    error = Index_setStorageState(indexHandle,
                                  storageId,
                                  INDEX_STATE_UPDATE_REQUESTED,
                                  0LL,  // lastCheckedDateTime
                                  NULL  // errorMessage
                                 );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"set storage state fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // trigger index thread
  Semaphore_signalModified(&updateIndexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // free resources
  Array_done(&storageIdArray);
  String_delete(storageName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexRemove
* Purpose: remove job/entity/storage from index database
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            state=<state>|*
*            uuidId=<id> or entityId=<id> or storageId=<id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  IndexStates      indexState;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get state and jobUUID, entityId, or storageId
  if      (stringEquals(StringMap_getTextCString(argumentMap,"state",NULL),"*"))
  {
    stateAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"state",&state,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_NONE))
  {
    stateAny = FALSE;
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"filter state=OK|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  StringMap_getIndexId(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE);
  StringMap_getIndexId(argumentMap,"entityId",&entityId,INDEX_ID_NONE);
  StringMap_getIndexId(argumentMap,"storageId",&storageId,INDEX_ID_NONE);
  String_clear(jobUUID);
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  if (   (INDEX_ID_IS_NONE(uuidId))
      && (INDEX_ID_IS_NONE(storageId))
      && (INDEX_ID_IS_NONE(entityId))
      && String_isEmpty(jobUUID)
      && String_isEmpty(name)
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
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"init list storage fail");
      String_delete(printableStorageName);
      String_delete(storageName);
      Storage_doneSpecifier(&storageSpecifier);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && !isQuit()
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // userName
                                   NULL,  // comment
                                   NULL,  // createdDateTime
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // get printable name (if possible)
        error = Storage_parseName(&storageSpecifier,storageName);
        if (error == ERROR_NONE)
        {
          Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
        }
        else
        {
          String_set(printableStorageName,storageName);
        }

        // purge from index
        error = IndexStorage_purge(indexHandle,
                                   storageId,
                                   NULL  // progressInfo
                                  );
        if (error == ERROR_NONE)
        {
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "storageId=%"PRIu64" name=%'S",
                              storageId,
                              printableStorageName
                             );
        }
        else
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
          Index_doneList(&indexQueryHandle);
          String_delete(name);
          return;
        }

        // remove index id
        SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
        {
          Array_removeAll(&clientInfo->indexIdArray,&storageId,CALLBACK_(NULL,NULL));
        }
      }
    }
    Index_doneList(&indexQueryHandle);

    // free resources
    String_delete(printableStorageName);
    String_delete(storageName);
    Storage_doneSpecifier(&storageSpecifier);
  }

  if (!INDEX_ID_IS_NONE(uuidId))
  {
    // delete UUID index
    error = Index_deleteUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      String_delete(name);
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&uuidId,CALLBACK_(NULL,NULL));
    }
  }

  if (!INDEX_ID_IS_NONE(entityId))
  {
    // delete entity index
    error = Index_deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      String_delete(name);
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&entityId,CALLBACK_(NULL,NULL));
    }
  }

  if (!INDEX_ID_IS_NONE(storageId))
  {
    // purge storage index
    error = IndexStorage_purge(indexHandle,
                               storageId,
                               NULL  // progressInfo
                              );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      String_delete(name);
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&storageId,CALLBACK_(NULL,NULL));
    }
  }

  if (!String_isEmpty(jobUUID))
  {
    // delete all storages which match job UUID
//TODO
  }

  if (!String_isEmpty(name))
  {
    // delete all storages which match name
//TODO
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_restore
* Purpose: restore archives/files
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            type=ARCHIVES|ENTRIES
*            destination=<name>
*            directoryContent=yes|no
*            sparse=yes|no
*            skipSignatures=yes|no
*            restoreEntryMode=STOP|RENAME|OVERWRITE|SKIP_EXISTING
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
    Semaphore  lock;
    bool       skipAllFlag;
    bool       abortFlag;
  } RestoreCommandInfo;

  /***********************************************************************\
  * Name   : parseRestoreType
  * Purpose: parse restore type
  * Input  : name     - name
  *          userData - user data (not used)
  * Output : type - type
  * Return : TRUE iff parsed
  * Notes  : -
  \***********************************************************************/

  auto bool parseRestoreType(const char *name, Types *type, void *userData);
  bool parseRestoreType(const char *name, Types *type, void *userData)
  {
    assert(name != NULL);
    assert(type != NULL);

    UNUSED_VARIABLE(userData);

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
  * Name   : parseRestoreEntryMode
  * Purpose: parse restore entry mode
  * Input  : name     - name
  *          userData - user data (not used)
  * Output : type - type
  * Return : TRUE iff parsed
  * Notes  : -
  \***********************************************************************/

  auto bool parseRestoreEntryMode(const char *name, RestoreEntryModes *restoreEntryMode, void *userData);
  bool parseRestoreEntryMode(const char *name, RestoreEntryModes *restoreEntryMode, void *userData)
  {
    assert(name != NULL);
    assert(restoreEntryMode != NULL);

    UNUSED_VARIABLE(userData);

    if      (stringEquals("STOP",name))
    {
      (*restoreEntryMode) = RESTORE_ENTRY_MODE_STOP;
      return TRUE;
    }
    else if (stringEquals("RENAME",name))
    {
      (*restoreEntryMode) = RESTORE_ENTRY_MODE_RENAME;
      return TRUE;
    }
    else if (stringEquals("OVERWRITE",name))
    {
      (*restoreEntryMode) = RESTORE_ENTRY_MODE_OVERWRITE;
      return TRUE;
    }
    else if (stringEquals("SKIP_EXISTING",name))
    {
      (*restoreEntryMode) = RESTORE_ENTRY_MODE_SKIP_EXISTING;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }

  /***********************************************************************\
  * Name   : restoreRunningInfo
  * Purpose: update restore running info
  * Input  : runningInfo - running info data,
  *          userData    - user data
  * Output : -
  * Return : TRUE to continue, FALSE to abort
  * Notes  : -
  \***********************************************************************/

  auto void restoreRunningInfo(const RunningInfo *runningInfo,
                               void              *userData
                              );
  void restoreRunningInfo(const RunningInfo *runningInfo,
                          void              *userData
                         )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;

    assert(restoreCommandInfo != NULL);
    assert(runningInfo != NULL);
    assert(runningInfo->progress.storage.name != NULL);
    assert(runningInfo->progress.entry.name != NULL);

    ServerIO_sendResult(&restoreCommandInfo->clientInfo->io,
                        restoreCommandInfo->id,
                        FALSE,
                        ERROR_NONE,
// TODO: rename totalEntryCount -> totalCount
// TODO: rename totalEntrySize -> totalSize
                        "state=RUNNING doneCount=%lu doneSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" entryName=%'S entryDoneSize=%"PRIu64" entryTotalSize=%"PRIu64" storageName=%'S storageDoneSize=%"PRIu64" storageTotalSize=%"PRIu64"",
                        runningInfo->progress.done.count,
                        runningInfo->progress.done.size,
                        runningInfo->progress.total.count,
                        runningInfo->progress.total.size,
                        runningInfo->progress.entry.name,
                        runningInfo->progress.entry.doneSize,
                        runningInfo->progress.entry.totalSize,
                        runningInfo->progress.storage.name,
                        runningInfo->progress.storage.doneSize,
                        runningInfo->progress.storage.totalSize
                       );
  }

  /***********************************************************************\
  * Name   : restoreErrorHandler
  * Purpose: restore error handler
  * Input  : storageName - storage name
  *          entryName   - entry name
  *          error       - error code
  *          userData    - user data
  * Output : -
  * Return : ERROR_NONE or old/new error code
  * Notes  : -
  \***********************************************************************/

  auto Errors restoreErrorHandler(ConstString storageName,
                                  ConstString entryName,
                                  Errors      error,
                                  void        *userData
                                 );
  Errors restoreErrorHandler(ConstString storageName,
                             ConstString entryName,
                             Errors      error,
                             void        *userData
                            )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;
    StringMap          resultMap;
    ServerIOActions    action;

    assert(restoreCommandInfo != NULL);
    assert(restoreCommandInfo->clientInfo != NULL);

    ServerIO_sendResult(&restoreCommandInfo->clientInfo->io,
                        restoreCommandInfo->id,
                        FALSE,
                        ERROR_NONE,
                        "state=FAILED errorNumber=%u errorData=%'s storageName=%'S entryName=%'S",
                        Error_getCode(error),
                        Error_getText(error),
                        storageName,
                        entryName
                       );

    SEMAPHORE_LOCKED_DO(&restoreCommandInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if      (   !restoreCommandInfo->skipAllFlag
               && !restoreCommandInfo->abortFlag
              )
      {
        // init variables
        resultMap = StringMap_new();

        // show error
        if (ServerIO_clientAction(&restoreCommandInfo->clientInfo->io,
                                  1*60*MS_PER_SECOND,
                                  resultMap,
                                  "CONFIRM",
                                  "type=RESTORE errorNumber=%d errorData=%'s storageName=%'S entryName=%'S",
                                  Error_getCode(error),
                                  Error_getText(error),
                                  storageName,
                                  entryName
                                 ) != ERROR_NONE
           )
        {
          StringMap_delete(resultMap);
          return error;
        }
        if (!StringMap_getEnum(resultMap,"action",&action,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseAction,NULL),SERVER_IO_ACTION_NONE))
        {
          StringMap_delete(resultMap);
          return error;
        }

        // update state
        switch (action)
        {
          case SERVER_IO_ACTION_NONE:
            error = ERROR_NONE;
            break;
          case SERVER_IO_ACTION_SKIP:
            error = ERROR_NONE;
            break;
          case SERVER_IO_ACTION_SKIP_ALL:
            restoreCommandInfo->skipAllFlag = TRUE;
            error = ERROR_NONE;
            break;
          case SERVER_IO_ACTION_ABORT:
            restoreCommandInfo->abortFlag = TRUE;
            error = ERROR_ABORTED;
            break;
        }

        // free resources
        StringMap_delete(resultMap);
      }
      else if (restoreCommandInfo->abortFlag)
      {
        error = ERROR_ABORTED;
      }
      else
      {
        error = ERROR_NONE;
      }
    }

    return error;
  }

  /***********************************************************************\
  * Name   : getNamePassword
  * Purpose: get name and password
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

  auto Errors getNamePassword(String        name,
                              Password      *password,
                              PasswordTypes passwordType,
                              const char    *text,
                              bool          validateFlag,
                              bool          weakCheckFlag,
                              void          *userData
                             );
  Errors getNamePassword(String        name,
                         Password      *password,
                         PasswordTypes passwordType,
                         const char    *text,
                         bool          validateFlag,
                         bool          weakCheckFlag,
                         void          *userData
                        )
  {
    RestoreCommandInfo   *restoreCommandInfo = (RestoreCommandInfo*)userData;
    StringMap            resultMap;
    Errors               error;
    ServerIOEncryptTypes encryptType;
    String               encryptedPassword;

    assert(password != NULL);
    assert(restoreCommandInfo != NULL);

    UNUSED_VARIABLE(validateFlag);
    UNUSED_VARIABLE(weakCheckFlag);

    // init variables
    resultMap = StringMap_new();

    // request password
    error = ServerIO_clientAction(&restoreCommandInfo->clientInfo->io,
                                  60*MS_PER_S,
                                  resultMap,
                                  "REQUEST_PASSWORD",
                                  "name=%'S passwordType=%'s passwordText=%'s",
                                  name,
                                  getPasswordTypeText(passwordType),
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
    if (!StringMap_getEnum(resultMap,"encryptType",&encryptType,CALLBACK_((StringMapParseEnumFunction)ServerIO_parseEncryptType,NULL),SERVER_IO_ENCRYPT_TYPE_NONE))
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

    // decrypt password
    error = ServerIO_decryptPassword(&clientInfo->io,
                                     password,
                                     encryptType,
                                     encryptedPassword
                                    );
    if (error != ERROR_NONE)
    {
      String_delete(encryptedPassword);
      StringMap_delete(resultMap);
      return error;
    }

    // free resources
    String_delete(encryptedPassword);
    StringMap_delete(resultMap);

    return ERROR_NONE;
  }

  typedef struct RestoreNode
  {
    LIST_NODE_HEADER(struct RestoreNode);

    String  jobUUID;
    String  storageName;
    IndexId entryId;
    String  entryName;
  } RestoreNode;

  typedef struct
  {
    LIST_HEADER(RestoreNode);
  } RestoreList;

  /***********************************************************************\
  * Name   : freeRestoreNode
  * Purpose: free restore node
  * Input  : restoreNode - restore node
  *          userData    - user data
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeRestoreNode(RestoreNode *restoreNode,
                            void          *userData
                           );
  void freeRestoreNode(RestoreNode *restoreNode,
                       void          *userData
                      )
  {
    assert(restoreNode != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(restoreNode->entryName);
    String_delete(restoreNode->storageName);
    String_delete(restoreNode->jobUUID);
  }

  /***********************************************************************\
  * Name   : deleteRestoreNode
  * Purpose: delete restore node
  * Input  : restoreNode - restore node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void deleteRestoreNode(RestoreNode *restoreNode);
  void deleteRestoreNode(RestoreNode *restoreNode)
  {
    assert(restoreNode != NULL);

    freeRestoreNode(restoreNode,NULL);
    LIST_DELETE_NODE(restoreNode);
  }

  Types              type;
  String             destination;
  bool               directoryContentFlag;
  bool               sparseFilesFlag;
  bool               skipVerifySignaturesFlag;
  RestoreEntryModes  restoreEntryMode;
  Errors             error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get type, destination, directory content flag, overwrite flag
  if (!StringMap_getEnum(argumentMap,"type",&type,CALLBACK_((StringMapParseEnumFunction)parseRestoreType,NULL),UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"type=ARCHIVES|ENTRIES");
    return;
  }
  destination = String_new();
  if (!StringMap_getString(argumentMap,"destination",destination,NULL))
  {
    String_delete(destination);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"destination=<name>");
    return;
  }
  StringMap_getBool(argumentMap,"directoryContent",&directoryContentFlag,FALSE);
  StringMap_getBool(argumentMap,"sparse",&sparseFilesFlag,FALSE);
  StringMap_getBool(argumentMap,"skipVerifySignatures",&skipVerifySignaturesFlag,FALSE);
  if (!StringMap_getEnum(argumentMap,"restoreEntryMode",&restoreEntryMode,CALLBACK_((StringMapParseEnumFunction)parseRestoreEntryMode,NULL),RESTORE_ENTRY_MODE_STOP))
  {
    String_delete(destination);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"restoreEntryMode=STOP|RENAME|OVERWRITE|SKIP_EXISTING");
    return;
  }

  // get storage/entry list
  RestoreList restoreList;

  List_init(&restoreList,
            CALLBACK_(NULL,NULL),
            CALLBACK_((ListNodeFreeFunction)freeRestoreNode,NULL)
           );


  error = ERROR_NONE;
  switch (type)
  {
    case ARCHIVES:
      {
        IndexQueryHandle indexQueryHandle;
        error = Index_initListStorages(&indexQueryHandle,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidId
                                       INDEX_ID_ANY,  // entityId
                                       NULL,  // jobUUID
                                       NULL,  // scheduleUUID,
                                       Array_cArray(&clientInfo->indexIdArray),
                                       Array_length(&clientInfo->indexIdArray),
                                       INDEX_TYPESET_ALL,
                                       INDEX_STATE_SET_ALL,
                                       INDEX_MODE_SET_ALL,
                                       NULL,  // hostName
                                       NULL,  // userName
                                       NULL,  // name
                                       INDEX_STORAGE_SORT_MODE_NONE,
                                       DATABASE_ORDERING_NONE,
                                       0LL,  // offset
                                       INDEX_UNLIMITED
                                      );
        if (error == ERROR_NONE)
        {
          StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
          String storageName = String_new();
          while (   !isCommandAborted(clientInfo,id)
                 && !isQuit()
                 && Index_getNextStorage(&indexQueryHandle,
                                         NULL,  // uuidId
                                         jobUUID,
                                         NULL,  // entityId
                                         NULL,  // scheduleUUID
                                         NULL,  // hostName
                                         NULL,  // userName
                                         NULL,  // comment
                                         NULL,  // createdDateTime
                                         NULL,  // archiveType
                                         NULL,  // storageId
                                         storageName,
                                         NULL,  // createdDateTime
                                         NULL,  // size
                                         NULL,  // indexState
                                         NULL,  // indexMode,
                                         NULL,  // lastCheckedDateTime,
                                         NULL,  // errorMessage,
                                         NULL,  // totalEntryCount
                                         NULL  // totalEntrySize
                                        )
                )
          {
            RestoreNode *restoreNode;
            if (!LIST_CONTAINS(&restoreList,
                               restoreNode,
                                  String_equals(restoreNode->jobUUID,jobUUID)
                               && String_equals(restoreNode->storageName,storageName)
                              )
               )
            {
              restoreNode = LIST_NEW_NODE(RestoreNode);
              if (restoreNode == NULL)
              {
                HALT_INSUFFICIENT_MEMORY();
              }
              restoreNode->jobUUID     = String_duplicate(jobUUID);
              restoreNode->storageName = String_duplicate(storageName);
              restoreNode->entryId     = INDEX_ID_NONE;
              restoreNode->entryName   = NULL;
              List_append(&restoreList,restoreNode);
            }
          }
          String_delete(storageName);
          Index_doneList(&indexQueryHandle);
        }
      }
      break;
    case ENTRIES:
      {
        IndexQueryHandle indexQueryHandle1,indexQueryHandle2;
        error = Index_initListEntries(&indexQueryHandle1,
                                      indexHandle,
                                      NULL, // indexIds
                                      0, // indexIdCount
                                      Array_cArray(&clientInfo->entryIdArray),
                                      Array_length(&clientInfo->entryIdArray),
                                      INDEX_TYPE_ANY,
                                      NULL, // name
                                      FALSE,  // newestOnly,
                                      FALSE,  //fragments
                                      INDEX_ENTRY_SORT_MODE_NONE,
                                      DATABASE_ORDERING_NONE,
                                      0,
                                      INDEX_UNLIMITED
                                     );
        if (error == ERROR_NONE)
        {
          StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
          IndexId entryId;
          String  storageName = String_new();
          String  entryName   = String_new();
          while (   !isCommandAborted(clientInfo,id)
                 && !isQuit()
                 && (error == ERROR_NONE)
                 && Index_getNextEntry(&indexQueryHandle1,
                                       NULL,  // uuidId
                                       jobUUID,
                                       NULL,  // entityId
                                       NULL,  // scheduleUUID,
                                       NULL,  // hostName
                                       NULL,  // userName
                                       NULL,  // archiveType,
                                       &entryId,
                                       entryName,
                                       NULL,  // storageId
                                       storageName,
                                       NULL,  // size
                                       NULL,  // timeModified
                                       NULL,  // userId
                                       NULL,  // groupId
                                       NULL,  // permission
                                       NULL,  // fragmentCount
                                       NULL,  // destinationName
                                       NULL,  // fileSystemType
                                       NULL  // blockSize
                                      )
                )
          {
            if (!String_isEmpty(storageName))
            {
              RestoreNode *restoreNode;
              if (!LIST_CONTAINS(&restoreList,
                                 restoreNode,
                                    String_equals(restoreNode->jobUUID,jobUUID)
                                 && String_equals(restoreNode->storageName,storageName)
                                 && String_equals(restoreNode->entryName,entryName)
                                )
                 )
              {
                restoreNode = LIST_NEW_NODE(RestoreNode);
                if (restoreNode == NULL)
                {
                  HALT_INSUFFICIENT_MEMORY();
                }
                restoreNode->jobUUID     = String_duplicate(jobUUID);
                restoreNode->storageName = String_duplicate(storageName);
                restoreNode->entryId     = entryId;
                restoreNode->entryName   = String_duplicate(entryName);
                List_append(&restoreList,restoreNode);
              }
            }

            if (   (INDEX_TYPE(entryId) == INDEX_TYPE_FILE)
                || (INDEX_TYPE(entryId) == INDEX_TYPE_IMAGE)
                || (INDEX_TYPE(entryId) == INDEX_TYPE_HARDLINK)
               )
            {
              error = Index_initListEntryFragments(&indexQueryHandle2,
                                                   indexHandle,
                                                   entryId,
                                                   0,
                                                   INDEX_UNLIMITED
                                                  );
              if (error == ERROR_NONE)
              {
                while (   !isCommandAborted(clientInfo,id)
                       && !isQuit()
                       && Index_getNextEntryFragment(&indexQueryHandle2,
                                                     NULL,  // entryFragmentId,
                                                     NULL,  // storageId,
                                                     storageName,
                                                     NULL,  // storageDateTime
                                                     NULL,  // fragmentOffset
                                                     NULL  // fragmentSize
                                                    )
                      )
                {
                  RestoreNode *restoreNode;
                  if (!LIST_CONTAINS(&restoreList,
                                     restoreNode,
                                        String_equals(restoreNode->jobUUID,jobUUID)
                                     && String_equals(restoreNode->storageName,storageName)
                                     && String_equals(restoreNode->entryName,entryName)
                                    )
                     )
                  {
                    restoreNode = LIST_NEW_NODE(RestoreNode);
                    if (restoreNode == NULL)
                    {
                      HALT_INSUFFICIENT_MEMORY();
                    }
                    restoreNode->jobUUID     = String_duplicate(jobUUID);
                    restoreNode->storageName = String_duplicate(storageName);
                    restoreNode->entryName   = String_duplicate(entryName);
                    List_append(&restoreList,restoreNode);
                  }
                }
                Index_doneList(&indexQueryHandle2);
              }
            }
          }
          String_delete(entryName);
          String_delete(storageName);
          Index_doneList(&indexQueryHandle1);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  if (List_isEmpty(&restoreList))
  {
    error = ERROR_ENTRY_NOT_FOUND;
  }

  // init variables
  char byName[256];
  getClientInfoString(clientInfo,byName,sizeof(byName));

  // restore
  RestoreCommandInfo restoreCommandInfo;
  restoreCommandInfo.clientInfo  = clientInfo;
  restoreCommandInfo.id          = id;
  Semaphore_init(&restoreCommandInfo.lock,SEMAPHORE_TYPE_BINARY);
  restoreCommandInfo.skipAllFlag = FALSE;
  restoreCommandInfo.abortFlag   = FALSE;

  StringList storageNameList;
  EntryList  includeEntryList;
  StringList_init(&storageNameList);
  EntryList_init(&includeEntryList);
  String entryName = String_new();
  while (   !List_isEmpty(&restoreList)
         && (error == ERROR_NONE)
        )
  {
    RestoreNode *restoreNode;

    // get storages/entries from job to restore
    StringList_clear(&storageNameList);
    EntryList_clear(&includeEntryList);
    StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
    String_set(jobUUID,LIST_HEAD(&restoreList)->jobUUID);
    while (   !List_isEmpty(&restoreList)
           && String_equals(jobUUID,LIST_HEAD(&restoreList)->jobUUID)
          )
    {
      restoreNode = (RestoreNode*)List_removeFirst(&restoreList);
      if (!StringList_contains(&storageNameList,restoreNode->storageName))
      {
        StringList_append(&storageNameList,restoreNode->storageName);
      }
      if (restoreNode->entryName != NULL)
      {
        if (!EntryList_contains(&includeEntryList,ENTRY_TYPE_FILE,restoreNode->entryName,PATTERN_TYPE_GLOB))
        {
          EntryList_append(&includeEntryList,ENTRY_TYPE_FILE,restoreNode->entryName,PATTERN_TYPE_GLOB,NULL);
        }
        if (directoryContentFlag && (INDEX_TYPE(restoreNode->entryId) == INDEX_TYPE_DIRECTORY))
        {
          String_appendCString(String_set(entryName,restoreNode->entryName),"/*");
          if (!EntryList_contains(&includeEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB))
          {
            EntryList_append(&includeEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB,NULL);
          }
        }
      }
      deleteRestoreNode(restoreNode);
    }

    // find job name, job options (if possible)
    String     jobName = String_new();
    JobOptions jobOptions;
    Job_getOptions(jobName,&jobOptions,jobUUID);
    String_set(jobOptions.destination,destination);
    jobOptions.sparseFilesFlag          = sparseFilesFlag;
    jobOptions.skipVerifySignaturesFlag = skipVerifySignaturesFlag;
    jobOptions.restoreEntryMode         = restoreEntryMode;

    // restore
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Start restore archives/entries from job '%s' by %s",
               String_cString(jobName),
               byName
              );

#if 0
StringNode *n;
String s;
fprintf(stderr,"%s:%d: s %d\n",__FILE__,__LINE__,StringList_count(&storageNameList));
STRINGLIST_ITERATE(&storageNameList,n,s)
{
  fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(s));
}
EntryNode *e;
LIST_ITERATE(&includeEntryList,e)
{
  fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(e->string));
}
#endif
    error = Command_restore(&storageNameList,
                            &includeEntryList,
                            NULL,  // excludePatternList
                            &jobOptions,
                            CALLBACK_(restoreRunningInfo,&restoreCommandInfo),
                            CALLBACK_(restoreErrorHandler,&restoreCommandInfo),
                            CALLBACK_(getNamePassword,&restoreCommandInfo),
                            CALLBACK_(NULL,NULL),  // isPause callback
                            CALLBACK_INLINE(bool,(void *userData),
                            {
                              UNUSED_VARIABLE(userData);
                              return isCommandAborted(clientInfo,id);
                            },NULL),
                            NULL  // logHandle
                           );
    if (error == ERROR_NONE)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Done restore archives/entries from job '%s' by %s",
                 String_cString(jobName),
                 byName
                );
    }
    else
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Restore from job '%s' failed (error: %s)",
                 String_cString(jobName),
                 Error_getText(error)
                );
    }

    Job_doneOptions(&jobOptions);
  }
  String_delete(entryName);
  EntryList_done(&includeEntryList);
  StringList_done(&storageNameList);
  Semaphore_done(&restoreCommandInfo.lock);

  // free resources
  List_done(&restoreList);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");

  // free resources
  String_delete(destination);
}

/***********************************************************************\
* Name   : serverCommand_restoreContinue
* Purpose: continue restore archives/files
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_restoreContinue(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : serverCommand_debugPrintStatistics
* Purpose: print array/string/file statistics info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugPrintMemoryInfo
* Purpose: print array/string debug info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

                                         ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=ARRAY n=%lu count=%lu",n,count);

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

                                          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=STRING n=%lu count=%lu",n,count);

                                          return TRUE;
                                        },
                                        NULL
                                       ),
                                       DUMP_INFO_TYPE_ALLOCATED|DUMP_INFO_TYPE_HISTOGRAM
                       );
  File_debugPrintInfo(CALLBACK_INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                      {
                                        UNUSED_VARIABLE(fileHandle);
                                        UNUSED_VARIABLE(fileName);
                                        UNUSED_VARIABLE(lineNb);
                                        UNUSED_VARIABLE(userData);

                                        ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=FILE n=%lu count=%lu",n,count);

                                        return TRUE;
                                      },
                                      NULL
                                     )
                     );

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugDumpMemoryInfo
* Purpose: dump array/string/file debug info into file "bar-memory.dump"
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_CREATE_FILE,"Cannot create 'bar-memory.dump'");
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

                                        ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=ARRAY n=%lu count=%lu",n,count);

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

                                         ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=STRING n=%lu count=%lu",n,count);

                                         return TRUE;
                                       },
                                       NULL
                                      ),
                                      DUMP_INFO_TYPE_HISTOGRAM
                     );
  File_debugDumpInfo(handle,
                     CALLBACK_INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                     {
                                       UNUSED_VARIABLE(fileHandle);
                                       UNUSED_VARIABLE(fileName);
                                       UNUSED_VARIABLE(lineNb);
                                       UNUSED_VARIABLE(userData);

                                       ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=FILE n=%lu count=%lu",n,count);

                                       return TRUE;
                                     },
                                     NULL
                                    )
                    );
  debugResourceDumpInfo(handle,
                        CALLBACK_INLINE(bool,(const char *variableName, const void *resource, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                        {
                                          UNUSED_VARIABLE(variableName);
                                          UNUSED_VARIABLE(resource);
                                          UNUSED_VARIABLE(fileName);
                                          UNUSED_VARIABLE(lineNb);
                                          UNUSED_VARIABLE(userData);

                                          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"type=RESOURCE n=%lu count=%lu",n,count);

                                          return TRUE;
                                        },
                                        NULL
                                       ),
                                       DUMP_INFO_TYPE_HISTOGRAM
                      );
  fclose(handle);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

void serverDumpMemoryInfo(void);
void serverDumpMemoryInfo(void)
{
  FILE *handle;

  handle = fopen("bar-memory.dump","w");
  if (handle == NULL)
  {
    fprintf(stderr,"%s, %d: Cannot open file (error: %s)\n",__FILE__,__LINE__,strerror(errno));
    return;
  }
  Array_debugDumpInfo(handle,CALLBACK_(NULL,NULL));
  String_debugDumpInfo(handle,CALLBACK_(NULL,NULL),DUMP_INFO_TYPE_HISTOGRAM);
  File_debugDumpInfo(handle,CALLBACK_(NULL,NULL));
  debugResourceDumpInfo(handle,CALLBACK_(NULL,NULL),DUMP_INFO_TYPE_HISTOGRAM);
  fclose(handle);
}

#endif /* NDEBUG */

// server commands
const struct
{
  const char            *name;
  ServerCommandFunction serverCommandFunction;
  AuthorizationStateSet authorizationStateSet;
}
SERVER_COMMANDS[] =
{
  { "ERROR_INFO",                  serverCommand_errorInfo,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "START_TLS",                   serverCommand_startTLS,                 AUTHORIZATION_STATE_WAITING                           },
  { "AUTHORIZE",                   serverCommand_authorize,                AUTHORIZATION_STATE_WAITING                           },
  { "VERSION",                     serverCommand_version,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "QUIT",                        serverCommand_quit,                     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ACTION_RESULT",               serverCommand_actionResult,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "SERVER_OPTION_GET",           serverCommand_serverOptionGet,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_OPTION_SET",           serverCommand_serverOptionSet,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_OPTION_FLUSH",         serverCommand_serverOptionFlush,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "MASTER_GET",                  serverCommand_masterGet,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
// TODO: remove
//  { "MASTER_WAIT",                 serverCommand_masterWait,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
//  { "MASTER_SET",                  serverCommand_masterSet,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MASTER_CLEAR",                serverCommand_masterClear,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MASTER_PAIRING_START",        serverCommand_masterPairingStart,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MASTER_PAIRING_STOP",         serverCommand_masterPairingStop,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MASTER_PAIRING_STATUS",       serverCommand_masterPairingStatus,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "MAINTENANCE_LIST",            serverCommand_maintenanceList,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MAINTENANCE_LIST_ADD",        serverCommand_maintenanceListAdd,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MAINTENANCE_LIST_UPDATE",     serverCommand_maintenanceListUpdate,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MAINTENANCE_LIST_REMOVE",     serverCommand_maintenanceListRemove,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "SERVER_LIST",                 serverCommand_serverList,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_ADD",             serverCommand_serverListAdd,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_UPDATE",          serverCommand_serverListUpdate,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_REMOVE",          serverCommand_serverListRemove,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

//TODO: GET obsolete?
  { "GET",                         serverCommand_get,                      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ABORT",                       serverCommand_abort,                    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STATUS",                      serverCommand_status,                   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PAUSE",                       serverCommand_pause,                    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SUSPEND",                     serverCommand_suspend,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "CONTINUE",                    serverCommand_continue,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MAINTENANCE",                 serverCommand_maintenance,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEVICE_LIST",                 serverCommand_deviceList,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ROOT_LIST",                   serverCommand_rootList,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_INFO",                   serverCommand_fileInfo,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_LIST",                   serverCommand_fileList,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_GET",          serverCommand_fileAttributeGet,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_SET",          serverCommand_fileAttributeSet,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_CLEAR",        serverCommand_fileAttributeClear,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_MKDIR",                  serverCommand_fileMkdir,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_DELETE",                 serverCommand_fileDelete,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DIRECTORY_INFO",              serverCommand_directoryInfo,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "TEST_SCRIPT",                 serverCommand_testScript,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "JOB_LIST",                    serverCommand_jobList,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_INFO",                    serverCommand_jobInfo,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_START",                   serverCommand_jobStart,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_ABORT",                   serverCommand_jobAbort,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_RESET",                   serverCommand_jobReset,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_NEW",                     serverCommand_jobNew,                   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_CLONE",                   serverCommand_jobClone,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_RENAME",                  serverCommand_jobRename,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_DELETE",                  serverCommand_jobDelete,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_FLUSH",                   serverCommand_jobFlush,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_OPTION_GET",              serverCommand_jobOptionGet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_OPTION_SET",              serverCommand_jobOptionSet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
// TODO: remove
  { "JOB_OPTION_DELETE",           serverCommand_jobOptionDelete,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "JOB_STATUS",                  serverCommand_jobStatus,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INCLUDE_LIST",                serverCommand_includeList,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INCLUDE_LIST_CLEAR",          serverCommand_includeListClear,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INCLUDE_LIST_ADD",            serverCommand_includeListAdd,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INCLUDE_LIST_UPDATE",         serverCommand_includeListUpdate,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INCLUDE_LIST_REMOVE",         serverCommand_includeListRemove,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "MOUNT_LIST",                  serverCommand_mountList,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MOUNT_LIST_CLEAR",            serverCommand_mountListClear,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MOUNT_LIST_ADD",              serverCommand_mountListAdd,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MOUNT_LIST_UPDATE",           serverCommand_mountListUpdate,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "MOUNT_LIST_REMOVE",           serverCommand_mountListRemove,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "EXCLUDE_LIST",                serverCommand_excludeList,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_LIST_CLEAR",          serverCommand_excludeListClear,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_LIST_ADD",            serverCommand_excludeListAdd,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_LIST_UPDATE",         serverCommand_excludeListUpdate,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_LIST_REMOVE",         serverCommand_excludeListRemove,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "SOURCE_LIST",                 serverCommand_sourceList,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SOURCE_LIST_CLEAR",           serverCommand_sourceListClear,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SOURCE_LIST_ADD",             serverCommand_sourceListAdd,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SOURCE_LIST_UPDATE",          serverCommand_sourceListUpdate,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SOURCE_LIST_REMOVE",          serverCommand_sourceListRemove,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "EXCLUDE_COMPRESS_LIST",       serverCommand_excludeCompressList,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_COMPRESS_LIST_CLEAR", serverCommand_excludeCompressListClear, AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_COMPRESS_LIST_ADD",   serverCommand_excludeCompressListAdd,   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_COMPRESS_LIST_UPDATE",serverCommand_excludeCompressListUpdate,AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "EXCLUDE_COMPRESS_LIST_REMOVE",serverCommand_excludeCompressListRemove,AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "SCHEDULE_LIST",               serverCommand_scheduleList,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_LIST_CLEAR",         serverCommand_scheduleListClear,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_LIST_ADD",           serverCommand_scheduleListAdd,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_LIST_REMOVE",        serverCommand_scheduleListRemove,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_OPTION_GET",         serverCommand_scheduleOptionGet,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_OPTION_SET",         serverCommand_scheduleOptionSet,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
// TODO: remove
  { "SCHEDULE_OPTION_DELETE",      serverCommand_scheduleOptionDelete,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_TRIGGER",            serverCommand_scheduleTrigger,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "PERSISTENCE_LIST",            serverCommand_persistenceList,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_CLEAR",      serverCommand_persistenceListClear,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_ADD",        serverCommand_persistenceListAdd,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_UPDATE",     serverCommand_persistenceListUpdate,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_REMOVE",     serverCommand_persistenceListRemove,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "DECRYPT_PASSWORD_ADD",        serverCommand_decryptPasswordAdd,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FTP_PASSWORD",                serverCommand_ftpPassword,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SSH_PASSWORD",                serverCommand_sshPassword,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "WEBDAV_PASSWORD",             serverCommand_webdavPassword,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "CRYPT_PASSWORD",              serverCommand_cryptPassword,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PASSWORDS_CLEAR",             serverCommand_passwordsClear,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "VOLUME_LOAD",                 serverCommand_volumeLoad,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "VOLUME_UNLOAD",               serverCommand_volumeUnload,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "ARCHIVE_LIST",                serverCommand_archiveList,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "ENTITY_MOVE_TO",              serverCommand_entityMoveTo,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "STORAGE_TEST",                serverCommand_storageTest,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STORAGE_DELETE",              serverCommand_storageDelete,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_INFO",                  serverCommand_indexInfo,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_UUID_LIST",             serverCommand_indexUUIDList,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTITY_LIST",           serverCommand_indexEntityList,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST",          serverCommand_indexStorageList,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST_CLEAR",    serverCommand_indexStorageListClear,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST_ADD",      serverCommand_indexStorageListAdd,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST_REMOVE",   serverCommand_indexStorageListRemove,   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST_INFO",     serverCommand_indexStorageListInfo,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST",            serverCommand_indexEntryList,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST_CLEAR",      serverCommand_indexEntryListClear,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST_ADD",        serverCommand_indexEntryListAdd,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST_REMOVE",     serverCommand_indexEntryListRemove,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST_INFO",       serverCommand_indexEntryListInfo,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_FRAGMENT_LIST",   serverCommand_indexEntryFragmentList,   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_HISTORY_LIST",          serverCommand_indexHistoryList,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_ENTITY_ADD",            serverCommand_indexEntityAdd,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_STORAGE_ADD",           serverCommand_indexStorageAdd,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_ASSIGN",                serverCommand_indexAssign,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_REFRESH",               serverCommand_indexRefresh,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_REMOVE",                serverCommand_indexRemove,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "RESTORE",                     serverCommand_restore,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "RESTORE_CONTINUE",            serverCommand_restoreContinue,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  // obsolete
  { "OPTION_GET",                  serverCommand_jobOptionGet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "OPTION_SET",                  serverCommand_jobOptionSet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
// TODO: remove
  { "OPTION_DELETE",               serverCommand_jobOptionDelete,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  #ifndef NDEBUG
  { "DEBUG_PRINT_STATISTICS",      serverCommand_debugPrintStatistics,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEBUG_PRINT_MEMORY_INFO",     serverCommand_debugPrintMemoryInfo,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEBUG_DUMP_MEMORY_INFO",      serverCommand_debugDumpMemoryInfo,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  #endif /* NDEBUG */
};

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : freeCommandInfo
* Purpose: free command info
* Input  : commandInfo - command info
*          userData    - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommandInfo(CommandInfoNode *commandInfoNode, void *userData)
{
  assert(commandInfoNode != NULL);

  UNUSED_VARIABLE(commandInfoNode);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : freeCommand
* Purpose: free command
* Input  : command  - command
*          userData - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommand(Command *command, void *userData)
{
  assert(command != NULL);

  UNUSED_VARIABLE(userData);

  StringMap_delete(command->argumentMap);
}

/***********************************************************************\
* Name   : findCommand
* Purpose: find command
* Input  : name - command name
* Output : serverCommandFunction - server command function
*          authorizationStateSet - required authorization state set
* Return : TRUE if command found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findCommand(ConstString           name,
                       ServerCommandFunction *serverCommandFunction,
                       AuthorizationStateSet *authorizationStateSet
                      )
{
  uint i;

  assert(name != NULL);
  assert(serverCommandFunction != NULL);
  assert(authorizationStateSet != NULL);

  // find command by name
  i = 0;
  while ((i < SIZE_OF_ARRAY(SERVER_COMMANDS)) && !String_equalsCString(name,SERVER_COMMANDS[i].name))
  {
    i++;
  }
  if (i >= SIZE_OF_ARRAY(SERVER_COMMANDS))
  {
    return FALSE;
  }
  (*serverCommandFunction) = SERVER_COMMANDS[i].serverCommandFunction;
  (*authorizationStateSet) = SERVER_COMMANDS[i].authorizationStateSet;

  return TRUE;
}

/***********************************************************************\
* Name   : putCommand
* Purpose: put command into queue for asynchronous execution
* Input  : clientInfo            - client info
*          serverCommandFunction - server command function
*          authorizationStateSet - required authorization state set
*          id                    - command id
*          argumentMap           - argument map
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void putCommand(ClientInfo            *clientInfo,
                      ServerCommandFunction serverCommandFunction,
                      AuthorizationStateSet authorizationStateSet,
                      uint                  id,
                      const StringMap       argumentMap
                     )
{
  Command command;

  assert(clientInfo != NULL);
  assert(serverCommandFunction != NULL);
  assert(argumentMap != NULL);

  command.serverCommandFunction = serverCommandFunction;
  command.authorizationStateSet = authorizationStateSet;
  command.id                    = id;
  command.argumentMap           = StringMap_duplicate(argumentMap);
  (void)MsgQueue_put(&clientInfo->commandQueue,&command,sizeof(Command));
}

//TODO: required?
#if 0
/***********************************************************************\
* Name   : getCommand
* Purpose: get next command to execute
* Input  : clientInfo - client info
* Output : serverCommandFunction - server command function
*          authorizationState    - required authorization state
*          id                    - command id
*          argumentMap           - argument map
* Return : TRUE if got command,FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getCommand(ClientInfo            *clientInfo,
                      ServerCommandFunction *serverCommandFunction,
                      AuthorizationStates   *authorizationState,
                      uint                  *id,
                      StringMap             argumentMap
                     )
{
  Command command;

  assert(clientInfo != NULL);
  assert(serverCommandFunction != NULL);
  assert(id != NULL);
  assert(argumentMap != NULL);

  if (MsgQueue_get(&clientInfo->commandQueue,&command,NULL,sizeof(command),WAIT_FOREVER))
  {
    (*serverCommandFunction) = command.serverCommandFunction;
    (*authorizationState   ) = command.authorizationState;
    (*id                   ) = command.id;
    StringMap_move(argumentMap,command.argumentMap);
    StringMap_done(command.argumentMap);

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
#endif

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
  IndexHandle     indexHandle;
  bool            isIndexOpen;
  CommandInfoNode *commandInfoNode;
  String          result;
  Command         command;
  #ifndef NDEBUG
    uint64 t0,t1;
  #endif /* not NDEBUG */

  assert(clientInfo != NULL);

  // init variables
  result = String_new();

  // try to open index
  if (Index_isAvailable())
  {
    isIndexOpen = (Index_open(&indexHandle,NULL,CLIENT_TIMEOUT) == ERROR_NONE);
  }
  else
  {
    isIndexOpen = FALSE;
  }

  // run client thread
  while (   !clientInfo->quitFlag
         && MsgQueue_get(&clientInfo->commandQueue,&command,NULL,sizeof(command),WAIT_FOREVER)
        )
  {
    // check authorization (if not in server debug mode)
    #ifndef NDEBUG
      if ((globalOptions.debug.serverLevel >= 1) || IS_SET(command.authorizationStateSet,clientInfo->authorizationState))
    #else
      if (IS_SET(command.authorizationStateSet,clientInfo->authorizationState))
    #endif
    {
      // try to open index if not already oppend
      if (Index_isAvailable() && !isIndexOpen)
      {
        isIndexOpen = (Index_open(&indexHandle,NULL,10*MS_PER_SECOND) == ERROR_NONE);
      }

      // add command info
      commandInfoNode = NULL;
      SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        commandInfoNode = LIST_NEW_NODE(CommandInfoNode);
        if (commandInfoNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        commandInfoNode->id          = command.id;
        commandInfoNode->indexHandle = Index_isAvailable() ? &indexHandle : NULL;
        List_append(&clientInfo->commandInfoList,commandInfoNode);
      }

      // execute command
      #ifndef NDEBUG
        t0 = 0LL;
        if (globalOptions.debug.serverLevel >= 1)
        {
          t0 = Misc_getTimestamp();
        }
      #endif /* not NDEBUG */
      command.serverCommandFunction(clientInfo,
                                    (Index_isAvailable() && isIndexOpen) ? &indexHandle : NULL,
                                    command.id,
                                    command.argumentMap
                                   );
      #ifndef NDEBUG
        if (globalOptions.debug.serverLevel >= 2)
        {
          t1 = Misc_getTimestamp();
          fprintf(stderr,"DEBUG: command time=%"PRIu64"ms\n",(t1-t0)/(uint64)US_PER_MS);
        }
      #endif /* not DEBUG */

      // remove command info
      SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        List_removeAndFree(&clientInfo->commandInfoList,commandInfoNode);
      }
    }
    else
    {
      // authorization failure -> mark for disconnect
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
      ServerIO_sendResult(&clientInfo->io,command.id,TRUE,ERROR_DATABASE_AUTHORIZATION,"authorization failure");
    }

    // free resources
    freeCommand(&command,NULL);
  }

  // close index
  if (Index_isAvailable() && isIndexOpen)
  {
    Index_close(&indexHandle);
  }

  // free resources
  String_delete(result);
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : initNetworkClient
* Purpose: init network client
* Input  : clientInfo         - client info
*          serverSocketHandle - server socket handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initNetworkClient(ClientInfo               *clientInfo,
                               const ServerSocketHandle *serverSocketHandle
                              )
{
  Errors error;
  uint   i;

  assert(clientInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientInfo);
  assert(serverSocketHandle != NULL);

  // connect network server i/o
  error = ServerIO_acceptNetwork(&clientInfo->io,
                                 serverSocketHandle
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // init client command threads
  if (!MsgQueue_init(&clientInfo->commandQueue,
                     0,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeCommand,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize client command message queue!");
  }
  for (i = 0; i < MAX_NETWORK_CLIENT_THREADS; i++)
  {
    clientInfo->threads[i] = ThreadPool_run(&clientThreadPool,networkClientThreadCode,clientInfo);
    if (clientInfo->threads[i] == NULL)
    {
      HALT_FATAL_ERROR("Cannot initialize client thread!");
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneNetworkClient
* Purpose: deinitialize network client
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneNetworkClient(ClientInfo *clientInfo)
{
  int i;

  assert(clientInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientInfo);

  // server i/o end
  ServerIO_setEnd(&clientInfo->io);

  // stop client command threads
  MsgQueue_setEndOfMsg(&clientInfo->commandQueue);
  for (i = MAX_NETWORK_CLIENT_THREADS-1; i >= 0; i--)
  {
    if (!ThreadPool_join(&clientThreadPool,clientInfo->threads[i]))
    {
      HALT_INTERNAL_ERROR("Cannot stop command threads!");
    }
  }

  // free resources
  MsgQueue_done(&clientInfo->commandQueue);

  // disconnect
  ServerIO_disconnect(&clientInfo->io);
}

/***********************************************************************\
* Name   : initBatchClient
* Purpose: create batch client with file i/o
* Input  : clientInfo       - client info to initialize
*          inputDescriptor  - client input file descriptor
*          outputDescriptor - client output file descriptor
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initBatchClient(ClientInfo *clientInfo,
                             int        inputDescriptor,
                             int        outputDescriptor
                            )
{
  Errors error;

  assert(clientInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientInfo);

  // connect batch server i/o
  error = ServerIO_connectBatch(&clientInfo->io,
                                inputDescriptor,
                                outputDescriptor
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // batch client do not require authorization
  clientInfo->authorizationState = AUTHORIZATION_STATE_CLIENT;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneBatchClient
* Purpose: deinitialize batch client
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneBatchClient(ClientInfo *clientInfo)
{
  assert(clientInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientInfo);

  // disconnect
  ServerIO_disconnect(&clientInfo->io);
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
  Semaphore_init(&clientInfo->lock,SEMAPHORE_TYPE_BINARY);

  clientInfo->authorizationState    = AUTHORIZATION_STATE_WAITING;
  clientInfo->authorizationFailNode = NULL;

  clientInfo->quitFlag              = FALSE;

  List_init(&clientInfo->commandInfoList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeCommandInfo,NULL));
  if (!RingBuffer_init(&clientInfo->abortedCommandIds,sizeof(uint),MAX_ABORT_COMMAND_IDS))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  clientInfo->abortedCommandIdStart = 0;

  EntryList_init(&clientInfo->includeEntryList);
  PatternList_init(&clientInfo->excludePatternList);
  Job_initOptions(&clientInfo->jobOptions);
  List_init(&clientInfo->directoryInfoList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeDirectoryInfoNode,NULL));
  Array_init(&clientInfo->indexIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&clientInfo->entryIdArray,sizeof(IndexId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  DEBUG_ADD_RESOURCE_TRACE(clientInfo,ClientInfo);
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
  JobNode         *jobNode;
  CommandInfoNode *commandInfoNode;

  assert(clientInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientInfo);

  // signal quit
  clientInfo->quitFlag = TRUE;

  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // abort all index commands
    LIST_ITERATE(&clientInfo->commandInfoList,commandInfoNode)
    {
      if (commandInfoNode->indexHandle != NULL)
      {
        Index_interrupt(commandInfoNode->indexHandle);
      }
    }

    // wait for commands done
    while (!List_isEmpty(&clientInfo->commandInfoList))
    {
      Semaphore_waitModified(&clientInfo->lock,500);
    }
  }

  // abort all running master jobs
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      if (jobNode->masterIO == &clientInfo->io)
      {
        Job_abort(jobNode,NULL);
        jobNode->masterIO = NULL;
      }
    }
  }

  // done client
  switch (clientInfo->io.type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_BATCH:
      doneBatchClient(clientInfo);
      break;
    case SERVER_IO_TYPE_NETWORK:
      doneNetworkClient(clientInfo);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  DEBUG_REMOVE_RESOURCE_TRACE(clientInfo,ClientInfo);

  // free resources
  Array_done(&clientInfo->entryIdArray);
  Array_done(&clientInfo->indexIdArray);
  List_done(&clientInfo->directoryInfoList);
  Job_doneOptions(&clientInfo->jobOptions);
  PatternList_done(&clientInfo->excludePatternList);
  EntryList_done(&clientInfo->includeEntryList);

  RingBuffer_done(&clientInfo->abortedCommandIds,CALLBACK_(NULL,NULL));
  List_done(&clientInfo->commandInfoList);
  Semaphore_done(&clientInfo->lock);
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

  DEBUG_ADD_RESOURCE_TRACE(clientNode,ClientNode);

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

  DEBUG_REMOVE_RESOURCE_TRACE(clientNode,ClientNode);

  freeClientNode(clientNode,NULL);
  LIST_DELETE_NODE(clientNode);
}

/***********************************************************************\
* Name   : newNetworkClient
* Purpose: create new network client
* Input  : serverSocketHandle - server socket handle
* Output : clientNode - new client node
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors newNetworkClient(ClientNode               **clientNode,
                              const ServerSocketHandle *serverSocketHandle
                             )
{
  Errors error;

  assert(clientNode != NULL);
  assert(serverSocketHandle != NULL);

  // create new client
  (*clientNode) = newClient();
  assert((*clientNode) != NULL);

  // initialize client
  initClient(&(*clientNode)->clientInfo);

  // init network client
  error = initNetworkClient(&(*clientNode)->clientInfo,serverSocketHandle);
  if (error != ERROR_NONE)
  {
    deleteClient(*clientNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : rejectNetworkClient
* Purpose: reject network client
* Input  : serverSocketHandle - server socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void rejectNetworkClient(const ServerSocketHandle *serverSocketHandle)
{
  ServerIO_rejectNetwork(serverSocketHandle);
}

#if 0
/***********************************************************************\
* Name   : findClient
* Purpose: find client by name, port
* Input  : slaveHost - slave host
* Output : -
* Return : client info or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL ClientInfo *findClient(const SlaveHost *slaveHost)
{
  ClientNode *clientNode;

  assert(slaveHost != NULL);
  assert(Semaphore_isLocked(&clientList.lock));

  clientNode = LIST_FIND(&clientList,
                         clientNode,
                            (clientNode->clientInfo.type == SERVER_IO_TYPE_NETWORK)
                         && (clientNode->clientInfo.io.network.port == slaveHost->port)
                         && String_equals(clientNode->clientInfo.io.network.name,slaveHost->name)
                        );

  return (clientNode != NULL) ? &clientNode->clientInfo : NULL;
}
#endif

/***********************************************************************\
* Name   : purgeNetworkClient
* Purpose: find client by name, port
* Input  : slaveHost - slave host
* Output : -
* Return : client info or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL void purgeNetworkClient(ClientList *clientList)
{
  ClientNode *clientNode;
  char       s[256];

  assert(clientList != NULL);

  SEMAPHORE_LOCKED_DO(&clientList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find incomplete connected client with connection older than 60s
    clientNode = LIST_FIND(clientList,
                           clientNode,
                              (clientNode->clientInfo.authorizationState == AUTHORIZATION_STATE_WAITING)
                           && (Misc_getTimestamp() > (clientNode->clientInfo.connectTimestamp+60LL*US_PER_SECOND))
                          );
    if (clientNode != NULL)
    {
      // remove and disconnect client
      List_remove(clientList,clientNode);
      getClientInfoString(&clientNode->clientInfo,s,sizeof(s));
      deleteClient(clientNode);

      printInfo(1,"Disconnected inactive client %s\n",s);
    }
  }
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : freeAuthorizationFailNode
* Purpose: free authorazation fail node
* Input  : authorizationFailNode - authorization fail node
*          userData              - user data
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
* Name   : newAuthorizationFailNode
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
* Purpose: delete authorization fail node
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
* Name   : deleteAuthorizationFailNode
* Purpose: reset authorization failure
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void resetAuthorizationFail(ClientNode *clientNode)
{
  assert(clientNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientNode);

  if (clientNode->clientInfo.authorizationFailNode != NULL)
  {
    clientNode->clientInfo.authorizationFailNode->count         = 0;
    clientNode->clientInfo.authorizationFailNode->lastTimestamp = Misc_getTimestamp();
  }
}

/***********************************************************************\
* Name   : incrementAuthorizationFail
* Purpose: increment authorization failure
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void incrementAuthorizationFail(ClientNode *clientNode)
{
  assert(clientNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(clientNode);

  if (clientNode->clientInfo.authorizationFailNode == NULL)
  {
    clientNode->clientInfo.authorizationFailNode = newAuthorizationFailNode(clientNode->clientInfo.io.network.name);
    assert(clientNode->clientInfo.authorizationFailNode != NULL);
    List_append(&authorizationFailList,clientNode->clientInfo.authorizationFailNode);
  }
  clientNode->clientInfo.authorizationFailNode->count++;
  clientNode->clientInfo.authorizationFailNode->lastTimestamp = Misc_getTimestamp();
}

/***********************************************************************\
* Name   : getAuthorizationFailTimestamp
* Purpose: get authorization fail timestamp
* Input  : authorizationFailNode - authorization fail node
* Output : -
* Return : authorization fail timestamp [us]
* Notes  : -
\***********************************************************************/

LOCAL uint64 getAuthorizationFailTimestamp(const AuthorizationFailNode *authorizationFailNode)
{
  assert(authorizationFailNode != NULL);

  return   authorizationFailNode->lastTimestamp
          +(uint64)MIN(SQUARE(authorizationFailNode->count)*AUTHORIZATION_PENALITY_TIME,
                        MAX_AUTHORIZATION_PENALITY_TIME
                       )*US_PER_MS;
}

/***********************************************************************\
* Name   : getAuthorizationWaitRestTime
* Purpose: get authorazation fail wait rest time
* Input  : authorizationFailNode - authorization fail node
* Output : -
* Return : wait rest time [s]
* Notes  : -
\***********************************************************************/

LOCAL uint getAuthorizationWaitRestTime(const AuthorizationFailNode *authorizationFailNode)
{
  uint   restTime;
  uint64 authorizationFailTimestamp;
  uint64 nowTimestamp;

  if (authorizationFailNode != NULL)
  {
    authorizationFailTimestamp = getAuthorizationFailTimestamp(authorizationFailNode);
    nowTimestamp               = Misc_getTimestamp();
    restTime = (nowTimestamp < authorizationFailTimestamp)
                 ? (uint)((authorizationFailTimestamp-nowTimestamp)/US_PER_SECOND)
                 : 0;
  }
  else
  {
    restTime = 0;
  }

  return restTime;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : processCommand
* Purpose: process client command
* Input  : clientInfo  - client info
*          id          - unique id
*          name        - command name
*          argumentMap - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processCommand(ClientInfo *clientInfo, uint id, ConstString name, const StringMap argumentMap)
{
  ServerCommandFunction serverCommandFunction;
  AuthorizationStateSet authorizationStateSet;

  assert(clientInfo != NULL);

  // find command
  if (!findCommand(name,&serverCommandFunction,&authorizationStateSet))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_COMMAND,"%S",name);
    return;
  }
  assert(serverCommandFunction != NULL);

  switch (clientInfo->io.type)
  {
    case SERVER_IO_TYPE_BATCH:
      // check authorization (if not in server debug mode)
      #ifndef NDEBUG
        if ((globalOptions.debug.serverLevel >= 1) || IS_SET(authorizationStateSet,clientInfo->authorizationState))
      #else
        if (IS_SET(authorizationStateSet,clientInfo->authorizationState))
      #endif
      {
        // execute
        serverCommandFunction(clientInfo,
                              Index_isAvailable() ? &indexHandle : NULL,
                              id,
                              argumentMap
                             );
      }
      else
      {
        // authorization failure -> mark for disconnect
        clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
      }
      break;
    case SERVER_IO_TYPE_NETWORK:
      switch (clientInfo->authorizationState)
      {
        case AUTHORIZATION_STATE_WAITING:
          // check authorization
          #ifndef NDEBUG
            if ((globalOptions.debug.serverLevel >= 1) || IS_SET(authorizationStateSet,AUTHORIZATION_STATE_WAITING))
          #else
            if (IS_SET(authorizationStateSet,AUTHORIZATION_STATE_WAITING))
          #endif
          {
            // execute command
            serverCommandFunction(clientInfo,
                                  Index_isAvailable() ? &indexHandle : NULL,
                                  id,
                                  argumentMap
                                 );
          }
          else
          {
            // authorization failure -> mark for disconnect
            clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
          }
          break;
        case AUTHORIZATION_STATE_CLIENT:
        case AUTHORIZATION_STATE_MASTER:
          // send command to client thread for asynchronous processing
          putCommand(clientInfo,serverCommandFunction,authorizationStateSet,id,argumentMap);
          break;
        case AUTHORIZATION_STATE_FAIL:
          break;
      }
      break;
    default:
      // free resources
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : isServerHandle
* Purpose: check if server handle
* Input  : serverFlag         - TRUE iff server
*          serverSocketHandle - socket handle
*          handle             - handle
* Output : -
* Return : TRUE iff server use handle
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isServerHandle(bool serverFlag, const ServerSocketHandle *serverSocketHandle, int handle)
{
  return serverFlag && (handle == Network_getServerSocket(serverSocketHandle));
}

/***********************************************************************\
* Name   : isServerTLSHandle
* Purpose: check if server TLS handle
* Input  : serverTLSFlag         - TRUE iff TLS server
*          serverTLSSocketHandle - socket handle
*          handle                - handle
* Output : -
* Return : TRUE iff server use handle
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isServerTLSHandle(bool serverTLSFlag, const ServerSocketHandle *serverTLSSocketHandle, int handle)
{
  return serverTLSFlag && (handle == Network_getServerSocket(serverTLSSocketHandle));
}

// ----------------------------------------------------------------------

Errors Server_initAll(void)
{
  Errors error;

  #ifdef SIMULATE_PURGE
    Array_init(&simulatedPurgeEntityIdArray,sizeof(IndexId),128,NULL,NULL,NULL,NULL);
  #endif /* SIMULATE_PURGE */

  error = Connector_initAll();
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Server_doneAll(void)
{
  Connector_doneAll();

  #ifdef SIMULATE_PURGE
    Array_done(&simulatedPurgeEntityIdArray);
  #endif /* SIMULATE_PURGE */
}

Errors Server_socket(void)
{
  AutoFreeList          autoFreeList;
  Errors                error;
  bool                  serverFlag,serverTLSFlag;
  ServerSocketHandle    serverSocketHandle,serverTLSSocketHandle;
  SignalMask            signalMask;
  WaitHandle            waitHandle;
  int                   handle;
  uint                  events;
  uint64                nowTimestamp,waitTimeout,nextTimestamp;  // [ms]
  bool                  clientDelayFlag;
  AuthorizationFailNode *authorizationFailNode,*oldestAuthorizationFailNode;
  uint                  clientWaitRestTime;
  ClientNode            *clientNode;
  char                  s[256];
  String                name;
  uint                  id;
  AggregateInfo         jobAggregateInfo,scheduleAggregateInfo;
  JobNode               *jobNode;
  ScheduleNode          *scheduleNode;
  StringMap             argumentMap;
  ClientNode            *disconnectClientNode;

  // initialize variables
  AutoFree_init(&autoFreeList);
  hostName                       = Network_getHostName(String_new());
//TODO: remove
//  serverMode                     = mode;
//  serverPort                     = port;
  Semaphore_init(&clientList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&clientList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL));
  List_init(&authorizationFailList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL));
  jobList.activeCount             = 0;
  Semaphore_init(&serverStateLock,SEMAPHORE_TYPE_BINARY);
  serverState                     = SERVER_STATE_RUNNING;
  pauseFlags.create               = FALSE;
  pauseFlags.storage              = FALSE;
  pauseFlags.restore              = FALSE;
  pauseFlags.indexUpdate          = FALSE;
  pauseFlags.indexMaintenance     = FALSE;
  pauseEndDateTime                = 0LL;
  Semaphore_init(&newMaster.lock,SEMAPHORE_TYPE_BINARY);
  newMaster.pairingMode           = PAIRING_MODE_NONE;
  Misc_initTimeout(&newMaster.pairingTimeoutInfo,0LL);
  newMaster.name                  = String_new();
  Crypt_initHash(&newMaster.uuidHash,PASSWORD_HASH_ALGORITHM);
  intermediateMaintenanceDateTime = 0LL;
  quitFlag                        = FALSE;
  AUTOFREE_ADD(&autoFreeList,hostName,{ String_delete(hostName); });
  AUTOFREE_ADD(&autoFreeList,&clientList,{ List_done(&clientList); });
  AUTOFREE_ADD(&autoFreeList,&clientList.lock,{ Semaphore_done(&clientList.lock); });
  AUTOFREE_ADD(&autoFreeList,&authorizationFailList,{ List_done(&authorizationFailList); });
  AUTOFREE_ADD(&autoFreeList,&serverStateLock,{ Semaphore_done(&serverStateLock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.lock,{ Semaphore_done(&newMaster.lock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.pairingTimeoutInfo,{ Misc_doneTimeout(&newMaster.pairingTimeoutInfo); });
  AUTOFREE_ADD(&autoFreeList,newMaster.name,{ String_delete(newMaster.name); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.uuidHash,{ Crypt_doneHash(&newMaster.uuidHash); });

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Started BAR %s server%s on '%s' with %u worker threads",
             VERSION_STRING,
             (globalOptions.serverMode == SERVER_MODE_SLAVE) ? " slave" : "",
             String_cString(hostName),
             (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores()
            );
  printInfo(1,"Started BAR %s server%s on '%s' with %u worker threads\n",
            VERSION_STRING,
            (globalOptions.serverMode == SERVER_MODE_SLAVE) ? " slave" : "",
            String_cString(hostName),
            (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores()
           );

  // create jobs directory if necessary
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    if (!String_isEmpty(globalOptions.jobsDirectory) && !File_exists(globalOptions.jobsDirectory))
    {
      error = File_makeDirectory(globalOptions.jobsDirectory,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSIONS,
                                 FALSE
                                );
      if (error != ERROR_NONE)
      {
        printError("cannot create directory '%s' (error: %s)",
                   String_cString(globalOptions.jobsDirectory),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    if (!File_isDirectory(globalOptions.jobsDirectory))
    {
      printError("'%s' is not a directory!",String_cString(globalOptions.jobsDirectory));
      AutoFree_cleanup(&autoFreeList);
      return ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(globalOptions.jobsDirectory));
    }
  }

  // init index database
  if (!String_isEmpty(globalOptions.indexDatabaseURI))
  {
    DatabaseSpecifier databaseSpecifier;
    String            printableDatabaseURI;

    error = Database_parseSpecifier(&databaseSpecifier,String_cString(globalOptions.indexDatabaseURI),INDEX_DEFAULT_DATABASE_NAME);
    if (error != ERROR_NONE)
    {
      printError("no valid database URI '%s'",globalOptions.indexDatabaseURI);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printableDatabaseURI = Database_getPrintableName(String_new(),&databaseSpecifier,NULL);

    error = Index_init(&databaseSpecifier,CALLBACK_(isMaintenanceTime,NULL));
    if (error != ERROR_NONE)
    {
      printError("cannot init index database '%s' (error: %s)!",
                 String_cString(printableDatabaseURI),
                 Error_getText(error)
                );
      String_delete(printableDatabaseURI);
      Database_doneSpecifier(&databaseSpecifier);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,globalOptions.indexDatabaseURI,{ Index_done(); });

    String_delete(printableDatabaseURI);
    Database_doneSpecifier(&databaseSpecifier);
  }

  // open index
  if (Index_isAvailable())
  {
    error = Index_open(&indexHandle,NULL,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError("cannot open index database (error: %s)!",
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&indexHandle,{ Index_close(&indexHandle); });
  }

  // init server sockets
  serverFlag    = FALSE;
  serverTLSFlag = FALSE;
  if (globalOptions.serverPort != 0)
  {
    error = Network_initServer(&serverSocketHandle,
                               globalOptions.serverPort,
                               SERVER_SOCKET_TYPE_PLAIN,
                               NULL,
                               0,
                               NULL,
                               0,
                               NULL,
                               0
                              );
    if (error == ERROR_NONE)
    {
      printInfo(1,"Opened port %d\n",globalOptions.serverPort);
      serverFlag = TRUE;
      AUTOFREE_ADD(&autoFreeList,&serverSocketHandle,{ Network_doneServer(&serverSocketHandle); });
    }
    else
    {
      printError("cannot initialize server at port %u (error: %s)!",
                 globalOptions.serverPort,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }
  if (globalOptions.serverTLSPort != 0)
  {
    if (   Configuration_isCertificateAvailable(&globalOptions.serverCert)
        && Configuration_isKeyAvailable(&globalOptions.serverKey)
       )
    {
      #ifdef HAVE_GNU_TLS
        error = Network_initServer(&serverTLSSocketHandle,
                                   globalOptions.serverTLSPort,
                                   SERVER_SOCKET_TYPE_TLS,
                                   globalOptions.serverCA.data,
                                   globalOptions.serverCA.length,
                                   globalOptions.serverCert.data,
                                   globalOptions.serverCert.length,
                                   globalOptions.serverKey.data,
                                   globalOptions.serverKey.length
                                  );
        if (error == ERROR_NONE)
        {
          printInfo(1,"Opened TLS/SSL port %u\n",globalOptions.serverTLSPort);
          serverTLSFlag = TRUE;
          AUTOFREE_ADD(&autoFreeList,&serverTLSSocketHandle,{ Network_doneServer(&serverTLSSocketHandle); });
        }
        else
        {
          printError("cannot initialize TLS/SSL server at port %u (error: %s)!",
                     globalOptions.serverTLSPort,
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
      #else /* not HAVE_GNU_TLS */
        printWarning("TLS/SSL is not supported!");
      #endif /* HAVE_GNU_TLS */
    }
    else
    {
      if (!Configuration_isCertificateAvailable(&globalOptions.serverCert)) printWarning("no certificate data (bar-server-cert.pem file) - TLS server not started");
      if (!Configuration_isKeyAvailable(&globalOptions.serverKey)) printWarning("no key data (bar-server-key.pem file) - TLS server not started");
    }
  }
  if (!serverFlag && !serverTLSFlag)
  {
    if ((globalOptions.serverPort == 0) && (globalOptions.serverTLSPort == 0))
    {
      printError("cannot start any server (error: no port numbers specified)!");
    }
    else
    {
      printError("cannot start any server!");
    }
    AutoFree_cleanup(&autoFreeList);
    return ERROR_INVALID_ARGUMENT;
  }
  if (!Configuration_isHashAvailable(&globalOptions.serverPasswordHash))
  {
    printWarning("no server password set!");
  }

  // initial update statics data
  initAggregateInfo(&jobAggregateInfo);
  initAggregateInfo(&scheduleAggregateInfo);
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      getAggregateInfo(&jobAggregateInfo,
                       jobNode->job.uuid,
                       NULL  // scheduleUUID
                      );
      jobNode->executionCount.normal            = jobAggregateInfo.executionCount.normal;
      jobNode->executionCount.full              = jobAggregateInfo.executionCount.full;
      jobNode->executionCount.incremental       = jobAggregateInfo.executionCount.incremental;
      jobNode->executionCount.differential      = jobAggregateInfo.executionCount.differential;
      jobNode->executionCount.continuous        = jobAggregateInfo.executionCount.continuous;
      jobNode->averageDuration.normal           = jobAggregateInfo.averageDuration.normal;
      jobNode->averageDuration.full             = jobAggregateInfo.averageDuration.full;
      jobNode->averageDuration.incremental      = jobAggregateInfo.averageDuration.incremental;
      jobNode->averageDuration.differential     = jobAggregateInfo.averageDuration.differential;
      jobNode->averageDuration.continuous       = jobAggregateInfo.averageDuration.continuous;
      jobNode->totalEntityCount                 = jobAggregateInfo.totalEntityCount;
      jobNode->totalStorageCount                = jobAggregateInfo.totalStorageCount;
      jobNode->totalStorageSize                 = jobAggregateInfo.totalStorageSize;
      jobNode->totalEntryCount                  = jobAggregateInfo.totalEntryCount;
      jobNode->totalEntrySize                   = jobAggregateInfo.totalEntrySize;

      LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
      {
        getAggregateInfo(&scheduleAggregateInfo,
                         jobNode->job.uuid,
                         scheduleNode->uuid
                        );
        scheduleNode->totalEntityCount     = scheduleAggregateInfo.totalEntityCount;
        scheduleNode->totalStorageCount    = scheduleAggregateInfo.totalStorageCount;
        scheduleNode->totalStorageSize     = scheduleAggregateInfo.totalStorageSize;
        scheduleNode->totalEntryCount      = scheduleAggregateInfo.totalEntryCount;
        scheduleNode->totalEntrySize       = scheduleAggregateInfo.totalEntrySize;
      }
    }
  }
  doneAggregateInfo(&scheduleAggregateInfo);
  doneAggregateInfo(&jobAggregateInfo);

  // start threads
  Semaphore_init(&delayThreadTrigger,SEMAPHORE_TYPE_BINARY);
  if (!Thread_init(&jobThread,"BAR job",globalOptions.niceLevel,jobThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize job thread!");
  }
  if (!Thread_init(&pauseThread,"BAR pause",globalOptions.niceLevel,pauseThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize pause thread!");
  }
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    Semaphore_init(&schedulerThreadTrigger,SEMAPHORE_TYPE_BINARY);
    if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize scheduler thread!");
    }
    Semaphore_init(&pairingThreadTrigger,SEMAPHORE_TYPE_BINARY);
    if (!Thread_init(&pairingThread,"BAR pairing",globalOptions.niceLevel,pairingThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize pause thread!");
    }

    if (Index_isAvailable())
    {
  //TODO: required?
      // init database pause callbacks
      Index_setPauseCallback(CALLBACK_(indexPauseCallback,NULL));

      Semaphore_init(&updateIndexThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&updateIndexThread,"BAR update index",globalOptions.niceLevel,updateIndexThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize index thread!");
      }
      Semaphore_init(&autoIndexThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&autoIndexThread,"BAR auto index",globalOptions.niceLevel,autoIndexThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize index update thread!");
      }
      Semaphore_init(&persistenceThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&persistenceThread,"BAR persistence",globalOptions.niceLevel,persistenceThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize persistence thread!");
      }
    }
  }

  // run as server
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      printWarning("server is running in debug mode. No authorization is done, auto-pairing and additional debug commands are enabled!");
    }
  #endif

  // auto-start pairing if unpaired slave or debug mode
  if (globalOptions.serverMode == SERVER_MODE_SLAVE)
  {
    #ifndef NDEBUG
      if (   String_isEmpty(globalOptions.masterInfo.name)
          || (globalOptions.debug.serverLevel >= 1)
         )
      {
        beginPairingMaster(DEFAULT_PAIRING_MASTER_TIMEOUT,PAIRING_MODE_AUTO);
      }
    #else
      if (String_isEmpty(globalOptions.masterInfo.name))
      {
        beginPairingMaster(DEFAULT_PAIRING_MASTER_TIMEOUT,PAIRING_MODE_AUTO);
      }
    #endif

    if (!String_isEmpty(globalOptions.masterInfo.name))
    {
//TODO: log, remove?
      printInfo(1,"Master: %s\n",String_cString(globalOptions.masterInfo.name));
    }
  }

  // Note: ignore SIGALRM in Misc_waitHandles()
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Ready"
            );
  printInfo(1,"Ready\n");
  MISC_SIGNAL_MASK_CLEAR(signalMask);
  #ifdef HAVE_SIGALRM
    MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
  #endif /* HAVE_SIGALRM */

  // process client requests
  Misc_initWait(&waitHandle,64);
  name                     = String_new();
  argumentMap              = StringMap_new();
  while (!isQuit())
  {
    // get active sockets to wait for
    Misc_waitReset(&waitHandle);
    waitTimeout = 0LL;
    SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      // get standard port connection requests
      if (serverFlag)
      {
        Misc_waitAdd(&waitHandle,Network_getServerSocket(&serverSocketHandle),HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID);
//ServerIO_addWait(&clientNode->clientInfo.io,Network_getServerSocket(&serverSocketHandle));
      }

      // get TLS port connection requests
      if (serverTLSFlag)
      {
        Misc_waitAdd(&waitHandle,Network_getServerSocket(&serverTLSSocketHandle),HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID);
//TODO:
//ServerIO_addWait(&clientNode->clientInfo.io,Network_getServerSocket(&serverTLSSocketHandle));
      }

      // get client connections, calculate min. wait timeout
      nowTimestamp = Misc_getTimestamp();
      waitTimeout  = 60LL*MS_PER_SECOND; // wait for network connection max. 1min [ms]
      LIST_ITERATE(&clientList,clientNode)
      {
        clientDelayFlag = FALSE;

        // check if client should be served now, calculate min. wait timeout
        if (clientNode->clientInfo.authorizationFailNode != NULL)
        {
          nextTimestamp = getAuthorizationFailTimestamp(clientNode->clientInfo.authorizationFailNode);
          if (nowTimestamp <= nextTimestamp)
          {
            clientDelayFlag = TRUE;
            if ((nextTimestamp-nowTimestamp) < waitTimeout)
            {
              waitTimeout = (nextTimestamp-nowTimestamp)/US_PER_MS;
            }
          }
        }

        // add client to be served
        if (!clientDelayFlag && ServerIO_isConnected(&clientNode->clientInfo.io))
        {
//TODO: remove
//fprintf(stderr,"%s, %d: add client wait %d\n",__FILE__,__LINE__,clientNode->clientInfo.io.network.port);
          Misc_waitAdd(&waitHandle,Network_getSocket(&clientNode->clientInfo.io.network.socketHandle),HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID);
        }
      }
    }

    // wait for connect, disconnect, command, or result
    (void)Misc_waitHandles(&waitHandle,&signalMask,waitTimeout);

    MISC_HANDLES_ITERATE(&waitHandle,handle,events)
    {
      // connect new clients via plain/standard port
      if      (isServerHandle(serverFlag,&serverSocketHandle,handle))
      {
        if (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
        {
          // try to disconnect incomplete connected clients iff too many connections
          if ((globalOptions.serverMaxConnections > 0) && (List_count(&clientList) >= globalOptions.serverMaxConnections))
          {
             purgeNetworkClient(&clientList);
          }

          // connect new client
          if ((globalOptions.serverMaxConnections == 0) || (List_count(&clientList) < globalOptions.serverMaxConnections))
          {
            error = newNetworkClient(&clientNode,&serverSocketHandle);
            if (error == ERROR_NONE)
            {
              SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // append to list of connected clients
                List_append(&clientList,clientNode);

                // find authorization fail node, get client wait rest time
                clientNode->clientInfo.authorizationFailNode = LIST_FIND(&authorizationFailList,
                                                                         authorizationFailNode,
                                                                         String_equals(authorizationFailNode->clientName,
                                                                                       clientNode->clientInfo.io.network.name
                                                                                      )
                                                                        );

                clientWaitRestTime = getAuthorizationWaitRestTime(clientNode->clientInfo.authorizationFailNode);
                if (globalOptions.serverMode == SERVER_MODE_MASTER)
                {
                  if (clientWaitRestTime > 0)
                  {
                    printInfo(1,
                              "Connected %s (delayed %us)\n",
                              getClientInfoString(&clientNode->clientInfo,s,sizeof(s)),
                              clientWaitRestTime
                             );
                  }
                  else
                  {
                    printInfo(1,
                              "Connected %s\n",
                              getClientInfoString(&clientNode->clientInfo,s,sizeof(s))
                             );
                  }
                }
              }
            }
            else
            {
              printError("cannot establish client connection (error: %s)!",
                         Error_getText(error)
                        );
            }
          }
          else
          {
            printError("establish client connection rejected (error: too many clients)!");
            rejectNetworkClient(&serverSocketHandle);
          }
        }
      }
      else if (isServerTLSHandle(serverTLSFlag,&serverTLSSocketHandle,handle))
      {
        if (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
        {
          // try to disconnect incomplete connected clients iff too many connections
          if ((globalOptions.serverMaxConnections > 0) && (List_count(&clientList) >= globalOptions.serverMaxConnections))
          {
             purgeNetworkClient(&clientList);
          }

          // connect new clients via TLS port
          if ((globalOptions.serverMaxConnections == 0) || (List_count(&clientList) < globalOptions.serverMaxConnections))
          {
            error = newNetworkClient(&clientNode,&serverTLSSocketHandle);
            if (error == ERROR_NONE)
            {
              SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // append to list of connected clients
                List_append(&clientList,clientNode);

                // find authorization fail node
                clientNode->clientInfo.authorizationFailNode = LIST_FIND(&authorizationFailList,
                                                                         authorizationFailNode,
                                                                         String_equals(authorizationFailNode->clientName,
                                                                                       clientNode->clientInfo.io.network.name
                                                                                      )
                                                                        );

                clientWaitRestTime = getAuthorizationWaitRestTime(clientNode->clientInfo.authorizationFailNode);
                if (clientWaitRestTime > 0)
                {
                  printInfo(1,
                            "Connected %s (TLS/SSL, delayed %us)\n",
                            getClientInfoString(&clientNode->clientInfo,s,sizeof(s)),
                            clientWaitRestTime
                           );
                }
                else
                {
                  printInfo(1,
                            "Connected %s (TLS/SSL)\n",
                            getClientInfoString(&clientNode->clientInfo,s,sizeof(s))
                           );
                }
              }
            }
            else
            {
              printError("cannot establish client TLS connection (error: %s)!",
                         Error_getText(error)
                        );
            }
          }
          else
          {
            printError("establish client connection rejected (error: too many clients)!");
            rejectNetworkClient(&serverSocketHandle);
          }
        }
      }
      else
      {
        // process client commands/disconnects/results
        assert(!isServerHandle(serverFlag,&serverSocketHandle,handle));
        assert(!isServerTLSHandle(serverTLSFlag,&serverTLSSocketHandle,handle));

        SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          if (Misc_isAnyEvent(events))
          {
            // find client node
            clientNode = LIST_FIND(&clientList,
                                   clientNode,
                                   handle == Network_getSocket(&clientNode->clientInfo.io.network.socketHandle)
                                  );
            if (clientNode != NULL)
            {
              if (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
              {
                if (ServerIO_receiveData(&clientNode->clientInfo.io))
                {
                  // process all commands
                  while (ServerIO_getCommand(&clientNode->clientInfo.io,
                                             &id,
                                             name,
                                             argumentMap
                                            )
                     )
                  {
                    #ifndef NDEBUG
                      if (globalOptions.debug.serverLevel >= 1)
                      {
                        String string;

                        string = StringMap_debugToString(String_new(),argumentMap);
                        fprintf(stderr,"DEBUG: received command #%u %s %s\n",id,String_cString(name),String_cString(string));
                        String_delete(string);
                      }
                    #endif /* not DEBUG */
                    processCommand(&clientNode->clientInfo,id,name,argumentMap);
                  }
                }
                else
                {
                  // disconnect -> remove from client list
                  disconnectClientNode = clientNode;
                  List_remove(&clientList,disconnectClientNode);

                  // update authorization fail info
                  switch (disconnectClientNode->clientInfo.authorizationState)
                  {
                    case AUTHORIZATION_STATE_WAITING:
                      break;
                    case AUTHORIZATION_STATE_CLIENT:
                    case AUTHORIZATION_STATE_MASTER:
                      // reset authorization failure
                      resetAuthorizationFail(disconnectClientNode);
                      break;
                    case AUTHORIZATION_STATE_FAIL:
                      // increment authorization failure
                      incrementAuthorizationFail(disconnectClientNode);
                      break;
                  }
                  getClientInfoString(&disconnectClientNode->clientInfo,s,sizeof(s));

                  // done client and free resources
                  deleteClient(disconnectClientNode);

                  if (globalOptions.serverMode == SERVER_MODE_MASTER)
                  {
                    printInfo(1,"Disconnected %s\n",s);
                  }
                }
              }
              else if (Misc_isHandleEvent(events,HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID))
              {
                // error/disconnect -> remove from client list
                disconnectClientNode = clientNode;
                clientNode = List_remove(&clientList,disconnectClientNode);

                // update authorization fail info
                switch (disconnectClientNode->clientInfo.authorizationState)
                {
                  case AUTHORIZATION_STATE_WAITING:
                    break;
                  case AUTHORIZATION_STATE_CLIENT:
                  case AUTHORIZATION_STATE_MASTER:
                    // reset authorization failure
                    resetAuthorizationFail(disconnectClientNode);
                    break;
                  case AUTHORIZATION_STATE_FAIL:
                    // increment authorization failure
                    incrementAuthorizationFail(disconnectClientNode);
                    break;
                }
                getClientInfoString(&disconnectClientNode->clientInfo,s,sizeof(s));

                // done client and free resources
                deleteClient(disconnectClientNode);

                if (globalOptions.serverMode == SERVER_MODE_MASTER)
                {
                  printInfo(1,"Disconnected %s\n",s);
                }
              }
              #ifndef NDEBUG
                else
                {
                  HALT_INTERNAL_ERROR("unknown poll events 0x%x",events);
                }
              #endif /* NDEBUG */
            }
            else
            {
              // disconnect unknown client
              Network_disconnectDescriptor(handle);
            }
          }
          else
          {
            // disconnect client with unknown event
            Network_disconnectDescriptor(handle);
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

              // increment authorization failure
              incrementAuthorizationFail(disconnectClientNode);

              getClientInfoString(&disconnectClientNode->clientInfo,s,sizeof(s));

              // done client and free resources
              deleteClient(disconnectClientNode);

              if (globalOptions.serverMode == SERVER_MODE_MASTER)
              {
                printInfo(1,"Disconnected %s\n",s);
              }
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
            // find client
            clientNode = LIST_FIND(&clientList,
                                   clientNode,
                                   clientNode->clientInfo.authorizationFailNode == authorizationFailNode
                                  );

            // check if authorization fail timed out for not active clients
            if (   (clientNode == NULL)
                && (nowTimestamp > (authorizationFailNode->lastTimestamp+(uint64)MAX_AUTHORIZATION_HISTORY_KEEP_TIME*US_PER_MS))
               )
            {
              authorizationFailNode = List_removeAndFree(&authorizationFailList,authorizationFailNode);
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
              // find client
              clientNode = LIST_FIND(&clientList,
                                     clientNode,
                                     clientNode->clientInfo.authorizationFailNode == authorizationFailNode
                                    );

              // get oldest not active client
              if (   (clientNode == NULL)
                  && (authorizationFailNode->lastTimestamp < oldestAuthorizationFailNode->lastTimestamp)
                 )
              {
                oldestAuthorizationFailNode = authorizationFailNode;
              }
            }

            // remove oldest authorization failure from list
            List_removeAndFree(&authorizationFailList,oldestAuthorizationFailNode);
          }
        }
      }
    }

    // auto pairing master
    if (globalOptions.serverMode == SERVER_MODE_SLAVE)
    {
      if (newMaster.pairingMode == PAIRING_MODE_AUTO)
      {
        // set paired master
        if (!String_isEmpty(newMaster.name))
        {
          error = endPairingMaster(newMaster.name,&newMaster.uuidHash);
          if (error != ERROR_NONE)
          {
            logMessage(NULL,  // logHandle,
                       LOG_TYPE_ERROR,
                       "Cannot pair master %s: %s",
                       String_cString(newMaster.name),Error_getText(error)
                      );
          }
        }
      }
    }
  }
  StringMap_delete(argumentMap);
  String_delete(name);
  Misc_doneWait(&waitHandle);

  // delete all clients
  while (!List_isEmpty(&clientList))
  {
    clientNode = (ClientNode*)List_removeFirst(&clientList);
    deleteClient(clientNode);
  }

  // wait for thread exit
  Job_listSetEnd();
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    if (Index_isAvailable())
    {
      if (!Thread_join(&persistenceThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop purge expired entities thread!");
      }
      Thread_done(&persistenceThread);
      Semaphore_done(&persistenceThreadTrigger);

      if (!Thread_join(&autoIndexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop auto index thread!");
      }
      Thread_done(&autoIndexThread);
      Semaphore_done(&autoIndexThreadTrigger);
      if (!Thread_join(&updateIndexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop index thread!");
      }
      Thread_done(&updateIndexThread);
      Semaphore_done(&updateIndexThreadTrigger);

      // done database pause callbacks
      Index_setPauseCallback(CALLBACK_(NULL,NULL));
    }

    if (!Thread_join(&pairingThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop pairing thread!");
    }
    Thread_done(&pairingThread);
    Semaphore_done(&pairingThreadTrigger);
    if (!Thread_join(&schedulerThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop scheduler thread!");
    }
    Thread_done(&schedulerThread);
    Semaphore_done(&schedulerThreadTrigger);
  }
  if (!Thread_join(&pauseThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop pause thread!");
  }
  Thread_done(&pauseThread);
  if (!Thread_join(&jobThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop job thread!");
  }
  Thread_done(&jobThread);
  Semaphore_done(&delayThreadTrigger);

  // done server
  if (serverFlag   ) Network_doneServer(&serverSocketHandle);
  if (serverTLSFlag) Network_doneServer(&serverTLSSocketHandle);

  // done index
  if (Index_isAvailable())
  {
    Index_close(&indexHandle);
  }

  // free resources
  Crypt_doneHash(&newMaster.uuidHash);
  String_delete(newMaster.name);
  Misc_doneTimeout(&newMaster.pairingTimeoutInfo);
  Semaphore_done(&newMaster.lock);
  Semaphore_done(&serverStateLock);
  if (!String_isEmpty(globalOptions.indexDatabaseURI)) Index_done();
  List_done(&authorizationFailList);
  List_done(&clientList);
  Semaphore_done(&clientList.lock);
  String_delete(hostName);
  AutoFree_done(&autoFreeList);

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Terminate BAR server%s",
             (globalOptions.serverMode == SERVER_MODE_SLAVE) ? " slave" : ""
            );
  printInfo(1,"Terminate BAR server%s\n",(globalOptions.serverMode == SERVER_MODE_SLAVE) ? " slave" : "");

  return ERROR_NONE;
}

Errors Server_batch(int inputDescriptor,
                    int outputDescriptor
                   )
{
  AutoFreeList autoFreeList;
  Errors       error;
  ClientInfo   clientInfo;
  String       name;
  uint         id;
  StringMap    argumentMap;

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Started BAR batch"
            );

  // initialize variables
  AutoFree_init(&autoFreeList);
  Semaphore_init(&clientList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&clientList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL));
  List_init(&authorizationFailList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL));
  jobList.activeCount             = 0;
  Semaphore_init(&serverStateLock,SEMAPHORE_TYPE_BINARY);
  serverState                     = SERVER_STATE_RUNNING;
  pauseFlags.create               = FALSE;
  pauseFlags.storage              = FALSE;
  pauseFlags.restore              = FALSE;
  pauseFlags.indexUpdate          = FALSE;
  pauseFlags.indexMaintenance     = FALSE;
  pauseEndDateTime                = 0LL;
  Semaphore_init(&newMaster.lock,SEMAPHORE_TYPE_BINARY);
  newMaster.pairingMode           = PAIRING_MODE_NONE;
  Misc_initTimeout(&newMaster.pairingTimeoutInfo,0LL);
  newMaster.name                  = String_new();
  Crypt_initHash(&newMaster.uuidHash,PASSWORD_HASH_ALGORITHM);
  intermediateMaintenanceDateTime = 0LL;
  quitFlag                        = FALSE;
  AUTOFREE_ADD(&autoFreeList,&clientList,{ List_done(&clientList); });
  AUTOFREE_ADD(&autoFreeList,&clientList.lock,{ Semaphore_done(&clientList.lock); });
  AUTOFREE_ADD(&autoFreeList,&authorizationFailList,{ List_done(&authorizationFailList); });
  AUTOFREE_ADD(&autoFreeList,&serverStateLock,{ Semaphore_done(&serverStateLock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.lock,{ Semaphore_done(&newMaster.lock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.pairingTimeoutInfo,{ Misc_doneTimeout(&newMaster.pairingTimeoutInfo); });
  AUTOFREE_ADD(&autoFreeList,newMaster.name,{ String_delete(newMaster.name); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.uuidHash,{ Crypt_doneHash(&newMaster.uuidHash); });

  // init index database
  if (!String_isEmpty(globalOptions.indexDatabaseURI))
  {
    DatabaseSpecifier databaseSpecifier;
    String            printableDatabaseURI;

    error = Database_parseSpecifier(&databaseSpecifier,String_cString(globalOptions.indexDatabaseURI),INDEX_DEFAULT_DATABASE_NAME);
    if (error != ERROR_NONE)
    {
      printError("no valid database URI '%s'",String_cString(globalOptions.indexDatabaseURI));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printableDatabaseURI = Database_getPrintableName(String_new(),&databaseSpecifier,NULL);

    error = Index_init(&databaseSpecifier,CALLBACK_(isMaintenanceTime,NULL));
    if (error != ERROR_NONE)
    {
      printError("cannot init index database '%s' (error: %s)!",
                 String_cString(printableDatabaseURI),
                 Error_getText(error)
                );
      String_delete(printableDatabaseURI);
      Database_doneSpecifier(&databaseSpecifier);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    String_delete(printableDatabaseURI);
    Database_doneSpecifier(&databaseSpecifier);
  }

  // open index
  if (Index_isAvailable())
  {
    error = Index_open(&indexHandle,NULL,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError("cannot open index database (error: %s)!",
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&indexHandle,{ Index_close(&indexHandle); });
  }

  // start threads
  Semaphore_init(&delayThreadTrigger,SEMAPHORE_TYPE_BINARY);
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    Semaphore_init(&schedulerThreadTrigger,SEMAPHORE_TYPE_BINARY);
    if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize scheduler thread!");
    }
    Semaphore_init(&pairingThreadTrigger,SEMAPHORE_TYPE_BINARY);
    if (!Thread_init(&pairingThread,"BAR pairing",globalOptions.niceLevel,pairingThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize pause thread!");
    }

    if (Index_isAvailable())
    {
//TODO: required?
      // init database pause callbacks
      Index_setPauseCallback(CALLBACK_(indexPauseCallback,NULL));

      Semaphore_init(&updateIndexThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&updateIndexThread,"BAR index",globalOptions.niceLevel,updateIndexThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize update index thread!");
      }

      Semaphore_init(&autoIndexThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&autoIndexThread,"BAR auto index",globalOptions.niceLevel,autoIndexThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize index update thread!");
      }

      Semaphore_init(&persistenceThreadTrigger,SEMAPHORE_TYPE_BINARY);
      if (!Thread_init(&persistenceThread,"BAR purge expired",globalOptions.niceLevel,persistenceThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize purge expire thread!");
      }
    }
  }

  // run in batch mode
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      printWarning("server is running in debug mode. No authorization is done and additional debug commands are enabled!");
    }
  #endif

  // initialize client
  initClient(&clientInfo);

  // init client
  error = initBatchClient(&clientInfo,inputDescriptor,outputDescriptor);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,
            "ERROR: Cannot initialize input/output (error: %s)!\n",
            Error_getText(error)
           );
    return error;
  }

  // send info
//TODO: via server io
#if 0
  File_printLine(&clientInfo.io.file.outputHandle,
                 "BAR VERSION %u %u\n",
                 SERVER_PROTOCOL_VERSION_MAJOR,
                 SERVER_PROTOCOL_VERSION_MINOR
                );
  File_flush(&clientInfo.io.file.outputHandle);
#endif

  // process client requests
#if 1
  name        = String_new();
  argumentMap = StringMap_new();
#if 1
  while (!isQuit())
  {
    if      (ServerIO_getCommand(&clientInfo.io,
                                 &id,
                                 name,
                                 argumentMap
                                )
            )
    {
      #ifndef NDEBUG
        if (globalOptions.debug.serverLevel >= 1)
        {
          String string;

          string = StringMap_debugToString(String_new(),argumentMap);
          fprintf(stderr,"DEBUG: received command #%u %s: %s\n",id,String_cString(name),String_cString(string));
          String_delete(string);
        }
      #endif /* not DEBUG */
      processCommand(&clientInfo,id,name,argumentMap);
    }
    else if (!File_eof(&clientInfo.io.file.inputHandle))
    {
      ServerIO_receiveData(&clientInfo.io);
    }
    else
    {
      setQuit();
    }
  }
#else /* 0 */
String_setCString(commandString,"1 SET crypt-password password='muster'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN type=REGEX pattern=test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST name=test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */
  StringMap_delete(argumentMap);
  String_delete(name);
#endif

  // wait for thread exit
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    if (Index_isAvailable())
    {
      if (!Thread_join(&persistenceThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop persistence thread!");
      }
      Thread_done(&persistenceThread);
      Semaphore_done(&persistenceThreadTrigger);

      if (!Thread_join(&autoIndexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop auto index thread!");
      }
      Thread_done(&autoIndexThread);
      Semaphore_done(&autoIndexThreadTrigger);

      if (!Thread_join(&updateIndexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop index thread!");
      }
      Thread_done(&updateIndexThread);
      Semaphore_done(&updateIndexThreadTrigger);

      // done database pause callbacks
      Index_setPauseCallback(CALLBACK_(NULL,NULL));
    }

    if (!Thread_join(&pairingThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop pairing thread!");
    }
    Thread_done(&pairingThread);
    Semaphore_done(&pairingThreadTrigger);
    if (!Thread_join(&schedulerThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop scheduler thread!");
    }
    Thread_done(&schedulerThread);
    Semaphore_done(&schedulerThreadTrigger);
  }
  Semaphore_done(&delayThreadTrigger);

  doneClient(&clientInfo);

  // done index
  if (Index_isAvailable())
  {
    Index_close(&indexHandle);
  }

  // free resources
  if (!String_isEmpty(globalOptions.indexDatabaseURI)) Index_done();
  Crypt_doneHash(&newMaster.uuidHash);
  String_delete(newMaster.name);
  Misc_doneTimeout(&newMaster.pairingTimeoutInfo);
  Semaphore_done(&newMaster.lock);
  Semaphore_done(&serverStateLock);
  List_done(&authorizationFailList);
  List_done(&clientList);
  Semaphore_done(&clientList.lock);
  AutoFree_done(&autoFreeList);

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Terminate BAR batch"
            );

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
