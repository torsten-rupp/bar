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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "arrays.h"
#include "configvalues.h"
#include "threads.h"
#include "semaphores.h"
#include "msgqueues.h"
#include "stringlists.h"

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
#include "misc.h"
#include "bar.h"

#include "commands_create.h"
#include "commands_restore.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define _SERVER_DEBUG
#define _NO_SESSION_ID
#define _SIMULATOR

/***************************** Constants *******************************/

#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 3

#define SESSION_ID_LENGTH 64                   // max. length of session id

#define MAX_NETWORK_CLIENT_THREADS 3           // number of threads for a client

// sleep times [s]
#define SLEEP_TIME_SCHEDULER_THREAD    ( 1*60)
#define SLEEP_TIME_PAUSE_THREAD        ( 1*60)
#define SLEEP_TIME_INDEX_THREAD        (15*60)
#define SLEEP_TIME_INDEX_UPDATE_THREAD (15*60)

/***************************** Datatypes *******************************/

// server states
typedef enum
{
  SERVER_STATE_RUNNING,
  SERVER_STATE_PAUSE,
  SERVER_STATE_SUSPENDED,
} ServerStates;

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
  JOB_STATE_REQUEST_VOLUME,
  JOB_STATE_DONE,
  JOB_STATE_ERROR,
  JOB_STATE_ABORTED
} JobStates;

// job node
typedef struct JobNode
{
  LIST_NODE_HEADER(struct JobNode);

  String       fileName;                       // file name
  uint64       timeModified;                   // file modified date/time (timestamp)

  // job config
  JobTypes     jobType;                        // job type: backup, restore
  String       name;                           // name of job
  String       archiveName;                    // archive name
  EntryList    includeEntryList;               // included entries
  PatternList  excludePatternList;             // excluded entry patterns
  PatternList  deltaSourcePatternList;         // delta source patterns
  PatternList  compressExcludePatternList;     // excluded compression patterns
  ScheduleList scheduleList;                   // schedule list
  JobOptions   jobOptions;                     // options for job
  bool         modifiedFlag;                   // TRUE iff job config modified

  // schedule info
  uint64       lastExecutedDateTime;           // last execution date/time (timestamp)
  uint64       lastCheckDateTime;              // last check date/time (timestamp)

  // job data
  Password     *ftpPassword;                   // FTP password if password mode is 'ask'
  Password     *sshPassword;                   // SSH password if password mode is 'ask'
  Password     *cryptPassword;                 // crypt password if password mode is 'ask'

  // job info
  uint         id;                             // unique job id
  JobStates    state;                          // current state of job
  ArchiveTypes archiveType;                    // archive type to create
  bool         requestedAbortFlag;             // request abort
  uint         requestedVolumeNumber;          // requested volume number
  uint         volumeNumber;                   // loaded volume number
  bool         volumeUnloadFlag;               // TRUE to unload volume

  // running info
  struct
  {
    Errors            error;                   // error code
    ulong             estimatedRestTime;       // estimated rest running time [s]
    ulong             doneEntries;             // number of processed entries
    uint64            doneBytes;               // sum of processed bytes
    ulong             totalEntries;            // number of total entries
    uint64            totalBytes;              // sum of total bytes
    ulong             skippedEntries;          // number of skipped entries
    uint64            skippedBytes;            // sum of skippped bytes
    ulong             errorEntries;            // number of entries with errors
    uint64            errorBytes;              // sum of bytes of files with errors
    PerformanceFilter entriesPerSecond;        // average processed entries per second
    PerformanceFilter bytesPerSecond;          // average processed bytes per second
    PerformanceFilter storageBytesPerSecond;   // average processed storage bytes per second
    uint64            archiveBytes;            // number of bytes stored in archive
    double            compressionRatio;        // compression ratio: saved "space" [%]
    String            name;                    // current entry
    uint64            entryDoneBytes;          // current entry bytes done
    uint64            entryTotalBytes;         // current entry bytes total
    String            storageName;             // current storage name
    uint64            archiveDoneBytes;        // current archive bytes done
    uint64            archiveTotalBytes;       // current archive bytes total
    uint              volumeNumber;            // current volume number
    double            volumeProgress;          // current volume progress
    String            message;                 // message text
  } runningInfo;
} JobNode;

// list with enqueued jobs
typedef struct
{
  LIST_HEADER(JobNode);

  Semaphore lock;
  uint      lastJobId;
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

// index database entry node
typedef struct IndexNode
{
  LIST_NODE_HEADER(struct IndexNode);

  ArchiveEntryTypes type;
  String            storageName;
  uint64            storageDateTime;
  String            name;
  uint64            timeModified;
  union
  {
    struct
    {
      uint64   size;
      uint32   userId;
      uint32   groupId;
      uint32   permission;
      uint64   fragmentOffset;
      uint64   fragmentSize;
    } file;
    struct
    {
      uint64 size;
      uint   blockSize;
      uint64 blockOffset;
      uint64 blockCount;
    } image;
    struct
    {
      uint32 userId;
      uint32 groupId;
      uint32 permission;
    } directory;
    struct
    {
      String destinationName;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
    } link;
    struct
    {
      uint64 size;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
      uint64 fragmentOffset;
      uint64 fragmentSize;
    } hardLink;
    struct
    {
      ulong  major;
      ulong  minor;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
    } special;
  };
} IndexNode;

// index database node list
typedef struct
{
  LIST_HEADER(IndexNode);
} IndexList;

// session id
typedef byte SessionId[SESSION_ID_LENGTH];

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

// client info
typedef struct
{
  ClientTypes         type;

  SessionId           sessionId;
  AuthorizationStates authorizationState;

  uint                abortCommandId;                      // command id to abort

  union
  {
    // i/o via file
    struct
    {
      FileHandle   fileHandle;
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
      bool         quitFlag;                               // TRUE if threads should terminate
    } network;
  };

  EntryList           includeEntryList;
  PatternList         excludePatternList;
  PatternList         deltaSourcePatternList;
  PatternList         compressExcludePatternList;
  JobOptions          jobOptions;
  DirectoryInfoList   directoryInfoList;
  Array               storageIdArray;
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

// server command function
typedef void(*ServerCommandFunction)(ClientInfo    *clientInfo,
                                     uint          id,
                                     const String  arguments[],
                                     uint          argumentCount
                                    );

// server command message
typedef struct
{
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
  uint                  id;
  Array                 arguments;
} CommandMsg;

LOCAL const ConfigValueUnit CONFIG_VALUE_BYTES_UNITS[] =
{
  {"G",1024*1024*1024},
  {"M",1024*1024},
  {"K",1024},
};

LOCAL const ConfigValueUnit CONFIG_VALUE_BITS_UNITS[] =
{
  {"K",1024},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_ARCHIVE_TYPES[] =
{
  {"normal",      ARCHIVE_TYPE_NORMAL,     },
  {"full",        ARCHIVE_TYPE_FULL,       },
  {"incremental", ARCHIVE_TYPE_INCREMENTAL },
  {"differential",ARCHIVE_TYPE_DIFFERENTIAL},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,         },
  {"regex",   PATTERN_TYPE_REGEX,        },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[] =
{
  {"none",CRYPT_ALGORITHM_NONE},

  #ifdef HAVE_GCRYPT
    {"3DES",      CRYPT_ALGORITHM_3DES      },
    {"CAST5",     CRYPT_ALGORITHM_CAST5     },
    {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH  },
    {"AES128",    CRYPT_ALGORITHM_AES128    },
    {"AES192",    CRYPT_ALGORITHM_AES192    },
    {"AES256",    CRYPT_ALGORITHM_AES256    },
    {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128},
    {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_TYPES[] =
{
  {"none",CRYPT_TYPE_NONE},

  #ifdef HAVE_GCRYPT
    {"symmetric", CRYPT_TYPE_SYMMETRIC },
    {"asymmetric",CRYPT_TYPE_ASYMMETRIC},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PASSWORD_MODES[] =
{
  {"default",PASSWORD_MODE_DEFAULT,},
  {"ask",    PASSWORD_MODE_ASK,    },
  {"config", PASSWORD_MODE_CONFIG, },
};

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_STRUCT_VALUE_STRING   ("archive-name",            JobNode,archiveName                             ),
  CONFIG_STRUCT_VALUE_SELECT   ("archive-type",            JobNode,jobOptions.archiveType,                 CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_STRUCT_VALUE_STRING   ("incremental-list-file",   JobNode,jobOptions.incrementalListFileName      ),

  CONFIG_STRUCT_VALUE_INTEGER64("archive-part-size",       JobNode,jobOptions.archivePartSize,             0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_STRUCT_VALUE_INTEGER  ("directory-strip",         JobNode,jobOptions.directoryStripCount,         0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("destination",             JobNode,jobOptions.destination                  ),
  CONFIG_STRUCT_VALUE_SPECIAL  ("owner",                   JobNode,jobOptions.owner,                       configValueParseOwner,configValueFormatInitOwner,NULL,configValueFormatOwner,NULL),

  CONFIG_STRUCT_VALUE_SELECT   ("pattern-type",            JobNode,jobOptions.patternType,                 CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_STRUCT_VALUE_SPECIAL  ("compress-algorithm",      JobNode,jobOptions.compressAlgorithm,           configValueParseCompressAlgorithm,configValueFormatInitCompressAlgorithm,configValueFormatDoneCompressAlgorithm,configValueFormatCompressAlgorithm,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL  ("compress-exclude",        JobNode,compressExcludePatternList,             configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),

  CONFIG_STRUCT_VALUE_SELECT   ("crypt-algorithm",         JobNode,jobOptions.cryptAlgorithm,              CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_STRUCT_VALUE_SELECT   ("crypt-type",              JobNode,jobOptions.cryptType,                   CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_STRUCT_VALUE_SELECT   ("crypt-password-mode",     JobNode,jobOptions.cryptPasswordMode,           CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_STRUCT_VALUE_SPECIAL  ("crypt-password",          JobNode,jobOptions.cryptPassword,               configValueParsePassword,configValueFormatInitPassord,NULL,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("crypt-public-key",        JobNode,jobOptions.cryptPublicKeyFileName       ),

  CONFIG_STRUCT_VALUE_STRING   ("ftp-login-name",          JobNode,jobOptions.ftpServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL  ("ftp-password",            JobNode,jobOptions.ftpServer.password,          configValueParsePassword,configValueFormatInitPassord,NULL,configValueFormatPassword,NULL),

  CONFIG_STRUCT_VALUE_INTEGER  ("ssh-port",                JobNode,jobOptions.sshServer.port,              0,65535,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-login-name",          JobNode,jobOptions.sshServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-password",            JobNode,jobOptions.sshServer.password,          configValueParsePassword,configValueFormatInitPassord,NULL,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-public-key",          JobNode,jobOptions.sshServer.publicKeyFileName  ),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-private-key",         JobNode,jobOptions.sshServer.privateKeyFileName ),

  CONFIG_STRUCT_VALUE_SPECIAL  ("include-file",            JobNode,includeEntryList,                       configValueParseFileEntry,configValueFormatInitEntry,configValueFormatDoneEntry,configValueFormatFileEntry,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL  ("include-image",           JobNode,includeEntryList,                       configValueParseImageEntry,configValueFormatInitEntry,configValueFormatDoneEntry,configValueFormatImageEntry,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL  ("exclude",                 JobNode,excludePatternList,                     configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL  ("delta-source",            JobNode,deltaSourcePatternList,                 configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),

  CONFIG_STRUCT_VALUE_INTEGER64("volume-size",             JobNode,jobOptions.volumeSize,                  0LL,MAX_LONG_LONG,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("ecc",                     JobNode,jobOptions.errorCorrectionCodesFlag     ),

  CONFIG_STRUCT_VALUE_BOOLEAN  ("skip-unreadable",         JobNode,jobOptions.skipUnreadableFlag           ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("raw-images",              JobNode,jobOptions.rawImagesFlag                ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("overwrite-archive-files", JobNode,jobOptions.overwriteArchiveFilesFlag    ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("overwrite-files",         JobNode,jobOptions.overwriteFilesFlag           ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("wait-first-volume",       JobNode,jobOptions.waitFirstVolumeFlag          ),

  CONFIG_STRUCT_VALUE_SPECIAL  ("schedule",                JobNode,scheduleList,                           configValueParseSchedule,configValueFormatInitSchedule,configValueFormatDoneSchedule,configValueFormatSchedule,NULL),
};

/***************************** Variables *******************************/

LOCAL const Password   *serverPassword;
LOCAL const char       *serverJobsDirectory;
LOCAL const JobOptions *serverDefaultJobOptions;
LOCAL JobList          jobList;
LOCAL Thread           jobThread;
LOCAL Thread           schedulerThread;
LOCAL Thread           pauseThread;
LOCAL Thread           indexThread;
LOCAL Thread           indexUpdateThread;
LOCAL ClientList       clientList;
LOCAL Semaphore        serverStateLock;
LOCAL ServerStates     serverState;
LOCAL bool             createFlag;            // TRUE iff create archive in progress
LOCAL bool             restoreFlag;           // TRUE iff restore archive in progress
LOCAL struct
      {
        bool create;
        bool storage;
        bool restore;
        bool indexUpdate;
      } pauseFlags;                           // TRUE iff pause
LOCAL uint64           pauseEndTimestamp;
LOCAL bool             indexFlag;             // TRUE iff index archive in progress
LOCAL bool             quitFlag;              // TRUE iff quit requested

/****************************** Macros *********************************/

#define CHECK_JOB_IS_ACTIVE(jobNode) (   (jobNode->state == JOB_STATE_WAITING) \
                                      || (jobNode->state == JOB_STATE_RUNNING) \
                                      || (jobNode->state == JOB_STATE_REQUEST_VOLUME) \
                                     )

#define CHECK_JOB_IS_RUNNING(jobNode) (   (jobNode->state == JOB_STATE_RUNNING) \
                                       || (jobNode->state == JOB_STATE_REQUEST_VOLUME) \
                                      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getArchiveTypeName
* Purpose: get archive type name
* Input  : archiveType - archive type
* Output : -
* Return : name or ""
* Notes  : -
\***********************************************************************/

LOCAL const char *getArchiveTypeName(ArchiveTypes archiveType)
{
  uint z;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CONFIG_VALUE_ARCHIVE_TYPES))
         && (archiveType != CONFIG_VALUE_ARCHIVE_TYPES[z].value)
        )
  {
    z++;
  }

  return (z < SIZE_OF_ARRAY(CONFIG_VALUE_ARCHIVE_TYPES)) ? CONFIG_VALUE_ARCHIVE_TYPES[z].name : "";
}

/***********************************************************************\
* Name   : getCryptPasswordModeName
* Purpose: get crypt password mode name
* Input  : passwordMode - password mode
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

LOCAL const char *getCryptPasswordModeName(PasswordModes passwordMode)
{
  uint z;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CONFIG_VALUE_PASSWORD_MODES))
         && (passwordMode != CONFIG_VALUE_PASSWORD_MODES[z].value)
        )
  {
    z++;
  }

  return (z < SIZE_OF_ARRAY(CONFIG_VALUE_PASSWORD_MODES)) ? CONFIG_VALUE_PASSWORD_MODES[z].name : "";
}

/***********************************************************************\
* Name   : copyScheduleNode
* Purpose: copy allocated schedule node
* Input  : scheduleNode - schedule node
* Output : -
* Return : copied schedule node
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *copyScheduleNode(ScheduleNode *scheduleNode,
                                     void         *userData
                                    )
{
  ScheduleNode *newScheduleNode;

  assert(scheduleNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate pattern node
  newScheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (newScheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  newScheduleNode->year        = scheduleNode->year;
  newScheduleNode->month       = scheduleNode->month;
  newScheduleNode->day         = scheduleNode->day;
  newScheduleNode->hour        = scheduleNode->hour;
  newScheduleNode->minute      = scheduleNode->minute;
  newScheduleNode->weekDays    = scheduleNode->weekDays;
  newScheduleNode->archiveType = scheduleNode->archiveType;
  newScheduleNode->enabled     = scheduleNode->enabled;

  return newScheduleNode;
}

/***********************************************************************\
* Name   : getNewJobId
* Purpose: get new job id
* Input  : -
* Output : -
* Return : job id
* Notes  : -
\***********************************************************************/

LOCAL uint getNewJobId(void)
{

  jobList.lastJobId++;

  return jobList.lastJobId;
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

  jobNode->runningInfo.error              = ERROR_NONE;
  jobNode->runningInfo.estimatedRestTime  = 0;
  jobNode->runningInfo.doneEntries        = 0L;
  jobNode->runningInfo.doneBytes          = 0LL;
  jobNode->runningInfo.totalEntries       = 0L;
  jobNode->runningInfo.totalBytes         = 0LL;
  jobNode->runningInfo.skippedEntries     = 0L;
  jobNode->runningInfo.skippedBytes       = 0LL;
  jobNode->runningInfo.errorEntries       = 0L;
  jobNode->runningInfo.errorBytes         = 0LL;
  jobNode->runningInfo.archiveBytes       = 0LL;
  jobNode->runningInfo.compressionRatio   = 0.0;
  jobNode->runningInfo.entryDoneBytes     = 0LL;
  jobNode->runningInfo.entryTotalBytes    = 0LL;
  jobNode->runningInfo.archiveDoneBytes   = 0LL;
  jobNode->runningInfo.archiveTotalBytes  = 0LL;
  jobNode->runningInfo.volumeNumber       = 0;
  jobNode->runningInfo.volumeProgress     = 0.0;

  String_clear(jobNode->runningInfo.name       );
  String_clear(jobNode->runningInfo.storageName);
  String_clear(jobNode->runningInfo.message    );

  Misc_performanceFilterClear(&jobNode->runningInfo.entriesPerSecond     );
  Misc_performanceFilterClear(&jobNode->runningInfo.bytesPerSecond       );
  Misc_performanceFilterClear(&jobNode->runningInfo.storageBytesPerSecond);
}

/***********************************************************************\
* Name   : newJob
* Purpose: create new job
* Input  : jobType  - job type
*          fileName - file name or NULL
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *newJob(JobTypes jobType, const String fileName)
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
  jobNode->timeModified                   = 0LL;

  jobNode->name                           = File_getFileBaseName(File_newFileName(),fileName);
  jobNode->archiveName                    = String_new();
  EntryList_init(&jobNode->includeEntryList);
  PatternList_init(&jobNode->excludePatternList);
  PatternList_init(&jobNode->deltaSourcePatternList);
  PatternList_init(&jobNode->compressExcludePatternList);
  List_init(&jobNode->scheduleList);
  initJobOptions(&jobNode->jobOptions);
  jobNode->modifiedFlag                   = FALSE;

  jobNode->lastExecutedDateTime           = 0LL;
  jobNode->lastCheckDateTime              = 0LL;

  jobNode->ftpPassword                    = NULL;
  jobNode->sshPassword                    = NULL;
  jobNode->cryptPassword                  = NULL;

  jobNode->id                             = getNewJobId();
  jobNode->state                          = JOB_STATE_NONE;
  jobNode->archiveType                    = ARCHIVE_TYPE_NORMAL;
  jobNode->requestedAbortFlag             = FALSE;
  jobNode->requestedVolumeNumber          = 0;
  jobNode->volumeNumber                   = 0;
  jobNode->volumeUnloadFlag               = FALSE;

  jobNode->runningInfo.name               = String_new();
  jobNode->runningInfo.storageName        = String_new();
  jobNode->runningInfo.message            = String_new();

  Misc_performanceFilterInit(&jobNode->runningInfo.entriesPerSecond,     10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.bytesPerSecond,       10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.storageBytesPerSecond,10*60);

  resetJobRunningInfo(jobNode);

  return jobNode;
}

/***********************************************************************\
* Name   : copyJob
* Purpose: copy job node
* Input  : jobNode  - job node
*          fileName - file name or NULL
*          name     - name of job
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *copyJob(JobNode      *jobNode,
                       const String fileName
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
  newJobNode->jobType                        = jobNode->jobType;
  newJobNode->fileName                       = String_duplicate(fileName);
  newJobNode->timeModified                   = 0LL;

  newJobNode->name                           = File_getFileBaseName(File_newFileName(),fileName);
  newJobNode->archiveName                    = String_duplicate(jobNode->archiveName);
  EntryList_init(&newJobNode->includeEntryList); EntryList_copy(&jobNode->includeEntryList,&newJobNode->includeEntryList);
  PatternList_init(&newJobNode->excludePatternList); PatternList_copy(&jobNode->excludePatternList,&newJobNode->excludePatternList);
  PatternList_init(&newJobNode->deltaSourcePatternList); PatternList_copy(&jobNode->deltaSourcePatternList,&newJobNode->deltaSourcePatternList);
  PatternList_init(&newJobNode->compressExcludePatternList); PatternList_copy(&jobNode->compressExcludePatternList,&newJobNode->compressExcludePatternList);
  List_init(&newJobNode->scheduleList); List_copy(&newJobNode->scheduleList,&jobNode->scheduleList,NULL,NULL,NULL,(ListNodeCopyFunction)copyScheduleNode,NULL);
  initJobOptions(&newJobNode->jobOptions); copyJobOptions(&newJobNode->jobOptions,&newJobNode->jobOptions);
  newJobNode->modifiedFlag                   = TRUE;

  newJobNode->lastExecutedDateTime           = 0LL;
  newJobNode->lastCheckDateTime              = 0LL;

  newJobNode->ftpPassword                    = NULL;
  newJobNode->sshPassword                    = NULL;
  newJobNode->cryptPassword                  = NULL;

  newJobNode->id                             = getNewJobId();
  newJobNode->state                          = JOB_STATE_NONE;
  newJobNode->archiveType                    = ARCHIVE_TYPE_NORMAL;
  newJobNode->requestedAbortFlag             = FALSE;
  newJobNode->requestedVolumeNumber          = 0;
  newJobNode->volumeNumber                   = 0;
  newJobNode->volumeUnloadFlag               = FALSE;

  newJobNode->runningInfo.name               = String_new();
  newJobNode->runningInfo.storageName        = String_new();
  newJobNode->runningInfo.message            = String_new();

  Misc_performanceFilterInit(&newJobNode->runningInfo.entriesPerSecond,     10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.bytesPerSecond,       10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.storageBytesPerSecond,10*60);

  resetJobRunningInfo(newJobNode);

  return newJobNode;
}

/***********************************************************************\
* Name   : freeJobNode
* Purpose: free job node
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeJobNode(JobNode *jobNode, void *userData)
{
  assert(jobNode != NULL);

  UNUSED_VARIABLE(userData);

  Misc_performanceFilterDone(&jobNode->runningInfo.storageBytesPerSecond);
  Misc_performanceFilterDone(&jobNode->runningInfo.bytesPerSecond);
  Misc_performanceFilterDone(&jobNode->runningInfo.entriesPerSecond);

  String_delete(jobNode->runningInfo.message);
  String_delete(jobNode->runningInfo.storageName);
  String_delete(jobNode->runningInfo.name);

  if (jobNode->cryptPassword != NULL) Password_delete(jobNode->cryptPassword);
  if (jobNode->sshPassword != NULL) Password_delete(jobNode->sshPassword);
  if (jobNode->ftpPassword != NULL) Password_delete(jobNode->ftpPassword);

  freeJobOptions(&jobNode->jobOptions);
  List_done(&jobNode->scheduleList,NULL,NULL);
  PatternList_done(&jobNode->compressExcludePatternList);
  PatternList_done(&jobNode->deltaSourcePatternList);
  PatternList_done(&jobNode->excludePatternList);
  EntryList_done(&jobNode->includeEntryList);
  String_delete(jobNode->archiveName);
  String_delete(jobNode->name);
  String_delete(jobNode->fileName);
}

/***********************************************************************\
* Name   : deleteJob
* Purpose: delete job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteJob(JobNode *jobNode)
{
  assert(jobNode != NULL);

  freeJobNode(jobNode,NULL);
  LIST_DELETE_NODE(jobNode);
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

  jobNode->state = JOB_STATE_RUNNING;
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
}

/***********************************************************************\
* Name   : findJobById
* Purpose: find job by id
* Input  : jobId - job id
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL JobNode *findJobById(uint jobId)
{
  JobNode *jobNode;

  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }

  return jobNode;
}

/***********************************************************************\
* Name   : findJobByName
* Purpose: find job by name
* Input  : naem - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL JobNode *findJobByName(const String name)
{
  JobNode *jobNode;

  jobNode = jobList.head;
  while ((jobNode != NULL) && !String_equals(jobNode->name,name))
  {
    jobNode = jobNode->next;
  }

  return jobNode;
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

  /* get filename*/
  fileName = File_newFileName();
  File_splitFileName(jobNode->fileName,&pathName,&baseName);
  File_setFileName(fileName,pathName);
  File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
  File_deleteFileName(baseName);
  File_deleteFileName(pathName);

  if (File_exists(fileName))
  {
    // open file .name
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      return error;
    }

    // read file
    line = String_new();
    while (!File_eof(&fileHandle))
    {
      // read line
      error = File_readLine(&fileHandle,line);
      if (error != ERROR_NONE) break;
      String_trim(line,STRING_WHITE_SPACES);

      // skip comments, empty lines
      if (String_isEmpty(line) || String_startsWithChar(line,'#'))
      {
        continue;
      }

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
      File_deleteFileName(fileName);
      return error;
    }
  }

  // free resources
  File_deleteFileName(fileName);

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

  // get filename
  fileName = File_newFileName();
  File_splitFileName(jobNode->fileName,&pathName,&baseName);
  File_setFileName(fileName,pathName);
  File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
  File_deleteFileName(baseName);
  File_deleteFileName(pathName);

  // create file .name
  error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }

  // write file
  error = File_printLine(&fileHandle,"%lld",jobNode->lastExecutedDateTime);

  // close file
  File_close(&fileHandle);

  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }

  // free resources
  File_deleteFileName(fileName);

  return ERROR_NONE;
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
  assert(jobNode != NULL);
  assert(jobNode->fileName != NULL);

  // reset values
  String_clear(jobNode->archiveName);
  EntryList_clear(&jobNode->includeEntryList);
  PatternList_clear(&jobNode->excludePatternList);
  PatternList_clear(&jobNode->deltaSourcePatternList);
  PatternList_clear(&jobNode->compressExcludePatternList);
  List_clear(&jobNode->scheduleList,NULL,NULL);
  jobNode->jobOptions.archiveType                  = ARCHIVE_TYPE_NORMAL;
  jobNode->jobOptions.archivePartSize              = 0LL;
  jobNode->jobOptions.incrementalListFileName      = NULL;
  jobNode->jobOptions.directoryStripCount          = 0;
  jobNode->jobOptions.destination                  = NULL;
  jobNode->jobOptions.patternType                  = PATTERN_TYPE_GLOB;
  jobNode->jobOptions.compressAlgorithm.delta      = COMPRESS_ALGORITHM_NONE;
  jobNode->jobOptions.compressAlgorithm.byte       = COMPRESS_ALGORITHM_NONE;
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
  String_clear(jobNode->jobOptions.sshServer.publicKeyFileName);
  String_clear(jobNode->jobOptions.sshServer.privateKeyFileName);
  jobNode->jobOptions.device.volumeSize            = 0LL;
  jobNode->jobOptions.waitFirstVolumeFlag          = FALSE;
  jobNode->jobOptions.errorCorrectionCodesFlag     = FALSE;
  jobNode->jobOptions.skipUnreadableFlag           = FALSE;
  jobNode->jobOptions.rawImagesFlag                = FALSE;
  jobNode->jobOptions.overwriteArchiveFilesFlag    = FALSE;
  jobNode->jobOptions.overwriteFilesFlag           = FALSE;

  // read file
  if (!readJobFile(jobNode->fileName,
                   CONFIG_VALUES,
                   SIZE_OF_ARRAY(CONFIG_VALUES),
                   jobNode
                  )
     )
  {
    return FALSE;
  }

  // save time modified
  jobNode->timeModified = File_getFileTimeModified(jobNode->fileName);

  // read schedule info
  readJobScheduleInfo(jobNode);

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
  JobNode             *jobNode,*deleteJobNode;

  assert(jobsDirectory != NULL);

  // init variables
  fileName = File_newFileName();

  // add new/update jobs
  File_setFileNameCString(fileName,jobsDirectory);
  error = File_openDirectoryList(&directoryListHandle,fileName);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }
  baseName = File_newFileName();
  while (!File_endOfDirectoryList(&directoryListHandle))
  {
    // read directory entry
    File_readDirectoryList(&directoryListHandle,fileName);

    // get base name
    File_getFileBaseName(baseName,fileName);

    // check if readable file and not ".*"
    if (File_isFile(fileName) && File_isReadable(fileName) && !String_startsWithChar(baseName,'.'))
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        // find/create job
        jobNode = jobList.head;
        while ((jobNode != NULL) && !String_equals(jobNode->name,baseName))
        {
          jobNode = jobNode->next;
        }
        if (jobNode == NULL)
        {
          jobNode = newJob(JOB_TYPE_CREATE,fileName);
          List_append(&jobList,jobNode);
        }

        if (   !CHECK_JOB_IS_ACTIVE(jobNode)
            && (jobNode->timeModified < File_getFileTimeModified(fileName))
           )
        {
          // read job
          readJob(jobNode);
        }
      }
    }
  }
  File_deleteFileName(baseName);
  File_closeDirectoryList(&directoryListHandle);

  // remove not existing jobs
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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
          // do not exists => delete job node
          deleteJobNode = jobNode;
          jobNode = jobNode->next;
          List_remove(&jobList,deleteJobNode);
          deleteJob(deleteJobNode);
        }
      }
      else
      {
        jobNode = jobNode->next;
      }
    }
  }

  // free resources
  File_deleteFileName(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteJobEntries
* Purpose: delete entries from job
* Input  : stringList - job file string list to modify
*          name       - name of value
* Output : -
* Return : next entry in string list or NULL
* Notes  : -
\***********************************************************************/

LOCAL StringNode *deleteJobEntries(StringList *stringList,
                                   const char *name
                                  )
{
  StringNode *nextNode;

  StringNode *stringNode;
  String     line;
  String     string;

  nextNode = NULL;
#warning check if works

  line = String_new();
  string = String_new();
  stringNode = stringList->head;
  while (stringNode != NULL)
  {
    // skip comments, empty lines
    String_trim(String_set(line,stringNode->string),STRING_WHITE_SPACES);
    if (String_isEmpty(line) || String_startsWithChar(line,'#'))
    {
      stringNode = stringNode->next;
      continue;
    }

    // parse and match
    if (   String_parse(line,STRING_BEGIN,"%S=% S",NULL,string,NULL)
        && String_equalsCString(string,name)
       )
    {
      // delete line
      stringNode = StringList_remove(stringList,stringNode);
    }
    else
    {
      // keep line
      stringNode = stringNode->next;
    }
  }
  String_delete(string);
  String_delete(line);

  return nextNode;
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
  StringList jobFileList;
  String     line;
  Errors     error;
  FileHandle fileHandle;
  uint       z;
  StringNode *nextNode;
  ConfigValueFormat configValueFormat;

  assert(jobNode != NULL);

  StringList_init(&jobFileList);
  line  = String_new();

  // read file
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    StringList_done(&jobFileList);
    String_delete(line);
    return error;
  }
  line  = String_new();
  while (!File_eof(&fileHandle))
  {
    // read line
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE) break;

    StringList_append(&jobFileList,line);
  }
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    StringList_done(&jobFileList);
    String_delete(line);
    return error;
  }

  // update in line list
  for (z = 0; z < SIZE_OF_ARRAY(CONFIG_VALUES); z++)
  {
    // delete old entries, get position for insert new entries
    nextNode = deleteJobEntries(&jobFileList,CONFIG_VALUES[z].name);

    // insert new entries
    ConfigValue_formatInit(&configValueFormat,
                           &CONFIG_VALUES[z],
                           CONFIG_VALUE_FORMAT_MODE_LINE,
                           jobNode
                          );
    while (ConfigValue_format(&configValueFormat,line))
    {
      StringList_insert(&jobFileList,line,nextNode);
    }
    ConfigValue_formatDone(&configValueFormat);
  }

  // write file
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&jobFileList);
    return error;
  }
  while (!StringList_isEmpty(&jobFileList))
  {
    StringList_getFirst(&jobFileList,line);
    error = File_writeLine(&fileHandle,line);
    if (error != ERROR_NONE) break;
  }
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&jobFileList);
    return error;
  }
  error = File_setPermission(jobNode->fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
  if (error != ERROR_NONE)
  {
    logMessage(LOG_TYPE_WARNING,
               "cannot set file permissions of job '%s' (error: %s)\n",
               String_cString(jobNode->fileName),
               Errors_getText(error)
              );
  }

  // save time modified
  jobNode->timeModified = File_getFileTimeModified(jobNode->fileName);

  // free resources
  String_delete(line);
  StringList_done(&jobFileList);

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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (jobNode->modifiedFlag)
      {
        updateJob(jobNode);
      }
    }
  }
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getCryptPassword
* Purpose: get crypt password
* Input  : userData      - job info
*          password      - crypt password variable
*          fileName      - file name
*          validateFlag  - TRUE to validate input, FALSE otherwise
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password)
* Output : password - crypt password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptPassword(void         *userData,
                              Password     *password,
                              const String fileName,
                              bool         validateFlag,
                              bool         weakCheckFlag
                             )
{
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(validateFlag);
  UNUSED_VARIABLE(weakCheckFlag);

  if (((JobNode*)userData)->cryptPassword != NULL)
  {
    Password_set(password,((JobNode*)userData)->cryptPassword);
    return ERROR_NONE;
  }
  else
  {
    return ERROR_NO_CRYPT_PASSWORD;
  }
}

/***********************************************************************\
* Name   : updateCreateJobStatus
* Purpose: update create status
* Input  : jobNode          - job node
*          error            - error code
*          createStatusInfo - create status info data
* Output : -
* Return : bool TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateCreateJobStatus(JobNode                *jobNode,
                                 Errors                 error,
                                 const CreateStatusInfo *createStatusInfo
                                )
{
  SemaphoreLock semaphoreLock;
  double        entriesPerSecond,bytesPerSecond,storageBytesPerSecond;
  ulong         restFiles;
  uint64        restBytes;
  uint64        restStorageBytes;
  ulong         estimatedRestTime;

  assert(jobNode != NULL);
  assert(createStatusInfo != NULL);
  assert(createStatusInfo->name != NULL);
  assert(createStatusInfo->storageName != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // calculate estimated rest time
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecond,     createStatusInfo->doneEntries);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecond,       createStatusInfo->doneBytes);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecond,createStatusInfo->archiveDoneBytes);
    entriesPerSecond      = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.entriesPerSecond     );
    bytesPerSecond        = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.bytesPerSecond       );
    storageBytesPerSecond = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.storageBytesPerSecond);

    restFiles        = (createStatusInfo->totalEntries      > createStatusInfo->doneEntries     )?createStatusInfo->totalEntries     -createStatusInfo->doneEntries     :0L;
    restBytes        = (createStatusInfo->totalBytes        > createStatusInfo->doneBytes       )?createStatusInfo->totalBytes       -createStatusInfo->doneBytes       :0LL;
    restStorageBytes = (createStatusInfo->archiveTotalBytes > createStatusInfo->archiveDoneBytes)?createStatusInfo->archiveTotalBytes-createStatusInfo->archiveDoneBytes:0LL;
    estimatedRestTime = 0;
    if (entriesPerSecond      > 0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restFiles/entriesPerSecond            )); }
    if (bytesPerSecond        > 0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restBytes/bytesPerSecond              )); }
    if (storageBytesPerSecond > 0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restStorageBytes/storageBytesPerSecond)); }

/*
fprintf(stderr,"%s,%d: createStatusInfo->doneEntries=%lu createStatusInfo->doneBytes=%llu jobNode->runningInfo.totalEntries=%lu jobNode->runningInfo.totalBytes %llu -- entriesPerSecond=%f bytesPerSecond=%f estimatedRestTime=%lus\n",__FILE__,__LINE__,
createStatusInfo->doneEntries,
createStatusInfo->doneBytes,
jobNode->runningInfo.totalEntries,
jobNode->runningInfo.totalBytes,
entriesPerSecond,bytesPerSecond,estimatedRestTime);
*/

    jobNode->runningInfo.error                 = error;
    jobNode->runningInfo.doneEntries           = createStatusInfo->doneEntries;
    jobNode->runningInfo.doneBytes             = createStatusInfo->doneBytes;
    jobNode->runningInfo.totalEntries          = createStatusInfo->totalEntries;
    jobNode->runningInfo.totalBytes            = createStatusInfo->totalBytes;
    jobNode->runningInfo.skippedEntries        = createStatusInfo->skippedEntries;
    jobNode->runningInfo.skippedBytes          = createStatusInfo->skippedBytes;
    jobNode->runningInfo.errorEntries          = createStatusInfo->errorEntries;
    jobNode->runningInfo.errorBytes            = createStatusInfo->errorBytes;
    jobNode->runningInfo.archiveBytes          = createStatusInfo->archiveBytes;
    jobNode->runningInfo.compressionRatio      = createStatusInfo->compressionRatio;
    jobNode->runningInfo.estimatedRestTime     = estimatedRestTime;
    String_set(jobNode->runningInfo.name,createStatusInfo->name);
    jobNode->runningInfo.entryDoneBytes        = createStatusInfo->entryDoneBytes;
    jobNode->runningInfo.entryTotalBytes       = createStatusInfo->entryTotalBytes;
    String_set(jobNode->runningInfo.storageName,createStatusInfo->storageName);
    jobNode->runningInfo.archiveDoneBytes      = createStatusInfo->archiveDoneBytes;
    jobNode->runningInfo.archiveTotalBytes     = createStatusInfo->archiveTotalBytes;
    jobNode->runningInfo.volumeNumber          = createStatusInfo->volumeNumber;
    jobNode->runningInfo.volumeProgress        = createStatusInfo->volumeProgress;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : updateRestoreJobStatus
* Purpose: update restore job status
* Input  : jobNode           - job node
*          error             - error code
*          restoreStatusInfo - create status info data
* Output : -
* Return : bool TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateRestoreJobStatus(JobNode                 *jobNode,
                                  Errors                  error,
                                  const RestoreStatusInfo *restoreStatusInfo
                                 )
{
  SemaphoreLock semaphoreLock;
  double        entriesPerSecond,bytesPerSecond,storageBytesPerSecond;

  assert(jobNode != NULL);
  assert(restoreStatusInfo != NULL);
  assert(restoreStatusInfo->name != NULL);
  assert(restoreStatusInfo->storageName != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // calculate estimated rest time
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecond,     restoreStatusInfo->doneEntries);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecond,       restoreStatusInfo->doneBytes);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecond,restoreStatusInfo->archiveDoneBytes);
    entriesPerSecond      = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.entriesPerSecond     );
    bytesPerSecond        = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.bytesPerSecond       );
    storageBytesPerSecond = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.storageBytesPerSecond);

/*
fprintf(stderr,"%s,%d: restoreStatusInfo->doneEntries=%lu restoreStatusInfo->doneBytes=%llu jobNode->runningInfo.totalEntries=%lu jobNode->runningInfo.totalBytes %llu -- entriesPerSecond=%f bytesPerSecond=%f estimatedRestTime=%lus\n",__FILE__,__LINE__,
restoreStatusInfo->doneEntries,
restoreStatusInfo->doneBytes,
jobNode->runningInfo.totalEntries,
jobNode->runningInfo.totalBytes,
entriesPerSecond,bytesPerSecond,estimatedRestTime);
*/

    jobNode->runningInfo.error                 = error;
    jobNode->runningInfo.doneEntries           = restoreStatusInfo->doneEntries;
    jobNode->runningInfo.doneBytes             = restoreStatusInfo->doneBytes;
    jobNode->runningInfo.skippedEntries        = restoreStatusInfo->skippedEntries;
    jobNode->runningInfo.skippedBytes          = restoreStatusInfo->skippedBytes;
    jobNode->runningInfo.errorEntries          = restoreStatusInfo->errorEntries;
    jobNode->runningInfo.errorBytes            = restoreStatusInfo->errorBytes;
    jobNode->runningInfo.archiveBytes          = 0LL;
    jobNode->runningInfo.compressionRatio      = 0.0;
    jobNode->runningInfo.estimatedRestTime     = 0;
    String_set(jobNode->runningInfo.name,restoreStatusInfo->name);
    jobNode->runningInfo.entryDoneBytes        = restoreStatusInfo->entryDoneBytes;
    jobNode->runningInfo.entryTotalBytes       = restoreStatusInfo->entryTotalBytes;
    String_set(jobNode->runningInfo.storageName,restoreStatusInfo->storageName);
    jobNode->runningInfo.archiveDoneBytes      = restoreStatusInfo->archiveDoneBytes;
    jobNode->runningInfo.archiveTotalBytes     = restoreStatusInfo->archiveTotalBytes;
    jobNode->runningInfo.volumeNumber          = 0; // ???
    jobNode->runningInfo.volumeProgress        = 0.0; // ???
  }

  return TRUE;
}

/***********************************************************************\
* Name   : storageRequestVolume
* Purpose: request volume call-back
* Input  : jobNode      - job node
*          volumeNumber - volume number
* Output : -
* Return : request result; see StorageRequestResults
* Notes  : -
\***********************************************************************/

LOCAL StorageRequestResults storageRequestVolume(JobNode *jobNode,
                                                 uint    volumeNumber
                                                )
{
  StorageRequestResults storageRequestResult;
  SemaphoreLock         semaphoreLock;

  assert(jobNode != NULL);

  storageRequestResult = STORAGE_REQUEST_VOLUME_NONE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // request volume
    jobNode->requestedVolumeNumber = volumeNumber;
    String_format(String_clear(jobNode->runningInfo.message),"Please insert DVD #%d",volumeNumber);

    // wait until volume is available or job is aborted
    assert(jobNode->state == JOB_STATE_RUNNING);
    jobNode->state = JOB_STATE_REQUEST_VOLUME;

    storageRequestResult = STORAGE_REQUEST_VOLUME_NONE;
    do
    {
      Semaphore_waitModified(&jobList.lock);

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
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_NONE);
    jobNode->state = JOB_STATE_RUNNING;
  }

  return storageRequestResult;
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
  JobNode      *jobNode;
  String       storageName;
  String       printableStorageName;
  EntryList    includeEntryList;
  PatternList  excludePatternList;
  PatternList  deltaSourcePatternList;
  PatternList  compressExcludePatternList;
  JobOptions   jobOptions;
  ArchiveTypes archiveType;
  StringList   archiveFileNameList;
  int          z;

  // initialize variables
  storageName          = String_new();
  printableStorageName = String_new();
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  PatternList_init(&deltaSourcePatternList);
  PatternList_init(&compressExcludePatternList);

  while (!quitFlag)
  {
    // lock
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

    // get next job
    do
    {
      jobNode = jobList.head;
      while ((jobNode != NULL) && (jobNode->state != JOB_STATE_WAITING))
      {
        jobNode = jobNode->next;
      }
      if (jobNode == NULL) Semaphore_waitModified(&jobList.lock);
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
    Storage_getPrintableName(printableStorageName,storageName);
    EntryList_clear(&includeEntryList); EntryList_copy(&jobNode->includeEntryList,&includeEntryList);
    PatternList_clear(&excludePatternList); PatternList_copy(&jobNode->excludePatternList,&excludePatternList);
    PatternList_clear(&deltaSourcePatternList); PatternList_copy(&jobNode->deltaSourcePatternList,&deltaSourcePatternList);
    PatternList_clear(&compressExcludePatternList); PatternList_copy(&jobNode->compressExcludePatternList,&compressExcludePatternList);
    initJobOptions(&jobOptions); copyJobOptions(&jobNode->jobOptions,&jobOptions);
    archiveType = jobNode->archiveType,

    // unlock is ok; job is now protected by running state
    Semaphore_unlock(&jobList.lock);

    // run job
#ifdef SIMULATOR
{
  int z;

  jobNode->runningInfo.estimatedRestTime=120;

  jobNode->runningInfo.totalEntries += 60;
  jobNode->runningInfo.totalBytes += 6000;

  for (z=0;z<120;z++)
  {
    extern void sleep(int);
    if (jobNode->requestedAbortFlag) break;

    fprintf(stderr,"%s,%d: z=%d\n",__FILE__,__LINE__,z);
    sleep(1);

    if (z==40) {
      jobNode->runningInfo.totalEntries += 80;
      jobNode->runningInfo.totalBytes += 8000;
    }

    jobNode->runningInfo.doneEntries++;
    jobNode->runningInfo.doneBytes += 100;
  //  jobNode->runningInfo.totalEntries += 3;
  //  jobNode->runningInfo.totalBytes += 181;
    jobNode->runningInfo.estimatedRestTime=120-z;
    String_clear(jobNode->runningInfo.fileName);String_format(jobNode->runningInfo.fileName,"file %d",z);
    String_clear(jobNode->runningInfo.storageName);String_format(jobNode->runningInfo.storageName,"storage %d%d",z,z);
  }
}
#else
    // run job
    logMessage(LOG_TYPE_ALWAYS,"------------------------------------------------------------\n");
    switch (jobNode->jobType)
    {
      case JOB_TYPE_CREATE:
        logMessage(LOG_TYPE_ALWAYS,"start create archive '%s'\n",String_cString(printableStorageName));

        // try to pause background index thread, do short delay to make sure network connection is possible
        createFlag = TRUE;
        if (indexFlag)
        {
          z = 0;
          while ((z < 5*60) && indexFlag)
          {
            Misc_udelay(10LL*1000LL*1000LL);
            z += 10;
          }
          Misc_udelay(30LL*1000LL*1000LL);
        }

        // create archive
        jobNode->runningInfo.error = Command_create(String_cString(storageName),
                                                    &includeEntryList,
                                                    &excludePatternList,
                                                    &compressExcludePatternList,
                                                    &jobOptions,
                                                    archiveType,
                                                    getCryptPassword,
                                                    jobNode,
                                                    (CreateStatusInfoFunction)updateCreateJobStatus,
                                                    jobNode,
                                                    (StorageRequestVolumeFunction)storageRequestVolume,
                                                    jobNode,
                                                    &pauseFlags.create,
                                                    &pauseFlags.storage,
                                                    &jobNode->requestedAbortFlag
                                                   );
        createFlag = FALSE;

        logMessage(LOG_TYPE_ALWAYS,"done create archive '%s' (error: %s)\n",String_cString(printableStorageName),Errors_getText(jobNode->runningInfo.error));
        break;
      case JOB_TYPE_RESTORE:
        logMessage(LOG_TYPE_ALWAYS,"start restore archive '%s'\n",String_cString(printableStorageName));

        // try to pause background index thread, do short delay to make sure network connection is possible
        restoreFlag = TRUE;
        if (indexFlag)
        {
          z = 0;
          while ((z < 5*60) && indexFlag)
          {
            Misc_udelay(10LL*1000LL*1000LL);
            z += 10;
          }
          Misc_udelay(30LL*1000LL*1000LL);
        }

        // restore archive
        StringList_init(&archiveFileNameList);
        StringList_append(&archiveFileNameList,storageName);
        jobNode->runningInfo.error = Command_restore(&archiveFileNameList,
                                                     &includeEntryList,
                                                     &excludePatternList,
                                                     &jobOptions,
                                                     getCryptPassword,
                                                     jobNode,
                                                     (RestoreStatusInfoFunction)updateRestoreJobStatus,
                                                     jobNode,
                                                     &pauseFlags.restore,
                                                     &jobNode->requestedAbortFlag
                                                    );
        StringList_done(&archiveFileNameList);
        restoreFlag = FALSE;

        logMessage(LOG_TYPE_ALWAYS,"done restore archive '%s' (error: %s)\n",String_cString(printableStorageName),Errors_getText(jobNode->runningInfo.error));
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
    logPostProcess();

#endif /* SIMULATOR */

    // free resources
    freeJobOptions(&jobOptions);
    PatternList_clear(&compressExcludePatternList);
    PatternList_clear(&deltaSourcePatternList);
    PatternList_clear(&excludePatternList);
    EntryList_clear(&includeEntryList);

    // lock
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // done job
    doneJob(jobNode);

    if (!jobNode->jobOptions.dryRunFlag)
    {
      // store schedule info
      writeJobScheduleInfo(jobNode);
    }

    // unlock
    Semaphore_unlock(&jobList.lock);
  }

  // free resources
  PatternList_done(&compressExcludePatternList);
  PatternList_done(&deltaSourcePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  String_delete(printableStorageName);
  String_delete(storageName);
}

/*---------------------------------------------------------------------*/

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
  JobNode      *jobNode;
  uint64       currentDateTime;
  uint64       dateTime;
  uint         year,month,day,hour,minute;
  WeekDays     weekDay;
  ScheduleNode *executeScheduleNode;
  ScheduleNode *scheduleNode;
  bool         pendingFlag;
  int          z;

  while (!quitFlag)
  {
    // update job files
    updateAllJobs();

    // re-read config files
    rereadAllJobs(serverJobsDirectory);

    // trigger jobs
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
    {
      currentDateTime = Misc_getCurrentDateTime();
      jobNode         = jobList.head;
      pendingFlag     = FALSE;
      while ((jobNode != NULL) && !pendingFlag && !quitFlag)
      {
        // check if job have to be executed
        executeScheduleNode = NULL;
        if (!CHECK_JOB_IS_ACTIVE(jobNode))
        {
          dateTime = currentDateTime;
          while (   ((dateTime/60LL) > (jobNode->lastCheckDateTime/60LL))
                 && (executeScheduleNode == NULL)
                 && !pendingFlag
                 && !quitFlag
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

            // check if matching with schedule list node
            scheduleNode = jobNode->scheduleList.head;
            while ((scheduleNode != NULL) && (executeScheduleNode == NULL))
            {
              if (   ((scheduleNode->year     == SCHEDULE_ANY    ) || (scheduleNode->year   == (int)year  ) )
                  && ((scheduleNode->month    == SCHEDULE_ANY    ) || (scheduleNode->month  == (int)month ) )
                  && ((scheduleNode->day      == SCHEDULE_ANY    ) || (scheduleNode->day    == (int)day   ) )
                  && ((scheduleNode->hour     == SCHEDULE_ANY    ) || (scheduleNode->hour   == (int)hour  ) )
                  && ((scheduleNode->minute   == SCHEDULE_ANY    ) || (scheduleNode->minute == (int)minute) )
                  && ((scheduleNode->weekDays == SCHEDULE_ANY_DAY) || IN_SET(scheduleNode->weekDays,weekDay))
                  && scheduleNode->enabled
                 )
              {
                executeScheduleNode = scheduleNode;
              }
              scheduleNode = scheduleNode->next;
            }

            // check if other thread pending for job list
            pendingFlag = Semaphore_checkPending(&jobList.lock);

            // next time
            dateTime -= 60LL;
          }
          if (quitFlag)
          {
            break;
          }
          if (!pendingFlag)
          {
            jobNode->lastCheckDateTime = currentDateTime;
          }
        }

        // trigger job
        if (executeScheduleNode != NULL)
        {
          // set state
          jobNode->state              = JOB_STATE_WAITING;
          jobNode->archiveType        = executeScheduleNode->archiveType;
          jobNode->requestedAbortFlag = FALSE;
          resetJobRunningInfo(jobNode);
        }

        // check next job
        jobNode = jobNode->next;
      }
    }
    Semaphore_unlock(&jobList.lock);

    // sleep, check quit flag
    z = 0;
    while ((z < SLEEP_TIME_SCHEDULER_THREAD) && !quitFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
      z += 10;
    }
  }
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
  uint64        nowTimestamp;
  int           z;

  while (!quitFlag)
  {
    // decrement pause time, continue
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      if (serverState == SERVER_STATE_PAUSE)
      {
        nowTimestamp = Misc_getCurrentDateTime();
        if (nowTimestamp > pauseEndTimestamp)
        {
          serverState = SERVER_STATE_RUNNING;
          pauseFlags.create      = FALSE;
          pauseFlags.storage     = FALSE;
          pauseFlags.restore     = FALSE;
          pauseFlags.indexUpdate = FALSE;
        }
      }
    }

    // sleep, check update and quit flag
    z = 0;
    while ((z < SLEEP_TIME_PAUSE_THREAD) && !quitFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
      z += 10;
    }
  }
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
* Return : TRUE on quit/create/restore, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool indexAbortCallback(void *userData)
{
  UNUSED_VARIABLE(userData);

  return quitFlag || createFlag || restoreFlag;
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
  String                 storageName;
  String                 printableStorageName;
  IndexCryptPasswordList indexCryptPasswordList;
  int64                  storageId;
  bool                   interruptFlag;
  Errors                 error;
  JobNode                *jobNode;
  IndexCryptPasswordNode *indexCryptPasswordNode;
  int                    z;

  // initialize variables
  storageName          = String_new();
  printableStorageName = String_new();
  List_init(&indexCryptPasswordList);

  // reset/delete incomplete database entries (ignore possible errors)
  plogMessage(LOG_TYPE_INDEX,"INDEX","start clean-up database\n");
  error = ERROR_NONE;
  while (Index_findByState(indexDatabaseHandle,
                           INDEX_STATE_UPDATE,
                           &storageId,
                           NULL,
                           NULL
                          )
         && (error == ERROR_NONE)
        )
  {
    error = Index_setState(indexDatabaseHandle,
                           storageId,
                           INDEX_STATE_UPDATE_REQUESTED,
                           0LL,
                           NULL
                          );
  }
  if (globalOptions.indexDatabaseNoAutoUpdateFlag)
  {
    while (Index_findByState(indexDatabaseHandle,
                             INDEX_STATE_UPDATE_REQUESTED,
                             &storageId,
                             NULL,
                             NULL
                            )
           && (error == ERROR_NONE)
          )
    {
      error = Index_setState(indexDatabaseHandle,
                             storageId,
                             INDEX_STATE_ERROR,
                             0LL,
                             NULL
                            );
    }
  }
  while (Index_findByState(indexDatabaseHandle,
                           INDEX_STATE_CREATE,
                           &storageId,
                           storageName,
                           NULL
                          )
         && (error == ERROR_NONE)
        )
  {
    plogMessage(LOG_TYPE_INDEX,"INDEX","delete incomplete index #%lld\n",storageId);
    error = Index_delete(indexDatabaseHandle,
                         storageId
                        );
  }
  plogMessage(LOG_TYPE_ALWAYS,"INDEX","done clean-up database\n");

  // add/update index database
  while (!quitFlag)
  {
    // get all job crypt passwords and crypt public keys (including no password and default)
    addIndexCryptPasswordNode(&indexCryptPasswordList,NULL,NULL);
    addIndexCryptPasswordNode(&indexCryptPasswordList,globalOptions.cryptPassword,NULL);
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
    {
      LIST_ITERATE(&jobList,jobNode)
      {
        if (jobNode->jobOptions.cryptPassword != NULL)
        {
          addIndexCryptPasswordNode(&indexCryptPasswordList,jobNode->jobOptions.cryptPassword,jobNode->jobOptions.cryptPrivateKeyFileName);
        }
      }
    }
    Semaphore_unlock(&jobList.lock);

    // update index entries
    interruptFlag = FALSE;
    while (   Index_findByState(indexDatabaseHandle,
                                INDEX_STATE_UPDATE_REQUESTED,
                                &storageId,
                                storageName,
                                NULL
                               )
           && !interruptFlag
           && !quitFlag
          )
    {
      Storage_getPrintableName(printableStorageName,storageName);

      plogMessage(LOG_TYPE_INDEX,
                  "INDEX",
                  "create index #%lld for '%s'\n",
                  storageId,
                  String_cString(printableStorageName)
                 );

      // try to create index
      LIST_ITERATE(&indexCryptPasswordList,indexCryptPasswordNode)
      {
        // set state 'index update in progress'
        indexFlag = TRUE;

        // index update
        error = Archive_updateIndex(indexDatabaseHandle,
                                    storageId,
                                    storageName,
                                    indexCryptPasswordNode->cryptPassword,
                                    indexCryptPasswordNode->cryptPrivateKeyFileName,
                                    indexPauseCallback,
                                    NULL,
                                    indexAbortCallback,
                                    NULL
                                   );
        if (error == ERROR_NONE)
        {
          indexFlag = FALSE;
          break;
        }

        // clear state 'index update in progress'
        indexFlag = FALSE;

        // check if interrupted
        if (createFlag || restoreFlag) interruptFlag = TRUE;
      }
      if (!interruptFlag && !quitFlag)
      {
        if (error == ERROR_NONE)
        {
          plogMessage(LOG_TYPE_INDEX,
                      "INDEX",
                      "created storage index '%s'\n",
                      String_cString(printableStorageName)
                     );
        }
        else
        {
          plogMessage(LOG_TYPE_ERROR,
                      "INDEX",
                      "cannot create storage index '%s' (error: %s)\n",
                      String_cString(printableStorageName),
                      Errors_getText(error)
                     );
        }
      }
    }

    // free resources
    List_done(&indexCryptPasswordList,(ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL);

    // wait until create/restore is done
    while ((createFlag || restoreFlag) && !quitFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
    }

    if (!interruptFlag)
    {
      // sleep for 10min, check quit flag
      z = 0;
      while ((z < SLEEP_TIME_INDEX_THREAD) && !quitFlag)
      {
        Misc_udelay(10LL*1000LL*1000LL);
        z += 10;
      }
    }
  }

  // free resources
  List_done(&indexCryptPasswordList,(ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL);
  String_delete(printableStorageName);
  String_delete(storageName);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      File_getFilePathName(storagePathName,jobNode->archiveName);
      if (StringList_find(storageDirectoryList,storagePathName) == NULL)
      {
        StringList_append(storageDirectoryList,storagePathName);
      }
    }
  }
  String_delete(storagePathName);
}

/***********************************************************************\
* Name   : indexUpdateThreadCode
* Purpose: index update thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexUpdateThreadCode(void)
{
  String                     storageSpecifier;
  String                     fileName;
  String                     storageName;
  StringList                 storageDirectoryList;
  JobOptions                 jobOptions;
  StringNode                 *storageDirectoryNode;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  FileInfo                   fileInfo;
  StorageTypes               storageType;
  int64                      storageId;
  uint64                     createdDateTime;
  IndexStates                indexState;
  uint64                     lastCheckedDateTime;
  int                        z;
  uint64                     now;
  String                     printableStorageName;
  String                     dateTime;
  DatabaseQueryHandle        databaseQueryHandle;
  IndexModes                 indexMode;

  // initialize variables
  StringList_init(&storageDirectoryList);

  // run continous check for index updates
  storageSpecifier = File_newFileName();
  fileName         = File_newFileName();
  while (!quitFlag)
  {
    // collect storage locations to check for BAR files
    getStorageDirectories(&storageDirectoryList);

    // check storage locations for BAR files, send index update request
    initJobOptions(&jobOptions);
    copyJobOptions(serverDefaultJobOptions,&jobOptions);
    STRINGLIST_ITERATE(&storageDirectoryList,storageDirectoryNode,storageName)
    {
      storageType = Storage_parseName(storageName,storageSpecifier,NULL);
      if (   (storageType == STORAGE_TYPE_FILESYSTEM)
          || (storageType == STORAGE_TYPE_FTP       )
          || (storageType == STORAGE_TYPE_SSH       )
          || (storageType == STORAGE_TYPE_SCP       )
          || (storageType == STORAGE_TYPE_SFTP      )
         )
      {
        // list directory, update index checked/request create index
//fprintf(stderr,"%s, %d: check %s\n",__FILE__,__LINE__,String_cString(storageName));
        error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                          storageName,
                                          &jobOptions
                                         );
        if (error == ERROR_NONE)
        {
          while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && !quitFlag)
          {
            // read next directory entry
            error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,&fileInfo);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // check entry type and file name
            if (    (fileInfo.type != FILE_TYPE_FILE)
                 || !String_endsWithCString(fileName,".bar")
                )
            {
              continue;
            }

            // check index update state of file, add index
            Storage_getName(storageName,storageType,storageSpecifier,fileName);
//fprintf(stderr,"%s, %d: find %s -- %s\n",__FILE__,__LINE__,String_cString(storageSpecifier),String_cString(fileName));

            // get index id, request index update
            if (Index_findByName(indexDatabaseHandle,
                                 storageName,
                                 &storageId,
                                 &indexState,
                                 NULL
                                )
               )
            {
              if (indexState == INDEX_STATE_OK)
              {
                // set checked date/time
                error = Index_setState(indexDatabaseHandle,
                                       storageId,
                                       INDEX_STATE_OK,
                                       Misc_getCurrentDateTime(),
                                       NULL
                                      );
                if (error != ERROR_NONE)
                {
                  continue;
                }
              }
            }
            else
            {
              // add index
              error = Index_create(indexDatabaseHandle,
                                   storageName,
                                   INDEX_STATE_UPDATE_REQUESTED,
                                   INDEX_MODE_AUTO,
                                   &storageId
                                  );
              if (error != ERROR_NONE)
              {
                continue;
              }
            }
          }

          // close directory
          Storage_closeDirectoryList(&storageDirectoryListHandle);
        }
      }
    }
    freeJobOptions(&jobOptions);

    // delete not existing indizes
    error = Index_initListStorage(&databaseQueryHandle,
                                  indexDatabaseHandle,
                                  INDEX_STATE_ALL,
                                  NULL
                                 );
    if (error == ERROR_NONE)
    {
      now                  = Misc_getCurrentDateTime();
      storageName          = String_new();
      printableStorageName = String_new();
      dateTime             = String_new();
      while (Index_getNextStorage(&databaseQueryHandle,
                                  &storageId,
                                  storageName,
                                  &createdDateTime,
                                  NULL,
                                  &indexState,
                                  &indexMode,
                                  &lastCheckedDateTime,
                                  NULL
                                 )
            )
      {
        Storage_getPrintableName(printableStorageName,storageName);

        if (   (indexMode == INDEX_MODE_AUTO)
            && (indexState != INDEX_STATE_UPDATE_REQUESTED)
            && (now > (createdDateTime+globalOptions.indexDatabaseKeepTime))
            && (now > (lastCheckedDateTime+globalOptions.indexDatabaseKeepTime))
           )
        {
          Index_delete(indexDatabaseHandle,storageId);

          Misc_formatDateTime(dateTime,lastCheckedDateTime,NULL);
          plogMessage(LOG_TYPE_INDEX,
                      "INDEX",
                      "Deleted index for '%s', last checked %s\n",
                      String_cString(printableStorageName),
                      String_cString(dateTime)
                     );
        }
      }
      Index_doneList(&databaseQueryHandle);
      String_delete(dateTime);
      String_delete(printableStorageName);
      String_delete(storageName);
    }

    // sleep, check quit flag
    z = 0;
    while ((z < SLEEP_TIME_INDEX_UPDATE_THREAD) && !quitFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
      z += 10;
    }
  }
  File_deleteFileName(fileName);
  File_deleteFileName(storageSpecifier);

  // free resources
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

LOCAL void sendClient(ClientInfo *clientInfo, String data)
{
  Errors        error;
  SemaphoreLock semaphoreLock;

  assert(clientInfo != NULL);
  assert(data != NULL);

  #ifdef SERVER_DEBUG
    fprintf(stderr,"%s,%d: result=%s",__FILE__,__LINE__,String_cString(data));
  #endif /* SERVER_DEBUG */

//fprintf(stderr,"%s,%d: sent data: '%s'",__FILE__,__LINE__,String_cString(data));
  switch (clientInfo->type)
  {
    case CLIENT_TYPE_BATCH:
      error = File_write(&clientInfo->file.fileHandle,String_cString(data),String_length(data));
      (void)File_flush(&clientInfo->file.fileHandle);
      break;
    case CLIENT_TYPE_NETWORK:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&clientInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        error = Network_send(&clientInfo->network.socketHandle,String_cString(data),String_length(data));
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
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
  String  result;
  va_list arguments;

  result = String_new();

  String_format(result,"%d %d %d ",id,completeFlag?1:0,Errors_getCode(errorCode));
  va_start(arguments,format);
  String_vformat(result,format,arguments);
  va_end(arguments);
//fprintf(stderr,"%s,%d: result=%s\n",__FILE__,__LINE__,String_cString(result));
  String_appendChar(result,'\n');

  sendClient(clientInfo,result);

  String_delete(result);
}

/***********************************************************************\
* Name   : commandAborted
* Purpose: check if command was aborted
* Input  : clientInfo - client info
* Output : -
* Return : TRUE if command execution aborted or client disconnected,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool commandAborted(ClientInfo *clientInfo, uint commandId)
{
  bool abortedFlag;

  assert(clientInfo != NULL);

  abortedFlag = (clientInfo->abortCommandId == commandId);
  switch (clientInfo->type)
  {
    case CLIENT_TYPE_BATCH:
      break;
    case CLIENT_TYPE_NETWORK:
      if (clientInfo->network.quitFlag) abortedFlag = TRUE;
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
* Name   : getJobStateText
* Purpose: get text for job state
* Input  : jobState - job state
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

LOCAL const char *getJobStateText(const JobOptions *jobOptions, JobStates jobState)
{
  const char *stateText;

  assert(jobOptions != NULL);

  stateText = "unknown";
  switch (jobState)
  {
    case JOB_STATE_NONE:
      stateText = "-";
      break;
    case JOB_STATE_WAITING:
      stateText = "waiting";
      break;
    case JOB_STATE_RUNNING:
      stateText = (jobOptions->dryRunFlag) ? "dry-run" : "running";
      break;
    case JOB_STATE_REQUEST_VOLUME:
      stateText = "request volume";
      break;
    case JOB_STATE_DONE:
      stateText = "done";
      break;
    case JOB_STATE_ERROR:
      stateText = "ERROR";
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

LOCAL DirectoryInfoNode *newDirectoryInfo(const String pathName)
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
* Name   : findJobById
* Purpose: find job by id
* Input  : jobId - job id
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DirectoryInfoNode *findDirectoryInfo(DirectoryInfoList *directoryInfoList, const String pathName)
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
*          timeoutFlag   - TRUE iff timeout, FALSE otherwise
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getDirectoryInfo(DirectoryInfoNode *directoryInfoNode,
                            long              timeout,
                            uint64            *fileCount,
                            uint64            *totalFileSize,
                            bool              *timeoutFlag
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
  if (timeoutFlag != NULL) (*timeoutFlag) = FALSE;
  while (   (   !StringList_isEmpty(&directoryInfoNode->pathNameList)
             || directoryInfoNode->directoryOpenFlag
            )
         && ((timeoutFlag == NULL) || !(*timeoutFlag))
         && !quitFlag
        )
  {
    if (!directoryInfoNode->directoryOpenFlag)
    {
      // process FIFO for deep-first search; this keep the directory list shorter
      StringList_getLast(&directoryInfoNode->pathNameList,pathName);

      // open diretory for reading
      error = File_openDirectoryList(&directoryInfoNode->directoryListHandle,pathName);
      if (error != ERROR_NONE)
      {
        continue;
      }
      directoryInfoNode->directoryOpenFlag = TRUE;
    }

    // read directory content
    while (   !File_endOfDirectoryList(&directoryInfoNode->directoryListHandle)
           && ((timeoutFlag == NULL) || !(*timeoutFlag))
           && !quitFlag
          )
    {
      // read next directory entry
      error = File_readDirectoryList(&directoryInfoNode->directoryListHandle,fileName);
      if (error != ERROR_NONE)
      {
        continue;
      }
      directoryInfoNode->fileCount++;
      error = File_getFileInfo(&fileInfo,fileName);
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
      if ((timeout >= 0) && (timeoutFlag != NULL))
      {
        (*timeoutFlag) = (Misc_getTimestamp() >= (startTimestamp+timeout*1000));
      }
    }

    if ((timeoutFlag == NULL) || !(*timeoutFlag))
    {
      // close diretory
      directoryInfoNode->directoryOpenFlag = FALSE;
      File_closeDirectoryList(&directoryInfoNode->directoryListHandle);
    }

    // check for timeout
    if ((timeout >= 0) && (timeoutFlag != NULL))
    {
      (*timeoutFlag) = (Misc_getTimestamp() >= (startTimestamp+timeout*1000));
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
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage name>
*            <destination directory>
*            <overwrite flag>
*            <files>...
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_errorInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Errors error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get error code
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected error code");
    return;
  }
  error = (Errors)(uint)String_toInteger(arguments[0],0,NULL,NULL,0);

  // format result
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                   "%s",
                   Errors_getText(error)
                  );
}

/***********************************************************************\
* Name   : serverCommand_authorize
* Purpose: user authorization: check password
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_authorize(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  bool okFlag;

  uint z;
  char s[3];
  char n0,n1;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get encoded password (to avoid plain text passwords in memory)
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password");
    return;
  }

  // check password
  okFlag = TRUE;
  if (serverPassword != NULL)
  {
    z = 0;
    while ((z < SESSION_ID_LENGTH) && okFlag)
    {
      n0 = (char)(strtoul(String_subCString(s,arguments[0],z*2,2),NULL,16) & 0xFF);
      n1 = clientInfo->sessionId[z];
      if (z < Password_length(serverPassword))
      {
        okFlag = (Password_getChar(serverPassword,z) == (n0^n1));
      }
      else
      {
        okFlag = (n0 == n1);
      }
      z++;
    }
  }

  if (okFlag)
  {
    clientInfo->authorizationState = AUTHORIZATION_STATE_OK;
  }
  else
  {
    clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_get
* Purpose: get setting from server
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <name>
*          Result:
*            <value>
\***********************************************************************/

LOCAL void serverCommand_get(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String name;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  name = arguments[0];

  if (String_equalsCString(name,"FILE_SEPARATOR"))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"%c",FILES_PATHNAME_SEPARATOR_CHAR);
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"%S",name);
  }
}

/***********************************************************************\
* Name   : serverCommand_abort
* Purpose: abort command execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <command id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_abort(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint commandId;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id");
    return;
  }
  commandId = String_toInteger(arguments[0],0,NULL,NULL,0);

  // abort command
  clientInfo->abortCommandId = commandId;

  // format result
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_status
* Purpose: get status
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <status>
\***********************************************************************/

LOCAL void serverCommand_status(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint64 nowTimestamp;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // format result
  Semaphore_lock(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ);
  {
    switch (serverState)
    {
      case SERVER_STATE_RUNNING:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"running");
        break;
      case SERVER_STATE_PAUSE:
        nowTimestamp = Misc_getCurrentDateTime();
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"pause %llu",(pauseEndTimestamp > nowTimestamp)?pauseEndTimestamp-nowTimestamp:0LL);
        break;
      case SERVER_STATE_SUSPENDED:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"suspended");
        break;
    }
  }
  Semaphore_unlock(&serverStateLock);
}

/***********************************************************************\
* Name   : serverCommand_pause
* Purpose: pause job execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <pause time [s]>
*            [CREATE,STORAGE,RESTORE,INDEX_UPDATE]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_pause(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint            pauseTime;
  String          modeMask;
  SemaphoreLock   semaphoreLock;
  StringTokenizer stringTokenizer;
  String          token;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get pause time
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pause time");
    return;
  }
  pauseTime = (uint)String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount >= 2)
  {
    modeMask = arguments[1];
  }
  else
  {
    modeMask = NULL;
  }

  // set pause time
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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
      }
      String_doneTokenizer(&stringTokenizer);
    }
    pauseEndTimestamp = Misc_getCurrentDateTime()+(uint64)pauseTime;
    logMessage(LOG_TYPE_ALWAYS,"pause server for %dmin\n",pauseTime/60);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_suspend
* Purpose: suspend job execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            [CREATE,STORAGE,RESTORE,INDEX_UPDATE]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_suspend(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String          modeMask;
  SemaphoreLock   semaphoreLock;
  StringTokenizer stringTokenizer;
  String          token;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // get mode
  if (argumentCount >= 1)
  {
    modeMask = arguments[0];
  }
  else
  {
    modeMask = NULL;
  }

  // set suspend
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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
    logMessage(LOG_TYPE_ALWAYS,"suspend server\n");
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_continue
* Purpose: continue job execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_continue(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  SemaphoreLock semaphoreLock;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // clear pause/suspend
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    serverState            = SERVER_STATE_RUNNING;
    pauseFlags.create      = FALSE;
    pauseFlags.storage     = FALSE;
    pauseFlags.restore     = FALSE;
    pauseFlags.indexUpdate = FALSE;
    logMessage(LOG_TYPE_ALWAYS,"continue server\n");
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_deviceList
* Purpose: get device list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <device size> <device name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_deviceList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Errors           error;
  DeviceListHandle deviceListHandle;
  String           deviceName;
  DeviceInfo       deviceInfo;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // open device list
  error = Device_openDeviceList(&deviceListHandle);
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"cannot open device list (error: %s)",Errors_getText(error));
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
      sendClientResult(clientInfo,id,TRUE,error,"cannot read device list (error: %s)",Errors_getText(error));
      Device_closeDeviceList(&deviceListHandle);
      String_delete(deviceName);
      return;
    }

    // get device info
    error = Device_getDeviceInfo(&deviceInfo,deviceName);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"cannot read device info (error: %s)",Errors_getText(error));
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
                       "%lld %d %'S",
                       deviceInfo.size,
0,//                     deviceInfo.mountedFlag?1:0,
                       deviceName
                      );
    }
  }
  String_delete(deviceName);

  // close device
  Device_closeDeviceList(&deviceListHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_fileList
* Purpose: file list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <directory>
*          Result:
*            FILE <size> <time modified> <name>
*            DIRECTORY <time modified> <name>
*            LINK <time modified> <name>
*            HARDLINK <time modified> <name>
*            DEVICE CHARACTER <time modified> <name>
*            DEVICE BLOCK <size> <time modified> <name>
*            FIFO <time modified> <name>
*            SOCKET <time modified> <name>
*            SPECIAL <time modified> <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_fileList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  FileInfo                   fileInfo;
  String                     string;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get path name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected path name");
    return;
  }

  // open directory
  error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                    arguments[0],
                                    &clientInfo->jobOptions
                                   );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"%s",Errors_getText(error));
    return;
  }

  // read directory entries
  fileName = String_new();
  string = String_new();
  while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
  {
    error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,&fileInfo);
    if (error == ERROR_NONE)
    {
      String_mapCString(String_set(string,fileName),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN));
      switch (fileInfo.type)
      {
        case FILE_TYPE_FILE:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "FILE %llu %llu %'S",
                           fileInfo.size,
                           fileInfo.timeModified,
                           string
                          );
          break;
        case FILE_TYPE_DIRECTORY:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "DIRECTORY %llu %'S",
                           fileInfo.timeModified,
                           string
                          );
          break;
        case FILE_TYPE_LINK:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "LINK %llu %'S",
                           fileInfo.timeModified,
                           string
                          );
          break;
        case FILE_TYPE_HARDLINK:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "HARDLINK %llu %'S",
                           fileInfo.timeModified,
                           string
                          );
          break;
        case FILE_TYPE_SPECIAL:
          switch (fileInfo.specialType)
          {
            case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "DEVICE CHARACTER %llu %'S",
                               fileInfo.timeModified,
                               string
                              );
              break;
            case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "DEVICE BLOCK %lld %llu %'S",
                               fileInfo.size,
                               fileInfo.timeModified,
                               string
                              );
              break;
            case FILE_SPECIAL_TYPE_FIFO:
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "FIFO %llu %'S",
                               fileInfo.timeModified,
                               string
                              );
              break;
            case FILE_SPECIAL_TYPE_SOCKET:
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "SOCKET %llu %'S",
                               fileInfo.timeModified,
                               string
                              );
              break;
            default:
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "SPECIAL %llu %'S",
                               fileInfo.timeModified,
                               string
                              );
              break;
          }
          break;
        default:
          break;
      }
    }
    else
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "UNKNOWN %'S",
                       string
                      );
    }
  }
  String_delete(string);
  String_delete(fileName);

  // close directory
  Storage_closeDirectoryList(&storageDirectoryListHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_directoryInfo
* Purpose: get directory info
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <directory>
*            <timeout [s]>
*          Result:
*            <file count> <total file size> <timeout flag>
\***********************************************************************/

LOCAL void serverCommand_directoryInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  long              timeout;
  DirectoryInfoNode *directoryInfoNode;
  uint64            fileCount;
  uint64            totalFileSize;
  bool              timeoutFlag;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get path name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected file name");
    return;
  }
  timeout = (argumentCount >= 2)?String_toInteger(arguments[1],0,NULL,NULL,0):0L;

  // find/create directory info
  directoryInfoNode = findDirectoryInfo(&clientInfo->directoryInfoList,arguments[0]);
  if (directoryInfoNode == NULL)
  {
    directoryInfoNode = newDirectoryInfo(arguments[0]);
    List_append(&clientInfo->directoryInfoList,directoryInfoNode);
  }

  // get total size of directoy/file
  getDirectoryInfo(directoryInfoNode,
                   timeout,
                   &fileCount,
                   &totalFileSize,
                   &timeoutFlag
                  );

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"%llu %llu %d",fileCount,totalFileSize,timeoutFlag?1:0);
}

/***********************************************************************\
* Name   : serverCommand_optionGet
* Purpose: get job options
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <option name>
*          Result:
*            <value>
\***********************************************************************/

LOCAL void serverCommand_optionGet(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint              jobId;
  String            name;
  JobNode           *jobNode;
  uint              z;
  String            s;
  ConfigValueFormat configValueFormat;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected config value name");
    return;
  }
  name = arguments[1];

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find config value
    z = 0;
    while (   (z < SIZE_OF_ARRAY(CONFIG_VALUES))
           && !String_equalsCString(name,CONFIG_VALUES[z].name)
          )
    {
      z++;
    }
    if (z >= SIZE_OF_ARRAY(CONFIG_VALUES))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown config value for '%S'",name);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send value
    s = String_new();
    ConfigValue_formatInit(&configValueFormat,
                           &CONFIG_VALUES[z],
                           CONFIG_VALUE_FORMAT_MODE_VALUE,
                           jobNode
                          );
    ConfigValue_format(&configValueFormat,s);
    ConfigValue_formatDone(&configValueFormat);
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"%S",s);
    String_delete(s);
  }
  Semaphore_unlock(&jobList.lock);
}

/***********************************************************************\
* Name   : serverCommand_optionSet
* Purpose: set job option
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <option name>
*            <value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_optionSet(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  String        name,value;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, name, value
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected config name");
    return;
  }
  name = arguments[1];
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected config value for '%S'",arguments[1]);
    return;
  }
  value = arguments[2];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // parse
    if (ConfigValue_parse(String_cString(name),
                          String_cString(value),
                          CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                          NULL,
                          NULL,
                          jobNode
                         )
       )
    {
      jobNode->modifiedFlag = TRUE;
      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown config value for '%S'",name);
    }
  }
}

/***********************************************************************\
* Name   : serverCommand_optionDelete
* Purpose: delete job option
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <option name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_optionDelete(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  uint          z;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected config value name");
    return;
  }
  name = arguments[1];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find config value
    z = 0;
    while (   (z < SIZE_OF_ARRAY(CONFIG_VALUES))
           && !String_equalsCString(name,CONFIG_VALUES[z].name)
          )
    {
      z++;
    }
    if (z >= SIZE_OF_ARRAY(CONFIG_VALUES))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown config value for '%S'",name);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // delete value
//    ConfigValue_reset(&CONFIG_VALUES[z],jobNode);
  }
}

/***********************************************************************\
* Name   : serverCommand_jobList
* Purpose: get job list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <id> <name> <state> <type> <part size> <delta compress algorithm> \
*            <byte compress alrogithm> <crypt algorithm> <crypt type> \
*            <password mode> <last executed time> <estimated rest time>
\***********************************************************************/

LOCAL void serverCommand_jobList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    jobNode = jobList.head;
    while (   (jobNode != NULL)
           && !commandAborted(clientInfo,id)
          )
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%u %'S %'s %s %llu '%s+%s' %'s %'s %'s %llu %lu",
                       jobNode->id,
                       jobNode->name,
                       getJobStateText(&jobNode->jobOptions,jobNode->state),
                       getArchiveTypeName((   (jobNode->archiveType == ARCHIVE_TYPE_FULL        )
                                           || (jobNode->archiveType == ARCHIVE_TYPE_INCREMENTAL )
                                           || (jobNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
                                          )
                                          ?jobNode->archiveType
                                          :jobNode->jobOptions.archiveType
                                         ),
                       jobNode->jobOptions.archivePartSize,
                       Compress_getAlgorithmName(jobNode->jobOptions.compressAlgorithm.delta),
                       Compress_getAlgorithmName(jobNode->jobOptions.compressAlgorithm.byte),
                       Crypt_getAlgorithmName(jobNode->jobOptions.cryptAlgorithm),
                       Crypt_getTypeName(jobNode->jobOptions.cryptType),
                       getCryptPasswordModeName(jobNode->jobOptions.cryptPasswordMode),
                       jobNode->lastExecutedDateTime,
                       jobNode->runningInfo.estimatedRestTime
                      );

      jobNode = jobNode->next;
    }
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobInfo
* Purpose: get job info
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <state> <message> <done entries> <done bytes> <total entries> \
*            <total bytes> <skipped entries> <skipped bytes> <error entries> \
*            <skipped bytes> <entries/s> <bytes/s> <storage bytes/s> \
*            <archive bytes> <compress ratio> <filename> <entry bytes> \
*            <entry total bytes> <storage name> <archive done> <archive total> \
*            <volumn number> <volume progress> <requested volume number>
\***********************************************************************/

LOCAL void serverCommand_jobInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  uint       jobId;
  JobNode    *jobNode;
  const char *message;
  String     string;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // format and send result
    if      (jobNode->runningInfo.error != ERROR_NONE)
    {
      message = Errors_getText(jobNode->runningInfo.error);
    }
    else if (CHECK_JOB_IS_RUNNING(jobNode) && !String_isEmpty(jobNode->runningInfo.message))
    {
      message = String_cString(jobNode->runningInfo.message);
    }
    else
    {
      message = "";
    }
    string = String_mapCString(String_duplicate(jobNode->runningInfo.name),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN));
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                     "%'s %'s %lu %llu %lu %llu %lu %llu %lu %llu %f %f %f %llu %f %'S %llu %llu %'S %llu %llu %d %f %d",
                     getJobStateText(&jobNode->jobOptions,jobNode->state),
                     message,
                     jobNode->runningInfo.doneEntries,
                     jobNode->runningInfo.doneBytes,
                     jobNode->runningInfo.totalEntries,
                     jobNode->runningInfo.totalBytes,
                     jobNode->runningInfo.skippedEntries,
                     jobNode->runningInfo.skippedBytes,
                     jobNode->runningInfo.errorEntries,
                     jobNode->runningInfo.errorBytes,
                     Misc_performanceFilterGetValue(&jobNode->runningInfo.entriesPerSecond,     10),
                     Misc_performanceFilterGetValue(&jobNode->runningInfo.bytesPerSecond,       10),
                     Misc_performanceFilterGetValue(&jobNode->runningInfo.storageBytesPerSecond,60),
                     jobNode->runningInfo.archiveBytes,
                     jobNode->runningInfo.compressionRatio,
                     string,
                     jobNode->runningInfo.entryDoneBytes,
                     jobNode->runningInfo.entryTotalBytes,
#warning todo map name
                     jobNode->runningInfo.storageName,
                     jobNode->runningInfo.archiveDoneBytes,
                     jobNode->runningInfo.archiveTotalBytes,
                     jobNode->runningInfo.volumeNumber,
                     jobNode->runningInfo.volumeProgress,
                     jobNode->requestedVolumeNumber
                    );
    String_delete(string);
  }
  Semaphore_unlock(&jobList.lock);
}

/***********************************************************************\
* Name   : serverCommand_jobNew
* Purpose: create new job
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job name>
*          Result:
*            <job id>
\***********************************************************************/

LOCAL void serverCommand_jobNew(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String        name;
  SemaphoreLock semaphoreLock;
  String        fileName;
  FileHandle    fileHandle;
  Errors        error;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get filename
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name");
    return;
  }
  name = arguments[0];

  jobNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // check if job already exists
    if (findJobByName(name) != NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(name));
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // create empty job file
    fileName = File_appendFileName(File_setFileNameCString(File_newFileName(),serverJobsDirectory),name);
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"create job '%s' fail: %s",String_cString(name),Errors_getText(error));
      Semaphore_unlock(&jobList.lock);
      return;
    }
    File_close(&fileHandle);
    (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

    // create new job
    jobNode = newJob(JOB_TYPE_CREATE,fileName);
    if (jobNode == NULL)
    {
      File_delete(fileName,FALSE);
      File_deleteFileName(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
      Semaphore_unlock(&jobList.lock);
      return;
    }
    copyJobOptions(serverDefaultJobOptions,&jobNode->jobOptions);

    // free resources
    File_deleteFileName(fileName);

    // write job to file
    updateJob(jobNode);

    // add new job to list
    List_append(&jobList,jobNode);
  }
  assert(jobNode != NULL);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"%d",jobNode->id);
}

/***********************************************************************\
* Name   : serverCommand_jobCopy
* Purpose: copy job
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <new job name>
*          Result:
*            <new job id>
\***********************************************************************/

LOCAL void serverCommand_jobCopy(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        fileName;
  FileHandle    fileHandle;
  Errors        error;
  JobNode       *newJobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name");
    return;
  }
  name = arguments[1];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // check if job already exists
    if (findJobByName(name) != NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(name));
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // create empty job file
    fileName = File_appendFileName(File_setFileNameCString(File_newFileName(),serverJobsDirectory),name);
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"create job '%s' fail: %s",String_cString(name),Errors_getText(error));
      Semaphore_unlock(&jobList.lock);
      return;
    }
    File_close(&fileHandle);
    (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

    // copy job
    newJobNode = copyJob(jobNode,fileName);
    if (newJobNode == NULL)
    {
      File_deleteFileName(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"error copy job #%d",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // free resources
    File_deleteFileName(fileName);

    // write job to file
    updateJob(jobNode);

    // add new job to list
    List_append(&jobList,newJobNode);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobRename
* Purpose: rename job
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <new job name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobRename(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        fileName;
  Errors        error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name");
    return;
  }
  name = arguments[1];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // check if job already exists
    if (findJobByName(name) != NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(name));
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // rename job
    fileName = File_appendFileName(File_setFileNameCString(File_newFileName(),serverJobsDirectory),name);
    error = File_rename(jobNode->fileName,fileName);
    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"error renaming job #%d: %s",jobId,Errors_getText(error));
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // free resources
    File_deleteFileName(fileName);

    // store new file name
    File_appendFileName(File_setFileNameCString(jobNode->fileName,serverJobsDirectory),name);
    String_set(jobNode->name,name);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobDelete
* Purpose: delete job
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobDelete(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove job in list if not running or requested volume
    if (CHECK_JOB_IS_RUNNING(jobNode))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job #%d running",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // delete job file
    error = File_delete(jobNode->fileName,FALSE);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"error deleting job #%d: %s",jobId,Errors_getText(error));
      Semaphore_unlock(&jobList.lock);
      return;
    }
    List_remove(&jobList,jobNode);
    deleteJob(jobNode);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobStart
* Purpose: start job execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobStart(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  ArchiveTypes  archiveType;
  bool          dryRunFlag;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  // get archive type
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storage archive type");
    return;
  }
  if      (String_equalsCString(arguments[1],"normal"     ))
  {
    archiveType = ARCHIVE_TYPE_NORMAL;
    dryRunFlag  = FALSE;
  }
  else if (String_equalsCString(arguments[1],"full"       ))
  {
    archiveType = ARCHIVE_TYPE_FULL;
    dryRunFlag  = FALSE;
  }
  else if (String_equalsCString(arguments[1],"incremental"))
  {
    archiveType = ARCHIVE_TYPE_INCREMENTAL;
    dryRunFlag  = FALSE;
  }
  else if (String_equalsCString(arguments[1],"differential"))
  {
    archiveType = ARCHIVE_TYPE_DIFFERENTIAL;
    dryRunFlag  = FALSE;
  }
  else if (String_equalsCString(arguments[1],"dry-run"    ))
  {
    archiveType = ARCHIVE_TYPE_NORMAL;
    dryRunFlag  = TRUE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown archive type '%S'",arguments[1]);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // run job
    if  (!CHECK_JOB_IS_ACTIVE(jobNode))
    {
      // set state
      jobNode->jobOptions.dryRunFlag = dryRunFlag;
      jobNode->state                 = JOB_STATE_WAITING;
      jobNode->archiveType           = archiveType;
      jobNode->requestedAbortFlag    = FALSE;
      resetJobRunningInfo(jobNode);
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobAbort
* Purpose: abort job execution
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobAbort(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // abort job
    if      (CHECK_JOB_IS_RUNNING(jobNode))
    {
      jobNode->requestedAbortFlag = TRUE;
      while (CHECK_JOB_IS_RUNNING(jobNode))
      {
        Semaphore_waitModified(&jobList.lock);
      }
    }
    else if (CHECK_JOB_IS_ACTIVE(jobNode))
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
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobFlush(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // update all job files
  updateAllJobs();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeList
* Purpose: get include list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <type> <pattern> <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_includeList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  uint       jobId;
  JobNode    *jobNode;
  String     string;
  EntryNode  *entryNode;
  const char *entryType,*patternType;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send include list
    string = String_new();
    LIST_ITERATE(&jobNode->includeEntryList,entryNode)
    {
      entryType   = NULL;
      patternType = NULL;
      switch (entryNode->type)
      {
        case ENTRY_TYPE_FILE : entryType = "FILE";  break;
        case ENTRY_TYPE_IMAGE: entryType = "IMAGE"; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      switch (entryNode->pattern.type)
      {
        case PATTERN_TYPE_GLOB          : patternType = "GLOB";           break;
        case PATTERN_TYPE_REGEX         : patternType = "REGEX";          break;
        case PATTERN_TYPE_EXTENDED_REGEX: patternType = "EXTENDED_REGEX"; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      String_mapCString(String_set(string,entryNode->string),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN));
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%s %s %'S",
                       entryType,
                       patternType,
                       string
                      );
    }
    String_delete(string);
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeListClear
* Purpose: clear include list
* Purpose: clear job include list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear include list
    EntryList_clear(&jobNode->includeEntryList);
    jobNode->modifiedFlag = TRUE;
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
}

/***********************************************************************\
* Name   : serverCommand_includeListAdd
* Purpose: add entry to include list
* Purpose: add entry to job include list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <entry type>
*            <pattern type>
*            <pattern>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};
  const char* MAP_BIN[]   = {"\n","\r","\\"};

  uint          jobId;
  EntryTypes    entryType;
  PatternTypes  patternType;
  String        string;
  String        pattern;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entry type");
    return;
  }
  if      (String_equalsCString(arguments[1],"FILE"))
  {
    entryType = ENTRY_TYPE_FILE;
  }
  else if (String_equalsCString(arguments[1],"IMAGE"))
  {
    entryType = ENTRY_TYPE_IMAGE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown entry type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern type");
    return;
  }
  if      (String_equalsCString(arguments[2],"GLOB"))
  {
    patternType = PATTERN_TYPE_GLOB;
  }
  else if (String_equalsCString(arguments[2],"REGEX"))
  {
    patternType = PATTERN_TYPE_REGEX;
  }
  else if (String_equalsCString(arguments[2],"EXTENDED_REGEX"))
  {
    patternType = PATTERN_TYPE_EXTENDED_REGEX;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown pattern type '%S'",arguments[2]);
    return;
  }
  if (argumentCount < 4)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern");
    return;
  }
  string = arguments[3];

  pattern = String_mapCString(String_duplicate(string),STRING_BEGIN,MAP_TEXT,MAP_BIN,SIZE_OF_ARRAY(MAP_TEXT));
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      String_delete(pattern);
      return;
    }

    // add to include list
    EntryList_append(&jobNode->includeEntryList,entryType,pattern,patternType);
    jobNode->modifiedFlag = TRUE;
  }
  String_delete(pattern);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeList
* Purpose: get job exclude list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <type> <pattern> <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_excludeList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  uint        jobId;
  JobNode     *jobNode;
  String      string;
  PatternNode *patternNode;
  const char  *type;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send exclude list
    string = String_new();
    LIST_ITERATE(&jobNode->excludePatternList,patternNode)
    {
      type = NULL;
      switch (patternNode->pattern.type)
      {
        case PATTERN_TYPE_GLOB          : type = "GLOB";           break;
        case PATTERN_TYPE_REGEX         : type = "REGEX";          break;
        case PATTERN_TYPE_EXTENDED_REGEX: type = "EXTENDED_REGEX"; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      String_mapCString(String_set(string,patternNode->string),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN));
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%s %'S",
                       type,
                       string
                      );
    }
    String_delete(string);
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListClear
* Purpose: clear job exclude list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->excludePatternList);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListAdd
* Purpose: add entry to job exclude list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <entry type>
*            <pattern type>
*            <pattern>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  uint          jobId;
  PatternTypes  patternType;
  String        string;
  String        pattern;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern type");
    return;
  }
  if      (String_equalsCString(arguments[1],"GLOB"))
  {
    patternType = PATTERN_TYPE_GLOB;
  }
  else if (String_equalsCString(arguments[1],"REGEX"))
  {
    patternType = PATTERN_TYPE_REGEX;
  }
  else if (String_equalsCString(arguments[1],"EXTENDED_REGEX"))
  {
    patternType = PATTERN_TYPE_EXTENDED_REGEX;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown pattern type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern");
    return;
  }
  string = arguments[2];

  pattern = String_mapCString(String_duplicate(string),STRING_BEGIN,MAP_TEXT,MAP_BIN,SIZE_OF_ARRAY(MAP_TEXT));
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      String_delete(pattern);
      return;
    }

    // add to exclude list
    PatternList_append(&jobNode->excludePatternList,pattern,patternType);
    jobNode->modifiedFlag = TRUE;
  }
  String_delete(pattern);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceList
* Purpose: get soource list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <type> <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_sourceList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint        jobId;
  JobNode     *jobNode;
  PatternNode *patternNode;
  const char  *type;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send soource list
    LIST_ITERATE(&jobNode->deltaSourcePatternList,patternNode)
    {
      type = NULL;
      switch (patternNode->pattern.type)
      {
        case PATTERN_TYPE_GLOB          : type = "GLOB";           break;
        case PATTERN_TYPE_REGEX         : type = "REGEX";          break;
        case PATTERN_TYPE_EXTENDED_REGEX: type = "EXTENDED_REGEX"; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%s %'S",
                       type,
                       patternNode->string
                      );
    }
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListClear
* Purpose: clear source list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear source list
    PatternList_clear(&jobNode->deltaSourcePatternList);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListAdd
* Purpose: add entry to source list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <entry type>
*            <pattern type>
*            <pattern>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  PatternTypes  patternType;
  String        pattern;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern type");
    return;
  }
  if      (String_equalsCString(arguments[1],"GLOB"))
  {
    patternType = PATTERN_TYPE_GLOB;
  }
  else if (String_equalsCString(arguments[1],"REGEX"))
  {
    patternType = PATTERN_TYPE_REGEX;
  }
  else if (String_equalsCString(arguments[1],"EXTENDED_REGEX"))
  {
    patternType = PATTERN_TYPE_EXTENDED_REGEX;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown pattern type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern");
    return;
  }
  pattern = arguments[2];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // add to source list
    PatternList_append(&jobNode->deltaSourcePatternList,pattern,patternType);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressList
* Purpose: get job exclude compress list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <type> <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_excludeCompressList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint        jobId;
  JobNode     *jobNode;
  PatternNode *patternNode;
  const char  *type;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send exclude list
    LIST_ITERATE(&jobNode->compressExcludePatternList,patternNode)
    {
      type = NULL;
      switch (patternNode->pattern.type)
      {
        case PATTERN_TYPE_GLOB          : type = "GLOB";           break;
        case PATTERN_TYPE_REGEX         : type = "REGEX";          break;
        case PATTERN_TYPE_EXTENDED_REGEX: type = "EXTENDED_REGEX"; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%s %'S",
                       type,
                       patternNode->string
                      );
    }
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListClear
* Purpose: clear job exclude compress list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->compressExcludePatternList);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListAdd
* Purpose: add entry to job exclude compress list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <entry type>
*            <pattern type>
*            <pattern>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  PatternTypes  patternType;
  String        pattern;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern type");
    return;
  }
  if      (String_equalsCString(arguments[1],"GLOB"))
  {
    patternType = PATTERN_TYPE_GLOB;
  }
  else if (String_equalsCString(arguments[1],"REGEX"))
  {
    patternType = PATTERN_TYPE_REGEX;
  }
  else if (String_equalsCString(arguments[1],"EXTENDED_REGEX"))
  {
    patternType = PATTERN_TYPE_EXTENDED_REGEX;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown pattern type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern");
    return;
  }
  pattern = arguments[2];

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // add to exclude list
    PatternList_append(&jobNode->compressExcludePatternList,pattern,patternType);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_scheduleList
* Purpose: get job schedule list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
*            <year> <month> <day> <week day> <hour> <minute> <enabled> <type>
*            ...
\***********************************************************************/

LOCAL void serverCommand_scheduleList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint         jobId;
  JobNode      *jobNode;
  String       line;
  ScheduleNode *scheduleNode;
  String       names;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send schedule list
    line = String_new();
    LIST_ITERATE(&jobNode->scheduleList,scheduleNode)
    {
      String_clear(line);

      if (scheduleNode->year != SCHEDULE_ANY)
      {
        String_format(line,"%d",scheduleNode->year);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,'-');
      if (scheduleNode->month != SCHEDULE_ANY)
      {
        String_format(line,"%02d",scheduleNode->month);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,'-');
      if (scheduleNode->day != SCHEDULE_ANY)
      {
        String_format(line,"%02d",scheduleNode->day);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,' ');
      if (scheduleNode->weekDays != SCHEDULE_ANY_DAY)
      {
        names = String_new();

        if (IN_SET(scheduleNode->weekDays,WEEKDAY_MON)) { String_joinCString(names,"Mon",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_TUE)) { String_joinCString(names,"Tue",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_WED)) { String_joinCString(names,"Wed",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_THU)) { String_joinCString(names,"Thu",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_FRI)) { String_joinCString(names,"Fri",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_SAT)) { String_joinCString(names,"Sat",','); }
        if (IN_SET(scheduleNode->weekDays,WEEKDAY_SUN)) { String_joinCString(names,"Sun",','); }

        String_append(line,names);
        String_appendChar(line,' ');

        String_delete(names);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,' ');
      if (scheduleNode->hour != SCHEDULE_ANY)
      {
        String_format(line,"%02d",scheduleNode->hour);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,':');
      if (scheduleNode->minute != SCHEDULE_ANY)
      {
        String_format(line,"%02d",scheduleNode->minute);
      }
      else
      {
        String_appendCString(line,"*");
      }
      String_appendChar(line,' ');
      String_format(line,"%y",scheduleNode->enabled);
      String_appendChar(line,' ');
      switch (scheduleNode->archiveType)
      {
        case ARCHIVE_TYPE_NORMAL      : String_appendCString(line,"normal"      ); break;
        case ARCHIVE_TYPE_FULL        : String_appendCString(line,"full"        ); break;
        case ARCHIVE_TYPE_INCREMENTAL : String_appendCString(line,"incremental" ); break;
        case ARCHIVE_TYPE_DIFFERENTIAL: String_appendCString(line,"differential"); break;
        default                       : String_appendCString(line,"*"           ); break;
      }

      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%S",
                       line
                      );
    }
    String_delete(line);
  }
  Semaphore_unlock(&jobList.lock);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_scheduleListClear
* Purpose: clear job schedule list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear schedule list
    List_clear(&jobNode->scheduleList,NULL,NULL);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_scheduleListAdd
* Purpose: add entry to job schedule list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <date>
*            <week day>
*            <time>
*            <enabled/disabled 0|1>
*            <type>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  ScheduleNode  *scheduleNode;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // initialise variables

  // get job id, date, weekday, time, enable, type
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected date");
    return;
  }
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected week day");
    return;
  }
  if (argumentCount < 4)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected time");
    return;
  }
  if (argumentCount < 5)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected enable/disable");
    return;
  }
  if (argumentCount < 6)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected type");
    return;
  }

  // parse schedule
  scheduleNode = parseScheduleParts(arguments[1],
                                    arguments[2],
                                    arguments[3],
                                    arguments[4],
                                    arguments[5]
                                   );
  if (scheduleNode == NULL)
  {
    sendClientResult(clientInfo,
                     id,
                     TRUE,
                     ERROR_PARSING,
                     "cannot parse schedule '%S %S %S %S %S'",
                     arguments[1],
                     arguments[2],
                     arguments[3],
                     arguments[4],
                     arguments[5]
                    );
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // add to schedule list
    List_append(&jobNode->scheduleList,scheduleNode);
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordsClear
* Purpose: clear decrypt passwords in internal list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage name>
*            <destination directory>
*            <overwrite flag>
*            <files>...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_decryptPasswordsClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // clear decrypt password list
  Archive_clearDecryptPasswords();
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // unlock
  Semaphore_unlock(&jobList.lock);
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordAdd
* Purpose: add password to internal list of decrypt passwords
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_decryptPasswordAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Password password;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  // get password
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password");
    return;
  }

  // add to decrypt password list
  Password_init(&password);
  Password_setString(&password,arguments[0]);
  Archive_appendDecryptPassword(&password);
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  Password_done(&password);
}

/***********************************************************************\
* Name   : serverCommand_ftpPassword
* Purpose: set job FTP password
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_ftpPassword(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String password;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get password
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password");
    return;
  }
  password = arguments[0];

  if (clientInfo->jobOptions.ftpServer.password == NULL) clientInfo->jobOptions.ftpServer.password = Password_new();
  Password_setString(clientInfo->jobOptions.ftpServer.password,password);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sshPassword
* Purpose: set job SSH password
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sshPassword(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String password;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get password
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password");
    return;
  }
  password = arguments[0];

  if (clientInfo->jobOptions.sshServer.password == NULL) clientInfo->jobOptions.sshServer.password = Password_new();
  Password_setString(clientInfo->jobOptions.sshServer.password,password);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_cryptPassword
* Purpose: set job encryption password
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>|0
*            <password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_cryptPassword(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  String        password;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id, type, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password");
    return;
  }
  password = arguments[1];

  if (jobId != 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      // find job
      jobNode = findJobById(jobId);
      if (jobNode == NULL)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
        Semaphore_unlock(&jobList.lock);
        return;
      }

      // set password
      if (jobNode->cryptPassword == NULL) jobNode->cryptPassword = Password_new();
      Password_setString(jobNode->cryptPassword,password);
    }
  }
  else
  {
    Password_setString(clientInfo->jobOptions.cryptPassword,password);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_passwordsClear
* Purpose: clear ssh/ftp/crypt passwords stored in memory
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_passwordsClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  if (clientInfo->jobOptions.ftpServer.password != NULL) Password_clear(clientInfo->jobOptions.ftpServer.password);
  if (clientInfo->jobOptions.sshServer.password != NULL) Password_clear(clientInfo->jobOptions.sshServer.password);
  if (clientInfo->jobOptions.cryptPassword != NULL) Password_clear(clientInfo->jobOptions.cryptPassword);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeLoad
* Purpose: set number of loaded volume
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*            <volume number>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeLoad(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  uint          volumeNumber;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  // get volume number
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected volume number");
    return;
  }
  volumeNumber = String_toInteger(arguments[1],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // set volume number
    jobNode->volumeNumber = volumeNumber;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeUnload
* Purpose: unload volumne
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <job id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeUnload(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint          jobId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get job id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find job
    jobNode = findJobById(jobId);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job #%d not found",jobId);
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
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage name>
*          Result:
*            FILE <name> <size> <time modifed> <chunk size> <delta compress algorithm> \
*            <byte compress algorithm> <crypt algorithm> <crypt type> <delta source name> \
*            <delta source size> <fragment offest> <fragment size>
*            IMAGE <name> <size> <chunk size> <delta compress algorithm> \
*            <byte compress algorithm> <crypt algorithm> <crypt type> <delta source name> \
*            <delta source size> <block size> <block offest> <block size>
*            DIRECTORY <name> <time modified> <crypt algorithm> <crypt type>
*            LINK <link name> <name> <crypt algorithm> <crypt type>
*            HARDLINK <name> <size> <time modifed> <chunk size> <delta compress algorithm> \
*            <byte compress algorithm> <crypt algorithm> <crypt type> <delta source name> \
*            <delta source size> <fragment offest> <fragment size>
*            SPECIAL <name>
*            ...
\***********************************************************************/

LOCAL void serverCommand_archiveList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String            storageName;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get archive name, pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storage name");
    return;
  }
  storageName = arguments[0];

  // open archive
  error = Archive_open(&archiveInfo,
                       storageName,
                       &clientInfo->jobOptions,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"%s",Errors_getText(error));
    return;
  }

  // list contents
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveInfo,TRUE)
         && (error == ERROR_NONE)
         && !commandAborted(clientInfo,id)
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
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          &deltaCompressAlgorithm,
                                          &byteCompressAlgorithm,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          fileName,
                                          &fileInfo,
                                          deltaSourceName,
                                          &deltaSourceSize,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
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
                               "FILE %'S %llu %llu %llu %d %d %d %d %'S %llu %llu %llu",
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
            String             deltaSourceName;
            uint64             deltaSourceSize;
            uint64             blockOffset,blockCount;

            // open archive file
            imageName       = String_new();
            deltaSourceName = String_new();
            error = Archive_readImageEntry(&archiveInfo,
                                           &archiveEntryInfo,
                                           &deltaCompressAlgorithm,
                                           &byteCompressAlgorithm,
                                           &cryptAlgorithm,
                                           &cryptType,
                                           imageName,
                                           &deviceInfo,
                                           deltaSourceName,
                                           &deltaSourceSize,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
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
                               "IMAGE %'S %llu %llu %d %d %d %d %u %llu %llu",
                               imageName,
                               deviceInfo.size,
                               archiveEntryInfo.image.chunkImageData.info.size,
                               deltaCompressAlgorithm,
                               byteCompressAlgorithm,
                               cryptAlgorithm,
                               cryptType,
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
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveEntryInfo,
                                               &cryptAlgorithm,
                                               &cryptType,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
              String_delete(directoryName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "DIRECTORY %'S %llu %d %d",
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
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          linkName,
                                          name,
                                          NULL
                                         );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
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
                               "LINK %'S %'S %d %d",
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
            error = Archive_readHardLinkEntry(&archiveInfo,
                                              &archiveEntryInfo,
                                              &deltaCompressAlgorithm,
                                              &byteCompressAlgorithm,
                                              &cryptAlgorithm,
                                              &cryptType,
                                              &fileNameList,
                                              &fileInfo,
                                              deltaSourceName,
                                              &deltaSourceSize,
                                              &fragmentOffset,
                                              &fragmentSize
                                             );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
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
                               "HARDLINK %'S %llu %llu %llu %d %d %d %d %'S %llu %llu %llu",
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
            String   name;
            FileInfo fileInfo;

            // open archive link
            name = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveEntryInfo,
                                             NULL,
                                             NULL,
                                             name,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S' (error: %s)",storageName,Errors_getText(error));
              String_delete(name);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,name,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,name,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "SPECIAL %'S",
                               name
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
      sendClientResult(clientInfo,id,TRUE,error,"Cannot read next entry of storage '%S' (error: %s)",storageName,Errors_getText(error));
      break;
    }
  }

  // close archive
  Archive_close(&archiveInfo);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

typedef struct
{
  ClientInfo *clientInfo;
  uint       id;
} RestoreCommandInfo;

/***********************************************************************\
* Name   : updateRestoreCommandStatus
* Purpose: update restore job status
* Input  : jobNode           - job node
*          error             - error code
*          restoreStatusInfo - create status info data
* Output : -
* Return : TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateRestoreCommandStatus(RestoreCommandInfo      *restoreCommandInfo,
                                      Errors                  error,
                                      const RestoreStatusInfo *restoreStatusInfo
                                     )
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  String string;

  assert(restoreCommandInfo != NULL);
  assert(restoreStatusInfo != NULL);
  assert(restoreStatusInfo->name != NULL);
  assert(restoreStatusInfo->storageName != NULL);

  UNUSED_VARIABLE(error);

  string = String_mapCString(String_duplicate(restoreStatusInfo->name),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN));
  sendClientResult(restoreCommandInfo->clientInfo,
                   restoreCommandInfo->id,
                   FALSE,
                   ERROR_NONE,
                   "%llu %llu %llu %llu %'S",
                   restoreStatusInfo->entryDoneBytes,
                   restoreStatusInfo->entryTotalBytes,
                   restoreStatusInfo->archiveDoneBytes,
                   restoreStatusInfo->archiveTotalBytes,
                   string
                  );
    String_delete(string);

  return !commandAborted(restoreCommandInfo->clientInfo,restoreCommandInfo->id);
}


/***********************************************************************\
* Name   : serverCommand_storageListClear
* Purpose: clear storage list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Array_clear(clientInfo->storageIdArray,NULL,NULL);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListAdd
* Purpose: add to storage list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  DatabaseId storageId;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get storage id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storage id");
    return;
  }
  storageId = (DatabaseId)String_toInteger(arguments[0],0,NULL,NULL,0);

  // add to storage id array
  Array_append(clientInfo->storageIdArray,&storageId);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_restore
* Purpose: restore archives/files
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage name>
*            <destination directory>
*            <overwrite flag>
*            <files>...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_restore(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  StringList         archiveNameList;
  String             string;
  EntryList          includeEntryList;
  PatternList        excludePatternList;
  PatternList        deltaSourcePatternList;
  uint               z;
  RestoreCommandInfo restoreCommandInfo;
  Errors             error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  StringList_init(&archiveNameList);
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  PatternList_init(&deltaSourcePatternList);

  // get archive name, destination, overwrite flag, files
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storage name");
    PatternList_done(&deltaSourcePatternList);
    PatternList_done(&excludePatternList);
    EntryList_done(&includeEntryList);
    StringList_done(&archiveNameList);
    return;
  }
  StringList_append(&archiveNameList,arguments[0]);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected destination directory or device name");
    PatternList_done(&deltaSourcePatternList);
    PatternList_done(&excludePatternList);
    EntryList_done(&includeEntryList);
    StringList_done(&archiveNameList);
    return;
  }
  clientInfo->jobOptions.destination = String_duplicate(arguments[1]);
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected overwrite flag");
    PatternList_done(&deltaSourcePatternList);
    PatternList_done(&excludePatternList);
    EntryList_done(&includeEntryList);
    StringList_done(&archiveNameList);
    return;
  }
  clientInfo->jobOptions.overwriteFilesFlag = String_equalsCString(arguments[2],"1");
  string = String_new();
  for (z = 3; z < argumentCount; z++)
  {
    EntryList_append(&includeEntryList,
//???
ENTRY_TYPE_FILE,
                     String_mapCString(String_set(string,arguments[z]),STRING_BEGIN,MAP_TEXT,MAP_BIN,SIZE_OF_ARRAY(MAP_TEXT)),
                     PATTERN_TYPE_GLOB
                    );
  }
  String_delete(string);

  // try to pause background index thread, do short delay to make sure network connection is possible
  restoreFlag = TRUE;
  if (indexFlag)
  {
    z = 0;
    while ((z < 5*60) && indexFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
      z += 10;
    }
    Misc_udelay(30LL*1000LL*1000LL);
  }

  // restore
  restoreCommandInfo.clientInfo = clientInfo;
  restoreCommandInfo.id         = id;
  error = Command_restore(&archiveNameList,
                          &includeEntryList,
                          NULL,
                          &clientInfo->jobOptions,
                          NULL,
                          NULL,
                          (RestoreStatusInfoFunction)updateRestoreCommandStatus,
                          &restoreCommandInfo,
                          NULL,
                          NULL
                         );
  sendClientResult(clientInfo,id,TRUE,error,Errors_getText(error));
  restoreFlag = FALSE;

  // free resources
  PatternList_done(&deltaSourcePatternList);
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  StringList_done(&archiveNameList);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageInfo
* Purpose: get index database storage info
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <entries OK> <entries create> <entries update requested> \
*            <entries update> <entries error>
\***********************************************************************/

LOCAL void serverCommand_indexStorageInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  long storageEntryCountOK;
  long storageEntryCountCreate;
  long storageEntryCountUpdateRequested;
  long storageEntryCountUpdate;
  long storageEntryCountError;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  if (indexDatabaseHandle != NULL)
  {
    storageEntryCountOK              = Index_countState(indexDatabaseHandle,INDEX_STATE_OK              );
    storageEntryCountCreate          = Index_countState(indexDatabaseHandle,INDEX_STATE_CREATE          );
    storageEntryCountUpdateRequested = Index_countState(indexDatabaseHandle,INDEX_STATE_UPDATE_REQUESTED);
    storageEntryCountUpdate          = Index_countState(indexDatabaseHandle,INDEX_STATE_UPDATE          );
    storageEntryCountError           = Index_countState(indexDatabaseHandle,INDEX_STATE_ERROR           );

    if (   (storageEntryCountOK              >= 0L)
        && (storageEntryCountCreate          >= 0L)
        && (storageEntryCountUpdateRequested >= 0L)
        && (storageEntryCountUpdate          >= 0L)
        && (storageEntryCountError           >= 0L)
       )
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                       "%ld %ld %ld %ld %ld",
                       storageEntryCountOK,
                       storageEntryCountCreate,
                       storageEntryCountUpdateRequested,
                       storageEntryCountUpdate,
                       storageEntryCountError
                      );
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"get index database fail");
    }
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
  }
}

/***********************************************************************\
* Name   : serverCommand_indexStorageList
* Purpose: get index database storage list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <max. count>|0
*            <status>|*
*            <mode>|*
*            <name pattern>
*          Result:
*            <storage id> <name>  <created date/time> <size> <state> \
*            <mode> <last checked date/time> <error message>
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexStorageList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  ulong               maxCount;
  String              indexStatusText;
  IndexStates         indexState;
  String              indexModeText;
  IndexModes          indexMode;
  String              patternText;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  ulong               n;
  String              storageName;
  String              printableStorageName;
  String              errorMessage;
  String              string1,string2;
  DatabaseId          storageId;
  uint64              storageDateTime;
  uint64              size;
  uint64              lastCheckedDateTime;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get max. count, status pattern, filter pattern,
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected max. number of entries to send");
    return;
  }
  maxCount = String_toInteger64(arguments[0],STRING_BEGIN,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter status");
    return;
  }
  indexStatusText = arguments[1];
  indexState = Index_stringToState(indexStatusText);
  if (indexState == INDEX_STATE_UNKNOWN)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"invalid filter status '%S'",indexStatusText);
    return;
  }
  indexModeText = arguments[2];
  indexMode = Index_stringToMode(indexModeText);
  if (indexMode == INDEX_MODE_UNKNOWN)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"invalid filter mode '%S'",indexModeText);
    return;
  }
  if (argumentCount < 4)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter pattern");
    return;
  }
  patternText = arguments[3];

  if (indexDatabaseHandle != NULL)
  {
    // initialise variables
    storageName          = String_new();
    errorMessage         = String_new();
    printableStorageName = String_new();
    string1              = String_new();
    string2              = String_new();

    // list index
    error = Index_initListStorage(&databaseQueryHandle,
                                  indexDatabaseHandle,
                                  indexState,
                                  patternText
                                 );
    if (error != ERROR_NONE)
    {
      String_delete(string2);
      String_delete(string1);
      String_delete(printableStorageName);
      String_delete(errorMessage);
      String_delete(storageName);

      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
      return;
    }
    n = 0L;
    while (   ((maxCount == 0L) || (n < maxCount))
           && Index_getNextStorage(&databaseQueryHandle,
                                   &storageId,
                                   storageName,
                                   &storageDateTime,
                                   &size,
                                   &indexState,
                                   &indexMode,
                                   &lastCheckedDateTime,
                                   errorMessage
                                  )
          )
    {
      assert(indexState < SIZE_OF_ARRAY(INDEX_STATE_STRINGS));
      assert(indexMode < SIZE_OF_ARRAY(INDEX_MODE_STRINGS));

      Storage_getPrintableName(printableStorageName,storageName);

      String_mapCString(String_set(string1,printableStorageName),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,errorMessage),STRING_BEGIN,MAP_BIN,MAP_TEXT,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "%llu %'S %llu %llu %'s %'s %llu %'S",
                       storageId,
                       string1,
                       storageDateTime,
                       size,
                       INDEX_STATE_STRINGS[indexState],
                       INDEX_MODE_STRINGS[indexMode],
                       lastCheckedDateTime,
                       string2
                      );
      n++;
    }
    Index_doneList(&databaseQueryHandle);

    // free resources
    String_delete(string2);
    String_delete(string1);
    String_delete(printableStorageName);
    String_delete(errorMessage);
    String_delete(storageName);

    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
  }
}

/***********************************************************************\
* Name   : serverCommand_indexStorageAdd
* Purpose: add storage to index database
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <storage name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String   storageName;
  int64    storageId;
  Errors   error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get archive name
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storage name");
    return;
  }
  storageName = arguments[0];

  if (indexDatabaseHandle != NULL)
  {
    // create index
    error = Index_create(indexDatabaseHandle,
                         storageName,
                         INDEX_STATE_UPDATE_REQUESTED,
                         INDEX_MODE_MANUAL,
                         &storageId
                        );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"send index add request fail");
      return;
    }
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexStorageRemove
* Purpose: remove storage from index database
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <status>|*
*            <id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageRemove(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  bool                stateAny;
  IndexStates         state;
  DatabaseId          storageId;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexStates         storageState;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // state, id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter status");
    return;
  }
  stateAny = FALSE;
  state    = INDEX_STATE_NONE;
  if      (String_equalsCString(arguments[0],"*"))
  {
    stateAny = TRUE;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"OK"))
  {
    state = INDEX_STATE_OK;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"UPDATE_REQUESTED"))
  {
    state = INDEX_STATE_UPDATE_REQUESTED;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"UPDATE"))
  {
    state = INDEX_STATE_UPDATE;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"ERROR"))
  {
    state = INDEX_STATE_ERROR;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter state *,OK,UPDATE_REQUESTED,UPDATE,ERROR");
    return;
  }
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter pattern");
    return;
  }
  storageId = (DatabaseId)String_toInteger64(arguments[1],0,NULL,NULL,0);

  if (indexDatabaseHandle != NULL)
  {
    if (storageId != 0)
    {
      // delete index
      error = Index_delete(indexDatabaseHandle,
                           storageId
                          );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,error,"remove index fail");
        return;
      }
    }
    else
    {
      error = Index_initListStorage(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    INDEX_STATE_ALL,
                                    NULL
                                   );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        return;
      }
      while (Index_getNextStorage(&databaseQueryHandle,
                                  &storageId,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &storageState,
                                  NULL,
                                  NULL,
                                  NULL
                                 )
            )
      {
        if (stateAny || (state == storageState))
        {
          // delete index
          error = Index_delete(indexDatabaseHandle,
                               storageId
                              );
          if (error != ERROR_NONE)
          {
            Index_doneList(&databaseQueryHandle);
            sendClientResult(clientInfo,id,TRUE,error,"remove index fail");
            return;
          }
        }
      }
      Index_doneList(&databaseQueryHandle);
    }
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexStorageRefresh
* Purpose: refresh index database for storage
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <status>|*
*            <id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexStorageRefresh(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  bool                stateAny;
  IndexStates         state;
  int64               storageId;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexStates         storageState;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // state, id
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter status");
    return;
  }
  stateAny = FALSE;
  state    = INDEX_STATE_NONE;
  if      (String_equalsCString(arguments[0],"*"))
  {
    stateAny = TRUE;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"OK"))
  {
    state = INDEX_STATE_OK;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"UPDATE_REQUESTED"))
  {
    state = INDEX_STATE_UPDATE_REQUESTED;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"UPDATE"))
  {
    state = INDEX_STATE_UPDATE;
  }
  else if (String_equalsIgnoreCaseCString(arguments[0],"ERROR"))
  {
    state = INDEX_STATE_ERROR;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter state *,OK,UPDATE_REQUESTED,UPDATE,ERROR");
    return;
  }
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter pattern");
    return;
  }
  storageId = (DatabaseId)String_toInteger64(arguments[1],0,NULL,NULL,0);

  if (indexDatabaseHandle != NULL)
  {
    if (storageId != 0)
    {
      // set state
      Index_setState(indexDatabaseHandle,
                     storageId,
                     INDEX_STATE_UPDATE_REQUESTED,
                     0LL,
                     NULL
                    );
    }
    else
    {
      error = Index_initListStorage(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    INDEX_STATE_ALL,
                                    NULL
                                   );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        return;
      }
      while (Index_getNextStorage(&databaseQueryHandle,
                                  &storageId,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &storageState,
                                  NULL,
                                  NULL,
                                  NULL
                                 )
            )
      {
        if (stateAny || (state == storageState))
        {
          // set state
          Index_setState(indexDatabaseHandle,
                         storageId,
                         INDEX_STATE_UPDATE_REQUESTED,
                         0LL,
                         NULL
                        );
        }
      }
      Index_doneList(&databaseQueryHandle);
    }
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}


/***********************************************************************\
* Name   : freeIndexNode
* Purpose: free allocated index node
* Input  : indexNode - index node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeIndexNode(IndexNode *indexNode, void *userData)
{
  assert(indexNode != NULL);

  UNUSED_VARIABLE(userData);

  switch (indexNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(indexNode->link.destinationName);
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  String_delete(indexNode->name);
  String_delete(indexNode->storageName);
}

/***********************************************************************\
* Name   : newIndexEntryNode
* Purpose: create new index entry node
* Input  : indexList        - index list
*          archiveEntryType - archive entry type
*          storageName      - storage name
*          name             - entry name
*          timeModified     - modification time stamp [s]
* Output : -
* Return : index node or NULL
* Notes  : -
\***********************************************************************/

LOCAL IndexNode *newIndexEntryNode(IndexList *indexList, ArchiveEntryTypes archiveEntryType, const String storageName, const String name, uint64 timeModified)
{
  IndexNode *indexNode;
  bool       foundFlag;
  IndexNode *nextIndexNode;

  // allocate node
  indexNode = LIST_NEW_NODE(IndexNode);
  if (indexNode == NULL)
  {
    return NULL;
  }

  // initialize
  indexNode->type         = archiveEntryType;
  indexNode->storageName  = String_duplicate(storageName);
  indexNode->name         = String_duplicate(name);
  indexNode->timeModified = timeModified;
  switch (indexNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      indexNode->link.destinationName = String_new();
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  // insert into list
  foundFlag     = FALSE;
  nextIndexNode = indexList->head;
  while (   (nextIndexNode != NULL)
         && !foundFlag
        )
  {
    switch (String_compare(nextIndexNode->storageName,indexNode->storageName,NULL,NULL))
    {
      case -1:
        // next
        nextIndexNode = nextIndexNode->next;
        break;
      case  0:
      case  1:
        // compare
        switch (String_compare(nextIndexNode->name,indexNode->name,NULL,NULL))
        {
          case -1:
            // next
            nextIndexNode = nextIndexNode->next;
            break;
          case  0:
          case  1:
            // found
            foundFlag = TRUE;
            break;
        }
        break;
    }
  }
  List_insert(indexList,indexNode,nextIndexNode);

  return indexNode;
}

/***********************************************************************\
* Name   : findIndexEntryNode
* Purpose: find index entry node
* Input  : indexList - index list
*          type      - archive entry type
*          name      - entry name
* Output : -
* Return : index node or NULL
* Notes  : -
\***********************************************************************/

LOCAL IndexNode *findIndexEntryNode(IndexList         *indexList,
                                    ArchiveEntryTypes type,
                                    const String      name
                                   )
{
  IndexNode *foundIndexNode;
  IndexNode *indexNode;

  assert(indexList != NULL);
  assert(name != NULL);

  foundIndexNode = NULL;
  indexNode = indexList->head;
  while (   (indexNode != NULL)
         && (foundIndexNode == NULL)
        )
  {
    if      (indexNode->type < type)
    {
      // next
      indexNode = indexNode->next;
    }
    else if (indexNode->type == type)
    {
      // compare
      switch (String_compare(indexNode->name,name,NULL,NULL))
      {
        case -1:
          // next
          indexNode = indexNode->next;
          break;
        case  0:
          // found
          foundIndexNode = indexNode;
          break;
        case  1:
          // not found
          indexNode = NULL;
          break;
      }
    }
    else
    {
      // not found
      indexNode = NULL;
    }
  }

  return foundIndexNode;
}

/***********************************************************************\
* Name   : serverCommand_indexEntriesList
* Purpose: get index database entry list
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <all=0|tagged storage archives=1>
*            <max. count>
*            <newestEntriesOnly 0|1>
*            <name pattern>
*          Result:
*            FILE <storage> <storage date/time> <name> <size> <time modified> \
*            <user id> <group id> <permission> <fragment offset> <fragment size>
*            IMAGE <storage> <storage date/time> <name> <size> <time modified> \
*            <block offset> <block count>
*            DIRECTORY <storage> <storage date/time> <name> <time modified> \
*            <user id> <group id> <permission>
*            LINK <storage> <storage date/time> <name> <destination name> \
*            <time modified> <user id> <group id> <permission>
*            HARDLINK <storage> <storage date/time> <name> <time modified> <user id> <group id> <permission>
*            SPECIAL <storage> <storage date/time> <name> <time modified> \
*            <user id> <group id> <permission>
\***********************************************************************/

LOCAL void serverCommand_indexEntriesList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  #define SEND_FILE_ENTRY(storageName,storageDateTime,name,size,timeModified,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "FILE %'S %llu %'S %llu %llu %u %u %u %llu %llu", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       size, \
                       timeModified, \
                       userId, \
                       groupId, \
                       permission, \
                       fragmentOffset, \
                       fragmentSize \
                      ); \
    } \
    while (0)
  #define SEND_IMAGE_ENTRY(storageName,storageDateTime,name,size,blockOffset,blockCount) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "IMAGE %'S %llu %'S %llu %ll %llu %llu", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       size, \
                       timeModified, \
                       blockOffset, \
                       blockCount \
                      ); \
    } \
    while (0)
  #define SEND_DIRECTORY_ENTRY(storageName,storageDateTime,name,timeModified,userId,groupId,permission) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "DIRECTORY %'S %llu %'S %llu %u %u %u", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       timeModified, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)
  #define SEND_LINK_ENTRY(storageName,storageDateTime,name,destinationName,timeModified,userId,groupId,permission) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string3,destinationName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "LINK %'S %llu %'S %'S %llu %u %u %u", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       string3, \
                       timeModified, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)
  #define SEND_HARDLINK_ENTRY(storageName,storageDateTime,name,size,timeModified,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "HARDLINK %'S %llu %'S %lld %llu %u %u %u %llu %llu", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       size, \
                       timeModified, \
                       userId, \
                       groupId, \
                       permission, \
                       fragmentOffset, \
                       fragmentSize \
                      ); \
    } \
    while (0)
  #define SEND_SPECIAL_ENTRY(storageName,storageDateTime,name,timeModified,userId,groupId,permission) \
    do \
    { \
      String_mapCString(String_set(string1,storageName),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      String_mapCString(String_set(string2,name),STRING_BEGIN,MAP_BIN,MAP_BIN,SIZE_OF_ARRAY(MAP_BIN)); \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "SPECIAL %'S %llu %'S %llu %u %u %u", \
                       string1, \
                       storageDateTime, \
                       string2, \
                       timeModified, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)

  const char* MAP_BIN[]  = {"\n","\r","\\"};
  const char* MAP_TEXT[] = {"\\n","\\r","\\\\"};

  bool                checkedStorageOnlyFlag;
  ulong               entryMaxCount;
  bool                newestEntriesOnlyFlag;
  String              string;
  String              pattern;
  IndexList           indexList;
  IndexNode           *indexNode;
  String              regexpString;
  DatabaseId          storageId;
  String              storageName;
  uint64              storageDateTime;
  String              name;
  String              destinationName;
  String              string1,string2,string3;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  uint64              size;
  uint64              timeModified;
  uint                userId,groupId;
  uint                permission;
  uint64              fragmentOffset,fragmentSize;
  uint64              blockOffset,blockCount;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  // get max. count, new entires only, filter pattern
  if (argumentCount < 1)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected checked storage only flag");
    return;
  }
  checkedStorageOnlyFlag = String_toBoolean(arguments[0],STRING_BEGIN,NULL,NULL,0,NULL,0);
  if (argumentCount < 2)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected max. number of entries to send");
    return;
  }
  entryMaxCount = String_toInteger64(arguments[1],STRING_BEGIN,NULL,NULL,0);
  if (argumentCount < 3)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected newest entries only flag");
    return;
  }
  newestEntriesOnlyFlag = String_toBoolean(arguments[2],STRING_BEGIN,NULL,NULL,0,NULL,0);
  if (argumentCount < 4)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter pattern");
    return;
  }
  string = arguments[3];

  if (indexDatabaseHandle != NULL)
  {
    // initialise variables
    pattern      = String_mapCString(String_duplicate(string),STRING_BEGIN,MAP_TEXT,MAP_BIN,SIZE_OF_ARRAY(MAP_TEXT));
    List_init(&indexList);
    regexpString = String_new();
    storageName  = String_new();
    name         = String_new();
    string1      = String_new();
    string2      = String_new();
    string3      = String_new();

    // collect index data
    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListFiles(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    Array_cArray(clientInfo->storageIdArray),
                                    Array_length(clientInfo->storageIdArray),
                                    pattern
                                   );
      }
      else
      {
        error = Index_initListFiles(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    NULL,
                                    0,
                                    pattern
                                   );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextFile(&databaseQueryHandle,
                                  &storageId,
                                  storageName,
                                  &storageDateTime,
                                  name,
                                  &size,
                                  &timeModified,
                                  &userId,
                                  &groupId,
                                  &permission,
                                  &fragmentOffset,
                                  &fragmentSize
                                 )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_FILE,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_FILE,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime     = storageDateTime;
            indexNode->timeModified        = timeModified;
            indexNode->file.size           = size;
            indexNode->file.userId         = userId;
            indexNode->file.groupId        = groupId;
            indexNode->file.permission     = permission;
            indexNode->file.fragmentOffset = fragmentOffset;
            indexNode->file.fragmentSize   = fragmentSize;
          }
        }
        else
        {
          SEND_FILE_ENTRY(storageName,storageDateTime,name,size,timeModified,userId,groupId,permission,fragmentOffset,fragmentSize);
        }
      }
      Index_doneList(&databaseQueryHandle);
    }

    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListImages(&databaseQueryHandle,
                                     indexDatabaseHandle,
                                     Array_cArray(clientInfo->storageIdArray),
                                     Array_length(clientInfo->storageIdArray),
                                     pattern
                                    );
      }
      else
      {
        error = Index_initListImages(&databaseQueryHandle,
                                     indexDatabaseHandle,
                                     NULL,
                                     0,
                                     pattern
                                    );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextImage(&databaseQueryHandle,
                                   &storageId,
                                   storageName,
                                   &storageDateTime,
                                   name,
                                   &size,
                                   &blockOffset,
                                   &blockCount
                                  )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_IMAGE,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_IMAGE,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime   = storageDateTime;
            indexNode->timeModified      = timeModified;
            indexNode->image.size        = size;
            indexNode->image.blockOffset = blockOffset;
            indexNode->image.blockCount  = blockCount;
          }
        }
        else
        {
          SEND_IMAGE_ENTRY(storageName,storageDateTime,name,size,blockOffset,blockCount);
        }
      }
      Index_doneList(&databaseQueryHandle);
    }

    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListDirectories(&databaseQueryHandle,
                                          indexDatabaseHandle,
                                          Array_cArray(clientInfo->storageIdArray),
                                          Array_length(clientInfo->storageIdArray),
                                          pattern
                                         );
      }
      else
      {
        error = Index_initListDirectories(&databaseQueryHandle,
                                          indexDatabaseHandle,
                                          NULL,
                                          0,
                                          pattern
                                         );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextDirectory(&databaseQueryHandle,
                                       &storageId,
                                       storageName,
                                       &storageDateTime,
                                       name,
                                       &timeModified,
                                       &userId,
                                       &groupId,
                                       &permission
                                      )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_DIRECTORY,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_DIRECTORY,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime      = storageDateTime;
            indexNode->timeModified         = timeModified;
            indexNode->directory.userId     = userId;
            indexNode->directory.groupId    = groupId;
            indexNode->directory.permission = permission;
          }
        }
        else
        {
          SEND_DIRECTORY_ENTRY(storageName,storageDateTime,name,timeModified,userId,groupId,permission);
        }
      }
      Index_doneList(&databaseQueryHandle);
    }

    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListLinks(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    Array_cArray(clientInfo->storageIdArray),
                                    Array_length(clientInfo->storageIdArray),
                                    pattern
                                   );
      }
      else
      {
        error = Index_initListLinks(&databaseQueryHandle,
                                    indexDatabaseHandle,
                                    NULL,
                                    0,
                                    pattern
                                   );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      destinationName = String_new();
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextLink(&databaseQueryHandle,
                                  &storageId,
                                  storageName,
                                  &storageDateTime,
                                  name,
                                  destinationName,
                                  &timeModified,
                                  &userId,
                                  &groupId,
                                  &permission
                                 )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_LINK,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_LINK,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime      = storageDateTime;
            indexNode->timeModified         = timeModified;
            String_set(indexNode->link.destinationName,destinationName);
            indexNode->link.userId          = userId;
            indexNode->link.groupId         = groupId;
            indexNode->link.permission      = permission;
          }
        }
        else
        {
          SEND_LINK_ENTRY(storageName,storageDateTime,name,destinationName,timeModified,userId,groupId,permission);
        }
      }
      Index_doneList(&databaseQueryHandle);
      String_delete(destinationName);
    }

    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListHardLinks(&databaseQueryHandle,
                                        indexDatabaseHandle,
                                        Array_cArray(clientInfo->storageIdArray),
                                        Array_length(clientInfo->storageIdArray),
                                        pattern
                                       );
      }
      else
      {
        error = Index_initListHardLinks(&databaseQueryHandle,
                                        indexDatabaseHandle,
                                        NULL,
                                        0,
                                        pattern
                                       );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      destinationName = String_new();
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextHardLink(&databaseQueryHandle,
                                      &storageId,
                                      storageName,
                                      &storageDateTime,
                                      name,
                                      &size,
                                      &timeModified,
                                      &userId,
                                      &groupId,
                                      &permission,
                                      &fragmentOffset,
                                      &fragmentSize
                                     )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_HARDLINK,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_HARDLINK,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime         = storageDateTime;
            indexNode->timeModified            = timeModified;
            indexNode->hardLink.size           = size;
            indexNode->hardLink.userId         = userId;
            indexNode->hardLink.groupId        = groupId;
            indexNode->hardLink.permission     = permission;
            indexNode->hardLink.fragmentOffset = fragmentOffset;
            indexNode->hardLink.fragmentSize   = fragmentSize;
          }
        }
        else
        {
          SEND_HARDLINK_ENTRY(storageName,storageDateTime,name,size,timeModified,userId,groupId,permission,fragmentOffset,fragmentSize);
        }
      }
      Index_doneList(&databaseQueryHandle);
      String_delete(destinationName);
    }

    if ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
    {
      if (checkedStorageOnlyFlag)
      {
        error = Index_initListSpecial(&databaseQueryHandle,
                                      indexDatabaseHandle,
                                      Array_cArray(clientInfo->storageIdArray),
                                      Array_length(clientInfo->storageIdArray),
                                      pattern
                                     );
      }
      else
      {
        error = Index_initListSpecial(&databaseQueryHandle,
                                      indexDatabaseHandle,
                                      NULL,
                                      0,
                                      pattern
                                     );
      }
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Errors_getText(error));
        String_delete(name);
        String_delete(storageName);
        String_delete(regexpString);
        List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
        String_delete(pattern);
        return;
      }
      while (   ((entryMaxCount == 0L) || (List_count(&indexList) < entryMaxCount))
             && !commandAborted(clientInfo,id)
             && Index_getNextSpecial(&databaseQueryHandle,
                                     &storageId,
                                     storageName,
                                     &storageDateTime,
                                     name,
                                     &timeModified,
                                     &userId,
                                     &groupId,
                                     &permission
                                    )
            )
      {
        UNUSED_VARIABLE(storageId);

        if (newestEntriesOnlyFlag)
        {
          // find/allocate index node
          indexNode = findIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_SPECIAL,name);
          if (indexNode == NULL)
          {
            indexNode = newIndexEntryNode(&indexList,ARCHIVE_ENTRY_TYPE_SPECIAL,storageName,name,timeModified);
          }
          if (indexNode == NULL) break;

          // update index node
          if (timeModified >= indexNode->timeModified)
          {
            String_set(indexNode->storageName,storageName);
            indexNode->storageDateTime    = storageDateTime;
            indexNode->timeModified       = timeModified;
            indexNode->special.userId     = userId;
            indexNode->special.groupId    = groupId;
            indexNode->special.permission = permission;
          }
        }
        else
        {
          SEND_SPECIAL_ENTRY(storageName,storageDateTime,name,timeModified,userId,groupId,permission);
        }
      }
      Index_doneList(&databaseQueryHandle);
    }

    // send data
    indexNode = indexList.head;
    while (   (indexNode != NULL)
           && !commandAborted(clientInfo,id)
          )
    {
      switch (indexNode->type)
      {
        case ARCHIVE_ENTRY_TYPE_NONE:
          break;
        case ARCHIVE_ENTRY_TYPE_FILE:
          SEND_FILE_ENTRY(indexNode->storageName,
                          indexNode->storageDateTime,
                          indexNode->name,
                          indexNode->file.size,
                          indexNode->timeModified,
                          indexNode->file.userId,
                          indexNode->file.groupId,
                          indexNode->file.permission,
                          indexNode->file.fragmentOffset,
                          indexNode->file.fragmentSize
                         );
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          SEND_IMAGE_ENTRY(indexNode->storageName,
                           indexNode->storageDateTime,
                           indexNode->name,
                           indexNode->image.size,
                           indexNode->image.blockOffset,
                           indexNode->image.blockCount
                          );
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          SEND_DIRECTORY_ENTRY(indexNode->storageName,
                               indexNode->storageDateTime,
                               indexNode->name,
                               indexNode->timeModified,
                               indexNode->directory.userId,
                               indexNode->directory.groupId,
                               indexNode->directory.permission
                              );
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          SEND_LINK_ENTRY(indexNode->storageName,
                          indexNode->storageDateTime,
                          indexNode->name,
                          indexNode->link.destinationName,
                          indexNode->timeModified,
                          indexNode->link.userId,
                          indexNode->link.groupId,
                          indexNode->link.permission
                         );
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          SEND_HARDLINK_ENTRY(indexNode->storageName,
                              indexNode->storageDateTime,
                              indexNode->name,
                              indexNode->hardLink.size,
                              indexNode->timeModified,
                              indexNode->hardLink.userId,
                              indexNode->hardLink.groupId,
                              indexNode->hardLink.permission,
                              indexNode->hardLink.fragmentOffset,
                              indexNode->hardLink.fragmentSize
                             );
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          SEND_SPECIAL_ENTRY(indexNode->storageName,
                             indexNode->storageDateTime,
                             indexNode->name,
                             indexNode->timeModified,
                             indexNode->special.userId,
                             indexNode->special.groupId,
                             indexNode->special.permission
                            );
          break;
        case ARCHIVE_ENTRY_TYPE_UNKNOWN:
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      indexNode = indexNode->next;
    }

    // free resources
    String_delete(string3);
    String_delete(string2);
    String_delete(string1);
    String_delete(name);
    String_delete(storageName);
    String_delete(regexpString);
    List_done(&indexList,(ListNodeFreeFunction)freeIndexNode,NULL);
    String_delete(pattern);

    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"no index database initialized");
  }
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : serverCommand_debugPrintInfo
* Purpose: print array/string/file debug info
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments (not used)
*          argumentCount - command arguments count (not used)
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_debugPrintStatistics(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Array_debugPrintStatistics();
  String_debugPrintStatistics();
  File_debugPrintStatistics();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugPrintMemoryInfo
* Purpose: print array/string debug memory info
* Input  : clientInfo    - client info
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

LOCAL void serverCommand_debugPrintMemoryInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Array_debugPrintInfo();
  String_debugPrintInfo();
  File_debugPrintInfo();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugDumpMemoryInfo
* Purpose: print array/string/file debug info to file
* Input  : clientInfo    - client info
*          id            - command id
*          arguments     - command arguments (not used)
*          argumentCount - command arguments count (not used)
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_debugDumpMemoryInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  FILE *handle;

  assert(clientInfo != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  handle = fopen("bar-memory.dump","w");
  if (handle == NULL)
  {
    sendClientResult(clientInfo,id,FALSE,ERROR_CREATE_FILE,"can not create 'bar-memory.dump'");
    return;
  }

  Array_debugDumpInfo(handle);
  String_debugDumpInfo(handle);
  File_debugDumpInfo(handle);

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
  { "ERROR_INFO",                 serverCommand_errorInfo,               AUTHORIZATION_STATE_OK      },
  { "AUTHORIZE",                  serverCommand_authorize,               AUTHORIZATION_STATE_WAITING },
  { "GET",                        serverCommand_get,                     AUTHORIZATION_STATE_OK      },
  { "ABORT",                      serverCommand_abort,                   AUTHORIZATION_STATE_OK      },
  { "STATUS",                     serverCommand_status,                  AUTHORIZATION_STATE_OK      },
  { "PAUSE",                      serverCommand_pause,                   AUTHORIZATION_STATE_OK      },
  { "SUSPEND",                    serverCommand_suspend,                 AUTHORIZATION_STATE_OK      },
  { "CONTINUE",                   serverCommand_continue,                AUTHORIZATION_STATE_OK      },
  { "DEVICE_LIST",                serverCommand_deviceList,              AUTHORIZATION_STATE_OK      },
  { "FILE_LIST",                  serverCommand_fileList,                AUTHORIZATION_STATE_OK      },
  { "DIRECTORY_INFO",             serverCommand_directoryInfo,           AUTHORIZATION_STATE_OK      },
  { "JOB_LIST",                   serverCommand_jobList,                 AUTHORIZATION_STATE_OK      },
  { "JOB_INFO",                   serverCommand_jobInfo,                 AUTHORIZATION_STATE_OK      },
  { "JOB_NEW",                    serverCommand_jobNew,                  AUTHORIZATION_STATE_OK      },
  { "JOB_COPY",                   serverCommand_jobCopy,                 AUTHORIZATION_STATE_OK      },
  { "JOB_RENAME",                 serverCommand_jobRename,               AUTHORIZATION_STATE_OK      },
  { "JOB_DELETE",                 serverCommand_jobDelete,               AUTHORIZATION_STATE_OK      },
  { "JOB_START",                  serverCommand_jobStart,                AUTHORIZATION_STATE_OK      },
  { "JOB_ABORT",                  serverCommand_jobAbort,                AUTHORIZATION_STATE_OK      },
  { "JOB_FLUSH",                  serverCommand_jobFlush,                AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST",               serverCommand_includeList,             AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST_CLEAR",         serverCommand_includeListClear,        AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST_ADD",           serverCommand_includeListAdd,          AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST",               serverCommand_excludeList,             AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST_CLEAR",         serverCommand_excludeListClear,        AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST_ADD",           serverCommand_excludeListAdd,          AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST",                serverCommand_sourceList,              AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST_CLEAR",          serverCommand_sourceListClear,         AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST_ADD",            serverCommand_sourceListAdd,           AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST",      serverCommand_excludeCompressList,     AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST_CLEAR",serverCommand_excludeCompressListClear,AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST_ADD",  serverCommand_excludeCompressListAdd,  AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_LIST",              serverCommand_scheduleList,            AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_LIST_CLEAR",        serverCommand_scheduleListClear,       AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_LIST_ADD",          serverCommand_scheduleListAdd,         AUTHORIZATION_STATE_OK      },
  { "OPTION_GET",                 serverCommand_optionGet,               AUTHORIZATION_STATE_OK      },
  { "OPTION_SET",                 serverCommand_optionSet,               AUTHORIZATION_STATE_OK      },
  { "OPTION_DELETE",              serverCommand_optionDelete,            AUTHORIZATION_STATE_OK      },
  { "DECRYPT_PASSWORD_CLEAR",     serverCommand_decryptPasswordsClear,   AUTHORIZATION_STATE_OK      },
  { "DECRYPT_PASSWORD_ADD",       serverCommand_decryptPasswordAdd,      AUTHORIZATION_STATE_OK      },
  { "FTP_PASSWORD",               serverCommand_ftpPassword,             AUTHORIZATION_STATE_OK      },
  { "SSH_PASSWORD",               serverCommand_sshPassword,             AUTHORIZATION_STATE_OK      },
  { "CRYPT_PASSWORD",             serverCommand_cryptPassword,           AUTHORIZATION_STATE_OK      },
  { "PASSWORDS_CLEAR",            serverCommand_passwordsClear,          AUTHORIZATION_STATE_OK      },
  { "VOLUME_LOAD",                serverCommand_volumeLoad,              AUTHORIZATION_STATE_OK      },
  { "VOLUME_UNLOAD",              serverCommand_volumeUnload,            AUTHORIZATION_STATE_OK      },

  { "ARCHIVE_LIST",               serverCommand_archiveList,             AUTHORIZATION_STATE_OK      },

  { "STORAGE_LIST_CLEAR",         serverCommand_storageListClear,        AUTHORIZATION_STATE_OK      },
  { "STORAGE_LIST_ADD",           serverCommand_storageListAdd,          AUTHORIZATION_STATE_OK      },

  { "RESTORE",                    serverCommand_restore,                 AUTHORIZATION_STATE_OK      },

  { "INDEX_STORAGE_INFO",         serverCommand_indexStorageInfo,        AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_LIST",         serverCommand_indexStorageList,        AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_ADD",          serverCommand_indexStorageAdd,         AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_REMOVE",       serverCommand_indexStorageRemove,      AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_REFRESH",      serverCommand_indexStorageRefresh,     AUTHORIZATION_STATE_OK      },

  { "INDEX_ENTRIES_LIST",         serverCommand_indexEntriesList,        AUTHORIZATION_STATE_OK      },

  #ifndef NDEBUG
  { "DEBUG_PRINT_STATISTICS",     serverCommand_debugPrintStatistics,    AUTHORIZATION_STATE_OK      },
  { "DEBUG_PRINT_MEMORY_INFO",    serverCommand_debugPrintMemoryInfo,    AUTHORIZATION_STATE_OK      },
  { "DEBUG_DUMP_MEMORY_INFO",     serverCommand_debugDumpMemoryInfo,     AUTHORIZATION_STATE_OK      },
  #endif /* NDEBUG */
};

/***********************************************************************\
* Name   : freeArgumentsArrayElement
* Purpose: free argument array element
* Input  : string   - array element to free
*          userData - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArgumentsArrayElement(String *string, void *userData)
{
  assert(string != NULL);

  UNUSED_VARIABLE(userData);

  String_erase(*string);
  String_delete(*string);
}

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

  Array_delete(commandMsg->arguments,(ArrayElementFreeFunction)freeArgumentsArrayElement,NULL);
}

/***********************************************************************\
* Name   : parseCommand
* Purpose: parse command
* Input  : string - command
* Output : commandMsg - command message
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool parseCommand(CommandMsg *commandMsg,
                        String      string
                       )
{
  StringTokenizer stringTokenizer;
  String          token;
  uint            z;
  long            index;
  String          argument;

  assert(commandMsg != NULL);

  // initialize variables
  commandMsg->serverCommandFunction = 0;
  commandMsg->authorizationState    = 0;
  commandMsg->id                    = 0;
  commandMsg->arguments             = NULL;

  // initialize tokenizer
  String_initTokenizer(&stringTokenizer,string,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,TRUE);

  // get command id
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  commandMsg->id = String_toInteger(token,0,&index,NULL,0);
  if (index != STRING_END)
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }

  // get command
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  z = 0;
  while ((z < SIZE_OF_ARRAY(SERVER_COMMANDS)) && !String_equalsCString(token,SERVER_COMMANDS[z].name))
  {
    z++;
  }
  if (z >= SIZE_OF_ARRAY(SERVER_COMMANDS))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  commandMsg->serverCommandFunction = SERVER_COMMANDS[z].serverCommandFunction;
  commandMsg->authorizationState    = SERVER_COMMANDS[z].authorizationState;

  // get arguments
  commandMsg->arguments = Array_new(sizeof(String),0);
  if (commandMsg->arguments == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    argument = String_duplicate(token);
    Array_append(commandMsg->arguments,&argument);
  }

  // free resources
  String_doneTokenizer(&stringTokenizer);

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
  CommandMsg commandMsg;
  String     result;

  assert(clientInfo != NULL);

  result = String_new();
  while (   !clientInfo->network.quitFlag
         && MsgQueue_get(&clientInfo->network.commandMsgQueue,&commandMsg,NULL,sizeof(commandMsg))
        )
  {
    // check authorization (if not in server debug mode)
    if (globalOptions.serverDebugFlag || (clientInfo->authorizationState == commandMsg.authorizationState))
    {
      // execute command
      commandMsg.serverCommandFunction(clientInfo,
                                       commandMsg.id,
                                       Array_cArray(commandMsg.arguments),
                                       Array_length(commandMsg.arguments)
                                      );
    }
    else
    {
      // authorization failure -> mark for disconnect
      sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
    }

    // free resources
    freeCommandMsg(&commandMsg,NULL);
  }
  String_delete(result);
}

/***********************************************************************\
* Name   : getNewSessionId
* Purpose: get new session id
* Input  : -
* Output : sessionId - new session id
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getNewSessionId(SessionId sessionId)
{
  #ifndef NO_SESSION_ID
    Crypt_randomize(sessionId,sizeof(SessionId));
  #else /* not NO_SESSION_ID */
    memset(sessionId,0,sizeof(SessionId));
  #endif /* NO_SESSION_ID */
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
  String s;
  uint   z;

  s = String_new();
  String_appendCString(s,"SESSION ");
  for (z = 0; z < sizeof(SessionId); z++)
  {
    String_format(s,"%02x",clientInfo->sessionId[z]);
  }
  String_appendChar(s,'\n');
  sendClient(clientInfo,s);
  String_delete(s);
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
                           FileHandle fileHandle
                          )
{
  assert(clientInfo != NULL);

  // initialize
  clientInfo->type               = CLIENT_TYPE_BATCH;
//  clientInfo->authorizationState   = AUTHORIZATION_STATE_WAITING;
clientInfo->authorizationState   = AUTHORIZATION_STATE_OK;
  clientInfo->abortCommandId     = 0;
  clientInfo->file.fileHandle    = fileHandle;

  EntryList_init(&clientInfo->includeEntryList);
  PatternList_init(&clientInfo->excludePatternList);
  PatternList_init(&clientInfo->compressExcludePatternList);
  initJobOptions(&clientInfo->jobOptions);
  List_init(&clientInfo->directoryInfoList);
  clientInfo->storageIdArray = Array_new(sizeof(DatabaseId),64);
  if (clientInfo->storageIdArray == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // create and send session id
// authorization?
//  getNewSessionId(clientInfo->sessionId);
//  sendSessionId(clientInfo);
}

/***********************************************************************\
* Name   : newNetworkClient
* Purpose: create new client with network i/o
* Input  : clientInfo   - client info to initialize
*          name         - client name
*          port         - client port
*          socketHandle - client socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initNetworkClient(ClientInfo   *clientInfo,
                             const String name,
                             uint         port,
                             SocketHandle socketHandle
                            )
{
  uint z;

  assert(clientInfo != NULL);

  // initialize
  clientInfo->type                 = CLIENT_TYPE_NETWORK;
  clientInfo->authorizationState   = AUTHORIZATION_STATE_WAITING;
  clientInfo->abortCommandId       = 0;
  clientInfo->network.name         = String_duplicate(name);
  clientInfo->network.port         = port;
  clientInfo->network.socketHandle = socketHandle;
  clientInfo->network.quitFlag     = FALSE;
  EntryList_init(&clientInfo->includeEntryList);
  PatternList_init(&clientInfo->excludePatternList);
  PatternList_init(&clientInfo->compressExcludePatternList);
  initJobOptions(&clientInfo->jobOptions);
  List_init(&clientInfo->directoryInfoList);
  clientInfo->storageIdArray = Array_new(sizeof(DatabaseId),64);
  if (clientInfo->storageIdArray == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  if (!MsgQueue_init(&clientInfo->network.commandMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise client command message queue!");
  }
  Semaphore_init(&clientInfo->network.writeLock);
  for (z = 0; z < MAX_NETWORK_CLIENT_THREADS; z++)
  {
    if (!Thread_init(&clientInfo->network.threads[z],"Client",0,networkClientThreadCode,clientInfo))
    {
      HALT_FATAL_ERROR("Cannot initialise client thread!");
    }
  }

  // create and send session id
  getNewSessionId(clientInfo->sessionId);
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

  switch (clientInfo->type)
  {
    case CLIENT_TYPE_BATCH:
      break;
    case CLIENT_TYPE_NETWORK:
      // stop client threads
      clientInfo->network.quitFlag = TRUE;
      MsgQueue_setEndOfMsg(&clientInfo->network.commandMsgQueue);
      for (z = MAX_NETWORK_CLIENT_THREADS-1; z >= 0; z--)
      {
        Thread_join(&clientInfo->network.threads[z]);
      }

      // free resources
      MsgQueue_done(&clientInfo->network.commandMsgQueue,(MsgQueueMsgFreeFunction)freeCommandMsg,NULL);
      String_delete(clientInfo->network.name);
      for (z = MAX_NETWORK_CLIENT_THREADS-1; z >= 0; z--)
      {
        Thread_done(&clientInfo->network.threads[z]);
      }
      Semaphore_done(&clientInfo->network.writeLock);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  Array_delete(clientInfo->storageIdArray,NULL,NULL);
  List_done(&clientInfo->directoryInfoList,(ListNodeFreeFunction)freeDirectoryInfoNode,NULL);
  freeJobOptions(&clientInfo->jobOptions);
  PatternList_done(&clientInfo->compressExcludePatternList);
  PatternList_done(&clientInfo->excludePatternList);
  EntryList_done(&clientInfo->includeEntryList);
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
* Input  : type - client type
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

  // initialize node
  clientNode->clientInfo.type               = CLIENT_TYPE_NONE;
  clientNode->clientInfo.authorizationState = AUTHORIZATION_STATE_WAITING;
  clientNode->commandString                 = String_new();

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
* Name   : processCommand
* Purpose: process client command
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processCommand(ClientInfo *clientInfo, const String command)
{
  CommandMsg commandMsg;

  assert(clientInfo != NULL);

  #ifdef SERVER_DEBUG
    fprintf(stderr,"%s,%d: command=%s\n",__FILE__,__LINE__,String_cString(command));
  #endif // SERVER_DEBUG
  if (String_equalsCString(command,"VERSION"))
  {
    // check authorization (if not in server debug mode)
    if (globalOptions.serverDebugFlag || (clientInfo->authorizationState == AUTHORIZATION_STATE_OK))
    {
      // version info
      sendClientResult(clientInfo,0,TRUE,ERROR_NONE,"%d %d",PROTOCOL_VERSION_MAJOR,PROTOCOL_VERSION_MINOR);
    }
    else
    {
      // authorization failure -> mark for disconnect
      sendClientResult(clientInfo,0,TRUE,ERROR_AUTHORIZATION,"authorization failure");
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
    }
  }
  else if (globalOptions.serverDebugFlag && String_equalsCString(command,"QUIT"))
  {
    quitFlag = TRUE;
    sendClientResult(clientInfo,0,TRUE,ERROR_NONE,"ok");
  }
  else
  {
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
        if (globalOptions.serverDebugFlag || (clientInfo->authorizationState == commandMsg.authorizationState))
        {
          // execute
          commandMsg.serverCommandFunction(clientInfo,
                                           commandMsg.id,
                                           Array_cArray(commandMsg.arguments),
                                           Array_length(commandMsg.arguments)
                                          );
        }
        else
        {
          // authorization failure -> mark for disconnect
          sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
          clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
        }

        // free resources
        freeCommandMsg(&commandMsg,NULL);
        break;
      case CLIENT_TYPE_NETWORK:
        // send command to client thread
        MsgQueue_put(&clientInfo->network.commandMsgQueue,&commandMsg,sizeof(commandMsg));
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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

Errors Server_run(uint             port,
                  uint             tlsPort,
                  const char       *caFileName,
                  const char       *certFileName,
                  const char       *keyFileName,
                  const Password   *password,
                  const char       *jobsDirectory,
                  const JobOptions *defaultJobOptions
                 )
{
  Errors             error;
  bool               serverFlag,serverTLSFlag;
  ServerSocketHandle serverSocketHandle,serverTLSSocketHandle;
  fd_set             selectSet;
  ClientNode         *clientNode;
  SocketHandle       socketHandle;
  String             clientName;
  uint               clientPort;
  char               buffer[256];
  ulong              receivedBytes;
  ulong              z;
  ClientNode         *deleteClientNode;

  // initialise variables
  serverPassword          = password;
  serverJobsDirectory     = jobsDirectory;
  serverDefaultJobOptions = defaultJobOptions;
  List_init(&jobList);
  Semaphore_init(&jobList.lock);
  Semaphore_init(&serverStateLock);
  List_init(&clientList);
  serverState             = SERVER_STATE_RUNNING;
  createFlag              = FALSE;
  restoreFlag             = FALSE;
  pauseFlags.create       = FALSE;
  pauseFlags.restore      = FALSE;
  pauseFlags.indexUpdate  = FALSE;
  pauseEndTimestamp       = 0LL;
  indexFlag               = FALSE;
  quitFlag                = FALSE;

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
                 Errors_getText(error)
                );
      return error;
    }
  }
  if (!File_isDirectoryCString(serverJobsDirectory))
  {
    printError("'%s' is not a directory!\n",serverJobsDirectory);
    return ERROR_NOT_A_DIRECTORY;
  }

  // init server sockets
  serverFlag    = FALSE;
  serverTLSFlag = FALSE;
  if (port != 0)
  {
    error = Network_initServer(&serverSocketHandle,
                               port,
                               SERVER_TYPE_PLAIN,
                               NULL,
                               NULL,
                               NULL
                              );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize server at port %u (error: %s)!\n",
                 port,
                 Errors_getText(error)
                );
      return error;
    }
    printInfo(1,"Started server on port %d\n",port);
    serverFlag = TRUE;
  }
  if (tlsPort != 0)
  {
    if (   File_existsCString(caFileName)
        && File_existsCString(certFileName)
        && File_existsCString(keyFileName)
       )
    {
      #ifdef HAVE_GNU_TLS
        error = Network_initServer(&serverTLSSocketHandle,
                                   tlsPort,
                                   SERVER_TYPE_TLS,
                                   caFileName,
                                   certFileName,
                                   keyFileName
                                  );
        if (error != ERROR_NONE)
        {
          printError("Cannot initialize TLS/SSL server at port %u (error: %s)!\n",
                     tlsPort,
                     Errors_getText(error)
                    );
          if (port != 0) Network_doneServer(&serverSocketHandle);
          return FALSE;
        }
        printInfo(1,"Started TLS/SSL server on port %u\n",tlsPort);
        serverTLSFlag = TRUE;
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(caFileName);
        UNUSED_VARIABLE(certFileName);
        UNUSED_VARIABLE(keyFileName);

        printError("TLS/SSL server is not supported!\n");
        Network_doneServer(&serverSocketHandle);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GNU_TLS */
    }
    else
    {
      if (!File_existsCString(caFileName)) printWarning("No certificate authority file '%s' (bar-ca.pem file) - TLS server not started.\n",caFileName);
      if (!File_existsCString(certFileName)) printWarning("No certificate file '%s' (bar-server-cert.pem file) - TLS server not started.\n",certFileName);
      if (!File_existsCString(keyFileName)) printWarning("No key file '%s' (bar-server-key.pem file) - TLS server not started.\n",keyFileName);
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
    return ERROR_INVALID_ARGUMENT;
  }
  if (Password_empty(password))
  {
    printWarning("No server password set!\n");
  }

  // start threads
  if (!Thread_init(&jobThread,"BAR job",globalOptions.niceLevel,jobThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialise job thread!");
  }
  if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialise scheduler thread!");
  }
  if (!Thread_init(&pauseThread,"BAR pause",globalOptions.niceLevel,pauseThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialise pause thread!");
  }
  if (indexDatabaseHandle != NULL)
  {
    if (!Thread_init(&indexThread,"BAR index",globalOptions.niceLevel,indexThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialise index thread!");
    }
    if (!globalOptions.indexDatabaseNoAutoUpdateFlag)
    {
      if (!Thread_init(&indexUpdateThread,"BAR update index",globalOptions.niceLevel,indexUpdateThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialise index update thread!");
      }
    }
  }

  // run server
  if (globalOptions.serverDebugFlag)
  {
    printWarning("Server is running in debug mode. No authorization is done and additional debug commands are enabled!\n");
  }
  clientName = String_new();
  while (!quitFlag)
  {
    // wait for command
    FD_ZERO(&selectSet);
    if (serverFlag   ) FD_SET(Network_getServerSocket(&serverSocketHandle),   &selectSet);
    if (serverTLSFlag) FD_SET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet);
    LIST_ITERATE(&clientList,clientNode)
    {
      FD_SET(Network_getSocket(&clientNode->clientInfo.network.socketHandle),&selectSet);
    }
    select(FD_SETSIZE,&selectSet,NULL,NULL,NULL);

    // connect new clients
    if (serverFlag && FD_ISSET(Network_getServerSocket(&serverSocketHandle),&selectSet))
    {
      error = Network_accept(&socketHandle,
                             &serverSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,clientName,&clientPort);
        clientNode = newClient();
        assert(clientNode != NULL);
        initNetworkClient(&clientNode->clientInfo,clientName,clientPort,socketHandle);
        List_append(&clientList,clientNode);

        printInfo(1,"Connected client '%s:%u'\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
      }
      else
      {
        printError("Cannot establish client connection (error: %s)!\n",
                   Errors_getText(error)
                  );
      }
    }
    if (serverTLSFlag && FD_ISSET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet))
    {
      error = Network_accept(&socketHandle,
                             &serverTLSSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,clientName,&clientPort);
        clientNode = newClient();
        assert(clientNode != NULL);
        initNetworkClient(&clientNode->clientInfo,clientName,clientPort,socketHandle);
        List_append(&clientList,clientNode);

        printInfo(1,"Connected client '%s:%u' (TLS/SSL)\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
      }
      else
      {
        printError("Cannot establish client TLS connection (error: %s)!\n",
                   Errors_getText(error)
                  );
      }
    }

    // process client commands/disconnect clients
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (FD_ISSET(Network_getSocket(&clientNode->clientInfo.network.socketHandle),&selectSet))
      {
        Network_receive(&clientNode->clientInfo.network.socketHandle,buffer,sizeof(buffer),WAIT_FOREVER,&receivedBytes);
        if (receivedBytes > 0)
        {
          // received data -> process
          do
          {
            for (z = 0; z < receivedBytes; z++)
            {
              if (buffer[z] != '\n')
              {
                String_appendChar(clientNode->commandString,buffer[z]);
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

          clientNode = clientNode->next;
        }
        else
        {
          // disconnect
          printInfo(1,"Disconnected client '%s:%u'\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);

          deleteClientNode = clientNode;
          clientNode = clientNode->next;
          List_remove(&clientList,deleteClientNode);

          Network_disconnect(&deleteClientNode->clientInfo.network.socketHandle);
          deleteClient(deleteClientNode);
        }
      }
      else
      {
        clientNode = clientNode->next;
      }
    }

    // disconnect clients because of authorization failure
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (clientNode->clientInfo.authorizationState == AUTHORIZATION_STATE_FAIL)
      {
        // authorizaton error -> disconnect  client
        printInfo(1,"Disconnected client '%s:%u': authorization failure\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);

        deleteClientNode = clientNode;
        clientNode = clientNode->next;
        List_remove(&clientList,deleteClientNode);

        Network_disconnect(&deleteClientNode->clientInfo.network.socketHandle);
        deleteClient(deleteClientNode);
      }
      else
      {
        // next client
        clientNode = clientNode->next;
      }
    }
  }
  String_delete(clientName);

  // disconnect all clients
  while (!List_isEmpty(&clientList))
  {
    clientNode = (ClientNode*)List_getFirst(&clientList);

    Network_disconnect(&clientNode->clientInfo.network.socketHandle);
    deleteClient(clientNode);
  }

  // wait for thread exit
  Semaphore_setEnd(&jobList.lock);
  if (indexDatabaseHandle != NULL)
  {
    if (!globalOptions.indexDatabaseNoAutoUpdateFlag)
    {
      Thread_join(&indexUpdateThread);
    }
    Thread_join(&indexThread);
  }
  Thread_join(&pauseThread);
  Thread_join(&schedulerThread);
  Thread_join(&jobThread);

  // done server
  if (serverFlag   ) Network_doneServer(&serverSocketHandle);
  if (serverTLSFlag) Network_doneServer(&serverTLSSocketHandle);

  // free resources
  if (indexDatabaseHandle != NULL)
  {
    if (!globalOptions.indexDatabaseNoAutoUpdateFlag)
    {
      Thread_done(&indexUpdateThread);
    }
    Thread_done(&indexThread);
  }
  Thread_done(&pauseThread);
  Thread_done(&schedulerThread);
  Thread_done(&jobThread);
  Semaphore_done(&serverStateLock);
  List_done(&clientList,(ListNodeFreeFunction)freeClientNode,NULL);
  Semaphore_done(&jobList.lock);
  List_done(&jobList,(ListNodeFreeFunction)freeJobNode,NULL);

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
  error = File_openDescriptor(&inputFileHandle,inputDescriptor,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,
            "Cannot initialize input: %s!\n",
            Errors_getText(error)
           );
    return error;
  }
  error = File_openDescriptor(&outputFileHandle,outputDescriptor,FILE_OPEN_WRITE);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,
            "Cannot initialize output: %s!\n",
            Errors_getText(error)
           );
    File_close(&inputFileHandle);
    return error;
  }

  // init client
  initBatchClient(&clientInfo,outputFileHandle);

  // send info
  File_printLine(&outputFileHandle,
                 "BAR VERSION %d %d\n",
                 PROTOCOL_VERSION_MAJOR,PROTOCOL_VERSION_MINOR
                );
  File_flush(&outputFileHandle);

  // run server
  commandString = String_new();
#if 1
  while (!quitFlag && !File_eof(&inputFileHandle))
  {
    // read command line
    File_readLine(&inputFileHandle,commandString);

    // process
    processCommand(&clientInfo,commandString);
  }
#else /* 0 */
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
String_setCString(commandString,"1 SET crypt-password 'muster'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN REGEX test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */
  String_delete(commandString);

  // free resources
  doneClient(&clientInfo);
  File_close(&outputFileHandle);
  File_close(&inputFileHandle);
  List_done(&jobList,(ListNodeFreeFunction)freeJobNode,NULL);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
