/***********************************************************************\
*
* $Revision: 4195 $
* $Date: 2015-10-17 10:41:02 +0200 (Sat, 17 Oct 2015) $
* $Author: torsten $
* Contents: continous functions
* Systems: all
*
\***********************************************************************/

#define __CONTINOUS_IMPLEMENTATION__

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

#include "entrylists.h"
#include "common/patternlists.h"
#include "bar_global.h"
#include "bar.h"

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

#define DATABASE_TIMEOUT (30L*MS_PER_SECOND)      // database timeout [ms]

// database version
#define CONTINOUS_VERSION 1

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
  "  UNIQUE (jobUUID,name) " \
  ");" \
  "CREATE INDEX IF NOT EXISTS namesIndex ON names (jobUUID,scheduleUUID,name);"

/***************************** Datatypes *******************************/

// job/schedule UUID list
typedef struct UUIDNode
{
  LIST_NODE_HEADER(struct UUIDNode);

  char   jobUUID[MISC_UUID_STRING_LENGTH+1];
  char   scheduleUUID[MISC_UUID_STRING_LENGTH+1];
  bool   cleanFlag;
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
  String name;
  char   jobUUID[MISC_UUID_STRING_LENGTH+1];
  char   scheduleUUID[MISC_UUID_STRING_LENGTH+1];
  EntryList entryList;
} InitNotifyMsg;

/***************************** Variables *******************************/
LOCAL bool              initFlag = FALSE;
LOCAL const char        *continuousDatabaseFileName;
LOCAL DatabaseOpenModes continuousDatabaseOpenMode;
LOCAL DatabaseHandle    continuousDatabaseHandle;
LOCAL Semaphore         notifyLock;                  // lock
LOCAL Dictionary        notifyHandles;
LOCAL Dictionary        notifyNames;
LOCAL MsgQueue          initDoneNotifyMsgQueue;
LOCAL Thread            continuousInitThread;
LOCAL Thread            continuousThread;
LOCAL bool              quitFlag;

#if   defined(PLATFORM_LINUX)
  LOCAL int             inotifyHandle;
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

/****************************** Macros *********************************/

#define IS_INOTIFY(mask,event) (((mask) & (event)) == (event))

#ifndef NDEBUG
  #define openContinuous(...)   __openContinuous  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define createContinuous(...) __createContinuous(__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeContinuous(...)  __closeContinuous (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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
        printf("     %s %s\n",
               uuidNode->jobUUID,
               uuidNode->scheduleUUID
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
* Input  : databaseHandle - database handle variable
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: always create new
#ifdef NDEBUG
  LOCAL Errors openContinuous(DatabaseHandle *databaseHandle
                             )
#else /* not NDEBUG */
  LOCAL Errors __openContinuous(const char     *__fileName__,
                                ulong          __lineNb__,
                                DatabaseHandle *databaseHandle
                               )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);

  // open continuous database
  #ifdef NDEBUG
    error = Database_open(databaseHandle,
                          continuousDatabaseFileName,
                          continuousDatabaseOpenMode|DATABASE_OPENMODE_READWRITE,
                          DATABASE_TIMEOUT
                         );
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,
                            databaseHandle,
                            continuousDatabaseFileName,
                            continuousDatabaseOpenMode|DATABASE_OPENMODE_READWRITE,
                            DATABASE_TIMEOUT
                           );
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      error = Database_open(databaseHandle,
                            continuousDatabaseFileName,
                            DATABASE_OPENMODE_CREATE,
                            DATABASE_TIMEOUT
                           );
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,
                              databaseHandle,
                              continuousDatabaseFileName,
                              DATABASE_OPENMODE_CREATE,
                              DATABASE_TIMEOUT
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
  LOCAL Errors createContinuous(DatabaseHandle *databaseHandle
                               )
#else /* not NDEBUG */
  LOCAL Errors __createContinuous(const char     *__fileName__,
                                  ulong          __lineNb__,
                                  DatabaseHandle *databaseHandle
                                 )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);

  // create continuous database
  if (!stringIsEmpty(continuousDatabaseFileName)) (void)File_deleteCString(continuousDatabaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(databaseHandle,
                          continuousDatabaseFileName,
                          continuousDatabaseOpenMode|DATABASE_OPENMODE_CREATE,
                          DATABASE_TIMEOUT
                         );
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,
                            databaseHandle,
                            continuousDatabaseFileName,
                            continuousDatabaseOpenMode|DATABASE_OPENMODE_CREATE,
                            DATABASE_TIMEOUT
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
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           CONTINUOUS_TABLE_DEFINITION
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
* Input  : -
* Output : continuousVersion - continuous version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getContinuousVersion(int64 *continuousVersion)
{
  Errors         error;
  DatabaseHandle databaseHandle;

  // open continuous database
  error = openContinuous(&databaseHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getInteger64(&databaseHandle,
                                continuousVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
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
  List_done(&notifyInfo->uuidList,CALLBACK_(NULL,NULL));
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
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : file notify info or NULL if not found
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
* Name   : freeNotifyDictionary
* Purpose: free notify info dictionary entry
* Input  : data     - data
*          length   - length (not used)
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeNotifyDictionary(const void *data, ulong length, void *userData)
{
  NotifyInfo *notifyInfo = (NotifyInfo*)data;
  assert(notifyInfo != NULL);

  UNUSED_VARIABLE(length);
  UNUSED_VARIABLE(userData);

  freeNotifyInfo(notifyInfo,NULL);
  free(notifyInfo);
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
    List_init(&notifyInfo->uuidList);

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
* Name   : removeNotify
* Purpose: remove notify for directory
* Input  : notifyInfo - file notify
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void removeNotify(NotifyInfo *notifyInfo)
{
  assert(notifyInfo != NULL);

  assert(Semaphore_isLocked(&notifyLock));

  DEBUG_REMOVE_RESOURCE_TRACE(notifyInfo,NotifyInfo);

  // remove notify
  #if   defined(PLATFORM_LINUX)
    (void)inotify_rm_watch(inotifyHandle,notifyInfo->watchHandle);
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // delete notify
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

LOCAL void addNotifySubDirectories(const char *jobUUID, const char *scheduleUUID, ConstString baseName)
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
          stringSet(uuidNode->jobUUID,sizeof(uuidNode->jobUUID),jobUUID);
          stringSet(uuidNode->scheduleUUID,sizeof(uuidNode->scheduleUUID),scheduleUUID);
          List_append(&notifyInfo->uuidList,uuidNode);
        }
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
* Name   : removeNotifySubDirectories
* Purpose: remove notify for directorty and all sub-directories
* Input  : name - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void removeNotifySubDirectories(ConstString name)
{
  DictionaryIterator dictionaryIterator;
//  const void         *keyData;
//  ulong              keyLength;
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
      removeNotify(notifyInfo);
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
          uuidNode = List_removeAndFree(&notifyInfo->uuidList,uuidNode,CALLBACK_(NULL,NULL));
        }
        else
        {
          uuidNode = uuidNode->next;
        }
      }

      // remove notify if no more uuids
      if (List_isEmpty(&notifyInfo->uuidList))
      {
        removeNotify(notifyInfo);
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
* Name   : removeNotifies
* Purpose: remove notifies for job and schedule
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void removeNotifies(const char *jobUUID, const char *scheduleUUID)
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
          uuidNode = List_removeAndFree(&notifyInfo->uuidList,uuidNode,CALLBACK_(NULL,NULL));
        }
        else
        {
          uuidNode = uuidNode->next;
        }
      }

      // remove notify if no more uuids
      if (List_isEmpty(&notifyInfo->uuidList))
      {
        removeNotify(notifyInfo);
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
* Name   : continuousInitThreadCode
* Purpose: continuous file thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void continuousInitThreadCode(void)
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
//fprintf(stderr,"%s, %d: INIT job=%s schedule=%s\n",__FILE__,__LINE__,initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
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
              File_setFileNameChar(baseName,FILE_PATHNAME_SEPARATOR_CHAR);
            }
          }
          while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
          {
            File_appendFileName(baseName,token);
          }
          File_doneSplitFileName(&fileNameTokenizer);

          // add directory and sub-directories to notify
          addNotifySubDirectories(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID,baseName);
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
        removeNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
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
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DELETE FROM names \
                            WHERE     storedFlag=1 \
                                  AND DATETIME('now','-%u seconds')>=dateTime;",
                           globalOptions.continuousMinTimeDelta
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // add entry (if not already exists or
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "INSERT INTO names \
                              (\
                               jobUUID,\
                               scheduleUUID,\
                               name\
                              ) \
                            VALUES \
                              (\
                               %'s,\
                               %'s,\
                               %'S\
                              ) \
                            ON CONFLICT(jobUUID,name) DO UPDATE SET \
                              storedFlag=0 \
                           ",
                           jobUUID,
                           scheduleUUID,
                           name,
                           name
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

  return Database_execute(databaseHandle,
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          "DELETE FROM names WHERE id=%lld;",
                          databaseId
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

  return Database_execute(databaseHandle,
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          "UPDATE names SET dateTime=DATETIME('now'),storedFlag=1 WHERE id=%lld;",
                          databaseId
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
  #define TIMEOUT     250L
  #define MAX_ENTRIES 128
  #define BUFFER_SIZE (MAX_ENTRIES*(sizeof(struct inotify_event)+NAME_MAX+1))

  void                       *buffer;
  String                     absoluteName;
  Errors                     error;
  DatabaseHandle             databaseHandle;
  SignalMask                 signalMask;
  ssize_t                    n;
  const struct inotify_event *inotifyEvent;
  NotifyInfo                 *notifyInfo;
  UUIDNode                   *uuidNode;

  // init variables
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  absoluteName = String_new();

  // open continous database
  error = Continuous_open(&databaseHandle);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialise continuous database (error: %s)!",
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
    inotifyEvent = (const struct inotify_event*)buffer;
    while ((n > 0) && !quitFlag)
    {
//fprintf(stderr,"%s, %d: n=%d %d\n",__FILE__,__LINE__,n,inotifyEvent->len);
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

                // add directory and sub-directories to notify
                addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
              }
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_DELETE))
            {
              // remove directory and sub-directories from notify
              notifyInfo = getNotifyInfo(inotifyEvent->wd);
              removeNotifySubDirectories(absoluteName);
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_MOVED_FROM))
            {
              // remove directory and sub-directories from notify
              removeNotifySubDirectories(absoluteName);
            }
            else if (IS_INOTIFY(inotifyEvent->mask,IN_MOVED_TO))
            {
              // add directory and sub-directories to notify
//              BLOCK_DO(Database_lock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle.timeout),
//                       Database_unlock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//              {
                LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                {
                  // store into notify database
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

                  // add directory and sub-directories to notify
                  addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                }
//              });
            }
            else
            {
//              // add directory and sub-directories to notify
//              BLOCK_DO(Database_lock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle.timeout),
//                       Database_unlock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//              {
                LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                {
                  // store into notify database
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
//              });
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
              // file move or changed -> store into notify database
//              BLOCK_DO(Database_lock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle.timeout),
//                       Database_unlock(&databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//              {
                LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
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
//              });
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

  // close continous database
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
  // init variables
  Semaphore_init(&notifyLock,SEMAPHORE_TYPE_BINARY);
  Dictionary_init(&notifyHandles,
                  CALLBACK_(NULL,NULL),  // dictionaryCopyFunction
//TODO: test
                  CALLBACK_(freeNotifyDictionary,NULL),
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
      Dictionary_done(&notifyNames);
      Dictionary_done(&notifyHandles);
      Semaphore_done(&notifyLock);
      return ERROR_INIT_FILE_NOTIFY;
    }
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // init command queue
  if (!MsgQueue_init(&initDoneNotifyMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize init notify message queue!");
  }

  initFlag = TRUE;

  return ERROR_NONE;
}

void Continuous_doneAll(void)
{
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;

  if (initFlag)
  {
    // done notify event message queue
    MsgQueue_done(&initDoneNotifyMsgQueue,CALLBACK_((MsgQueueMsgFreeFunction)freeInitNotifyMsg,NULL));

    #if   defined(PLATFORM_LINUX)
      assert(inotifyHandle != -1);
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    #if   defined(PLATFORM_LINUX)
      // close inotify
      close(inotifyHandle);
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    // done dictionaries
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

      #if   defined(PLATFORM_LINUX)
        (void)inotify_rm_watch(inotifyHandle,notifyInfo->watchHandle);
      #elif defined(PLATFORM_WINDOWS)
      #endif /* PLATFORM_... */

      // free resources
      freeNotifyInfo(notifyInfo,NULL);
      free(notifyInfo);
    }
    Dictionary_doneIterator(&dictionaryIterator);
    Dictionary_done(&notifyNames);
    Dictionary_done(&notifyHandles);

    Semaphore_done(&notifyLock);
  }
}

bool Continuous_isAvailable(void)
{
  return initFlag;
}

Errors Continuous_init(const char *databaseFileName)
{
  Errors error;
  int64  continuousVersion;

  if (initFlag)
  {
    // init variables
    quitFlag = FALSE;

    // get continiuous database name, mode
    if (!stringIsEmpty(databaseFileName))
    {
      continuousDatabaseFileName = databaseFileName;
      continuousDatabaseOpenMode = DATABASE_OPENMODE_SHARED;
    }
    else
    {
      continuousDatabaseFileName = "continuous.db";
      continuousDatabaseOpenMode = DATABASE_OPENMODE_SHARED|DATABASE_OPENMODE_MEMORY;
    }

    // check if continuous database exists in expected version, create database
    error = getContinuousVersion(&continuousVersion);
    if (error == ERROR_NONE)
    {
      if (continuousVersion < CONTINOUS_VERSION)
      {
        // discard existing continuous database, create new database
        if (!stringIsEmpty(databaseFileName)) (void)File_deleteCString(databaseFileName,FALSE);
        error = createContinuous(&continuousDatabaseHandle);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      else
      {
        // open continuous database
        error = openContinuous(&continuousDatabaseHandle);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
    }
    else
    {
      // create new database
      error = createContinuous(&continuousDatabaseHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // start threads
    if (!Thread_init(&continuousInitThread,"BAR continuous init",globalOptions.niceLevel,continuousInitThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize continuous init thread!");
    }
    if (!Thread_init(&continuousThread,"BAR continuous",globalOptions.niceLevel,continuousThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize continuous thread!");
    }
  }
  else
  {
    return ERROR_INIT_FILE_NOTIFY;
  }

  return ERROR_NONE;
}

void Continuous_done(void)
{
  if (initFlag)
  {
    quitFlag = TRUE;
    MsgQueue_setEndOfMsg(&initDoneNotifyMsgQueue);
    if (!Thread_join(&continuousThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop continuous thread!");
    }
    Thread_done(&continuousThread);
    if (!Thread_join(&continuousInitThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop continuous init thread!");
    }
    Thread_done(&continuousInitThread);

    (void)closeContinuous(&continuousDatabaseHandle);
  }
}

Errors Continuous_initNotify(ConstString     name,
                             ConstString     jobUUID,
                             ConstString     scheduleUUID,
                             const EntryList *entryList
                            )
{
  ulong         n;
  InitNotifyMsg initNotifyMsg;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(entryList != NULL);

  if (initFlag)
  {
    // check number of possible notifies
    EXECUTE_ONCE(
    {
      n = getMaxNotifyWatches();
      if (n < MIN_NOTIFY_WATCHES_WARNING) printWarning("Low number of notify watches %lu. Please check settings in '%s'!",n,PROC_MAX_NOTIFY_WATCHES_FILENAME);
      n = getMaxNotifyInstances();
      if (n < MIN_NOTIFY_INSTANCES_WARNING) printWarning("Low number of notify instances %lu. Please check settings in '%s'!",n,PROC_MAX_NOTIFY_INSTANCES_FILENAME);
    });

    initNotifyMsg.type = INIT;
    initNotifyMsg.name = String_duplicate(name);
    stringSet(initNotifyMsg.jobUUID,sizeof(initNotifyMsg.jobUUID),String_cString(jobUUID));
    stringSet(initNotifyMsg.scheduleUUID,sizeof(initNotifyMsg.scheduleUUID),String_cString(scheduleUUID));
    EntryList_initDuplicate(&initNotifyMsg.entryList,entryList,NULL,NULL);

    (void)MsgQueue_put(&initDoneNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));
  }

  return ERROR_NONE;
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
//fprintf(stderr,"%s, %d: Continuous_doneNotify jobUUID=%s scheduleUUID=%s\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(scheduleUUID));
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

  assert(databaseHandle != NULL);

  if (initFlag)
  {
    error = openContinuous(databaseHandle);
  }
  else
  {
    error = ERROR_INIT_FILE_NOTIFY;
  }

  return error;
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
                           ConstString    name
                          )
{
  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(!String_isEmpty(name));

  return addEntry(databaseHandle,String_cString(jobUUID),String_cString(scheduleUUID),name);
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
                         ConstString    jobUUID,
                         ConstString    scheduleUUID,
                         DatabaseId     *databaseId,
                         String         name
                        )
{
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          databaseId_;

  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(jobUUID != NULL);
  assert(name != NULL);

//  BLOCK_DOX(result,
//            Database_lock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout),
//            Database_unlock(databaseHandle,SEMAPHORE_LOCK_TYPE_READ_WRITE),
//  {
    // prepare list
    if (Database_prepare(&databaseQueryHandle,
                         databaseHandle,
                         "SELECT id,name \
                          FROM names \
                          WHERE     storedFlag=0 \
                                AND DATETIME('now','-%u seconds')>=dateTime \
                                AND jobUUID=%'S \
                                AND scheduleUUID=%'S \
                          LIMIT 0,1 \
                         ",
                         globalOptions.continuousMinTimeDelta,
                         jobUUID,
                         scheduleUUID
                        ) != ERROR_NONE
       )
    {
      return FALSE;
    }

    // get next entry
    if (!Database_getNextRow(&databaseQueryHandle,
                             "%lld %S",
                             &databaseId_,
                             name
                            )
       )
    {
      Database_finalize(&databaseQueryHandle);
      return FALSE;
    }

    // done list
    Database_finalize(&databaseQueryHandle);

    // mark entry stored
    if (markEntryStored(databaseHandle,databaseId_) != ERROR_NONE)
    {
      return FALSE;
    }

//    return TRUE;
//  });
result = TRUE;

  if (databaseId != NULL) (*databaseId) = databaseId_;

  return result;
}

bool Continuous_isEntryAvailable(DatabaseHandle *databaseHandle,
                                 ConstString    jobUUID,
                                 ConstString    scheduleUUID
                                )
{
  assert(initFlag);
  assert(databaseHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  return Database_exists(databaseHandle,
                         "names",
                         "id",
                         "WHERE     DATETIME('now','-%u seconds')>=dateTime \
                                AND jobUUID=%'S \
                                AND scheduleUUID=%'S \
                                AND storedFlag=0 \
                         ",
                         globalOptions.continuousMinTimeDelta,
                         jobUUID,
                         scheduleUUID
                        );
}

Errors Continuous_initList(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseHandle      *databaseHandle,
                           ConstString         jobUUID,
                           ConstString         scheduleUUID
                          )
{
  Errors error;

  assert(initFlag);
  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  // prepare list
  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT id,name \
                            FROM names \
                            WHERE     storedFlag=0 \
                                  AND DATETIME('now','-%u seconds')>=dateTime \
                                  AND jobUUID=%'S \
                                  AND scheduleUUID=%'S \
                           ",
                           globalOptions.continuousMinTimeDelta,
                           jobUUID,
                           scheduleUUID
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Continuous_doneList(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(initFlag);
  assert(databaseQueryHandle != NULL);

  Database_finalize(databaseQueryHandle);
}

bool Continuous_getNext(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              name
                       )
{
  assert(initFlag);
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S",
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
  DatabaseQueryHandle databaseQueryHandle;
  uint64              dateTime;
  DatabaseId          databaseId;
  String              name;
  uint                storedFlag;

  name = String_new();

  Database_prepare(&databaseQueryHandle,
                         databaseHandle,
                         "SELECT id,UNIXTIMESTAMP(dateTime),name,storedFlag \
                          FROM names \
                          WHERE     jobUUID=%'s \
                                AND scheduleUUID=%'s \
                         ",
                         jobUUID,
                         scheduleUUID
                        );
  while (Database_getNextRow(&databaseQueryHandle,
                           "%lld %lld %S %d",
                           &databaseId,
                           &dateTime,
                           name,
                           &storedFlag
                          )
        )
  {
     fprintf(stderr,"%s, %d: %ld: %lu %s %d\n",__FILE__,__LINE__,databaseId,dateTime,String_cString(name),storedFlag);
  }
  Database_finalize(&databaseQueryHandle);

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
