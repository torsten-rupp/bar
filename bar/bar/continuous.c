/***********************************************************************\
*
* $Revision: 4195 $
* $Date: 2015-10-17 10:41:02 +0200 (Sat, 17 Oct 2015) $
* $Author: torsten $
* Contents: continous functions
* Systems: all
*
\***********************************************************************/

#define __CONTINOUS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <poll.h>
#ifdef HAVE_SYS_INOTIFY_H
  #include <sys/inotify.h>
#endif
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "threads.h"
#include "strings.h"
#include "files.h"
#include "database.h"
#include "arrays.h"
#include "dictionaries.h"
#include "msgqueues.h"
#include "misc.h"
#include "errors.h"

#include "entrylists.h"
#include "patternlists.h"
#include "bar_global.h"
#include "bar.h"

#include "continuous.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#ifdef HAVE_IN_EXCL_UNLINK
  #define NOTIFY_FLAGS (IN_EXCL_UNLINK|IN_ISDIR)
#else
  #define NOTIFY_FLAGS (IN_ISDIR)
#endif
//#define NOTIFY_EVENTS IN_ALL_EVENTS
#define NOTIFY_EVENTS (IN_CREATE|IN_MODIFY|IN_ATTRIB|IN_CLOSE_WRITE|IN_DELETE|IN_DELETE_SELF|IN_MODIFY|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO)

#define PROC_MAX_NOTIFIES_FILENAME "/proc/sys/fs/inotify/max_user_watches"
#define MIN_NOTIFIES_WARNING       (64*1024)


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
  "  jobUUID      TEXT NOT NULL," \
  "  scheduleUUID TEXT NOT NULL," \
  "  name         TEXT NOT NULL," \
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
  String   directory;
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
LOCAL Semaphore      notifyLock;                  // lock
LOCAL Dictionary     notifyHandles;
LOCAL Dictionary     notifyDirectories;
LOCAL DatabaseHandle continuousDatabaseHandle;
LOCAL int            inotifyHandle;
LOCAL MsgQueue       initDoneNotifyMsgQueue;
LOCAL Thread         continuousInitThread;
LOCAL Thread         continuousThread;
LOCAL bool           quitFlag;

/****************************** Macros *********************************/

#define IS_INOTIFY(mask,event) (((mask) & (event)) == (event))

#ifndef NDEBUG
  #define openContinuous(...)  __openContinuous(__FILE__,__LINE__, ## __VA_ARGS__)
  #define createContinuous(...) __createContinuous(__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeContinuous(...) __closeContinuous(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
//TODO
#ifndef WERROR
LOCAL void printNotifies(void)
{
  SemaphoreLock      semaphoreLock;
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  const NotifyInfo   *notifyInfo;
  const UUIDNode     *uuidNode;

  printf("Notifies:\n");
  SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
             String_cString(notifyInfo->directory)
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
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: always create new
#ifdef NDEBUG
  LOCAL Errors openContinuous(DatabaseHandle *databaseHandle,
                              const char     *databaseFileName
                             )
#else /* not NDEBUG */
  LOCAL Errors __openContinuous(const char     *__fileName__,
                                uint           __lineNb__,
                                DatabaseHandle *databaseHandle,
                                const char     *databaseFileName
                               )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseFileName != NULL);

  // open continuous database
  #ifdef NDEBUG
    error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,DATABASE_PRIORITY_HIGH,NO_WAIT);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,DATABASE_PRIORITY_HIGH,NO_WAIT);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createContinuous
* Purpose: create empty continuous database
* Input  : databaseFileName - database file name (can be NULL)
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createContinuous(DatabaseHandle *databaseHandle,
                                const char     *databaseFileName
                               )
#else /* not NDEBUG */
  LOCAL Errors __createContinuous(const char     *__fileName__,
                                  uint           __lineNb__,
                                  DatabaseHandle *databaseHandle,
                                  const char     *databaseFileName
                                 )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);

  // create continuous database
  if (databaseFileName != NULL) (void)File_deleteCString(databaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,DATABASE_PRIORITY_HIGH,NO_WAIT);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,DATABASE_PRIORITY_HIGH,NO_WAIT);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  // create tables
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                 uint           __lineNb__,
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
* Input  : databaseFileName - database file name
* Output : continuousVersion - continuous version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getContinuousVersion(const char *databaseFileName, int64 *continuousVersion)
{
  Errors         error;
  DatabaseHandle databaseHandle;

  // open continuous database
  error = openContinuous(&databaseHandle,databaseFileName);
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
* Name   : getMaxNotifies
* Purpose: get max. number of notifies
* Input  : -
* Output : -
* Return : max. number of notifies or 0
* Notes  : -
\***********************************************************************/

LOCAL ulong getMaxNotifies(void)
{
  ulong maxNotifies;
  FILE  *handle;
  char  line[256];

  maxNotifies = 0;

  handle = fopen(PROC_MAX_NOTIFIES_FILENAME,"r");
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

  List_done(&notifyInfo->uuidList,CALLBACK_NULL);
  String_delete(notifyInfo->directory);
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

  if (!Dictionary_find(&notifyDirectories,
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

LOCAL NotifyInfo *addNotify(ConstString directory)
{
  int        watchHandle;
  NotifyInfo *notifyInfo;

  assert(directory != NULL);
  assert(Semaphore_isLocked(&notifyLock));

  notifyInfo = getNotifyInfoByDirectory(directory);
  if (notifyInfo == NULL)
  {
    // create notify
    watchHandle = inotify_add_watch(inotifyHandle,String_cString(directory),NOTIFY_FLAGS|NOTIFY_EVENTS);
    if (watchHandle == -1)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_CONTINUOUS,
                  "CONTINUOUS","Add notify watch for '%s' fail (error: %s)\n",
                  String_cString(directory),strerror(errno)
                 );
      return NULL;
    }

    // init notify
    notifyInfo = (NotifyInfo*)malloc(sizeof(NotifyInfo));
    if (notifyInfo == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    notifyInfo->watchHandle = watchHandle;
    notifyInfo->directory   = String_duplicate(directory);
    List_init(&notifyInfo->uuidList);

    // add notify
    Dictionary_add(&notifyHandles,
                   &notifyInfo->watchHandle,sizeof(notifyInfo->watchHandle),
                   notifyInfo,sizeof(NotifyInfo*)
                  );
    Dictionary_add(&notifyDirectories,
                   String_cString(notifyInfo->directory),String_length(notifyInfo->directory),
                   notifyInfo,sizeof(NotifyInfo*)
                  );
  }

//fprintf(stderr,"%s, %d: add notify %d: %s\n",__FILE__,__LINE__,notifyInfo->watchHandle,String_cString(notifyInfo->directory));

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

//fprintf(stderr,"%s, %d: rem %d: %s\n",__FILE__,__LINE__,notifyInfo->watchHandle,String_cString(notifyInfo->directory));

  assert(Semaphore_isLocked(&notifyLock));

  // remove notify
  Dictionary_remove(&notifyDirectories,
                    String_cString(notifyInfo->directory),
                    String_length(notifyInfo->directory)
                   );
  Dictionary_remove(&notifyHandles,
                    &notifyInfo->watchHandle,
                    sizeof(notifyInfo->watchHandle)
                   );

  // delete notify
  (void)inotify_rm_watch(inotifyHandle,notifyInfo->watchHandle);

  // free resources
  freeNotifyInfo(notifyInfo,NULL);
  free(notifyInfo);
}

/***********************************************************************\
* Name   : addNotifySubDirectories
* Purpose: add notify for directorty and all sub-directories
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
*          directory    - directory to add
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addNotifySubDirectories(const char *jobUUID, const char *scheduleUUID, String directory)
{
  StringList          directoryList;
  String              name;
  Errors              error;
  FileInfo            fileInfo;
  SemaphoreLock       semaphoreLock;
  DirectoryListHandle directoryListHandle;
  NotifyInfo          *notifyInfo;
  UUIDNode            *uuidNode;

  // init variables
  StringList_init(&directoryList);
  name = String_new();

  StringList_append(&directoryList,directory);
  while (   !StringList_isEmpty(&directoryList)
         && !quitFlag
        )
  {
    // get next entry to process
    StringList_removeLast(&directoryList,name);

    if (!isNoBackup(name))
    {
//fprintf(stderr,"%s, %d: name=%s %d\n",__FILE__,__LINE__,String_cString(name),StringList_count(&directoryList));
      // read file info
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
//TODO: log?
        continue;
      }

      // update/add notify
      SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get/add notify
        notifyInfo = addNotify(name);
        if (notifyInfo == NULL)
        {
          Semaphore_unlock(&notifyLock);
          break;
        }

        // update/add uuid
        uuidNode = notifyInfo->uuidList.head;
        while (   (uuidNode != NULL)
               && (   !stringEquals(uuidNode->jobUUID,jobUUID)
                   || !stringEquals(uuidNode->scheduleUUID,scheduleUUID)
                  )
              )
        {
          uuidNode = uuidNode->next;
        }
        if (uuidNode == NULL)
        {
          uuidNode = LIST_NEW_NODE(UUIDNode);
          if (uuidNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          stringCopy(uuidNode->jobUUID,jobUUID,sizeof(uuidNode->jobUUID));
          stringCopy(uuidNode->scheduleUUID,scheduleUUID,sizeof(uuidNode->scheduleUUID));
          List_append(&notifyInfo->uuidList,uuidNode);
        }
        uuidNode->cleanFlag = FALSE;
      }

      // scan sub-directories
      if (fileInfo.type == FILE_TYPE_DIRECTORY)
      {
        // open directory contents
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

            if (!isNoBackup(name))
            {
              // read file info
              error = File_getFileInfo(name,&fileInfo);
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
* Input  : directory - directory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void removeNotifySubDirectories(ConstString directory)
{
  DictionaryIterator dictionaryIterator;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;

  assert(directory != NULL);
  assert(Semaphore_isLocked(&notifyLock));

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

    if (   String_length(notifyInfo->directory) >= String_length(directory)
        && String_startsWith(notifyInfo->directory,directory)
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
  SemaphoreLock      semaphoreLock;
  DictionaryIterator dictionaryIterator;
  void               *data;
  ulong              length;
  NotifyInfo         *notifyInfo;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  SemaphoreLock      semaphoreLock;
  DictionaryIterator dictionaryIterator;
  NotifyInfo         *notifyInfo;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
          uuidNode = List_remove(&notifyInfo->uuidList,uuidNode);
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
  SemaphoreLock      semaphoreLock;
  DictionaryIterator dictionaryIterator;
  NotifyInfo         *notifyInfo;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
  UUIDNode           *uuidNode;

  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
          uuidNode = List_remove(&notifyInfo->uuidList,uuidNode);
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
  InitNotifyMsg   initNotifyMsg;
  StringList      nameList;
  String          basePath;

  EntryNode       *includeEntryNode;
  StringTokenizer fileNameTokenizer;
  ConstString     token;

  // init variables
  StringList_init(&nameList);
  basePath = String_new();

  while (   !quitFlag
         && MsgQueue_get(&initDoneNotifyMsgQueue,&initNotifyMsg,NULL,sizeof(initNotifyMsg),WAIT_FOREVER)
        )
  {
    switch (initNotifyMsg.type)
    {
      case INIT:
//fprintf(stderr,"%s, %d: INIT job=%s scheudle=%s\n",__FILE__,__LINE__,initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_CONTINUOUS,
                    "CONTINUOUS","Start initialize watches for '%s'\n",
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
              File_setFileName(basePath,token);
            }
            else
            {
              File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
            }
          }
          while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
          {
            File_appendFileName(basePath,token);
          }
          File_doneSplitFileName(&fileNameTokenizer);

          // add directory and sub-directories to notify
          addNotifySubDirectories(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID,basePath);
        }

        // clean not existing notifies for job
        cleanNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);

        plogMessage(NULL,  // logHandle
                    LOG_TYPE_CONTINUOUS,
                    "CONTINUOUS","%lu watches (max. %lu)\n",
                    Dictionary_count(&notifyHandles),
                    getMaxNotifies()
                   );
        break;
      case DONE:
//fprintf(stderr,"%s, %d: DONE job=%s scheudle=%s\n",__FILE__,__LINE__,initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
        removeNotifies(initNotifyMsg.jobUUID,initNotifyMsg.scheduleUUID);
        break;
    }

    // free init notify message
    freeInitNotifyMsg(&initNotifyMsg,NULL);
  }

  // free resources
  String_delete(basePath);
  StringList_done(&nameList);
}

/***********************************************************************\
* Name   : existsContinuousEntry
* Purpose: check if continuous entry exists in database
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
*          name         - entry name
* Output : -
* Return : TRUE iff exists
* Notes  : -
\***********************************************************************/

LOCAL bool existsContinuousEntry(const char  *jobUUID,
                                 const char  *scheduleUUID,
                                 ConstString name
                                )
{
  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);
  assert(name != NULL);
  assert(Database_isLocked(&continuousDatabaseHandle));

  return Database_exists(&continuousDatabaseHandle,
                         "names",
                         "id",
                         "WHERE jobUUID=%'s AND scheduleUUID=%'s AND name=%'S",
                         jobUUID,
                         scheduleUUID,
                         name
                        );
}

/***********************************************************************\
* Name   : addContinuousEntry
* Purpose: add continuous entry to database
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
*          name         - entry name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addContinuousEntry(const char  *jobUUID,
                                const char  *scheduleUUID,
                                ConstString name
                               )
{
  assert(jobUUID != NULL);
  assert(scheduleUUID != NULL);
  assert(name != NULL);
  assert(Database_isLocked(&continuousDatabaseHandle));

  return Database_execute(&continuousDatabaseHandle,
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          "INSERT OR IGNORE INTO names \
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
                             ); \
                          ",
                          jobUUID,
                          scheduleUUID,
                          name
                         );
}

/***********************************************************************\
* Name   : removeContinuousEntry
* Purpose: remove continuous entry from database
* Input  : databaseId - database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeContinuousEntry(DatabaseId databaseId)
{
  assert(Database_isLocked(&continuousDatabaseHandle));

  return Database_execute(&continuousDatabaseHandle,
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          "DELETE FROM names WHERE id=%ld;",
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
  #define MAX_ENTRIES 128
  #define BUFFER_SIZE (MAX_ENTRIES*(sizeof(struct inotify_event)+NAME_MAX+1))

  void                       *buffer;
  String                     absoluteName;
  sigset_t                   signalMask;
  struct pollfd              pollfds[1];
  struct timespec            selectTimeout;
  ssize_t                    n;
  const struct inotify_event *inotifyEvent;
  SemaphoreLock              semaphoreLock;
  NotifyInfo                 *notifyInfo;
  UUIDNode                   *uuidNode;
  Errors                     error;

  // init variables
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  absoluteName = String_new();

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  while (!quitFlag)
  {
    // read inotify events
    n = 0;
    do
    {
      // wait for event or timeout
      pollfds[0].fd     = inotifyHandle;
      pollfds[0].events = POLLIN;
      selectTimeout.tv_sec  = 10L;
      selectTimeout.tv_nsec = 0L;
      n = ppoll(pollfds,1,&selectTimeout,&signalMask);

      if (n > 0)
      {
        // read events
        if ((pollfds[0].revents & POLLIN) != 0)
        {
          n = read(inotifyHandle,buffer,BUFFER_SIZE);
        }
      }
    }
    while ((n == -1) && ((errno == EAGAIN) || (errno == EINTR)) && !quitFlag);
    if (quitFlag) break;

    // process inotify events
    inotifyEvent = (const struct inotify_event*)buffer;
    while ((n > 0) && !quitFlag)
    {
//fprintf(stderr,"%s, %d: n=%d\n",__FILE__,__LINE__,n);
      if (inotifyEvent->len > 0)
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

        SEMAPHORE_LOCKED_DO(semaphoreLock,&notifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          notifyInfo = getNotifyInfo(inotifyEvent->wd);
          if (notifyInfo != NULL)
          {
            File_appendFileNameCString(String_set(absoluteName,notifyInfo->directory),inotifyEvent->name);

            if (IS_INOTIFY(inotifyEvent->mask,IN_ISDIR))
            {
              // directory changed

              if      (IS_INOTIFY(inotifyEvent->mask,IN_CREATE))
              {
                // add directory and sub-directories to notify
                BLOCK_DO(Database_lock(&continuousDatabaseHandle),
                         Database_unlock(&continuousDatabaseHandle),
                {
                  LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                  {
                    // store into notify database
                    error = addContinuousEntry(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Marked for storage '%s'\n",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Store continuous entry fail (error: %s)\n",
                                  Error_getText(error)
                                 );
                    }

                    // add directory and sub-directories to notify
                    addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                  }
                });
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
                BLOCK_DO(Database_lock(&continuousDatabaseHandle),
                         Database_unlock(&continuousDatabaseHandle),
                {
                  LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                  {
                    // store into notify database
                    error = addContinuousEntry(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Marked for storage '%s'\n",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Store continuous entry fail (error: %s)\n",
                                  Error_getText(error)
                                 );
                    }

                    // add directory and sub-directories to notify
                    addNotifySubDirectories(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                  }
                });
              }
              else
              {
                // add directory and sub-directories to notify
                BLOCK_DO(Database_lock(&continuousDatabaseHandle),
                         Database_unlock(&continuousDatabaseHandle),
                {
                  LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                  {
                    // store into notify database
                    error = addContinuousEntry(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Marked for storage '%s'\n",
                                  String_cString(absoluteName)
                                 );
                    }
                    else
                    {
                      plogMessage(NULL,  // logHandle
                                  LOG_TYPE_CONTINUOUS,
                                  "CONTINUOUS",
                                  "Store continuous entry fail (error: %s)\n",
                                  Error_getText(error)
                                 );
                    }
                  }
                });
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
                BLOCK_DO(Database_lock(&continuousDatabaseHandle),
                         Database_unlock(&continuousDatabaseHandle),
                {
                  LIST_ITERATE(&notifyInfo->uuidList,uuidNode)
                  {
                    if (!existsContinuousEntry(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName))
                    {
                      error = addContinuousEntry(uuidNode->jobUUID,uuidNode->scheduleUUID,absoluteName);
                      if (error == ERROR_NONE)
                      {
                        plogMessage(NULL,  // logHandle
                                    LOG_TYPE_CONTINUOUS,
                                    "CONTINUOUS",
                                    "Marked for storage '%s'\n",
                                    String_cString(absoluteName)
                                   );
                      }
                      else
                      {
                        plogMessage(NULL,  // logHandle
                                    LOG_TYPE_CONTINUOUS,
                                    "CONTINUOUS",
                                    "Store continuous entry fail (error: %s)\n",
                                    Error_getText(error)
                                   );
                      }
                    }
                  }
                });
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
      }

      // next event
      n -= sizeof(struct inotify_event)+inotifyEvent->len;
      inotifyEvent = (const struct inotify_event*)((byte*)inotifyEvent+sizeof(struct inotify_event)+inotifyEvent->len);
    }
    assert(quitFlag || (n == 0));
  }

  // free resources
  String_delete(absoluteName);
  free(buffer);
}

/*---------------------------------------------------------------------*/

Errors Continuous_initAll(void)
{
  ulong n;

  // init variables
  Semaphore_init(&notifyLock);
  Dictionary_init(&notifyHandles,
                  CALLBACK_NULL,  // dictionaryCopyFunction
                  CALLBACK_INLINE(void,(const void *data, ulong length, void *userData),
                  {
                    NotifyInfo *notifyInfo = (NotifyInfo*)data;
                    assert(notifyInfo != NULL);

                    UNUSED_VARIABLE(length);
                    UNUSED_VARIABLE(userData);

                    freeNotifyInfo(notifyInfo,NULL);
                    free(notifyInfo);
                  },NULL),
                  CALLBACK_NULL  // dictionaryCompareFunction
                 );
  Dictionary_init(&notifyDirectories,
                  CALLBACK_NULL,  // dictionaryCopyFunction
                  CALLBACK_NULL,  // dictionaryFreeFunction (Note: free done in notifyHandles)
                  CALLBACK_NULL  // dictionaryCompareFunction
                 );

  // check number of possible notifies
  n = getMaxNotifies();
  if (n < MIN_NOTIFIES_WARNING) printWarning("Low number of notifies %lu. Please check settings in '%s'!\n",n,PROC_MAX_NOTIFIES_FILENAME);

  // init inotify
  inotifyHandle = inotify_init();
  if (inotifyHandle == -1)
  {
    return ERRORX_(OPEN_FILE,errno,"inotify");
  }

  // init command queue
  if (!MsgQueue_init(&initDoneNotifyMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize init notify message queue!");
  }

  return ERROR_NONE;
}

void Continuous_doneAll(void)
{
  MsgQueue_done(&initDoneNotifyMsgQueue,CALLBACK((MsgQueueMsgFreeFunction)freeInitNotifyMsg,NULL));
  close(inotifyHandle);
  Dictionary_done(&notifyDirectories);
  Dictionary_done(&notifyHandles);
  Semaphore_done(&notifyLock);
}

Errors Continuous_init(const char *databaseFileName)
{
  Errors error;
  int64  continuousVersion;

  // init variables
  quitFlag = FALSE;

  // check if continuous database exists, create database
  if ((databaseFileName != NULL) && File_existsCString(databaseFileName))
  {
    // get continuous version
    error = getContinuousVersion(databaseFileName,&continuousVersion);
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (continuousVersion < CONTINOUS_VERSION)
    {
      // discard existing continuous database, create new database
      File_deleteCString(databaseFileName,FALSE);
      error = createContinuous(&continuousDatabaseHandle,databaseFileName);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    else
    {
      // open continuous database
      error = openContinuous(&continuousDatabaseHandle,databaseFileName);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else
  {
    error = createContinuous(&continuousDatabaseHandle,databaseFileName);
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

  return ERROR_NONE;
}

void Continuous_done(void)
{
  quitFlag = TRUE;
  MsgQueue_setEndOfMsg(&initDoneNotifyMsgQueue);
  Thread_join(&continuousThread);
  Thread_join(&continuousInitThread);

  Thread_done(&continuousThread);
  Thread_done(&continuousInitThread);

  (void)closeContinuous(&continuousDatabaseHandle);
}

Errors Continuous_initNotify(ConstString     name,
                             ConstString     jobUUID,
                             ConstString     scheduleUUID,
                             const EntryList *entryList
                            )
{
  InitNotifyMsg initNotifyMsg;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(entryList != NULL);

//fprintf(stderr,"%s, %d: Continuous_initNotify jobUUID=%s scheduleUUID=%s\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(scheduleUUID));
  initNotifyMsg.type = INIT;
  initNotifyMsg.name = String_duplicate(name);
  stringCopy(initNotifyMsg.jobUUID,String_cString(jobUUID),sizeof(initNotifyMsg.jobUUID));
  stringCopy(initNotifyMsg.scheduleUUID,String_cString(scheduleUUID),sizeof(initNotifyMsg.scheduleUUID));
  EntryList_init(&initNotifyMsg.entryList); EntryList_copy(entryList,&initNotifyMsg.entryList,NULL,NULL);

  (void)MsgQueue_put(&initDoneNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));

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

//fprintf(stderr,"%s, %d: Continuous_doneNotify jobUUID=%s scheduleUUID=%s\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(scheduleUUID));
  initNotifyMsg.type = DONE;
  initNotifyMsg.name = String_duplicate(name);
  stringCopy(initNotifyMsg.jobUUID,String_cString(jobUUID),sizeof(initNotifyMsg.jobUUID));
  if (scheduleUUID != NULL)
  {
    stringCopy(initNotifyMsg.scheduleUUID,String_cString(scheduleUUID),sizeof(initNotifyMsg.scheduleUUID));
  }
  else
  {
    initNotifyMsg.scheduleUUID[0] = '\0';
  }

  (void)MsgQueue_put(&initDoneNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));

  return ERROR_NONE;
}

Errors Continuous_add(ConstString jobUUID,
                      ConstString scheduleUUID,
                      ConstString name
                     )
{
  Errors error;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));
  assert(!String_isEmpty(name));

  BLOCK_DOX(error,
            Database_lock(&continuousDatabaseHandle),
            Database_unlock(&continuousDatabaseHandle),
  {
    return addContinuousEntry(String_cString(jobUUID),String_cString(scheduleUUID),name);
  });

  return error;
}

Errors Continuous_remove(DatabaseId databaseId)
{
  Errors error;

  BLOCK_DOX(error,
            Database_lock(&continuousDatabaseHandle),
            Database_unlock(&continuousDatabaseHandle),
  {
    return removeContinuousEntry(databaseId);
  });

  return error;
}

bool Continuous_removeNext(ConstString jobUUID,
                           ConstString scheduleUUID,
                           String      name
                          )
{
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          databaseId;

  assert(jobUUID != NULL);
  assert(name != NULL);

  BLOCK_DOX(result,
            Database_lock(&continuousDatabaseHandle),
            Database_unlock(&continuousDatabaseHandle),
  {
    // prepare list
    if (Database_prepare(&databaseQueryHandle,
                         &continuousDatabaseHandle,
                         "SELECT id,name FROM names WHERE jobUUID=%'S AND scheduleUUID=%'S",
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
                             &databaseId,
                             name
                            )
       )
    {
      Database_finalize(&databaseQueryHandle);
      return FALSE;
    }

    // done list
    Database_finalize(&databaseQueryHandle);

    // delete entry
    if (removeContinuousEntry(databaseId) != ERROR_NONE)
    {
      return FALSE;
    }

    return TRUE;
  });

  return result;
}

bool Continuous_isAvailable(ConstString jobUUID, ConstString scheduleUUID)
{
  Errors error;

  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  BLOCK_DOX(error,
            Database_lock(&continuousDatabaseHandle),
            Database_unlock(&continuousDatabaseHandle),
  {
    return Database_exists(&continuousDatabaseHandle,
                           "names",
                           "id",
                           "WHERE jobUUID=%'S AND scheduleUUID=%'S",
                           jobUUID,
                           scheduleUUID
                          );
  });

  return error;
}

Errors Continuous_initList(DatabaseQueryHandle *databaseQueryHandle,
                           ConstString         jobUUID,
                           ConstString         scheduleUUID
                          )
{
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(!String_isEmpty(jobUUID));
  assert(!String_isEmpty(scheduleUUID));

  // lock
  Database_lock(&continuousDatabaseHandle);

  // prepare list
  error = Database_prepare(databaseQueryHandle,
                           &continuousDatabaseHandle,
                           "SELECT id,name FROM names WHERE jobUUID=%'S AND scheduleUUID=%'S",
                           jobUUID,
                           scheduleUUID
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&continuousDatabaseHandle);
    return error;
  }

  return ERROR_NONE;
}

void Continuous_doneList(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

  // done list
  Database_finalize(databaseQueryHandle);

  // unlock
  Database_unlock(&continuousDatabaseHandle);
}

bool Continuous_getNext(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              name
                       )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S",
                             databaseId,
                             name
                            );
}

#ifdef __cplusplus
  }
#endif

/* end of file */
