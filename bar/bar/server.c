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

#include "entrylists.h"
#include "crypt.h"
#include "archive.h"
#include "storage.h"
#include "index.h"
#include "continuous.h"
#include "jobs.h"
#include "server_io.h"
#include "connector.h"
#include "bar.h"

#include "commands_create.h"
#include "commands_restore.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define _NO_SESSION_ID
#define _SIMULATOR
#define _SIMULATE_PURGE

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE                         1024     // number of session key bits

#define MAX_NETWORK_CLIENT_THREADS               3        // number of threads for a client
#define LOCK_TIMEOUT                             (10L*60L*MS_PER_SECOND)  // general lock timeout [ms]
#define CLIENT_TIMEOUT                           (30L*MS_PER_SECOND)  // client timeout [ms]

#define SLAVE_DEBUG_LEVEL                        1
#define SLAVE_COMMAND_TIMEOUT                    (10L*MS_PER_SECOND)

#define AUTHORIZATION_PENALITY_TIME              500      // delay processing by failCount^2*n [ms]
#define MAX_AUTHORIZATION_PENALITY_TIME          30000    // max. penality time [ms]
#define MAX_AUTHORIZATION_HISTORY_KEEP_TIME      30000    // max. time to keep entries in authorization fail history [ms]
#define MAX_AUTHORIZATION_FAIL_HISTORY           64       // max. length of history of authorization fail clients
#define MAX_ABORT_COMMAND_IDS                    512      // max. aborted command ids history

#define MAX_SCHEDULE_CATCH_TIME                  30       // max. schedule catch time [days]

#define DEFAULT_PAIRING_MASTER_TIMEOUT           120      // default timeout pairing new master [s]

// sleep times
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

// expiration
typedef struct ExpirationEntityNode
{
  LIST_NODE_HEADER(struct ExpirationEntityNode);

  IndexId               entityId;
  String                jobUUID;
  ArchiveTypes          archiveType;
  uint64                createdDateTime;
  uint64                size;
  ulong                 totalEntryCount;
  uint64                totalEntrySize;
  const PersistenceNode *persistenceNode;
} ExpirationEntityNode;

typedef struct
{
  LIST_HEADER(ExpirationEntityNode);
} ExpirationEntityList;

// aggregate info
typedef struct
{
  uint64 lastExecutedDateTime;
  String lastErrorMessage;
  struct
  {
    ulong normal;
    ulong full;
    ulong incremental;
    ulong differential;
    ulong continuous;
  }      executionCount;
  struct
  {
    uint64 normal;
    uint64 full;
    uint64 incremental;
    uint64 differential;
    uint64 continuous;
  }      averageDuration;
  ulong  totalEntityCount;
  ulong  totalStorageCount;
  uint64 totalStorageSize;
  ulong  totalEntryCount;
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

typedef struct IndexCryptPasswordNode
{
  LIST_NODE_HEADER(struct IndexCryptPasswordNode);

  Password *cryptPassword;
  Key      cryptPrivateKey;
} IndexCryptPasswordNode;

// list with index decrypt passwords
typedef struct
{
  LIST_HEADER(IndexCryptPasswordNode);
} IndexCryptPasswordList;

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

  String clientName;                                    // client name
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

  // authization data
  AuthorizationStates   authorizationState;
  AuthorizationFailNode *authorizationFailNode;

  bool                  quitFlag;

  // commands
  CommandInfoList       commandInfoList;                   // running command list
  RingBuffer            abortedCommandIds;                 // aborted command ids
  uint                  abortedCommandIdStart;

  // i/o
  ServerIO              io;

  // command processing
  Thread                threads[MAX_NETWORK_CLIENT_THREADS];
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

/***************************** Variables *******************************/
LOCAL String                hostName;

LOCAL ServerModes           serverMode;
LOCAL uint                  serverPort;
#ifdef HAVE_GNU_TLS
LOCAL const Certificate     *serverCA;
LOCAL const Certificate     *serverCert;
LOCAL const Key             *serverKey;
#endif /* HAVE_GNU_TLS */
LOCAL const Hash            *serverPasswordHash;

LOCAL ClientList            clientList;                  // list with clients
LOCAL AuthorizationFailList authorizationFailList;       // list with failed client authorizations
LOCAL Thread                jobThread;                   // thread executing jobs create/restore
LOCAL Thread                schedulerThread;             // thread for scheduling jobs
LOCAL Thread                pauseThread;
LOCAL Thread                pairingThread;               // thread for pairing master/slaves
LOCAL Semaphore             indexThreadTrigger;
LOCAL Thread                indexThread;                 // thread to add/update index
LOCAL Thread                autoIndexThread;             // thread to collect BAR files for auto-index
LOCAL Thread                purgeExpiredEntitiesThread;  // thread to purge expired archive files

LOCAL Semaphore             serverStateLock;
LOCAL ServerStates          serverState;                 // current server state
LOCAL struct
      {
        bool create;
        bool storage;
        bool restore;
        bool indexUpdate;
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
LOCAL IndexHandle           *indexHandle;                // index handle
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
* Input  : scheduleUUID - schedule UUIDor NULL for generate new UUID
* Output : -
* Return : new schedule node
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *newScheduleNode(ConstString scheduleUUID)
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

  if (!String_isEmpty(scheduleUUID))
  {
    String_set(scheduleNode->uuid,scheduleUUID);
  }
  else
  {
    Misc_getUUID(scheduleNode->uuid);
  }

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
* Name   : parseScheduleDateTime
* Purpose: parse schedule date/time
* Input  : scheduleNode - schedule node variable
*          date         - date string (<year|*>-<month|*>-<day|*>)
*          weekDays     - week days string (<day>,...)
*          time         - time string <hour|*>:<minute|*>
* Output : scheduleNode - schedule node
* Return : ERROR_NONE or error code
* Notes  : month names: jan, feb, mar, apr, may, jun, jul, aug, sep, oct
*          nov, dec
*          week day names: mon, tue, wed, thu, fri, sat, sun
\***********************************************************************/

LOCAL Errors parseScheduleDateTime(ScheduleNode *scheduleNode,
                                   ConstString  date,
                                   ConstString  weekDays,
                                   ConstString  time
                                  )
{
  Errors error;
  String s0,s1,s2;

  assert(scheduleNode != NULL);
  assert(date != NULL);
  assert(weekDays != NULL);
  assert(time != NULL);

  error = ERROR_NONE;

  // parse date
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parse(date,STRING_BEGIN,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (   !parseDateTimeNumber(s0,&scheduleNode->date.year )
        || !parseDateMonth     (s1,&scheduleNode->date.month)
        || !parseDateTimeNumber(s2,&scheduleNode->date.day  )
       )
    {
      error = ERROR_PARSE;
    }
  }
  else
  {
    error = ERROR_PARSE;
  }

  // parse week days
  if (!parseWeekDaySet(String_cString(weekDays),&scheduleNode->weekDaySet))
  {
    error = ERROR_PARSE;
  }

  // parse time
  if (String_parse(time,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (   !parseDateTimeNumber(s0,&scheduleNode->time.hour  )
        || !parseDateTimeNumber(s1,&scheduleNode->time.minute)
       )
    {
      error = ERROR_PARSE;
    }
  }
  else
  {
    error = ERROR_PARSE;
  }
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
* Name   : getClientInfo
* Purpose: get client info string
* Input  : clientInfo - client info
*          buffer     - info string variable
*          bufferSize - buffer size
* Output : buffer - info string
* Return : buffer - info string
* Notes  : -
\***********************************************************************/

LOCAL const char *getClientInfo(ClientInfo *clientInfo, char *buffer, uint bufferSize)
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
      stringFormat(buffer,bufferSize,"%s:%d",String_cString(clientInfo->io.network.name),clientInfo->io.network.port);
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
* Name   : storageRequestVolume
* Purpose: request volume call-back
* Input  : type         - storage request volume type,
*          volumeNumber - volume number
*          message      - message or NULL
*          userData     - user data: job node
* Output : -
* Return : request result; see StorageRequestResults
* Notes  : -
\***********************************************************************/

LOCAL StorageRequestVolumeResults storageRequestVolume(StorageRequestVolumeTypes type,
                                                       uint                      volumeNumber,
                                                       const char                *message,
                                                       void                      *userData
                                                      )
{
  JobNode                     *jobNode = (JobNode*)userData;
  StorageRequestVolumeResults storageRequestVolumeResult;

  assert(jobNode != NULL);

//TODO: use type?
  UNUSED_VARIABLE(type);

  storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_NONE;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // request volume, set job state
    assert(jobNode->jobState == JOB_STATE_RUNNING);
    jobNode->requestedVolumeNumber = volumeNumber;
    String_setCString(jobNode->statusInfo.message,message);
    jobNode->jobState = JOB_STATE_REQUEST_VOLUME;
    Job_listSignalModifed();

    // wait until volume is available or job is aborted
    storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_NONE;
    do
    {
      Job_listWaitModifed(LOCK_TIMEOUT);

      if      (jobNode->requestedAbortFlag)
      {
        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_ABORTED;
      }
      else if (jobNode->volumeUnloadFlag)
      {
        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_UNLOAD;
        jobNode->volumeUnloadFlag = FALSE;
      }
      else if (jobNode->volumeNumber == jobNode->requestedVolumeNumber)
      {
        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
    }
    while (   !quitFlag
           && (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_NONE)
          );

    // clear request volume, set job state
    String_clear(jobNode->statusInfo.message);
    jobNode->jobState = JOB_STATE_RUNNING;
    Job_listSignalModifed();
  }

  return storageRequestVolumeResult;
}

/***********************************************************************\
* Name   : startPairing
* Purpose: start pairing master (if not already started)
* Input  : timeout - timeout [s]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void startPairingMaster(uint timeout, PairingModes pairingMode)
{
  ClientNode *clientNode,*disconnectClientNode;

  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (newMaster.pairingMode == PAIRING_MODE_NONE)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 (pairingMode == PAIRING_MODE_AUTO)
                   ? "Start auto pairing master"
                   : "Start pairing master"
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
* Name   : stopPairingMaster
* Purpose: stop pairing master (if started)
* Input  : name     - master name
*          uuidHash - master UUID hash
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors stopPairingMaster(ConstString name, const CryptHash *uuidHash)
{
  Errors error;

  assert(name != NULL);
  assert(uuidHash != NULL);

  SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // set/clear paired master
    if (!String_isEmpty(name))
    {
      String_set(globalOptions.masterInfo.name,name);
      if (!setHash(&globalOptions.masterInfo.uuidHash,uuidHash))
      {
        Semaphore_unlock(&newMaster.lock);
        return ERROR_INSUFFICIENT_MEMORY;
      }
      logMessage(NULL,  // logHandle,
                LOG_TYPE_ALWAYS,
                "Paired master '%s'",
                String_cString(globalOptions.masterInfo.name)
               );
    }
    else
    {
      String_clear(globalOptions.masterInfo.name);
      clearHash(&globalOptions.masterInfo.uuidHash);
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Cleared paired master"
                );
    }

    // stop pairing
    newMaster.pairingMode = PAIRING_MODE_NONE;
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               (newMaster.pairingMode == PAIRING_MODE_AUTO)
                 ? "Stopped auto pairing master"
                 : "Stopped pairing master"
              );

    // update config file
    error = updateConfig();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&newMaster.lock);
      return error;
    }
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
      clearHash(&globalOptions.masterInfo.uuidHash);
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
    error = updateConfig();
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

  aggregateInfo->lastErrorMessage = String_new();
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

  String_delete(aggregateInfo->lastErrorMessage);
}

/***********************************************************************\
* Name   : getAggregateInfo
* Purpose: get aggregate info for job/sched7ule
* Input  : aggregateInfo - aggregate info variable
*          jobUUID       - job UUID
*          scheduleUUID  - schedule UUID or NULL
* Output : aggregateInfo - aggregate info
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getAggregateInfo(AggregateInfo *aggregateInfo,
                            ConstString   jobUUID,
                            ConstString   scheduleUUID
                           )
{
  assert(aggregateInfo != NULL);
  assert(aggregateInfo->lastErrorMessage != NULL);
  assert(jobUUID != NULL);

  // init variables
  aggregateInfo->lastExecutedDateTime         = 0LL;
  String_clear(aggregateInfo->lastErrorMessage);
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
  if (indexHandle != NULL)
  {
    (void)Index_findUUID(indexHandle,
                         jobUUID,
                         scheduleUUID,
                         NULL,  // uuidIndexId
                         &aggregateInfo->lastExecutedDateTime,
                         aggregateInfo->lastErrorMessage,
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
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : error      - error code
*          statusInfo - status info data
*          userData   - user data: job node
* Output : -
* Return :
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(Errors           error,
                            const StatusInfo *statusInfo,
                            void             *userData
                           )
{
  JobNode *jobNode = (JobNode*)userData;
  double  entriesPerSecondAverage,bytesPerSecondAverage,storageBytesPerSecondAverage;
  ulong   restFiles;
  uint64  restBytes;
  uint64  restStorageBytes;
  ulong   estimatedRestTime;

  assert(jobNode != NULL);
  assert(statusInfo != NULL);
  assert(statusInfo->entry.name != NULL);
  assert(statusInfo->storage.name != NULL);

  UNUSED_VARIABLE(error);

  // Note: only try for 2s
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,2*MS_PER_SECOND)
  {
    // store status
    setStatusInfo(&jobNode->statusInfo,statusInfo);

    // calculate statics values
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     statusInfo->done.count);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       statusInfo->done.size);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,statusInfo->storage.doneSize);
    entriesPerSecondAverage      = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.entriesPerSecondFilter     );
    bytesPerSecondAverage        = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.bytesPerSecondFilter       );
    storageBytesPerSecondAverage = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.storageBytesPerSecondFilter);

    // rest rest values
    restFiles         = (statusInfo->total.count       > statusInfo->done.count      ) ? statusInfo->total.count      -statusInfo->done.count       : 0L;
    restBytes         = (statusInfo->total.size        > statusInfo->done.size       ) ? statusInfo->total.size       -statusInfo->done.size        : 0LL;
    restStorageBytes  = (statusInfo->storage.totalSize > statusInfo->storage.doneSize) ? statusInfo->storage.totalSize-statusInfo->storage.doneSize : 0LL;

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
* Name   : restoreUpdateStatusInfo
* Purpose: update restore status info
* Input  : statusInfo - status info data
*          userData   - user data: job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void restoreUpdateStatusInfo(const StatusInfo *statusInfo,
                                   void             *userData
                                  )
{
  JobNode *jobNode = (JobNode*)userData;
//NYI:  double        entriesPerSecond,bytesPerSecond,storageBytesPerSecond;

  assert(jobNode != NULL);
  assert(statusInfo != NULL);
  assert(statusInfo->storage.name != NULL);
  assert(statusInfo->entry.name != NULL);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,2*MS_PER_SECOND)
  {
    // calculate estimated rest time
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     statusInfo->done.count);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       statusInfo->done.size);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,statusInfo->storage.doneSize);

    jobNode->statusInfo.done.count         = statusInfo->done.count;
    jobNode->statusInfo.done.size          = statusInfo->done.size;
    jobNode->statusInfo.skipped.count      = statusInfo->skipped.count;
    jobNode->statusInfo.skipped.size       = statusInfo->skipped.size;
    jobNode->statusInfo.error.count        = statusInfo->error.count;
    jobNode->statusInfo.error.size         = statusInfo->error.size;
    jobNode->statusInfo.archiveSize        = 0LL;
    jobNode->statusInfo.compressionRatio   = 0.0;
    String_set(jobNode->statusInfo.entry.name,statusInfo->entry.name);
    jobNode->statusInfo.entry.doneSize     = statusInfo->entry.doneSize;
    jobNode->statusInfo.entry.totalSize    = statusInfo->entry.totalSize;
    String_set(jobNode->statusInfo.storage.name,statusInfo->storage.name);
    jobNode->statusInfo.storage.doneSize   = statusInfo->storage.doneSize;
    jobNode->statusInfo.storage.totalSize  = statusInfo->storage.totalSize;
    jobNode->statusInfo.volume.number      = 0;
    jobNode->statusInfo.volume.progress    = 0.0;

    jobNode->runningInfo.estimatedRestTime = 0;
  }
}

/***********************************************************************\
* Name   : delayThread
* Purpose: delay thread
* Input  : sleepTime - sleep time [s]
*          trigger   - trigger semaphore (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayThread(uint sleepTime, Semaphore *trigger)
{
  uint n;

  n = 0;
  if (trigger != NULL)
  {
    SEMAPHORE_LOCKED_DO(trigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !quitFlag
             && (n < sleepTime)
             && !Semaphore_waitModified(&indexThreadTrigger,10*MS_PER_SECOND)
            )
      {
        n += 10;
      }
    }
  }
  else
  {
    while (   !quitFlag
           && (n < sleepTime)
          )
    {
      Misc_udelay(10LL*US_PER_SECOND);
      n += 10;
    }
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
  String           scheduleCustomText;
  String           byName;
  ConnectorInfo    *connectorInfo;
  AggregateInfo    jobAggregateInfo,scheduleAggregateInfo;
  StringMap        resultMap;
  JobNode          *jobNode;
  ArchiveTypes     archiveType;
  StorageFlags     storageFlags;
  uint64           startDateTime;
  JobOptions       jobOptions;
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  LogHandle        logHandle;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  IndexHandle      *indexHandle;
  uint64           executeStartDateTime,executeEndDateTime;
  StringList       storageNameList;
  TextMacros       (textMacros,9);
  StaticString     (s,64);
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
  scheduleCustomText = String_new();
  byName             = String_new();
  connectorInfo      = NULL;
  initAggregateInfo(&jobAggregateInfo);
  initAggregateInfo(&scheduleAggregateInfo);
  resultMap          = StringMap_new();
  if (resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  jobNode       = NULL;
  archiveType   = ARCHIVE_ENTRY_TYPE_UNKNOWN;
  storageFlags  = STORAGE_FLAGS_NONE;
  startDateTime = 0LL;
  while (!quitFlag)
  {
    // wait and get next job to run
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // wait and get next job to execute
      do
      {
        // first check for a continuous job to run
        jobNode = jobList.head;
        while (   !quitFlag
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
          while (   !quitFlag
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
        if (!quitFlag && (jobNode == NULL)) Job_listWaitModifed(LOCK_TIMEOUT);
      }
      while (!quitFlag && (jobNode == NULL));
      if (quitFlag)
      {
        Job_listUnlock();
        break;
      }
      assert(jobNode != NULL);

      // get copy of mandatory job data
      String_set(jobName,jobNode->name);
      String_set(storageName,jobNode->job.archiveName);
      String_set(jobUUID,jobNode->job.uuid);
      EntryList_clear(&includeEntryList); EntryList_copy(&includeEntryList,&jobNode->job.includeEntryList,CALLBACK_(NULL,NULL));
      PatternList_clear(&excludePatternList); PatternList_copy(&excludePatternList,&jobNode->job.excludePatternList,CALLBACK_(NULL,NULL));
      Job_duplicateOptions(&jobOptions,&jobNode->job.options);
      if (!String_isEmpty(jobNode->scheduleUUID))
      {
        String_set(scheduleUUID,      jobNode->scheduleUUID);
        String_set(scheduleCustomText,jobNode->scheduleCustomText);
      }
      else
      {
        String_clear(scheduleUUID);
        String_clear(scheduleCustomText);
      }
      archiveType   = jobNode->archiveType;
      startDateTime = jobNode->startDateTime;
      storageFlags  = jobNode->storageFlags;
      String_set(byName,jobNode->byName);

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
    if (storageFlags.noStorage || storageFlags.dryRun)
    {
      String_appendCString(s," (");
      n = 0;
      if (storageFlags.noStorage)
      {
        if (n > 0) String_appendCString(s,", ");
        String_appendCString(s,"no-storage");
        n++;
      }
      if (storageFlags.dryRun)
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
                   "Start job '%s'%s %s%s%s%s",
                   String_cString(jobName),
                   !String_isEmpty(s) ? String_cString(s) : "",
                   Archive_archiveTypeToString(archiveType),
                   !String_isEmpty(byName) ? " by '" : "",
                   String_cString(byName),
                   !String_isEmpty(byName) ? "'" : ""
                  );
        break;
      case JOB_TYPE_RESTORE:
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Start restore%s%s%s%s",
                   !String_isEmpty(s) ? String_cString(s) : "",
                   !String_isEmpty(byName) ? " by '" : "",
                   String_cString(byName),
                   !String_isEmpty(byName) ? "'" : ""
                  );
        break;
    }

    // open index
    indexHandle = NULL;
    if (Index_isAvailable())
    {
      while (!quitFlag && (indexHandle == NULL))
      {
        indexHandle = Index_open(jobNode->masterIO,10*MS_PER_SECOND);
      }
    }

    // get start date/time
    executeStartDateTime = Misc_getCurrentDateTime();

    // pre-process command
    if (!String_isEmpty(jobNode->job.options.preProcessScript))
    {
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%name",     jobName,                                                      NULL);
        TEXT_MACRO_X_STRING ("%archive",  storageName,                                                  NULL);
        TEXT_MACRO_X_CSTRING("%type",     Archive_archiveTypeToString(archiveType),                     NULL);
        TEXT_MACRO_X_CSTRING("%T",        Archive_archiveTypeToShortString(archiveType),                NULL);
        TEXT_MACRO_X_STRING ("%directory",File_getDirectoryName(directory,storageSpecifier.archiveName),NULL);
        TEXT_MACRO_X_STRING ("%file",     storageSpecifier.archiveName,                                 NULL);
      }
      error = executeTemplate(String_cString(jobNode->job.options.preProcessScript),
                              executeStartDateTime,
                              textMacros.data,
                              textMacros.count
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

    // execute create/restore operation
    Index_beginInUse();
    {
      if      (!Job_isRemote(jobNode))
      {
        // local job -> run on this machine

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

        // create/restore operaton
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
                // create archive
                jobNode->runningInfo.error = Command_create(jobNode->masterIO,
                                                            archiveType,
                                                            jobUUID,
                                                            scheduleUUID,
                                                            storageName,
                                                            &includeEntryList,
                                                            &excludePatternList,
                                                            &jobOptions,
//TODO:
NULL,//                                                        scheduleTitle,
                                                            scheduleCustomText,
                                                            startDateTime,
                                                            storageFlags,
                                                            CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                            CALLBACK_(updateStatusInfo,jobNode),
                                                            CALLBACK_(storageRequestVolume,jobNode),
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
                                                             storageFlags,
                                                             CALLBACK_(restoreUpdateStatusInfo,jobNode),
                                                             CALLBACK_(NULL,NULL),  // restoreHandleError
                                                             CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                             CALLBACK__INLINE(bool,(void *userData),{ UNUSED_VARIABLE(userData); return pauseFlags.restore; },NULL),
                                                             CALLBACK__INLINE(bool,(void *userData),{ UNUSED_VARIABLE(userData); return jobNode->requestedAbortFlag; },NULL),
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

          logPostProcess(&logHandle,
                         &jobNode->job.options,
                         archiveType,
                         scheduleCustomText,
                         jobName,
                         jobNode->jobState,
                         jobNode->storageFlags,
                         jobNode->statusInfo.message
                        );
        }
      }
      else
      {
        // slave job -> send to slave and run on slave machine
        if (jobNode->runningInfo.error == ERROR_NONE)
        {
          if (connectorInfo == NULL)
          {
            jobNode->runningInfo.error = ERROR_SLAVE_DISCONNECTED;
          }
        }

        if (jobNode->runningInfo.error == ERROR_NONE)
        {
          if (!Connector_isAuthorized(connectorInfo))
          {
            jobNode->runningInfo.error = ERROR_NOT_PAIRED;
          }
        }

        if (jobNode->runningInfo.error == ERROR_NONE)
        {
          // init storage
          jobNode->runningInfo.error = Connector_initStorage(connectorInfo,
                                                             jobNode->job.archiveName,
                                                             &jobNode->job.options,
                                                             storageFlags
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
                                                          storageFlags,
                                                          CALLBACK_(getCryptPasswordFromConfig,jobNode),
                                                          CALLBACK_(updateStatusInfo,jobNode),
                                                          CALLBACK_(storageRequestVolume,jobNode)
                                                         );

            // done storage
            Connector_doneStorage(connectorInfo);
          }
        }
      }
    }
    Index_endInUse();

    // post-process command
    if (!String_isEmpty(jobNode->job.options.postProcessScript))
    {
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%name",     jobName,                                                      NULL);
        TEXT_MACRO_X_STRING ("%archive",  storageName,                                                  NULL);
        TEXT_MACRO_X_CSTRING("%type",     Archive_archiveTypeToString(archiveType),                     NULL);
        TEXT_MACRO_X_CSTRING("%T",        Archive_archiveTypeToShortString(archiveType),                NULL);
        TEXT_MACRO_X_STRING ("%directory",File_getDirectoryName(directory,storageSpecifier.archiveName),NULL);
        TEXT_MACRO_X_STRING ("%file",     storageSpecifier.archiveName,                                 NULL);
        TEXT_MACRO_X_CSTRING("%state",    Job_getStateText(jobNode->jobState,jobNode->storageFlags),    NULL);
        TEXT_MACRO_X_INTEGER("%error",    Error_getCode(jobNode->runningInfo.error),                    NULL);
        TEXT_MACRO_X_CSTRING("%message",  Error_getText(jobNode->runningInfo.error),                    NULL);
      }
      error = executeTemplate(String_cString(jobNode->job.options.postProcessScript),
                              executeStartDateTime,
                              textMacros.data,
                              textMacros.count
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

    // add index history information
    switch (jobNode->jobType)
    {
      case JOB_TYPE_NONE:
        break;
      case JOB_TYPE_CREATE:
        if (jobNode->requestedAbortFlag)
        {
          if (indexHandle != NULL)
          {
            error = Index_newHistory(indexHandle,
                                     jobUUID,
                                     scheduleUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     "aborted",
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->statusInfo.total.count,
                                     jobNode->statusInfo.total.size,
                                     jobNode->statusInfo.skipped.count,
                                     jobNode->statusInfo.skipped.size,
                                     jobNode->statusInfo.error.count,
                                     jobNode->statusInfo.error.size,
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
          if (indexHandle != NULL)
          {
            error = Index_newHistory(indexHandle,
                                     jobUUID,
                                     scheduleUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     Error_getText(jobNode->runningInfo.error),
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->statusInfo.total.count,
                                     jobNode->statusInfo.total.size,
                                     jobNode->statusInfo.skipped.count,
                                     jobNode->statusInfo.skipped.size,
                                     jobNode->statusInfo.error.count,
                                     jobNode->statusInfo.error.size,
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
          if (indexHandle != NULL)
          {
            error = Index_newHistory(indexHandle,
                                     jobUUID,
                                     scheduleUUID,
                                     hostName,
                                     NULL,  // userName
                                     archiveType,
                                     Misc_getCurrentDateTime(),
                                     NULL,  // errorMessage
                                     executeEndDateTime-executeStartDateTime,
                                     jobNode->statusInfo.total.count,
                                     jobNode->statusInfo.total.size,
                                     jobNode->statusInfo.skipped.count,
                                     jobNode->statusInfo.skipped.size,
                                     jobNode->statusInfo.error.count,
                                     jobNode->statusInfo.error.size,
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

    // log
    switch (jobNode->jobType)
    {
      case JOB_TYPE_NONE:
        break;
      case JOB_TYPE_CREATE:
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

    // close index
    if (Index_isAvailable())
    {
      Index_close(indexHandle);
    }

    // done log
    doneLog(&logHandle);

    // get statistics data
    getAggregateInfo(&jobAggregateInfo,
                     jobNode->job.uuid,
                     NULL  // scheduleUUID
                    );
    getAggregateInfo(&scheduleAggregateInfo,
                     jobNode->job.uuid,
                     scheduleUUID
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
        scheduleNode->lastExecutedDateTime = executeEndDateTime;
        scheduleNode->totalEntityCount     = scheduleAggregateInfo.totalEntityCount;
        scheduleNode->totalStorageCount    = scheduleAggregateInfo.totalStorageCount;
        scheduleNode->totalStorageSize     = scheduleAggregateInfo.totalStorageSize;
        scheduleNode->totalEntryCount      = scheduleAggregateInfo.totalEntryCount;
        scheduleNode->totalEntrySize       = scheduleAggregateInfo.totalEntrySize;
      }

      // free resources
      if (connectorInfo != NULL)
      {
        Job_connectorUnlock(connectorInfo);
      }
      Job_doneOptions(&jobOptions);
      PatternList_clear(&excludePatternList);
      EntryList_clear(&includeEntryList);

      if (!storageFlags.dryRun)
      {
        // store schedule info
        Job_writeScheduleInfo(jobNode,archiveType,executeEndDateTime);
      }
    }
  }

  // free resources
  StringMap_delete(resultMap);
  doneAggregateInfo(&scheduleAggregateInfo);
  doneAggregateInfo(&jobAggregateInfo);
  String_delete(byName);
  String_delete(scheduleCustomText);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  String_delete(directory);
  String_delete(storageName);
  String_delete(jobName);
  Storage_doneSpecifier(&storageSpecifier);
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
  * Input  : slaveNode  - slave node
  *          slaveState - slave state
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void updateSlaveState(const SlaveNode *slaveNode, SlaveStates slaveState);
  void updateSlaveState(const SlaveNode *slaveNode, SlaveStates slaveState)
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
          jobNode->slaveState = slaveState;
        }
      }
    }
  }

  JobNode    *jobNode;
  SlaveNode  *slaveNode;
  Errors     error;
  bool       anyOfflineFlag,anyUnpairedFlag;
  FileHandle fileHandle;
  FileInfo   fileInfo;
  String     line;
  uint64     pairingStopDateTime;
  bool       clearPairing;

  line = String_new();
  while (!quitFlag)
  {
    switch (serverMode)
    {
      case SERVER_MODE_MASTER:
        // try pairing all slaves
        JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          // disconnect disconnected slaves
          JOB_SLAVE_LIST_ITERATE(slaveNode)
          {
            if (Connector_isShutdown(&slaveNode->connectorInfo))
            {
              // disconnect slave
              Connector_disconnect(&slaveNode->connectorInfo);

              // update slave state in job
              updateSlaveState(slaveNode,SLAVE_STATE_OFFLINE);

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
                                           jobNode->job.slaveHost.port
                                          );
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
              if (!Connector_isConnected(&slaveNode->connectorInfo))
              {
                // try connect to slave
                error = Connector_connect(&slaveNode->connectorInfo,
                                          slaveNode->name,
                                          slaveNode->port
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
                error = Connector_authorize(&slaveNode->connectorInfo);
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
                updateSlaveState(slaveNode,SLAVE_STATE_PAIRED);
              }
              else if (Connector_isConnected(&slaveNode->connectorInfo))
              {
                updateSlaveState(slaveNode,SLAVE_STATE_ONLINE);
              }
              else
              {
                updateSlaveState(slaveNode,SLAVE_STATE_OFFLINE);
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
          delayThread(SLEEP_TIME_PAIRING_THREAD,NULL);
        }
        else
        {
          // short sleep
          Misc_udelay(5LL*US_PER_SECOND);
        }
        break;
      case SERVER_MODE_SLAVE:
        // check if pairing/clear master requested
        pairingStopDateTime = 0LL;
        if (File_openCString(&fileHandle,globalOptions.masterInfo.pairingFileName,FILE_OPEN_READ) == ERROR_NONE)
        {
          clearPairing = FALSE;

          // get modified time
          if (File_getInfoCString(&fileInfo,globalOptions.masterInfo.pairingFileName) == ERROR_NONE)
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
          (void)File_deleteCString(globalOptions.masterInfo.pairingFileName,FALSE);

          // check if clear/start/stop pairing
          if (!clearPairing)
          {
            if (Misc_getCurrentDateTime() < pairingStopDateTime)
            {
              startPairingMaster(DEFAULT_PAIRING_MASTER_TIMEOUT,PAIRING_MODE_AUTO);
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
          Misc_udelay(5LL*US_PER_SECOND);
        }
        else
        {
          // sleep and check quit flag
          delayThread(SLEEP_TIME_PAIRING_THREAD,NULL);
        }
        break;
    }
  }
  String_delete(line);

  switch (serverMode)
  {
    case SERVER_MODE_MASTER:
      // disconnect slaves
      JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        JOB_SLAVE_LIST_ITERATE(slaveNode)
        {
          if (Connector_isConnected(&slaveNode->connectorInfo))
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
* Name   : schedulerThreadCode
* Purpose: scheduler thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void schedulerThreadCode(void)
{
  DatabaseHandle continuousDatabaseHandle;
  IndexHandle    *indexHandle;
  JobNode        *jobNode;
  bool           jobListPendingFlag;
  uint64         currentDateTime;
  uint64         dateTime;
  uint           year,month,day,hour,minute;
  WeekDays       weekDay;
  ScheduleNode   *executeScheduleNode;
  uint64         executeScheduleDateTime;
  ScheduleNode   *scheduleNode;

  // open continuous database
  if (Continuous_isAvailable())
  {
      Errors  error = Continuous_open(&continuousDatabaseHandle);
      if (error != ERROR_NONE)
      {
        printError("Cannot initialise continuous database (error: %s)!",
                   Error_getText(error)
                  );
        return;
      }
  }

  // open index
  indexHandle = NULL;
  if (Index_isAvailable())
  {
    while (!quitFlag && (indexHandle == NULL))
    {
      indexHandle = Index_open(NULL,10*MS_PER_SECOND);
    }
  }

  executeScheduleDateTime = 0LL;
  while (!quitFlag)
  {
    // write all modified jobs
    Job_writeModifiedAll();

    // re-read all job config files
    Job_rereadAll(globalOptions.jobsDirectory);

    // check for jobs triggers
    jobListPendingFlag  = FALSE;
    currentDateTime     = Misc_getCurrentDateTime();
//TODO: avoid long running lock
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)  // Note: read/write because of trigger job
    {
      JOB_LIST_ITERATEX(jobNode,!quitFlag && !jobListPendingFlag)
      {
        if (!Job_isActive(jobNode->jobState))
        {
          // check if job have to be executed by regular schedule (check backward in time)
          executeScheduleNode = NULL;
          if (executeScheduleNode == NULL)
          {
            // find oldest job to execute, prefer 'full' job
            if (!List_isEmpty(&jobNode->job.options.scheduleList))
            {
              dateTime = currentDateTime;
              while (   !jobListPendingFlag
                     && !quitFlag
                     && ((dateTime/60LL) > (jobNode->lastScheduleCheckDateTime/60LL))
                     && ((executeScheduleNode == NULL) || (executeScheduleNode->archiveType != ARCHIVE_TYPE_FULL))
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
                LIST_ITERATEX(&jobNode->job.options.scheduleList,scheduleNode,executeScheduleNode == NULL)
                {
                  if (   scheduleNode->enabled
                      && (scheduleNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
                      && (dateTime > scheduleNode->lastExecutedDateTime)
                      && ((scheduleNode->date.year     == DATE_ANY       ) || (scheduleNode->date.year   == (int)year  ))
                      && ((scheduleNode->date.month    == DATE_ANY       ) || (scheduleNode->date.month  == (int)month ))
                      && ((scheduleNode->date.day      == DATE_ANY       ) || (scheduleNode->date.day    == (int)day   ))
                      && ((scheduleNode->weekDaySet    == WEEKDAY_SET_ANY) || IN_SET(scheduleNode->weekDaySet,weekDay)  )
                      && ((scheduleNode->time.hour     == TIME_ANY       ) || (scheduleNode->time.hour   == (int)hour  ))
                      && ((scheduleNode->time.minute   == TIME_ANY       ) || (scheduleNode->time.minute == (int)minute))
                     )
                  {
                    // Note: prefer oldest job or 'full' job
                    if (   (executeScheduleNode == NULL)
                        || (scheduleNode->archiveType == ARCHIVE_TYPE_FULL)
                        || (scheduleNode->lastExecutedDateTime < executeScheduleNode->lastExecutedDateTime)
                       )
                    {
                      executeScheduleNode     = scheduleNode;
                      executeScheduleDateTime = dateTime;
                    }
                  }
                }

                // check if another thread is pending for job list
                jobListPendingFlag = Job_isListLockPending();

                // next time
                dateTime -= 60LL;
              }
            }
          }
          if (jobListPendingFlag || quitFlag) break;

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
            LIST_ITERATEX(&jobNode->job.options.scheduleList,scheduleNode,executeScheduleNode == NULL)
            {
              if (   scheduleNode->enabled
                  && (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
                  && ((scheduleNode->date.year     == DATE_ANY       ) || (scheduleNode->date.year   == (int)year  ))
                  && ((scheduleNode->date.month    == DATE_ANY       ) || (scheduleNode->date.month  == (int)month ))
                  && ((scheduleNode->date.day      == DATE_ANY       ) || (scheduleNode->date.day    == (int)day   ))
                  && ((scheduleNode->weekDaySet    == WEEKDAY_SET_ANY) || IN_SET(scheduleNode->weekDaySet,weekDay)  )
                  && ((scheduleNode->time.hour     == TIME_ANY       ) || (scheduleNode->time.hour   == (int)hour  ))
                  && ((scheduleNode->time.minute   == TIME_ANY       ) || (scheduleNode->time.minute == (int)minute))
                  && (currentDateTime >= (scheduleNode->lastExecutedDateTime + (uint64)scheduleNode->interval*60LL))
                  && Continuous_isAvailable()
                  && Continuous_isEntryAvailable(&continuousDatabaseHandle,jobNode->job.uuid,scheduleNode->uuid)
                 )
              {
                executeScheduleNode     = scheduleNode;
                executeScheduleDateTime = currentDateTime;
              }
//fprintf(stderr,"%s, %d: check %s %"PRIu64" %"PRIu64" -> %"PRIu64": scheduleNode %d %d %p\n",__FILE__,__LINE__,String_cString(jobNode->name),currentDateTime,jobNode->lastExecutedDateTime,currentDateTime-jobNode->lastExecutedDateTime,scheduleNode->archiveType,scheduleNode->interval,executeScheduleNode);

              // check if another thread is pending for job list
              jobListPendingFlag = Job_isListLockPending();
            }
          }
          if (jobListPendingFlag || quitFlag) break;

          // trigger job
          if (executeScheduleNode != NULL)
          {
            Job_trigger(jobNode,
                        executeScheduleNode->uuid,
                        executeScheduleNode->customText,
                        executeScheduleNode->archiveType,
                        executeScheduleNode->noStorage ? STORAGE_FLAGS_NO_STORAGE : STORAGE_FLAGS_NONE,
                        executeScheduleDateTime,
                        "scheduler"
                       );
          }

          // store last schedule check time
          jobNode->lastScheduleCheckDateTime = currentDateTime;

          // check if another thread is pending for job list
          jobListPendingFlag = Job_isListLockPending();
        }
      }
    }

    if (!quitFlag)
    {
      if (!jobListPendingFlag)
      {
        // sleep and check quit flag
        delayThread(SLEEP_TIME_SCHEDULER_THREAD,NULL);
      }
      else
      {
        // short sleep
        Misc_udelay(1LL*US_PER_SECOND);
      }
    }
  }

  // done index
  if (Index_isAvailable())
  {
    Index_close(indexHandle);
  }

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
  while (!quitFlag)
  {
    // decrement pause time, continue
    SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
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

    // sleep, check quit flag
    delayThread(SLEEP_TIME_PAUSE_THREAD,NULL);
  }
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

LOCAL Errors deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors           resultError;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           storageName;
  String           string;
  uint64           createdDateTime;
  const JobNode    *jobNode;
  StorageSpecifier storageSpecifier;
  Errors           error;
  StorageInfo      storageInfo;

  assert(indexHandle != NULL);

  // init variables
  resultError = ERROR_UNKNOWN;
  storageName = String_new();
  string      = String_new();

  // find storage
  if (!Index_findStorageById(indexHandle,
                             storageId,
                             jobUUID,
                             NULL,  // scheduleUUID
                             NULL,  // uuidIndexId
                             NULL,  // entityIndexId
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
    return ERROR_DATABASE_INDEX_NOT_FOUND;
  }

  if (!String_isEmpty(storageName))
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      // find job if possible
      jobNode = Job_findByUUID(jobUUID);

      // delete storage file
      Storage_initSpecifier(&storageSpecifier);
      resultError = Storage_parseName(&storageSpecifier,storageName);
      if (resultError == ERROR_NONE)
      {
//TODO
#ifndef WERROR
#warning NYI: move this special handling of limited scp into Storage_delete()?
#endif
        // init storage
        if (storageSpecifier.type == STORAGE_TYPE_SCP)
        {
          // try to init scp-storage first with sftp
          storageSpecifier.type = STORAGE_TYPE_SFTP;
          resultError = Storage_init(&storageInfo,
                                     NULL,  // masterIO
                                     &storageSpecifier,
                                     (jobNode != NULL) ? &jobNode->job.options : NULL,
                                     &globalOptions.indexDatabaseMaxBandWidthList,
                                     SERVER_CONNECTION_PRIORITY_HIGH,
                                     STORAGE_FLAGS_NONE,
                                     CALLBACK_(NULL,NULL),  // updateStatusInfo
                                     CALLBACK_(NULL,NULL),  // getNamePassword
                                     CALLBACK_(NULL,NULL),  // requestVolume
                                     CALLBACK_(NULL,NULL),  // isPause
                                     CALLBACK_(NULL,NULL),  // isAborted
                                     NULL  // logHandle
                                    );
          if (resultError != ERROR_NONE)
          {
            // init scp-storage
            storageSpecifier.type = STORAGE_TYPE_SCP;
            resultError = Storage_init(&storageInfo,
                                       NULL,  // masterIO
                                       &storageSpecifier,
                                       (jobNode != NULL) ? &jobNode->job.options : NULL,
                                       &globalOptions.indexDatabaseMaxBandWidthList,
                                       SERVER_CONNECTION_PRIORITY_HIGH,
                                       STORAGE_FLAGS_NONE,
                                       CALLBACK_(NULL,NULL),  // updateStatusInfo
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
          resultError = Storage_init(&storageInfo,
                                     NULL,  // masterIO
                                     &storageSpecifier,
                                     (jobNode != NULL) ? &jobNode->job.options : NULL,
                                     &globalOptions.indexDatabaseMaxBandWidthList,
                                     SERVER_CONNECTION_PRIORITY_HIGH,
                                     STORAGE_FLAGS_NONE,
                                     CALLBACK_(NULL,NULL),  // updateStatusInfo
                                     CALLBACK_(NULL,NULL),  // getNamePassword
                                     CALLBACK_(NULL,NULL),  // requestVolume
                                     CALLBACK_(NULL,NULL),  // isPause
                                     CALLBACK_(NULL,NULL),  // isAborted
                                     NULL  // logHandle
                                    );
        }
        if (resultError == ERROR_NONE)
        {
          if (Storage_exists(&storageInfo,
                             NULL  // archiveName
                            )
             )
          {
            // delete storage
            resultError = Storage_delete(&storageInfo,
                                         NULL  // archiveName
                                        );
          }

          // prune empty directories
          Storage_pruneDirectories(&storageInfo,
                                   NULL  // archiveName
                                  );

          // close storage
          Storage_done(&storageInfo);
        }
      }
      Storage_doneSpecifier(&storageSpecifier);
    }
  }

  // delete index
  error = Index_deleteStorage(indexHandle,storageId);
  if (error == ERROR_NONE)
  {
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Deleted storage #%lld: '%s', created at %s",
               Index_getDatabaseId(storageId),
               String_cString(storageName),
               String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,NULL))
              );
  }
  else
  {
    resultError = error;
  }

  // free resources
  String_delete(string);
  String_delete(storageName);

  return resultError;
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
                        NULL,  // hostName
                        ARCHIVE_TYPE_NONE,
                        0LL,  // find createdDateTime,
                        jobUUID,
                        NULL,  // scheduleUUID
                        NULL,  // uuidIndexId
                        NULL,  // entityIndexId
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
    return ERROR_DATABASE_INDEX_NOT_FOUND;
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

  // delete all storage with entity id
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidIndexId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount
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
  while (   (error == ERROR_NONE)
         && !quitFlag
         && Index_getNextStorage(&indexQueryHandle,
                                 NULL,  // uuidIndexId
                                 NULL,  // jobUUID
                                 NULL,  // entityIndexId
                                 NULL,  // scheduleUUID
                                 NULL,  // hostName
                                 NULL,  // archiveType
                                 &storageId,
                                 NULL,  // storageName
                                 NULL,  // createdDateTime
                                 NULL,  // size
                                 NULL,  // userName
                                 NULL,  // comment
                                 NULL,  // indexState
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime
                                 NULL,  // errorMessage
                                 NULL,  // totalEntryCount
                                 NULL  // totalEntrySize
                                )
        )
  {
    error = deleteStorage(indexHandle,storageId);
  }
  Index_doneList(&indexQueryHandle);
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(jobName);
    return error;
  }
  if (quitFlag)
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
//TODO: better info?
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Deleted entity #%lld: job '%s', created at %s",
             Index_getDatabaseId(entityId),
             String_cString(jobName),
             String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,NULL))
            );

  // free resources
  String_delete(string);
  String_delete(jobName);

  return ERROR_NONE;
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
                      jobUUID,
                      NULL,  // findScheduleUUID
                      &uuidId,
                      NULL,  // lastCreatedDateTime,
                      NULL,  // lastErrorMessage,
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
    return ERROR_DATABASE_INDEX_NOT_FOUND;
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
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (   (error == ERROR_NONE)
         && !quitFlag
         && !Job_isSomeActive()
         && Index_getNextEntity(&indexQueryHandle,
                                NULL,  // uuidIndexId
                                NULL,  // jobUUID
                                NULL,  // scheduleUUID
                                &entityId,
                                NULL,  // archiveType
                                NULL,  // createdDateTime
                                NULL,  // lastErrorMessage
                                NULL,  // totalSize
                                NULL,  // totalEntryCount
                                NULL,  // totalEntrySize
                                NULL  // lockedCount
                               )
        )
  {
    (void)deleteEntity(indexHandle,entityId);
  }
  Index_doneList(&indexQueryHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (quitFlag || Job_isSomeActive())
  {
    return ERROR_INTERRUPTED;
  }

  // delete UUID
  error = Index_deleteUUID(indexHandle,uuidId);
  if (error != ERROR_NONE)
  {
    return error;
  }
//TODO: better info?
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Deleted UUID '%s'",
             String_cString(jobUUID)
            );

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : freeExpirationNode
* Purpose: free expiration node
* Input  : expirationEntityNode - expiration entity node
*          userData             - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeExpirationNode(ExpirationEntityNode *expirationEntityNode, void *userData)
{
  assert(expirationEntityNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(expirationEntityNode->jobUUID);
}

/***********************************************************************\
* Name   : newExpirationNode
* Purpose: allocate new expiration node
* Input  : entityId        - entity index id
*          jobUUID         - job UUID
*          archiveType     - archive type; see ArchiveTypes
*          createdDateTime - create date/time
*          size            - size [bytes]
*          totalEntryCount - total entry count
*          totalEntrySize  - total entry size [bytes]
* Output : -
* Return : new persistence node
* Notes  : -
\***********************************************************************/

LOCAL ExpirationEntityNode *newExpirationNode(IndexId      entityId,
                                              ConstString  jobUUID,
                                              ArchiveTypes archiveType,
                                              uint64       createdDateTime,
                                              uint64       size,
                                              ulong        totalEntryCount,
                                              uint64       totalEntrySize
                                             )
{
  ExpirationEntityNode *expirationEntityNode;

  expirationEntityNode = LIST_NEW_NODE(ExpirationEntityNode);
  if (expirationEntityNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  expirationEntityNode->entityId        = entityId;
  expirationEntityNode->jobUUID         = String_duplicate(jobUUID);
  expirationEntityNode->archiveType     = archiveType;
  expirationEntityNode->createdDateTime = createdDateTime;
  expirationEntityNode->size            = size;
  expirationEntityNode->totalEntryCount = totalEntryCount;
  expirationEntityNode->totalEntrySize  = totalEntrySize;
  expirationEntityNode->persistenceNode = NULL;

  return expirationEntityNode;
}

/***********************************************************************\
* Name   : getJobExpirationEntityList
* Purpose: get entity expiration list for job
* Input  : expirationList - expiration list
*          indexHandle    - index handle
*          jobNode        - job node
* Output : -
* Return : TRUE iff got expiration list
* Notes  : -
\***********************************************************************/

LOCAL bool getJobExpirationEntityList(ExpirationEntityList *expirationEntityList,
                                      IndexHandle          *indexHandle,
                                      const JobNode        *jobNode
                                     )
{
  Errors                error;
  IndexQueryHandle      indexQueryHandle;
  uint64                now;
  IndexId               entityId;
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes          archiveType;
  uint64                createdDateTime;
  uint64                size;
  ulong                 totalEntryCount;
  uint64                totalEntrySize;
  uint                  lockedCount;
  ExpirationEntityNode  *expirationEntityNode;
  int                   age;
  const PersistenceNode *lastPersistenceNode;
  const PersistenceNode *persistenceNode,*nextPersistenceNode;

  assert(jobNode != NULL);
  assert(Job_isListLocked());
  assert(indexHandle != NULL);

  List_clear(expirationEntityList,CALLBACK_(NULL,NULL));

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidIndexId
                                 jobNode->job.uuid,
                                 NULL,  // scheduldUUID
                                 ARCHIVE_TYPE_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 DATABASE_ORDERING_DESCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  now = Misc_getCurrentDateTime();
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidIndexId,
                             jobUUID,
                             NULL,  // scheduleUUID,
                             &entityId,
                             &archiveType,
                             &createdDateTime,
                             NULL,  // lastErrorMessage
                             &size,
                             &totalEntryCount,
                             &totalEntrySize,
                             &lockedCount
                            )
        )
  {
//fprintf(stderr,"%s, %d: entityId=%lld archiveType=%d totalSize=%llu now=%llu createdDateTime=%llu -> age=%llu\n",__FILE__,__LINE__,entityId,archiveType,totalSize,now,createdDateTime,(now-createdDateTime)/S_PER_DAY);
    if (   (lockedCount == 0)
        #ifdef SIMULATE_PURGE
        && !Array_contains(&simulatedPurgeEntityIdArray,&entityId,NULL,NULL)
        #endif /* SIMULATE_PURGE */
       )
    {
      // create expiration node
      expirationEntityNode = newExpirationNode(entityId,
                                               jobUUID,
                                               archiveType,
                                               createdDateTime,
                                               size,
                                               totalEntryCount,
                                               totalEntrySize
                                              );
      assert(expirationEntityNode != NULL);

      // find persistence node for entity
      age                 = (now-createdDateTime)/S_PER_DAY;
      lastPersistenceNode = NULL;

      persistenceNode = LIST_HEAD(&jobNode->job.options.persistenceList);
      do
      {
        // find persistence node for archive type
        while (   (persistenceNode != NULL)
               && (persistenceNode->archiveType != archiveType)
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
                 && (nextPersistenceNode->archiveType != archiveType)
                );
        }

        if (persistenceNode != NULL)
        {
          if (   ((lastPersistenceNode == NULL) || (age >= lastPersistenceNode->maxAge))
              && ((persistenceNode->maxAge == AGE_FOREVER) || (age < persistenceNode->maxAge))
             )
          {
            expirationEntityNode->persistenceNode = persistenceNode;
            break;
          }
        }
        lastPersistenceNode = persistenceNode;

        persistenceNode = nextPersistenceNode;
      }
      while (persistenceNode != NULL);

      // add to list
      List_append(expirationEntityList,expirationEntityNode);
    }
  }

  Index_doneList(&indexQueryHandle);

  return TRUE;
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
* Input  : expirationEntityNode - expiration entity node
* Output : -
* Return : TRUE iff in transit
* Notes  : -
\***********************************************************************/

LOCAL bool isInTransit(const ExpirationEntityNode *expirationEntityNode)
{
  const PersistenceNode      *nextPersistenceNode;
  const ExpirationEntityNode *nextExpirationEntityNode;

  assert(expirationEntityNode != NULL);

  nextPersistenceNode      = LIST_FIND_NEXT(expirationEntityNode->persistenceNode,
                                            nextPersistenceNode,
                                            nextPersistenceNode->archiveType == expirationEntityNode->archiveType
                                           );
  nextExpirationEntityNode = LIST_FIND_NEXT(expirationEntityNode,
                                            nextExpirationEntityNode,
                                            nextExpirationEntityNode->archiveType == expirationEntityNode->archiveType
                                           );

  return (   (nextPersistenceNode != NULL)
          && (   (nextExpirationEntityNode == NULL)
              || (   (nextExpirationEntityNode != NULL)
                  && (expirationEntityNode->persistenceNode != nextExpirationEntityNode->persistenceNode)
                 )
             )
         );
}

/***********************************************************************\
* Name   : purgeExpiredEntitiesThreadCode
* Purpose: purge expired entities thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeExpiredEntitiesThreadCode(void)
{
  String                     string;
  IndexId                    expiredEntityId;
  String                     expiredJobName;
  ArchiveTypes               expiredArchiveType;
  uint64                     expiredCreatedDateTime;
  ulong                      expiredTotalEntryCount;
  uint64                     expiredTotalEntrySize;
  MountList                  mountList;
  IndexHandle                *indexHandle;
  Errors                     error;
  const JobNode              *jobNode;
  ExpirationEntityList       expirationEntityList;
  const ExpirationEntityNode *expirationEntityNode,*otherExpirationEntityNode;
  bool                       inTransit;
  uint                       totalEntityCount;
  uint64                     totalEntitySize;
//  const PersistenceNode      *persistenceNode;

  // init variables
  expiredJobName = String_new();
  string  = String_new();

  // open index
  indexHandle = NULL;
  if (Index_isAvailable())
  {
    while (!quitFlag && (indexHandle == NULL))
    {
      indexHandle = Index_open(NULL,10*MS_PER_SECOND);
    }
  }

  if (indexHandle != NULL)
  {
    while (!quitFlag)
    {
      error = ERROR_NONE;

      if (Index_isInitialized())
      {
        do
        {
          // init variables
          expiredEntityId        = INDEX_ID_NONE;
          expiredArchiveType     = ARCHIVE_TYPE_NONE;
          expiredCreatedDateTime = 0LL;
          expiredTotalEntryCount = 0;
          expiredTotalEntrySize  = 0LL;
          List_init(&mountList);

          JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
          {
            // find expired/surpluse entity
            JOB_LIST_ITERATEX(jobNode,expiredEntityId == INDEX_ID_NONE)
            {
              List_init(&expirationEntityList);

              if (   (Misc_getCurrentDateTime() > (jobNode->job.options.persistenceList.lastModificationDateTime+10*S_PER_MINUTE))
                  && getJobExpirationEntityList(&expirationEntityList,indexHandle,jobNode)
                  && !List_isEmpty(&expirationEntityList)  // only expire if persistence list is not empty
                 )
              {
//LIST_ITERATE(&expirationEntityList,expirationEntityNode) { fprintf(stderr,"%s, %d: exp entity %lld: %llu %llu\n",__FILE__,__LINE__,expirationEntityNode->entityId,expirationEntityNode->createdDateTime,expirationEntityNode->totalEntrySize); }
                // find expired entity
                LIST_ITERATEX(&expirationEntityList,expirationEntityNode,expiredEntityId == INDEX_ID_NONE)
                {
                  totalEntityCount = 0;
                  totalEntitySize  = 0LL;
                  inTransit        = FALSE;

                  if (expirationEntityNode->persistenceNode != NULL)
                  {
                    // calculate number/total size of entities in persistence periode
                    LIST_ITERATE(&expirationEntityList,otherExpirationEntityNode)
                    {
                      if (otherExpirationEntityNode->persistenceNode == expirationEntityNode->persistenceNode)
                      {
                        totalEntityCount++;
                        totalEntitySize += otherExpirationEntityNode->size;
                      }
                    }

                    // check if "in-transit"
                    inTransit = isInTransit(expirationEntityNode);
                  }
//fprintf(stderr,"%s, %d: totalEntityCount=%u totalEntitySize=%llu inTransit=%d\n",__FILE__,__LINE__,totalEntityCount,totalEntitySize,inTransit);

                  // check if expired, keep one "in-transit" entity
                  if (   !inTransit
                      && hasPersistence(jobNode,expirationEntityNode->archiveType)
                      && (   (expirationEntityNode->persistenceNode == NULL)
                          || ((   (expirationEntityNode->persistenceNode->maxKeep > 0)
                               && (expirationEntityNode->persistenceNode->maxKeep >= expirationEntityNode->persistenceNode->minKeep)
                               && (totalEntityCount > (uint)expirationEntityNode->persistenceNode->maxKeep)
                              )
                             )
                         )
                     )
                  {
                    // find oldest entry
                    while (   (expirationEntityNode->next != NULL)
                           && (expirationEntityNode->persistenceNode == expirationEntityNode->next->persistenceNode)
                          )
                    {
                      expirationEntityNode = expirationEntityNode->next;
                    }

                    // get expired entity
                    expiredEntityId        = expirationEntityNode->entityId;
                    String_set(expiredJobName,jobNode->name);
                    expiredArchiveType     = expirationEntityNode->archiveType;
                    expiredCreatedDateTime = expirationEntityNode->createdDateTime;
                    expiredTotalEntryCount = expirationEntityNode->totalEntryCount;
                    expiredTotalEntrySize  = expirationEntityNode->totalEntrySize;

                    // get mount list
                    List_copy(&mountList,
                              NULL,
                              &jobNode->job.options.mountList,
                              NULL,
                              NULL,
                              (ListNodeDuplicateFunction)duplicateMountNode,
                              NULL
                             );
                  }
                }
              }

              List_done(&expirationEntityList,CALLBACK_((ListNodeFreeFunction)freeExpirationNode,NULL));
            }
          } // jobList

          // delete expired entity
          if (expiredEntityId != INDEX_ID_NONE)
          {
            // mount devices
            error = mountAll(&mountList);
            if (error == ERROR_NONE)
            {
              // delete expired entity
              #ifndef SIMULATE_PURGE
                error = deleteEntity(indexHandle,expiredEntityId);
              #else /* not SIMULATE_PURGE */
                Array_append(&simulatedPurgeEntityIdArray,&expiredEntityId);
                error = ERROR_NONE;
              #endif /* SIMULATE_PURGE */

              // unmount devices
              (void)unmountAll(&mountList);
            }
            if (error == ERROR_NONE)
            {
              plogMessage(NULL,  // logHandle,
                          LOG_TYPE_INDEX,
                          "INDEX",
                          #ifdef SIMULATE_PURGE
                            "Purged expired entity of job '%s': %s, created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes) (simulated)",
                          #else /* not SIMULATE_PURGE */
                            "Purged expired entity of job '%s': %s, created at %s, %"PRIu64" entries/%.1f%s (%"PRIu64" bytes)",
                          #endif /* SIMULATE_PURGE */
                          String_cString(expiredJobName),
                          Archive_archiveTypeToString(expiredArchiveType),
                          String_cString(Misc_formatDateTime(String_clear(string),expiredCreatedDateTime,NULL)),
                          expiredTotalEntryCount,
                          BYTES_SHORT(expiredTotalEntrySize),
                          BYTES_UNIT(expiredTotalEntrySize),
                          expiredTotalEntrySize
                         );
            }
          }

          // free resources
          List_done(&mountList,(ListNodeFreeFunction)freeMountNode,NULL);
        }
        while (   !quitFlag
               && (expiredEntityId != INDEX_ID_NONE)
              );
      }

      // purge expired mounts
      purgeMounts();

      // sleep
      if (error == ERROR_NONE)
      {
        // sleep and check quit flag
        delayThread(SLEEP_TIME_PURGE_EXPIRED_ENTITIES_THREAD,NULL);
      }
      else
      {
        // wait a short time and try again
        Misc_udelay(30*US_PER_SECOND);
      }
    }

    // done index
    if (Index_isAvailable())
    {
      Index_close(indexHandle);
    }
  }
  else
  {
    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Index database not available - disabled purge expired"
               );
  }

  // free resources
  String_delete(string);
  String_delete(expiredJobName);
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

LOCAL void addIndexCryptPasswordNode(IndexCryptPasswordList *indexCryptPasswordList, const Password *cryptPassword, const Key *cryptPrivateKey)
{
  IndexCryptPasswordNode *indexCryptPasswordNode;

  indexCryptPasswordNode = LIST_NEW_NODE(IndexCryptPasswordNode);
  if (indexCryptPasswordNode == NULL)
  {
    return;
  }

  indexCryptPasswordNode->cryptPassword = Password_duplicate(cryptPassword);
  duplicateKey(&indexCryptPasswordNode->cryptPrivateKey,cryptPrivateKey);

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

  doneKey(&indexCryptPasswordNode->cryptPrivateKey);
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
    Misc_udelay(500L*US_PER_MS);
  }
}

/***********************************************************************\
* Name   : indexThreadCode
* Purpose: index update thread
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
  StorageInfo            storageInfo;
  IndexCryptPasswordList indexCryptPasswordList;
  JobOptions             jobOptions;
  uint64                 startTimestamp,endTimestamp;
  Errors                 error;
  const JobNode          *jobNode;
  IndexCryptPasswordNode *indexCryptPasswordNode;
  uint64                 totalTimeLastChanged;
  uint64                 totalEntryCount,totalEntrySize;

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();
  List_init(&indexCryptPasswordList);

  // open index
  indexHandle = NULL;
  if (Index_isAvailable())
  {
    while (!quitFlag && (indexHandle == NULL))
    {
      indexHandle = Index_open(NULL,10*MS_PER_SECOND);
    }
  }

  if (indexHandle != NULL)
  {
    // add/update index database
    while (!quitFlag)
    {
      // pause
      pauseIndexUpdate();
      if (quitFlag) break;

      if (Index_isInitialized())
      {
        // get all job crypt passwords and crypt private keys (including no password and default crypt password)
        addIndexCryptPasswordNode(&indexCryptPasswordList,NULL,NULL);
//        addIndexCryptPasswordNode(&indexCryptPasswordList,globalOptions.cryptPassword,NULL);
        JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
        {
          JOB_LIST_ITERATE(jobNode)
          {
            if (!Password_isEmpty(&jobNode->job.options.cryptPassword))
            {
              addIndexCryptPasswordNode(&indexCryptPasswordList,&jobNode->job.options.cryptPassword,&jobNode->job.options.cryptPrivateKey);
            }
          }
        }

        // update index entries
        while (   !quitFlag
               && Index_findStorageByState(indexHandle,
                                           INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED),
                                           NULL,  // uuidIndexId
                                           NULL,  // jobUUID
                                           NULL,  // entityIndexId
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
          if (quitFlag) break;

          // parse storage name, get printable name
          error = Storage_parseName(&storageSpecifier,storageName);
          if (error == ERROR_NONE)
          {
            Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
          }
          else
          {
            String_set(printableStorageName,storageName);
          }

          // init storage
          startTimestamp = 0LL;
          endTimestamp   = 0LL;
          Job_initOptions(&jobOptions);
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &storageSpecifier,
                               &jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_LOW,
                               STORAGE_FLAGS_NONE,
                               CALLBACK_(NULL,NULL),  // updateStatusInfo
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
          if (error == ERROR_NONE)
          {
            // try to create index
            LIST_ITERATE(&indexCryptPasswordList,indexCryptPasswordNode)
            {
              // set password/key
              Password_set(&jobOptions.cryptPassword,indexCryptPasswordNode->cryptPassword);
              setKey(&jobOptions.cryptPrivateKey,indexCryptPasswordNode->cryptPrivateKey.data,indexCryptPasswordNode->cryptPrivateKey.length);

              // index update
              startTimestamp = Misc_getTimestamp();
              error = Archive_updateIndex(indexHandle,
                                          storageId,
                                          &storageInfo,
                                          &totalTimeLastChanged,
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
                  || quitFlag
                 )
              {
                break;
              }
            }

            // done storage
            (void)Storage_done(&storageInfo);
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
          Job_doneOptions(&jobOptions);

          if (error == ERROR_NONE)
          {
            plogMessage(NULL,  // logHandle,
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Created index for '%s', %"PRIu64" entries/%.1f%s (%"PRIu64" bytes), %lumin:%02lus",
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
            // nothing to do
          }
          else
          {
            plogMessage(NULL,  // logHandle,
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Cannot create index for '%s' (error: %s)",
                        String_cString(printableStorageName),
                        Error_getText(error)
                       );
          }
        }

        // free resources
        List_done(&indexCryptPasswordList,(ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL);
      }
      if (quitFlag) break;

      // sleep and check quit flag
      delayThread(SLEEP_TIME_INDEX_THREAD,&indexThreadTrigger);
    }

    // done index
    if (Index_isAvailable())
    {
      Index_close(indexHandle);
    }
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
  List_done(&indexCryptPasswordList,CALLBACK_((ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL));
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
  String        storageDirectoryName;
  const JobNode *jobNode;

  // collect storage locations to check for BAR files
  storageDirectoryName = String_new();
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      File_getDirectoryName(storageDirectoryName,jobNode->job.archiveName);
      if (!String_isEmpty(storageDirectoryName))
      {
        if (!StringList_contains(storageDirectoryList,storageDirectoryName))
        {
          StringList_append(storageDirectoryList,storageDirectoryName);
        }
      }
    }
  }
  String_delete(storageDirectoryName);
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

  // open index (Note: timeout is not important; auto-index should not block)
  indexHandle = NULL;
  if (Index_isAvailable())
  {
    while (!quitFlag && (indexHandle == NULL))
    {
      indexHandle = Index_open(NULL,10*MS_PER_SECOND);
    }
  }

  if (indexHandle != NULL)
  {
    // run continous check for auto index
    while (!quitFlag)
    {
      // pause
      pauseIndexUpdate();
      if (quitFlag) break;

      if (Index_isInitialized())
      {
        // collect storage locations to check for BAR files
        getStorageDirectories(&storageDirectoryList);

        // check storage locations for BAR files, send index update request
        Job_initOptions(&jobOptions);
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
                // get printable storage name
                Storage_getPrintableName(printableStorageName,&storageSpecifier,baseName);

                // read directory and scan all sub-directories for .bar files if possible
                pprintInfo(4,
                           "INDEX: ",
                           "Auto-index scan '%s'\n",
                           String_cString(printableStorageName)
                          );
                File_appendFileNameCString(File_setFileName(pattern,baseName),"*.bar");
                (void)Storage_forAll(pattern,
                                     CALLBACK__INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
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
                                             Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

                                             // get index id, request index update
                                             if (Index_findStorageByName(indexHandle,
                                                                         &storageSpecifier,
                                                                         NULL,  // archiveName
                                                                         NULL,  // uuidIndexId
                                                                         NULL,  // entityIndexId
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
                                                               "Requested update index for '%s'",
                                                               String_cString(printableStorageName)
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
                                                                        INDEX_ID_NONE, // entityIndexId
                                                                        NULL,  // hostName
                                                                        NULL,  // userName
                                                                        storageName,
                                                                        0LL,  // createdDateTime
                                                                        0LL,  // size
                                                                        INDEX_STATE_UPDATE_REQUESTED,
                                                                        INDEX_MODE_AUTO,
                                                                        &storageId
                                                                       );
                                               if (error == ERROR_NONE)
                                               {
                                                 plogMessage(NULL,  // logHandle,
                                                             LOG_TYPE_INDEX,
                                                             "INDEX",
                                                             "Requested add index for '%s'",
                                                             String_cString(printableStorageName)
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
        Job_doneOptions(&jobOptions);

        // delete not existing and expired indizes
        error = Index_initListStorages(&indexQueryHandle,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidIndexId
                                       INDEX_ID_ANY,  // entity id
                                       NULL,  // jobUUID
                                       NULL,  // scheduleUUID,
                                       NULL,  // indexIds
                                       0,  // indexIdCount
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
          now    = Misc_getCurrentDateTime();
          string = String_new();
          while (   !quitFlag
                 && Index_getNextStorage(&indexQueryHandle,
                                         NULL,  // uuidIndexId
                                         NULL,  // jobUUID
                                         NULL,  // entityIndexId
                                         NULL,  // scheduleUUID
                                         NULL,  // hostName
                                         NULL,  // archiveType
                                         &storageId,
                                         storageName,
                                         &createdDateTime,
                                         NULL,  // size
                                         NULL,  // userName
                                         NULL,  // comment
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
              Index_deleteStorage(indexHandle,storageId);

              plogMessage(NULL,  // logHandle,
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Deleted index for '%s', last checked %s",
                          String_cString(printableStorageName),
                          String_cString(Misc_formatDateTime(String_clear(string),lastCheckedDateTime,NULL))
                         );
            }
          }
          Index_doneList(&indexQueryHandle);
          String_delete(string);
        }
      }
      if (quitFlag) break;

      // sleep and check quit flag
      delayThread(SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD,NULL);
    }

    // done index
    if (Index_isAvailable())
    {
      Index_close(indexHandle);
    }
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
  String_delete(storageName);
  String_delete(printableStorageName);
  String_delete(pattern);
  String_delete(baseName);
  Storage_doneSpecifier(&storageSpecifier);
  StringList_done(&storageDirectoryList);
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
* Name   : serverCommand_startSSL
* Purpose: start SSL connection
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
    if ((serverCA == NULL) || (serverCA->data == NULL) || (serverCA->length == 0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NO_TLS_CA,"no server certificate authority data");
      return;
    }
    if ((serverCert == NULL) || (serverCert->data == NULL) || (serverCert->length == 0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NO_TLS_CERTIFICATE,"no server certificate data");
      return;
    }
    if ((serverKey == NULL) || (serverKey->data == NULL) || (serverKey->length == 0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NO_TLS_KEY,"no server key data");
      return;
    }

    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

    SEMAPHORE_LOCKED_DO(&clientInfo->io.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      error = Network_startSSL(&clientInfo->io.network.socketHandle,
                               serverCA->data,
                               serverCA->length,
                               serverCert->data,
                               serverCert->length,
                               serverKey->data,
                               serverKey->length
                              );
      if (error != ERROR_NONE)
      {
        Network_disconnect(&clientInfo->io.network.socketHandle);
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
    if (globalOptions.serverDebugLevel == 0)
    {
      if (ServerIO_verifyPassword(&clientInfo->io,
                                  encryptedPassword,
                                  encryptType,
                                  serverPasswordHash
                                 )
         )
      {
        error = ERROR_NONE;
      }
      else
      {
        logMessage(NULL,  // logHandle,
                   LOG_TYPE_ALWAYS,
                   "Authorization of client %s fail - invalid password",
                   getClientInfo(clientInfo,s,sizeof(s))
                  );
        error = ERROR_INVALID_PASSWORD;
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
//fprintf(stderr,"%s, %d: decrypted uuid\n",__FILE__,__LINE__); debugDumpMemory(buffer,bufferLength,0);
          // calculate hash from UUID
          (void)Crypt_initHash(&uuidHash,PASSWORD_HASH_ALGORITHM);
          Crypt_updateHash(&uuidHash,buffer,bufferLength);

          // verify master UUID (UUID hash)
//fprintf(stderr,"%s, %d: globalOptions.masterInfo.uuidHash length=%d: \n",__FILE__,__LINE__,globalOptions.masterInfo.uuidHash.length);
//if (globalOptions.masterInfo.uuidHash.data != NULL) debugDumpMemory(globalOptions.masterInfo.uuidHash.data,globalOptions.masterInfo.uuidHash.length,0);
          if (!equalsHash(&globalOptions.masterInfo.uuidHash,&uuidHash))
          {
            error = ((serverMode == SERVER_MODE_SLAVE) && String_isEmpty(globalOptions.masterInfo.name))
                      ? ERROR_NOT_PAIRED
                      : ERROR_INVALID_PASSWORD;
            logMessage(NULL,  // logHandle,
                       LOG_TYPE_ALWAYS,
                       "Authorization of client %s fail (error: %s)",
                       getClientInfo(clientInfo,s,sizeof(s)),
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
            Crypt_updateHash(&newMaster.uuidHash,buffer,bufferLength);
          }

          error = stopPairingMaster(newMaster.name,&newMaster.uuidHash);
          break;
        case PAIRING_MODE_MANUAL:
          // manual pairing -> just store new master name+UUID hash
          SEMAPHORE_LOCKED_DO(&newMaster.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            // store name
            String_set(newMaster.name,name);

            // calculate hash from UUID
            (void)Crypt_resetHash(&newMaster.uuidHash);
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
                 "Authorization of client %s fail (error: %s)",
                 getClientInfo(clientInfo,s,sizeof(s)),
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
//TODO: remove/replace
printInfo(1,"Client authorization failure %s: %s \n",getClientInfo(clientInfo,s,sizeof(s)),Error_getText(error));
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
  switch (serverMode)
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
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  if (globalOptions.serverDebugLevel >= 1)
  {
    quitFlag = TRUE;
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_FUNCTION_NOT_SUPPORTED,"not in debug mode");
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
  if (String_equalsCString(name,"FILE_SEPARATOR"))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%c",FILES_PATHNAME_SEPARATOR_CHAR);
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }

  // find config value
  i = ConfigValue_valueIndex(CONFIG_VALUES,NULL,String_cString(name));
  if (i < 0)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"server config '%S'",name);
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
  if (!ConfigValue_parse(String_cString(name),
                         String_cString(value),
                         CONFIG_VALUES,
                         NULL, // sectionName
                         CALLBACK_(NULL,NULL),  // errorFunction
                         CALLBACK_(NULL,NULL),  // warningFunction
                         &globalOptions
                        )
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"server config '%S'",name);
    String_delete(value);
    String_delete(name);
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

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

  error = updateConfig();
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
  startPairingMaster(timeout,PAIRING_MODE_MANUAL);

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
    error = stopPairingMaster(newMaster.name,&newMaster.uuidHash);
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
                      Misc_getRestTimeout(&newMaster.pairingTimeoutInfo)/MS_PER_S,
                      Misc_getTotalTimeout(&newMaster.pairingTimeoutInfo)/MS_PER_S
                     );
}

/***********************************************************************\
* Name   : serverCommand_serverList
* Purpose: get job server list
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
                              serverNode->id,
                              serverNode->server.name,
                              "FILE",
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_FTP:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
                              serverNode->id,
                              serverNode->server.name,
                              "FTP",
                              serverNode->server.ftp.loginName,
                              serverNode->server.maxConnectionCount,
                              serverNode->server.maxStorageSize
                             );
          break;
        case SERVER_TYPE_SSH:
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s port=%d loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
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
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "id=%u name=%'S serverType=%s loginName=%'S maxConnectionCount=%d maxStorageSize=%"PRIu64,
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
  String      name;
  ServerTypes serverType;
  uint        port;
  String      loginName;
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

  // get name, server type, login name, port, password, public/private key, max. connections, max. storage size
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,(StringMapParseEnumFunction)parseServerType,SERVER_TYPE_FILE))
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
  loginName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",loginName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(loginName);
    String_delete(name);
    return;
  }
  password = String_new();
  if (!StringMap_getString(argumentMap,"password",password,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"password=<password>");
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  publicKey = String_new();
  if (!StringMap_getString(argumentMap,"publicKey",publicKey,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"publicKey=<data>");
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  privateKey = String_new();
  if (!StringMap_getString(argumentMap,"privateKey",privateKey,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"privateKey=<data>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxConnectionCount=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxStorageSize=<n>");
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
        Password_setString(&serverNode->server.ftp.password,password);
      }
      break;
    case SERVER_TYPE_SSH:
      serverNode->server.ssh.port = port;
      String_set(serverNode->server.ssh.loginName,loginName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.ssh.password,password);
      }
      setKeyString(&serverNode->server.ssh.publicKey,publicKey);
      setKeyString(&serverNode->server.ssh.privateKey,privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      String_set(serverNode->server.webDAV.loginName,loginName);
      if (!String_isEmpty(password))
      {
        Password_setString(&serverNode->server.webDAV.password,password);
      }
      setKeyString(&serverNode->server.webDAV.publicKey,publicKey);
      setKeyString(&serverNode->server.webDAV.privateKey,privateKey);
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
  error = updateConfig();
  if (error != ERROR_NONE)
  {
    Semaphore_unlock(&globalOptions.serverList.lock);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",serverNode->id);

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
  uint        serverId;
  String      name;
  ServerTypes serverType;
  uint        port;
  String      loginName;
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
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,(StringMapParseEnumFunction)parseServerType,SERVER_TYPE_FILE))
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
  loginName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",loginName,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"loginName=<name>");
    String_delete(loginName);
    String_delete(name);
    return;
  }
  password = String_new();
  if (!StringMap_getString(argumentMap,"password",password,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"password=<password>");
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  publicKey = String_new();
  if (!StringMap_getString(argumentMap,"publicKey",publicKey,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"publicKey=<data>");
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  privateKey = String_new();
  if (!StringMap_getString(argumentMap,"privateKey",privateKey,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"privateKey=<data>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxConnectionCount=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxStorageSize=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find storage server
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,serverId,TRUE,ERROR_SERVER_ID_NOT_FOUND,"%u",serverId);
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
          Password_setString(&serverNode->server.ftp.password,password);
        }
        break;
      case SERVER_TYPE_SSH:
        serverNode->server.ssh.port = port;
        String_set(serverNode->server.ssh.loginName,loginName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.ssh.password,password);
        }
        setKeyString(&serverNode->server.ssh.publicKey,publicKey);
        setKeyString(&serverNode->server.ssh.privateKey,privateKey);
        break;
      case SERVER_TYPE_WEBDAV:
        String_set(serverNode->server.webDAV.loginName,loginName);
        if (!String_isEmpty(password))
        {
          Password_setString(&serverNode->server.webDAV.password,password);
        }
        setKeyString(&serverNode->server.webDAV.publicKey,publicKey);
        setKeyString(&serverNode->server.webDAV.privateKey,privateKey);
        break;
    }
    serverNode->server.maxConnectionCount = maxConnectionCount;
    serverNode->server.maxStorageSize     = maxStorageSize;

    // update config file
    error = updateConfig();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"write config file fail");
      String_delete(privateKey);
      String_delete(publicKey);
      String_delete(password);
      String_delete(loginName);
      String_delete(name);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

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
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      ServerIO_sendResult(&clientInfo->io,serverId,TRUE,ERROR_SERVER_ID_NOT_FOUND,"%u",serverId);
      return;
    }

    // delete storage server
    List_removeAndFree(&globalOptions.serverList,serverNode,CALLBACK_((ListNodeFreeFunction)freeServerNode,NULL));

    // update config file
    error = updateConfig();
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
    if (commandInfoNode != NULL)
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
*            modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE
*          Result:
\***********************************************************************/

LOCAL void serverCommand_pause(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            pauseTime;
  String          modeMask;
  StringTokenizer stringTokenizer;
  ConstString     token;
  char            s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get pause time
  if (!StringMap_getUInt(argumentMap,"time",&pauseTime,0))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"time=<time>");
    return;
  }
  modeMask = String_new();
  if (!StringMap_getString(argumentMap,"modeMask",modeMask,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE|ALL");
    String_delete(modeMask);
    return;
  }

  // set pause time
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
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
               "Pause server by %s for %dmin: %s",
               getClientInfo(clientInfo,s,sizeof(s)),
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
    serverState = SERVER_STATE_SUSPENDED;
    if (String_isEmpty(modeMask))
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
               "Suspended server by %s",
               getClientInfo(clientInfo,s,sizeof(s))
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
  char s[256];

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear pause/suspend
  SEMAPHORE_LOCKED_DO(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    serverState            = SERVER_STATE_RUNNING;
    pauseFlags.create      = FALSE;
    pauseFlags.storage     = FALSE;
    pauseFlags.restore     = FALSE;
    pauseFlags.indexUpdate = FALSE;
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Continued server by %s",
               getClientInfo(clientInfo,s,sizeof(s))
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot open device list");
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read device list");
      Device_closeDeviceList(&deviceListHandle);
      String_delete(deviceName);
      return;
    }

    // get device info
    error = Device_getInfo(&deviceInfo,deviceName);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"cannot read device info");
      Device_closeDeviceList(&deviceListHandle);
      String_delete(deviceName);
      return;
    }

    if (deviceInfo.type == DEVICE_TYPE_BLOCK)
    {
      ServerIO_sendResult(&clientInfo->io,
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
*            jobUUID=<uuid>|""
*          Result:
*            name=<name> size=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_rootList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString   (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode  *jobNode;
  Errors         error;
  RootListHandle rootListHandle;
  String         name;
  DeviceInfo     deviceInfo;
  uint64         size;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

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
      // remote directory list
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,FALSE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "ROOT_LIST"
                                          );
        }
      }
    }
    else
    {
      // local directory list

      // open root list
      error = File_openRootList(&rootListHandle);
      if (error != ERROR_NONE)
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open root list fail");
        return;
      }

      // read root list entries
      name = String_new();
      while (!File_endOfRootList(&rootListHandle))
      {
        error = File_readRootList(&rootListHandle,name);
        if (error == ERROR_NONE)
        {
          error = Device_getInfo(&deviceInfo,name);
          if (error == ERROR_NONE)
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
        else
        {
          ServerIO_sendResult(&clientInfo->io,id,FALSE,error,"open root list fail");
        }
      }
      String_delete(name);

      // close root list
      File_closeRootList(&rootListHandle);
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
*            jobUUID=<uuid>|""
*            name=<name>
*          Result:
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> noBackup=yes|no noDump=yes|no
*            fileType=LINK destinationFileType=<type> name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE CHARACTER name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE BLOCK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=FIFO name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SOCKET name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SPECIAL name=<name> dateTime=<time stamp> noDump=yes|no
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_INFO name=%'S",
                                           name
                                          );
        }
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
                                "fileType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" noDump=%y",
                                name,
                                fileInfo.size,
                                fileInfo.timeModified,
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_DIRECTORY:
            // check if .nobackup exists
            noBackupExists = hasNoBackup(name);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=DIRECTORY name=%'S dateTime=%"PRIu64" noBackup=%y noDump=%y",
                                name,
                                fileInfo.timeModified,
                                noBackupExists,
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_LINK:
            destinationFileType = File_getRealType(name);
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=LINK destinationFileType=%s name=%'S dateTime=%"PRIu64" noDump=%y",
                                File_fileTypeToString(destinationFileType,NULL),
                                name,
                                fileInfo.timeModified,
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_HARDLINK:
            ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                "fileType=HARDLINK name=%'S dateTime=%"PRIu64" noDump=%y",
                                name,
                                fileInfo.timeModified,
                                File_hasAttributeNoDump(&fileInfo)
                               );
            break;
          case FILE_TYPE_SPECIAL:
            switch (fileInfo.specialType)
            {
              case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S size=%"PRIu64" specialType=DEVICE_BLOCK dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_SPECIAL_TYPE_FIFO:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                   );
                break;
              case FILE_SPECIAL_TYPE_SOCKET:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              default:
                ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                                    "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.timeModified,
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
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> noBackup=yes|no noDump=yes|no
*            fileType=LINK destinationFileType=<type> name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE CHARACTER name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE BLOCK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=FIFO name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SOCKET name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SPECIAL name=<name> dateTime=<time stamp> noDump=yes|no
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
  bool                noBackupExists;
  FileTypes           destinationFileType;

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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                           {
                                             assert(resultMap != NULL);

                                             UNUSED_VARIABLE(userData);

                                             return ServerIO_passResult(&clientInfo->io,id,FALSE,ERROR_NONE,resultMap);
                                           },NULL),
                                           "FILE_LIST directory=%'S",
                                           directory
                                          );
        }
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
        ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"open directory fail '%S'",directory);
        String_delete(directory);
        return;
      }

      // read directory entries
      name = String_new();
      while (!File_endOfDirectoryList(&directoryListHandle))
      {
        error = File_readDirectoryList(&directoryListHandle,name);
        if (error == ERROR_NONE)
        {
          error = File_getInfo(&fileInfo,name);
          if (error == ERROR_NONE)
          {
            switch (fileInfo.type)
            {
              case FILE_TYPE_FILE:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_DIRECTORY:
                // check if .nobackup exists
                noBackupExists = hasNoBackup(name);
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=DIRECTORY name=%'S dateTime=%"PRIu64" noBackup=%y noDump=%y",
                                    name,
                                    fileInfo.timeModified,
                                    noBackupExists,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_LINK:
                destinationFileType = File_getRealType(name);
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=LINK destinationFileType=%s name=%'S dateTime=%"PRIu64" noDump=%y",
                                    File_fileTypeToString(destinationFileType,NULL),
                                    name,
                                    fileInfo.timeModified,
                                    ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                   );
                break;
              case FILE_TYPE_HARDLINK:
                ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                    "fileType=HARDLINK name=%'S size=%"PRIu64" dateTime=%"PRIu64" noDump=%y",
                                    name,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    File_hasAttributeNoDump(&fileInfo)
                                   );
                break;
              case FILE_TYPE_SPECIAL:
                switch (fileInfo.specialType)
                {
                  case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%"PRIu64" noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S size=%"PRIu64" specialType=DEVICE_BLOCK dateTime=%"PRIu64" noDump=%y",
                                        name,
                                        fileInfo.size,
                                        fileInfo.timeModified,
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_FIFO:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%"PRIu64" noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  case FILE_SPECIAL_TYPE_SOCKET:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%"PRIu64" noDump=%y",
                                        name,
                                        fileInfo.timeModified,
                                        File_hasAttributeNoDump(&fileInfo)
                                       );
                    break;
                  default:
                    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                                        "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%"PRIu64" noDump=%y",
                                        name,
                                        fileInfo.timeModified,
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
                                "get file info fail for '%S'",
                                name
                               );
          }
        }
        else
        {
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "read directory entry fail for '%S'",
                              name
                             );
        }
      }
      String_delete(name);

      // close directory
      File_closeDirectoryList(&directoryListHandle);
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

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, attribute
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          Connector_executeCommand(connectorInfo,
                                   1,
                                   10*MS_PER_SECOND,
                                   CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
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

  // get name, value
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
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
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot create .nobackup file");
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

  // get name, attribute
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
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
        if (error != ERROR_NONE)
        {
          ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get file attributes fail for '%S'",name);
          Job_listUnlock();
          String_delete(attribute);
          String_delete(name);
          return;
        }

        if ((fileAttributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
        {
          fileAttributes &= ~FILE_ATTRIBUTE_NO_DUMP;
          error = File_setAttributes(fileAttributes,name);
          if (error != ERROR_NONE)
          {
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"set attribute no-dump fail for '%S'",name);
            Job_listUnlock();
            String_delete(attribute);
            String_delete(name);
            return;
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          Connector_executeCommand(connectorInfo,
                                   1,
                                   10*MS_PER_SECOND,
                                   CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                   {
                                     assert(resultMap != NULL);

                                     UNUSED_VARIABLE(userData);

                                     return ServerIO_passResult(&clientInfo->io,id,TRUE,ERROR_NONE,resultMap);
                                   },NULL),
                                   "DIRECTORY_INFO name=%'S timeout=%lld",
                                   name,
                                   timeout
                                  );
        }
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
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        if (Connector_isConnected(connectorInfo))
        {
          error = Connector_executeCommand(connectorInfo,
                                           1,
                                           10*MS_PER_SECOND,
                                           CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
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
          error = ERROR_DISCONNECTED;
        }
      }
    }
    else
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK__INLINE(void,(ConstString line, void *userData),
                                 {
                                   UNUSED_VARIABLE(userData);

                                   ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"line=%'S",line);
                                 },NULL),
                                 CALLBACK__INLINE(void,(ConstString line, void *userData),
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
  int               i;
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
    if (i < 0)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config '%S'",name);
      Job_listUnlock();
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%S",s);
    String_delete(s);
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
    if (ConfigValue_parse(String_cString(name),
                          String_cString(value),
                          JOB_CONFIG_VALUES,
                          NULL, // sectionName
                          CALLBACK_(NULL,NULL),  // errorFunction
                          CALLBACK_(NULL,NULL),  // warningFunction
                          jobNode
                         )
       )
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

      // set modified
      Job_setModified(jobNode);
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"invalid job config '%S' value: '%S'",name,value);
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
  int          i;

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
    if (i < 0)
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
*            lastErrorMessage=<test> \
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
    JOB_LIST_ITERATEX(jobNode,!quitFlag && !isCommandAborted(clientInfo,id))
    {
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "jobUUID=%S master=%'S name=%'S state=%s slaveHostName=%'S slaveHostPort=%d slaveHostForceSSL=%y slaveState=%'s archiveType=%s archivePartSize=%"PRIu64" deltaCompressAlgorithm=%s byteCompressAlgorithm=%s cryptAlgorithm=%'s cryptType=%'s cryptPasswordMode=%'s lastExecutedDateTime=%"PRIu64" lastErrorMessage=%'S estimatedRestTime=%lu",
                          jobNode->job.uuid,
                          (jobNode->masterIO != NULL) ? jobNode->masterIO->network.name : NULL,
                          jobNode->name,
                          Job_getStateText(jobNode->jobState,jobNode->storageFlags),
                          jobNode->job.slaveHost.name,
                          jobNode->job.slaveHost.port,
                          jobNode->job.slaveHost.forceSSL,
                          getSlaveStateText(jobNode->slaveState),
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
                          Job_getLastExecutedDateTime(jobNode),
                          jobNode->lastErrorMessage,
                          jobNode->runningInfo.estimatedRestTime
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
*            lastErrorMessage=<text>
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

    // format and send result
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                        "lastExecutedDateTime=%"PRIu64" lastErrorMessage=%'S executionCountNormal=%lu executionCountFull=%lu executionCountIncremental=%lu executionCountDifferential=%lu executionCountContinuous=%lu averageDurationNormal=%"PRIu64" averageDurationFull=%"PRIu64" averageDurationIncremental=%"PRIu64" averageDurationDifferential=%"PRIu64" averageDurationContinuous=%"PRIu64" totalEntityCount=%lu totalStorageCount=%lu totalStorageSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64,
                        Job_getLastExecutedDateTime(jobNode),
                        jobNode->lastErrorMessage,
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
*            [scheduleCustomText=<text>]
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [noStorage=yes|no]
*            [dryRun=yes|no]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobStart(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       scheduleCustomText;
  ArchiveTypes archiveType;
  StorageFlags storageFlags;
  bool         flag;
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
  scheduleCustomText = String_new();
  StringMap_getString(argumentMap,"scheduleCustomText",scheduleCustomText,NULL);
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(scheduleCustomText);
    return;
  }
  storageFlags = STORAGE_FLAGS_NONE;
  StringMap_getBool(argumentMap,"noStorage",&flag,FALSE);
  if (flag) storageFlags.noStorage = TRUE;
  StringMap_getBool(argumentMap,"dryRun",&flag,FALSE);
  if (flag) storageFlags.dryRun = TRUE;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      String_delete(scheduleCustomText);
      Job_listUnlock();
      return;
    }

    // run job
    if  (!Job_isActive(jobNode->jobState))
    {
      // trigger job
      Job_trigger(jobNode,
                  scheduleUUID,
                  scheduleCustomText,
                  archiveType,
                  storageFlags,
                  Misc_getCurrentDateTime(),
                  getClientInfo(clientInfo,s,sizeof(s))
                 );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(scheduleCustomText);
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
      Job_abort(jobNode);
      String_setCString(jobNode->abortedByInfo,getClientInfo(clientInfo,s,sizeof(s)));
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

    if (serverMode == SERVER_MODE_MASTER)
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
        printWarning("Cannot create job '%s' (error: %s)",String_cString(jobNode->fileName),Error_getText(error));
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

    // get new UUID, set timestamp
    Misc_getUUID(newJobNode->job.uuid);

    // free resources
    String_delete(fileName);

    // write job to file
    error = Job_write(newJobNode);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot update job '%s' (error: %s)",String_cString(jobNode->fileName),Error_getText(error));
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
  String       fileName,pathName,baseName;

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

    // remove job in list if not running or requested volume
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
      File_splitFileName(jobNode->fileName,&pathName,&baseName);
      File_setFileName(fileName,pathName);
      File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
      String_delete(baseName);
      String_delete(pathName);
      (void)File_delete(fileName,FALSE);
      String_delete(fileName);
    }

    // remove from list
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
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobFlush(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // write all job files
  Job_writeModifiedAll();

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
*            entryBytes=<n [bytes]>
*            entryTotalSize=<n [bytes]>
*            storageName=<name>
*            storageDoneSize=<n [bytes]>
*            storageTotalSize=<n [bytes]>
*            volumeNumber=<number>
*            volumeProgress=<n [0..100]>
*            requestedVolumeNumber=<n>
*            entriesPerSecond=<n [1/s]>
*            bytesPerSecond=<n [bytes/s]>
*            storageBytesPerSecond=<n [bytes/s]>
*            estimatedRestTime=<n [s]>
*            message=<text>
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

    // format and send result
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,
                        "state=%s errorCode=%u errorData=%'s doneCount=%lu doneSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" collectTotalSumDone=%y skippedEntryCount=%lu skippedEntrySize=%"PRIu64" errorEntryCount=%lu errorEntrySize=%"PRIu64" archiveSize=%"PRIu64" compressionRatio=%lf entryName=%'S entryDoneSize=%"PRIu64" entryTotalSize=%"PRIu64" storageName=%'S storageDoneSize=%"PRIu64" storageTotalSize=%"PRIu64" volumeNumber=%d volumeProgress=%lf requestedVolumeNumber=%d message=%'S entriesPerSecond=%lf bytesPerSecond=%lf storageBytesPerSecond=%lf estimatedRestTime=%lu",
                        Job_getStateText(jobNode->jobState,jobNode->storageFlags),
                        Error_getCode(jobNode->runningInfo.error),
                        Error_getData(jobNode->runningInfo.error),
                        jobNode->statusInfo.done.count,
                        jobNode->statusInfo.done.size,
                        jobNode->statusInfo.total.count,
                        jobNode->statusInfo.total.size,
                        jobNode->statusInfo.collectTotalSumDone,
                        jobNode->statusInfo.skipped.count,
                        jobNode->statusInfo.skipped.size,
                        jobNode->statusInfo.error.count,
                        jobNode->statusInfo.error.size,
                        jobNode->statusInfo.archiveSize,
                        jobNode->statusInfo.compressionRatio,
                        jobNode->statusInfo.entry.name,
                        jobNode->statusInfo.entry.doneSize,
                        jobNode->statusInfo.entry.totalSize,
                        jobNode->statusInfo.storage.name,
                        jobNode->statusInfo.storage.doneSize,
                        jobNode->statusInfo.storage.totalSize,
                        jobNode->statusInfo.volume.number,
                        jobNode->statusInfo.volume.progress,
                        jobNode->requestedVolumeNumber,
                        jobNode->statusInfo.message,
                        jobNode->runningInfo.entriesPerSecond,
                        jobNode->runningInfo.bytesPerSecond,
                        jobNode->runningInfo.storageBytesPerSecond,
                        jobNode->runningInfo.estimatedRestTime
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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,(StringMapParseEnumFunction)EntryList_parseEntryType,ENTRY_TYPE_UNKNOWN))
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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,(StringMapParseEnumFunction)EntryList_parseEntryType,ENTRY_TYPE_UNKNOWN))
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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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
    PatternList_append(&jobNode->job.excludePatternList,patternString,patternType,&patternId);

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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

    // notify about changed lists
    Job_includeExcludeChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
    List_clear(&jobNode->job.options.mountList,CALLBACK_((ListNodeFreeFunction)freeMountNode,NULL));

    // notify about changed lists
    Job_mountChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
    mountNode = newMountNode(name,device);
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

    // notify about changed lists
    Job_mountChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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

    // notify about changed lists
    Job_mountChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
    deleteMountNode(mountNode);

    // notify about changed lists
    Job_mountChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

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
*            jobUUID=<uuid>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*          Result:
*            scheduleUUID=<uuid> \
*            archiveType=normal|full|incremental|differential
*            date=<year>|*-<month>|*-<day>|* \
*            weekDay=<week day>|* \
*            time=<hour>|*:<minute>|* \
*            interval=<n>
*            customText=<text> \
*            noStorage=yes|no \
*            enabled=yes|no \
*            totalEntities=<n>|0 \
*            totalEntryCount=<n>|0 \
*            totalEntrySize=<n>|0 \
*            ...
\***********************************************************************/

LOCAL void serverCommand_scheduleList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes  archiveType;
  const JobNode *jobNode;
  ScheduleNode  *scheduleNode;
  String        date,weekDays,time;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, archive type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE);

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

    // send schedule list
    date     = String_new();
    weekDays = String_new();
    time     = String_new();
    LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
    {
      if ((archiveType == ARCHIVE_TYPE_NONE) || (scheduleNode->archiveType == archiveType))
      {
        // get date string
        String_clear(date);
        if (scheduleNode->date.year != DATE_ANY)
        {
          String_appendFormat(date,"%d",scheduleNode->date.year);
        }
        else
        {
          String_appendCString(date,"*");
        }
        String_appendChar(date,'-');
        if (scheduleNode->date.month != DATE_ANY)
        {
          String_appendFormat(date,"%02d",scheduleNode->date.month);
        }
        else
        {
          String_appendCString(date,"*");
        }
        String_appendChar(date,'-');
        if (scheduleNode->date.day != DATE_ANY)
        {
          String_appendFormat(date,"%02d",scheduleNode->date.day);
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
          String_appendFormat(time,"%02d",scheduleNode->time.hour);
        }
        else
        {
          String_appendCString(time,"*");
        }
        String_appendChar(time,':');
        if (scheduleNode->time.minute != TIME_ANY)
        {
          String_appendFormat(time,"%02d",scheduleNode->time.minute);
        }
        else
        {
          String_appendCString(time,"*");
        }

        // send schedule info
        ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                            "scheduleUUID=%S archiveType=%s date=%S weekDays=%S time=%S interval=%u customText=%'S noStorage=%y enabled=%y lastExecutedDateTime=%"PRIu64" totalEntities=%lu totalStorageCount=%lu totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                            scheduleNode->uuid,
                            (scheduleNode->archiveType != ARCHIVE_TYPE_UNKNOWN) ? Archive_archiveTypeToString(scheduleNode->archiveType) : "*",
                            date,
                            weekDays,
                            time,
                            scheduleNode->interval,
                            scheduleNode->customText,
                            scheduleNode->noStorage,
                            scheduleNode->enabled,
                            scheduleNode->lastExecutedDateTime,
                            scheduleNode->totalEntityCount,
                            scheduleNode->totalStorageCount,
                            scheduleNode->totalEntryCount,
                            scheduleNode->totalEntrySize
                           );
      }
    }
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"weekDays=<name>|*");
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
  if   (stringEquals(StringMap_getTextCString(argumentMap,"archiveType","*"),"*"))
  {
    archiveType = ARCHIVE_TYPE_NORMAL;
  }
  else
  {
    if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_UNKNOWN))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
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
  if (!StringMap_getBool(argumentMap,"noStorage",&noStorage,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"noStorage=yes|no");
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
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }

  // create new schedule
  scheduleNode = newScheduleNode(scheduleUUID);
  assert(scheduleNode != NULL);

  // parse schedule
  error = parseScheduleDateTime(scheduleNode,
                                date,
                                weekDays,
                                time
                               );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,
                        id,
                        TRUE,
                        ERROR_PARSE,
                        "cannot parse schedule '%S %S %S'",
                        date,
                        weekDays,
                        time
                       );
    deleteScheduleNode(scheduleNode);
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
  scheduleNode->noStorage   = noStorage;
  scheduleNode->enabled     = enabled;
  scheduleNode->minKeep     = 0;
  scheduleNode->maxKeep     = 0;
  scheduleNode->maxAge      = 0;

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      deleteScheduleNode(scheduleNode);
      String_delete(customText);
      String_delete(time);
      String_delete(weekDays);
      String_delete(date);
      String_delete(title);
      return;
    }

    // add to schedule list
    List_append(&jobNode->job.options.scheduleList,scheduleNode);

    // notify about changed schedule
    Job_scheduleChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"scheduleUUID=%S",scheduleNode->uuid);

  // free resources
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
    List_removeAndFree(&jobNode->job.options.scheduleList,scheduleNode,CALLBACK_((ListNodeFreeFunction)freeScheduleNode,NULL));

    // notify about changed schedule
    Job_scheduleChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
  StaticString               (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode              *jobNode;
  const PersistenceNode      *persistenceNode;
  char                       s1[16],s2[16],s3[16];
  ExpirationEntityList       expirationEntityList;
  const ExpirationEntityNode *expirationEntityNode;
  bool                       inTransit;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get job UUID, archive type
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

    // get expiration entity list
    List_init(&expirationEntityList);
    if (indexHandle != NULL)
    {
      getJobExpirationEntityList(&expirationEntityList,indexHandle,jobNode);
    }

//TODO: totalStorageCount, totalStorageSize
    LIST_ITERATE(&jobNode->job.options.persistenceList,persistenceNode)
    {
      // send persistence info
      if (persistenceNode->minKeep != KEEP_ALL   ) stringFormat(s1,sizeof(s1),"%d",persistenceNode->minKeep); else stringSet(s1,sizeof(s1),"*");
      if (persistenceNode->maxKeep != KEEP_ALL   ) stringFormat(s2,sizeof(s2),"%d",persistenceNode->maxKeep); else stringSet(s2,sizeof(s2),"*");
      if (persistenceNode->maxAge  != AGE_FOREVER) stringFormat(s3,sizeof(s3),"%d",persistenceNode->maxAge ); else stringSet(s3,sizeof(s3),"*");
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                          "persistenceId=%u archiveType=%s minKeep=%s maxKeep=%s maxAge=%s size=%"PRIu64"",
                          persistenceNode->id,
                          ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,persistenceNode->archiveType,NULL),
                          s1,
                          s2,
                          s3,
//TODO
0LL//                            persistenceNode->totalEntitySize
                         );
      LIST_ITERATE(&expirationEntityList,expirationEntityNode)
      {
        if (expirationEntityNode->persistenceNode == persistenceNode)
        {
          inTransit = isInTransit(expirationEntityNode);

          // send entity info
          ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                              "persistenceId=%u entityId=%"PRIindexId" createdDateTime=%"PRIu64" size=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" inTransit=%y",
                              expirationEntityNode->persistenceNode->id,
                              expirationEntityNode->entityId,
                              expirationEntityNode->createdDateTime,
                              expirationEntityNode->size,
                              expirationEntityNode->totalEntryCount,
                              expirationEntityNode->totalEntrySize,
                              inTransit
                             );
        }
      }
    }

    // free resources
    List_done(&expirationEntityList,CALLBACK_((ListNodeFreeFunction)freeExpirationNode,NULL));
  }

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
    List_clear(&jobNode->job.options.persistenceList,CALLBACK_((ListNodeFreeFunction)freePersistenceNode,NULL));
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify about changed lists
    Job_persistenceChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_persistenceListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString    (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes    archiveType;
  int             minKeep,maxKeep;
  int             maxAge;
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
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"minKeep","*"),"*"))
  {
    minKeep = KEEP_ALL;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"minKeep",&minKeep,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"minKeep=<n>|*");
      return;
    }
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"maxKeep","*"),"*"))
  {
    maxKeep = KEEP_ALL;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"maxKeep",&maxKeep,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxKeep=<n>|*");
      return;
    }
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"maxAge","*"),"*"))
  {
    maxAge = AGE_FOREVER;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"maxAge",&maxAge,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxAge=<n>|*");
      return;
    }
  }

  persistenceId = ID_NONE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
  persistenceId = ID_NONE;
    jobNode = Job_findByUUID(jobUUID);
    if (jobNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_JOB_NOT_FOUND,"%S",jobUUID);
      Job_listUnlock();
      deletePersistenceNode(persistenceNode);
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
      persistenceNode = newPersistenceNode(archiveType,minKeep,maxKeep,maxAge);
      assert(persistenceNode != NULL);
      insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);

      // set last-modified timestamp
      jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

      // get id
      persistenceId = persistenceNode->id;

      // notify about changed schedule
      Job_scheduleChanged(jobNode);

      // set modified
      Job_setModified(jobNode);
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"id=%u",persistenceId);

  // free resources
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
*          Result:
\***********************************************************************/

LOCAL void serverCommand_persistenceListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  uint                  persistenceId;
  ArchiveTypes          archiveType;
  int                   minKeep,maxKeep;
  int                   maxAge;
  JobNode               *jobNode;
  PersistenceNode       *persistenceNode;
  const PersistenceNode *existingPersistenceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, mount id, name
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
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"minKeep","*"),"*"))
  {
    minKeep = KEEP_ALL;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"minKeep",&minKeep,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"minKeep=<n>|*");
      return;
    }
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"maxKeep","*"),"*"))
  {
    maxKeep = KEEP_ALL;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"maxKeep",&maxKeep,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxKeep=<n>|*");
      return;
    }
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"maxAge","*"),"*"))
  {
    maxAge = AGE_FOREVER;
  }
  else
  {
    if (!StringMap_getInt(argumentMap,"maxAge",&maxAge,0))
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"maxAge=<n>|*");
      return;
    }
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

    // find persistence node
    persistenceNode = LIST_FIND(&jobNode->job.options.persistenceList,persistenceNode,persistenceNode->id == persistenceId);
    if (persistenceNode == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PERSISTENCE_ID_NOT_FOUND,"%u",persistenceId);
      Job_listUnlock();
      return;
    }

    // remove from persistence list
    List_remove(&jobNode->job.options.persistenceList,persistenceNode);

    // update persistence
    persistenceNode->archiveType = archiveType;
    persistenceNode->minKeep     = minKeep;
    persistenceNode->maxKeep     = maxKeep;
    persistenceNode->maxAge      = maxAge;

    if (!LIST_CONTAINS(&jobNode->job.options.persistenceList,
                       existingPersistenceNode,
                          (existingPersistenceNode->archiveType == persistenceNode->archiveType)
                       && (existingPersistenceNode->minKeep     == persistenceNode->minKeep    )
                       && (existingPersistenceNode->maxKeep     == persistenceNode->maxKeep    )
                       && (existingPersistenceNode->maxAge      == persistenceNode->maxAge     )
                      )
       )
    {
      // re-insert updated node into persistence list
      insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
    }
    else
    {
      // duplicate -> discard
     deletePersistenceNode(persistenceNode);
    }

//TODO: remove
    // update "forever"-nodes
//    insertForeverPersistenceNodes(&jobNode->job.options.persistenceList);

    // set last-modified timestamp
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify about changed lists
    Job_persistenceChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
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
    List_removeAndFree(&jobNode->job.options.persistenceList,
                       persistenceNode,
                       CALLBACK_((ListNodeFreeFunction)freePersistenceNode,NULL)
                      );

//TODO: remove
    // update "forever"-nodes
//    insertForeverPersistenceNodes(&jobNode->job.options.persistenceList);

    // set last-modified timestamp
    jobNode->job.options.persistenceList.lastModificationDateTime = Misc_getCurrentDateTime();

    // notify about changed persistence
    Job_persistenceChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
    i = 0;
    while (   (JOB_CONFIG_VALUES[i].name != NULL)
           && !String_equalsCString(name,JOB_CONFIG_VALUES[i].name)
          )
    {
      i++;
    }
    if (JOB_CONFIG_VALUES[i].name == NULL)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config '%S'",name);
      Job_listUnlock();
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"value=%S",s);
    String_delete(s);
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
    if (ConfigValue_parse(String_cString(name),
                          String_cString(value),
                          JOB_CONFIG_VALUES,
                          "schedule",
                          CALLBACK_(NULL,NULL),  // errorFunction
                          CALLBACK_(NULL,NULL),  // warningFunction
                          scheduleNode
                         )
       )
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config '%S'",name);
    }

    // notify about changed schedule
    Job_scheduleChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
  }

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
    i = 0;
    while (   (JOB_CONFIG_VALUES[i].name != NULL)
           && !String_equalsCString(name,JOB_CONFIG_VALUES[i].name)
          )
    {
      i++;
    }
    if (JOB_CONFIG_VALUES[i].name == NULL)
    {
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

    // notify about changed schedule
    Job_scheduleChanged(jobNode);

    // set modified
    Job_setModified(jobNode);
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

    // trigger job
    if (!Job_isActive(jobNode->jobState))
    {
      Job_trigger(jobNode,
                  scheduleNode->uuid,
                  scheduleNode->customText,
                  scheduleNode->archiveType,
                  scheduleNode->noStorage ? STORAGE_FLAGS_NO_STORAGE : STORAGE_FLAGS_NONE,
                  Misc_getCurrentDateTime(),
                  getClientInfo(clientInfo,s,sizeof(s))
                 );
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordsClear
* Purpose: clear decrypt passwords in internal list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
  if (!StringMap_getEnum(argumentMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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
* Purpose: clear ssh/ftp/crypt passwords stored in memory
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

  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Password_clear(&clientInfo->jobOptions.ftpServer.password);
    Password_clear(&clientInfo->jobOptions.sshServer.password);
    Password_clear(&clientInfo->jobOptions.cryptPassword);
  }

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
                       STORAGE_FLAGS_NONE,
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
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
                       CALLBACK_(NULL,NULL),
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"%S",storageName);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // list contents
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveHandle,ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS)
         && (error == ERROR_NONE)
         && !isCommandAborted(clientInfo,id)
        )
  {
    // get next file type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo
                                        NULL,  // offset
                                        ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS
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
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
            uint64             blockOffset,blockCount;

            // open archive file
            imageName       = String_new();
            deltaSourceName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveHandle,
                                           &deltaCompressAlgorithm,
                                           &byteCompressAlgorithm,
                                           &cryptType,
                                           &cryptAlgorithm,
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
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
                                  "fileType=IMAGE name=%'S size=%"PRIu64" archiveSize=%"PRIu64" deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%"PRIu64" fileSystemType=%s blockSize=%u blockOffset=%"PRIu64" blockCount=%"PRIu64"",
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
                                               &archiveHandle,
                                               &cryptType,
                                               &cryptAlgorithm,
                                               directoryName,
                                               &fileInfo,
                                               NULL   // fileExtendedAttributeList
                                              );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
                                          linkName,
                                          name,
                                          NULL,  // fileInfo
                                          NULL   // fileExtendedAttributeList
                                         );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
                                             name,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
            if (error != ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
          error = Archive_skipNextEntry(&archiveHandle);
          if (error != ERROR_NONE)
          {
            ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read content of storage '%S'",storageName);
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"Cannot read next entry of storage '%S'",storageName);
      break;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // close archive
  Archive_close(&archiveHandle);

  // done storage
  (void)Storage_done(&storageInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : serverCommand_storageList
* Purpose: list selected storages
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
                                 INDEX_ID_ANY,  // uuidIndexId
                                 INDEX_ID_ANY,  // entityIndexId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 Array_cArray(&clientInfo->indexIdArray),
                                 Array_length(&clientInfo->indexIdArray),
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail: '%S'",storageName);
    String_delete(storageName);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextStorage(&indexQueryHandle,
                                 NULL,  // uuidIndexId
                                 NULL,  // jobUUID
                                 NULL,  // entityIndexId
                                 NULL,  // scheduleUUID
                                 NULL,  // hostName
                                 NULL,  // archiveType
                                 &storageId,
                                 storageName,
                                 NULL,  // createdDateTime
                                 NULL,  // size
                                 NULL,  // userName
                                 NULL,  // comment
                                 NULL,  // indexState
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime
                                 NULL,  // errorMessage
                                 &totalEntryCount,
                                 &totalEntrySize
                                )
        )
  {
    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "storageId=%"PRIu64" name=%'S totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                        storageId,
                        storageName,
                        totalEntryCount,
                        totalEntrySize
                       );
  }
  Index_doneList(&indexQueryHandle);

  String_delete(storageName);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListClear
* Purpose: clear selected storage list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Array_clear(&clientInfo->indexIdArray);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListAdd
* Purpose: add to selected storage list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          indexIds;
  IndexId         indexId;
  StringTokenizer stringTokenizer;
  ConstString     token;
  long            nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index ids
  indexIds = String_new();
  if (!StringMap_getString(argumentMap,"indexIds",indexIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexIds=<id>,...");
    String_delete(indexIds);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(indexIds);
    return;
  }

  // add to id array
  String_initTokenizer(&stringTokenizer,indexIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      indexId = (IndexId)String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex == STRING_END)
      {
        Array_append(&clientInfo->indexIdArray,&indexId);
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PARSE,"%S",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(indexIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(indexIds);
}

/***********************************************************************\
* Name   : serverCommand_storageListRemove
* Purpose: remove from selected storage list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexIds=<id>,..
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          indexIds;
  IndexId         indexId;
  StringTokenizer stringTokenizer;
  ConstString     token;
  long            nextIndex;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index ids
  indexIds = String_new();
  if (!StringMap_getString(argumentMap,"indexIds",indexIds,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexIds=<id>,...");
    String_delete(indexIds);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(indexIds);
    return;
  }

  // remove from id array
  String_initTokenizer(&stringTokenizer,indexIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      indexId = (IndexId)String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex == STRING_END)
      {
        Array_removeAll(&clientInfo->indexIdArray,&indexId,CALLBACK_(NULL,NULL));
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PARSE,"%S",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(indexIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(indexIds);
}

/***********************************************************************\
* Name   : serverCommand_storageListInfo
* Purpose: get selected storage list info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  error = Index_getStoragesInfos(indexHandle,
                                 INDEX_ID_ANY,  // uuidIndexId
                                 INDEX_ID_ANY,  // entityIndexId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get storages info from index database fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"storageCount=%lu totalEntryCount=%lu totalEntrySize=%"PRIu64" totalEntryContentSize=%"PRIu64"",storageCount,totalEntryCount,totalEntrySize,totalEntryContentSize);
}

/***********************************************************************\
* Name   : serverCommand_entryList
* Purpose: list selected entries
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            entryId=<n> name=<text> type=<type> size=<n> dateTime=<n>
\***********************************************************************/

LOCAL void serverCommand_entryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           entryName;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entryId;
  const char       *type;
  uint64           size;
  uint64           timeModified;

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
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                FALSE,  // newestOnly,
                                0,
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail: '%S'",entryName);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntry(&indexQueryHandle,
                               NULL,  // uuidIndexId
                               NULL,  // jobUUID
                               NULL,  // entityIndexId
                               NULL,  // scheduleUUID
                               NULL,  // hostName
                               NULL,  // userName
                               NULL,  // archiveType
                               NULL,  // storageId
                               NULL,  // storageName
                               NULL,  // storageDateTime
                               &entryId,
                               entryName,
                               NULL,  // destinationName
                               NULL,  // fileSystemType
                               &size,  // size
                               &timeModified,
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

    ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,
                        "entryId=%"PRIu64" name=%'S type=%s size=%"PRIu64" dateTime=%"PRIu64"",
                        entryId,
                        entryName,
                        type,
                        size,
                        timeModified
                       );
  }
  Index_doneList(&indexQueryHandle);

  String_delete(entryName);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListClear
* Purpose: clear selected entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    Array_clear(&clientInfo->entryIdArray);
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListAdd
* Purpose: add to selected entry list
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

LOCAL void serverCommand_entryListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(entryIds);
    return;
  }

  // add to id array
  String_initTokenizer(&stringTokenizer,entryIds,STRING_BEGIN,",",NULL,TRUE);
  SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      entryId = (IndexId)String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex == STRING_END)
      {
        Array_append(&clientInfo->entryIdArray,&entryId);
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PARSE,"%S",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(entryIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(entryIds);
}

/***********************************************************************\
* Name   : serverCommand_entryListRemove
* Purpose: remove from selected entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entryId=<id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_entryListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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

  // check if index database is available, check if index database is ready
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
      entryId = (IndexId)String_toInteger64(token,STRING_BEGIN,&nextIndex,NULL,0);
      if (nextIndex == STRING_END)
      {
        Array_removeAll(&clientInfo->entryIdArray,&entryId,CALLBACK_(NULL,NULL));
      }
      else
      {
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_PARSE,"%S",token);
        String_doneTokenizer(&stringTokenizer);
        String_delete(entryIds);
        Semaphore_unlock(&clientInfo->lock);
        return;
      }
    }
  }
  String_doneTokenizer(&stringTokenizer);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(entryIds);
}

/***********************************************************************\
* Name   : serverCommand_entryListInfo
* Purpose: get restore entry list info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  error = Index_getEntriesInfo(indexHandle,
                               NULL,  // storageIds
                               0,  // storageIdCount
                               Array_cArray(&clientInfo->entryIdArray),
                               Array_length(&clientInfo->entryIdArray),
                               INDEX_TYPE_SET_ANY_ENTRY,
                               NULL,  // name,
                               FALSE,  // newestOnly
                               &totalEntryCount,
                               &totalEntrySize,
                               &totalEntryContentSize
                              );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get entries info from index database fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"totalEntryCount=%lu totalEntrySize=%"PRIu64" totalEntryContentSize=%"PRIu64"",totalEntryCount,totalEntrySize,totalEntryContentSize);
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

  // get uuid, job id, and/or storage id
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
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
  if      (!String_isEmpty(jobUUID))
  {
    JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = Job_findByUUID(jobUUID);
      if (jobNode != NULL)
      {
        error = mountAll(&jobNode->job.options.mountList);
      }
    }
  }
  else if (entityId != INDEX_ID_NONE)
  {
    if (Index_findEntity(indexHandle,
                         entityId,
                         NULL,  // jobUUID
                         NULL,  // scheduleUUID
                         NULL,  // hostName
                         ARCHIVE_TYPE_NONE,
                         0LL,  // createdDateTime
                         uuid,
                         NULL,  // scheduleUUID
                         NULL,  // uuidIndexId
                         NULL,  // entityIndexId
                         NULL,  // archiveType
                         NULL,  // createdDateTime
                         NULL,  // lastErrorMessage
                         NULL,  // totalEntryCount
                         NULL  // totalEntrySize
                        )
       )
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
    else
    {
      error = ERROR_DATABASE_INDEX_NOT_FOUND;
    }
  }
  else if (storageId != INDEX_ID_NONE)
  {
    if (Index_findStorageById(indexHandle,
                              storageId,
                              uuid,
                              NULL,  // scheduleUUID
                              NULL,  // uuidIndexId
                              NULL,  // entityIndexId
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
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
      {
        jobNode = Job_findByUUID(uuid);
        if (jobNode != NULL)
        {
          error = mountAll(&jobNode->job.options.mountList);
        }
      }
    }
    else
    {
      error = ERROR_DATABASE_INDEX_NOT_FOUND;
    }
  }
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

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

  if (entityId != INDEX_ID_NONE)
  {
    // delete entity
    error = deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    // delete storage file
    error = deleteStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
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
*            restoreEntryMode=STOP|RENAME|OVERWRITE
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
    else
    {
      return FALSE;
    }
  }

  /***********************************************************************\
  * Name   : restoreUpdateStatusInfo
  * Purpose: update restore status info
  * Input  : statusInfo - status info data,
  *          userData   - user data
  * Output : -
  * Return : TRUE to continue, FALSE to abort
  * Notes  : -
  \***********************************************************************/

  auto void restoreUpdateStatusInfo(const StatusInfo *statusInfo,
                                    void             *userData
                                   );
  void restoreUpdateStatusInfo(const StatusInfo *statusInfo,
                               void             *userData
                              )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;

    assert(restoreCommandInfo != NULL);
    assert(statusInfo != NULL);
    assert(statusInfo->storage.name != NULL);
    assert(statusInfo->entry.name != NULL);

    ServerIO_sendResult(&restoreCommandInfo->clientInfo->io,
                        restoreCommandInfo->id,
                        FALSE,
                        ERROR_NONE,
                        "state=RUNNING doneCount=%lu doneSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" entryName=%'S entryDoneSize=%"PRIu64" entryTotalSize=%"PRIu64" storageName=%'S storageDoneSize=%"PRIu64" storageTotalSize=%"PRIu64"",
                        statusInfo->done.count,
                        statusInfo->done.size,
                        statusInfo->total.count,
                        statusInfo->total.size,
                        statusInfo->entry.name,
                        statusInfo->entry.doneSize,
                        statusInfo->entry.totalSize,
                        statusInfo->storage.name,
                        statusInfo->storage.doneSize,
                        statusInfo->storage.totalSize
                       );
  }

  /***********************************************************************\
  * Name   : restoreHandleError
  * Purpose: handle restore error
  * Input  : error      - error code
  *          statusInfo - status info data,
  *          userData   - user data
  * Output : -
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  auto Errors restoreHandleError(Errors           error,
                                 const StatusInfo *statusInfo,
                                 void             *userData
                                );
  Errors restoreHandleError(Errors           error,
                            const StatusInfo *statusInfo,
                            void             *userData
                           )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;
    StringMap          resultMap;
    ServerIOActions    action;

    assert(restoreCommandInfo != NULL);
    assert(restoreCommandInfo->clientInfo != NULL);
    assert(statusInfo != NULL);
    assert(statusInfo->storage.name != NULL);
    assert(statusInfo->entry.name != NULL);

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
                                "type=RESTORE errorCode=%d errorData=%'s storageName=%'S entryName=%'S message=%'S",
                                error,
                                Error_getText(error),
                                statusInfo->storage.name,
                                statusInfo->entry.name,
                                statusInfo->message
                               ) != ERROR_NONE
         )
      {
        StringMap_delete(resultMap);
        return error;
      }
      if (!StringMap_getEnum(resultMap,"action",&action,(StringMapParseEnumFunction)ServerIO_parseAction,SERVER_IO_ACTION_NONE))
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
                                  60*1000,
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
    if (!StringMap_getEnum(resultMap,"encryptType",&encryptType,(StringMapParseEnumFunction)ServerIO_parseEncryptType,SERVER_IO_ENCRYPT_TYPE_NONE))
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

  Types              type;
  bool               directoryContentFlag;
  String             storageName;
  IndexId            entryId;
  String             entryName;
  StringList         storageNameList;
  EntryList          includeEntryList;
  IndexQueryHandle   indexQueryHandle;
  String             byName;
  RestoreCommandInfo restoreCommandInfo;
  Errors             error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get type, destination, directory content flag, overwrite flag
  if (!StringMap_getEnum(argumentMap,"type",&type,(StringMapParseEnumFunction)parseRestoreType,UNKNOWN))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"type=ARCHIVES|ENTRIES");
    return;
  }
  if (!StringMap_getString(argumentMap,"destination",clientInfo->jobOptions.destination,NULL))
  {
    String_delete(clientInfo->jobOptions.destination);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"destination=<name>");
    return;
  }
  StringMap_getBool(argumentMap,"directoryContent",&directoryContentFlag,FALSE);
  if (!StringMap_getEnum(argumentMap,"restoreEntryMode",&clientInfo->jobOptions.restoreEntryMode,(StringMapParseEnumFunction)parseRestoreEntryMode,RESTORE_ENTRY_MODE_STOP))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"restoreEntryMode=STOP|RENAME|OVERWRITE");
    return;
  }

  // init variables
  storageName = String_new();
  entryName   = String_new();

  // get storage/entry list
  error = ERROR_NONE;
  StringList_init(&storageNameList);
  EntryList_init(&includeEntryList);
  switch (type)
  {
    case ARCHIVES:
      if (error == ERROR_NONE)
      {
        error = Index_initListStorages(&indexQueryHandle,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidIndexId
                                       INDEX_ID_ANY,  // entityIndexId
                                       NULL,  // jobUUID
                                       NULL,  // scheduleUUID,
                                       Array_cArray(&clientInfo->indexIdArray),
                                       Array_length(&clientInfo->indexIdArray),
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
          while (   !isCommandAborted(clientInfo,id)
                 && Index_getNextStorage(&indexQueryHandle,
                                         NULL,  // uuidIndexId
                                         NULL,  // jobUUID
                                         NULL,  // entityIndexId
                                         NULL,  // scheduleUUID
                                         NULL,  // hostName
                                         NULL,  // archiveType
                                         NULL,  // storageId
                                         storageName,
                                         NULL,  // createdDateTime
                                         NULL,  // size
                                         NULL,  // userName
                                         NULL,  // comment
                                         NULL,  // totalEntrySize
                                         NULL,  // indexState,
                                         NULL,  // indexMode,
                                         NULL,  // lastCheckedDateTime,
                                         NULL,  // errorMessage
                                         NULL  // totalEntryCount
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
                                    INDEX_ENTRY_SORT_MODE_NONE,
                                    DATABASE_ORDERING_NONE,
                                    FALSE,  // newestOnly,
                                    0,
                                    INDEX_UNLIMITED
                                   );
      if (error == ERROR_NONE)
      {
        while (   !isCommandAborted(clientInfo,id)
               && Index_getNextEntry(&indexQueryHandle,
                                     NULL,  // uuidIndexId
                                     NULL,  // jobUUID,
                                     NULL,  // entityIndexId
                                     NULL,  // scheduleUUID,
                                     NULL,  // hostName
                                     NULL,  // userName
                                     NULL,  // archiveType,
                                     NULL,  // storageId,
                                     storageName,  // storageName
                                     NULL,  // storageDateTime
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
          if (!StringList_contains(&storageNameList,storageName))
          {
            StringList_append(&storageNameList,storageName);
          }
          EntryList_append(&includeEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB,NULL);
          if (directoryContentFlag && (Index_getType(entryId) == INDEX_TYPE_DIRECTORY))
          {
            String_appendCString(entryName,"/*");
            EntryList_append(&includeEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB,NULL);
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
    EntryList_done(&includeEntryList);
    StringList_done(&storageNameList);
    String_delete(entryName);
    String_delete(storageName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
    return;
  }

  // restore
  byName = String_format(String_new(),"%s:%u",String_cString(clientInfo->io.network.name),clientInfo->io.network.port);
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Started restore%s%s: %d archives/%d entries",
             !String_isEmpty(byName) ? " by " : "",
             String_cString(byName),
             List_count(&storageNameList),
             List_count(&includeEntryList)
            );
  restoreCommandInfo.clientInfo  = clientInfo;
  restoreCommandInfo.id          = id;
  restoreCommandInfo.skipAllFlag = FALSE;
  restoreCommandInfo.abortFlag   = FALSE;
  error = Command_restore(&storageNameList,
                          &includeEntryList,
                          NULL,  // excludePatternList
                          &clientInfo->jobOptions,
                          STORAGE_FLAGS_NONE,
                          CALLBACK_(restoreUpdateStatusInfo,&restoreCommandInfo),
                          CALLBACK_(restoreHandleError,&restoreCommandInfo),
                          CALLBACK_(getNamePassword,&restoreCommandInfo),
                          CALLBACK_(NULL,NULL),  // isPause callback
                          CALLBACK__INLINE(bool,(void *userData),
                          {
                            UNUSED_VARIABLE(userData);
                            return isCommandAborted(clientInfo,id);
                          },NULL),
                          NULL  // logHandle
                         );
  if (error == ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"state=RESTORED");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
  }
  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Done restore%s%s",
             !String_isEmpty(byName) ? " by " : "",
             String_cString(byName)
            );
  String_delete(byName);

  // free resources
  EntryList_done(&includeEntryList);
  StringList_done(&storageNameList);
  String_delete(entryName);
  String_delete(storageName);
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
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*            [name=<text>]
*          Result:
*            uuidId=<n>
*            jobUUID=<uuid> \
*            name=<name> \
*            lastCreatedDateTime=<time stamp [s]> \
*            lastErrorMessage=<text> \
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
    String  lastErrorMessage;
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

    String_delete(uuidNode->lastErrorMessage);
    String_delete(uuidNode->jobUUID);
  }

  bool             indexStateAny;
  IndexStateSet    indexStateSet;
  bool             indexModeAny;
  IndexModeSet     indexModeSet;
  String           name;
  UUIDList         uuidList;
  String           lastErrorMessage;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64           lastExecutedDateTime;
  uint64           totalSize;
  ulong            totalEntryCount;
  uint64           totalEntrySize;
  UUIDNode         *uuidNode;
  const JobNode    *jobNode;
  bool             exitsFlag;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter index state set, index mode set, name
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexStateSet=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet","*"),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexModeSet=MANUAL|AUTO|*");
    return;
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
  List_init(&uuidList);
  lastErrorMessage = String_new();

  // get UUIDs from database (Note: use list to avoid dead-lock in job list)
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
    String_delete(lastErrorMessage);
    List_done(&uuidList,(ListNodeFreeFunction)freeUUIDNode,NULL);

    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init uuid list fail");

    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextUUID(&indexQueryHandle,
                              &uuidId,
                              jobUUID,
                              &lastExecutedDateTime,
                              lastErrorMessage,
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
    uuidNode->lastErrorMessage     = String_duplicate(lastErrorMessage);
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
                          "uuidId=%"PRIu64" jobUUID=%S name=%'S lastExecutedDateTime=%"PRIu64" lastErrorMessage=%'S totalSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                          uuidNode->uuidId,
                          uuidNode->jobUUID,
                          name,
                          uuidNode->lastExecutedDateTime,
                          uuidNode->lastErrorMessage,
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
                            "uuidId=0 jobUUID=%S name=%'S lastExecutedDateTime=0 lastErrorMessage='' totalSize=0 totalEntryCount=0 totalEntrySize=0",
                            jobNode->job.uuid,
                            jobNode->name
                           );
      }
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorMessage);
  List_done(&uuidList,(ListNodeFreeFunction)freeUUIDNode,NULL);
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
*            [jobUUID=<uuid>]
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*            [name=<text>]
*          Result:
*            jobUUID=<uuid> \
*            scheduleUUID=<uuid> \
*            entityId=<id> \
*            archiveType=<type> \
*            createdDateTime=<time stamp [s]> \
*            lastErrorMessage=<error message>
*            totalSize=<n> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            expireDateTime=<time stamp [s]>
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexEntityList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString       (jobUUID,MISC_UUID_STRING_LENGTH);
  bool               indexStateAny;
  IndexStateSet      indexStateSet;
  bool               indexModeAny;
  IndexModeSet       indexModeSet;
  String             name;
  Errors             error;
  IndexQueryHandle   indexQueryHandle;
  IndexId            uuidId;
  String             jobName;
  IndexId            entityId;
  StaticString       (scheduleUUID,MISC_UUID_STRING_LENGTH);
  uint64             createdDateTime;
  ArchiveTypes       archiveType;
  String             lastErrorMessage;
  uint64             totalSize;
  ulong              totalEntryCount;
  uint64             totalEntrySize;
  int                maxAge;
  const JobNode      *jobNode;
  const ScheduleNode *scheduleNode;
  uint64             expireDateTime;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get uuid, index state set, index mode set, name, name
  StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL);
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexStateSet=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet","*"),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexModeSet=MANUAL|AUTO|*");
    return;
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

  // initialize variables
  jobName          = String_new();
  lastErrorMessage = String_new();

  // get entities
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidIndexId
                                 jobUUID,
                                 NULL,  // scheduldUUID
                                 ARCHIVE_TYPE_ANY,
                                 indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                 indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                 name,
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init entity list fail");
    String_delete(lastErrorMessage);
    String_delete(jobName);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntity(&indexQueryHandle,
                                &uuidId,
                                jobUUID,
                                scheduleUUID,
                                &entityId,
                                &archiveType,
                                &createdDateTime,
                                lastErrorMessage,
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
                        "uuid=%"PRIu64" jobUUID=%S jobName=%'S scheduleUUID=%S entityId=%"PRIindexId" archiveType=%s createdDateTime=%"PRIu64" lastErrorMessage=%'S totalSize=%"PRIu64" totalEntryCount=%lu totalEntrySize=%"PRIu64" expireDateTime=%"PRIu64"",
                        uuidId,
                        jobUUID,
                        jobName,
                        scheduleUUID,
                        entityId,
                        Archive_archiveTypeToString(archiveType),
                        createdDateTime,
                        lastErrorMessage,
                        totalSize,
                        totalEntryCount,
                        totalEntrySize,
                        expireDateTime
                       );
  }
  Index_doneList(&indexQueryHandle);

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorMessage);
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
*            entityId=<id>|0|*
*            [jobUUID=<uuid>|*|""]
*            [scheduleUUID=<uuid>|*|""]
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
  uint64                n;
  IndexId               entityId;
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  bool                  jobUUIDAny;
  StaticString          (scheduleUUID,MISC_UUID_STRING_LENGTH);
  bool                  scheduleUUIDAny;
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
  ArchiveTypes          archiveType;
  uint64                dateTime;
  uint64                size;
  IndexStates           indexState;
  IndexModes            indexMode;
  uint64                lastCheckedDateTime;
  ulong                 totalEntryCount;
  uint64                totalEntrySize;
  const JobNode         *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get jobUUID, scheduleUUID, entity id, filter storage pattern, index state set, index mode set, name, offset, limit
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<id>");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"jobUUID","*"),"*"))
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
  if      (stringEquals(StringMap_getTextCString(argumentMap,"scheduleUUID","*"),"*"))
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexStateSet=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet","*"),"*"))
  {
    indexModeAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexModeSet=MANUAL|AUTO|*");
    return;
  }
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);
  StringMap_getEnum(argumentMap,"sortMode",&sortMode,(StringMapParseEnumFunction)Index_parseStorageSortMode,INDEX_STORAGE_SORT_MODE_NAME);
  StringMap_getEnum(argumentMap,"ordering",&ordering,(StringMapParseEnumFunction)Index_parseOrdering,DATABASE_ORDERING_NONE);

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
                                 INDEX_ID_ANY,  // uuidIndexId
                                 entityId,
                                 !jobUUIDAny ? jobUUID : NULL,
                                 !scheduleUUIDAny ? scheduleUUID : NULL,
                                 NULL,  // indexIds
                                 0,  // indexIdCount
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init storage list fail");
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
                                 hostName,
                                 &archiveType,
                                 &storageId,
                                 storageName,
                                 &dateTime,
                                 &size,
                                 NULL,  // userName
                                 NULL,  // comment
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
                        "uuidId=%"PRIu64" jobUUID=%S jobName=%'S entityId=%"PRIu64" scheduleUUID=%S hostName=%'S archiveType='%s' storageId=%"PRIu64" name=%'S dateTime=%"PRIu64" size=%"PRIu64" indexState=%'s indexMode=%'s lastCheckedDateTime=%"PRIu64" errorMessage=%'S totalEntryCount=%lu totalEntrySize=%"PRIu64"",
                        uuidId,
                        jobUUID,
                        jobName,
                        entityId,
                        scheduleUUID,
                        hostName,
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
* Name   : serverCommand_indexEntryList
* Purpose: get index database entry list
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*
*            newestOnly=yes|no
*            name=<text>
*            [offset=<n>]
*            [limit=<n>]
*            [sortMode=ARCHIVE|NAME|TYPE|SIZE|LAST_CHANGED]
*            [ordering=ASCENDING|DESCENDING]
*          Result:
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
*            storageName=<name> storageDateTime=<time stamp> \
*            entryId=<n> name=<name> entryType=FILE size=<n [bytes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n> fragmentOffset=<n [bytes]> fragmentSize=<n [bytes]>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            storageName=<name> storageDateTime=<time stamp> \
+            entryId=<n> entryType=IMAGE name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            blockOffset=<n [bytes]> blockCount=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            storageName=<name> storageDateTime=<time stamp> \
+            entryId=<n> entryType=DIRECTORY name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            storageName=<name> storageDateTime=<time stamp> \
+            entryId=<n> entryType=LINK linkName=<name> name=<name> \
*            dateTime=<time stamp> userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            storageName=<name> storageDateTime=<time stamp> \
+            entryId=<n> entryType=HARDLINK name=<name> dateTime=<time stamp>
*            userId=<n> groupId=<n> permission=<n> fragmentOffset=<n [bytes]> fragmentSize=<n [bytes]>
*
*            jobName=<name> \
+            archiveType=<type> \
*            hostName=<name> \
+            storageName=<name> storageDateTime=<time stamp> \
+            entryId=<n> entryType=SPECIAL name=<name> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
\***********************************************************************/

LOCAL void serverCommand_indexEntryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  #define SEND_FILE_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=FILE name=%'S size=%"PRIu64" dateTime=%"PRIu64" userId=%u groupId=%u permission=%u fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
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
  #define SEND_IMAGE_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,fileSystemType,size,blockOffset,blockCount) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=IMAGE name=%'S fileSystemType=%s size=%"PRIu64" blockOffset=%"PRIu64" blockCount=%"PRIu64"", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
                          storageName, \
                          storageDateTime, \
                          entryId, \
                          name, \
                          FileSystem_fileSystemTypeToString(fileSystemType,NULL), \
                          size, \
                          blockOffset, \
                          blockCount \
                         ); \
    } \
    while (0)
  #define SEND_DIRECTORY_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=DIRECTORY name=%'S size=%"PRIu64" dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
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
  #define SEND_LINK_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,destinationName,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=LINK name=%'S destinationName=%'S dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
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
  #define SEND_HARDLINK_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=HARDLINK name=%'S size=%lld dateTime=%"PRIu64" userId=%u groupId=%u permission=%u fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64"", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
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
  #define SEND_SPECIAL_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,name,dateTime,userId,groupId,permission) \
    do \
    { \
      ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE, \
                          "jobName=%'S archiveType=%s hostName=%'S storageName=%'S storageDateTime=%"PRIu64" entryId=%"PRIindexId" entryType=SPECIAL name=%'S dateTime=%"PRIu64" userId=%u groupId=%u permission=%u", \
                          jobName, \
                          Archive_archiveTypeToString(archiveType), \
                          hostName, \
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

  String                name;
  bool                  indexTypeAny;
  IndexTypes            indexType;
  bool                  newestOnly;
  uint64                offset;
  uint64                limit;
  IndexStorageSortModes sortMode;
  DatabaseOrdering      ordering;
  IndexId               prevUUIDId;
  String                jobName;
  String                hostName;
  String                storageName;
  uint64                storageDateTime;
  String                entryName;
  String                destinationName;
  Errors                error;
  IndexQueryHandle      indexQueryHandle;
  FileSystemTypes       fileSystemType;
  IndexId               uuidId,entityId,storageId,entryId;
  StaticString          (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes          archiveType;
  uint64                size;
  uint64                timeModified;
  uint                  userId,groupId;
  uint                  permission;
  uint64                fragmentOrBlockOffset,fragmentSizeOrBlockCount;
  const JobNode         *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter entry pattern, index type, new entries only, offset, limit
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*");
    return;
  }
  if (!StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"newestOnly=yes|no");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<text>");
    String_delete(name);
    return;
  }
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);
  StringMap_getEnum(argumentMap,"sortMode",&sortMode,(StringMapParseEnumFunction)Index_parseEntrySortMode,INDEX_ENTRY_SORT_MODE_NAME);
  StringMap_getEnum(argumentMap,"ordering",&ordering,(StringMapParseEnumFunction)Index_parseOrdering,DATABASE_ORDERING_NONE);

  // check if index database is available, check if index database is ready
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
                                Array_cArray(&clientInfo->indexIdArray),
                                Array_length(&clientInfo->indexIdArray),
                                NULL,  // entryIds
                                0,  // entryIdCount
                                indexTypeAny ? INDEX_TYPE_SET_ANY_ENTRY : SET_VALUE(indexType),
                                name,
                                sortMode,
                                ordering,
                                newestOnly,
                                offset,
                                limit
                               );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list entries fail");
    String_delete(destinationName);
    String_delete(entryName);
    String_delete(storageName);
    String_delete(hostName);
    String_delete(jobName);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntry(&indexQueryHandle,
                               &uuidId,
                               jobUUID,
                               &entityId,
                               NULL,  // scheduleUUID,
                               hostName,
                               NULL,  // userName
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
    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        SEND_FILE_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_IMAGE:
        SEND_IMAGE_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,fileSystemType,size,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_DIRECTORY:
        SEND_DIRECTORY_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_LINK:
        SEND_LINK_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,destinationName,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_HARDLINK:
        SEND_HARDLINK_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_SPECIAL:
        SEND_SPECIAL_ENTRY(jobName,archiveType,hostName,storageName,storageDateTime,entryId,entryName,timeModified,userId,groupId,permission);
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
  ulong            totalEntryCount;
  uint64           totalEntrySize;
  ulong            skippedEntryCount;
  uint64           skippedEntrySize;
  ulong            errorEntryCount;
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
                                 INDEX_ID_ANY,  // uuidIndexId
                                 jobUUID,
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init history list fail");
    String_delete(errorMessage);
    String_delete(hostName);
    String_delete(jobName);
    return;
  }

  while (   !isCommandAborted(clientInfo,id)
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
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [createdDateTime=<time stamp [s]>]
*          Result:
*            entityId=<id>
\***********************************************************************/

LOCAL void serverCommand_indexEntityAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       hostName;
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  IndexId      entityId;
  Errors       error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get jobUUID, archive type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid>");
    return;
  }
  StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL);
  hostName = String_new();
  StringMap_getString(argumentMap,"hostName",hostName,NULL);
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_UNKNOWN))
  {
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
                          jobUUID,
                          scheduleUUID,
                          hostName,
                          archiveType,
                          createdDateTime,
                          FALSE,  // not locked
                          &entityId
                         );
  if (error != ERROR_NONE)
  {
    String_delete(hostName);
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"add entity fail");
    return;
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"entityId=%"PRIu64"",entityId);

  // free resources
  String_delete(hostName);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageAdd
* Purpose: add storage to index database (if not already exists)
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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
  StorageInfo      storageInfo;
  String           printableStorageName;
  IndexId          storageId;
  Errors           error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get pattern type, pattern, force refresh
  pattern = String_new();
  if (!StringMap_getString(argumentMap,"pattern",pattern,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"pattern=<text>");
    String_delete(pattern);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);
  StringMap_getBool(argumentMap,"forceRefresh",&forceRefresh,FALSE);

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(pattern);
    return;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);

  error = ERROR_UNKNOWN;

  // try to open as storage file
  if (error != ERROR_NONE)
  {
    if (   (Storage_parseName(&storageSpecifier,pattern) == ERROR_NONE)
        && !Storage_isPatternSpecifier(&storageSpecifier)
       )
    {
      if (Storage_init(&storageInfo,
                       NULL, // masterIO
                       &storageSpecifier,
                       NULL, // jobOptions
                       &globalOptions.indexDatabaseMaxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_LOW,
                       STORAGE_FLAGS_NONE,
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
                       CALLBACK_(NULL,NULL),  // getNamePassword
                       CALLBACK_(NULL,NULL),  // requestVolume
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      ) == ERROR_NONE
         )
      {
        printableStorageName = Storage_getPrintableName(String_new(),&storageSpecifier,NULL);

        if (Index_findStorageByName(indexHandle,
                                    &storageSpecifier,
                                    NULL,  // archiveName
                                    NULL,  // uuidIndexId
                                    NULL,  // entityIndexId
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
            error = Index_setState(indexHandle,
                                   storageId,
                                   INDEX_STATE_UPDATE_REQUESTED,
                                   Misc_getCurrentDateTime(),
                                   NULL
                                  );
            if (error == ERROR_NONE)
            {
              ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                  storageId,
                                  printableStorageName
                                 );
            }
          }
          else
          {
            error = ERROR_NONE;
          }
        }
        else
        {
          error = Index_newStorage(indexHandle,
                                   INDEX_ID_NONE, // entityIndexId
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
        }
        Storage_done(&storageInfo);

        String_delete(printableStorageName);
      }
    }
  }

  // try to open as directory: add all matching entries
  if (error != ERROR_NONE)
  {
    error = Storage_forAll(pattern,
                           CALLBACK__INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                           {
                             String printableStorageName;

                             UNUSED_VARIABLE(fileInfo);
                             UNUSED_VARIABLE(userData);

                             error = Storage_parseName(&storageSpecifier,storageName);
                             if (error == ERROR_NONE)
                             {
                               if (String_endsWithCString(storageSpecifier.archiveName,FILE_NAME_EXTENSION_ARCHIVE_FILE))
                               {
                                 printableStorageName = Storage_getPrintableName(String_new(),&storageSpecifier,storageName);

                                 if (Index_findStorageByName(indexHandle,
                                                             &storageSpecifier,
                                                             NULL,  // archiveName
                                                             NULL,  // uuidIndexId
                                                             NULL,  // entityIndexId
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
                                     error = Index_setState(indexHandle,
                                                            storageId,
                                                            INDEX_STATE_UPDATE_REQUESTED,
                                                            Misc_getCurrentDateTime(),
                                                            NULL
                                                           );
                                     if (error == ERROR_NONE)
                                     {
                                       ServerIO_sendResult(&clientInfo->io,id,FALSE,ERROR_NONE,"storageId=%"PRIu64" name=%'S",
                                                           storageId,
                                                           printableStorageName
                                                          );
                                     }
                                   }
                                   else
                                   {
                                     error = ERROR_NONE;
                                   }
                                 }
                                 else
                                 {
                                   error = Index_newStorage(indexHandle,
                                                            INDEX_ID_NONE, // entityIndexId
                                                            NULL,  // hostName
                                                            NULL,  // userName
                                                            storageName,
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
                                 }
                                 if (error != ERROR_NONE)
                                 {
                                   return error;
                                 }

                                 String_delete(printableStorageName);
                               }
                             }

                             return !isCommandAborted(clientInfo,id) ? ERROR_NONE : ERROR_ABORTED;
                           },NULL)
                          );
  }

  // trigger index thread
  Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

  if (error == ERROR_NONE)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(pattern);
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

  // get toJobUUID, toScheduleUUID, toEntityId, archive type, jobUUID/entityId/storageId
  String_clear(toJobUUID);
  toEntityId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"toJobUUID",toJobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"toEntityId",&toEntityId,INDEX_ID_NONE)
     )
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"toJobUUID=<uuid> or toEntityId=<id>");
    return;
  }
  StringMap_getString(argumentMap,"toScheduleUUID",toScheduleUUID,NULL);
  toHostName = String_new();
  StringMap_getString(argumentMap,"toHostName",toHostName,NULL);
  StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE);
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
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
    if      (toEntityId != INDEX_ID_NONE)
    {
      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityIndexId
                             INDEX_ID_NONE,  // storageId
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              toJobUUID,
                              toScheduleUUID,
                              toHostName,
                              archiveType,
                              createdDateTime,
                              TRUE,  // locked
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"cannot create entity for %S",toJobUUID);
        return;
      }

      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityIndexId
                             INDEX_ID_NONE,  // storageId
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
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

  if (entityId != INDEX_ID_NONE)
  {
    // assign all storages/entity
    if      (toEntityId != INDEX_ID_NONE)
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
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
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
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
        return;
      }
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    // assign storage
    if      (toEntityId != INDEX_ID_NONE)
    {
      // assign storage to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityIndexId
                             storageId,
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              toJobUUID,
                              toScheduleUUID,
                              toHostName,
                              archiveType,
                              createdDateTime,
                              TRUE,  // locked
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"cannot create entity for %S",toJobUUID);
        return;
      }

      // assign storage to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityIndexId
                             storageId,
                             NULL,  // toJobUUID
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        String_delete(toHostName);
        ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"assign storage fail");
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
*            state=<state>|*
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"filter state=OK|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  name = String_new();
  if (   !StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
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
  if ((uuidId == INDEX_ID_NONE) && (storageId == INDEX_ID_NONE) && (entityId == INDEX_ID_NONE) && String_isEmpty(jobUUID) && String_isEmpty(name))
  {
    // refresh all storage with specific state
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidIndexId
                                   INDEX_ID_ANY,  // entity id
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // entityIndexId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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

  if (uuidId != INDEX_ID_NONE)
  {
    // refresh all storage of uuid
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   uuidId,
                                   INDEX_ID_ANY,  // entityIndexId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // entityIndexId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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

  if (entityId != INDEX_ID_NONE)
  {
    // refresh all storage of entity
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidIndexId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // entityIndexId
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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

  if (storageId != INDEX_ID_NONE)
  {
    Array_append(&storageIdArray,&storageId);
  }

  if (!String_isEmpty(jobUUID))
  {
    // refresh all storage of all entities of job
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidIndexId
                                   INDEX_ID_ANY,  // entity id
                                   jobUUID,
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // entityIndexId
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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
                                   INDEX_ID_ANY,  // uuidIndexId
                                   INDEX_ID_ANY,  // entityIndexId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // entityIndexId
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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
    error = Index_setState(indexHandle,
                           storageId,
                           INDEX_STATE_UPDATE_REQUESTED,
                           0LL,
                           NULL
                          );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"set storage state fail");
      Array_done(&storageIdArray);
      String_delete(storageName);
      String_delete(name);
      return;
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // trigger index thread
  Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);

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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"filter state=OK|UPDATE_REQUESTED|UPDATE|ERROR|*");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
                                   INDEX_ID_ANY,  // uuidIndexId
                                   INDEX_ID_ANY, // entity id
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
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
      ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE,"init list storage fail");
      String_delete(printableStorageName);
      String_delete(storageName);
      Storage_doneSpecifier(&storageSpecifier);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidIndexId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // hostName
                                   NULL,  // entityIndexId
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // userName
                                   NULL,  // comment
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

        // delete from index
        error = Index_deleteStorage(indexHandle,storageId);
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

  if (uuidId != INDEX_ID_NONE)
  {
    // delete UUID index
    error = Index_deleteUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&uuidId,CALLBACK_(NULL,NULL));
    }
  }

  if (entityId != INDEX_ID_NONE)
  {
    // delete entity
    error = Index_deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&entityId,CALLBACK_(NULL,NULL));
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    // delete storage index
    error = Index_deleteStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"");
      return;
    }

    // remove index id
    SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      Array_removeAll(&clientInfo->indexIdArray,&storageId,CALLBACK_(NULL,NULL));
    }
  }

  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_indexStoragesInfo
* Purpose: get index database storage info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>|0|*
*            [jobUUID=<uuid>|*|""]
*            [scheduleUUID=<uuid>|*|""]
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
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  bool          jobUUIDAny;
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  bool          scheduleUUIDAny;
  bool          indexStateAny;
  IndexStateSet indexStateSet;
  bool          indexModeAny;
  IndexModeSet  indexModeSet;
  String        name;
  Errors        error;
  ulong         storageCount,totalEntryCount;
  uint64        totalEntrySize,totalEntryContentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // get jobUUID, scheduleUUID, entryId, index state set, index mode set, name
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<id>");
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"jobUUID","*"),"*"))
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
  if      (stringEquals(StringMap_getTextCString(argumentMap,"scheduleUUID","*"),"*"))
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
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<text>");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexState=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexModeSet=MANUAL|AUTO|*");
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

  // get index info
  error = Index_getStoragesInfos(indexHandle,
                                 INDEX_ID_ANY,  // uuidIndexId
                                 entityId,
                                 !jobUUIDAny ? jobUUID : NULL,
                                 !scheduleUUIDAny ? scheduleUUID : NULL,
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get storages info from index database fail");
    String_delete(name);
    return;
  }

  // send data
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"storageCount=%lu totalEntryCount=%lu totalEntrySize=%"PRIu64" totalEntryContentSize=%"PRIu64"",storageCount,totalEntryCount,totalEntrySize,totalEntryContentSize);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntriesInfo
* Purpose: get index database entries info
* Input  : clientInfo  - client info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<text>");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*");
    String_delete(name);
    return;
  }
  if (!StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE))
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"newestOnly=yes|no");
    String_delete(name);
    return;
  }

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
    ServerIO_sendResult(&clientInfo->io,id,TRUE,error,"get entries info index database fail");
    String_delete(name);
    return;
  }

  // send data
  ServerIO_sendResult(&clientInfo->io,id,TRUE,ERROR_NONE,"totalEntryCount=%lu totalEntrySize=%"PRIu64" totalEntryContentSize=%"PRIu64"",totalEntryCount,totalEntrySize,totalEntryContentSize);

  // free resources
  String_delete(name);
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

  Array_debugPrintInfo(CALLBACK__INLINE(bool,(const Array *array, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
  String_debugPrintInfo(CALLBACK__INLINE(bool,(ConstString string, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
  File_debugPrintInfo(CALLBACK__INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
                      CALLBACK__INLINE(bool,(const Array *array, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
                       CALLBACK__INLINE(bool,(ConstString string, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
                     CALLBACK__INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
                        CALLBACK__INLINE(bool,(const char *variableName, const void *resource, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
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
  { "START_SSL",                   serverCommand_startSSL,                 AUTHORIZATION_STATE_WAITING                           },
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

  { "SERVER_LIST",                 serverCommand_serverList,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_ADD",             serverCommand_serverListAdd,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_UPDATE",          serverCommand_serverListUpdate,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SERVER_LIST_REMOVE",          serverCommand_serverListRemove,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

//TODO: obsolete?
  { "GET",                         serverCommand_get,                      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ABORT",                       serverCommand_abort,                    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STATUS",                      serverCommand_status,                   AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PAUSE",                       serverCommand_pause,                    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SUSPEND",                     serverCommand_suspend,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "CONTINUE",                    serverCommand_continue,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEVICE_LIST",                 serverCommand_deviceList,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ROOT_LIST",                   serverCommand_rootList,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_INFO",                   serverCommand_fileInfo,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_LIST",                   serverCommand_fileList,                 AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_GET",          serverCommand_fileAttributeGet,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_SET",          serverCommand_fileAttributeSet,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FILE_ATTRIBUTE_CLEAR",        serverCommand_fileAttributeClear,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
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
  { "SCHEDULE_LIST_ADD",           serverCommand_scheduleListAdd,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_LIST_REMOVE",        serverCommand_scheduleListRemove,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_OPTION_GET",         serverCommand_scheduleOptionGet,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_OPTION_SET",         serverCommand_scheduleOptionSet,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_OPTION_DELETE",      serverCommand_scheduleOptionDelete,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SCHEDULE_TRIGGER",            serverCommand_scheduleTrigger,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "PERSISTENCE_LIST",            serverCommand_persistenceList,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_CLEAR",      serverCommand_persistenceListClear,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_ADD",        serverCommand_persistenceListAdd,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_UPDATE",     serverCommand_persistenceListUpdate,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PERSISTENCE_LIST_REMOVE",     serverCommand_persistenceListRemove,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "DECRYPT_PASSWORD_CLEAR",      serverCommand_decryptPasswordsClear,    AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DECRYPT_PASSWORD_ADD",        serverCommand_decryptPasswordAdd,       AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "FTP_PASSWORD",                serverCommand_ftpPassword,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "SSH_PASSWORD",                serverCommand_sshPassword,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "WEBDAV_PASSWORD",             serverCommand_webdavPassword,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "CRYPT_PASSWORD",              serverCommand_cryptPassword,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "PASSWORDS_CLEAR",             serverCommand_passwordsClear,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "VOLUME_LOAD",                 serverCommand_volumeLoad,               AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "VOLUME_UNLOAD",               serverCommand_volumeUnload,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "ARCHIVE_LIST",                serverCommand_archiveList,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

//TODO: replace by single list?
  { "STORAGE_LIST",                serverCommand_storageList,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STORAGE_LIST_CLEAR",          serverCommand_storageListClear,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STORAGE_LIST_ADD",            serverCommand_storageListAdd,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STORAGE_LIST_REMOVE",         serverCommand_storageListRemove,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "STORAGE_LIST_INFO",           serverCommand_storageListInfo,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ENTRY_LIST",                  serverCommand_entryList,                AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ENTRY_LIST_CLEAR",            serverCommand_entryListClear,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ENTRY_LIST_ADD",              serverCommand_entryListAdd,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ENTRY_LIST_REMOVE",           serverCommand_entryListRemove,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "ENTRY_LIST_INFO",             serverCommand_entryListInfo,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "STORAGE_DELETE",              serverCommand_storageDelete,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "RESTORE",                     serverCommand_restore,                  AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "RESTORE_CONTINUE",            serverCommand_restoreContinue,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_UUID_LIST",             serverCommand_indexUUIDList,            AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTITY_LIST",           serverCommand_indexEntityList,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_LIST",          serverCommand_indexStorageList,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRY_LIST",            serverCommand_indexEntryList,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_HISTORY_LIST",          serverCommand_indexHistoryList,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_ENTITY_ADD",            serverCommand_indexEntityAdd,           AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_STORAGE_ADD",           serverCommand_indexStorageAdd,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_ASSIGN",                serverCommand_indexAssign,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_REFRESH",               serverCommand_indexRefresh,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_REMOVE",                serverCommand_indexRemove,              AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  { "INDEX_STORAGES_INFO",         serverCommand_indexStoragesInfo,        AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "INDEX_ENTRIES_INFO",          serverCommand_indexEntriesInfo,         AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  // obsolete
  { "OPTION_GET",                  serverCommand_jobOptionGet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "OPTION_SET",                  serverCommand_jobOptionSet,             AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "OPTION_DELETE",               serverCommand_jobOptionDelete,          AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },

  #ifndef NDEBUG
  { "DEBUG_PRINT_STATISTICS",      serverCommand_debugPrintStatistics,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEBUG_PRINT_MEMORY_INFO",     serverCommand_debugPrintMemoryInfo,     AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  { "DEBUG_DUMP_MEMORY_INFO",      serverCommand_debugDumpMemoryInfo,      AUTHORIZATION_STATE_CLIENT|AUTHORIZATION_STATE_MASTER },
  #endif /* NDEBUG */
};

/*---------------------------------------------------------------------*/

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
  IndexHandle     *indexHandle;
  CommandInfoNode *commandInfoNode;
  String          result;
  Command         command;
  #ifndef NDEBUG
    uint64 t0,t1;
  #endif /* not NDEBUG */

  assert(clientInfo != NULL);

  // init variables
  result = String_new();

  // open index
  indexHandle = NULL;
  if (Index_isAvailable())
  {
    while (!clientInfo->quitFlag && (indexHandle == NULL))
    {
      indexHandle = Index_open(NULL,CLIENT_TIMEOUT);
    }
  }

  while (   !clientInfo->quitFlag
         && MsgQueue_get(&clientInfo->commandQueue,&command,NULL,sizeof(command),WAIT_FOREVER)
        )
  {
    // check authorization (if not in server debug mode)
    if ((globalOptions.serverDebugLevel >= 1) || IS_SET(command.authorizationStateSet,clientInfo->authorizationState))
    {
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
        commandInfoNode->indexHandle = indexHandle;
        List_append(&clientInfo->commandInfoList,commandInfoNode);
      }

      // execute command
      #ifndef NDEBUG
        t0 = 0LL;
        if (globalOptions.serverDebugLevel >= 1)
        {
          t0 = Misc_getTimestamp();
        }
      #endif /* not NDEBUG */
      command.serverCommandFunction(clientInfo,
                                    indexHandle,
                                    command.id,
                                    command.argumentMap
                                   );
      #ifndef NDEBUG
        if (globalOptions.serverDebugLevel >= 2)
        {
          t1 = Misc_getTimestamp();
          fprintf(stderr,"DEBUG: command time=%"PRIu64"ms\n",(t1-t0)/(uint64)US_PER_MS);
        }
      #endif /* not DEBUG */

      // remove command info
      SEMAPHORE_LOCKED_DO(&clientInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        List_removeAndFree(&clientInfo->commandInfoList,commandInfoNode,CALLBACK_(NULL,NULL));
      }
    }
    else
    {
      // authorization failure -> mark for disconnect
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
      ServerIO_sendResult(&clientInfo->io,command.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
    }

    // free resources
    freeCommand(&command,NULL);
  }

  // done index
  if (Index_isAvailable())
  {
    Index_close(indexHandle);
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
  if (!MsgQueue_init(&clientInfo->commandQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize client command message queue!");
  }
  for (i = 0; i < MAX_NETWORK_CLIENT_THREADS; i++)
  {
    if (!Thread_init(&clientInfo->threads[i],"BAR client",0,networkClientThreadCode,clientInfo))
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
    if (!Thread_join(&clientInfo->threads[i]))
    {
      HALT_INTERNAL_ERROR("Cannot stop command threads!");
    }
  }

  // free resources
  for (i = MAX_NETWORK_CLIENT_THREADS-1; i >= 0; i--)
  {
    Thread_done(&clientInfo->threads[i]);
  }
  MsgQueue_done(&clientInfo->commandQueue,CALLBACK_((MsgQueueMsgFreeFunction)freeCommand,NULL));

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

  List_init(&clientInfo->commandInfoList);
  if (!RingBuffer_init(&clientInfo->abortedCommandIds,sizeof(uint),MAX_ABORT_COMMAND_IDS))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  clientInfo->abortedCommandIdStart = 0;

  EntryList_init(&clientInfo->includeEntryList);
  PatternList_init(&clientInfo->excludePatternList);
  Job_initOptions(&clientInfo->jobOptions);
  List_init(&clientInfo->directoryInfoList);
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

  clientInfo->quitFlag = TRUE;

  // abort all running master jobs
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    JOB_LIST_ITERATE(jobNode)
    {
      if (jobNode->masterIO == &clientInfo->io)
      {
        Job_abort(jobNode);
        jobNode->masterIO = NULL;
      }
    }
  }

  // abort all running commands
  LIST_ITERATE(&clientInfo->commandInfoList,commandInfoNode)
  {
    Index_interrupt(commandInfoNode->indexHandle);
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
  List_done(&clientInfo->directoryInfoList,CALLBACK_((ListNodeFreeFunction)freeDirectoryInfoNode,NULL));
  Job_doneOptions(&clientInfo->jobOptions);
  PatternList_done(&clientInfo->excludePatternList);
  EntryList_done(&clientInfo->includeEntryList);

  RingBuffer_done(&clientInfo->abortedCommandIds,CALLBACK_(NULL,NULL));
  List_done(&clientInfo->commandInfoList,CALLBACK_(NULL,NULL));
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
      if ((globalOptions.serverDebugLevel >= 1) || IS_SET(authorizationStateSet,clientInfo->authorizationState))
      {
        // execute
        serverCommandFunction(clientInfo,
                              indexHandle,
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
          // check authorization (if not in server debug mode)
          if ((globalOptions.serverDebugLevel >= 1) || IS_SET(authorizationStateSet,AUTHORIZATION_STATE_WAITING))
          {
            // execute command
            serverCommandFunction(clientInfo,
                                  indexHandle,
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

/*---------------------------------------------------------------------*/

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

Errors Server_run(ServerModes       mode,
                  uint              port,
                  uint              tlsPort,
                  const Certificate *ca,
                  const Certificate *cert,
                  const Key         *key,
                  const Hash        *passwordHash,
                  uint              maxConnections,
                  const char        *indexDatabaseFileName
                 )
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
  serverMode                     = mode;
  serverPort                     = port;
  #ifdef HAVE_GNU_TLS
    serverCA                     = ca;
    serverCert                   = cert;
    serverKey                    = key;
  #endif /* HAVE_GNU_TLS */
  serverPasswordHash             = passwordHash;
  Semaphore_init(&clientList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&clientList);
  List_init(&authorizationFailList);
  jobList.activeCount            = 0;
  Semaphore_init(&serverStateLock,SEMAPHORE_TYPE_BINARY);
  serverState                    = SERVER_STATE_RUNNING;
  pauseFlags.create              = FALSE;
  pauseFlags.restore             = FALSE;
  pauseFlags.indexUpdate         = FALSE;
  pauseEndDateTime               = 0LL;
  Semaphore_init(&newMaster.lock,SEMAPHORE_TYPE_BINARY);
  newMaster.pairingMode          = PAIRING_MODE_NONE;
  Misc_initTimeout(&newMaster.pairingTimeoutInfo,0LL);
  newMaster.name                 = String_new();
  Crypt_initHash(&newMaster.uuidHash,PASSWORD_HASH_ALGORITHM);
  indexHandle                    = NULL;
  quitFlag                       = FALSE;
  AUTOFREE_ADD(&autoFreeList,hostName,{ String_delete(hostName); });
  AUTOFREE_ADD(&autoFreeList,&clientList,{ List_done(&clientList,CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&clientList.lock,{ Semaphore_done(&clientList.lock); });
  AUTOFREE_ADD(&autoFreeList,&authorizationFailList,{ List_done(&authorizationFailList,CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&serverStateLock,{ Semaphore_done(&serverStateLock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.lock,{ Semaphore_done(&newMaster.lock); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.pairingTimeoutInfo,{ Misc_doneTimeout(&newMaster.pairingTimeoutInfo); });
  AUTOFREE_ADD(&autoFreeList,newMaster.name,{ String_delete(newMaster.name); });
  AUTOFREE_ADD(&autoFreeList,&newMaster.uuidHash,{ Crypt_doneHash(&newMaster.uuidHash); });

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Started BAR %u.%u server%s on '%s' with %u threads",
             VERSION_MAJOR,
             VERSION_MINOR,
             (serverMode == SERVER_MODE_SLAVE) ? " slave" : "",
             String_cString(hostName),
             (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores()
            );
  printInfo(1,"Started BAR %u.%u server%s on '%s' with %u threads\n",
            VERSION_MAJOR,
            VERSION_MINOR,
            (serverMode == SERVER_MODE_SLAVE) ? " slave" : "",
            String_cString(hostName),
            (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores()
           );

  // create jobs directory if necessary
  if (mode == SERVER_MODE_MASTER)
  {
    if (!File_exists(globalOptions.jobsDirectory))
    {
      error = File_makeDirectory(globalOptions.jobsDirectory,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSION
                                );
      if (error != ERROR_NONE)
      {
        printError("Cannot create directory '%s' (error: %s)",
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
      return ERROR_NOT_A_DIRECTORY;
    }
  }

  // init index database
  if (!stringIsEmpty(indexDatabaseFileName))
  {
    error = Index_init(indexDatabaseFileName);
    if (error != ERROR_NONE)
    {
      printError("Cannot init index database '%s' (error: %s)!",
                 indexDatabaseFileName,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,indexDatabaseFileName,{ Index_done(); });
  }

  // open index
  if (Index_isAvailable())
  {
    indexHandle = Index_open(NULL,INDEX_TIMEOUT);
    if (indexHandle != NULL)
    {
      if (globalOptions.serverDebugIndexOperationsFlag)
      {
        // wait for index opertions
        while (!Index_isInitialized())
        {
          Misc_udelay(1*US_PER_SECOND);
        }

        // terminate
        Index_close(indexHandle);
        AutoFree_cleanup(&autoFreeList);
        return ERROR_NONE;
      }
    }
    AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });
  }
  else
  {
    indexHandle = NULL;
  }

  // init server sockets
  serverFlag    = FALSE;
  serverTLSFlag = FALSE;
  if (port != 0)
  {
    error = Network_initServer(&serverSocketHandle,
                               port,
                               SERVER_SOCKET_TYPE_PLAIN,
                               NULL,
                               0,
                               NULL,
                               0,
                               NULL,
                               0
                              );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize server at port %u (error: %s)!",
                 port,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printInfo(1,"Opened port %d\n",port);
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
                                   SERVER_SOCKET_TYPE_TLS,
                                   serverCA->data,
                                   serverCA->length,
                                   serverCert->data,
                                   serverCert->length,
                                   serverKey->data,
                                   serverKey->length
                                  );
        if (error != ERROR_NONE)
        {
          printError("Cannot initialize TLS/SSL server at port %u (error: %s)!",
                     tlsPort,
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return FALSE;
        }
        AUTOFREE_ADD(&autoFreeList,&serverTLSSocketHandle,{ Network_doneServer(&serverTLSSocketHandle); });
        printInfo(1,"Opened TLS/SSL port %u\n",tlsPort);
        serverTLSFlag = TRUE;
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(ca);
        UNUSED_VARIABLE(cert);
        UNUSED_VARIABLE(key);

        printWarning("TLS/SSL is not supported!");
      #endif /* HAVE_GNU_TLS */
    }
    else
    {
      if (!isCertificateAvailable(ca)) printWarning("No certificate authority data (bar-ca.pem file) - TLS server not started");
      if (!isCertificateAvailable(cert)) printWarning("No certificate data (bar-server-cert.pem file) - TLS server not started");
      if (!isKeyAvailable(key)) printWarning("No key data (bar-server-key.pem file) - TLS server not started");
    }
  }
  if (!serverFlag && !serverTLSFlag)
  {
    if ((port == 0) && (tlsPort == 0))
    {
      printError("Cannot start any server (error: no port numbers specified)!");
    }
    else
    {
      printError("Cannot start any server!");
    }
    AutoFree_cleanup(&autoFreeList);
    return ERROR_INVALID_ARGUMENT;
  }
  if (serverPasswordHash->data == NULL)
  {
    printWarning("No server password set!");
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
//TODO: remove      jobNode->lastExecutedDateTime         = jobAggregateInfo.lastExecutedDateTime;
      String_set(jobNode->lastErrorMessage,jobAggregateInfo.lastErrorMessage);
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

      LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
      {
        getAggregateInfo(&scheduleAggregateInfo,
                         jobNode->job.uuid,
                         scheduleNode->uuid
                        );
        scheduleNode->lastExecutedDateTime = scheduleAggregateInfo.lastExecutedDateTime;
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
  if (!Thread_init(&jobThread,"BAR job",globalOptions.niceLevel,jobThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize job thread!");
  }
  if (!Thread_init(&pauseThread,"BAR pause",globalOptions.niceLevel,pauseThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize pause thread!");
  }
  if (serverMode == SERVER_MODE_MASTER)
  {
    if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize scheduler thread!");
    }
    if (!Thread_init(&pairingThread,"BAR pairing",globalOptions.niceLevel,pairingThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize pause thread!");
    }

    if (Index_isAvailable())
    {
  //TODO: required?
      // init database pause callbacks
      Index_setPauseCallback(CALLBACK_(indexPauseCallback,NULL));

      Semaphore_init(&indexThreadTrigger,SEMAPHORE_TYPE_BINARY);
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
      if (!Thread_init(&purgeExpiredEntitiesThread,"BAR purge expired",globalOptions.niceLevel,purgeExpiredEntitiesThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize purge expire thread!");
      }
    }
  }

  // run as server
  if (globalOptions.serverDebugLevel >= 1)
  {
    printWarning("Server is running in debug mode. No authorization is done, auto-pairing and additional debug commands are enabled!");
  }

  // auto-start pairing if unpaired slave or debug mode
  if (serverMode == SERVER_MODE_SLAVE)
  {
    if (   String_isEmpty(globalOptions.masterInfo.name)
        || (globalOptions.serverDebugLevel >= 1)
       )
    {
      startPairingMaster(DEFAULT_PAIRING_MASTER_TIMEOUT,PAIRING_MODE_AUTO);
    }

    if (!String_isEmpty(globalOptions.masterInfo.name))
    {
//TODO: log, remove?
      printInfo(1,"Master: %s\n",String_cString(globalOptions.masterInfo.name));
    }
  }

  // Note: ignore SIGALRM in Misc_waitHandles()
  MISC_SIGNAL_MASK_CLEAR(signalMask);
  #ifdef HAVE_SIGALRM
    MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
  #endif /* HAVE_SIGALRM */

  // process client requests
  Misc_initWait(&waitHandle,64);
  name                     = String_new();
  argumentMap              = StringMap_new();
  while (!quitFlag)
  {
    // get active sockets to wait for
    Misc_waitReset(&waitHandle);
    waitTimeout = 0LL;
    SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      // get standard port connection requests
      if (serverFlag    && ((maxConnections == 0) || (List_count(&clientList) < maxConnections)))
      {
        Misc_waitAdd(&waitHandle,Network_getServerSocket(&serverSocketHandle),HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID);
//ServerIO_addWait(&clientNode->clientInfo.io,Network_getServerSocket(&serverSocketHandle));
      }

      // get TLS port connection requests
      if (serverTLSFlag && ((maxConnections == 0) || (List_count(&clientList) < maxConnections)))
      {
        Misc_waitAdd(&waitHandle,Network_getServerSocket(&serverTLSSocketHandle),HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID);
//TODO:
//ServerIO_addWait(&clientNode->clientInfo.io,Network_getServerSocket(&serverTLSSocketHandle));
      }

      // get client connections, calculate min. wait timeout
      nowTimestamp = Misc_getTimestamp();
      waitTimeout  = 60LL*MS_PER_MINUTE; // wait for network connection max. 60min [ms]
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
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); asm("int3");

    MISC_HANDLES_ITERATE(&waitHandle,handle,events)
    {
      // connect new clients via plain/standard port
      if      (   serverFlag
               && (handle == Network_getServerSocket(&serverSocketHandle))
               && (events == HANDLE_EVENT_INPUT)
               && ((maxConnections == 0) || (List_count(&clientList) < maxConnections))
              )
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
            if (serverMode == SERVER_MODE_MASTER)
            {
              if (clientWaitRestTime > 0)
              {
                printInfo(1,
                          "Connected client %s (delayed %us)\n",
                          getClientInfo(&clientNode->clientInfo,s,sizeof(s)),
                          clientWaitRestTime
                         );
              }
              else
              {
                printInfo(1,
                          "Connected client %s\n",
                          getClientInfo(&clientNode->clientInfo,s,sizeof(s))
                         );
              }
            }
          }
        }
        else
        {
          printError("Cannot establish client connection (error: %s)!",
                     Error_getText(error)
                    );
        }
      }

      // connect new clients via TLS port
      else if (   serverTLSFlag
               && (handle == Network_getServerSocket(&serverTLSSocketHandle))
               && (events == HANDLE_EVENT_INPUT)
               && ((maxConnections == 0) || (List_count(&clientList) < maxConnections))
              )
      {
        error = newNetworkClient(&clientNode,&serverTLSSocketHandle);
        if (error == ERROR_NONE)
        {
          // start SSL
          #ifdef HAVE_GNU_TLS
            error = Network_startSSL(&clientNode->clientInfo.io.network.socketHandle,
                                     serverCA->data,
                                     serverCA->length,
                                     serverCert->data,
                                     serverCert->length,
                                     serverKey->data,
                                     serverKey->length
                                    );
            if (error != ERROR_NONE)
            {
              printError("Cannot initialize TLS/SSL session for client '%s:%d' (error: %s)!",
                         String_cString(clientNode->clientInfo.io.network.name),
                         clientNode->clientInfo.io.network.port,
                         Error_getText(error)
                        );
              deleteClient(clientNode);
              AutoFree_cleanup(&autoFreeList);
              return FALSE;
            }
          #else /* HAVE_GNU_TLS */
            printError("TLS/SSL server is not supported for client '%s:%d' (error: %s)!",
                       String_cString(clientNode->clientInfo.io.network.name),
                       clientNode->clientInfo.io.network.port,
                       Error_getText(error)
                      );
            deleteClient(&clientNode);
            AutoFree_cleanup(&autoFreeList);
            return FALSE;
          #endif /* HAVE_GNU_TLS */

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
                        "Connected client %s (TLS/SSL, delayed %us)\n",
                        getClientInfo(&clientNode->clientInfo,s,sizeof(s)),
                        clientWaitRestTime
                       );
            }
            else
            {
              printInfo(1,
                        "Connected client %s (TLS/SSL)\n",
                        getClientInfo(&clientNode->clientInfo,s,sizeof(s))
                       );
            }
          }
        }
        else
        {
          printError("Cannot establish client TLS connection (error: %s)!",
                     Error_getText(error)
                    );
        }
      }
      else
      {
        // process client commands/disconnects/results
        SEMAPHORE_LOCKED_DO(&clientList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          if (events != 0)
          {
            // find client node
            clientNode = LIST_FIND(&clientList,
                                   clientNode,
                                   handle == Network_getSocket(&clientNode->clientInfo.io.network.socketHandle)
                                  );
            if (clientNode != NULL)
            {
              if ((events & HANDLE_EVENT_INPUT) != 0)
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

                  if (serverMode == SERVER_MODE_MASTER)
                  {
                    printInfo(1,"Disconnected client %s\n",getClientInfo(&disconnectClientNode->clientInfo,s,sizeof(s)));
                  }

                  // done client and free resources
                  deleteClient(disconnectClientNode);
                }
              }
              else if ((events & (HANDLE_EVENT_ERROR|HANDLE_EVENT_INVALID)) != 0)
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

                if (serverMode == SERVER_MODE_MASTER)
                {
                  printInfo(1,"Disconnected client %s\n",getClientInfo(&disconnectClientNode->clientInfo,s,sizeof(s)));
                }

                // done client and free resources
                deleteClient(disconnectClientNode);
              }
              #ifndef NDEBUG
                else
                {
                  HALT_INTERNAL_ERROR("unknown poll events 0x%x",events);
                }
              #endif /* NDEBUG */
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

              // increment authorization failure
              incrementAuthorizationFail(disconnectClientNode);

              if (serverMode == SERVER_MODE_MASTER)
              {
                printInfo(1,"Disconnected client %s\n",getClientInfo(&disconnectClientNode->clientInfo,s,sizeof(s)));
              }

              // done client and free resources
              deleteClient(disconnectClientNode);
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
              authorizationFailNode = List_removeAndFree(&authorizationFailList,
                                                         authorizationFailNode,
                                                         CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
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
            List_removeAndFree(&authorizationFailList,
                               oldestAuthorizationFailNode,
                               CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
                              );
          }
        }
      }
    }

    // auto pairing master
    if (serverMode == SERVER_MODE_SLAVE)
    {
      if (newMaster.pairingMode == PAIRING_MODE_AUTO)
      {
        // set paired master
        if (!String_isEmpty(newMaster.name))
        {
          error = stopPairingMaster(newMaster.name,&newMaster.uuidHash);
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
  if (serverMode == SERVER_MODE_MASTER)
  {
    if (Index_isAvailable())
    {
      if (!Thread_join(&purgeExpiredEntitiesThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop purge expired entities thread!");
      }
      Thread_done(&purgeExpiredEntitiesThread);

      if (globalOptions.indexDatabaseAutoUpdateFlag)
      {
        if (!Thread_join(&autoIndexThread))
        {
          HALT_INTERNAL_ERROR("Cannot stop auto index thread!");
        }
        Thread_done(&autoIndexThread);
      }
      if (!Thread_join(&indexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop index thread!");
      }
      Thread_done(&indexThread);

      Semaphore_done(&indexThreadTrigger);

      // done database pause callbacks
      Index_setPauseCallback(CALLBACK_(NULL,NULL));
    }

    if (!Thread_join(&pairingThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop pairing thread!");
    }
    Thread_done(&pairingThread);
    if (!Thread_join(&schedulerThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop scheduler thread!");
    }
    Thread_done(&schedulerThread);
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

  // done server
  if (serverFlag   ) Network_doneServer(&serverSocketHandle);
  if (serverTLSFlag) Network_doneServer(&serverTLSSocketHandle);

  // done index
  if (Index_isAvailable())
  {
    Index_close(indexHandle);
  }

  // free resources
  Crypt_doneHash(&newMaster.uuidHash);
  String_delete(newMaster.name);
  Misc_doneTimeout(&newMaster.pairingTimeoutInfo);
  Semaphore_done(&newMaster.lock);
  Semaphore_done(&serverStateLock);
  if (!stringIsEmpty(indexDatabaseFileName)) Index_done();
  List_done(&authorizationFailList,CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL));
  List_done(&clientList,CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL));
  Semaphore_done(&clientList.lock);
  String_delete(hostName);
  AutoFree_done(&autoFreeList);

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Terminate BAR server%s",
             (serverMode == SERVER_MODE_SLAVE) ? " slave" : ""
            );
  printInfo(1,"Terminate BAR server%s\n",(serverMode == SERVER_MODE_SLAVE) ? " slave" : "");

  return ERROR_NONE;
}

Errors Server_batch(int        inputDescriptor,
                    int        outputDescriptor,
                    const char *indexDatabaseFileName
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
  List_init(&clientList);
  List_init(&authorizationFailList);
  jobList.activeCount    = 0;
  Semaphore_init(&serverStateLock,SEMAPHORE_TYPE_BINARY);
  serverState            = SERVER_STATE_RUNNING;
  pauseFlags.create      = FALSE;
  pauseFlags.restore     = FALSE;
  pauseFlags.indexUpdate = FALSE;
  pauseEndDateTime       = 0LL;
  indexHandle            = NULL;
  quitFlag               = FALSE;
  AUTOFREE_ADD(&autoFreeList,&clientList,{ List_done(&clientList,CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&clientList.lock,{ Semaphore_done(&clientList.lock); });
  AUTOFREE_ADD(&autoFreeList,&authorizationFailList,{ List_done(&authorizationFailList,CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&serverStateLock,{ Semaphore_done(&serverStateLock); });

  // init index database
  if (!stringIsEmpty(indexDatabaseFileName))
  {
    error = Index_init(indexDatabaseFileName);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot init index database '%s' (error: %s)!",
                 indexDatabaseFileName,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printInfo(1,"OK\n");
    AUTOFREE_ADD(&autoFreeList,indexDatabaseFileName,{ Index_done(); });
  }

  // open index
  if (Index_isAvailable())
  {
    indexHandle = Index_open(NULL,INDEX_TIMEOUT);
    AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });
  }
  else
  {
    indexHandle = NULL;
  }

  // start threads
  if (Index_isAvailable())
  {
//TODO: required?
    // init database pause callbacks
    Index_setPauseCallback(CALLBACK_(indexPauseCallback,NULL));

    Semaphore_init(&indexThreadTrigger,SEMAPHORE_TYPE_BINARY);
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
    if (!Thread_init(&purgeExpiredEntitiesThread,"BAR purge expired",globalOptions.niceLevel,purgeExpiredEntitiesThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize purge expire thread!");
    }
  }

  // run in batch mode
  if (globalOptions.serverDebugLevel >= 1)
  {
    printWarning("Server is running in debug mode. No authorization is done and additional debug commands are enabled!");
  }

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
  name        = String_new();
  argumentMap = StringMap_new();
#if 1
#if 0
  while (   1//!quitFlag
         && !File_eof(&clientInfo.io.file.inputHandle)
         && 1
        )
  {
    if (ServerIO_getCommand(&clientInfo.io,
                            &id,
                            name,
                            argumentMap
                           )
       )
    {
      processCommand(&clientInfo,id,name,argumentMap);
    }
  }
#else
  while (!quitFlag)
  {
    if      (ServerIO_getCommand(&clientInfo.io,
                                 &id,
                                 name,
                                 argumentMap
                                )
            )
    {
      processCommand(&clientInfo,id,name,argumentMap);
    }
    else if (!File_eof(&clientInfo.io.file.inputHandle))
    {
      ServerIO_receiveData(&clientInfo.io);
    }
    else
    {
      quitFlag = TRUE;
    }
  }
#endif
  StringMap_delete(argumentMap);
  String_delete(name);
#else /* 0 */
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
String_setCString(commandString,"1 SET crypt-password password='muster'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN type=REGEX pattern=test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST name=test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */

  // wait for thread exit
  if (Index_isAvailable())
  {
    if (!Thread_join(&purgeExpiredEntitiesThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop purge expired entities thread!");
    }
    Thread_done(&purgeExpiredEntitiesThread);

    if (globalOptions.indexDatabaseAutoUpdateFlag)
    {
      if (!Thread_join(&autoIndexThread))
      {
        HALT_INTERNAL_ERROR("Cannot stop auto index thread!");
      }
      Thread_done(&autoIndexThread);
    }
    if (!Thread_join(&indexThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop index thread!");
    }
    Thread_done(&indexThread);

    Semaphore_done(&indexThreadTrigger);

    // done database pause callbacks
    Index_setPauseCallback(CALLBACK_(NULL,NULL));
  }

  // done index
  if (Index_isAvailable())
  {
    Index_close(indexHandle);
  }

  // free resources
  doneClient(&clientInfo);
  if (!stringIsEmpty(indexDatabaseFileName)) Index_done();
  Semaphore_done(&serverStateLock);
  List_done(&authorizationFailList,CALLBACK_((ListNodeFreeFunction)freeAuthorizationFailNode,NULL));
  List_done(&clientList,CALLBACK_((ListNodeFreeFunction)freeClientNode,NULL));
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
