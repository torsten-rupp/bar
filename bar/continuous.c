/***********************************************************************\
*
* $Revision: 4195 $
* $Date: 2015-10-17 10:41:02 +0200 (Sat, 17 Oct 2015) $
* $Author: torsten $
* Contents: continuous functions
* Systems: all
*
\***********************************************************************/

#define __CONTINUOUS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_INOTIFY_H
  #include <sys/inotify.h>
#endif
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/dictionaries.h"
#include "common/msgqueues.h"
#include "common/misc.h"
#include "errors.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "entrylists.h"
#include "common/patternlists.h"

#include "continuous.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define LOG_PREFIX "CONTINUOUS"

#ifdef HAVE_IN_EXCL_UNLINK
  #define NOTIFY_FLAGS (IN_EXCL_UNLINK)
#else
  #define NOTIFY_FLAGS 0
#endif
//#define NOTIFY_EVENTS IN_ALL_EVENTS
#define NOTIFY_EVENTS (IN_CREATE|IN_MODIFY|IN_ATTRIB|IN_CLOSE_WRITE|IN_DELETE|IN_DELETE_SELF|IN_MODIFY|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO)

#define PROC_MAX_NOTIFY_WATCHES_FILENAME   "/proc/sys/fs/inotify/max_user_watches"
#define PROC_MAX_NOTIFY_INSTANCES_FILENAME "/proc/sys/fs/inotify/max_user_instances"

#define MIN_NOTIFY_WATCHES_WARNING   (64*1024)
#define MIN_NOTIFY_INSTANCES_WARNING 256

#define CONTINUOUS_DATABASE_TIMEOUT (30L*MS_PER_SECOND)      // database timeout [ms]

// database version
#define CONTINUOUS_VERSION 1

#define CONTINUOUS_TABLE_DEFINITION \
  "CREATE TABLE IF NOT EXISTS meta(" \
  "  name  TEXT UNIQUE," \
  "  value TEXT" \
  ");" \
  "INSERT OR IGNORE INTO meta (name,value) VALUES ('version',1);" \
  "INSERT OR IGNORE INTO meta (name,value) VALUES ('datetime',DATETIME('now'));" \
  "" \
  "CREATE TABLE IF NOT EXISTS names(" \
  "  id           INTEGER PRIMARY KEY," \
  "  dateTime     INTEGER DEFAULT 0," \
  "  jobUUID      TEXT NOT NULL," \
  "  scheduleUUID TEXT NOT NULL," \
  "  name         TEXT NOT NULL," \
  "  storedFlag   INTEGER DEFAULT 0," \
  "  UNIQUE (jobUUID,scheduleUUID,name) " \
  ");" \
  "CREATE INDEX IF NOT EXISTS namesIndex ON names (jobUUID,scheduleUUID,name);"

/***************************** Datatypes *******************************/

// job/schedule UUID list
typedef struct UUIDNode
{
  LIST_NODE_HEADER(struct UUIDNode);

  char         jobUUID[MISC_UUID_STRING_LENGTH+1];
  char         scheduleUUID[MISC_UUID_STRING_LENGTH+1];
  ScheduleTime beginTime,endTime;
  bool         cleanFlag;
} UUIDNode;

typedef struct
{
  LIST_HEADER(UUIDNode);
} UUIDList;

// notify info
typedef struct
{
  UUIDList uuidList;
  int      watchHandle;
  String   name;
} NotifyInfo;

// init notify message
typedef struct
{
  enum
  {
    INIT,
    DONE
  } type;
  String       name;
  char         jobUUID[MISC_UUID_STRING_LENGTH+1];
  char         scheduleUUID[MISC_UUID_STRING_LENGTH+1];
  ScheduleTime beginTime,endTime;
  EntryList    entryList;
} InitNotifyMsg;

/***************************** Variables *******************************/
LOCAL bool              initFlag = FALSE;
LOCAL DatabaseSpecifier *continuousDatabaseSpecifier = NULL;
LOCAL Semaphore         notifyLock;                  // lock
LOCAL Dictionary        notifyHandles;
LOCAL Dictionary        notifyNames;
LOCAL MsgQueue          initDoneNotifyMsgQueue;
LOCAL Thread            continuousInitDoneThread;
LOCAL Thread            continuousThread;
LOCAL bool              quitFlag;

#if   defined(PLATFORM_LINUX)
  LOCAL int             inotifyHandle;
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

/****************************** Macros *********************************/

#define IS_INOTIFY(mask,event) (((mask) & (event)) == (event))

#ifndef NDEBUG
  #define createContinuous(...) __createContinuous(__FILE__,__LINE__, ## __VA_ARGS__)
  #define openContinuous(...)   __openContinuous  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeContinuous(...)  __closeContinuous (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeUUIDNode
* Purpose: free UUID node
* Input  : uuidNode - UUID node
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeUUIDNode(UUIDNode *uuidNode, void *userData)
{
  UNUSED_VARIABLE(uuidNode);
  UNUSED_VARIABLE(userData);
}

#ifndef NDEBUG
//TODO
#ifndef WERROR
/***********************************************************************\
* Name   : printNotifies
* Purpose: print notifies
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printNotifies(void)
{
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  const NotifyInfo   *notifyInfo;
  const UUIDNode     *uuidNode;

  printf("Notifies:\n");
  SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
    while (Dictionary_getNext(&dictionaryIterator,
                              NULL,  // keyData,
                              NULL,  // keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;

      printf("%3d: %s\n",
             notifyInfo->watchHandle,
             String_cString(notifyInfo->name)
            );
      LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
      {
        printf("     %s %s, %2d:%2d-%2d:%2d\n",
               uuidNode->jobUUID,
               uuidNode->scheduleUUID,
               uuidNode->beginTime.hour,
               uuidNode->beginTime.minute,
               uuidNode->endTime.hour,
               uuidNode->endTime.minute
              );
      }
    }
    Dictionary_doneIterator(&dictionaryIterator);
  }
}
#endif
#endif // NDEBUG

/***********************************************************************\
* Name   : openContinuous
* Purpose: open continuous database
* Input  : databaseHandle    - database handle variable
*          databaseSpecifier - database specifier
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: always create new
#ifdef NDEBUG
  LOCAL Errors openContinuous(DatabaseHandle          *databaseHandle,
                              const DatabaseSpecifier *databaseSpecifier
                             )
#else /* not NDEBUG */
  LOCAL Errors __openContinuous(const char              *__fileName__,
                                ulong                   __lineNb__,
                                DatabaseHandle          *databaseHandle,
                                const DatabaseSpecifier *databaseSpecifier
                               )
#endif /* NDEBUG */
{
  DatabaseOpenModes databaseOpenMode;
  Errors            error;

  assert(databaseHandle != NULL);
  assert(databaseSpecifier != NULL);
  assert(databaseSpecifier->type == DATABASE_TYPE_SQLITE3);

  // open continuous database
  databaseOpenMode = 0;
  if (String_isEmpty(databaseSpecifier->sqlite.fileName))
  {
    databaseOpenMode |= DATABASE_OPEN_MODE_MEMORY|DATABASE_OPEN_MODE_SHARED;
  }
  #ifdef NDEBUG
    error = Database_open(databaseHandle,
                          databaseSpecifier,
                          "continuous.db",
                          DATABASE_OPEN_MODE_READWRITE|databaseOpenMode,
                          CONTINUOUS_DATABASE_TIMEOUT
                         );
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,
                            databaseHandle,
                            databaseSpecifier,
                            "continuous.db",
                            DATABASE_OPEN_MODE_READWRITE|databaseOpenMode,
                            CONTINUOUS_DATABASE_TIMEOUT
                           );
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      error = Database_open(databaseHandle,
                            databaseSpecifier,
                            "continuous.db",
                            DATABASE_OPEN_MODE_CREATE|databaseOpenMode,
                            CONTINUOUS_DATABASE_TIMEOUT
                           );
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,
                              databaseHandle,
                              databaseSpecifier,
                              "continuous.db",
                              DATABASE_OPEN_MODE_CREATE|databaseOpenMode,
                              CONTINUOUS_DATABASE_TIMEOUT
                             );
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createContinuous
* Purpose: create empty continuous database
* Input  : databaseHandle - database handle variable
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createContinuous(DatabaseHandle          *databaseHandle,
                                const DatabaseSpecifier *databaseSpecifier
                               )
#else /* not NDEBUG */
  LOCAL Errors __createContinuous(const char              *__fileName__,
                                  ulong                   __lineNb__,
                                  DatabaseHandle          *databaseHandle,
                                  const DatabaseSpecifier *databaseSpecifier
                                 )
#endif /* NDEBUG */
{
  DatabaseOpenModes databaseOpenMode;
  Errors            error;

  assert(databaseHandle != NULL);
  assert(databaseSpecifier != NULL);

  // discard existing continuous database, create new database
  (void)Database_drop(databaseSpecifier,NULL);

  // create continuous database
  databaseOpenMode = 0;
  if (String_isEmpty(databaseSpecifier->sqlite.fileName))
  {
    databaseOpenMode |= DATABASE_OPEN_MODE_MEMORY|DATABASE_OPEN_MODE_SHARED;
  }
  #ifdef NDEBUG
    error = Database_open(databaseHandle,
                          databaseSpecifier,
                          NULL,  // databaseName
                          DATABASE_OPEN_MODE_CREATE|databaseOpenMode,
                          CONTINUOUS_DATABASE_TIMEOUT
                         );
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,
                            databaseHandle,
                            databaseSpecifier,
                            NULL,  // databaseName
                            DATABASE_OPEN_MODE_CREATE|databaseOpenMode,
                            CONTINUOUS_DATABASE_TIMEOUT
                           );
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  // create tables
  error = Database_execute(databaseHandle,
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           CONTINUOUS_TABLE_DEFINITION,
                           DATABASE_PARAMETERS_NONE
                          );
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      Database_close(databaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,databaseHandle);
    #endif /* NDEBUG */
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeContinuous
* Purpose: close continuous database
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors closeContinuous(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  LOCAL Errors __closeContinuous(const char     *__fileName__,
                                 ulong          __lineNb__,
                                 DatabaseHandle *databaseHandle
                                )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);

  #ifdef NDEBUG
    Database_close(databaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,databaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getContinuousVersion
* Purpose: get continuous version
* Input  : databaseSpecifier - database specifier
* Output : continuousVersion - continuous version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getContinuousVersion(uint *continuousVersion, const DatabaseSpecifier *databaseSpecifier)
{
  Errors         error;
  DatabaseHandle databaseHandle;

  assert(continuousVersion != NULL);
  assert(databaseSpecifier != NULL);
  assert(databaseSpecifier->type == DATABASE_TYPE_SQLITE3);

  (*continuousVersion) = 0;

  // open continuous database
  error = openContinuous(&databaseHandle,databaseSpecifier);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getUInt(&databaseHandle,
                           continuousVersion,
                           "meta",
                           "value",
                           "name='version'",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    (void)closeContinuous(&databaseHandle);
    return error;
  }

  // close continuous database
  (void)closeContinuous(&databaseHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getMaxNotifyWatches
* Purpose: get max. number of notify watches
* Input  : -
* Output : -
* Return : max. number of watches or 0
* Notes  : -
\***********************************************************************/

LOCAL ulong getMaxNotifyWatches(void)
{
  ulong maxNotifies;
  FILE  *handle;
  char  line[256];

  maxNotifies = 0;

  handle = fopen(PROC_MAX_NOTIFY_WATCHES_FILENAME,"r");
  if (handle != NULL)
  {
    if (fgets(line,sizeof(line),handle) != NULL)
    {
      maxNotifies = (ulong)atol(line);
    }
    fclose(handle);
  }

  return maxNotifies;
}

/***********************************************************************\
* Name   : getMaxNotifyInstances
* Purpose: get max. number of notify instances
* Input  : -
* Output : -
* Return : max. number of instances or 0
* Notes  : -
\***********************************************************************/

LOCAL ulong getMaxNotifyInstances(void)
{
  ulong maxNotifies;
  FILE  *handle;
  char  line[256];

  maxNotifies = 0;

  handle = fopen(PROC_MAX_NOTIFY_INSTANCES_FILENAME,"r");
  if (handle != NULL)
  {
    if (fgets(line,sizeof(line),handle) != NULL)
    {
      maxNotifies = (ulong)atol(line);
    }
    fclose(handle);
  }

  return maxNotifies;
}

/***********************************************************************\
* Name   : freeNotifyInfo
* Purpose: free file notify info
* Input  : notifyInfo - file notify info
*          userData   - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeNotifyInfo(NotifyInfo *notifyInfo, void *userData)
{
  assert(notifyInfo != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(notifyInfo->name);
  List_done(&notifyInfo->uuidList);
}

/***********************************************************************\
* Name   : getNotifyInfo
* Purpose: get file notify by watch handle
* Input  : watchHandle - watch handle
* Output : -
* Return : file notify info or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL NotifyInfo *getNotifyInfo(int watchHandle)
{
  NotifyInfo *notifyInfo;

  assert(Semaphore_isLocked(&notifyLock));

  if (!Dictionary_find(&notifyHandles,
                       &watchHandle,
                       sizeof(watchHandle),
                       (void**)&notifyInfo,
                       NULL
                      )
     )
  {
    notifyInfo = NULL;
  }

  return notifyInfo;
}

/***********************************************************************\
* Name   : getNotifyInfoByDirectory
* Purpose: get file notify by job/schedule UUID
* Input  : directory - directory
* Output : -
* Return : notify info or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL NotifyInfo *getNotifyInfoByDirectory(ConstString directory)
{
  NotifyInfo *notifyInfo;

  assert(Semaphore_isLocked(&notifyLock));

  if (!Dictionary_find(&notifyNames,
                       String_cString(directory),
                       String_length(directory),
                       (void**)&notifyInfo,
                       NULL
                      )
     )
  {
    notifyInfo = NULL;
  }

  return notifyInfo;
}

/***********************************************************************\
* Name   : addNotify
* Purpose: add notify for directory
* Input  : directory - directory
* Output : -
* Return : file notify or NULL on error
* Notes  : -
\***********************************************************************/

LOCAL NotifyInfo *addNotify(ConstString name)
{
  int        watchHandle;
  NotifyInfo *notifyInfo;

  assert(name != NULL);
  assert(Semaphore_isLocked(&notifyLock));

  notifyInfo = getNotifyInfoByDirectory(name);
  if (notifyInfo == NULL)
  {
    // create notify
    #if   defined(PLATFORM_LINUX)
      watchHandle = inotify_add_watch(inotifyHandle,String_cString(name),NOTIFY_FLAGS|NOTIFY_EVENTS);
      if (watchHandle == -1)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_CONTINUOUS,
                    LOG_PREFIX,"Add notify watch for '%s' fail (error: %s)",
                    String_cString(name),strerror(errno)
                   );
        return NULL;
      }
    #elif defined(PLATFORM_WINDOWS)
//TODO: NYI
      return NULL;
    #endif /* PLATFORM_... */

    // init notify
    notifyInfo = (NotifyInfo*)malloc(sizeof(NotifyInfo));
    if (notifyInfo == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    notifyInfo->watchHandle = watchHandle;
    notifyInfo->name        = String_duplicate(name);
    List_init(&notifyInfo->uuidList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeUUIDNode,NULL));

    // add notify
    Dictionary_add(&notifyHandles,
                   &notifyInfo->watchHandle,sizeof(notifyInfo->watchHandle),
                   notifyInfo,sizeof(NotifyInfo*)
                  );
    Dictionary_add(&notifyNames,
                   String_cString(notifyInfo->name),String_length(notifyInfo->name),
                   notifyInfo,sizeof(NotifyInfo*)
                  );

    DEBUG_ADD_RESOURCE_TRACE(notifyInfo,NotifyInfo);
  }

  return notifyInfo;
}

/***********************************************************************\
* Name   : deleteNotify
* Purpose: remove and delete notify for directory
* Input  : notifyInfo - file notify
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteNotify(NotifyInfo *notifyInfo)
{
  assert(notifyInfo != NULL);

  assert(Semaphore_isLocked(&notifyLock));

  DEBUG_REMOVE_RESOURCE_TRACE(notifyInfo,NotifyInfo);

  // remove notify watch
  #if   defined(PLATFORM_LINUX)
    (void)inotify_rm_watch(inotifyHandle,notifyInfo->watchHandle);
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // remove notify
  Dictionary_remove(&notifyNames,
                    String_cString(notifyInfo->name),
                    String_length(notifyInfo->name)
                   );
  Dictionary_remove(&notifyHandles,
                    &notifyInfo->watchHandle,
                    sizeof(notifyInfo->watchHandle)
                   );

  // free resources
  freeNotifyInfo(notifyInfo,NULL);
  free(notifyInfo);
}

/***********************************************************************\
* Name   : addNotifySubDirectories
* Purpose: add notify for directorty and all sub-directories
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
*          name         - file or directory to add
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addNotifySubDirectories(const char  *jobUUID,
                                   const char  *scheduleUUID,
                                   ScheduleTime beginTime,
                                   ScheduleTime endTime,
                                   ConstString  baseName
                                  )
{
  StringList          directoryList;
  String              name;
  Errors              error;
  FileInfo            fileInfo;
  DirectoryListHandle directoryListHandle;
  NotifyInfo          *notifyInfo;
  UUIDNode            *uuidNode;

  // init variables
  StringList_init(&directoryList);
  name = String_new();

  StringList_append(&directoryList,baseName);
  while (   !StringList_isEmpty(&directoryList)
         && !quitFlag
        )
  {
    // get next entry to process
    StringList_removeLast(&directoryList,name);

    if (globalOptions.ignoreNoBackupFileFlag || !hasNoBackup(name))
    {
//fprintf(stderr,"%s, %d: name=%s %d\n",__FILE__,__LINE__,String_cString(name),StringList_count(&directoryList));
      // read file info
      error = File_getInfo(&fileInfo,name);
      if (error != ERROR_NONE)
      {
//TODO: log?
        continue;
      }

      // update/add notify
      SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get/add notify
        notifyInfo = addNotify(name);
        if (notifyInfo == NULL)
        {
          Semaphore_unlock(&notifyLock);
          break;
        }

        // update/add uuid
        uuidNode = LIST_FIND(&notifyInfo->uuidList,
                             uuidNode,
                                stringEquals(uuidNode->jobUUID,jobUUID)
                             && stringEquals(uuidNode->scheduleUUID,scheduleUUID)
                            );
        if (uuidNode == NULL)
        {
          uuidNode = LIST_NEW_NODE(UUIDNode);
          if (uuidNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          List_append(&notifyInfo->uuidList,uuidNode);
        }
        stringSet(uuidNode->jobUUID,sizeof(uuidNode->jobUUID),jobUUID);
        stringSet(uuidNode->scheduleUUID,sizeof(uuidNode->scheduleUUID),scheduleUUID);
        uuidNode->beginTime = beginTime;
        uuidNode->endTime   = endTime;
        uuidNode->cleanFlag = FALSE;
      }
      if (notifyInfo == NULL)
      {
        continue;
      }

      // scan sub-directories
      if (fileInfo.type == FILE_TYPE_DIRECTORY)
      {
        // open directory content

        error = File_openDirectoryList(&directoryListHandle,name);
        if (error == ERROR_NONE)
        {
          // read directory content
          while (   !File_endOfDirectoryList(&directoryListHandle)
                 && !quitFlag
                )
          {
            // read next directory entry
            error = File_readDirectoryList(&directoryListHandle,name);
            if (error != ERROR_NONE)
            {
//TODO: log?
              continue;
            }

            if (globalOptions.ignoreNoBackupFileFlag || !hasNoBackup(name))
            {
              // read file info
              error = File_getInfo(&fileInfo,name);
              if (error != ERROR_NONE)
              {
//TODO: log?
                continue;
              }

              // add sub-directory to name list
              if (fileInfo.type == FILE_TYPE_DIRECTORY)
              {
                StringList_append(&directoryList,name);
              }
            }
          }

          // close directory
          File_closeDirectoryList(&directoryListHandle);
        }
        else
        {
//TODO: log?
        }
      }
    }
  }

  // free resources
  String_delete(name);
  StringList_done(&directoryList);
}

/***********************************************************************\
* Name   : deleteNotifySubDirectories
* Purpose: remove and delete notify for directorty and all
*          sub-directories
* Input  : name - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteNotifySubDirectories(ConstString name)
{
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;

  assert(name != NULL);
  assert(Semaphore_isLocked(&notifyLock));

  Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
  while (Dictionary_getNext(&dictionaryIterator,
                            NULL,  // keyData,
                            NULL,  // keyLength,
                            &data,
                            &length
                           )
        )
  {
    assert(data != NULL);
    assert(length == sizeof(NotifyInfo*));

    notifyInfo = (NotifyInfo*)data;

    if (   String_length(notifyInfo->name) >= String_length(name)
        && String_startsWith(notifyInfo->name,name)
       )
    {
      deleteNotify(notifyInfo);
    }
  }
  Dictionary_doneIterator(&dictionaryIterator);
}

/***********************************************************************\
* Name   : markNotifies
* Purpose: mark notifies for clean
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void markNotifies(const char *jobUUID, const char *scheduleUUID)
{
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
    while (Dictionary_getNext(&dictionaryIterator,
                              NULL,  // keyData,
                              NULL,  // keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;

      LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
      {
        if (   stringEquals(uuidNode->jobUUID,jobUUID)
            && stringEquals(uuidNode->scheduleUUID,scheduleUUID)
           )
        {
//fprintf(stderr,"%s, %d: mark %s %s: %s\n",__FILE__,__LINE__,uuidNode->jobUUID,uuidNode->scheduleUUID,String_cString(((NotifyInfo*)data)->directory));
          uuidNode->cleanFlag = TRUE;
        }
      }
    }
    Dictionary_doneIterator(&dictionaryIterator);
  }
}

/***********************************************************************\
* Name   : cleanNotifies
* Purpose: clean obsolete notifies for job and schedule
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanNotifies(const char *jobUUID, const char *scheduleUUID)
{
  DictionaryIterator dictionaryIterator;
  NotifyInfo         *notifyInfo;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
    while (Dictionary_getNext(&dictionaryIterator,
                              &keyData,
                              &keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;

      // remove uuids
      uuidNode = notifyInfo->uuidList.head;
      while (uuidNode != NULL)
      {
        if (   uuidNode->cleanFlag
            && stringEquals(uuidNode->jobUUID,jobUUID)
            && stringEquals(uuidNode->scheduleUUID,scheduleUUID)
           )
        {
          uuidNode = List_removeAndFree(&notifyInfo->uuidList,uuidNode);
        }
        else
        {
          uuidNode = uuidNode->next;
        }
      }

      // delete notify if no more uuids
      if (List_isEmpty(&notifyInfo->uuidList))
      {
        deleteNotify(notifyInfo);
      }
      else
      {
        break;
      }
    }
    Dictionary_doneIterator(&dictionaryIterator);
  }
}

/***********************************************************************\
* Name   : purgeNotifies
* Purpose: purge notifies for job and schedule
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeNotifies(const char *jobUUID, const char *scheduleUUID)
{
  DictionaryIterator dictionaryIterator;
  NotifyInfo         *notifyInfo;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
    while (Dictionary_getNext(&dictionaryIterator,
                              &keyData,
                              &keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;

      // remove uuids
      uuidNode = notifyInfo->uuidList.head;
      while (uuidNode != NULL)
      {
        if (   stringEquals(uuidNode->jobUUID,jobUUID)
            && (   (scheduleUUID == NULL)
                || stringEquals(uuidNode->scheduleUUID,scheduleUUID)
               )
           )
        {
          uuidNode = List_removeAndFree(&notifyInfo->uuidList,uuidNode);
        }
        else
        {
          uuidNode = uuidNode->next;
        }
      }

      // remove notify if no more uuids
      if (List_isEmpty(&notifyInfo->uuidList))
      {
        deleteNotify(notifyInfo);
      }
    }
    Dictionary_doneIterator(&dictionaryIterator);
  }
}

/***********************************************************************\
* Name   : freeInitNotifyMsg
* Purpose: free init notify msg
* Input  : initNotifyMsg - init notify message
*          userData      - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeInitNotifyMsg(InitNotifyMsg *initNotifyMsg, void *userData)
{
  assert(initNotifyMsg != NULL);

  UNUSED_VARIABLE(userData);

  switch (initNotifyMsg->type)
  {
    case INIT:
      EntryList_done(&initNotifyMsg->entryList);
      break;
    case DONE:
      break;
  }
  String_delete(initNotifyMsg->name);
}

/***********************************************************************\
* Name   : continuousInitDoneThreadCode
* Purpose: continuous init/done thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void continuousInitDoneThreadCode(void)
{
  StringList      nameList;
  String          baseName;
  ulong           maxWatches;//,maxInstances;
  InitNotifyMsg   initNotifyMsg;
  EntryNode       *includeEntryNode;
  StringTokenizer fileNameTokenizer;
  ConstString     token;

  // init variables
  StringList_init(&nameList);
  baseName = String_new();

  maxWatches = getMaxNotifyWatches();
  while (   !quitFlag
         && MsgQueue_get(&initDoneNotifyMsgQueue,&initNotifyMsg,NULL,sizeof(initNotifyMsg),WAIT_FOREVER)
        )
  {
    switch (initNotifyMsg.type)
    {
      case INIT:
//fprintf(stderr,"%s, %d: INIT job=%s schedule=%s time=%02d:%02d..%02d:%02d\n",__FILE__,__LINE__,initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID,initNotifyMsg.beginTime.hour,initNotifyMsg.beginTime.minute,initNotifyMsg.endTime.hour,initNotifyMsg.endTime.minute);
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_CONTINUOUS,
                    LOG_PREFIX,"Start initialize watches for '%s'",
                    String_cString(initNotifyMsg.name)
                   );

        // mark notifies for update or clean
        markNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);

        // add notify for include directories
        LIST_ITERATEX(&initNotifyMsg.entryList,includeEntryNode,!quitFlag)
        {
          // find base path
          File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
          if (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
          {
            if (!String_isEmpty(token))
            {
              File_setFileName(baseName,token);
            }
            else
            {
              File_getSystemDirectory(baseName,FILE_SYSTEM_PATH_ROOT,NULL);
            }
          }
          while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
          {
            File_appendFileName(baseName,token);
          }
          File_doneSplitFileName(&fileNameTokenizer);

          // add directory and sub-directories to notify
          addNotifySubDirectories(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID,initNotifyMsg.beginTime,initNotifyMsg.endTime,baseName);
        }

        // clean not existing notifies for job
        cleanNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);

        plogMessage(NULL,  // logHandle
                    LOG_TYPE_CONTINUOUS,
                    LOG_PREFIX,"Done initialize watches for '%s': %lu (max. %lu)",
                    String_cString(initNotifyMsg.name),
                    Dictionary_count(&notifyHandles),
                    maxWatches
                   );
        break;
      case DONE:
        purgeNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
        break;
    }

    // free init notify message
    freeInitNotifyMsg(&initNotifyMsg,NULL);
  }

  // free resources
  String_delete(baseName);
  StringList_done(&nameList);
}

/***********************************************************************\
* Name   : addEntry
* Purpose: add continuous entry to database
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
*          name           - entry name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addEntry(DatabaseHandle *databaseHandle,
                      const char     *jobUUID,
                      const char     *scheduleUUID,
                      ConstString    name
                     )
{
  Errors error;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);
  assert(name != NULL);
//  assert(Database_isLocked(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE));

  // purge all stored entries
//fprintf(stderr,"%s, %d: --- jobUUID=%s scheduleUUID=%s delta=%d\n",__FILE__,__LINE__,jobUUID,scheduleUUID,globalOptions.continuousMinTimeDelta);
//Continuous_dumpEntries(databaseHandle,jobUUID,scheduleUUID);
  error = Database_delete(databaseHandle,
                          NULL,  // changedRowCount
                          "names",
                          DATABASE_FLAG_NONE,
                          "    storedFlag=TRUE \
                           AND (NOW()-?)>=UNIX_TIMESTAMP(dateTime) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT  (globalOptions.continuousMinTimeDelta)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // add entry (if not already exists)
  error = Database_insert(databaseHandle,
                          NULL,  // insertRowId
                          "names",
                          DATABASE_FLAG_IGNORE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_CSTRING ("jobUUID",     jobUUID),
                            DATABASE_VALUE_CSTRING ("scheduleUUID",scheduleUUID),
                            DATABASE_VALUE_STRING  ("name",        name)
                          ),
                          DATABASE_COLUMNS_NONE,
                          DATABASE_FILTERS_NONE
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
//Continuous_dumpEntries(databaseHandle,jobUUID,scheduleUUID);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : removeEntry
* Purpose: remove continuous entry from database
* Input  : databaseHandle - database handle
*          databaseId     - database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeEntry(DatabaseHandle *databaseHandle,
                         DatabaseId     databaseId
                        )
{
//  assert(Database_isLocked(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE));

  return Database_delete(databaseHandle,
                         NULL,  // changedRowCount
                         "names",
                         DATABASE_FLAG_NONE,
                         "id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(databaseId)
                         ),
                         DATABASE_UNLIMITED
                        );
}

/***********************************************************************\
* Name   : markEntryStored
* Purpose: mark continuous entry stored
* Input  : databaseHandle - database handle
*          databaseId     - database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors markEntryStored(DatabaseHandle *databaseHandle,
                             DatabaseId     databaseId
                            )
{
//  assert(Database_isLocked(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE));

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         "names",
                         DATABASE_FLAG_NONE,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE     ("dateTime",  "NOW()"),
                           DATABASE_VALUE_BOOL("storedFlag",TRUE)
                         ),
                         "id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(databaseId)
                         )
                        );
}

/***********************************************************************\
* Name   : existsEntry
* Purpose: check if entry exists in database and is still not marked
*          as stored
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
*          name           - entry name
* Output : -
* Return : TRUE iff non-stored entry exists
* Notes  : -
\***********************************************************************/

LOCAL bool existsEntry(DatabaseHandle *databaseHandle,
                       const char     *jobUUID,
                       const char     *scheduleUUID,
                       ConstString    name
                      )
{
  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);
  assert(name != NULL);
//  assert(Database_isLocked(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE));

// TODO: lock required?
//  BLOCK_DOX(result,
//            Database_lock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout),
//            Database_unlock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//  {
  return Database_existsValue(databaseHandle,
                              "names",
                              DATABASE_FLAG_NONE,
                              "id",
                              "    storedFlag=FALSE \
                               AND jobUUID=? \
                               AND scheduleUUID=? \
                               AND name=? \
                              ",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_CSTRING(jobUUID),
                                DATABASE_FILTER_CSTRING(scheduleUUID),
                                DATABASE_FILTER_STRING (name)
                              )
                             );
}

/***********************************************************************\
* Name   : continuousThreadCode
* Purpose: continuous file thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void continuousThreadCode(void)
{
#if   defined(PLATFORM_LINUX)
  #define TIMEOUT     (5*MS_PER_SECOND)
  #define MAX_ENTRIES 128
  #define BUFFER_SIZE (MAX_ENTRIES*(sizeof(struct inotify_event)+NAME_MAX+1))

  void                       *buffer;
  String                     absoluteName;
  Errors                     error;
  DatabaseHandle             databaseHandle;
  SignalMask                 signalMask;
  ssize_t                    n;
  uint                       currentHour,currentMinute;
  const struct inotify_event *inotifyEvent;
  const NotifyInfo           *notifyInfo;
  const UUIDNode             *uuidNode;

  // init variables
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  absoluteName = String_new();

  // open continuous database
  error = Continuous_open(&databaseHandle);
  if (error != ERROR_NONE)
  {
    printError("cannot initialise continuous database (error: %s)!",
               Error_getText(error)
              );
    String_delete(absoluteName);
    free(buffer);
    return;
  }

  // Note: ignore SIGALRM in poll()/pselect()
  MISC_SIGNAL_MASK_CLEAR(signalMask);
  #ifdef HAVE_SIGALRM
    MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
  #endif /* HAVE_SIGALRM */

  while (!quitFlag)
  {
    // read inotify events
    n = 0;
    do
    {
      // wait for event or timeout
      if ((Misc_waitHandle(inotifyHandle,&signalMask,HANDLE_EVENT_INPUT,TIMEOUT) & HANDLE_EVENT_INPUT) != 0)
      {
        n = read(inotifyHandle,buffer,BUFFER_SIZE);
      }
    }
    while ((n == -1) && ((errno == EAGAIN) || (errno == EINTR)) && !quitFlag);
    if (quitFlag) break;

    // process inotify events
    Misc_splitDateTime(Misc_getCurrentDateTime(),
                       NULL,  // year
                       NULL,  // month
                       NULL,  // day
                       &currentHour,
                       &currentMinute,
                       NULL,  // second
                       NULL,  // weekDay
                       NULL  // isDayLightSaving
                      );
    inotifyEvent = (const struct inotify_event*)buffer;
    while ((n > 0) && !quitFlag)
    {
#if 0
fprintf(stderr,"%s, %d: inotify event wd=%d mask=%08x: name=%s ->",__FILE__,__LINE__,inotifyEvent->wd,inotifyEvent->mask,inotifyEvent->name);
   if (inotifyEvent->mask & IN_ACCESS)        fprintf(stderr," IN_ACCESS"       );
   if (inotifyEvent->mask & IN_ATTRIB)        fprintf(stderr," IN_ATTRIB"       );
   if (inotifyEvent->mask & IN_CLOSE_NOWRITE) fprintf(stderr," IN_CLOSE_NOWRITE");
   if (inotifyEvent->mask & IN_CLOSE_WRITE)   fprintf(stderr," IN_CLOSE_WRITE"  );
   if (inotifyEvent->mask & IN_CREATE)        fprintf(stderr," IN_CREATE"       );
   if (inotifyEvent->mask & IN_DELETE)        fprintf(stderr," IN_DELETE"       );
   if (inotifyEvent->mask & IN_DELETE_SELF)   fprintf(stderr," IN_DELETE_SELF"  );
   if (inotifyEvent->mask & IN_IGNORED)       fprintf(stderr," IN_IGNORED"      );
   if (inotifyEvent->mask & IN_ISDIR)         fprintf(stderr," IN_ISDIR"        );
   if (inotifyEvent->mask & IN_MODIFY)        fprintf(stderr," IN_MODIFY"       );
   if (inotifyEvent->mask & IN_MOVE_SELF)     fprintf(stderr," IN_MOVE_SELF"    );
   if (inotifyEvent->mask & IN_MOVED_FROM)    fprintf(stderr," IN_MOVED_FROM"   );
   if (inotifyEvent->mask & IN_MOVED_TO)      fprintf(stderr," IN_MOVED_TO"     );
   if (inotifyEvent->mask & IN_OPEN)          fprintf(stderr," IN_OPEN"         );
   if (inotifyEvent->mask & IN_Q_OVERFLOW)    fprintf(stderr," IN_Q_OVERFLOW"   );
   if (inotifyEvent->mask & IN_UNMOUNT)       fprintf(stderr," IN_UNMOUNT"      );
fprintf(stderr,"\n");
#endif
      SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        notifyInfo = getNotifyInfo(inotifyEvent->wd);
        if (notifyInfo != NULL)
        {
          // get absolute name
          String_set(absoluteName,notifyInfo->name);

          if (inotifyEvent->len > 0) File_appendFileNameCString(absoluteName,inotifyEvent->name);

          if (IS_INOTIFY(inotifyEvent->mask,IN_ISDIR))
          {
            // directory changed
            if      (IS_INOTIFY(inotifyEvent->mask,IN_CREATE))
            {
              // add directory and sub-directories to notify
              LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
              {
                // store into notify database
                if (inTimeRange(currentHour,currentMinute,
                                uuidNode->beginTime.hour,uuidNode->beginTime.hour,
                                uuidNode->endTime.hour,uuidNode->endTime.hour
                               )
                   )
                {
                  if (!existsEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName))
                  {
                    error = addEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Marked for storage '%s'",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Store continuous entry fail (error: %s)",
                                  Error_getText(error)
                                 );
                    }
                  }
                }

                // add directory and sub-directories to notify
                addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,uuidNode->beginTime,uuidNode->endTime,absoluteName);
              }
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_DELETE))
            {
              // remove and delete directory and sub-directories from notify
              deleteNotifySubDirectories(absoluteName);
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_MOVED_FROM))
            {
              // remove and delete directory and sub-directories from notify
              deleteNotifySubDirectories(absoluteName);
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_MOVED_TO))
            {
              LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
              {
                // store into notify database
                if (inTimeRange(currentHour,currentMinute,
                                uuidNode->beginTime.hour,uuidNode->beginTime.hour,
                                uuidNode->endTime.hour,uuidNode->endTime.hour
                               )
                   )
                {
                  if (!existsEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName))
                  {
                    error = addEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Marked for storage '%s'",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Store continuous entry fail (error: %s)",
                                  Error_getText(error)
                                 );
                    }
                  }
                }

                // add directory and sub-directories to notify
                addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,uuidNode->beginTime,uuidNode->endTime,absoluteName);
              }
            }
            else
            {
              LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
              {
                // store into notify database
                if (inTimeRange(currentHour,currentMinute,
                                uuidNode->beginTime.hour,uuidNode->beginTime.hour,
                                uuidNode->endTime.hour,uuidNode->endTime.hour
                               )
                   )
                {
                  if (!existsEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName))
                  {
                    error = addEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Marked for storage '%s'",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Store continuous entry fail (error: %s)",
                                  Error_getText(error)
                                 );
                    }
                  }
                }
              }
            }
          }
          else
          {
            // file changed
            if      (IS_INOTIFY(inotifyEvent->mask,IN_DELETE))
            {
              // file deleted -> nothing to do
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_MOVED_FROM))
            {
              // file move away -> nothing to do
            }
            else
            {
              LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
              {
                if (inTimeRange(currentHour,currentMinute,
                                uuidNode->beginTime.hour,uuidNode->beginTime.hour,
                                uuidNode->endTime.hour,uuidNode->endTime.hour
                               )
                   )
                {
                  if (!existsEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName))
                  {
                    error = addEntry(&databaseHandle,uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Marked for storage '%s'",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  LOG_PREFIX,
                                  "Store continuous entry fail (error: %s)",
                                  Error_getText(error)
                                 );
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          #ifndef NDEBUG
            fprintf(stderr,"%s, %d: Warning: inotify event 0x%08x received for unknown handle %d!\n",__FILE__,__LINE__,inotifyEvent->mask,inotifyEvent->wd);
          #endif /* not NDEBUG */
        }
      }

      // next event
      n -= sizeof(struct inotify_event)+inotifyEvent->len;
      inotifyEvent = (const struct inotify_event*)((byte*)inotifyEvent+sizeof(struct inotify_event)+inotifyEvent->len);
    }
    assert(quitFlag || (n == 0));
  }

  // close continuous database
  Continuous_close(&databaseHandle);

  // free resources
  String_delete(absoluteName);
  free(buffer);
#elif defined(PLATFORM_WINDOWS)
//TODO: NYI
#endif /* PLATFORM_... */
}

/*---------------------------------------------------------------------*/

Errors Continuous_initAll(void)
{
  ulong n;

  // check number of possible notifies
  n = getMaxNotifyWatches();
  if (n < MIN_NOTIFY_WATCHES_WARNING) printWarning("low number of notify watches %lu. Please check settings in '%s'!",n,PROC_MAX_NOTIFY_WATCHES_FILENAME);
  n = getMaxNotifyInstances();
  if (n < MIN_NOTIFY_INSTANCES_WARNING) printWarning("low number of notify instances %lu. Please check settings in '%s'!",n,PROC_MAX_NOTIFY_INSTANCES_FILENAME);

  return ERROR_NONE;
}

void Continuous_doneAll(void)
{
}

bool Continuous_isAvailable(void)
{
  return initFlag;
}

Errors Continuous_init(const char *databaseURI)
{
  bool           createFlag;
  Errors         error;
  DatabaseHandle databaseHandle;
  uint           continuousVersion;

  // init variables
  quitFlag = FALSE;
  Semaphore_init(&notifyLock,SEMAPHORE_TYPE_BINARY);
  Dictionary_init(&notifyHandles,
                  CALLBACK_(NULL,NULL),  // dictionaryCopyFunction
                  CALLBACK_(NULL,NULL),  // dictionaryFreeFunction
                  CALLBACK_(NULL,NULL)  // dictionaryCompareFunction
                 );
  Dictionary_init(&notifyNames,
                  CALLBACK_(NULL,NULL),  // dictionaryCopyFunction
                  CALLBACK_(NULL,NULL),  // dictionaryFreeFunction
                  CALLBACK_(NULL,NULL)  // dictionaryCompareFunction
                 );

  // init inotify
  #if   defined(PLATFORM_LINUX)
    inotifyHandle = inotify_init();
    if (inotifyHandle == -1)
    {
      if (errno == EMFILE)
      {
        error = ERRORX_(INSUFFICIENT_FILE_NOTIFY,errno,"%s",strerror(errno));
      }
      else
      {
        error = ERRORX_(INIT_FILE_NOTIFY,errno,"%s",strerror(errno));
      }
      Dictionary_done(&notifyNames);
      Dictionary_done(&notifyHandles);
      Semaphore_done(&notifyLock);
      return error;
    }
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // init command queue
  if (!MsgQueue_init(&initDoneNotifyMsgQueue,
                     0,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeInitNotifyMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize init notify message queue!");
  }

  // get database specifier
  assert(continuousDatabaseSpecifier == NULL);
  continuousDatabaseSpecifier = Database_newSpecifier(databaseURI,"continuous.db",NULL);
  if (continuousDatabaseSpecifier == NULL)
  {
    MsgQueue_done(&initDoneNotifyMsgQueue);
    Dictionary_done(&notifyNames);
    Dictionary_done(&notifyHandles);
    Semaphore_done(&notifyLock);
    return ERROR_DATABASE;
  }
  if (continuousDatabaseSpecifier->type != DATABASE_TYPE_SQLITE3)
  {
    Database_deleteSpecifier(continuousDatabaseSpecifier);
    continuousDatabaseSpecifier = NULL;
    MsgQueue_done(&initDoneNotifyMsgQueue);
    Dictionary_done(&notifyNames);
    Dictionary_done(&notifyHandles);
    Semaphore_done(&notifyLock);
    return ERROR_DATABASE_NOT_SUPPORTED;
  }
  assert(continuousDatabaseSpecifier != NULL);

  createFlag = FALSE;
  if (Database_exists(continuousDatabaseSpecifier,NULL))
  {
    // check if continuous database exists in expected version, create database
    error = getContinuousVersion(&continuousVersion,continuousDatabaseSpecifier);
    if (error == ERROR_NONE)
    {
      if (continuousVersion != CONTINUOUS_VERSION)
      {
        // insufficient version -> create new
        createFlag = TRUE;
      }
    }
    else
    {
      // unknown version -> create new
      createFlag = TRUE;
    }
  }
  else
  {
    // does not exists -> create new
    createFlag = TRUE;
  }
  if (createFlag)
  {
    // create new database
    error = createContinuous(&databaseHandle,continuousDatabaseSpecifier);
    if (error == ERROR_NONE)
    {
      closeContinuous(&databaseHandle);
    }
    else
    {
      Database_deleteSpecifier(continuousDatabaseSpecifier);
      continuousDatabaseSpecifier = NULL;
      MsgQueue_done(&initDoneNotifyMsgQueue);
      Dictionary_done(&notifyNames);
      Dictionary_done(&notifyHandles);
      Semaphore_done(&notifyLock);
      return error;
    }
  }

  initFlag = TRUE;

  // start threads
  if (!Thread_init(&continuousInitDoneThread,"BAR continuous init",globalOptions.niceLevel,continuousInitDoneThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize continuous init thread!");
  }
  if (!Thread_init(&continuousThread,"BAR continuous",globalOptions.niceLevel,continuousThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize continuous thread!");
  }

  return ERROR_NONE;
}

void Continuous_done(void)
{
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;

  if (initFlag)
  {
    quitFlag = TRUE;
    MsgQueue_setEndOfMsg(&initDoneNotifyMsgQueue);
    if (!Thread_join(&continuousThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop continuous thread!");
    }
    Thread_done(&continuousThread);
    if (!Thread_join(&continuousInitDoneThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop continuous init thread!");
    }
    Thread_done(&continuousInitDoneThread);

    Database_deleteSpecifier(continuousDatabaseSpecifier);
    continuousDatabaseSpecifier = NULL;

    // done notify event message queue
    MsgQueue_done(&initDoneNotifyMsgQueue);

    #if   defined(PLATFORM_LINUX)
      assert(inotifyHandle != -1);
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    #if   defined(PLATFORM_LINUX)
      // close inotify
      close(inotifyHandle);
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    // remove inotifies
    Dictionary_initIterator(&dictionaryIterator,&notifyNames);
    while (Dictionary_getNext(&dictionaryIterator,
                              NULL,  // keyData,
                              NULL,  // keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;

      DEBUG_REMOVE_RESOURCE_TRACE(notifyInfo,NotifyInfo);

      // remove notify watch
      #if   defined(PLATFORM_LINUX)
        (void)inotify_rm_watch(inotifyHandle,notifyInfo->watchHandle);
      #elif defined(PLATFORM_WINDOWS)
      #endif /* PLATFORM_... */

      // free resources
      freeNotifyInfo(notifyInfo,NULL);
      free(notifyInfo);
    }
    Dictionary_doneIterator(&dictionaryIterator);

    // done dictionaries
    Dictionary_done(&notifyNames);
    Dictionary_done(&notifyHandles);

    Semaphore_done(&notifyLock);
  }
}

Errors Continuous_initNotify(ConstString     name,
                             ConstString     jobUUID,
                             ConstString     scheduleUUID,
                             ScheduleTime    beginTime,
                             ScheduleTime    endTime,
                             const EntryList *entryList
                            )
{
  InitNotifyMsg initNotifyMsg;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(entryList != NULL);

  if (initFlag)
  {
    initNotifyMsg.type      = INIT;
    initNotifyMsg.name      = String_duplicate(name);
    stringSet(initNotifyMsg.jobUUID,sizeof(initNotifyMsg.jobUUID),String_cString(jobUUID));
    stringSet(initNotifyMsg.scheduleUUID,sizeof(initNotifyMsg.scheduleUUID),String_cString(scheduleUUID));
    initNotifyMsg.beginTime = beginTime;
    initNotifyMsg.endTime   = endTime;
    EntryList_initDuplicate(&initNotifyMsg.entryList,entryList,NULL,NULL);

    (void)MsgQueue_put(&initDoneNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));

    return ERROR_NONE;
  }
  else
  {
    return ERROR_INIT_FILE_NOTIFY;
  }
}

Errors Continuous_doneNotify(ConstString name,
                             ConstString jobUUID,
                             ConstString scheduleUUID
                            )
{
  InitNotifyMsg initNotifyMsg;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  if (initFlag)
  {
    initNotifyMsg.type = DONE;
    initNotifyMsg.name = String_duplicate(name);
    stringSet(initNotifyMsg.jobUUID,sizeof(initNotifyMsg.jobUUID),String_cString(jobUUID));
    if (scheduleUUID != NULL)
    {
      stringSet(initNotifyMsg.scheduleUUID,sizeof(initNotifyMsg.scheduleUUID),String_cString(scheduleUUID));
    }
    else
    {
      stringClear(initNotifyMsg.scheduleUUID);
    }

    (void)MsgQueue_put(&initDoneNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));
  }

  return ERROR_NONE;
}

Errors Continuous_open(DatabaseHandle *databaseHandle)
{
  Errors error;
  uint   continuousVersion;

  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(continuousDatabaseSpecifier != NULL);
  assert(Database_exists(continuousDatabaseSpecifier,NULL));
  assert(   (getContinuousVersion(&continuousVersion,continuousDatabaseSpecifier) != ERROR_NONE)
         || (continuousVersion == CONTINUOUS_VERSION)
        );

  return openContinuous(databaseHandle,continuousDatabaseSpecifier);
}

void Continuous_close(DatabaseHandle *databaseHandle)
{
  assert(initFlag);
  assert(databaseHandle != NULL);

  (void)closeContinuous(databaseHandle);
}

Errors Continuous_addEntry(DatabaseHandle *databaseHandle,
                           ConstString    jobUUID,
                           ConstString    scheduleUUID,
                           ScheduleTime   beginTime,
                           ScheduleTime   endTime,
                           ConstString    name
                          )
{
  uint   currentHour,currentMinute;
  Errors error;

  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(!String_isEmpty(name));

  Misc_splitDateTime(Misc_getCurrentDateTime(),
                     NULL,  // year
                     NULL,  // month
                     NULL,  // day
                     &currentHour,
                     &currentMinute,
                     NULL,  // second
                     NULL,  // weekDay
                     NULL  // isDayLightSaving
                    );
  if (inTimeRange(currentHour,currentMinute,
                  beginTime.hour,beginTime.hour,
                  endTime.hour,endTime.hour
                 )
     )
  {
    error = addEntry(databaseHandle,String_cString(jobUUID),String_cString(scheduleUUID),name);
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

Errors Continuous_removeEntry(DatabaseHandle *databaseHandle,
                              DatabaseId     databaseId
                             )
{
  assert(initFlag);
  assert(databaseHandle != NULL);

  return removeEntry(databaseHandle,databaseId);
}

bool Continuous_getEntry(DatabaseHandle *databaseHandle,
                         const char     *jobUUID,
                         const char     *scheduleUUID,
                         DatabaseId     *databaseId,
                         String         name
                        )
{
  bool       result;
  DatabaseId databaseId_;

  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);
  assert(name != NULL);

  result = FALSE;

// TODO: lock required?
//  BLOCK_DOX(result,
//            Database_lock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout),
//            Database_unlock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//  {
    if (Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 2);

                       UNUSED_VARIABLE(userData);
                       UNUSED_VARIABLE(valueCount);

                       databaseId_ = values[0].id;
                       String_set(name,values[1].string);
// TODO: Database_get() return code to bool
result = TRUE;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "names"
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY   ("id"),
                       DATABASE_COLUMN_STRING("name")
                     ),
                     "    storedFlag=FALSE \
                      AND (NOW()-?)>=UNIX_TIMESTAMP(dateTime) \
                      AND jobUUID=? \
                      AND scheduleUUID=? \
                     ",
                     DATABASE_FILTERS
                     (
                       DATABASE_FILTER_UINT   (globalOptions.continuousMinTimeDelta),
                       DATABASE_FILTER_CSTRING(jobUUID),
                       DATABASE_FILTER_CSTRING(scheduleUUID)
                     ),
                     NULL,  // groupBy
                     NULL,  // orderBy
                     0LL,
                     1LL
                    ) != ERROR_NONE
       )
    {
      return FALSE;
    }

    // mark entry stored
    if (markEntryStored(databaseHandle,databaseId_) != ERROR_NONE)
    {
      return FALSE;
    }

//    return TRUE;
//  });

  if (databaseId != NULL) (*databaseId) = databaseId_;

  return result;
}

void Continuous_discardEntries(DatabaseHandle *databaseHandle,
                               const char     *jobUUID,
                               const char     *scheduleUUID
                              )
{
  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

// TODO: lock required?
//  BLOCK_DOX(result,
//            Database_lock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout),
//            Database_unlock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//  {
  Database_delete(databaseHandle,
                  NULL,  // changedRowCount
                  "names",
                  DATABASE_FLAG_NONE,
                  "    jobUUID=? \
                   AND scheduleUUID=? \
                  ",
                  DATABASE_FILTERS
                  (
                    DATABASE_FILTER_CSTRING(jobUUID),
                    DATABASE_FILTER_CSTRING(scheduleUUID)
                  ),
                  DATABASE_UNLIMITED
                 );

//    return TRUE;
//  });
}

bool Continuous_isEntryAvailable(DatabaseHandle *databaseHandle,
                                 ConstString    jobUUID,
                                 ConstString    scheduleUUID
                                )
{
  assert(databaseHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  return    initFlag
         && Database_existsValue(databaseHandle,
                                 "names",
                                 DATABASE_FLAG_NONE,
                                 "id",
                                 "    storedFlag=FALSE \
                                  AND (NOW()-?)>=UNIX_TIMESTAMP(dateTime) \
                                  AND jobUUID=? \
                                  AND scheduleUUID=? \
                                 ",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_UINT  (globalOptions.continuousMinTimeDelta),
                                   DATABASE_FILTER_STRING(jobUUID),
                                   DATABASE_FILTER_STRING(scheduleUUID),
                                 )
                                );
}

Errors Continuous_initList(DatabaseStatementHandle *databaseStatementHandle,
                           DatabaseHandle          *databaseHandle,
                           const char              *jobUUID,
                           const char              *scheduleUUID
                          )
{
  Errors error;

  assert(initFlag);
  assert(databaseStatementHandle != NULL);
  assert(databaseHandle != NULL);
  assert(!stringIsEmpty(jobUUID));

  // prepare list
  error = Database_select(databaseStatementHandle,
                          databaseHandle,
                          "names",
                          DATABASE_FLAG_NONE,
                          DATABASE_COLUMNS
                          (
                            DATABASE_COLUMN_KEY   ("id"),
                            DATABASE_COLUMN_STRING("name")
                          ),
                          "    storedFlag=FALSE \
                           AND (NOW()-?)>=UNIX_TIMESTAMP(dateTime) \
                           AND jobUUID=? \
                           AND scheduleUUID=? \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT   (globalOptions.continuousMinTimeDelta),
                            DATABASE_FILTER_CSTRING(jobUUID),
                            DATABASE_FILTER_CSTRING(scheduleUUID)
                          ),
                          NULL,  // groupBy
                          NULL,  // orderBy
                          0LL,
                          DATABASE_UNLIMITED
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Continuous_doneList(DatabaseStatementHandle *databaseStatementHandle)
{
  assert(initFlag);
  assert(databaseStatementHandle != NULL);

  Database_finalize(databaseStatementHandle);
}

bool Continuous_getNext(DatabaseStatementHandle *databaseStatementHandle,
                        DatabaseId              *databaseId,
                        String                  name
                       )
{
  assert(databaseStatementHandle != NULL);

  return Database_getNextRow(databaseStatementHandle,
                             databaseId,
                             name
                            );
}

#ifndef NDEBUG
void Continuous_dumpEntries(DatabaseHandle *databaseHandle,
                            const char     *jobUUID,
                            const char     *scheduleUUID
                           )
{
  uint64     dateTime;
  DatabaseId databaseId;
  String     name;
  uint       storedFlag;

  assert(databaseHandle != NULL);

  name = String_new();

  Database_get(databaseHandle,
               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
               {
                 assert(values != NULL);
                 assert(valueCount == 4);

                 UNUSED_VARIABLE(userData);
                 UNUSED_VARIABLE(valueCount);

                 databaseId = values[0].id;
                 dateTime   = values[1].dateTime;
                 String_set(name,values[2].string);
                 storedFlag = values[3].b;

                 printf("#%ld: %lu %s %d\n",databaseId,dateTime,String_cString(name),storedFlag);

                 return ERROR_NONE;
               },NULL),
               NULL,  // changedRowCount
               DATABASE_TABLES
               (
                 "names"
               ),
               DATABASE_FLAG_NONE,
               DATABASE_COLUMNS
               (
                 DATABASE_COLUMN_KEY     ("id"),
                 DATABASE_COLUMN_DATETIME("dateTime"),
                 DATABASE_COLUMN_STRING  ("name"),
                 DATABASE_COLUMN_BOOL    ("storedFlag")
               ),
               "    (? OR jobUUID=?) \
                AND (? OR scheduleUUID=?) \
               ",
               DATABASE_FILTERS
               (
                 DATABASE_FILTER_BOOL   (jobUUID == NULL),
                 DATABASE_FILTER_CSTRING(jobUUID),
                 DATABASE_FILTER_BOOL   (scheduleUUID == NULL),
                 DATABASE_FILTER_CSTRING(scheduleUUID)
               ),
               NULL,  // groupBy
               NULL,  // orderBy
               0LL,
               DATABASE_UNLIMITED
              );

  String_delete(name);
}

void Continuous_debugPrintStatistics(void)
{
//  uint               jobCount;
//  ulong              entryCount;
//  DictionaryIterator dictionaryIterator;
//  void               *data;
//  ulong              length;
//  const NotifyInfo   *notifyInfo;

//  jobCount   = 0;
//  entryCount = 0L;
  SEMAPHORE_LOCKED_DO(&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    fprintf(stderr,"DEBUG: %lu continuous entries\n",
            Dictionary_count(&notifyHandles)
           );
#if 0
    Dictionary_initIterator(&dictionaryIterator,&notifyHandles);
    while (Dictionary_getNext(&dictionaryIterator,
                              NULL,  // keyData,
                              NULL,  // keyLength,
                              &data,
                              &length
                             )
          )
    {
      assert(data != NULL);
      assert(length == sizeof(NotifyInfo*));

      notifyInfo = (NotifyInfo*)data;
      jobCount += 1;
      entryCount += List_count(&notifyInfo->uuidList);
    }
    Dictionary_doneIterator(&dictionaryIterator);
#endif
  }
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
