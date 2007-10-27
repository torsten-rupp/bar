/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.c,v $
* $Revision: 1.17 $
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
#include "semaphores.h"
#include "msgqueues.h"
#include "stringlists.h"

#include "bar.h"
#include "passwords.h"
#include "network.h"
#include "files.h"
#include "patterns.h"
#include "archive.h"

#include "commands_create.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define SERVER_DEBUG
#define _SIMULATOR

/***************************** Constants *******************************/

#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

#define SESSION_ID_LENGTH 64

#define MAX_JOBS_IN_LIST 32

/***************************** Datatypes *******************************/

/* backup/restore job */
typedef struct JobNode
{
  NODE_HEADER(struct JobNode);

  uint        id;
  String      name;
  enum
  {
    MODE_BACKUP,
    MODE_RESTORE,
  } mode;
  String      archiveFileName;
  PatternList includePatternList;
  PatternList excludePatternList;
  Options     options;

  /* running info */
  struct
  {
    enum
    {
      JOB_STATE_WAITING,
      JOB_STATE_RUNNING,
      JOB_STATE_COMPLETED,
      JOB_STATE_ERROR
    } state;
    time_t startTime;                       // start time [s]
    ulong  estimatedRestTime;               // estimated rest running time [s]
    double bytesPerSecond;                  // average processed bytes per second
    double filesPerSecond;                  // average processed files per second
    double storageBytesPerSecond;           // average processed storage bytes per second
    ulong  doneFiles;                       // number of processed files
    uint64 doneBytes;                       // sum of processed bytes
    ulong  totalFiles;                      // number of total files
    uint64 totalBytes;                      // sum of total bytes
    ulong  skippedFiles;                    // number of skipped files
    uint64 skippedBytes;                    // sum of skippped bytes
    ulong  errorFiles;                      // number of files with errors
    uint64 errorBytes;                      // sum of bytes of files with errors
    double compressionRatio;
    String fileName;                        // current file
    uint64 fileDoneBytes;                   // current file bytes done
    uint64 fileTotalBytes;                  // current file bytes total
    String storageName;                     // current storage file
    uint64 storageDoneBytes;                // current storage file bytes done
    uint64 storageTotalBytes;               // current storage file bytes total

    bool   abortRequestFlag;                // TRUE for abort operation
    Errors error;
  } runningInfo;
} JobNode;

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

typedef struct
{
  ClientTypes         type;

  SessionId           sessionId;
  AuthorizationStates authorizationState;

  union
  {
    /* i/o via vile */
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
      pthread_t    threadId;
      MsgQueue     commandMsgQueue;
      bool         exitFlag;
    } network;
  };

  PatternList         includePatternList;
  PatternList         excludePatternList;
  Options             options;
} ClientInfo;

/* client node */
typedef struct ClientNode
{
  NODE_HEADER(struct ClientNode);

  ClientInfo clientInfo;
  String     commandString;
} ClientNode;

typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

typedef void(*ServerCommandFunction)(ClientInfo    *clientInfo,
                                     uint          id,
                                     const String  arguments[],
                                     uint          argumentCount
                                    );

typedef struct
{
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
  uint                  id;
  Array                 arguments;
} CommandMsg;

/***************************** Variables *******************************/
LOCAL Password   *password;
LOCAL JobList    jobList;
LOCAL pthread_t  jobThreadId;
LOCAL ClientList clientList;
LOCAL bool       quitFlag;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : updateCreateStatus
* Purpose: update create status
* Input  : createStatusInfo - create status info data
*          jobNode          - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateCreateStatus(Errors                 error,
                              const CreateStatusInfo *createStatusInfo,
                              JobNode                *jobNode
                             )
{
  ulong  elapsedTime;
  double filesPerSecond,bytesPerSecond,storageBytesPerSecond;
  ulong  restFiles;
  uint64 restBytes;
  uint64 restStorageBytes;
  ulong  estimtedRestTime;
  double sum;
  uint   n;
static int zz=0;

  assert(createStatusInfo != NULL);
  assert(createStatusInfo->fileName != NULL);
  assert(createStatusInfo->storageName != NULL);
  assert(jobNode != NULL);

  /* calculate estimated rest time */
  elapsedTime = (ulong)(time(NULL)-jobNode->runningInfo.startTime);
  filesPerSecond        = (elapsedTime > 0)?(double)createStatusInfo->doneFiles/(double)elapsedTime:0;
  bytesPerSecond        = (elapsedTime > 0)?(double)createStatusInfo->doneBytes/(double)elapsedTime:0;
  storageBytesPerSecond = (elapsedTime > 0)?(double)createStatusInfo->storageDoneBytes/(double)elapsedTime:0;

zz++;
if (zz>10) {
fprintf(stderr,"%s,%d: filesPerSecond=%f bytesPerSecond=%f storageBytesPerSecond=%f %llu %llu\n",__FILE__,__LINE__,filesPerSecond,bytesPerSecond,
storageBytesPerSecond,
createStatusInfo->storageDoneBytes,createStatusInfo->storageTotalBytes
);
zz=0;
}
  restFiles        = (createStatusInfo->totalFiles        > createStatusInfo->doneFiles       )?createStatusInfo->totalFiles       -createStatusInfo->doneFiles       :0L;
  restBytes        = (createStatusInfo->totalBytes        > createStatusInfo->doneBytes       )?createStatusInfo->totalBytes       -createStatusInfo->doneBytes       :0LL;
  restStorageBytes = (createStatusInfo->storageTotalBytes > createStatusInfo->storageDoneBytes)?createStatusInfo->storageTotalBytes-createStatusInfo->storageDoneBytes:0LL;
  sum = 0; n = 0;
  if (filesPerSecond        > 0) { sum += (double)restFiles/filesPerSecond;               n++; }
  if (bytesPerSecond        > 0) { sum += (double)restBytes/bytesPerSecond;               n++; }
  if (storageBytesPerSecond > 0) { sum += (double)restStorageBytes/storageBytesPerSecond; n++; }
  if (n > 0)
  estimtedRestTime = (n > 0)?(ulong)round(sum/n):0;
/*
fprintf(stderr,"%s,%d: createStatusInfo->doneFiles=%lu createStatusInfo->doneBytes=%llu jobNode->runningInfo.totalFiles=%lu jobNode->runningInfo.totalBytes %llu -- elapsedTime=%lus filesPerSecond=%f bytesPerSecond=%f estimtedRestTime=%lus\n",__FILE__,__LINE__,
createStatusInfo->doneFiles,
createStatusInfo->doneBytes,
jobNode->runningInfo.totalFiles,
jobNode->runningInfo.totalBytes,
elapsedTime,filesPerSecond,bytesPerSecond,estimtedRestTime);
*/

  jobNode->runningInfo.error                 = error;
  jobNode->runningInfo.filesPerSecond        = filesPerSecond;
  jobNode->runningInfo.bytesPerSecond        = bytesPerSecond;
  jobNode->runningInfo.storageBytesPerSecond = restStorageBytes;
  jobNode->runningInfo.doneFiles             = createStatusInfo->doneFiles;
  jobNode->runningInfo.doneBytes             = createStatusInfo->doneBytes;
  jobNode->runningInfo.totalFiles            = createStatusInfo->totalFiles;
  jobNode->runningInfo.totalBytes            = createStatusInfo->totalBytes;
  jobNode->runningInfo.skippedFiles          = createStatusInfo->skippedFiles;
  jobNode->runningInfo.skippedBytes          = createStatusInfo->skippedBytes;
  jobNode->runningInfo.errorFiles            = createStatusInfo->errorFiles;
  jobNode->runningInfo.errorBytes            = createStatusInfo->errorBytes;
  jobNode->runningInfo.compressionRatio      = createStatusInfo->compressionRatio;
  jobNode->runningInfo.estimatedRestTime     = estimtedRestTime;
  String_set(jobNode->runningInfo.fileName,createStatusInfo->fileName);
  jobNode->runningInfo.fileDoneBytes         = createStatusInfo->fileDoneBytes;
  jobNode->runningInfo.fileTotalBytes        = createStatusInfo->fileTotalBytes;
  String_set(jobNode->runningInfo.storageName,createStatusInfo->storageName);
  jobNode->runningInfo.storageDoneBytes      = createStatusInfo->storageDoneBytes;
  jobNode->runningInfo.storageTotalBytes     = createStatusInfo->storageTotalBytes;
//fprintf(stderr,"%s,%d: createStatusInfo->fileName=%s\n",__FILE__,__LINE__,String_cString(jobNode->runningInfo.fileName));
}

/***********************************************************************\
* Name   : jobThread
* Purpose: job thread
* Input  : jobList - job list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobThread(JobList *jobList)
{
  JobNode *jobNode;
//  Options options;

  while (!quitFlag)
  {
    /* get next job */
    Semaphore_lock(&jobList->lock);
    do
    {
      jobNode = jobList->head;
      while ((jobNode != NULL) && (jobNode->runningInfo.state != JOB_STATE_WAITING))
      {
        jobNode = jobNode->next;
      }
      if (jobNode == NULL) Semaphore_waitModified(&jobList->lock);
    }
    while (!quitFlag && (jobNode == NULL));
    if (jobNode != NULL)
    {
      jobNode->runningInfo.state = JOB_STATE_RUNNING;
    }
    Semaphore_unlock(&jobList->lock);    /* unlock is ok; job is protected by running state */
    if (quitFlag) break;

    /* run job */
    jobNode->runningInfo.startTime = time(NULL);
#ifdef SIMULATOR
{
  int z;

  jobNode->runningInfo.estimatedRestTime=120;

  jobNode->runningInfo.totalFiles+=60;
  jobNode->runningInfo.totalBytes+=6000;

  for (z=0;z<120;z++)
  {
    extern void sleep(int);
    if (jobNode->runningInfo.abortRequestFlag) break;

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
    /* create archive */
    jobNode->runningInfo.error = Command_create(String_cString(jobNode->archiveFileName),
                                                &jobNode->includePatternList,
                                                &jobNode->excludePatternList,
                                                &jobNode->options,
                                                (CreateStatusInfoFunction)updateCreateStatus,
                                                jobNode,
                                                &jobNode->runningInfo.abortRequestFlag
                                               );

#endif /* SIMULATOR */

    /* done job (lock list to signal modifcation to waiting threads) */
    Semaphore_lock(&jobList->lock);
    jobNode->runningInfo.state = (jobNode->runningInfo.error == ERROR_NONE)?JOB_STATE_COMPLETED:JOB_STATE_ERROR;
    Semaphore_unlock(&jobList->lock);
  }
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

  freeOptions(&jobNode->options);
  Pattern_doneList(&jobNode->excludePatternList);
  Pattern_doneList(&jobNode->includePatternList);
  String_delete(jobNode->archiveFileName);
  String_delete(jobNode->name);

  String_delete(jobNode->runningInfo.fileName);
  String_delete(jobNode->runningInfo.storageName);
}

/***********************************************************************\
* Name   : newJob
* Purpose: create new job
* Input  : -
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *newJob(void)
{
  JobNode *jobNode;

  jobNode = LIST_NEW_NODE(JobNode);
  if (jobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Pattern_initList(&jobNode->includePatternList);
  Pattern_initList(&jobNode->excludePatternList);

  jobNode->runningInfo.state             = JOB_STATE_WAITING;
  jobNode->runningInfo.startTime         = 0;
  jobNode->runningInfo.estimatedRestTime = 0;
  jobNode->runningInfo.doneFiles         = 0L;
  jobNode->runningInfo.doneBytes         = 0LL;
  jobNode->runningInfo.totalFiles        = 0L;
  jobNode->runningInfo.totalBytes        = 0LL;
  jobNode->runningInfo.skippedFiles      = 0L;
  jobNode->runningInfo.skippedBytes      = 0LL;
  jobNode->runningInfo.totalFiles        = 0L;
  jobNode->runningInfo.totalBytes        = 0LL;
  jobNode->runningInfo.compressionRatio  = 0.0;
  jobNode->runningInfo.fileName          = String_new();
  jobNode->runningInfo.fileDoneBytes     = 0LL;
  jobNode->runningInfo.fileTotalBytes    = 0LL;
  jobNode->runningInfo.storageName       = String_new();
  jobNode->runningInfo.storageDoneBytes  = 0LL;
  jobNode->runningInfo.storageTotalBytes = 0LL;

  jobNode->runningInfo.abortRequestFlag  = FALSE;
  jobNode->runningInfo.error             = ERROR_NONE;

  return jobNode;  
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
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
  StringList_append(&pathNameList,String_copy(subDirectory));
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
          StringList_append(&pathNameList,String_copy(fileName));
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
  if (password != NULL)
  {
    z = 0;
    while ((z < Password_length(password)) && okFlag)
    {
      n0 = (char)(strtoul(String_subCString(s,arguments[0],z*2,sizeof(s)),NULL,16) & 0xFF);
      n1 = clientInfo->sessionId[z];
      okFlag = (Password_getChar(password,z) == (n0^n1));
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
                       "FILE %llu %'S",
                       fileInfo.size,
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
                       "DIRECTORY %llu %'S",
                       totalSize,
                       fileName
                      );
            break;
          case FILE_TYPE_LINK:
            sendResult(clientInfo,id,FALSE,0,
                       "LINK %'S",
                       fileName
                      );
            break;
          default:
            sendResult(clientInfo,id,FALSE,0,
                       "unknown %'S",
                       fileName
                      );
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
  JobNode    *jobNode;
  const char *stateText;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Semaphore_lock(&jobList.lock);
  jobNode = jobList.head;
  while (jobNode != NULL)
  {
    switch (jobNode->runningInfo.state)
    {
      case JOB_STATE_WAITING:   stateText = "WAITING";   break;
      case JOB_STATE_RUNNING:   stateText = "RUNNING";   break;
      case JOB_STATE_COMPLETED: stateText = "COMPLETED"; break;
      case JOB_STATE_ERROR:     stateText = "ERROR";     break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    sendResult(clientInfo,id,FALSE,0,
               "%u %'S %s %llu %'s %'s %lu %lu",
               jobNode->id,
               jobNode->name,
               stateText,
               jobNode->options.archivePartSize,
               Compress_getAlgorithmName(jobNode->options.compressAlgorithm),
               Crypt_getAlgorithmName(jobNode->options.cryptAlgorithm),
               jobNode->runningInfo.startTime,
               jobNode->runningInfo.estimatedRestTime
              );

    jobNode = jobNode->next;
  }
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_jobInfo(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  uint       jobId;
  JobNode    *jobNode;
//  int        errorCode;
  const char *stateText;

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
  Semaphore_lock(&jobList.lock);

  /* find job */
  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }

  /* format result */
  if (jobNode != NULL)
  {
    switch (jobNode->runningInfo.state)
    {
      case JOB_STATE_WAITING:   stateText = "WAITING";   break;
      case JOB_STATE_RUNNING:   stateText = "RUNNING";   break;
      case JOB_STATE_COMPLETED: stateText = "COMPLETED"; break;
      case JOB_STATE_ERROR:     stateText = "ERROR";     break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    sendResult(clientInfo,id,TRUE,0,
               "%s %lu %llu %lu %llu %lu %llu %lu %llu %f %'S %llu %llu %'S %llu %llu",
               stateText,
               jobNode->runningInfo.doneFiles,
               jobNode->runningInfo.doneBytes,
               jobNode->runningInfo.totalFiles,
               jobNode->runningInfo.totalBytes,
               jobNode->runningInfo.skippedFiles,
               jobNode->runningInfo.skippedBytes,
               jobNode->runningInfo.errorFiles,
               jobNode->runningInfo.errorBytes,
               jobNode->runningInfo.compressionRatio,
               jobNode->runningInfo.fileName,
               jobNode->runningInfo.fileDoneBytes,
               jobNode->runningInfo.fileTotalBytes,
               jobNode->runningInfo.storageName,
               jobNode->runningInfo.storageDoneBytes,
               jobNode->runningInfo.storageTotalBytes
              );
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"job %d not found",jobId);
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_clear(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  UNUSED_VARIABLE(arguments);
  UNUSED_VARIABLE(argumentCount);

  Pattern_clearList(&clientInfo->includePatternList);
  Pattern_clearList(&clientInfo->excludePatternList);
  freeOptions(&clientInfo->options);
  copyOptions(&defaultOptions,&clientInfo->options); 

  sendResult(clientInfo,id,TRUE,0,"");
}

LOCAL void serverCommand_addIncludePattern(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get include pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern type");
    return;
  }
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern");
    return;
  }

  if      (String_equalsCString(arguments[0],"GLOB"))
  {
    Pattern_appendList(&clientInfo->includePatternList,String_cString(arguments[1]),PATTERN_TYPE_GLOB);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"REGEX"))
  {
    Pattern_appendList(&clientInfo->includePatternList,String_cString(arguments[1]),PATTERN_TYPE_REGEX);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"EXTENDED_REGEX"))
  {
    Pattern_appendList(&clientInfo->includePatternList,String_cString(arguments[1]),PATTERN_TYPE_EXTENDED_REGEX);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown pattern type");
  }
}

LOCAL void serverCommand_addExcludePattern(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get exclude pattern */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected pattern");
    return;
  }

  if      (String_equalsCString(arguments[0],"GLOB"))
  {
    Pattern_appendList(&clientInfo->excludePatternList,String_cString(arguments[1]),PATTERN_TYPE_GLOB);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"REGEX"))
  {
    Pattern_appendList(&clientInfo->excludePatternList,String_cString(arguments[1]),PATTERN_TYPE_REGEX);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"EXTENDED_REGEX"))
  {
    Pattern_appendList(&clientInfo->excludePatternList,String_cString(arguments[1]),PATTERN_TYPE_EXTENDED_REGEX);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown pattern type");
  }
}

LOCAL void serverCommand_get(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get config value name */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config value name");
    return;
  }

  if      (String_equalsCString(arguments[0],"archive-part-size"))
  {
    sendResult(clientInfo,id,TRUE,0,"%llu",clientInfo->options.archivePartSize);
  }
  else if (String_equalsCString(arguments[0],"max-tmp-size"))
  {
    sendResult(clientInfo,id,TRUE,0,"%llu",clientInfo->options.maxTmpSize);
  }
  else if (String_equalsCString(arguments[0],"max-band-width"))
  {
    sendResult(clientInfo,id,TRUE,0,"%'S",clientInfo->options.maxBandWidth);
  }
  else if (String_equalsCString(arguments[0],"compress-algorithm"))
  {
    sendResult(clientInfo,id,TRUE,0,"%'s",Compress_getAlgorithmName(clientInfo->options.compressAlgorithm));
  }
  else if (String_equalsCString(arguments[0],"crypt-algorithm"))
  {
    sendResult(clientInfo,id,TRUE,0,"%'s",Crypt_getAlgorithmName(clientInfo->options.cryptAlgorithm));
  }
  else if (String_equalsCString(arguments[0],"skip-unreadable"))
  {
    sendResult(clientInfo,id,TRUE,0,"%d",clientInfo->options.skipUnreadableFlag?1:0);
  }
  else if (String_equalsCString(arguments[0],"overwrite-archives"))
  {
    sendResult(clientInfo,id,TRUE,0,"%d",clientInfo->options.overwriteArchiveFilesFlag?1:0);
  }
  else if (String_equalsCString(arguments[0],"overwrite-files"))
  {
    sendResult(clientInfo,id,TRUE,0,"%d",clientInfo->options.overwriteFilesFlag?1:0);
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown config value");
  }
}

LOCAL void serverCommand_set(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get config value name */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config name");
    return;
  }
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected config value");
    return;
  }

  if      (String_equalsCString(arguments[0],"archive-part-size"))
  {
    // archive-part-size <n>
    const StringUnit UNITS[] = {{"K",1024},{"M",1024*1024},{"G",1024*1024*1024}};

    clientInfo->options.archivePartSize = String_toInteger64(arguments[1],0,NULL,UNITS,SIZE_OF_ARRAY(UNITS));
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"max-tmp-size"))
  {
    // max-tmp-part-size <n>
    const StringUnit UNITS[] = {{"K",1024},{"M",1024*1024},{"G",1024*1024*1024}};

    clientInfo->options.maxTmpSize = String_toInteger64(arguments[1],0,NULL,UNITS,SIZE_OF_ARRAY(UNITS));
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"max-band-width"))
  {
    // max-tmp-part-size <n>
    const StringUnit UNITS[] = {{"K",1024}};

    clientInfo->options.maxBandWidth = String_toInteger(arguments[1],0,NULL,UNITS,SIZE_OF_ARRAY(UNITS));
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"ssh-port"))
  {
    // ssh-port <n>
    clientInfo->options.sshPort = String_toInteger(arguments[1],0,NULL,NULL,0);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"ssh-public-key"))
  {
    // ssh-public-key <file name>
    String_set(clientInfo->options.sshPublicKeyFileName,arguments[1]);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"ssh-prvat-key"))
  {
    // ssh-privat-key <file name>
    String_set(clientInfo->options.sshPrivatKeyFileName,arguments[1]);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"compress-algorithm"))
  {
    // compress-algorithm <name>
    CompressAlgorithms compressAlgorithm;

    compressAlgorithm = Compress_getAlgorithm(String_cString(arguments[1]));
    if (compressAlgorithm == COMPRESS_ALGORITHM_UNKNOWN)
    {
      sendResult(clientInfo,id,TRUE,1,"unknown compress algorithm");
      return;
    }
    clientInfo->options.compressAlgorithm = compressAlgorithm;
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"crypt-algorithm"))
  {
    // crypt-algorithm <name>
    CryptAlgorithms cryptAlgorithm;

    cryptAlgorithm = Crypt_getAlgorithm(String_cString(arguments[1]));
    if (cryptAlgorithm == CRYPT_ALGORITHM_UNKNOWN)
    {
      sendResult(clientInfo,id,TRUE,1,"unknown crypt algorithm");
      return;
    }
    clientInfo->options.cryptAlgorithm = cryptAlgorithm;
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"crypt-password"))
  {
    // crypt-password <password>
    Password_set(clientInfo->options.cryptPassword,arguments[1]);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"skip-unreadable"))
  {
    // skip-unreadable 1|0
    clientInfo->options.skipUnreadableFlag = String_toBoolean(arguments[1],0,NULL,NULL,0,NULL,0);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"overwrite-archive-files"))
  {
    // overwrite-archive-files 1|0
    clientInfo->options.overwriteArchiveFilesFlag = String_toBoolean(arguments[1],0,NULL,NULL,0,NULL,0);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else if (String_equalsCString(arguments[0],"overwrite-files"))
  {
    // overwrite-files 1|0
    clientInfo->options.overwriteFilesFlag = String_toBoolean(arguments[1],0,NULL,NULL,0,NULL,0);
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"unknown config value '%S'",arguments[0]);
  }
}

LOCAL void serverCommand_addJob(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(arguments != NULL);

  /* get archive name */
  if (argumentCount < 1)
  {
    sendResult(clientInfo,id,TRUE,1,"expected name");
    return;
  }
  if (argumentCount < 2)
  {
    sendResult(clientInfo,id,TRUE,1,"expected archive name");
    return;
  }

  /* create new job */
  jobNode = newJob();
  if (jobNode == NULL)
  {
    sendResult(clientInfo,id,TRUE,1,"insufficient memory");
    return;
  }
  jobNode->name            = String_copy(arguments[0]);
  jobNode->id              = getNewJobId();
  jobNode->archiveFileName = String_copy(arguments[1]);
  Pattern_copyList(&clientInfo->includePatternList,&jobNode->includePatternList);
  Pattern_copyList(&clientInfo->excludePatternList,&jobNode->excludePatternList);
  copyOptions(&clientInfo->options,&jobNode->options);

  /* lock */
  Semaphore_lock(&jobList.lock);

  /* clean-up job list */
  while (List_count(&jobList) >= MAX_JOBS_IN_LIST)
  {
    jobNode = jobList.head;
    while ((jobNode != NULL) && (jobNode->runningInfo.state != JOB_STATE_COMPLETED))
    {
      jobNode = jobNode->next;
    }
    if (jobNode != NULL)
    {
      List_remove(&jobList,jobNode);
    }
  }

  /* add new job to list */
  List_append(&jobList,jobNode);

  /* unlock */
  Semaphore_unlock(&jobList.lock);

  sendResult(clientInfo,id,TRUE,0,"%d",jobNode->id);
}

LOCAL void serverCommand_abortJob(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  JobNode *jobNode;
  uint    jobId;

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
  Semaphore_lock(&jobList.lock);

  /* find job */
  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }

  if (jobNode != NULL)
  {
    /* check if job running, remove job in list */
    jobNode->runningInfo.abortRequestFlag = TRUE;
    while (jobNode->runningInfo.state == JOB_STATE_RUNNING)
    {
      Semaphore_waitModified(&jobList.lock);
    }
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"job %d not found",jobId);
  }

  /* unlock */
  Semaphore_unlock(&jobList.lock);
}

LOCAL void serverCommand_remJob(ClientInfo *clientInfo, uint id, const String arguments[], uint argumentCount)
{
  JobNode *jobNode;
  uint    jobId;

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
  Semaphore_lock(&jobList.lock);

  /* find job */
  jobNode = jobList.head;
  while ((jobNode != NULL) && (jobNode->id != jobId))
  {
    jobNode = jobNode->next;
  }
  if (jobNode != NULL)
  {
    /* remove job in list if not running */
    if (jobNode->runningInfo.state != JOB_STATE_RUNNING)
    {
      List_remove(&jobList,jobNode);
    }
    sendResult(clientInfo,id,TRUE,0,"");
  }
  else
  {
    sendResult(clientInfo,id,TRUE,1,"job %d not found",jobId);
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
                       &clientInfo->options
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

          /* open archive lin */
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

          /* open archive lin */
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

const struct
{
  const char            *name;
  const char            *parameters;
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
}
SERVER_COMMANDS[] =
{
  { "AUTHORIZE",           "S",   serverCommand_authorize,         AUTHORIZATION_STATE_WAITING }, 
  { "DEVICE_LIST",         "",    serverCommand_deviceList,        AUTHORIZATION_STATE_OK      }, 
  { "FILE_LIST",           "S",   serverCommand_fileList,          AUTHORIZATION_STATE_OK      },
  { "JOB_LIST",            "",    serverCommand_jobList,           AUTHORIZATION_STATE_OK      },
  { "JOB_INFO",            "i",   serverCommand_jobInfo,           AUTHORIZATION_STATE_OK      },
  { "CLEAR",               "",    serverCommand_clear,             AUTHORIZATION_STATE_OK      },
  { "ADD_INCLUDE_PATTERN", "S",   serverCommand_addIncludePattern, AUTHORIZATION_STATE_OK      },
  { "ADD_EXCLUDE_PATTERN", "S",   serverCommand_addExcludePattern, AUTHORIZATION_STATE_OK      },
  { "GET",                 "s",   serverCommand_get,               AUTHORIZATION_STATE_OK      },
  { "SET",                 "s S", serverCommand_set,               AUTHORIZATION_STATE_OK      },
  { "ADD_JOB",             "",    serverCommand_addJob,            AUTHORIZATION_STATE_OK      },
  { "REM_JOB",             "i",   serverCommand_remJob,            AUTHORIZATION_STATE_OK      },
  { "ABORT_JOB",           "i",   serverCommand_abortJob,          AUTHORIZATION_STATE_OK      },
  { "ARCHIVE_LIST",        "S",   serverCommand_archiveList,       AUTHORIZATION_STATE_OK      },
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
fprintf(stderr,"%s,%d: unknown %s\n",__FILE__,__LINE__,String_cString(token));
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  commandMsg->serverCommandFunction = SERVER_COMMANDS[z].serverCommandFunction;
  commandMsg->authorizationState    = SERVER_COMMANDS[z].authorizationState;

  /* get arguments */
  commandMsg->arguments = Array_new(sizeof(String),0);
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    argument = String_copy(token);
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
  Crypt_randomize(sessionId,sizeof(SessionId));
//  memset(sessionId,0,sizeof(SessionId));
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
  copyOptions(&defaultOptions,&clientInfo->options);

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
  clientInfo->network.name         = String_copy(name);
  clientInfo->network.port         = port;
  clientInfo->network.socketHandle = socketHandle;
  clientInfo->network.exitFlag     = FALSE;

  Pattern_initList(&clientInfo->includePatternList);
  Pattern_initList(&clientInfo->excludePatternList);
  copyOptions(&defaultOptions,&clientInfo->options);

  if (!MsgQueue_init(&clientInfo->network.commandMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise client command message queue!");
  }
  if (pthread_create(&clientInfo->network.threadId,NULL,(void*(*)(void*))networkClientThread,clientInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise client thread!");
  }

  /* create and send session id */
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
  assert(clientInfo != NULL);

  switch (clientInfo->type)
  {
    case CLIENT_TYPE_FILE:
      break;
    case CLIENT_TYPE_NETWORK:
      /* stop client thread */
      clientInfo->network.exitFlag = TRUE;
      MsgQueue_setEndOfMsg(&clientInfo->network.commandMsgQueue);
      pthread_join(clientInfo->network.threadId,NULL);

      /* free resources */
      MsgQueue_done(&clientInfo->network.commandMsgQueue,(MsgQueueMsgFreeFunction)freeCommandMsg,NULL);
      String_delete(clientInfo->network.name);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  freeOptions(&clientInfo->options);
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
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
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

Errors Server_run(uint       serverPort,
                  uint       serverTLSPort,
                  const char *caFileName,
                  const char *certFileName,
                  const char *keyFileName,
                  Password   *serverPassword
                 )
{
  Errors             error;
  ServerSocketHandle serverSocketHandle,serverTLSSocketHandle;
  fd_set             selectSet;
  ClientNode         *clientNode;
  SocketHandle       socketHandle;
  String             name;
  uint               port;
  char               buffer[256];
  ulong              receivedBytes;
  ulong              z;
  ClientNode         *deleteClientNode;

  assert((serverPort != 0) || (serverTLSPort != 0));

  /* initialise variables */
  password = serverPassword;
  List_init(&jobList);
  List_init(&clientList);
  quitFlag = FALSE;

  /* init server sockets */
  if (serverPort != 0)
  {
    error = Network_initServer(&serverSocketHandle,
                               serverPort,
                               SERVER_TYPE_PLAIN,
                               NULL,
                               NULL,
                               NULL
                              );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize server (error: %s)!\n",
                 getErrorText(error)
                );
      return FALSE;
    }
  }
  if (serverTLSPort != 0)
  {
    #ifdef HAVE_GNU_TLS
      error = Network_initServer(&serverTLSSocketHandle,
                                 serverTLSPort,
                                 SERVER_TYPE_TLS,
                                 caFileName,
                                 certFileName,
                                 keyFileName
                                );
      if (error != ERROR_NONE)
      {
        printError("Cannot initialize SSL/TLS server (error: %s)!\n",
                   getErrorText(error)
                  );
        Network_doneServer(&serverSocketHandle);
        return FALSE;
      }
  #else /* not HAVE_GNU_TLS */
    printError("SSL/TLS server is not supported!\n");
    Network_doneServer(&serverSocketHandle);
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GNU_TLS */
  }

  /* start job-thread */
  if (pthread_create(&jobThreadId,NULL,(void*(*)(void*))jobThread,&jobList) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise job thread!");
  }

  /* run server */
  name = String_new();
  while (!quitFlag)
  {
    /* wait for command */
    FD_ZERO(&selectSet);
    if (serverPort    != 0) FD_SET(Network_getServerSocket(&serverSocketHandle),   &selectSet);
    if (serverTLSPort != 0) FD_SET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet);
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      FD_SET(Network_getSocket(&clientNode->clientInfo.network.socketHandle),&selectSet);
      clientNode = clientNode->next;
    }
    select(FD_SETSIZE,&selectSet,NULL,NULL,NULL);

    /* connect new clients */
    if ((serverPort != 0) && FD_ISSET(Network_getServerSocket(&serverSocketHandle),&selectSet))
    {
      error = Network_accept(&socketHandle,
                             &serverSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,name,&port);
        clientNode = newClient();
        assert(clientNode != NULL);
        initNetworkClient(&clientNode->clientInfo,name,port,socketHandle);
        List_append(&clientList,clientNode);

        info(1,"Connected client '%s:%u'\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
      }
      else
      {
        printError("Cannot establish client connection (error: %s)!\n",
                   getErrorText(error)
                  );
      }
    }
    if ((serverTLSPort != 0) && FD_ISSET(Network_getServerSocket(&serverTLSSocketHandle),&selectSet))
    {
      error = Network_accept(&socketHandle,
                             &serverTLSSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,name,&port);
        clientNode = newClient();
        assert(clientNode != NULL);
        initNetworkClient(&clientNode->clientInfo,name,port,socketHandle);
        List_append(&clientList,clientNode);

        info(1,"Connected client '%s:%u' (TLS/SSL)\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
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
          info(1,"Disconnected client '%s:%u'\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);

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
        info(1,"Disconnected client '%s:%u': authorization failure\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);

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
  String_delete(name);

  /* done server */
  if (serverPort    != 0) Network_doneServer(&serverSocketHandle);
  if (serverTLSPort != 0) Network_doneServer(&serverTLSSocketHandle);

  /* free resources */
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
String_setCString(commandString,"1 SET crypt-password 'xaver !45'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN REGEX test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"4 ARCHIVE_LIST scp:torsten@it-zo.de:backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */
  String_delete(commandString);

  /* free resources */
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
