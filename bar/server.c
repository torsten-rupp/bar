/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.c,v $
* $Revision: 1.40 $
* $Author: torsten $
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

#include "bar.h"
#include "passwords.h"
#include "network.h"
#include "files.h"
#include "patterns.h"
#include "archive.h"
#include "storage.h"
#include "misc.h"

#include "commands_create.h"
#include "commands_restore.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define _SERVER_DEBUG
#define _NO_SESSION_ID
#define _SIMULATOR

/***************************** Constants *******************************/

#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

#define SESSION_ID_LENGTH 64                   // max. length of session id

/***************************** Datatypes *******************************/

/* job states */
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

typedef struct JobNode
{
  LIST_NODE_HEADER(struct JobNode);

  String       fileName;                       // file name
  uint64       timeModified;                   // file modified date/time (timestamp)

  /* job config */
  JobTypes     type;                           // job type: backup, restore
  String       name;                           // name of job
  String       archiveName;                    // archive name
  PatternList  includePatternList;             // included files
  PatternList  excludePatternList;             // excluded files
  ScheduleList scheduleList;                   // schedule list
  JobOptions   jobOptions;                     // options for job
  bool         modifiedFlag;                   // TRUE iff job config modified

  /* schedule info */
  uint64       lastExecutedDateTime;           // last execution date/time (timestamp)
  uint64       lastCheckDateTime;              // last check date/time (timestamp)

  /* job info */
  uint         id;                             // unique job id
  JobStates    state;                          // current state of job
  ArchiveTypes archiveType;                    // archive type to create
  bool         requestedAbortFlag;             // request abort
  uint         requestedVolumeNumber;          // requested volume number
  uint         volumeNumber;                   // loaded volume number

  /* running info */
  struct
  {
    Errors     error;                          // error code
    uint64     startDateTime;                  // start time (timestamp)
    ulong      estimatedRestTime;              // estimated rest running time [s]
    ulong      doneFiles;                      // number of processed files
    uint64     doneBytes;                      // sum of processed bytes
    ulong      totalFiles;                     // number of total files
    uint64     totalBytes;                     // sum of total bytes
    ulong      skippedFiles;                   // number of skipped files
    uint64     skippedBytes;                   // sum of skippped bytes
    ulong      errorFiles;                     // number of files with errors
    uint64     errorBytes;                     // sum of bytes of files with errors
    double     filesPerSecond;                 // average processed files per second
    double     bytesPerSecond;                 // average processed bytes per second
    double     storageBytesPerSecond;          // average processed storage bytes per second
    uint64     archiveBytes;                   // number of bytes stored in archive
    double     compressionRatio;
    String     fileName;                       // current file
    uint64     fileDoneBytes;                  // current file bytes done
    uint64     fileTotalBytes;                 // current file bytes total
    String     storageName;                    // current storage file
    uint64     storageDoneBytes;               // current storage file bytes done
    uint64     storageTotalBytes;              // current storage file bytes total
    uint       volumeNumber;                   // current volume number
    double     volumeProgress;                 // current volume progress
  } runningInfo;
} JobNode;

// list with enqueued jobs
typedef struct
{
  LIST_HEADER(JobNode);

  Semaphore lock;
  uint      lastJobId;
} JobList;

/* session id */
typedef byte SessionId[SESSION_ID_LENGTH];

/* client types */
typedef enum
{
  CLIENT_TYPE_NONE,
  CLIENT_TYPE_FILE,
  CLIENT_TYPE_NETWORK,
} ClientTypes;

/* authorization states */
typedef enum
{
  AUTHORIZATION_STATE_WAITING,
  AUTHORIZATION_STATE_OK,
  AUTHORIZATION_STATE_FAIL,
} AuthorizationStates;

/* client info */
typedef struct
{
  ClientTypes         type;

  SessionId           sessionId;
  AuthorizationStates authorizationState;

  union
  {
    /* i/o via file */
    struct
    {
      FileHandle   fileHandle;
    } file;

    /* i/o via network and separated processing thread */
    struct
    {
      /* connection */
      String       name;
      uint         port;
      SocketHandle socketHandle;

      /* thread */
      Thread       thread;
      MsgQueue     commandMsgQueue;
      bool         exitFlag;
    } network;
  };

  PatternList         includePatternList;
  PatternList         excludePatternList;
  JobOptions          jobOptions;
} ClientInfo;

/* client node */
typedef struct ClientNode
{
  LIST_NODE_HEADER(struct ClientNode);

  ClientInfo clientInfo;
  String     commandString;
} ClientNode;

typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

/* server command function */
typedef void(*ServerCommandFunction)(ClientInfo    *clientInfo,
                                     uint          id,
                                     const String  arguments[],
                                     uint          argumentCount
                                    );

/* server command message */
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
  {"normal",     ARCHIVE_TYPE_NORMAL,    },
  {"full",       ARCHIVE_TYPE_FULL,      },
  {"incremental",ARCHIVE_TYPE_INCREMENTAL},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PATTERN_TYPES[] =
{
  {"glob",    PATTERN_TYPE_GLOB,         },
  {"regex",   PATTERN_TYPE_REGEX,        },
  {"extended",PATTERN_TYPE_EXTENDED_REGEX},
};

LOCAL const ConfigValueSelect CONFIG_VALUE_COMPRESS_ALGORITHMS[] =
{
  {"none", COMPRESS_ALGORITHM_NONE,  },

  {"zip0", COMPRESS_ALGORITHM_ZIP_0, },
  {"zip1", COMPRESS_ALGORITHM_ZIP_1, },
  {"zip2", COMPRESS_ALGORITHM_ZIP_2, },
  {"zip3", COMPRESS_ALGORITHM_ZIP_3, },
  {"zip4", COMPRESS_ALGORITHM_ZIP_4, },
  {"zip5", COMPRESS_ALGORITHM_ZIP_5, },
  {"zip6", COMPRESS_ALGORITHM_ZIP_6, },
  {"zip7", COMPRESS_ALGORITHM_ZIP_7, },
  {"zip8", COMPRESS_ALGORITHM_ZIP_8, },
  {"zip9", COMPRESS_ALGORITHM_ZIP_9, },

  #ifdef HAVE_BZ2
    {"bzip1",COMPRESS_ALGORITHM_BZIP2_1},
    {"bzip2",COMPRESS_ALGORITHM_BZIP2_2},
    {"bzip3",COMPRESS_ALGORITHM_BZIP2_3},
    {"bzip4",COMPRESS_ALGORITHM_BZIP2_4},
    {"bzip5",COMPRESS_ALGORITHM_BZIP2_5},
    {"bzip6",COMPRESS_ALGORITHM_BZIP2_6},
    {"bzip7",COMPRESS_ALGORITHM_BZIP2_7},
    {"bzip8",COMPRESS_ALGORITHM_BZIP2_8},
    {"bzip9",COMPRESS_ALGORITHM_BZIP2_9},
  #endif /* HAVE_BZ2 */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE,     },

  #ifdef HAVE_GCRYPT
    {"3DES",      CRYPT_ALGORITHM_3DES,     },
    {"CAST5",     CRYPT_ALGORITHM_CAST5,    },
    {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH, },
    {"AES128",    CRYPT_ALGORITHM_AES128,   },
    {"AES192",    CRYPT_ALGORITHM_AES192,   },
    {"AES256",    CRYPT_ALGORITHM_AES256,   },
    {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128},
    {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256},
  #endif /* HAVE_GCRYPT */
};

LOCAL const ConfigValueSelect CONFIG_VALUE_PASSWORD_MODES[] =
{
  {"default",   PASSWORD_MODE_DEFAULT,    },
  {"ask",       PASSWORD_MODE_ASK,        },
  {"config",    PASSWORD_MODE_CONFIG,     },
};

LOCAL const ConfigValue CONFIG_VALUES[] =
{
  CONFIG_STRUCT_VALUE_STRING   ("archive-name",           JobNode,archiveName                            ),
  CONFIG_STRUCT_VALUE_SELECT   ("archive-type",           JobNode,jobOptions.archiveType,                CONFIG_VALUE_ARCHIVE_TYPES),
  CONFIG_STRUCT_VALUE_INTEGER64("archive-part-size",      JobNode,jobOptions.archivePartSize,            0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_STRUCT_VALUE_STRING   ("incremental-list-file",  JobNode,jobOptions.incrementalListFileName     ),

  CONFIG_STRUCT_VALUE_INTEGER  ("directory-strip",        JobNode,jobOptions.directoryStripCount,        0,INT_MAX,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("directory",              JobNode,jobOptions.directory                   ),

  CONFIG_STRUCT_VALUE_SELECT   ("pattern-type",           JobNode,jobOptions.patternType,                CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_STRUCT_VALUE_SELECT   ("compress-algorithm",     JobNode,jobOptions.compressAlgorithm,          CONFIG_VALUE_COMPRESS_ALGORITHMS),

  CONFIG_STRUCT_VALUE_SELECT   ("crypt-algorithm",        JobNode,jobOptions.cryptAlgorithm,             CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_STRUCT_VALUE_SELECT   ("crypt-password-mode",    JobNode,jobOptions.cryptPasswordMode,          CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_STRUCT_VALUE_SPECIAL  ("crypt-password",         JobNode,jobOptions.cryptPassword,              configValueParsePassword,NULL,NULL,configValueFormatPassword,NULL),

  CONFIG_STRUCT_VALUE_STRING   ("ftp-login-name",         JobNode,jobOptions.ftpServer.loginName         ),
  CONFIG_STRUCT_VALUE_SPECIAL  ("ftp-password",           JobNode,jobOptions.ftpServer.password,         configValueParsePassword,NULL,NULL,configValueFormatPassword,NULL),

  CONFIG_STRUCT_VALUE_INTEGER  ("ssh-port",               JobNode,jobOptions.sshServer.port,             0,65535,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-login-name",         JobNode,jobOptions.sshServer.loginName         ),
  CONFIG_STRUCT_VALUE_SPECIAL  ("ssh-password",           JobNode,jobOptions.sshServer.password ,        configValueParsePassword,NULL,NULL,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-public-key",         JobNode,jobOptions.sshServer.publicKeyFileName ),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-private-key",        JobNode,jobOptions.sshServer.privateKeyFileName),

  CONFIG_STRUCT_VALUE_INTEGER64("volume-size",            JobNode,jobOptions.device.volumeSize,          0LL,LONG_LONG_MAX,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_STRUCT_VALUE_BOOLEAN  ("skip-unreadable",        JobNode,jobOptions.skipUnreadableFlag          ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("overwrite-archive-files",JobNode,jobOptions.overwriteArchiveFilesFlag   ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("overwrite-files",        JobNode,jobOptions.overwriteFilesFlag          ),
  CONFIG_STRUCT_VALUE_BOOLEAN  ("ecc",                    JobNode,jobOptions.errorCorrectionCodesFlag    ),

  CONFIG_STRUCT_VALUE_SPECIAL  ("include",                JobNode,includePatternList,                    configValueParseIncludeExclude,configValueFormatInitIncludeExclude,configValueFormatDoneIncludeExclude,configValueFormatIncludeExclude,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL  ("exclude",                JobNode,excludePatternList,                    configValueParseIncludeExclude,configValueFormatInitIncludeExclude,configValueFormatDoneIncludeExclude,configValueFormatIncludeExclude,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL  ("schedule",               JobNode,scheduleList,                          configValueParseSchedule,configValueFormatInitSchedule,configValueFormatDoneSchedule,configValueFormatSchedule,NULL),
};

/***************************** Variables *******************************/
LOCAL const Password   *serverPassword;
LOCAL const char       *serverJobDirectory;
LOCAL const JobOptions *serverDefaultJobOptions;
LOCAL JobList          jobList;
LOCAL Thread           jobThread;
LOCAL Thread           schedulerThread;
LOCAL ClientList       clientList;
LOCAL bool             pauseFlag;
LOCAL bool             quitFlag;

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
* Return : name
* Notes  : -
\***********************************************************************/

LOCAL const char *getArchiveTypeName(ArchiveTypes archiveType)
{
  int z;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CONFIG_VALUE_ARCHIVE_TYPES))
         && (archiveType != CONFIG_VALUE_ARCHIVE_TYPES[z].value)
        )
  {
    z++;
  }

  return (z < SIZE_OF_ARRAY(CONFIG_VALUE_ARCHIVE_TYPES))?CONFIG_VALUE_ARCHIVE_TYPES[z].name:"";
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
* Name   : newJob
* Purpose: create new job
* Input  : jobType  - job type
*          fileName - file name or NULL
*          name     - name of job
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *newJob(JobTypes     jobType,
                      const String fileName
                     )
{
  JobNode *jobNode;

  /* allocate job node */
  jobNode = LIST_NEW_NODE(JobNode);
  if (jobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init job node */
  jobNode->fileName                          = String_duplicate(fileName);
  jobNode->timeModified                      = 0LL;

  jobNode->type                              = jobType;
  jobNode->name                              = File_getFileBaseName(File_newFileName(),fileName);
  jobNode->archiveName                       = String_new();
  Pattern_initList(&jobNode->includePatternList);
  Pattern_initList(&jobNode->excludePatternList);
  List_init(&jobNode->scheduleList);
  initJobOptions(&jobNode->jobOptions);
  jobNode->modifiedFlag                      = FALSE;

  jobNode->lastExecutedDateTime              = 0LL;
  jobNode->lastCheckDateTime                 = 0LL;

  jobNode->id                                = getNewJobId();
  jobNode->state                             = JOB_STATE_NONE;
  jobNode->archiveType                       = ARCHIVE_TYPE_NORMAL;
  jobNode->requestedAbortFlag                = FALSE;
  jobNode->requestedVolumeNumber             = 0;
  jobNode->volumeNumber                      = 0;

  jobNode->runningInfo.error                 = ERROR_NONE;
  jobNode->runningInfo.startDateTime         = 0LL;
  jobNode->runningInfo.estimatedRestTime     = 0;
  jobNode->runningInfo.doneFiles             = 0L;
  jobNode->runningInfo.doneBytes             = 0LL;
  jobNode->runningInfo.totalFiles            = 0L;
  jobNode->runningInfo.totalBytes            = 0LL;
  jobNode->runningInfo.skippedFiles          = 0L;
  jobNode->runningInfo.skippedBytes          = 0LL;
  jobNode->runningInfo.errorFiles            = 0L;
  jobNode->runningInfo.errorBytes            = 0LL;
  jobNode->runningInfo.filesPerSecond        = 0.0;
  jobNode->runningInfo.bytesPerSecond        = 0.0;
  jobNode->runningInfo.storageBytesPerSecond = 0.0;
  jobNode->runningInfo.archiveBytes          = 0LL;
  jobNode->runningInfo.compressionRatio      = 0.0;
  jobNode->runningInfo.fileName              = String_new();
  jobNode->runningInfo.fileDoneBytes         = 0LL;
  jobNode->runningInfo.fileTotalBytes        = 0LL;
  jobNode->runningInfo.storageName           = String_new();
  jobNode->runningInfo.storageDoneBytes      = 0LL;
  jobNode->runningInfo.storageTotalBytes     = 0LL;
  jobNode->runningInfo.volumeNumber          = 0;
  jobNode->runningInfo.volumeProgress        = 0.0;

  return jobNode;
}

/***********************************************************************\
* Name   : freeJobNode
* Purpose: free job node
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeJobNode(JobNode *jobNode)
{
  assert(jobNode != NULL);

  String_delete(jobNode->runningInfo.fileName);
  String_delete(jobNode->runningInfo.storageName);

  List_done(&jobNode->scheduleList,NULL,NULL);
  Pattern_doneList(&jobNode->excludePatternList);
  Pattern_doneList(&jobNode->includePatternList);
  freeJobOptions(&jobNode->jobOptions);
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

  freeJobNode(jobNode);
  LIST_DELETE_NODE(jobNode);
}

/***********************************************************************\
* Name   : findJobById
* Purpose: find job by id
* Input  : jobId - job id
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL JobNode *findJobById(int jobId)
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
* Name   : readJobFileScheduleInfo
* Purpose: read job file schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readJobFileScheduleInfo(JobNode *jobNode)
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
    /* open file .name */
    error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      return error;
    }

    /* read file */
    line = String_new();
    while (!File_eof(&fileHandle))
    {
      /* read line */
      error = File_readLine(&fileHandle,line);
      if (error != ERROR_NONE) break;

      /* skip comments, empty lines */
      if ((String_length(line) == 0) || (String_index(line,0) == '#'))
      {
        continue;
      }

      /* parse */
      if (String_parse(line,STRING_BEGIN,"%lld",NULL,&n))
      {
        jobNode->lastExecutedDateTime = n;
        jobNode->lastCheckDateTime    = n;
      }
    }
    String_delete(line);

    /* close file */
    File_close(&fileHandle);

    if (error != ERROR_NONE)
    {
      File_deleteFileName(fileName);
      return error;
    }
  }

  /* free resources */
  File_deleteFileName(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeJobFileScheduleInfo
* Purpose: write job file schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeJobFileScheduleInfo(JobNode *jobNode)
{
  String     fileName,pathName,baseName;
  FileHandle fileHandle;
  Errors     error;

  assert(jobNode != NULL);

  /* get filename*/
  fileName = File_newFileName();
  File_splitFileName(jobNode->fileName,&pathName,&baseName);
  File_setFileName(fileName,pathName);
  File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
  File_deleteFileName(baseName);
  File_deleteFileName(pathName);

  /* create file .name */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }

  /* write file */
  error = File_printLine(&fileHandle,"%lld",jobNode->lastExecutedDateTime);

  /* close file */
  File_close(&fileHandle);

  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }

  /* free resources */
  File_deleteFileName(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readJobFile
* Purpose: read job from file
* Input  : fileName - file name
* Output : -
* Return : TRUE iff job read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

LOCAL bool readJobFile(JobNode *jobNode)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     name,value;
  ulong      nextIndex;

  assert(jobNode != NULL);
  assert(jobNode->fileName != NULL);

  /* initialise variables */
  line = String_new();

  /* open file */
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open file '%s' (error: %s)!\n",
               String_cString(jobNode->fileName),
               getErrorText(error)
              );
    String_delete(line);
    return FALSE;
  }

  /* reset values */
  Pattern_clearList(&jobNode->includePatternList);
  Pattern_clearList(&jobNode->excludePatternList);
  List_clear(&jobNode->scheduleList,NULL,NULL);

  /* parse file */
  failFlag = FALSE;
  lineNb   = 0;
  name     = String_new();
  value    = String_new();
  while (!File_eof(&fileHandle) && !failFlag)
  {
    /* read line */
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      printError("Cannot read file '%s' (error: %s)!\n",
                 String_cString(jobNode->fileName),
                 getErrorText(error)
                );
      failFlag = TRUE;
      break;
    }
    String_trim(line,STRING_WHITE_SPACES);
    lineNb++;

    /* skip comments, empty lines */
    if ((String_length(line) == 0) || (String_index(line,0) == '#'))
    {
      continue;
    }

    /* parse line */
    if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
//      String_unquote(String_trim(String_sub(value,line,nextIndex,STRING_END),STRING_WHITE_SPACES),STRING_QUOTES);
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             CONFIG_VALUES,SIZE_OF_ARRAY(CONFIG_VALUES),
                             NULL,
                             NULL,
                             jobNode
                            )
         )
      {
        printError("Unknown or invalid config value '%s' in %s, line %ld - skipped\n",
                   String_cString(name),
                   String_cString(jobNode->fileName),
                   lineNb
                  );
//        failFlag = TRUE;
//        break;
      }
    }
    else
    {
      printError("Error in %s, line %ld: '%s' - skipped\n",
                 String_cString(jobNode->fileName),
                 lineNb,
                 String_cString(line)
                );
//      failFlag = TRUE;
//      break;
    }
  }
  String_delete(value);
  String_delete(name);

  /* close file */
  File_close(&fileHandle);

  /* save time modified */
  jobNode->timeModified = File_getFileTimeModified(jobNode->fileName);

  /* read schedule info */
  readJobFileScheduleInfo(jobNode);

  /* free resources */
  String_delete(line);

  return !failFlag;
}

/***********************************************************************\
* Name   : rereadJobFiles
* Purpose: re-read job files
* Input  : jobDirectory - job directory
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors rereadJobFiles(const char *jobDirectory)
{
  Errors          error;
  DirectoryHandle directoryHandle;
  String          fileName;
  String          baseName;
  JobNode         *jobNode,*deleteJobNode;

  assert(jobDirectory != NULL);

  /* init variables */
  fileName = File_newFileName();

  /* add new/update jobs */
  File_setFileNameCString(fileName,jobDirectory);
  error = File_openDirectory(&directoryHandle,fileName);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return error;
  }
  baseName = File_newFileName();
  while (!File_endOfDirectory(&directoryHandle))
  {
    /* read directory entry */
    File_readDirectory(&directoryHandle,fileName);

    /* get base name */
    File_getFileBaseName(baseName,fileName);

    /* check if readable file and not ".*" */
    if (File_isFileReadable(fileName) && (String_index(baseName,0) != '.'))
    {
      /* lock */
      Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

      /* find/create job */
      jobNode = jobList.head;
      while ((jobNode != NULL) && !String_equals(jobNode->name,baseName))
      {
        jobNode = jobNode->next;
      }
      if (jobNode == NULL)
      {
        jobNode = newJob(JOB_TYPE_BACKUP,fileName);
        List_append(&jobList,jobNode);
      }

      if (   !CHECK_JOB_IS_ACTIVE(jobNode)
          && (jobNode->timeModified < File_getFileTimeModified(fileName))
         )
      {
        /* read job */
        readJobFile(jobNode);
      }

      /* unlock */
      Semaphore_unlock(&jobList.lock);
    }
  }
  File_deleteFileName(baseName);
  File_closeDirectory(&directoryHandle);

  /* remove not existing jobs */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
  jobNode = jobList.head;
  while (jobNode != NULL)
  {
    if (jobNode->state == JOB_STATE_NONE)
    {
      File_setFileNameCString(fileName,jobDirectory);
      File_appendFileName(fileName,jobNode->name);
      if (File_isFileReadable(fileName))
      {
        /* exists => ok */
        jobNode = jobNode->next;
      }
      else
      {
        /* not exists => delete */
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
  Semaphore_unlock(&jobList.lock);

  /* free resources */
  File_deleteFileName(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteJobFileEntries
* Purpose: delete entries from job file
* Input  : stringList - job file string list to modify
*          name       - name of value
* Output : -
* Return : next entry in string list or NULL
* Notes  : -
\***********************************************************************/

LOCAL StringNode *deleteJobFileEntries(StringList *stringList, 
                                       const char *name
                                      )
{
  StringNode *nextNode;
  
  StringNode *stringNode;
  String     s;

  nextNode = NULL;

  s = String_new();
  stringNode = stringList->head;
  while (stringNode != NULL)
  {
    /* skip comments, empty lines */
    if ((String_length(stringNode->string) == 0) || (String_index(stringNode->string,0) == '#'))
    {
      stringNode = stringNode->next;
      continue;
    }

    /* parse and delete */
    if (String_parse(stringNode->string,STRING_BEGIN,"%S=% S",NULL,s,NULL) && String_equalsCString(s,name))
    {
      if ((nextNode == NULL) || (nextNode = stringNode)) nextNode = stringNode->next;
      stringNode = StringList_remove(stringList,stringNode);
    }
    else
    {
      stringNode = stringNode->next;
    }
  }
  String_delete(s);

  return nextNode;
}

/***********************************************************************\
* Name   : updateJobFile
* Purpose: update job file
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateJobFile(JobNode *jobNode)
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

  /* read file */
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    StringList_done(&jobFileList);
    String_delete(line);
    return error;
  }
  line  = String_new();
  while (!File_eof(&fileHandle))
  {
    /* read line */
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

  /* update in line list */
  for (z = 0; z < SIZE_OF_ARRAY(CONFIG_VALUES); z++)
  {
    /* delete old entries, get position for insert new entries */
    nextNode = deleteJobFileEntries(&jobFileList,CONFIG_VALUES[z].name);

    /* insert new entries */      
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
//StringList_print(&jobFileList);
//  return ERROR_NONE;

  /* write file */
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    StringList_done(&jobFileList);
    return error;
  }
  while (!StringList_empty(&jobFileList))
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

  /* save time modified */
  jobNode->timeModified = File_getFileTimeModified(jobNode->fileName);

  /* free resources */
  String_delete(line);
  StringList_done(&jobFileList);

  /* reset modified flag */
  jobNode->modifiedFlag = FALSE;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : updateCreateStatus
* Purpose: update create status
* Input  : jobNode          - job node
*          error            - error code
*          createStatusInfo - create status info data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateCreateStatus(JobNode                *jobNode,
                              Errors                 error,
                              const CreateStatusInfo *createStatusInfo
                             )
{
  uint64 elapsedTime;
  double filesPerSecond,bytesPerSecond,storageBytesPerSecond;
  ulong  restFiles;
  uint64 restBytes;
  uint64 restStorageBytes;
  ulong  estimatedRestTime;
  double sum;
  uint   n;

  assert(jobNode != NULL);
  assert(createStatusInfo != NULL);
  assert(createStatusInfo->fileName != NULL);
  assert(createStatusInfo->storageName != NULL);

  /* calculate estimated rest time */
  elapsedTime = Misc_getCurrentDateTime()-jobNode->runningInfo.startDateTime;
  filesPerSecond        = (elapsedTime > 0LL)?(double)createStatusInfo->doneFiles/(double)elapsedTime:0;
  bytesPerSecond        = (elapsedTime > 0LL)?(double)createStatusInfo->doneBytes/(double)elapsedTime:0;
  storageBytesPerSecond = (elapsedTime > 0LL)?(double)createStatusInfo->storageDoneBytes/(double)elapsedTime:0;

  restFiles        = (createStatusInfo->totalFiles        > createStatusInfo->doneFiles       )?createStatusInfo->totalFiles       -createStatusInfo->doneFiles       :0L;
  restBytes        = (createStatusInfo->totalBytes        > createStatusInfo->doneBytes       )?createStatusInfo->totalBytes       -createStatusInfo->doneBytes       :0LL;
  restStorageBytes = (createStatusInfo->storageTotalBytes > createStatusInfo->storageDoneBytes)?createStatusInfo->storageTotalBytes-createStatusInfo->storageDoneBytes:0LL;
  sum = 0; n = 0;
  if (filesPerSecond        > 0) { sum += (double)restFiles/filesPerSecond;               n++; }
  if (bytesPerSecond        > 0) { sum += (double)restBytes/bytesPerSecond;               n++; }
  if (storageBytesPerSecond > 0) { sum += (double)restStorageBytes/storageBytesPerSecond; n++; }
  estimatedRestTime = (n > 0)?(ulong)round(sum/n):0;
/*
fprintf(stderr,"%s,%d: createStatusInfo->doneFiles=%lu createStatusInfo->doneBytes=%llu jobNode->runningInfo.totalFiles=%lu jobNode->runningInfo.totalBytes %llu -- elapsedTime=%lus filesPerSecond=%f bytesPerSecond=%f estimatedRestTime=%lus\n",__FILE__,__LINE__,
createStatusInfo->doneFiles,
createStatusInfo->doneBytes,
jobNode->runningInfo.totalFiles,
jobNode->runningInfo.totalBytes,
elapsedTime,filesPerSecond,bytesPerSecond,estimatedRestTime);
*/

  jobNode->runningInfo.error                 = error;
//  jobNode->runningInfo.error                 = error;
  jobNode->runningInfo.doneFiles             = createStatusInfo->doneFiles;
  jobNode->runningInfo.doneBytes             = createStatusInfo->doneBytes;
  jobNode->runningInfo.totalFiles            = createStatusInfo->totalFiles;
  jobNode->runningInfo.totalBytes            = createStatusInfo->totalBytes;
  jobNode->runningInfo.skippedFiles          = createStatusInfo->skippedFiles;
  jobNode->runningInfo.skippedBytes          = createStatusInfo->skippedBytes;
  jobNode->runningInfo.errorFiles            = createStatusInfo->errorFiles;
  jobNode->runningInfo.errorBytes            = createStatusInfo->errorBytes;
  jobNode->runningInfo.filesPerSecond        = filesPerSecond;
  jobNode->runningInfo.bytesPerSecond        = bytesPerSecond;
  jobNode->runningInfo.storageBytesPerSecond = storageBytesPerSecond;
  jobNode->runningInfo.archiveBytes          = createStatusInfo->archiveBytes;
  jobNode->runningInfo.compressionRatio      = createStatusInfo->compressionRatio;
  jobNode->runningInfo.estimatedRestTime     = estimatedRestTime;
  String_set(jobNode->runningInfo.fileName,createStatusInfo->fileName);
  jobNode->runningInfo.fileDoneBytes         = createStatusInfo->fileDoneBytes;
  jobNode->runningInfo.fileTotalBytes        = createStatusInfo->fileTotalBytes;
  String_set(jobNode->runningInfo.storageName,createStatusInfo->storageName);
  jobNode->runningInfo.storageDoneBytes      = createStatusInfo->storageDoneBytes;
  jobNode->runningInfo.storageTotalBytes     = createStatusInfo->storageTotalBytes;
  jobNode->runningInfo.volumeNumber          = createStatusInfo->volumeNumber;
  jobNode->runningInfo.volumeProgress        = createStatusInfo->volumeProgress;
//fprintf(stderr,"%s,%d: createStatusInfo->fileName=%s\n",__FILE__,__LINE__,String_cString(jobNode->runningInfo.fileName));
}

/***********************************************************************\
* Name   : updateRestoreStatus
* Purpose: update restore status
* Input  : jobNode           - job node
*          error             - error code
*          restoreStatusInfo - create status info data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateRestoreStatus(JobNode                 *jobNode,
                               Errors                  error,
                               const RestoreStatusInfo *restoreStatusInfo
                              )
{
  ulong  elapsedTime;
  double filesPerSecond,bytesPerSecond,storageBytesPerSecond;

  assert(jobNode != NULL);
  assert(restoreStatusInfo != NULL);
  assert(restoreStatusInfo->fileName != NULL);
  assert(restoreStatusInfo->storageName != NULL);

  /* calculate estimated rest time */
  elapsedTime = Misc_getCurrentDateTime()-jobNode->runningInfo.startDateTime;
  filesPerSecond        = (elapsedTime > 0)?(double)restoreStatusInfo->doneFiles/(double)elapsedTime:0;
  bytesPerSecond        = (elapsedTime > 0)?(double)restoreStatusInfo->doneBytes/(double)elapsedTime:0;
  storageBytesPerSecond = (elapsedTime > 0)?(double)restoreStatusInfo->storageDoneBytes/(double)elapsedTime:0;

/*
fprintf(stderr,"%s,%d: restoreStatusInfo->doneFiles=%lu restoreStatusInfo->doneBytes=%llu jobNode->runningInfo.totalFiles=%lu jobNode->runningInfo.totalBytes %llu -- elapsedTime=%lus filesPerSecond=%f bytesPerSecond=%f estimatedRestTime=%lus\n",__FILE__,__LINE__,
restoreStatusInfo->doneFiles,
restoreStatusInfo->doneBytes,
jobNode->runningInfo.totalFiles,
jobNode->runningInfo.totalBytes,
elapsedTime,filesPerSecond,bytesPerSecond,estimatedRestTime);
*/

  jobNode->runningInfo.error                 = error;
  jobNode->runningInfo.doneFiles             = restoreStatusInfo->doneFiles;
  jobNode->runningInfo.doneBytes             = restoreStatusInfo->doneBytes;
  jobNode->runningInfo.skippedFiles          = restoreStatusInfo->skippedFiles;
  jobNode->runningInfo.skippedBytes          = restoreStatusInfo->skippedBytes;
  jobNode->runningInfo.errorFiles            = restoreStatusInfo->errorFiles;
  jobNode->runningInfo.errorBytes            = restoreStatusInfo->errorBytes;
  jobNode->runningInfo.filesPerSecond        = filesPerSecond;
  jobNode->runningInfo.bytesPerSecond        = bytesPerSecond;
  jobNode->runningInfo.storageBytesPerSecond = storageBytesPerSecond;
  jobNode->runningInfo.archiveBytes          = 0LL;
  jobNode->runningInfo.compressionRatio      = 0.0;
  jobNode->runningInfo.estimatedRestTime     = 0;
  String_set(jobNode->runningInfo.fileName,restoreStatusInfo->fileName);
  jobNode->runningInfo.fileDoneBytes         = restoreStatusInfo->fileDoneBytes;
  jobNode->runningInfo.fileTotalBytes        = restoreStatusInfo->fileTotalBytes;
  String_set(jobNode->runningInfo.storageName,restoreStatusInfo->storageName);
  jobNode->runningInfo.storageDoneBytes      = restoreStatusInfo->storageDoneBytes;
  jobNode->runningInfo.storageTotalBytes     = restoreStatusInfo->storageTotalBytes;
  jobNode->runningInfo.volumeNumber          = 0; // ???
  jobNode->runningInfo.volumeProgress        = 0.0; // ???
//fprintf(stderr,"%s,%d: restoreStatusInfo->fileName=%s\n",__FILE__,__LINE__,String_cString(jobNode->runningInfo.fileName));
}

/***********************************************************************\
* Name   : storageRequestVolume
* Purpose: request volume call-back
* Input  : jobNode      - job node
*          volumeNumber - volume number
* Output : -
* Return : TRUE if volume loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool storageRequestVolume(JobNode *jobNode,
                                uint    volumeNumber
                               )
{
  assert(jobNode != NULL);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

//??? lock nicht readwrite

  /* request volume */
  jobNode->requestedVolumeNumber = volumeNumber;

  /* wait until volume is available or job is aborted */
  assert(jobNode->state == JOB_STATE_RUNNING);
  jobNode->state = JOB_STATE_REQUEST_VOLUME;
  do
  {
    Semaphore_waitModified(&jobList.lock);
  }
  while (   !jobNode->requestedAbortFlag
         && (jobNode->volumeNumber != jobNode->requestedVolumeNumber)
        );
  jobNode->state = JOB_STATE_RUNNING;

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  return (jobNode->volumeNumber == jobNode->requestedVolumeNumber);
}

/***********************************************************************\
* Name   : jobThreadEntry
* Purpose: job thread entry
* Input  : jobList - job list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobThreadEntry(void)
{
  JobNode *jobNode;

  while (!quitFlag)
  {
    /* get next job */
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
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
    if (jobNode != NULL)
    {
      jobNode->state = JOB_STATE_RUNNING;
    }
    Semaphore_unlock(&jobList.lock);    /* unlock is ok; job is protected by running state */
    if (quitFlag) break;

    /* run job */
    jobNode->runningInfo.startDateTime = Misc_getCurrentDateTime();
#ifdef SIMULATOR
{
  int z;

  jobNode->runningInfo.estimatedRestTime=120;

  jobNode->runningInfo.totalFiles+=60;
  jobNode->runningInfo.totalBytes+=6000;

  for (z=0;z<120;z++)
  {
    extern void sleep(int);
    if (jobNode->requestedAbortFlag) break;

    fprintf(stderr,"%s,%d: z=%d\n",__FILE__,__LINE__,z);
    sleep(1);

    if (z==40) {
      jobNode->runningInfo.totalFiles+=80;
      jobNode->runningInfo.totalBytes+=8000;
    }

    jobNode->runningInfo.doneFiles++;
    jobNode->runningInfo.doneBytes+=100;
  //  jobNode->runningInfo.totalFiles+=3;
  //  jobNode->runningInfo.totalBytes+=181;
    jobNode->runningInfo.estimatedRestTime=120-z;
    String_clear(jobNode->runningInfo.fileName);String_format(jobNode->runningInfo.fileName,"file %d",z);
    String_clear(jobNode->runningInfo.storageName);String_format(jobNode->runningInfo.storageName,"storage %d%d",z,z);
  }
}
#else
    logMessage(LOG_TYPE_ALWAYS,"------------------------------------------------------------");
    switch (jobNode->type)
    {
      case JOB_TYPE_BACKUP:
        /* create archive */
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
        logMessage(LOG_TYPE_ALWAYS,"start create '%s'",String_cString(jobNode->fileName));
        jobNode->runningInfo.error = Command_create(String_cString(jobNode->archiveName),
                                                    &jobNode->includePatternList,
                                                    &jobNode->excludePatternList,
                                                    &jobNode->jobOptions,
                                                    jobNode->archiveType,
                                                    (CreateStatusInfoFunction)updateCreateStatus,
                                                    jobNode,
                                                    (StorageRequestVolumeFunction)storageRequestVolume,
                                                    jobNode,
                                                    &pauseFlag,
                                                    &jobNode->requestedAbortFlag
                                                   );
        logMessage(LOG_TYPE_ALWAYS,"done create '%s' (error: %s)",String_cString(jobNode->fileName),getErrorText(jobNode->runningInfo.error));
        break;
      case JOB_TYPE_RESTORE:
        {
          StringList archiveFileNameList;

          logMessage(LOG_TYPE_ALWAYS,"start restore");
          StringList_init(&archiveFileNameList);
          StringList_append(&archiveFileNameList,jobNode->archiveName);
          jobNode->runningInfo.error = Command_restore(&archiveFileNameList,
                                                       &jobNode->includePatternList,
                                                       &jobNode->excludePatternList,
                                                       &jobNode->jobOptions,
                                                       (RestoreStatusInfoFunction)updateRestoreStatus,
                                                       jobNode,
                                                       &pauseFlag,
                                                       &jobNode->requestedAbortFlag
                                                      );
          StringList_done(&archiveFileNameList);
          logMessage(LOG_TYPE_ALWAYS,"done restore (error: %s)",getErrorText(jobNode->runningInfo.error));
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
    logPostProcess();
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);

#endif /* SIMULATOR */

    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    /* done job */
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

    /* store schedule info */
    writeJobFileScheduleInfo(jobNode);

    Semaphore_unlock(&jobList.lock);
  }
}

/*---------------------------------------------------------------------*/

LOCAL void schedulerThreadEntry(void)
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
    /* update job files */
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
    jobNode = jobList.head;
    while (jobNode != NULL)
    {
      if (jobNode->modifiedFlag)
      {
        updateJobFile(jobNode);
      }

      jobNode = jobNode->next;
    }
    Semaphore_unlock(&jobList.lock);

    /* re-read config files */
    rereadJobFiles(serverJobDirectory);

    /* trigger jobs */
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
    currentDateTime = Misc_getCurrentDateTime();
    jobNode         = jobList.head;
    pendingFlag     = FALSE;
    while ((jobNode != NULL) && !pendingFlag)
    {
      /* check if job have to be executed */
      executeScheduleNode = NULL;
      if (!CHECK_JOB_IS_ACTIVE(jobNode))
      {
        dateTime = currentDateTime;
        while (   ((dateTime/60LL) > (jobNode->lastCheckDateTime/60LL))
               && (executeScheduleNode == NULL)
               && !pendingFlag
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
            if (   ((scheduleNode->year    == SCHEDULE_ANY) || (scheduleNode->year    == year   ))
                && ((scheduleNode->month   == SCHEDULE_ANY) || (scheduleNode->month   == month  ))
                && ((scheduleNode->day     == SCHEDULE_ANY) || (scheduleNode->day     == day    ))
                && ((scheduleNode->hour    == SCHEDULE_ANY) || (scheduleNode->hour    == hour   ))
                && ((scheduleNode->minute  == SCHEDULE_ANY) || (scheduleNode->minute  == minute ))
                && ((scheduleNode->weekDay == SCHEDULE_ANY) || (scheduleNode->weekDay == weekDay))
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
        if (!pendingFlag)
        {
          jobNode->lastCheckDateTime = currentDateTime;
        }
      }

      /* trigger job */
      if (executeScheduleNode != NULL)
      {
        /* set state */
        jobNode->state              = JOB_STATE_WAITING;
        jobNode->archiveType        = executeScheduleNode->archiveType;
        jobNode->requestedAbortFlag = FALSE;
      }

      /* check next job */
      jobNode = jobNode->next;
    }
    Semaphore_unlock(&jobList.lock);

    /* sleep 1min */
    z = 0;
    while ((z < 60) && !quitFlag)
    {
      Misc_udelay(10LL*1000LL*1000LL);
      z+=10;
    }
  }
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
  assert(clientInfo != NULL);
  assert(data != NULL);

  #ifdef SERVER_DEBUG
    fprintf(stderr,"%s,%d: result=%s",__FILE__,__LINE__,String_cString(data));
  #endif /* SERVER_DEBUG */

  switch (clientInfo->type)
  {
    case CLIENT_TYPE_FILE:
      if (File_write(&clientInfo->file.fileHandle,String_cString(data),String_length(data)) != ERROR_NONE)
      {
fprintf(stderr,"%s,%d: WRITE ERROR X\n",__FILE__,__LINE__);
      }
fflush(stdout);
      break;
    case CLIENT_TYPE_NETWORK:
//??? blockieren?
      Network_send(&clientInfo->network.socketHandle,String_cString(data),String_length(data));
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
//fprintf(stderr,"%s,%d: sent data: '%s'",__FILE__,__LINE__,String_cString(result));
}

/***********************************************************************\
* Name   : sendResult
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

LOCAL void sendResult(ClientInfo *clientInfo, uint id, bool completeFlag, uint errorCode, const char *format, ...)
{
  String  result;
  va_list arguments;

  result = String_new();

  String_format(result,"%d %d %d ",id,completeFlag?1:0,errorCode);
  va_start(arguments,format);
  String_vformat(result,format,arguments);
  va_end(arguments);
  String_appendChar(result,'\n');

  sendClient(clientInfo,result);

  String_delete(result);
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

LOCAL const char *getJobStateText(JobStates jobState)
{
  const char *stateText;

  stateText = "unknown";
  switch (jobState)
  {
    case JOB_STATE_NONE:           stateText = "-";              break;
    case JOB_STATE_WAITING:        stateText = "waiting";        break;
    case JOB_STATE_RUNNING:        stateText = "running";        break;
    case JOB_STATE_REQUEST_VOLUME: stateText = "request volume"; break;
    case JOB_STATE_DONE:           stateText = "done";           break;
    case JOB_STATE_ERROR:          stateText = "ERROR";          break;
    case JOB_STATE_ABORTED:        stateText = "aborted";        break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return stateText;
}

/***********************************************************************\
* Name   : getTotalSubDirectorySize
* Purpose: get total size of files in sub-directory
* Input  : subDirectory - path name
* Output : -
* Return : total size of files in sub-directory (in bytes)
* Notes  : -
\***********************************************************************/

LOCAL uint64 getTotalSubDirectorySize(const String subDirectory)
{
  uint64          totalSize;
  StringList      pathNameList;
  String          pathName;
  DirectoryHandle directoryHandle;
  String          fileName;
  FileInfo        fileInfo;
  Errors          error;

  totalSize = 0;

  StringList_init(&pathNameList);
  StringList_append(&pathNameList,String_duplicate(subDirectory));
  pathName = String_new();
  fileName = String_new();
  while (!StringList_empty(&pathNameList))
  {
    pathName = StringList_getFirst(&pathNameList,pathName);

    error = File_openDirectory(&directoryHandle,pathName);
    if (error != ERROR_NONE)
    {
      continue;
    }

    while (!File_endOfDirectory(&directoryHandle))
    {
      error = File_readDirectory(&directoryHandle,fileName);
      if (error != ERROR_NONE)
      {
        continue;
      }

      error = File_getFileInfo(fileName,&fileInfo);
      if (error != ERROR_NONE)
      {
        continue;
      }

      switch (File_getType(fileName))
      {
        case FILE_TYPE_FILE:
          totalSize += fileInfo.size;
          break;
        case FILE_TYPE_DIRECTORY:
          StringList_append(&pathNameList,String_duplicate(fileName));
          break;
        case FILE_TYPE_LINK:
          break;
        default:
          break;
      }
    }

    File_closeDirectory(&directoryHandle);
  }
  String_delete(pathName);
  String_delete(fileName);
  StringList_done(&pathNameList);

  return totalSize;
}

/*---------------------------------------------------------------------*/

LOCAL void serverCommand_authorize(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  bool okFlag;

  uint z;
  char s[3];
  char n0,n1;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get encoded password (to avoid plain text passwords in memory) */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected password");
    return;
  }

  /* check password */
  okFlag = TRUE;
  if (serverPassword != NULL)
  {
    z = 0;
    while ((z < Password_length(serverPassword)) && okFlag)
    {
      n0 = (char)(strtoul(String_subCString(s,arguments[0],z*2,2),NULL,16) & 0xFF);
      n1 = clientInfo->sessionId[z];
      okFlag = (Password_getChar(serverPassword,z) == (n0^n1));
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

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_status(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  /* format result */
  sendResult(clientInfo,id,TRUE,0,
             "%s",
             pauseFlag?"pause":"ok"
            );
}

LOCAL void serverCommand_pause(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  pauseFlag = TRUE;

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_continue(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  pauseFlag = FALSE;

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_errorInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Errors error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected error code");
    return;
  }
  error = (Errors)String_toInteger(arguments[0],0,NULL,NULL,0);

  /* format result */
  sendResult(clientInfo,id,TRUE,0,
             "%s",
             getErrorText(error)
            );
}

LOCAL void serverCommand_deviceList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Errors       error;
  DeviceHandle deviceHandle;
  String       deviceName;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  error = File_openDevices(&deviceHandle);
  if (error != ERROR_NONE)
  {
    sendResult(clientInfo,id,TRUE,1,"cannot open device list (error: %s)",getErrorText(error));
    return;
  }

  deviceName = String_new();
  while (!File_endOfDevices(&deviceHandle))
  {
    error = File_readDevice(&deviceHandle,deviceName);
    if (error != ERROR_NONE)
    {
      sendResult(clientInfo,id,TRUE,1,"cannot read device list (error: %s)",getErrorText(error));
      File_closeDevices(&deviceHandle);
      String_delete(deviceName);
      return;
    }

    sendResult(clientInfo,id,FALSE,0,"%'S",deviceName);
  }
  String_delete(deviceName);

  File_closeDevices(&deviceHandle);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_fileList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  bool            totalSizeFlag;
  Errors          error;
  DirectoryHandle directoryHandle;
  String          fileName;
  FileInfo        fileInfo;
  uint64          totalSize;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected path name");
    return;
  }
  totalSizeFlag = ((argumentCount >= 2) && String_toBoolean(arguments[1],0,NULL,NULL,0,NULL,0));

  error = File_openDirectory(&directoryHandle,arguments[0]);
  if (error != ERROR_NONE)
  {
    sendResult(clientInfo,id,TRUE,1,"cannot open '%S' (error: %s)",arguments[0],getErrorText(error));
    return;
  }

  fileName = String_new();
  while (!File_endOfDirectory(&directoryHandle))
  {
    error = File_readDirectory(&directoryHandle,fileName);
    if (error == ERROR_NONE)
    {
      error = File_getFileInfo(fileName,&fileInfo);
      if (error == ERROR_NONE)
      {
        switch (File_getType(fileName))
        {
          case FILE_TYPE_FILE:
            sendResult(clientInfo,id,FALSE,0,
                       "FILE %llu %llu %'S",
                       fileInfo.size,
                       fileInfo.timeModified,
                       fileName
                      );
            break;
          case FILE_TYPE_DIRECTORY:
            if (totalSizeFlag)
            {
              totalSize = getTotalSubDirectorySize(fileName);
            }
            else
            {
              totalSize = 0;
            }
            sendResult(clientInfo,id,FALSE,0,
                       "DIRECTORY %llu %llu %'S",
                       totalSize,
                       fileInfo.timeModified,
                       fileName
                      );
            break;
          case FILE_TYPE_LINK:
            sendResult(clientInfo,id,FALSE,0,
                       "LINK %llu %'S",
                       fileInfo.timeModified,
                       fileName
                      );
            break;
          case FILE_TYPE_SPECIAL:
            sendResult(clientInfo,id,FALSE,0,
                       "SPECIAL %llu %'S",
                       fileInfo.timeModified,
                       fileName
                      );
            break;
          default:
            break;
        }
      }
      else
      {
        sendResult(clientInfo,id,FALSE,0,
                   "UNKNOWN %'S",
                   fileName
                  );
      }
    }
    else
    {
      sendResult(clientInfo,id,TRUE,1,"cannot read directory '%S' (error: %s)",arguments[0],getErrorText(error));
      File_closeDirectory(&directoryHandle);
      String_delete(fileName);
      return;
    }
  }
  String_delete(fileName);

  File_closeDirectory(&directoryHandle);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);
  jobNode = jobList.head;
  while (jobNode != NULL)
  {
    sendResult(clientInfo,id,FALSE,0,
               "%u %'S %'s %s %llu %'s %'s %llu %lu",
               jobNode->id,
               jobNode->name,
               getJobStateText(jobNode->state),
               getArchiveTypeName(((jobNode->archiveType == ARCHIVE_TYPE_FULL) || (jobNode->archiveType == ARCHIVE_TYPE_INCREMENTAL))?jobNode->archiveType:jobNode->jobOptions.archiveType),
               jobNode->jobOptions.archivePartSize,
               Compress_getAlgorithmName(jobNode->jobOptions.compressAlgorithm),
               Crypt_getAlgorithmName(jobNode->jobOptions.cryptAlgorithm),
               jobNode->lastExecutedDateTime,
               jobNode->runningInfo.estimatedRestTime
              );

    jobNode = jobNode->next;
  }
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

  /* find job */
  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }

  /* format result */
  if (jobNode != NULL)
  {
    sendResult(clientInfo,id,TRUE,0,
               "%'s %'s %lu %llu %lu %llu %lu %llu %lu %llu %f %f %f %llu %f %'S %llu %llu %'S %llu %llu %d %f %d",
               getJobStateText(jobNode->state),
               (jobNode->runningInfo.error != ERROR_NONE)?getErrorText(jobNode->runningInfo.error):"",
               jobNode->runningInfo.doneFiles,
               jobNode->runningInfo.doneBytes,
               jobNode->runningInfo.totalFiles,
               jobNode->runningInfo.totalBytes,
               jobNode->runningInfo.skippedFiles,
               jobNode->runningInfo.skippedBytes,
               jobNode->runningInfo.errorFiles,
               jobNode->runningInfo.errorBytes,
               jobNode->runningInfo.filesPerSecond,
               jobNode->runningInfo.bytesPerSecond,
               jobNode->runningInfo.storageBytesPerSecond,
               jobNode->runningInfo.archiveBytes,
               jobNode->runningInfo.compressionRatio,
               jobNode->runningInfo.fileName,
               jobNode->runningInfo.fileDoneBytes,
               jobNode->runningInfo.fileTotalBytes,
               jobNode->runningInfo.storageName,
               jobNode->runningInfo.storageDoneBytes,
               jobNode->runningInfo.storageTotalBytes,
               jobNode->runningInfo.volumeNumber,
               jobNode->runningInfo.volumeProgress,
               jobNode->requestedVolumeNumber
              );
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_includePatternsList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint        jobId;
  JobNode     *jobNode;
  PatternNode *patternNode;
  const char  *type;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* send include list */
  patternNode = jobNode->includePatternList.head;
  while (patternNode != NULL)
  {
    type = NULL;
    switch (patternNode->type)
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
    sendResult(clientInfo,id,FALSE,0,
               "%s %'S",
               type,
               patternNode->pattern
              );
    patternNode = patternNode->next;
  }
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_includePatternsClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* clear include list */
  Pattern_clearList(&jobNode->includePatternList);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_includePatternsAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint         jobId;
  PatternTypes patternType;
  String       pattern;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern type");
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
    sendResult(clientInfo,id,TRUE,1,"unknown pattern type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern");
    return;
  }
  pattern = arguments[2];

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* add to include list */
  Pattern_appendList(&jobNode->includePatternList,String_cString(pattern),patternType);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_excludePatternsList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint        jobId;
  JobNode     *jobNode;
  PatternNode *patternNode;
  const char  *type;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* send exclude list */
  patternNode = jobNode->excludePatternList.head;
  while (patternNode != NULL)
  {
    type = NULL;
    switch (patternNode->type)
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
    sendResult(clientInfo,id,FALSE,0,
               "%s %'S",
               type,
               patternNode->pattern
              );
    patternNode = patternNode->next;
  }
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_excludePatternsClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* clear exclude list */
  Pattern_clearList(&jobNode->excludePatternList);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_excludePatternsAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint         jobId;
  PatternTypes patternType;
  String       pattern;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern type");
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
    sendResult(clientInfo,id,TRUE,1,"unknown pattern type '%S'",arguments[1]);
    return;
  }
  if (argumentCount < 3)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern");
    return;
  }
  pattern = arguments[2];

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* add to exclude list */
  Pattern_appendList(&jobNode->excludePatternList,String_cString(pattern),patternType);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_scheduleList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint         jobId;
  JobNode      *jobNode;
  String       line;
  ScheduleNode *scheduleNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* send schedule list */
  line = String_new();
  scheduleNode = jobNode->scheduleList.head;
  while (scheduleNode != NULL)
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
    if (scheduleNode->weekDay != SCHEDULE_ANY)
    {
      switch (scheduleNode->weekDay)
      {
        case WEEKDAY_MON: String_appendCString(line,"Mon"); break;
        case WEEKDAY_TUE: String_appendCString(line,"Tue"); break;
        case WEEKDAY_WED: String_appendCString(line,"Wed"); break;
        case WEEKDAY_THU: String_appendCString(line,"Thu"); break;
        case WEEKDAY_FRI: String_appendCString(line,"Fri"); break;
        case WEEKDAY_SAT: String_appendCString(line,"Sat"); break;
        case WEEKDAY_SUN: String_appendCString(line,"Sun"); break;
      }
      String_appendChar(line,' ');
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
    switch (scheduleNode->archiveType)
    {
      case ARCHIVE_TYPE_NORMAL     : String_appendCString(line,"normal"     ); break;
      case ARCHIVE_TYPE_FULL       : String_appendCString(line,"full"       ); break;
      case ARCHIVE_TYPE_INCREMENTAL: String_appendCString(line,"incremental"); break;
      default                      : String_appendCString(line,"*"          ); break;
    }

    sendResult(clientInfo,id,FALSE,0,
               "%S",
               line
              );
    scheduleNode = scheduleNode->next;
  }
  sendResult(clientInfo,id,TRUE,0,"");
  String_delete(line);

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_scheduleClear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, type, pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* clear schedule list */
  List_clear(&jobNode->scheduleList,NULL,NULL);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_scheduleAdd(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String       s;
  uint         jobId;
  JobNode      *jobNode;
  ScheduleNode *scheduleNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* initialise variables */
  s = String_new();

  /* get job id, date, weekday, time */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    String_delete(s);
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected date");
    String_delete(s);
    return;
  }
  if (argumentCount < 3)
  {
    String_delete(s);
    sendResult(clientInfo,id,TRUE,1,"expected week day");
    return;
  }
  if (argumentCount < 4)
  {
    sendResult(clientInfo,id,TRUE,1,"expected time");
    String_delete(s);
    return;
  }
  if (argumentCount < 5)
  {
    sendResult(clientInfo,id,TRUE,1,"expected type");
    String_delete(s);
    return;
  }
  String_format(s,"%S %S %S %S",arguments[1],arguments[2],arguments[3],arguments[4]);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    String_delete(s);
    return;
  }

  /* parse schedule */
  scheduleNode = parseSchedule(s);  
  if (scheduleNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"cannot parse schedule '%S'",s);
    Semaphore_unlock(&jobList.lock);
    String_delete(s);
    return;
  }
  /* add to schedule list */
  List_append(&jobNode->scheduleList,scheduleNode);
  jobNode->modifiedFlag = TRUE;
  sendResult(clientInfo,id,TRUE,0,"");

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  /* free resources */
  String_delete(s);
}

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

  /* get job id, name, value */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config value name");
    return;
  }
  name = arguments[1];

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* find config value */
  z = 0;
  while (   (z < SIZE_OF_ARRAY(CONFIG_VALUES))
         && !String_equalsCString(name,CONFIG_VALUES[z].name)
        )
  {
    z++;
  }
  if (z >= SIZE_OF_ARRAY(CONFIG_VALUES))
  {
    sendResult(clientInfo,id,TRUE,1,"unknown config value '%S'",name);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* send value */
  s = String_new();
  ConfigValue_formatInit(&configValueFormat,
                         &CONFIG_VALUES[z],
                         CONFIG_VALUE_FORMAT_MODE_VALUE,
                         jobNode
                        );
  ConfigValue_format(&configValueFormat,s);
  ConfigValue_formatDone(&configValueFormat);
  sendResult(clientInfo,id,TRUE,0,"%S",s);
  String_delete(s);

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_optionSet(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  String  name,value;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, name, value */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config name");
    return;
  }
  name = arguments[1];
  if (argumentCount < 3)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config value for '%S'",arguments[0]);
    return;
  }
  value = arguments[2];

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* parse */
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
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown config value '%S'",name);
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_optionDelete(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  String  name;
  JobNode *jobNode;
  uint    z;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get job id, name, value */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config value name");
    return;
  }
  name = arguments[1];

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* find config value */
  z = 0;
  while (   (z < SIZE_OF_ARRAY(CONFIG_VALUES))
         && !String_equalsCString(name,CONFIG_VALUES[z].name)
        )
  {
    z++;
  }
  if (z >= SIZE_OF_ARRAY(CONFIG_VALUES))
  {
    sendResult(clientInfo,id,TRUE,1,"unknown config value '%S'",name);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* delete value */
//  ConfigValue_reset(&CONFIG_VALUES[z],jobNode);

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_jobNew(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  String     fileName;
  JobNode    *jobNode;
  FileHandle fileHandle;
  Errors     error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get filename */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected name");
    return;
  }
  fileName = File_appendFileName(File_setFileNameCString(File_newFileName(),serverJobDirectory),arguments[0]);
  if (File_exists(fileName))
  {
    sendResult(clientInfo,id,TRUE,1,"job '%s' already exists",String_cString(arguments[0]));
    return;
  }

  /* create file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(fileName);
    return;
  }
  File_close(&fileHandle);

  /* create new job */
  jobNode = newJob(JOB_TYPE_BACKUP,fileName);
  if (jobNode == NULL)
  {
    File_delete(fileName,FALSE);
    File_deleteFileName(fileName);
    sendResult(clientInfo,id,TRUE,1,"insufficient memory");
    return;
  }
  copyJobOptions(serverDefaultJobOptions,&jobNode->jobOptions);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* add new job to list */
  List_append(&jobList,jobNode);

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"%d",jobNode->id);

  /* free resources */
  File_deleteFileName(fileName);
}

LOCAL void serverCommand_jobRename(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  String  name;
  JobNode *jobNode;
  String  fileName;
  Errors  error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected name");
    return;
  }
  name = arguments[1];
  if (File_exists(name))
  {
    sendResult(clientInfo,id,TRUE,1,"job '%s' already exists",String_cString(arguments[1]));
    return;
  }

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* rename job */
  fileName = File_appendFileName(File_setFileNameCString(File_newFileName(),serverJobDirectory),name);
  error = File_rename(jobNode->fileName,fileName);
  if (error != ERROR_NONE)
  {
    sendResult(clientInfo,id,TRUE,1,"error renaming job #%d: %s",jobId,getErrorText(error));
    Semaphore_unlock(&jobList.lock);
    File_deleteFileName(fileName);
    return;
  }
  File_deleteFileName(fileName);

  /* store new file name */
  File_appendFileName(File_setFileNameCString(jobNode->fileName,serverJobDirectory),name);
  String_set(jobNode->name,name);

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobDelete(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;
  Errors  error;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* remove job in list if not running or requested volume */
  if (CHECK_JOB_IS_RUNNING(jobNode))
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d running",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* delete job file */
  error = File_delete(jobNode->fileName,FALSE);
  if (error != ERROR_NONE)
  {
    sendResult(clientInfo,id,TRUE,1,"error deleting job #%d: %s",jobId,getErrorText(error));
    Semaphore_unlock(&jobList.lock);
    return;
  }
  List_remove(&jobList,jobNode);
  deleteJob(jobNode);

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobStart(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint         jobId;
  ArchiveTypes archiveType;
  JobNode      *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* get archive type */
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected archive type");
    return;
  }
  if      (String_equalsCString(arguments[1],"full"       )) archiveType = ARCHIVE_TYPE_FULL;
  else if (String_equalsCString(arguments[1],"incremental")) archiveType = ARCHIVE_TYPE_INCREMENTAL;
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown archive type '%S'",arguments[1]);
    return;
  }

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* run job */
  if  (!CHECK_JOB_IS_ACTIVE(jobNode))
  {
    /* set state */
    jobNode->state              = JOB_STATE_WAITING;
    jobNode->archiveType        = archiveType;
    jobNode->requestedAbortFlag = FALSE;
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobAbort(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = findJobById(jobId);
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
    Semaphore_unlock(&jobList.lock);
    return;
  }

  /* abort job */
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
    jobNode->state = JOB_STATE_NONE;
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_volume(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint    jobId;
  uint    volumeNumber;
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get id */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected job id");
    return;
  }
  jobId = String_toInteger(arguments[0],0,NULL,NULL,0);

  /* get volume number */
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected volume number");
    return;
  }
  volumeNumber = String_toInteger(arguments[1],0,NULL,NULL,0);

  /* lock */
  Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* find job */
  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }

  if (jobNode != NULL)
  {
    jobNode->volumeNumber = volumeNumber;
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"job #%d not found",jobId);
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_archiveList(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  Errors          error;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileTypes       fileType;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected archive name");
    return;
  }

  /* open archive */
  error = Archive_open(&archiveInfo,
                       arguments[0],
                       &clientInfo->jobOptions,
                       PASSWORD_MODE_CONFIG
                      );
  if (error != ERROR_NONE)
  {
    sendResult(clientInfo,id,TRUE,1,"Cannot open '%S' (error: %s)",arguments[0],getErrorText(error));
    return;
  }

  /* list contents */
  while (!Archive_eof(&archiveInfo))
  {
    /* get next file type */
    error = Archive_getNextFileType(&archiveInfo,
                                    &archiveFileInfo,
                                    &fileType
                                   );
    if (error != ERROR_NONE)
    {
      sendResult(clientInfo,id,TRUE,1,"Cannot not read content of archive '%S' (error: %s)",arguments[0],getErrorText(error));
      break;
    }

    switch (fileType)
    {
      case FILE_TYPE_FILE:
        {
          ArchiveFileInfo    archiveFileInfo;
          CompressAlgorithms compressAlgorithm;
          CryptAlgorithms    cryptAlgorithm;
          String             fileName;
          FileInfo           fileInfo;
          uint64             fragmentOffset,fragmentSize;

          /* open archive file */
          fileName = String_new();
          error = Archive_readFileEntry(&archiveInfo,
                                        &archiveFileInfo,
                                        &compressAlgorithm,
                                        &cryptAlgorithm,
                                        fileName,
                                        &fileInfo,
                                        &fragmentOffset,
                                        &fragmentSize
                                       );
          if (error != ERROR_NONE)
          {
            sendResult(clientInfo,id,TRUE,1,"Cannot not read content of archive '%S' (error: %s)",arguments[0],getErrorText(error));
            String_delete(fileName);
            break;
          }

          /* match pattern */
          if (   (List_empty(&clientInfo->includePatternList) || Pattern_matchList(&clientInfo->includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
              && !Pattern_matchList(&clientInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            sendResult(clientInfo,id,FALSE,0,
                       "FILE %llu %llu %llu %llu %d %d %'S",
                       fileInfo.size,
                       archiveFileInfo.file.chunkInfoFileData.size,
                       fragmentOffset,
                       fragmentSize,
                       compressAlgorithm,
                       cryptAlgorithm,
                       fileName
                      );
          }

          /* close archive file, free resources */
          Archive_closeEntry(&archiveFileInfo);
          String_delete(fileName);
        }
        break;
      case FILE_TYPE_DIRECTORY:
        {
          CryptAlgorithms cryptAlgorithm;
          String          directoryName;
          FileInfo        fileInfo;

          /* open archive directory */
          directoryName = String_new();
          error = Archive_readDirectoryEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             &cryptAlgorithm,
                                             directoryName,
                                             &fileInfo
                                            );
          if (error != ERROR_NONE)
          {
            sendResult(clientInfo,id,TRUE,1,"Cannot not read content of archive '%S' (error: %s)",arguments[0],getErrorText(error));
            String_delete(directoryName);
            break;
          }

          /* match pattern */
          if (   (List_empty(&clientInfo->includePatternList) || Pattern_matchList(&clientInfo->includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
              && !Pattern_matchList(&clientInfo->excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            sendResult(clientInfo,id,FALSE,0,
                       "DIRECTORY %d %'S",
                       cryptAlgorithm,
                       directoryName
                      );
          }

          /* close archive file, free resources */
          Archive_closeEntry(&archiveFileInfo);
          String_delete(directoryName);
        }
        break;
      case FILE_TYPE_LINK:
        {
          CryptAlgorithms cryptAlgorithm;
          String          linkName;
          String          fileName;
          FileInfo        fileInfo;

          /* open archive link */
          linkName = String_new();
          fileName = String_new();
          error = Archive_readLinkEntry(&archiveInfo,
                                        &archiveFileInfo,
                                        &cryptAlgorithm,
                                        linkName,
                                        fileName,
                                        &fileInfo
                                       );
          if (error != ERROR_NONE)
          {
            sendResult(clientInfo,id,TRUE,1,"Cannot not read content of archive '%S' (error: %s)",arguments[0],getErrorText(error));
            String_delete(fileName);
            String_delete(linkName);
            break;
          }

          /* match pattern */
          if (   (List_empty(&clientInfo->includePatternList) || Pattern_matchList(&clientInfo->includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
              && !Pattern_matchList(&clientInfo->excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            sendResult(clientInfo,id,FALSE,0,
                       "LINK %d %'S %'S",
                       cryptAlgorithm,
                       linkName,
                       fileName
                      );
          }

          /* close archive file, free resources */
          Archive_closeEntry(&archiveFileInfo);
          String_delete(fileName);
          String_delete(linkName);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  }

  /* close archive */
  Archive_close(&archiveInfo);

  sendResult(clientInfo,id,TRUE,0,"");
}

#ifndef NDEBUG
LOCAL void serverCommand_debugMemoryInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Array_debug();
  String_debug();

  sendResult(clientInfo,id,TRUE,0,"");
}
#endif /* NDEBUG */

// server commands
const struct
{
  const char            *name;
  const char            *parameters;
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
}
SERVER_COMMANDS[] =
{
  { "AUTHORIZE",             "S",   serverCommand_authorize,           AUTHORIZATION_STATE_WAITING },
  { "STATUS",                "",    serverCommand_status,              AUTHORIZATION_STATE_OK      },
  { "PAUSE",                 "",    serverCommand_pause ,              AUTHORIZATION_STATE_OK      },
  { "CONTINUE",              "",    serverCommand_continue,            AUTHORIZATION_STATE_OK      },
  { "ERROR_INFO",            "i",   serverCommand_errorInfo,           AUTHORIZATION_STATE_OK      },
  { "DEVICE_LIST",           "",    serverCommand_deviceList,          AUTHORIZATION_STATE_OK      },
  { "FILE_LIST",             "S",   serverCommand_fileList,            AUTHORIZATION_STATE_OK      },
  { "JOB_LIST",              "",    serverCommand_jobList,             AUTHORIZATION_STATE_OK      },
  { "JOB_INFO",              "i",   serverCommand_jobInfo,             AUTHORIZATION_STATE_OK      },
  { "JOB_NEW",               "S",   serverCommand_jobNew,              AUTHORIZATION_STATE_OK      },
  { "JOB_RENAME",            "i S", serverCommand_jobRename,           AUTHORIZATION_STATE_OK      },
  { "JOB_DELETE",            "i",   serverCommand_jobDelete,           AUTHORIZATION_STATE_OK      },
  { "JOB_START",             "i",   serverCommand_jobStart,            AUTHORIZATION_STATE_OK      },
  { "JOB_ABORT",             "i",   serverCommand_jobAbort,            AUTHORIZATION_STATE_OK      },
  { "INCLUDE_PATTERNS_LIST", "i",   serverCommand_includePatternsList, AUTHORIZATION_STATE_OK      },
  { "INCLUDE_PATTERNS_CLEAR","i",   serverCommand_includePatternsClear,AUTHORIZATION_STATE_OK      },
  { "INCLUDE_PATTERNS_ADD",  "i S", serverCommand_includePatternsAdd,  AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_PATTERNS_LIST", "i",   serverCommand_excludePatternsList, AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_PATTERNS_CLEAR","i",   serverCommand_excludePatternsClear,AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_PATTERNS_ADD",  "i S", serverCommand_excludePatternsAdd,  AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_LIST",         "i",   serverCommand_scheduleList,        AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_CLEAR",        "i",   serverCommand_scheduleClear,       AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_ADD",          "i S", serverCommand_scheduleAdd,         AUTHORIZATION_STATE_OK      },
  { "OPTION_GET",            "s",   serverCommand_optionGet,           AUTHORIZATION_STATE_OK      },
  { "OPTION_SET",            "s S", serverCommand_optionSet,           AUTHORIZATION_STATE_OK      },
  { "OPTION_DELETE",         "s S", serverCommand_optionDelete,        AUTHORIZATION_STATE_OK      },
  { "VOLUME",                "S i", serverCommand_volume,              AUTHORIZATION_STATE_OK      },
  { "ARCHIVE_LIST",          "S",   serverCommand_archiveList,         AUTHORIZATION_STATE_OK      },
  #ifndef NDEBUG
  { "DEBUG_MEMORY_INFO",     "",    serverCommand_debugMemoryInfo,     AUTHORIZATION_STATE_OK      },
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

  String_delete(*string);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : freeCommand
* Purpose: free allocated command message
* Input  : commandMsg - command message
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommand(CommandMsg *commandMsg)
{
  assert(commandMsg != NULL);

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

  /* initialize tokenizer */
  String_initTokenizer(&stringTokenizer,string,STRING_WHITE_SPACES,STRING_QUOTES,TRUE);

  /* get id */
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

  /* get command */
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

  /* get arguments */
  commandMsg->arguments = Array_new(sizeof(String),0);
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    argument = String_duplicate(token);
    Array_append(commandMsg->arguments,&argument);
  }

  /* free resources */
  String_doneTokenizer(&stringTokenizer);

  return TRUE;
}

/***********************************************************************\
* Name   : networkClientThread
* Purpose: processing thread for network clients
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void networkClientThread(ClientInfo *clientInfo)
{
  CommandMsg commandMsg;
  String     result;

  assert(clientInfo != NULL);

  result = String_new();
  while (   !clientInfo->network.exitFlag
         && MsgQueue_get(&clientInfo->network.commandMsgQueue,&commandMsg,NULL,sizeof(commandMsg))
        )
  {
    /* check authorization */
    if (clientInfo->authorizationState == commandMsg.authorizationState)
    {
      /* execute command */
      commandMsg.serverCommandFunction(clientInfo,
                                       commandMsg.id,
                                       Array_cArray(commandMsg.arguments),
                                       Array_length(commandMsg.arguments)
                                      );
    }
    else
    {
      /* authorization failure -> mark for disconnect */
      sendResult(clientInfo,commandMsg.id,TRUE,1,"authorization failure");
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
    }

    /* free resources */
    freeCommand(&commandMsg);
  }
  String_delete(result);

  clientInfo->network.exitFlag = TRUE;
}

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

  freeCommand(commandMsg);
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
* Name   : newFileClient
* Purpose: create new client with file i/o
* Input  : clientInfo - client info to initialize
*          fileHandle - client file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initFileClient(ClientInfo *clientInfo,
                          FileHandle fileHandle
                         )
{
  assert(clientInfo != NULL);

  /* initialize */
  clientInfo->type               = CLIENT_TYPE_FILE;
//  clientInfo->authorizationState   = AUTHORIZATION_STATE_WAITING;
clientInfo->authorizationState   = AUTHORIZATION_STATE_OK;
  clientInfo->file.fileHandle    = fileHandle;

  Pattern_initList(&clientInfo->includePatternList);
  Pattern_initList(&clientInfo->excludePatternList);
  initJobOptions(&clientInfo->jobOptions);

  /* create and send session id */
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
  assert(clientInfo != NULL);

  /* initialize */
  clientInfo->type                 = CLIENT_TYPE_NETWORK;
  clientInfo->authorizationState   = AUTHORIZATION_STATE_WAITING;
  clientInfo->network.name         = String_duplicate(name);
  clientInfo->network.port         = port;
  clientInfo->network.socketHandle = socketHandle;
  clientInfo->network.exitFlag     = FALSE;

  Pattern_initList(&clientInfo->includePatternList);
  Pattern_initList(&clientInfo->excludePatternList);
  initJobOptions(&clientInfo->jobOptions);

  if (!MsgQueue_init(&clientInfo->network.commandMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise client command message queue!");
  }
  if (!Thread_init(&clientInfo->network.thread,"Client",0,networkClientThread,clientInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise client thread!");
  }

  /* create and send session id */
  getNewSessionId(clientInfo->sessionId);
  sendSessionId(clientInfo);
}

/***********************************************************************\
* Name   : doneNetworkClient
* Purpose: deinitialize client
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneNetworkClient(ClientInfo *clientInfo)
{
  assert(clientInfo != NULL);

  switch (clientInfo->type)
  {
    case CLIENT_TYPE_FILE:
      break;
    case CLIENT_TYPE_NETWORK:
      /* stop client thread */
      clientInfo->network.exitFlag = TRUE;
      MsgQueue_setEndOfMsg(&clientInfo->network.commandMsgQueue);
      Thread_join(&clientInfo->network.thread);

      /* free resources */
      MsgQueue_done(&clientInfo->network.commandMsgQueue,(MsgQueueMsgFreeFunction)freeCommandMsg,NULL);
      String_delete(clientInfo->network.name);
      Thread_done(&clientInfo->network.thread);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  freeJobOptions(&clientInfo->jobOptions);
  Pattern_doneList(&clientInfo->excludePatternList);
  Pattern_doneList(&clientInfo->includePatternList);
}

/***********************************************************************\
* Name   : freeClientNode
* Purpose: free client node
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeClientNode(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  doneNetworkClient(&clientNode->clientInfo);
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

  /* create new client node */
  clientNode = LIST_NEW_NODE(ClientNode);
  if (clientNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* initialize node */
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

  freeClientNode(clientNode);

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
  #endif /* SERVER_DEBUG */
  if (String_equalsCString(command,"VERSION"))
  {
    /* version info */
    sendResult(clientInfo,0,TRUE,0,"%d %d\n",PROTOCOL_VERSION_MAJOR,PROTOCOL_VERSION_MINOR);
  }
  #ifndef NDEBUG
    else if (String_equalsCString(command,"QUIT"))
    {
      quitFlag = TRUE;
      sendResult(clientInfo,0,TRUE,0,"ok\n");
    }
  #endif /* not NDEBUG */
  else
  {
    /* parse command */
    if (!parseCommand(&commandMsg,command))
    {
      sendResult(clientInfo,0,TRUE,1,"parse error");
      return;
    }

    switch (clientInfo->type)
    {
      case CLIENT_TYPE_FILE:
        /* check authorization */
        if (clientInfo->authorizationState == commandMsg.authorizationState)
        {
          /* execute */
          commandMsg.serverCommandFunction(clientInfo,
                                           commandMsg.id,
                                           Array_cArray(commandMsg.arguments),
                                           Array_length(commandMsg.arguments)
                                          );
        }
        else
        {
          /* authorization failure -> mark for disconnect */
          sendResult(clientInfo,commandMsg.id,TRUE,1,"authorization failure");
          clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
        }

        /* free resources */
        freeCommand(&commandMsg);
        break;
      case CLIENT_TYPE_NETWORK:
        /* send command to client thread */
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
                  const char       *jobDirectory,
                  const JobOptions *defaultJobOptions
                 )
{
  Errors             error;
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

  assert((port != 0) || (tlsPort != 0));

  /* initialise variables */
  serverPassword          = password;
  serverJobDirectory      = jobDirectory;
  serverDefaultJobOptions = defaultJobOptions;
  List_init(&jobList);
  List_init(&clientList);
  pauseFlag               = FALSE;
  quitFlag                = FALSE;

  /* init server sockets */
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
      printError("Cannot initialize server at port %d (error: %s)!\n",
                 port,
                 getErrorText(error)
                );
      return FALSE;
    }
    printInfo(1,"Started server on port %d\n",port);
  }
  if (tlsPort != 0)
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
        printError("Cannot initialize TLS/SSL server at port %d (error: %s)!\n",
                   tlsPort,
                   getErrorText(error)
                  );
        if (port != 0) Network_doneServer(&serverSocketHandle);
        return FALSE;
      }
      printInfo(1,"Started TLS/SSL server on port %d\n",tlsPort);
  #else /* not HAVE_GNU_TLS */
    UNUSED_VARIABLE(caFileName);
    UNUSED_VARIABLE(certFileName);
    UNUSED_VARIABLE(keyFileName);

    printError("TLS/SSL server is not supported!\n");
    Network_doneServer(&serverSocketHandle);
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GNU_TLS */
  }

  /* start threads */
  if (!Thread_init(&jobThread,"BAR job",globalOptions.niceLevel,jobThreadEntry,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialise job thread!");
  }
  if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadEntry,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialise scheduler thread!");
  }

  /* run server */
  clientName = String_new();
  while (!quitFlag)
  {
    /* wait for command */
    FD_ZERO(&selectSet);
    if (port    != 0) FD_SET(Network_getServerSocket(&serverSocketHandle),   &selectSet);
    if (tlsPort != 0) FD_SET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet);
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      FD_SET(Network_getSocket(&clientNode->clientInfo.network.socketHandle),&selectSet);
      clientNode = clientNode->next;
    }
    select(FD_SETSIZE,&selectSet,NULL,NULL,NULL);

    /* connect new clients */
    if ((port != 0) && FD_ISSET(Network_getServerSocket(&serverSocketHandle),&selectSet))
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
                   getErrorText(error)
                  );
      }
    }
    if ((tlsPort != 0) && FD_ISSET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet))
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
                   getErrorText(error)
                  );
      }
    }

    /* process client commands/disconnect clients */
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (FD_ISSET(Network_getSocket(&clientNode->clientInfo.network.socketHandle),&selectSet))
      {
        Network_receive(&clientNode->clientInfo.network.socketHandle,buffer,sizeof(buffer),WAIT_FOREVER,&receivedBytes);
        if (receivedBytes > 0)
        {
          /* received data -> process */
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
          /* disconnect */
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

    /* removed disconnect clients because of authorization failure */
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (clientNode->clientInfo.authorizationState == AUTHORIZATION_STATE_FAIL)
      {
        /* authorizaton error -> disconnect  client */
        printInfo(1,"Disconnected client '%s:%u': authorization failure\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);

        deleteClientNode = clientNode;
        clientNode = clientNode->next;
        List_remove(&clientList,deleteClientNode);

        Network_disconnect(&deleteClientNode->clientInfo.network.socketHandle);
        deleteClient(deleteClientNode);
      }
      else
      {
        /* next client */
        clientNode = clientNode->next;
      }
    }
  }
  String_delete(clientName);

  /* wait for thread exit */
  Thread_join(&schedulerThread);
  Thread_join(&jobThread);

  /* done server */
  if (port    != 0) Network_doneServer(&serverSocketHandle);
  if (tlsPort != 0) Network_doneServer(&serverTLSSocketHandle);

  /* free resources */
  Thread_done(&schedulerThread);
  Thread_done(&jobThread);
  List_done(&clientList,(ListNodeFreeFunction)freeClientNode,NULL);
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

  /* initialize variables */
  List_init(&jobList);
  quitFlag = FALSE;

  /* initialize input/output */
  error = File_openDescriptor(&inputFileHandle,inputDescriptor,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize input (error: %s)!\n",
               getErrorText(error)
              );
    return error;
  }
  error = File_openDescriptor(&outputFileHandle,outputDescriptor,FILE_OPENMODE_WRITE);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize input (error: %s)!\n",
               getErrorText(error)
              );
    File_close(&inputFileHandle);
    return error;
  }

  /* init variables */
  initFileClient(&clientInfo,outputFileHandle);

  /* run server */
  commandString = String_new();
#if 1
  while (!quitFlag && !File_eof(&inputFileHandle))
  {
    /* read command line */
    File_readLine(&inputFileHandle,commandString);

    /* process */
    processCommand(&clientInfo,commandString);
  }
#else /* 0 */
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
String_setCString(commandString,"1 SET crypt-password 'muster'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN REGEX test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"4 ARCHIVE_LIST scp:torsten@it-zo.de:backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */
  String_delete(commandString);

  /* free resources */
  doneNetworkClient(&clientInfo);
  File_close(&outputFileHandle);
  File_close(&inputFileHandle);
  List_done(&jobList,(ListNodeFreeFunction)freeJobNode,NULL);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
