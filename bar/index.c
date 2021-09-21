 /***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "errors.h"

#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"
#include "bar.h"
#include "bar_global.h"

#include "index.h"

/****************** Conditional compilation switches *******************/
// switch off for debugging only!
#define INDEX_INTIIAL_CLEANUP
#define INDEX_IMPORT_OLD_DATABASE
#define INDEX_SUPPORT_DELETE

#ifndef INDEX_IMPORT_OLD_DATABASE
  #warning Index import old databases disabled!
#endif
#ifndef INDEX_INTIIAL_CLEANUP
  #warning Index initial cleanup disabled!
#endif
#ifndef INDEX_SUPPORT_DELETE
  #warning Index delete storages disabled!
#endif

/***************************** Constants *******************************/
//TODO: use type safe type
#ifndef __INDEX_ID_TYPE_SAFE
#else
const IndexId INDEX_ID_NONE = {INDEX_TYPE_NONE, 0LL};
const IndexId INDEX_ID_ANY  = {INDEX_TYPE_NONE,-1LL};
#endif

// index open mask
#define INDEX_OPEN_MASK_MODE  0x0000000F
#define INDEX_OPEN_MASK_FLAGS 0xFFFF0000

//TODO: 5
#define DATABASE_TIMEOUT (5*120L*MS_PER_SECOND)

LOCAL const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[] =
{
  { "NONE",             INDEX_STATE_NONE             },
  { "OK",               INDEX_STATE_OK               },
  { "CREATE",           INDEX_STATE_CREATE           },
  { "UPDATE_REQUESTED", INDEX_STATE_UPDATE_REQUESTED },
  { "UPDATE",           INDEX_STATE_UPDATE           },
  { "ERROR",            INDEX_STATE_ERROR            }
};

LOCAL const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[] =
{
  { "MANUAL", INDEX_MODE_MANUAL },
  { "AUTO",   INDEX_MODE_AUTO   },
//TODO: requried?
  { "*",      INDEX_MODE_SET_ALL }
};

LOCAL const struct
{
  const char *name;
  IndexTypes indexType;
} INDEX_TYPES[] =
{
  { "UUID",      INDEX_TYPE_UUID       },
  { "ENTITY",    INDEX_TYPE_ENTITY     },
  { "STORAGE",   INDEX_TYPE_STORAGE    },
  { "ENTRY",     INDEX_TYPE_ENTRY      },
  { "FILE",      INDEX_TYPE_FILE       },
  { "IMAGE",     INDEX_TYPE_IMAGE      },
  { "DIRECTORY", INDEX_TYPE_DIRECTORY  },
  { "LINK",      INDEX_TYPE_LINK       },
  { "HARDLINK",  INDEX_TYPE_HARDLINK   },
  { "SPECIAL",   INDEX_TYPE_SPECIAL    },
  { "HISTORY",   INDEX_TYPE_HISTORY    },
};

LOCAL const struct
{
  const char           *name;
  IndexEntitySortModes sortMode;
} INDEX_ENTITY_SORT_MODES[] =
{
  { "JOB_UUID",INDEX_ENTITY_SORT_MODE_JOB_UUID },
  { "CREATED", INDEX_ENTITY_SORT_MODE_CREATED  },
};

LOCAL const struct
{
  const char            *name;
  IndexStorageSortModes sortMode;
} INDEX_STORAGE_SORT_MODES[] =
{
//  { "HOSTNAME",INDEX_STORAGE_SORT_MODE_HOSTNAME},
  { "NAME",    INDEX_STORAGE_SORT_MODE_NAME    },
  { "SIZE",    INDEX_STORAGE_SORT_MODE_SIZE    },
  { "CREATED", INDEX_STORAGE_SORT_MODE_CREATED },
  { "STATE",   INDEX_STORAGE_SORT_MODE_STATE   },
};

LOCAL const struct
{
  const char          *name;
  IndexEntrySortModes sortMode;
} INDEX_ENTRY_SORT_MODES[] =
{
  { "ARCHIVE",      INDEX_ENTRY_SORT_MODE_ARCHIVE      },
  { "NAME",         INDEX_ENTRY_SORT_MODE_NAME         },
  { "TYPE",         INDEX_ENTRY_SORT_MODE_TYPE         },
  { "SIZE",         INDEX_ENTRY_SORT_MODE_SIZE         },
  { "FRAGMENT",     INDEX_ENTRY_SORT_MODE_FRAGMENT     },
  { "LAST_CHANGED", INDEX_ENTRY_SORT_MODE_LAST_CHANGED },
};

LOCAL const char *INDEX_ENTITY_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTITY_SORT_MODE_NONE    ] = NULL,

  [INDEX_ENTITY_SORT_MODE_JOB_UUID] = "entities.jobUUID",
  [INDEX_ENTITY_SORT_MODE_CREATED ] = "entities.created",
};

LOCAL const char *INDEX_STORAGE_SORT_MODE_COLUMNS[] =
{
  [INDEX_STORAGE_SORT_MODE_NONE    ] = NULL,

  [INDEX_STORAGE_SORT_MODE_HOSTNAME] = "entities.hostName",
  [INDEX_STORAGE_SORT_MODE_NAME    ] = "storages.name",
  [INDEX_STORAGE_SORT_MODE_SIZE    ] = "storages.totalEntrySize",
  [INDEX_STORAGE_SORT_MODE_CREATED ] = "storages.created",
  [INDEX_STORAGE_SORT_MODE_USERNAME] = "storages.userName",
  [INDEX_STORAGE_SORT_MODE_STATE   ] = "storages.state"
};

LOCAL const char *INDEX_ENTRY_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE        ] = NULL,

  [INDEX_ENTRY_SORT_MODE_ARCHIVE     ] = "storages.name",
  [INDEX_ENTRY_SORT_MODE_NAME        ] = "entries.name",
  [INDEX_ENTRY_SORT_MODE_TYPE        ] = "entries.type",
  [INDEX_ENTRY_SORT_MODE_SIZE        ] = "entries.type,entries.size",
  [INDEX_ENTRY_SORT_MODE_FRAGMENT    ] = "entryFragments.offset",
  [INDEX_ENTRY_SORT_MODE_LAST_CHANGED] = "entries.timeLastChanged"
};
LOCAL const char *INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE        ] = NULL,

  [INDEX_ENTRY_SORT_MODE_ARCHIVE     ] = "storages.name",
  [INDEX_ENTRY_SORT_MODE_NAME        ] = "entriesNewest.name",
  [INDEX_ENTRY_SORT_MODE_TYPE        ] = "entriesNewest.type",
  [INDEX_ENTRY_SORT_MODE_SIZE        ] = "entriesNewest.type,entriesNewest.size",
  [INDEX_ENTRY_SORT_MODE_FRAGMENT    ] = "entryFragments.offset",
  [INDEX_ENTRY_SORT_MODE_LAST_CHANGED] = "entriesNewest.timeLastChanged"
};

// time for index clean-up [s]
#define TIME_INDEX_CLEANUP (4*S_PER_HOUR)

// sleep times [s]
#define SLEEP_TIME_INDEX_CLEANUP_THREAD 120L
#define SLEEP_TIME_PURGE                  2L

// server i/o
#define SERVER_IO_DEBUG_LEVEL 1
#define SERVER_IO_TIMEOUT     (10LL*MS_PER_MINUTE)

// single step purge limit
//  const uint SINGLE_STEP_PURGE_LIMIT = 64;
const uint SINGLE_STEP_PURGE_LIMIT = 4096;

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define IMPORT_INDEX_LOG_FILENAME "import_index.log"
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

/***************************** Datatypes *******************************/

// index open modes
typedef enum
{
  INDEX_OPEN_MODE_READ,
  INDEX_OPEN_MODE_READ_WRITE,
  INDEX_OPEN_MODE_CREATE
} IndexOpenModes;

// additional index open mode flags
#define INDEX_OPEN_MODE_NO_JOURNAL   (1 << 16)
#define INDEX_OPEN_MODE_KEYS (1 << 17)

// thread info
typedef struct
{
  ThreadId   threadId;
//TODO: remove
//  uint       count;
//  const char *fileName;
//  uint       lineNb;
//  uint64     cycleCounter;
  #ifdef INDEX_DEBUG_LOCK
    ThreadLWPId threadLWPId;
    #ifndef NDEBUG
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* NDEBUG */
  #endif /* INDEX_DEBUG_LOCK */
} ThreadInfo;

// progress info data
typedef struct
{
  const char *text;
  uint64     startTimestamp;
  uint64     steps,maxSteps;
  ulong      lastProgressSum;  // last progress sum [1/1000]
  uint       lastProgressCount;
  uint64     lastProgressTimestamp;
} ProgressInfo;

/***************************** Variables *******************************/
//TODO
LOCAL const char                 *indexDatabaseFileName = NULL;
LOCAL IndexIsMaintenanceTime     indexIsMaintenanceTimeFunction;
LOCAL void                       *indexIsMaintenanceTimeUserData;
LOCAL bool                       indexInitializedFlag = FALSE;
LOCAL Semaphore                  indexLock;
LOCAL Array                      indexUsedBy;
LOCAL Semaphore                  indexPauseLock;
LOCAL IndexPauseCallbackFunction indexPauseCallbackFunction = NULL;
LOCAL void                       *indexPauseCallbackUserData;
LOCAL Semaphore                  indexThreadTrigger;
LOCAL Thread                     indexThread;    // upgrade/clean-up thread
LOCAL IndexHandle                *indexThreadIndexHandle;
LOCAL Semaphore                  indexClearStorageLock;
LOCAL bool                       quitFlag;

LOCAL ProgressInfo               importProgressInfo;

#ifndef NDEBUG
  void const *indexBusyStackTrace[32];
  uint       indexBusyStackTraceSize;
#endif /* not NDEBUG */

#ifdef INDEX_DEBUG_LOCK
  ThreadLWPId indexUseCountLPWIds[32];
#endif /* INDEX_DEBUG_LOCK */

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  LOCAL FILE *logImportIndexHandle;
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : DIMPORT
* Purpose: debug import index
* Input  : format - format string
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define DIMPORT(format,...) \
    do \
    { \
      logImportIndex(__FILE__,__LINE__,format, ## __VA_ARGS__); \
    } \
    while (0)
#else /* not INDEX_DEBUG_IMPORT_OLD_DATABASE */
  #define DIMPORT(format,...) \
    do \
    { \
    } \
    while (0)
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

/***********************************************************************\
* Name   : INDEX_DO
* Purpose: index block-operation
* Input  : indexHandle - index handle
*          block       - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_DO(indexHandle,block) \
  do \
  { \
    addIndexInUseThreadInfo(); \
    if (!Thread_isCurrentThread(Thread_getId(&indexThread))) \
    { \
      indexThreadInterrupt(); \
    } \
    ({ \
      auto void __closure__(void); \
      \
      void __closure__(void)block; __closure__; \
    })(); \
    removeIndexInUseThreadInfo(); \
  } \
  while (0)

/***********************************************************************\
* Name   : INDEX_DOX
* Purpose: index block-operation
* Input  : indexHandle - index handle
*          block       - code block
* Output : result - result
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_DOX(result,indexHandle,block) \
  do \
  { \
    addIndexInUseThreadInfo(); \
    if (!Thread_isCurrentThread(Thread_getId(&indexThread))) \
    { \
      indexThreadInterrupt(); \
    } \
    result = ({ \
               auto typeof(result) __closure__(void); \
               \
               typeof(result) __closure__(void)block; __closure__; \
             })(); \
    removeIndexInUseThreadInfo(); \
  } \
  while (0)


/***********************************************************************\
* Name   : INDEX_INTERRUPTABLE_OPERATION_DOX
* Purpose: index interruptable block-operation
* Input  : error           - error code
*          indexHandle     - index handle
*          transactionFlag - transaction flag
*          block           - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,block) \
  do \
  { \
    error = beginInterruptableOperation(indexHandle,&transactionFlag); \
    if (error == ERROR_NONE) \
    { \
      error = ({ \
                auto typeof(error) __closure__(void); \
                \
                typeof(error) __closure__(void)block; __closure__; \
              })(); \
      (void)endInterruptableOperation(indexHandle,&transactionFlag); \
    } \
  } \
  while (0)

/***********************************************************************\
* Name   : WAIT_NOT_IN_USE
* Purpose: wait until index is unused
* Input  : time - wait delta time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define WAIT_NOT_IN_USE(time) \
  do \
  { \
    while (   isIndexInUse() \
           && !quitFlag \
          ) \
    { \
      Misc_udelay(time*US_PER_MS); \
    } \
  } \
  while (0)

/***********************************************************************\
* Name   : WAIT_NOT_IN_USE
* Purpose: wait until index is unused
* Input  : time      - wait delta time [ms]
*          condition - condition to check
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define WAIT_NOT_IN_USEX(time,condition) \
  do \
  { \
    while (   (condition) \
           && isIndexInUse() \
           && !quitFlag \
          ) \
    { \
      Misc_udelay(time*US_PER_MS); \
    } \
  } \
  while (0)

#ifndef NDEBUG
  #define createIndex(...) __createIndex(__FILE__,__LINE__, ## __VA_ARGS__)
  #define openIndex(...)   __openIndex  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeIndex(...)  __closeIndex (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : addIndexInUseThreadInfo
* Purpose: add in use thread info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void addIndexInUseThreadInfo(void)
{
  ThreadInfo threadInfo;

  threadInfo.threadId = Thread_getCurrentId();
  #ifdef INDEX_DEBUG_LOCK
    threadInfo.threadLWPId = Thread_getCurrentLWPId();
    #ifdef HAVE_BACKTRACE
      BACKTRACE(threadInfo.stackTrace,threadInfo.stackTraceSize);
    #endif /* HAVE_BACKTRACE */
  #endif /* INDEX_DEBUG_LOCK */

  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Array_append(&indexUsedBy,&threadInfo);
  }
//Index_debugPrintInUseInfo();
}

/***********************************************************************\
* Name   : removeIndexInUseThreadInfo
* Purpose: remove in use thread info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void removeIndexInUseThreadInfo(void)
{
  ThreadId threadId;
  long     i;

  threadId = Thread_getCurrentId();

  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    i = Array_find(&indexUsedBy,
                   ARRAY_FIND_BACKWARD,
                   NULL,
                   CALLBACK_INLINE(int,(const void *data1, const void *data2, void *userData),
                   {
                     const ThreadInfo *threadInfo = (ThreadInfo*)data1;

                     assert(threadInfo != NULL);
                     assert(data2 == NULL);

                     UNUSED_VARIABLE(data2);
                     UNUSED_VARIABLE(userData);

                     return Thread_equalThreads(threadInfo->threadId,threadId) ? 0 : -1;
                   },NULL)
                  );
    if (i >= 0)
    {
      Array_remove(&indexUsedBy,i);
    }
  }
//Index_debugPrintInUseInfo();
}

/***********************************************************************\
* Name   : isIndexInUse
* Purpose: check if index is in use by some other thread
* Input  : -
* Output : -
* Return : TRUE iff index is in use by some other thread
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isIndexInUse(void)
{
  ThreadId      threadId;
  bool          indexInUse;
  ArrayIterator arrayIterator;
  ThreadInfo    threadInfo;

  threadId   = Thread_getCurrentId();
  indexInUse = FALSE;
  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    ARRAY_ITERATEX(&indexUsedBy,arrayIterator,threadInfo,!indexInUse)
    {
      if (!Thread_equalThreads(threadInfo.threadId,threadId))
      {
        indexInUse = TRUE;
      }
    }
  }

  return indexInUse;
}

/***********************************************************************\
* Name   : isMaintenanceTime
* Purpose: check if maintenance time
* Input  : dateTime - date/time
* Output : -
* Return : TRUE iff maintenance time
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isMaintenanceTime(uint64 dateTime)
{
  return    (indexIsMaintenanceTimeFunction == NULL)
         || indexIsMaintenanceTimeFunction(dateTime,indexIsMaintenanceTimeUserData);
}

/***********************************************************************\
* Name   : busyHandler
* Purpose: index busy handler
* Input  : userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void busyHandler(void *userData)
{
  IndexHandle *indexHandle = (IndexHandle*)userData;

  assert (indexHandle != NULL);

  if (indexHandle->busyHandlerFunction != NULL)
  {
    indexHandle->busyHandlerFunction(indexHandle->busyHandlerUserData);
  }
}

/***********************************************************************\
* Name   : indexThreadInterrupt
* Purpose: interrupt index thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexThreadInterrupt(void)
{
  if (   indexInitializedFlag
      && !isMaintenanceTime(Misc_getCurrentDateTime())
      && !quitFlag
     )
  {
    assert(indexThreadIndexHandle != NULL);

    Database_interrupt(&indexThreadIndexHandle->databaseHandle);
  }
}

/***********************************************************************\
* Name   : openIndex
* Purpose: open index database
* Input  : databaseFileName - database file name NULL for "in memory"
*          indexOpenModes   - open modes; see INDEX_OPEN_MODE_...
*          timeout          - timeout [ms]
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle    *indexHandle,
                         const char     *databaseFileName,
                         ServerIO       *masterIO,
                         IndexOpenModes indexOpenMode,
                         long           timeout
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char     *__fileName__,
                           ulong          __lineNb__,
                           IndexHandle    *indexHandle,
                           const char     *databaseFileName,
                           ServerIO       *masterIO,
                           IndexOpenModes indexOpenMode,
                           long           timeout
                          )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);

  // init variables
  indexHandle->masterIO            = masterIO;
  indexHandle->databaseFileName    = databaseFileName;
  indexHandle->busyHandlerFunction = NULL;
  indexHandle->busyHandlerUserData = NULL;
  indexHandle->upgradeError        = ERROR_NONE;
  #ifndef NDEBUG
    indexHandle->threadId = pthread_self();
  #endif /* NDEBUG */

  // open index database
  if ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_CREATE)
  {
    // delete old file
    if (databaseFileName != NULL)
    {
      (void)File_deleteCString(databaseFileName,FALSE);
    }

    // open database
    INDEX_DOX(error,
              indexHandle,
    {
      #ifdef NDEBUG
        return Database_open(&indexHandle->databaseHandle,
                             databaseFileName,
                             DATABASE_OPENMODE_CREATE,
                             DATABASE_TIMEOUT
                            );
      #else /* not NDEBUG */
        return __Database_open(__fileName__,__lineNb__,
                               &indexHandle->databaseHandle,
                               databaseFileName,
                               DATABASE_OPENMODE_CREATE,
                               DATABASE_TIMEOUT
                              );
      #endif /* NDEBUG */
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // create tables
    INDEX_DOX(error,
              indexHandle,
    {
      INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITIONS[Database_getType(&indexHandle->databaseHandle)],
                                 indexDefinition,
                                 error == ERROR_NONE
                                )
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 indexDefinition
                                );
      }

      return error;
    });
    if (error != ERROR_NONE)
    {
      INDEX_DO(indexHandle,
      {
        #ifdef NDEBUG
          Database_close(&indexHandle->databaseHandle);
        #else /* not NDEBUG */
          __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
        #endif /* NDEBUG */
      });
      return error;
    }
  }
  else
  {
    // open database
    INDEX_DOX(error,
              indexHandle,
    {
      #ifdef NDEBUG
        return Database_open(&indexHandle->databaseHandle,
                             databaseFileName,
                             (((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
                               ? DATABASE_OPENMODE_READWRITE
                               : DATABASE_OPENMODE_READ
                             )|DATABASE_OPENMODE_AUX,
                             timeout
                            );
      #else /* not NDEBUG */
        return __Database_open(__fileName__,__lineNb__,
                               &indexHandle->databaseHandle,
                               databaseFileName,
                               (((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
                                 ? DATABASE_OPENMODE_READWRITE
                                 : DATABASE_OPENMODE_READ
                               )|DATABASE_OPENMODE_AUX,
                               timeout
                              );
      #endif /* NDEBUG */
    });
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // add busy handler
  Database_addBusyHandler(&indexHandle->databaseHandle,CALLBACK_(busyHandler,indexHandle));

  INDEX_DO(indexHandle,
  {
    // disable sync, enable foreign keys
    if (   ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
        || ((indexOpenMode & INDEX_OPEN_MODE_NO_JOURNAL) != 0)
       )
    {
      // disable synchronous mode and journal to increase transaction speed
      (void)Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);
    }
    if ((indexOpenMode & INDEX_OPEN_MODE_KEYS) != 0)
    {
      // enable foreign key constrains
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
    }
  });

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeIndex
* Purpose: close index database
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors closeIndex(IndexHandle *indexHandle)
#else /* not NDEBUG */
  LOCAL Errors __closeIndex(const char  *__fileName__,
                            ulong       __lineNb__,
                            IndexHandle *indexHandle
                           )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  // remove busy handler
  Database_removeBusyHandler(&indexHandle->databaseHandle,CALLBACK_(busyHandler,indexHandle));

  INDEX_DO(indexHandle,
  {
    #ifdef NDEBUG
      Database_close(&indexHandle->databaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
    #endif /* NDEBUG */
  });

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pauseCallback
* Purpose: pause callback
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

LOCAL bool pauseCallback(void *userData)
{
  bool result;

  UNUSED_VARIABLE(userData);

  result = FALSE;
  if (indexPauseCallbackFunction != NULL)
  {
    SEMAPHORE_LOCKED_DO(&indexPauseLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      result = indexPauseCallbackFunction(indexPauseCallbackUserData);
    }
  }

  return result;
}

/***********************************************************************\
* Name   : getCopyPauseCallback
* Purpose: get pause callback
* Input  : -
* Output : -
* Return : pause callback function or NULL
* Notes  : -
\***********************************************************************/

LOCAL_INLINE DatabaseCopyPauseCallbackFunction getCopyPauseCallback(void)
{
  return (indexPauseCallbackFunction != NULL) ? pauseCallback : NULL;
}

/***********************************************************************\
* Name   : getIndexVersion
* Purpose: get index version
* Input  : databaseFileName - database file name
* Output : indexVersion - index version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getIndexVersion(const char *databaseFileName, int64 *indexVersion)
{
  Errors      error;
  IndexHandle indexHandle;

  // open index database
  error = openIndex(&indexHandle,databaseFileName,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getInteger64(&indexHandle.databaseHandle,
                                indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&indexHandle);
    return error;
  }

  // close index database
  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

// TODO
#ifndef NDEBUG
LOCAL void verify(IndexHandle *indexHandle,
                  const char  *tableName,
                  const char  *columnName,
                  int64       value,
                  const char  *condition,
                  ...
                 )
{
//  va_list arguments;
//  Errors  error;
//  int64   n;

  assert(indexHandle != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);
  assert(condition != NULL);

UNUSED_VARIABLE(value);
//TODO
#if 0
  va_start(arguments,condition);
  error = Database_vgetInteger64(&indexHandle->databaseHandle,
                                &n,
                                tableName,
                                columnName,
                                condition,
                                arguments
                               );
  assert(error == ERROR_NONE);
  assert(n == value);
  va_end(arguments);
#endif
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : fixBrokenIds
* Purpose: fix broken ids
* Input  : indexHandle - index handle
*          tableName   - table name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fixBrokenIds(IndexHandle *indexHandle, const char *tableName)
{
  assert(indexHandle != NULL);
  assert(tableName != NULL);

  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount
                         DATABASE_COLUMN_TYPES(),
                         "UPDATE %s SET id=rowId WHERE id IS NULL",
                         tableName
                        );
}

/***********************************************************************\
* Name   : initProgress
* Purpose: init progress
* Input  : text - text
* Output : progressInfo - progress info
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initProgress(ProgressInfo *progressInfo, const char *text)
{
  assert(progressInfo != NULL);

  progressInfo->text                  = text;
  progressInfo->startTimestamp        = 0ll;
  progressInfo->steps                 = 0;
  progressInfo->maxSteps              = 0;
  progressInfo->lastProgressSum       = 0L;
  progressInfo->lastProgressCount     = 0;
  progressInfo->lastProgressTimestamp = 0LL;
}

/***********************************************************************\
* Name   : resetProgress
* Purpose: RESET progress
* Input  : progressInfo - progress info
*          maxSteps     - max. number of steps
* Output :
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void resetProgress(ProgressInfo *progressInfo, uint64 maxSteps)
{
  if (progressInfo != NULL)
  {
    progressInfo->startTimestamp        = Misc_getTimestamp();
    progressInfo->steps                 = 0;
    progressInfo->maxSteps              = maxSteps;
    progressInfo->lastProgressSum       = 0L;
    progressInfo->lastProgressCount     = 0;
    progressInfo->lastProgressTimestamp = 0LL;
  }
}

/***********************************************************************\
* Name   : doneProgress
* Purpose: done progress
* Input  : progressInfo - progress info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneProgress(ProgressInfo *progressInfo)
{
  if (progressInfo != NULL)
  {
  }
}

/***********************************************************************\
* Name   : progressStep
* Purpose: step progress and log
* Input  : userData - user data (progress info)
* Output : -
* Return : -
* Notes  : increment step counter for each call!
\***********************************************************************/

LOCAL void progressStep(void *userData)
{
  ProgressInfo *progressInfo = (ProgressInfo*)userData;
  uint         progress;
  uint         importLastProgress;
  uint64       now;
  uint64       elapsedTime,totalTime;
  uint64       estimatedRestTime;

  if (progressInfo != NULL)
  {
    progressInfo->steps++;

    if (progressInfo->maxSteps > 0)
    {
      progress           = (progressInfo->steps*1000)/progressInfo->maxSteps;
      importLastProgress = (progressInfo->lastProgressCount > 0) ? (uint)(progressInfo->lastProgressSum/(ulong)progressInfo->lastProgressCount) : 0;
      now                = Misc_getTimestamp();
      if (   (progress >= (importLastProgress+1))
          && (now > (progressInfo->lastProgressTimestamp+60*US_PER_SECOND))
         )
      {
        elapsedTime       = now-progressInfo->startTimestamp;
        totalTime         = (elapsedTime*progressInfo->maxSteps)/progressInfo->steps;
        estimatedRestTime = totalTime-elapsedTime;

        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "%s %0.1f%%, estimated rest time %uh:%02umin:%02us",
                    progressInfo->text,
                    (float)progress/10.0,
                    (uint)((estimatedRestTime/US_PER_SECOND)/3600LL),
                    (uint)(((estimatedRestTime/US_PER_SECOND)%3600LL)/60),
                    (uint)((estimatedRestTime/US_PER_SECOND)%60LL)
                   );
        progressInfo->lastProgressSum       += progress;
        progressInfo->lastProgressCount     += 1;
        progressInfo->lastProgressTimestamp = now;
      }
    }
  }
}

/***********************************************************************\
* Name   : logImportProgress
* Purpose: log import progress
* Input  : format - log format string (can be NULL)
*          ...    - optional arguments for log format string
* Output : -
* Return : -
* Notes  : increment step counter for each call!
\***********************************************************************/

LOCAL void logImportProgress(const char *format, ...)
{
  va_list arguments;

  va_start(arguments,format);
  vlogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              format,
              arguments
             );
  va_end(arguments);
}

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
/***********************************************************************\
* Name   : logImportIndex
* Purpose: log import index
* Input  : fileName - file name
*          lineNb   - line number
*          format   - format string
*          ...      - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void logImportIndex(const char *fileName, ulong lineNb, const char *format, ...)
{
  va_list arguments;

  if (logImportIndexHandle != NULL)
  {
    va_start(arguments,format);
    fprintf(logImportIndexHandle,"DEBUG import %s, %4lu: ",fileName,lineNb);
    vfprintf(logImportIndexHandle,format,arguments);
    fprintf(logImportIndexHandle,"\n");
    va_end(arguments);
    fflush(logImportIndexHandle);
  }
}
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

// TODO:
#if 0
#include "index_version1.c"
#include "index_version2.c"
#include "index_version3.c"
#include "index_version4.c"
#include "index_version5.c"
#include "index_version6.c"
#include "index_version7.c"
#endif

/***********************************************************************\
* Name   : getImportStepsCurrentVersion
* Purpose: get number of import steps for current index version
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL ulong getImportStepsCurrentVersion(IndexHandle *oldIndexHandle,
                                         uint        uuidCountFactor,
                                         uint        entityCountFactor,
                                         uint        storageCountFactor
                                        )
{
// TODO:  return getImportStepsVersion7(oldIndexHandle,uuidCountFactor,entityCountFactor,storageCountFactor);
return 7;
}

/***********************************************************************\
* Name   : importCurrentVersion
* Purpose: import current index version
* Input  : oldIndexHandle,newIndexHandle - index handles
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors importCurrentVersion(IndexHandle *oldIndexHandle,
                                  IndexHandle *newIndexHandle
                                 )
{
// TODO:  return importIndexVersion7(oldIndexHandle,newIndexHandle);
return 7;
}

/***********************************************************************\
* Name   : importIndex
* Purpose: upgrade and import index
* Input  : indexHandle         - index handle
*          oldDatabaseFileName - old database file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndex(IndexHandle *indexHandle, ConstString oldDatabaseFileName)
{
  Errors           error;
  IndexHandle      oldIndexHandle;
  int64            indexVersion;
  ulong            maxSteps;
  IndexQueryHandle indexQueryHandle;
  uint64           t0;
  uint64           t1;
  IndexId          uuidId,entityId,storageId;

  // open old index (Note: must be read/write to fix errors in database)
  error = openIndex(&oldIndexHandle,String_cString(oldDatabaseFileName),NULL,INDEX_OPEN_MODE_READ_WRITE,NO_WAIT);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get index version
  error = Database_getInteger64(&oldIndexHandle.databaseHandle,
                                &indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&oldIndexHandle);
    return error;
  }

  // upgrade index structure
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Import index database '%s' (version %d)",
              String_cString(oldDatabaseFileName),
              indexVersion
             );
  DIMPORT("import index %"PRIi64"",indexVersion);
  maxSteps = 0L;
// TODO:
#if 0
  switch (indexVersion)
  {
    case 1:
      maxSteps = getImportStepsVersion1(&oldIndexHandle,2,2,2);
      break;
    case 2:
      maxSteps = getImportStepsVersion2(&oldIndexHandle,2,2,2);
      break;
    case 3:
      maxSteps = getImportStepsVersion3(&oldIndexHandle,2,2,2);
      break;
    case 4:
      maxSteps = getImportStepsVersion4(&oldIndexHandle,2,2,2);
      break;
    case 5:
      maxSteps = getImportStepsVersion5(&oldIndexHandle,2,2,2);
      break;
    case 6:
      maxSteps = getImportStepsVersion6(&oldIndexHandle,2,2,2);
      break;
    case INDEX_CONST_VERSION:
      maxSteps = getImportStepsCurrentVersion(&oldIndexHandle,2,2,2);
      break;
    default:
      // unknown version if index
      error = ERROR_DATABASE_VERSION_UNKNOWN;
      break;
  }
  initProgress(&importProgressInfo,"Import");
  resetProgress(&importProgressInfo,maxSteps);
  switch (indexVersion)
  {
    case 1:
      error = importIndexVersion1(&oldIndexHandle,indexHandle);
      break;
    case 2:
      error = importIndexVersion2(&oldIndexHandle,indexHandle);
      break;
    case 3:
      error = importIndexVersion3(&oldIndexHandle,indexHandle);
      break;
    case 4:
      error = importIndexVersion4(&oldIndexHandle,indexHandle);
      break;
    case 5:
      error = importIndexVersion5(&oldIndexHandle,indexHandle);
      break;
    case 6:
      error = importIndexVersion6(&oldIndexHandle,indexHandle);
      break;
    case INDEX_CONST_VERSION:
      error = importCurrentVersion(&oldIndexHandle,indexHandle);
      break;
    default:
      // unknown version if index
      error = ERROR_DATABASE_VERSION_UNKNOWN;
      break;
  }
#endif
  DIMPORT("import index done (error: %s)",Error_getText(error));

  DIMPORT("create aggregates");
  if (error == ERROR_NONE)
  {
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entityId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount,
                                   INDEX_TYPE_SET_ALL,
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
    while (   (error == ERROR_NONE)
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
                                   NULL,  // storageName,
                                   NULL,  // dateTime
                                   NULL,  // size,
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      t0 = Misc_getTimestamp();
      error = Index_updateStorageInfos(indexHandle,storageId);
      t1 = Misc_getTimestamp();
      if (error == ERROR_NONE)
      {
        logImportProgress("Aggregated storage #%"PRIi64": (%llus)",
                          storageId,
                          (t1-t0)/US_PER_SECOND
                         );
      }
      progressStep(&importProgressInfo);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    error = Index_initListEntities(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   NULL,  // jobUUID,
                                   NULL,  // scheduldUUID
                                   ARCHIVE_TYPE_ANY,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_ENTITY_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    while (   (error == ERROR_NONE)
           && Index_getNextEntity(&indexQueryHandle,
                                  NULL,  // uuidId,
                                  NULL,  // jobUUID,
                                  NULL,  // scheduleUUID,
                                  &entityId,
                                  NULL,  // archiveType,
                                  NULL,  // createdDateTime,
                                  NULL,  // lastErrorMessage
                                  NULL,  // totalSize
                                  NULL,  // totalEntryCount
                                  NULL,  // totalEntrySize
                                  NULL  // lockedCount
                                 )
          )
    {
      t0 = Misc_getTimestamp();
      error = Index_updateEntityInfos(indexHandle,entityId);
      t1 = Misc_getTimestamp();
      if (error == ERROR_NONE)
      {
        logImportProgress("Aggregated entity #%"PRIi64": (%llus)",
                          entityId,
                          (t1-t0)/US_PER_SECOND
                         );
      }
      progressStep(&importProgressInfo);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    error = Index_initListUUIDs(&indexQueryHandle,
                                indexHandle,
                                INDEX_STATE_SET_ALL,
                                INDEX_MODE_SET_ALL,
                                NULL,  // name
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
    while (   (error == ERROR_NONE)
           && Index_getNextUUID(&indexQueryHandle,
                                &uuidId,
                                NULL,  // jobUUID
                                NULL,  // lastCheckedDateTime
                                NULL,  // lastErrorMessage
                                NULL,  // size
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      t0 = Misc_getTimestamp();
      error = Index_updateUUIDInfos(indexHandle,uuidId);
      t1 = Misc_getTimestamp();
      if (error == ERROR_NONE)
      {
        logImportProgress("Aggregated UUID #%"PRIi64": (%llus)",
                          uuidId,
                          (t1-t0)/US_PER_SECOND
                         );
      }
      progressStep(&importProgressInfo);
    }
    Index_doneList(&indexQueryHandle);
  }
  DIMPORT("create aggregates done (error: %s)",Error_getText(error));

  doneProgress(&importProgressInfo);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Imported old index database '%s' (version %d)",
                String_cString(oldDatabaseFileName),
                indexVersion
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Import old index database '%s' (version %d) fail: %s",
                String_cString(oldDatabaseFileName),
                indexVersion,
                Error_getText(error)
               );
  }

  // close old index
  (void)closeIndex(&oldIndexHandle);

  return error;
}

/***********************************************************************\
* Name   : cleanUpDuplicateMeta
* Purpose: purge duplicate meta data
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateMeta(IndexHandle *indexHandle)
{
//  String              name;
  Errors              error;
//  DatabaseStatementHandle databaseStatementHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up duplicate meta data"
             );

  // init variables
//  name = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM meta \
                              WHERE ROWID NOT IN (SELECT MIN(rowid) FROM meta GROUP BY name) \
                             "
                            );
#if 0
  if (Database_prepare(&databaseStatementHandle,
                       &indexHandle->databaseHandle,
                       DATABASE_COLUMN_TYPES(TEXT),
                       "SELECT name FROM meta GROUP BY name"
                      ) == ERROR_NONE
     )
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(TEXT),
                             "SELECT name FROM meta GROUP BY name"
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle,
                                 "%S",
                                 name
                                )
            )
      {
        (void)Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DELETE FROM meta \
                                WHERE     name=%'S \
                                      AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1)) \
                               ",
                               name,
                               name
                              );
      }
      Database_finalize(&databaseStatementHandle);
    }
#endif

    return error;
  });
//TODO: error?

  // free resources
//  String_delete(name);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up duplicate meta data"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up duplicate meta data failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteUpdate
* Purpose: reset incomplete updated database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteUpdate(IndexHandle *indexHandle)
{
  Errors           error;
  IndexId          indexId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up incomplete updates"
             );

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    // reset lock count
    (void)Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(),
                           "UPDATE entities SET lockedCount=0"
                          );

    // clear state of deleted storages
    (void)Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(),
                           "UPDATE storages SET state=%u WHERE deletedFlag=1",
                           INDEX_STATE_NONE
                          );

    // request update state of incomplete storages
    error = ERROR_NONE;
    while (Index_findStorageByState(indexHandle,
                                    INDEX_STATE_SET(INDEX_STATE_UPDATE),
                                    NULL,  // uuidId
                                    NULL,  // jobUUID
                                    NULL,  // entityId
                                    NULL,  // scheduleUUID
                                    &indexId,
                                    storageName,
                                    NULL,  // dateTime
                                    NULL,  // size
                                    NULL,  // indexMode
                                    NULL,  // lastCheckedDateTime
                                    NULL,  // errorMessage
                                    NULL,  // totalEntryCount
                                    NULL  // totalEntrySize
                                   )
           && (error == ERROR_NONE)
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

      error = Index_setStorageState(indexHandle,
                                    indexId,
                                    INDEX_STATE_UPDATE_REQUESTED,
                                    0LL,
                                    NULL
                                   );
      if (error == ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "Requested update index #%lld: %s",
                    indexId,
                    String_cString(printableStorageName)
                   );
      }
    }

    return error;
  });

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up incomplete updates"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up incomplete updates failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteCreate
* Purpose: purge incomplete created database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteCreate(IndexHandle *indexHandle)
{
  Errors           error;
  IndexId          storageId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up incomplete created archives"
             );

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  error = ERROR_NONE;
  while (Index_findStorageByState(indexHandle,
                                  INDEX_STATE_SET(INDEX_STATE_CREATE),
                                  NULL,  // uuidId
                                  NULL,  // jobUUID
                                  NULL,  // entityId
                                  NULL,  // scheduleUUID
                                  &storageId,
                                  storageName,
                                  NULL,  // dateTime
                                  NULL,  // size
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
         && (error == ERROR_NONE)
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

    error = Index_deleteStorage(indexHandle,storageId);
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Deleted incomplete storage #%lld: '%s'",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up incomplete created entries"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up incomplete created entries failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStoragenNoName
* Purpose: purge storage entries without name
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoName(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes without name"
             );

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount,
                                 INDEX_TYPE_SET_ALL,
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
                                &storageId,
                                storageName,
                                NULL,  // dateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteStorage(indexHandle,storageId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  // free resource
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes without name",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without name failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStorageNoEntity
* Purpose: clean-up storage entries without any entity
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoEntity(IndexHandle *indexHandle)
{
//TODO
#if 0
  Errors              error;
  String              name1,name2;
  DatabaseStatementHandle databaseStatementHandle1,databaseStatementHandle2;
  DatabaseId          storageDatabaseId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityDatabaseId;
  bool                equalsFlag;
  ulong               i;
  String              oldDatabaseFileName;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up no entity-entries"
             );

  // init variables
  name1 = String_new();
  name2 = String_new();

  // try to set entityId in storage entries
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle1,
                             &indexHandle->databaseHandle,
                             "SELECT uuid, \
                                     name, \
                                     UNIXTIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY id,created ASC \
                             "
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,
                                 "%S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseStatementHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT,TEXT),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=%'S \
                                 ",
                                 uuid
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseStatementHandle2,
                                     "%lld %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
            // compare names (equals except digits)
            equalsFlag = String_length(name1) == String_length(name2);
            i = STRING_BEGIN;
            while (equalsFlag
                   && (i < String_length(name1))
                   && (   isdigit(String_index(name1,i))
                       || (String_index(name1,i) == String_index(name2,i))
                      )
                  )
            {
              i++;
            }
            if (equalsFlag)
            {
              // assign entity id
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET entityId=%lld \
                                      WHERE id=%lld \
                                     ",
                                     entityDatabaseId,
                                     storageDatabaseId
                                    );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_execute(&newIndexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "INSERT INTO entities \
                                    ( \
                                     jobUUID, \
                                     created, \
                                     type \
                                    ) \
                                  VALUES \
                                    ( \
                                     %'S, \
                                     DATETIME(%llu,'unixepoch'), \
                                     %d\
                                    ) \
                                 ",
                                 uuid,
                                 createdDateTime,
                                 ARCHIVE_TYPE_FULL
                                );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

          // assign entity id for all storage entries with same uuid and matching name (equals except digits)
          error = Database_prepare(&databaseStatementHandle2,
                                   &oldIndexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT,TEXT),
                                   "SELECT id, \
                                           name \
                                    FROM storages \
                                    WHERE uuid=%'S \
                                   ",
                                   uuid
                                  );
          if (error == ERROR_NONE)
          {
            while (Database_getNextRow(&databaseStatementHandle2,
                                       "%lld %S",
                                       &storageId,
                                       name2
                                      )
                  )
            {
              // compare names (equals except digits)
              equalsFlag = String_length(name1) == String_length(name2);
              i = STRING_BEGIN;
              while (equalsFlag
                     && (i < String_length(name1))
                     && (   isdigit(String_index(name1,i))
                         || (String_index(name1,i) == String_index(name2,i))
                        )
                    )
              {
                i++;
              }
              if (equalsFlag)
              {
                // assign entity id
                (void)Database_execute(&newIndexHandle->databaseHandle,
                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                       NULL,  // changedRowCount
                                       "UPDATE storages \
                                        SET entityId=%lld \
                                        WHERE id=%lld \
                                       ",
                                       entityId,
                                       storageId
                                      );
              }
            }
            Database_finalize(&databaseStatementHandle2);
          }
        }
      }
      Database_finalize(&databaseStatementHandle1);
    }

    return ERROR_NONE;
  });

  // free resources
  String_delete(name2);
  String_delete(name1);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up no entity-entries"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up no entity-entries failed (error: %s)",
                Error_getText(error)
               );
  }
#else
UNUSED_VARIABLE(indexHandle);
return ERROR_NONE;
#endif
}

/***********************************************************************\
* Name   : cleanUpStorageInvalidState
* Purpose: clean-up storage entries with invalid state
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageInvalidState(IndexHandle *indexHandle)
{
  ulong   n;
  Errors  error;
  IndexId storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes with invalid state"
             );

  // init variables

  // clean-up
  n = 0;
  INDEX_DOX(error,
            indexHandle,
  {
    do
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &storageId,
                             "storages",
                             "id",
                             "WHERE     ((state<%u) OR (state>%u)) \
                                    AND deletedFlag!=1 \
                             ",
                             INDEX_STATE_MIN,
                             INDEX_STATE_MAX
                            );
      if ((error == ERROR_NONE) && (storageId != DATABASE_ID_NONE))
      {
        error = Index_deleteStorage(indexHandle,INDEX_ID_STORAGE(storageId));
        n++;
      }
    }
    while ((error == ERROR_NONE) && (storageId != DATABASE_ID_NONE));

    return error;
  });

  // free resource

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes with invalid state",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes with invalid state failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpDuplicateStorages
* Purpose: clean-up duplicate storages
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: use
#if 0
LOCAL Errors cleanUpDuplicateStorages(IndexHandle *indexHandle)
{
  Errors              error;
  String              name1,name2;
  DatabaseStatementHandle databaseStatementHandle1,databaseStatementHandle2;
  DatabaseId          storageDatabaseId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityDatabaseId;
  bool                equalsFlag;
  ulong               i;
  String              oldDatabaseFileName;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up duplicate storages"
             );

  // init variables
  name1 = String_new();
  name2 = String_new();

  // try to set entityId in storage entries
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle1,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT,TEXT,DATETIME),
                             "SELECT id, \
                                     name, \
                                     UNIXTIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY name ASC \
                             "
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,
                                 "%S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseStatementHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT,TEXT),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=%'S \
                                 ",
                                 uuid
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseStatementHandle2,
                                     "%lld %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
            // compare names (equals except digits)
            equalsFlag = String_length(name1) == String_length(name2);
            i = STRING_BEGIN;
            while (equalsFlag
                   && (i < String_length(name1))
                   && (   isdigit(String_index(name1,i))
                       || (String_index(name1,i) == String_index(name2,i))
                      )
                  )
            {
              i++;
            }
            if (equalsFlag)
            {
              // assign entity id
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET entityId=%lld \
                                      WHERE id=%lld \
                                     ",
                                     entityDatabaseId,
                                     storageDatabaseId
                                    );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_execute(&newIndexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "INSERT INTO entities \
                                    ( \
                                     jobUUID, \
                                     created, \
                                     type \
                                    ) \
                                  VALUES \
                                    ( \
                                     %'S, \
                                     DATETIME(%llu,'unixepoch'), \
                                     %d\
                                    ) \
                                 ",
                                 uuid,
                                 createdDateTime,
                                 ARCHIVE_TYPE_FULL
                                );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);
        }
      }
      Database_finalize(&databaseStatementHandle1);
    }

    return ERROR_NONE;
  });

  // free resources
  String_delete(name2);
  String_delete(name1);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up duplicate storages"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up duplicate storages failed (error: %s)",
                Error_getText(error)
               );
  }
}
#endif

/***********************************************************************\
* Name   : cleanUpNoUUID
* Purpose: purge UUID entries without jobUUID
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpNoUUID(IndexHandle *indexHandle)
{
  ulong  n;
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes without UUID"
             );

  // init variables

  // clean-up
  n = 0L;
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_execute(&indexHandle->databaseHandle,
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            &n,
                            DATABASE_COLUMN_TYPES(),
                            "DELETE FROM uuids \
                             WHERE uuids.jobUUID='' \
                            "
                           );
  });

  // free resource

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes without UUID",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without UUID failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : beginInterruptableOperation
* Purpose: begin interruptable operation
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors beginInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag)
{
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);

  (*transactionFlag) = FALSE;

  error = Index_beginTransaction(indexHandle,WAIT_FOREVER);
  if (error != ERROR_NONE)
  {
    return error;
  }

  (*transactionFlag) = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : endInterruptableOperation
* Purpose: end interruptable operation
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors endInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag)
{
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);

  if (*transactionFlag)
  {
    error = Index_endTransaction(indexHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  (*transactionFlag) = FALSE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : updateDirectoryContentAggregates
* Purpose: update directory content count/size
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          entryId     - entry database id
*          fileName    - file name
*          size        - size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateDirectoryContentAggregates(IndexHandle *indexHandle,
                                              DatabaseId  storageId,
                                              DatabaseId  entryId,
                                              ConstString fileName,
                                              uint64      size
                                             )
{
  Errors     error;
  DatabaseId databaseId;
  String     directoryName;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(entryId != DATABASE_ID_NONE);
  assert(fileName != NULL);

  // get newest id
  error = Database_getId(&indexHandle->databaseHandle,
                         &databaseId,
                         "entriesNewest",
                         "id",
                         "WHERE entryId=%lld",
                         entryId
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  directoryName = File_getDirectoryName(String_new(),fileName);
  error = ERROR_NONE;
  while ((error == ERROR_NONE) && !String_isEmpty(directoryName))
  {
//fprintf(stderr,"%s, %d: directoryName=%s %llu\n",__FILE__,__LINE__,String_cString(directoryName),size);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE directoryEntries \
                              SET totalEntryCount=totalEntryCount+1, \
                                  totalEntrySize =totalEntrySize +%llu \
                              WHERE     storageId=%lld \
                                    AND name=%'S \
                             ",
                             size,
                             storageId,
                             directoryName
                            );
    if (error != ERROR_NONE)
    {
      break;
    }

    if (databaseId != DATABASE_ID_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE directoryEntries \
                                SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                    totalEntrySizeNewest =totalEntrySizeNewest +%llu \
                                WHERE     storageId=%lld \
                                      AND name=%'S \
                               ",
                               size,
                               storageId,
                               directoryName
                              );
      if (error != ERROR_NONE)
      {
        break;
      }
    }

    File_getDirectoryName(directoryName,directoryName);
  }
  String_delete(directoryName);

  return ERROR_NONE;
}

#if 0
//TODO: still not used
/***********************************************************************\
* Name   : updateUUIDAggregates
* Purpose: update UUID aggregates
* Input  : indexHandle - index handle
*          uuidId      - UUID database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : currently nothing to do
\***********************************************************************/

LOCAL Errors updateUUIDAggregates(IndexHandle *indexHandle,
                                  DatabaseId  uuidId
                                 )
{
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(uuidId);

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : updateEntityAggregates
* Purpose: update entity aggregates
* Input  : indexHandle - index handle
*          entityId    - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateEntityAggregates(IndexHandle *indexHandle,
                                    DatabaseId  entityId
                                   )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalFileCount;
  double              totalFileSize_;
  uint64              totalFileSize;
  ulong               totalImageCount;
  double              totalImageSize_;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  double              totalHardlinkSize_;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;

  assert(indexHandle != NULL);

  // get file aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_FILE,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get image aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_IMAGE,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get directory aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM entries \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_DIRECTORY,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // get link aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM entries \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_LINK,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // get hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_HARDLINK,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get special aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM entries \
                            WHERE     entries.type=%u \
                                  AND entries.entityId=%lld \
                           ",
                           INDEX_TYPE_SPECIAL,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // update entity aggregate data
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entities \
                            SET totalEntryCount    =%llu, \
                                totalEntrySize     =%llu, \
                                totalFileCount     =%llu, \
                                totalFileSize      =%llu, \
                                totalImageCount    =%llu, \
                                totalImageSize     =%llu, \
                                totalDirectoryCount=%llu, \
                                totalLinkCount     =%llu, \
                                totalHardlinkCount =%llu, \
                                totalHardlinkSize  =%llu, \
                                totalSpecialCount  =%llu \
                            WHERE id=%lld \
                           ",
                           totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                           totalFileSize+totalImageSize+totalHardlinkSize,
                           totalFileCount,
                           totalFileSize,
                           totalImageCount,
                           totalImageSize,
                           totalDirectoryCount,
                           totalLinkCount,
                           totalHardlinkCount,
                           totalHardlinkSize,
                           totalSpecialCount,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // -----------------------------------------------------------------

  // get newest file aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_FILE,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest image aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_IMAGE,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest directory aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM entriesNewest \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_DIRECTORY,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest link aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM entriesNewest \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_LINK,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entriesNewest.size?
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_HARDLINK,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest special aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM entriesNewest \
                            WHERE     entriesNewest.type=%u \
                                  AND entriesNewest.entityId=%lld \
                           ",
                           INDEX_TYPE_SPECIAL,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  Database_finalize(&databaseStatementHandle);

  // update newest aggregate data
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entities \
                            SET totalEntryCountNewest    =%llu, \
                                totalEntrySizeNewest     =%llu, \
                                totalFileCountNewest     =%llu, \
                                totalFileSizeNewest      =%llu, \
                                totalImageCountNewest    =%llu, \
                                totalImageSizeNewest     =%llu, \
                                totalDirectoryCountNewest=%llu, \
                                totalLinkCountNewest     =%llu, \
                                totalHardlinkCountNewest =%llu, \
                                totalHardlinkSizeNewest  =%llu, \
                                totalSpecialCountNewest  =%llu \
                            WHERE id=%lld \
                           ",
                           totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                           totalFileSize+totalImageSize+totalHardlinkSize,
                           totalFileCount,
                           totalFileSize,
                           totalImageCount,
                           totalImageSize,
                           totalDirectoryCount,
                           totalLinkCount,
                           totalHardlinkCount,
                           totalHardlinkSize,
                           totalSpecialCount,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : updateStorageAggregates
* Purpose: update storage aggregates
* Input  : indexHandle - index handle
*          storageId   - storage index id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateStorageAggregates(IndexHandle *indexHandle,
                                     DatabaseId  storageId
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalFileCount;
  double              totalFileSize_;
  uint64              totalFileSize;
  ulong               totalImageCount;
  double              totalImageSize_;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  double              totalHardlinkSize_;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;
  DatabaseId          entityId;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // get file aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entries.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_FILE
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalFileCount = 0L;
    totalFileSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get image aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entries.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_IMAGE
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                            WHERE directoryEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalDirectoryCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM linkEntries \
                              LEFT JOIN entries ON entries.id=linkEntries.entryId \
                            WHERE linkEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalLinkCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entries.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_HARDLINK
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalHardlinkCount = 0L;
    totalHardlinkSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                            WHERE specialEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalSpecialCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // update aggregate data
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE storages \
                            SET totalEntryCount    =%llu, \
                                totalEntrySize     =%llu, \
                                totalFileCount     =%llu, \
                                totalFileSize      =%llu, \
                                totalImageCount    =%llu, \
                                totalImageSize     =%llu, \
                                totalDirectoryCount=%llu, \
                                totalLinkCount     =%llu, \
                                totalHardlinkCount =%llu, \
                                totalHardlinkSize  =%llu, \
                                totalSpecialCount  =%llu \
                            WHERE id=%lld \
                           ",
                           totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                           totalFileSize+totalImageSize+totalHardlinkSize,
                           totalFileCount,
                           totalFileSize,
                           totalImageCount,
                           totalImageSize,
                           totalDirectoryCount,
                           totalLinkCount,
                           totalHardlinkCount,
                           totalHardlinkSize,
                           totalSpecialCount,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // -----------------------------------------------------------------

  // get newest file aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entriesNewest.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_FILE
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalFileCount = 0L;
    totalFileSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest image aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entriesNewest.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_IMAGE
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM directoryEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                            WHERE directoryEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalDirectoryCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM linkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                            WHERE linkEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalLinkCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
//TODO: use entriesNewest.size?
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=%lld \
                                  AND entriesNewest.type=%u \
                           ",
                           storageId,
                           INDEX_TYPE_HARDLINK
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalHardlinkCount = 0L;
    totalHardlinkSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM specialEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                            WHERE specialEntries.storageId=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalSpecialCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // update newest aggregate data
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE storages \
                            SET totalEntryCountNewest    =%llu, \
                                totalEntrySizeNewest     =%llu, \
                                totalFileCountNewest     =%llu, \
                                totalFileSizeNewest      =%llu, \
                                totalImageCountNewest    =%llu, \
                                totalImageSizeNewest     =%llu, \
                                totalDirectoryCountNewest=%llu, \
                                totalLinkCountNewest     =%llu, \
                                totalHardlinkCountNewest =%llu, \
                                totalHardlinkSizeNewest  =%llu, \
                                totalSpecialCountNewest  =%llu \
                            WHERE id=%lld \
                           ",
                           totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                           totalFileSize+totalImageSize+totalHardlinkSize,
                           totalFileCount,
                           totalFileSize,
                           totalImageCount,
                           totalImageSize,
                           totalDirectoryCount,
                           totalLinkCount,
                           totalHardlinkCount,
                           totalHardlinkSize,
                           totalSpecialCount,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // update entity aggregates
  error = Database_getId(&indexHandle->databaseHandle,
                         &entityId,
                         "storages",
                         "entityId",
                         "WHERE id=%lld \
                         ",
                         storageId
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = updateEntityAggregates(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : interruptOperation
* Purpose: interrupt operation, temporary close transcation and wait
*          until index is unused
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable or NULL
*          time            - interruption wait delta time [ms[
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors interruptOperation(IndexHandle *indexHandle, bool *transactionFlag, ulong time)
{
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);
  assert(*transactionFlag);

  if (isIndexInUse())
  {
    if (transactionFlag != NULL)
    {
      // temporary end transaction
      error = Index_endTransaction(indexHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }
      (*transactionFlag) = FALSE;
    }

    // wait until index is unused
    WAIT_NOT_IN_USE(time);
    if (quitFlag)
    {
      return ERROR_INTERRUPTED;
    }

    if (transactionFlag != NULL)
    {
      // begin transaction
      error = Index_beginTransaction(indexHandle,WAIT_FOREVER);
      if (error != ERROR_NONE)
      {
        return error;
      }
      (*transactionFlag) = TRUE;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purge
* Purpose: purge with delay/check if index-usage
* Input  : indexHandle    - index handle
*          doneFlag       - done flag variable (can be NULL)
*          deletedCounter - deleted entries count variable (can be NULL)
*          tableName      - table name
*          filter         - filter string
*          ...            - optional arguments for filter
* Output : doneFlag       - set to FALSE if delete not completely done
*          deletedCounter - updated deleted entries count
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purge(IndexHandle *indexHandle,
                   bool        *doneFlag,
                   ulong       *deletedCounter,
                   const char  *tableName,
                   const char  *filter,
                   ...
                  )
{
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0;
  #endif

  String  filterString;
  va_list arguments;
  Errors  error;
  ulong   changedRowCount;

  // init variables
  filterString = String_new();

  // get filter
  va_start(arguments,filter);
  String_vformat(filterString,filter,arguments);
  va_end(arguments);

//fprintf(stderr,"%s, %d: purge (%d): %s %s\n",__FILE__,__LINE__,isIndexInUse(),tableName,String_cString(filterString));
  error = ERROR_NONE;
  do
  {
    changedRowCount = 0;
    #ifdef INDEX_DEBUG_PURGE
      t0 = Misc_getTimestamp();
    #endif
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             &changedRowCount,
                             "DELETE FROM %s \
                              WHERE %S \
                              LIMIT %u \
                             ",
                             tableName,
                             filterString,
                             SINGLE_STEP_PURGE_LIMIT
                            );
    if (error == ERROR_NONE)
    {
      if (deletedCounter != NULL)(*deletedCounter) += changedRowCount;
    }
    #ifdef INDEX_DEBUG_PURGE
      if (changedRowCount > 0)
      {
        fprintf(stderr,"%s, %d: error: %s, purged %lu entries from '%s': %llums\n",__FILE__,__LINE__,
                Error_getText(error),
                changedRowCount,
                tableName,
                (Misc_getTimestamp()-t0)/US_PER_MS
               );
      }
    #endif
//fprintf(stderr,"%s, %d: isIndexInUse=%d\n",__FILE__,__LINE__,isIndexInUse());
  }
  while (   (error == ERROR_NONE)
         && (changedRowCount > 0)
         && !isIndexInUse()
        );
  #ifdef INDEX_DEBUG_PURGE
    if (error == ERROR_INTERRUPTED)
    {
      fprintf(stderr,"%s, %d: purge interrupted\n",__FILE__,__LINE__);
    }
  #endif

  // update done-flag
  if ((error == ERROR_NONE) && (doneFlag != NULL))
  {
//fprintf(stderr,"%s, %d: exists check %s: %s c=%lu count=%d inuse=%d\n",__FILE__,__LINE__,tableName,String_cString(filterString),x,changedRowCount,isIndexInUse());
    if ((changedRowCount > 0) && isIndexInUse())
    {
//fprintf(stderr,"%s, %d: clear done %d %d\n",__FILE__,__LINE__,changedRowCount,isIndexInUse());
      (*doneFlag) = FALSE;
    }
//fprintf(stderr,"%s, %d: %d %d\n",__FILE__,__LINE__,*doneFlag,isIndexInUse());
  }

  // free resources
  String_delete(filterString);

  return error;
}

/***********************************************************************\
* Name   : isEmptyUUID
* Purpose: check if UUID if empty
* Input  : indexHandle - index handle
*          uuidId      - UUID database id
* Output : -
* Return : TRUE iff UUID is empty
* Notes  : -
\***********************************************************************/

LOCAL bool isEmptyUUID(IndexHandle *indexHandle,
                       DatabaseId  uuidId
                      )
{
  assert(indexHandle != NULL);

  return    (uuidId != DATABASE_ID_NONE)
         && !Database_exists(&indexHandle->databaseHandle,
                             "entities",
                             "entities.id",
                             "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE uuids.id=%lld \
                             ",
                             uuidId
                            );
}

/***********************************************************************\
* Name   : pruneUUID
* Purpose: prune empty UUID
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          uuidId         - UUID database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneUUID(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       DatabaseId  uuidId
                      )
{
  Errors        error;

  assert(indexHandle != NULL);
  assert(uuidId != DATABASE_ID_NONE);

  UNUSED_VARIABLE(doneFlag);
  UNUSED_VARIABLE(deletedCounter);

  // delete uuid if empty
  if (isEmptyUUID(indexHandle,uuidId))
  {
    // delete UUID index
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM uuids WHERE id=%lld",
                             uuidId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Purged UUID #%llu: no entities",
                Index_getDatabaseId(uuidId)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneUUIDs
* Purpose: prune all enpty UUIDs
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneUUIDs(IndexHandle *indexHandle,
                        bool        *doneFlag,
                        ulong       *deletedCounter
                       )
{
  Array         uuidIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    uuidId;

  assert(indexHandle != NULL);

  Array_init(&uuidIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = Database_getIds(&indexHandle->databaseHandle,
                          &uuidIds,
                          "uuids",
                          "id",
                          ""
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&uuidIds);
    return error;
  }
  ARRAY_ITERATEX(&uuidIds,arrayIterator,uuidId,error == ERROR_NONE)
  {
    error = pruneUUID(indexHandle,
                      doneFlag,
                      deletedCounter,
                      uuidId
                     );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&uuidIds);
    return error;
  }
  Array_done(&uuidIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : isEmptyEntity
* Purpose: check if entity if empty
* Input  : indexHandle - index handle
*          entityId    - entity database id
* Output : -
* Return : TRUE iff entity is empty
* Notes  : -
\***********************************************************************/

LOCAL bool isEmptyEntity(IndexHandle *indexHandle,
                         DatabaseId  entityId
                        )
{
  assert(indexHandle != NULL);

  return    (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
         && !Database_exists(&indexHandle->databaseHandle,
                             "storages",
                             "id",
                             "WHERE entityId=%lld",
                             entityId
                            )
         && !Database_exists(&indexHandle->databaseHandle,
                             "entries",
                             "id",
                             "WHERE entityId=%lld",
                             entityId
                            )
         && !Database_exists(&indexHandle->databaseHandle,
                             "entriesNewest",
                             "id",
                             "WHERE entityId=%lld",
                             entityId
                            );
}

/***********************************************************************\
* Name   : pruneEntity
* Purpose: purge entity if empty, is not default entity and is not
*          locked, purge UUID if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          entityId       - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneEntity(IndexHandle *indexHandle,
                         bool        *doneFlag,
                         ulong       *deletedCounter,
                         DatabaseId  entityId
                        )
{
  String              string;
  int64               lockedCount;
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidId;
  StaticString        (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  ArchiveTypes        archiveType;

  assert(indexHandle != NULL);

  // init variables
  string = String_new();

  if (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
  {
    // get locked count
    error = Database_getInteger64(&indexHandle->databaseHandle,
                                  &lockedCount,
                                  "entities",
                                  "lockedCount",
                                  "WHERE id=%lld",
                                  entityId
                                 );
    if (error != ERROR_NONE)
    {
      String_delete(string);
      return error;
    }

    // prune if not locked entity and empty
    if ((lockedCount == 0LL) && isEmptyEntity(indexHandle,entityId))
    {
      // get uuid id, job UUID, created date/time, archive type
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT uuids.id, \
                                       entities.jobUUID, \
                                       UNIXTIMESTAMP(entities.created), \
                                       entities.type \
                                FROM entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE entities.id=%lld \
                               ",
                               entityId
                              );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        return error;
      }
      if (!Database_getNextRow(&databaseStatementHandle,
                               "%llu %S %llu %u",
                               &uuidId,
                               jobUUID,
                               &createdDateTime,
                               &archiveType
                              )
         )
      {
        String_clear(jobUUID);
        createdDateTime = 0LL;
        archiveType     = ARCHIVE_TYPE_NONE;
      }
      Database_finalize(&databaseStatementHandle);

      // delete entity from index
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entities WHERE id=%lld",
                               entityId
                              );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        return error;
      }

      // purge skipped entries of entity
      error = purge(indexHandle,
                    doneFlag,
                    deletedCounter,
                    "skippedEntries",
                    "entityId=%lld",
                    entityId
                   );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        return error;
      }

      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Purged entity #%llu, job %s, created at %s: no archives",
                  entityId,
                  String_cString(jobUUID),
                  String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,NULL))
                 );

      // prune UUID
      if (uuidId != DATABASE_ID_NONE)
      {
        error = pruneUUID(indexHandle,
                          doneFlag,
                          deletedCounter,
                          uuidId
                         );
        if (error != ERROR_NONE)
        {
          String_delete(string);
          return error;
        }
      }
    }
  }

  // free resources
  String_delete(string);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneEntities
* Purpose: prune all empty entities
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneEntities(IndexHandle *indexHandle,
                           bool        *doneFlag,
                           ulong       *deletedCounter
                          )
{
  Array         entityIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    entityId;

  assert(indexHandle != NULL);

  // init variables
  Array_init(&entityIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all entity ids (Note: skip default entity!)
  error = Database_getIds(&indexHandle->databaseHandle,
                          &entityIds,
                          "entities",
                          "id",
                          "WHERE id!=%lld \
                          ",
                          INDEX_DEFAULT_ENTITY_DATABASE_ID
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&entityIds);
    return error;
  }

  // prune entities
  ARRAY_ITERATEX(&entityIds,arrayIterator,entityId,error == ERROR_NONE)
  {
    error = pruneEntity(indexHandle,doneFlag,deletedCounter,entityId);
  }
  if (error != ERROR_NONE)
  {
    Array_done(&entityIds);
    return error;
  }

  // free resources
  Array_done(&entityIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeEntry
* Purpose: purge entry
* Input  : indexHandle - index handle
*          entryId     - entry database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: use
#if 0
LOCAL Errors purgeEntry(IndexHandle *indexHandle,
                        DatabaseId  entryId
                       )
{
  Errors              error;
//  DatabaseStatementHandle databaseStatementHandle;
//  uint64              createdDateTime;
//  bool                transactionFlag;
//  bool                doneFlag;
  #ifndef NDEBUG
//    ulong               deletedCounter;
  #endif

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);

  // init variables

//    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
//                                      indexHandle,
//                                      transactionFlag,
//    {

  error = ERROR_NONE;

  // purge FTS entry
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM FTS_entries \
                              WHERE entryId=%lld \
                             ",
                             entryId
                            );
  }

  // update newest entries
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest \
                              WHERE entryId=%lld \
                             ",
                             entryId
                            );
//TODO
#if 0
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "INSERT OR IGNORE INTO entriesNewest \
                                ( \
                                 uuidId,\
                                 entityId, \
                                 type, \
                                 name, \
                                 timeLastAccess, \
                                 timeModified, \
                                 timeLastChanged, \
                                 userId, \
                                 groupId, \
                                 permission \
                                ) \
                              SELECT \
                                uuidId,\
                                entityId, \
                                type, \
                                name, \
                                timeLastAccess, \
                                timeModified, \
                                timeLastChanged, \
                                userId, \
                                groupId, \
                                permission \
                              FROM entries \
                              WHERE id!=%lld AND name=%'S \
                              ORDER BY timeModified DESC \
                              LIMIT 0,1 \
                              WHERE name \
                             ",
                             entryId,
                             name
                            );
#endif
  }

  // delete entry
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries \
                              WHERE id=%lld \
                             ",
                             entryId
                            );
  }

  // free resources

  return error;
}
#endif

#if 0
//TODO: still not used
/***********************************************************************\
* Name   : pruneEntries
* Purpose: prune all entries (file, image, hardlink) with no fragments
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneEntries(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter
                         )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(doneFlag != NULL);
  assert(deletedCounter != NULL);

  // init vairables
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all file entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "fileEntries",
                          "fileEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge file entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !quitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "fileEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all image entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "imageEntries",
                          "imageEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge image entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !quitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "imageEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all hardlink entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "hardlinkEntries",
                          "hardlinkEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge hardlink entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !quitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "hardlinkEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // free resources
  Array_done(&databaseIds);

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : getStorageState
* Purpose: get index storage state
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
*          errorMessage        - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getStorageState(IndexHandle *indexHandle,
                             DatabaseId  storageId,
                             IndexStates *indexState,
                             uint64      *lastCheckedDateTime,
                             String      errorMessage
                            )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;

  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT state, \
                                   UNIXTIMESTAMP(lastChecked), \
                                   errorMessage \
                            FROM storages \
                            WHERE id=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (!Database_getNextRow(&databaseStatementHandle,
                           "%d %llu %S",
                           indexState,
                           lastCheckedDateTime,
                           errorMessage
                          )
     )
  {
    (*indexState) = INDEX_STATE_UNKNOWN;
    if (errorMessage != NULL) String_clear(errorMessage);
  }

  Database_finalize(&databaseStatementHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : isEmptyStorage
* Purpose: check if storage if empty
* Input  : indexHandle - index handle
*          storageId   - storage database id
* Output : -
* Return : TRUE iff entity is empty
* Notes  : -
\***********************************************************************/

LOCAL bool isEmptyStorage(IndexHandle *indexHandle,
                          DatabaseId  storageId
                         )
{
  assert(indexHandle != NULL);

  return    (storageId != DATABASE_ID_NONE)
         && !Database_exists(&indexHandle->databaseHandle,
                             "entryFragments",
                              "id",
                              "WHERE storageId=%lld",
                              storageId
                             )
         && !Database_exists(&indexHandle->databaseHandle,
                             "directoryEntries",
                             "id",
                             "WHERE storageId=%lld",
                             storageId
                            )
         && !Database_exists(&indexHandle->databaseHandle,
                             "linkEntries",
                             "id",
                             "WHERE storageId=%lld",
                             storageId
                            )
         && !Database_exists(&indexHandle->databaseHandle,
                             "specialEntries",
                             "id",
                             "WHERE storageId=%lld",
                             storageId
                            );
}

/***********************************************************************\
* Name   : addStorageToNewest
* Purpose: add storage entries to newest entries (if newest)
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addStorageToNewest(IndexHandle  *indexHandle,
                                DatabaseId   storageId,
                                ProgressInfo *progressInfo
                               )
{
  typedef struct EntryNode
  {
    LIST_NODE_HEADER(struct EntryNode);

    DatabaseId entryId;
    DatabaseId uuidId;
    DatabaseId entityId;
    IndexTypes indexType;
    String     name;
    uint64     timeLastChanged;
    uint32     userId;
    uint32     groupId;
    uint32     permission;
    uint64     size;

    struct
    {
      DatabaseId entryId;
      uint64     timeLastChanged;
    } newest;
  } EntryNode;

  typedef struct
  {
    LIST_HEADER(EntryNode);
  } EntryList;

  /***********************************************************************\
  * Name   : freeEntryNode
  * Purpose: free entry node
  * Input  : entryNode - entry node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode);
  void freeEntryNode(EntryNode *entryNode)
  {
    assert(entryNode != NULL);

    String_delete(entryNode->name);
  }

  EntryList           entryList;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entryId;
  DatabaseId          uuidId;
  DatabaseId          entityId;
  uint                indexType;
  String              entryName;
  uint64              timeLastChanged;
  uint32              userId;
  uint32              groupId;
  uint32              permission;
  uint64              size;
  Errors              error;
  EntryNode           *entryNode;
  bool                transactionFlag;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  entryName = String_new();
  error     = ERROR_NONE;

  // get entries info to add
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "      SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM entryFragments \
                                        LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                      WHERE entryFragments.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM directoryEntries \
                                        LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                      WHERE directoryEntries.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM linkEntries \
                                        LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                      WHERE linkEntries.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM specialEntries \
                                        LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                      WHERE specialEntries.storageId=%lld \
                                GROUP BY entries.name \
                               ",
                               storageId,
                               storageId,
                               storageId,
                               storageId
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      while (Database_getNextRow(&databaseStatementHandle,
                                 "%lld %lld %lld %u %S %llu %u %u %u %llu",
                                 &entryId,
                                 &uuidId,
                                 &entityId,
                                 &indexType,
                                 entryName,
                                 &timeLastChanged,
                                 &userId,
                                 &groupId,
                                 &permission,
                                 &size
                                )
            )
      {
        entryNode = LIST_NEW_NODE(EntryNode);
        if (entryNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }

        entryNode->entryId                = entryId;
        entryNode->uuidId                 = uuidId;
        entryNode->entityId               = entityId;
        entryNode->indexType              = (IndexTypes)indexType;
        entryNode->name                   = String_duplicate(entryName);
        entryNode->timeLastChanged        = timeLastChanged;
        entryNode->userId                 = (uint32)userId;
        entryNode->groupId                = (uint32)groupId;
        entryNode->permission             = (uint32)permission;
        entryNode->size                   = (uint64)size;
        entryNode->newest.entryId         = DATABASE_ID_NONE;
        entryNode->newest.timeLastChanged = 0LL;

        List_append(&entryList,entryNode);
      }

      Database_finalize(&databaseStatementHandle);

      return ERROR_NONE;
    });
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // find newest entries for entries to add
//fprintf(stderr,"%s, %d: find newest entries for entries to add %d\n",__FILE__,__LINE__,List_count(&entryList));
  resetProgress(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
//fprintf(stderr,"a");
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                              {
                                assert(count == 2);
                                assert(values != NULL);

                                UNUSED_VARIABLE(columns);
                                UNUSED_VARIABLE(count);
                                UNUSED_VARIABLE(userData);

                                entryNode->newest.entryId         = (values[0] != NULL) ? (DatabaseId)strtoll(values[0],NULL,10) : DATABASE_ID_NONE;
                                entryNode->newest.timeLastChanged = (values[1] != NULL) ? (uint64)strtoull(values[1],NULL,10) : 0LL;

                                return ERROR_NONE;
                              },NULL),
                              NULL,  // changedRowCount
                              "      SELECT entriesNewest.id, \
                                            UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM entryFragments \
                                       LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=entryFragments.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM directoryEntries \
                                       LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=directoryEntries.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM linkEntries \
                                       LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=linkEntries.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM specialEntries \
                                       LEFT JOIN storages ON storages.id=specialEntries.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=specialEntries.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               ORDER BY timeLastChanged DESC \
                               LIMIT 0,1 \
                              ",
                              entryNode->name,
                              entryNode->name,
                              entryNode->name,
                              entryNode->name
                             );
    });
    progressStep(progressInfo);
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // add entries to newest entries
//fprintf(stderr,"%s, %d: add entries to newest entries %d\n",__FILE__,__LINE__,List_count(&entryList));
  INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
//fprintf(stderr,"b");
      if (entryNode->timeLastChanged > entryNode->newest.timeLastChanged)
      {
        INDEX_DOX(error,
                  indexHandle,
        {
          return Database_execute(&indexHandle->databaseHandle,
                                  CALLBACK_(NULL,NULL),  // databaseRowFunction
                                  NULL,  // changedRowCount
                                  "INSERT OR REPLACE INTO entriesNewest \
                                     ( \
                                      entryId, \
                                      uuidId, \
                                      entityId, \
                                      type, \
                                      name, \
                                      timeLastChanged, \
                                      userId, \
                                      groupId, \
                                      permission, \
                                      size \
                                     ) \
                                   VALUES \
                                     ( \
                                       %lld, \
                                       %lld, \
                                       %lld, \
                                       %u, \
                                       %'S, \
                                       %llu, \
                                       %lu, \
                                       %lu, \
                                       %lu, \
                                       %llu \
                                     ) \
                                  ",
                                  entryNode->entryId,
                                  entryNode->uuidId,
                                  entryNode->entityId,
                                  entryNode->indexType,
                                  entryNode->name,
                                  entryNode->timeLastChanged,
                                  entryNode->userId,
                                  entryNode->groupId,
                                  entryNode->permission,
                                  entryNode->size
                                 );
        });
      }

#if 1
      if (error == ERROR_NONE)
      {
        error = interruptOperation(indexHandle,&transactionFlag,5LL*MS_PER_SECOND);
      }
#endif
    }

    return error;
  });
//fprintf(stderr,"%s, %d: add entries to newest entries %d done\n",__FILE__,__LINE__,List_count(&entryList));
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // free resources
  String_delete(entryName);
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : removeStorageFromNewest
* Purpose: remove storage entries from newest entries
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeStorageFromNewest(IndexHandle  *indexHandle,
                                     DatabaseId   storageId,
                                     ProgressInfo *progressInfo
                                    )
{
  typedef struct EntryNode
  {
    LIST_NODE_HEADER(struct EntryNode);

    DatabaseId entryId;
    String     name;

    struct
    {
      DatabaseId entryId;
      DatabaseId uuidId;
      DatabaseId entityId;
      IndexTypes indexType;
      uint64     timeLastChanged;
      uint32     userId;
      uint32     groupId;
      uint32     permission;
      uint64     size;
    } newest;
  } EntryNode;

  typedef struct
  {
    LIST_HEADER(EntryNode);
  } EntryList;

  /***********************************************************************\
  * Name   : freeEntryNode
  * Purpose: free entry node
  * Input  : entryNode - entry node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode);
  void freeEntryNode(EntryNode *entryNode)
  {
    assert(entryNode != NULL);

    String_delete(entryNode->name);
  }

  EntryList           entryList;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entryId;
  String              entryName;
  Errors              error;
  EntryNode           *entryNode;
  bool                transactionFlag;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  entryName = String_new();
  error     = ERROR_NONE;

  // get entries info to remove
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "      SELECT entries.id, \
                                             entries.name \
                                      FROM entryFragments \
                                        LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                      WHERE entryFragments.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM directoryEntries \
                                        LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                      WHERE directoryEntries.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM linkEntries \
                                        LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                      WHERE linkEntries.storageId=%lld \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM specialEntries \
                                        LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                      WHERE specialEntries.storageId=%lld \
                                ORDER BY entries.name \
                               ",
                               storageId,
                               storageId,
                               storageId,
                               storageId
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      while (Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S",
                                 &entryId,
                                 entryName
                                )
            )
      {
        entryNode = LIST_NEW_NODE(EntryNode);
        if (entryNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }

        entryNode->entryId        = entryId;
        entryNode->name           = String_duplicate(entryName);
        entryNode->newest.entryId = DATABASE_ID_NONE;

        List_append(&entryList,entryNode);
      }

      Database_finalize(&databaseStatementHandle);

      return ERROR_NONE;
    });
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // find new newest entries for entries to remove
//fprintf(stderr,"%s, %d: find new newest entries for entries to remove %d\n",__FILE__,__LINE__,List_count(&entryList));
  resetProgress(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
//fprintf(stderr,"c");
    // wait until index is unused
    WAIT_NOT_IN_USE(5LL*MS_PER_SECOND);

    if ((entryNode->prev == NULL) || !String_equals(entryNode->prev->name,entryNode->name))
    {
      INDEX_DOX(error,
                indexHandle,
      {
        return Database_execute(&indexHandle->databaseHandle,
                                CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                {
                                  assert(count == 9);
                                  assert(values != NULL);

                                  UNUSED_VARIABLE(columns);
                                  UNUSED_VARIABLE(count);
                                  UNUSED_VARIABLE(userData);

                                  entryNode->newest.entryId         = (values[0] != NULL) ? (DatabaseId)strtoll(values[0],NULL,10) : DATABASE_ID_NONE;
                                  entryNode->newest.uuidId          = (values[1] != NULL) ? (DatabaseId)strtoll(values[1],NULL,10) : DATABASE_ID_NONE;
                                  entryNode->newest.entityId        = (values[2] != NULL) ? (DatabaseId)strtoll(values[2],NULL,10) : DATABASE_ID_NONE;
                                  entryNode->newest.indexType       = (values[3] != NULL) ? (IndexTypes)(uint)strtoul(values[3],NULL,10) : INDEX_TYPE_NONE;
                                  entryNode->newest.timeLastChanged = (values[4] != NULL) ? (uint64)strtoull(values[4],NULL,10) : 0LL;
                                  entryNode->newest.userId          = (values[5] != NULL) ? (uint32)strtoul(values[5],NULL,10) : 0;
                                  entryNode->newest.groupId         = (values[6] != NULL) ? (uint32)strtoul(values[6],NULL,10) : 0;
                                  entryNode->newest.permission      = (values[7] != NULL) ? (uint32)strtoul(values[7],NULL,10) : 0;
                                  entryNode->newest.size            = (values[8] != NULL) ? (uint64)strtoull(values[8],NULL,10) : 0LL;

                                  return ERROR_NONE;
                                },NULL),
                                NULL,  // changedRowCount
                                "      SELECT entries.id, \
                                              entries.uuidId, \
                                              entries.entityId, \
                                              entries.type, \
                                              UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                              entries.userId, \
                                              entries.groupId, \
                                              entries.permission, \
                                              entries.size \
                                       FROM entryFragments \
                                         LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                         LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                       WHERE     storages.deletedFlag!=1 \
                                             AND entries.name=%'S \
                                 UNION SELECT entries.id, \
                                              entries.uuidId, \
                                              entries.entityId, \
                                              entries.type, \
                                              UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                              entries.userId, \
                                              entries.groupId, \
                                              entries.permission, \
                                              entries.size \
                                       FROM directoryEntries \
                                         LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                         LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                       WHERE     storages.deletedFlag!=1 \
                                             AND entries.name=%'S \
                                 UNION SELECT entries.id, \
                                              entries.uuidId, \
                                              entries.entityId, \
                                              entries.type, \
                                              UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                              entries.userId, \
                                              entries.groupId, \
                                              entries.permission, \
                                              entries.size \
                                       FROM linkEntries \
                                         LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                         LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                       WHERE     storages.deletedFlag!=1 \
                                             AND entries.name=%'S \
                                 UNION SELECT entries.id, \
                                              entries.uuidId, \
                                              entries.entityId, \
                                              entries.type, \
                                              UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                              entries.userId, \
                                              entries.groupId, \
                                              entries.permission, \
                                              entries.size \
                                       FROM specialEntries \
                                         LEFT JOIN storages ON storages.id=specialEntries.storageId \
                                         LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                       WHERE     storages.deletedFlag!=1 \
                                             AND entries.name=%'S \
                                 ORDER BY timeLastChanged DESC \
                                 LIMIT 0,1 \
                                ",
                                entryNode->name,
                                entryNode->name,
                                entryNode->name,
                                entryNode->name
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }

        while (Database_getNextRow(&databaseStatementHandle,
                                   "%lld %S",
                                   &entryId,
                                   entryName
                                  )
              )
        {
          entryNode = LIST_NEW_NODE(EntryNode);
          if (entryNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          entryNode->entryId        = entryId;
          entryNode->name           = String_duplicate(entryName);
          entryNode->newest.entryId = DATABASE_ID_NONE;

          List_append(&entryList,entryNode);
        }

        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });
    }
    progressStep(progressInfo);
  }
//fprintf(stderr,"%s, %d: find new newest entries for entries to remove %d done\n",__FILE__,__LINE__,List_count(&entryList));
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // remove/update entries from newest entries
//fprintf(stderr,"%s, %d: remove/update entries from newest entries %d\n",__FILE__,__LINE__,List_count(&entryList));
  INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
//fprintf(stderr,"d");
      INDEX_DOX(error,
                indexHandle,
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM entriesNewest \
                                  WHERE entryId=%lld \
                                 ",
                                 entryNode->entryId
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (entryNode->newest.entryId != DATABASE_ID_NONE)
        {
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT OR REPLACE INTO entriesNewest \
                                      ( \
                                       entryId, \
                                       uuidId, \
                                       entityId, \
                                       type, \
                                       name, \
                                       timeLastChanged, \
                                       userId, \
                                       groupId, \
                                       permission, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                        %lld, \
                                        %lld, \
                                        %lld, \
                                        %u, \
                                        %'S, \
                                        %llu, \
                                        %lu, \
                                        %lu, \
                                        %lu, \
                                        %llu \
                                      ) \
                                   ",
                                   entryNode->newest.entryId,
                                   entryNode->newest.uuidId,
                                   entryNode->newest.entityId,
                                   entryNode->newest.indexType,
                                   entryNode->name,
                                   entryNode->newest.timeLastChanged,
                                   entryNode->newest.userId,
                                   entryNode->newest.groupId,
                                   entryNode->newest.permission,
                                   entryNode->newest.size
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        return ERROR_NONE;
      });

#if 1
      if (error == ERROR_NONE)
      {
        error = interruptOperation(indexHandle,&transactionFlag,5LL*MS_PER_SECOND);
      }
#endif
    }

    return error;
  });
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // free resources
  String_delete(entryName);
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : collectEntryIds
* Purpose: collect entry ids for storage
* Input  : entryIds     - entry ids array variable
*          indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : entryIds     - entry ids array
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors collectEntryIds(Array        *entryIds,
                             IndexHandle  *indexHandle,
                             DatabaseId   storageId,
                             ProgressInfo *progressInfo
                            )
{
  Errors error;
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  Array_clear(entryIds);
  error = ERROR_NONE;

  // collect file/image/hardlink entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // get file/image/hardlink entry ids to check for purge
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "entryFragments",
                             "entryId",
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[0] = Misc_getTimestamp()-t0;
  #endif

  // collect directory/link/special entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "directoryEntries",
                             "entryId",
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[1] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "linkEntries",
                             "entryId",
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[2] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "specialEntries",
                             "entryId",
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[3] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries to purge: fragment %llums, directory %llums, link %llums, special %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            Array_length(&entryIds),
            dt[0]/US_PER_MS,
            dt[1]/US_PER_MS,
            dt[2]/US_PER_MS,
            dt[3]/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : clearStorageFragments
* Purpose: purge storage fragments
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageFragments(IndexHandle  *indexHandle,
                                   DatabaseId   storageId,
                                   ProgressInfo *progressInfo
                                  )
{
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;

  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  // purge fragments
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"entryFragments","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: fragments %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = purge(indexHandle,
                      &doneFlag,
                      #ifndef NDEBUG
                        &deletedCounter,
                      #else
                        NULL,  // deletedCounter
                      #endif
                      "entryFragments",
                      "storageId=%lld",
                      storageId
                     );
        if (error == ERROR_NONE)
        {
          error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
#ifndef NDEBUG
//fprintf(stderr,"%s, %d: 1 fragments done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));
#endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : clearStorageFTSEntries
* Purpose: purge storage FTS entries
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageFTSEntries(IndexHandle  *indexHandle,
                                    ProgressInfo *progressInfo,
                                    Array        *entryIds
                                   )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "FTS_entries",
                        "entryId IN (%S)",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged FTS entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageSubEntries
* Purpose: purge storage file/image/directory/link/hardlink/special sub
*          entries
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageSubEntries(IndexHandle  *indexHandle,
                                    DatabaseId   storageId,
                                    ProgressInfo *progressInfo,
                                    Array        *entryIds
                                   )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge file entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "fileEntries",
                        "    entryId IN (%S) \
                         AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 0,1) \
                        ",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged file entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge image entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "imageEntries",
                        "    entryId IN (%S) \
                         AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 0,1) \
                        ",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged image entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge directory entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"fileEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: directoryEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = purge(indexHandle,
                      &doneFlag,
                      #ifndef NDEBUG
                        &deletedCounter,
                      #else
                        NULL,  // deletedCounter
                      #endif
                      "directoryEntries",
                      "storageId=%lld",
                      storageId
                     );
        if (error == ERROR_NONE)
        {
          error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged directory entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge link entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"fileEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: linkEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = purge(indexHandle,
                      &doneFlag,
                      #ifndef NDEBUG
                        &deletedCounter,
                      #else
                        NULL,  // deletedCounter
                      #endif
                      "linkEntries",
                      "storageId=%lld",
                      storageId
                     );
        if (error == ERROR_NONE)
        {
          error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged link entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge hardlink entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "hardlinkEntries",
                        "    entryId IN (%S) \
                         AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 0,1) \
                        ",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged hardlink entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge special entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"specialEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: specialEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = purge(indexHandle,
                      &doneFlag,
                      #ifndef NDEBUG
                        &deletedCounter,
                      #else
                        NULL,  // deletedCounter
                      #endif
                      "specialEntries",
                      "storageId=%lld",
                      storageId
                     );
        if (error == ERROR_NONE)
        {
          error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged special entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageEntries
* Purpose: purge storage  entries
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageEntries(IndexHandle  *indexHandle,
                                 ProgressInfo *progressInfo,
                                 Array        *entryIds
                                )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (%S)",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageAggregates
* Purpose: clear storage aggregates
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageAggregates(IndexHandle  *indexHandle,
                                    DatabaseId   storageId,
                                    ProgressInfo *progressInfo
                                   )
{
  Errors               error;
  DatabaseId           entityId;
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // clear storage aggregates
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              DATABASE_COLUMN_TYPES(),
                              NULL,  // changedRowCount
                              "UPDATE storages \
                               SET totalEntryCount          =0, \
                                   totalEntrySize           =0, \
                                   totalFileCount           =0, \
                                   totalFileSize            =0, \
                                   totalImageCount          =0, \
                                   totalImageSize           =0, \
                                   totalDirectoryCount      =0, \
                                   totalLinkCount           =0, \
                                   totalHardlinkCount       =0, \
                                   totalHardlinkSize        =0, \
                                   totalSpecialCount        =0, \
                                   \
                                   totalEntryCountNewest    =0, \
                                   totalEntrySizeNewest     =0, \
                                   totalFileCountNewest     =0, \
                                   totalFileSizeNewest      =0, \
                                   totalImageCountNewest    =0, \
                                   totalImageSizeNewest     =0, \
                                   totalDirectoryCountNewest=0, \
                                   totalLinkCountNewest     =0, \
                                   totalHardlinkCountNewest =0, \
                                   totalHardlinkSizeNewest  =0, \
                                   totalSpecialCountNewest  =0 \
                               WHERE id=%lld \
                              ",
                              storageId
                             );
    });
  }
  if (error == ERROR_NONE)
  {
    // update entity aggregates
    error = Database_getId(&indexHandle->databaseHandle,
                           &entityId,
                           "storages",
                           "entityId",
                           "WHERE id=%lld \
                           ",
                           storageId
                          );
    if (error == ERROR_NONE)
    {
      error = updateEntityAggregates(indexHandle,entityId);
    }
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, updated aggregates: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : clearStorage
* Purpose: clear index storage content
* Input  : indexHandle  - index handle
*          storageId    - database id of storage
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  Array                entryIds;
  //bool                 transactionFlag;
  Errors               error;
  //bool                 doneFlag;
  //ArraySegmentIterator arraySegmentIterator;
  //ArrayIterator        arrayIterator;
//  DatabaseId           entryId;
  //DatabaseId           entityId;
  #ifndef NDEBUG
    //ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // lock
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage #%"PRIi64"\n",__FILE__,__LINE__,storageId);
  #endif
  Semaphore_lock(&indexClearStorageLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage locked\n",__FILE__,__LINE__);
  #endif

  // init variables
  Array_init(&entryIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error           = ERROR_NONE;

//TODO: do regulary in index thread?
#if 0
  /* get entries to purge without associated file/image/directory/link/hardlink/special entry
     Note: may be left from interrupted purge of previous run
  */
//l=0; Database_getInteger64(&indexHandle->databaseHandle,&l,"entries","count(id)",""); fprintf(stderr,"%s, %d: l=%lld\n",__FILE__,__LINE__,l);
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  Array_clear(&entryIds);
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                              WHERE entries.type=%u AND fileEntries.id IS NULL \
                             ",
                             INDEX_TYPE_FILE
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[0] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                              WHERE entries.type=%u AND imageEntries.id IS NULL \
                             ",
                             INDEX_TYPE_IMAGE
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[1] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                              WHERE entries.type=%u AND directoryEntries.id IS NULL \
                             ",
                             INDEX_TYPE_DIRECTORY
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[2] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                              WHERE entries.type=%u AND linkEntries.id IS NULL \
                             ",
                             INDEX_TYPE_LINK
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[3] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                              WHERE entries.type=%u AND hardlinkEntries.id IS NULL \
                             ",
                             INDEX_TYPE_HARDLINK
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[4] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                              WHERE entries.type=%u AND specialEntries.id IS NULL \
                             ",
                             INDEX_TYPE_SPECIAL
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[5] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries without associated entry to purge: file %llums, image %llums, directory %llums, link %llums, hardlink %llums, special %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            Array_length(&entryIds),
            dt[0]/US_PER_MS,
            dt[1]/US_PER_MS,
            dt[2]/US_PER_MS,
            dt[3]/US_PER_MS,
            dt[4]/US_PER_MS,
            dt[5]/US_PER_MS
           );
  #endif

  /* purge entries without associated file/image/directory/link/hardlink/special entry
  */
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(&entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(&entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (%S)",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return ERROR_NONE;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged orphaned entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
#endif

  // collect file/image/diretory/link/hardlink/special entries to purge
  if (error == ERROR_NONE)
  {
    error = collectEntryIds(&entryIds,
                            indexHandle,
                            storageId,
                            progressInfo
                           );
  }

  // purge entry fragments
  if (error == ERROR_NONE)
  {
    error = clearStorageFragments(indexHandle,
                                  storageId,
                                  progressInfo
                                 );
  }

  // purge FTS entries
  if (error == ERROR_NONE)
  {
    error = clearStorageFTSEntries(indexHandle,
                                   progressInfo,
                                   &entryIds
                                  );
  }

  // purge file/image/directory/link/hardlink/special entries
  if (error == ERROR_NONE)
  {
    error = clearStorageSubEntries(indexHandle,
                                   storageId,
                                   progressInfo,
                                   &entryIds
                                  );
  }

  // purge entries
  if (error == ERROR_NONE)
  {
    error = clearStorageEntries(indexHandle,
                                progressInfo,
                                &entryIds
                               );
  }

  // remove from newest entries
  if (error == ERROR_NONE)
  {
    #ifdef INDEX_DEBUG_PURGE
      t0 = Misc_getTimestamp();
    #endif
    if (error == ERROR_NONE)
    {
      error = removeStorageFromNewest(indexHandle,
                                      storageId,
                                      progressInfo
                                     );
    }
    #ifdef INDEX_DEBUG_PURGE
      fprintf(stderr,"%s, %d: error: %s, removed from newest entries: %llums\n",__FILE__,__LINE__,
              Error_getText(error),
              (Misc_getTimestamp()-t0)/US_PER_MS
             );
    #endif
  }

  // clear aggregates
  if (error == ERROR_NONE)
  {
    error = clearStorageAggregates(indexHandle,
                                   storageId,
                                   progressInfo
                                  );
  }

  // unlock
  Semaphore_unlock(&indexClearStorageLock);
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage unlocked\n",__FILE__,__LINE__);
  #endif

  // free resources
  Array_done(&entryIds);

  return error;
}

/***********************************************************************\
* Name   : purgeStorage
* Purpose: purge storage (mark as "deleted")
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  String              name;
  String              string;
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  uint64              createdDateTime;
  bool                transactionFlag;
  bool                doneFlag;
  #ifndef NDEBUG
    ulong               deletedCounter;
  #endif

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  name   = String_new();
  string = String_new();

  // get storage name, created date/time
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT storages.name, \
                                   UNIXTIMESTAMP(entities.created) \
                            FROM storages \
                              LEFT JOIN entities ON entities.id=storages.entityId \
                            WHERE storages.id=%lld \
                           ",
                           storageId
                          );
  if (error == ERROR_NONE)
  {
    if (!Database_getNextRow(&databaseStatementHandle,
                             "%'S %llu",
                             name,
                             &createdDateTime
                            )
       )
    {
      error = ERRORX_(DATABASE,0,"prune storages");
    }
    Database_finalize(&databaseStatementHandle);
  }
  if (error != ERROR_NONE)
  {
    String_clear(name);
    createdDateTime = 0LL;
  }

  // clear storage
  if (error == ERROR_NONE)
  {
    error = clearStorage(indexHandle,storageId,progressInfo);
  }

  // purge FTS storages
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
        doneFlag = TRUE;
        error = purge(indexHandle,
                      &doneFlag,
                      #ifndef NDEBUG
                        &deletedCounter,
                      #else
                        NULL,  // deletedCounter
                      #endif
                      "FTS_storages",
                      "storageId=%lld",
                      storageId
                     );
        if (error == ERROR_NONE)
        {
          error = interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }

  // delete storage
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM storages \
                              WHERE id=%lld \
                             ",
                             storageId
                            );
  }

  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(name);
    return error;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Removed deleted storage #%llu from index: %s, created at %s",
              storageId,
              String_cString(name),
              (createdDateTime != 0LL) ? String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,NULL)) : "unknown"
             );

  // free resources
  String_delete(string);
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneStorage
* Purpose: prune storage if empty, prune entity/UUID
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  String              name;
  String              string;
  IndexStates         indexState;
  Errors              error;
  DatabaseId          entityId;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  name   = String_new();
  string = String_new();

  // get storage state
  error = getStorageState(indexHandle,
                          storageId,
                          &indexState,
                          NULL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(name);
    return error;
  }

  // prune storage if not in error state/in use and empty
  if ((indexState == INDEX_STATE_OK) && isEmptyStorage(indexHandle,storageId))
  {
    // get entity id
    error = Database_getId(&indexHandle->databaseHandle,
                           &entityId,
                           "storages",
                           "entityId",
                           "WHERE id=%lld",
                           storageId
                          );
    if (error != ERROR_NONE)
    {
      entityId = DATABASE_ID_NONE;
    }

    // purge storage
    error = purgeStorage(indexHandle,storageId,progressInfo);
    if (error != ERROR_NONE)
    {
      String_delete(string);
      String_delete(name);
      return error;
    }

    // prune entity
    if (entityId != DATABASE_ID_NONE)
    {
      error = pruneEntity(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        String_delete(name);
        return error;
      }
    }
  }

  // free resources
  String_delete(string);
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneStorages
* Purpose: prune all empty storages which have state OK or ERROR
* Input  : indexHandle  - index handle
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorages(IndexHandle  *indexHandle,
                           ProgressInfo *progressInfo
                          )
{
  Array         storageIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    storageId;

  assert(indexHandle != NULL);

  // init variables
  Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all storage ids
  error = Database_getIds(&indexHandle->databaseHandle,
                          &storageIds,
                          "storages",
                          "id",
                          "WHERE state IN (%u,%u) \
                          ",
                          INDEX_STATE_OK,
                          INDEX_STATE_ERROR
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    return error;
  }

  // prune storages
  ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
  {
    error = pruneStorage(indexHandle,storageId,progressInfo);
  }
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    return error;
  }

  // free resources
  Array_done(&storageIds);

  return ERROR_NONE;
}

#if 0
//TODO: not used, remove
/***********************************************************************\
* Name   : refreshStoragesInfos
* Purpose: refresh storages info
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshStoragesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          indexId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount,
                                 INDEX_TYPE_SET_ANY,
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
                                &storageId,
                                NULL,  // storageName,
                                NULL,  // dateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      error = Index_updateStorageInfos(indexHandle,indexId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}

/***********************************************************************\
* Name   : refreshEntitiesInfos
* Purpose: refresh entities infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshEntitiesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 NULL,  // jobUUID,
                                 NULL,  // scheduldUUID
                                 ARCHIVE_TYPE_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextEntity(&indexQueryHandle,
                               NULL,  // uuidId,
                               NULL,  // jobUUID,
                               NULL,  // scheduleUUID,
                               &entityId,
                               NULL,  // archiveType,
                               NULL,  // createdDateTime,
                               NULL,  // lastErrorMessage
                               NULL,  // totalSize
                               NULL,  // totalEntryCount
                               NULL,  // totalEntrySize
                               NULL  // lockedCount
                              )
          )
    {
      error = Index_updateEntityInfos(indexHandle,entityId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}

//TODO: not used, remove
/***********************************************************************\
* Name   : refreshUUIDsInfos
* Purpose: refresh UUIDs infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshUUIDsInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          indexId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListUUIDs(&indexQueryHandle,
                              indexHandle,
                              INDEX_STATE_SET_ALL,
                              INDEX_MODE_SET_ALL,
                              NULL,  // name
                              0LL,  // offset
                              INDEX_UNLIMITED
                             );
  if (error == ERROR_NONE)
  {
    while (Index_getNextUUID(&indexQueryHandle,
                             &indexId,
                             NULL,  // jobUUID
                             NULL,  // lastCheckedDateTime
                             NULL,  // lastErrorMessage
                             NULL,  // size
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
          )
    {
      error = Index_updateUUIDInfos(indexHandle,indexId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}
#endif

/***********************************************************************\
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO
#if 0
// not used
LOCAL Errors rebuildNewestInfo(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entryId;
  String           name;
  uint64           size;
  uint64           timeModified;


  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // indexIds
                                0L,  // indexIdCount
                                NULL,  // entryIds
                                0L,  // entryIdCount
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                FALSE,  // newestOnly
                                FALSE,  // fragmentsCount
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  name = String_new();
  while (Index_getNextEntry(&indexQueryHandle,
                            NULL,  // uuidId
                            NULL,  // jobUUID
                            NULL,  // entityId
                            NULL,  // scheduleUUID
                            NULL,  // hostName
                            NULL,  // userName
                            NULL,  // archiveType
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // storageDateTime
                            &entryId,
                            name,
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // destinationName
                            NULL,  // fileSystemType
                            &size,
                            &timeModified,
                            NULL,  // userId
                            NULL,  // groupId
                            NULL,  // permission
                            NULL  // fragmentCount
                           )
        )
  {
  }
  String_delete(name);
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : initIndexQueryHandle
* Purpose: init index query handle
* Input  : indexQueryHandle - index query handle
*          indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  indexQueryHandle->indexHandle = indexHandle;
}

/***********************************************************************\
* Name   : doneIndexQueryHandle
* Purpose: done index query handle
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneIndexQueryHandle(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  UNUSED_VARIABLE(indexQueryHandle);
}

/***********************************************************************\
* Name   : getIndexStateSetString
* Purpose: get index state filter string
* Input  : string        - string variable
*          indexStateSet - index state set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexStateSetString(String string, IndexStateSet indexStateSet)
{
  IndexStates indexState;

  String_clear(string);
  for (indexState = INDEX_STATE_MIN; indexState <= INDEX_STATE_MAX; indexState++)
  {
    if (IN_SET(indexStateSet,indexState))
    {
      if (!String_isEmpty(string)) String_appendChar(string,',');
      String_appendFormat(string,"%d",indexState);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getIndexModeSetString
* Purpose: get index mode filter string
* Input  : string       - string variable
*          indexModeSet - index mode set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexModeSetString(String string, IndexModeSet indexModeSet)
{
  IndexModes indexMode;

  String_clear(string);
  for (indexMode = INDEX_MODE_MIN; indexMode <= INDEX_MODE_MAX; indexMode++)
  {
    if (IN_SET(indexModeSet,indexMode))
    {
      if (!String_isEmpty(string)) String_appendChar(string,',');
      String_appendFormat(string,"%d",indexMode);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getFTSString
* Purpose: get full-text-search filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedTextFlag,addedPatternFlag;
  size_t          iteratorVariable;
  Codepoint       codepoint;

  String_clear(string);

  if (!String_isEmpty(patternText))
  {
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      addedTextFlag    = FALSE;
      addedPatternFlag = FALSE;
      STRING_CHAR_ITERATE_UTF8(token,iteratorVariable,codepoint)
      {
        if (isalnum(codepoint) || (codepoint >= 128))
        {
          if (addedPatternFlag)
          {
            String_appendChar(string,' ');
            addedPatternFlag = FALSE;
          }
          String_appendCharUTF8(string,codepoint);
          addedTextFlag = TRUE;
        }
        else
        {
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(string,'*');
            addedTextFlag    = FALSE;
            addedPatternFlag = TRUE;
          }
        }
      }
      if (addedTextFlag && !addedPatternFlag)
      {
        String_appendChar(string,'*');
      }
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
}

/***********************************************************************\
* Name   : filterAppend
* Purpose: append to SQL filter string
* Input  : filterString - filter string
*          condition    - append iff true
*          concatenator - concatenator string
*          format       - format string (printf-style)
*          ...          - optional arguments for format
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void filterAppend(String filterString, bool condition, const char *concatenator, const char *format, ...)
{
  va_list arguments;

  if (condition)
  {
    if (!String_isEmpty(filterString))
    {
      String_appendChar(filterString,' ');
      String_appendCString(filterString,concatenator);
      String_appendChar(filterString,' ');
    }
    va_start(arguments,format);
    String_appendVFormat(filterString,format,arguments);
    va_end(arguments);
  }
}

/***********************************************************************\
* Name   : appendOrdering
* Purpose: append to SQL ordering string
* Input  : filterString - filter string
*          condition    - append iff true
*          columnName   - column name
*          ordering     - database ordering
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendOrdering(String orderString, bool condition, const char *columnName, DatabaseOrdering ordering)
{
  if (condition && (ordering != DATABASE_ORDERING_NONE))
  {
    if (String_isEmpty(orderString))
    {
      String_appendCString(orderString,"ORDER BY ");
    }
    else
    {
      String_appendChar(orderString,',');
    }
    String_appendCString(orderString,columnName);
    switch (ordering)
    {
      case DATABASE_ORDERING_NONE:       /* nothing tod do */                       break;
      case DATABASE_ORDERING_ASCENDING:  String_appendCString(orderString," ASC");  break;
      case DATABASE_ORDERING_DESCENDING: String_appendCString(orderString," DESC"); break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }
}

// ----------------------------------------------------------------------

#if 0
//TODO: trigger or implementation here?
/***********************************************************************\
* Name   : insertUpdateNewestEntry
* Purpose: insert or update newest entry
* Input  : indexHandle     - index handle
*          entryId         - entry database id
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          name            - file name
*          size            - size [bytes]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors insertUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     DatabaseId  uuidId,
                                     DatabaseId  entityId,
                                     ConstString name,
                                     IndexTypes  indexType,
                                     uint64      size,
                                     uint64      timeLastChanged,
                                     uint32      userId,
                                     uint32      groupId,
                                     uint32      permission
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          newestEntryId;
  uint64              newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    // atomic add/get entry
    DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // get existing newest entry
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT id, \
                                       UNIXTIMESTAMP(timeLastChanged) \
                                FROM entriesNewest\
                                WHERE name=%'S \
                               ",
                               name
                              );
      if (error == ERROR_NONE)
      {
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lld %llu",
                                 &newestEntryId,
                                 &newestTimeLastChanged
                                )
           )
        {
          newestEntryId         = DATABASE_ID_NONE;
          newestTimeLastChanged = 0LL;
        }
        Database_finalize(&databaseStatementHandle);
      }

      // insert/update newest
      if (error == ERROR_NONE)
      {
        if (timeLastChanged > newestTimeLastChanged)
        {
          if (newestEntryId != DATABASE_ID_NONE)
          {
            // update
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE entriesNewest \
                                      SET entryId=%lld, \
                                          uuidId=%lld, \
                                          entityId=%lld, \
                                          type=%u, \
                                          timeLastChanged=%lld, \
                                          userId=%u, \
                                          groupId=%u, \
                                          permission=%u \
                                      WHERE id=%lld \
                                     ",
                                     entryId,
                                     uuidId,
                                     entityId,
                                     indexType,
                                     timeLastChanged,
                                     userId,
                                     groupId,
                                     permission,
                                     newestEntryId
                                    );
          }
          else
          {
            // insert
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO entriesNewest \
                                        ( \
                                         entryId, \
                                         uuidId, \
                                         entityId, \
                                         type, \
                                         name, \
                                         timeLastChanged, \
                                         userId, \
                                         groupId, \
                                         permission, \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %lld, \
                                         %lld, \
                                         %u, \
                                         %'S, \
                                         %llu, \
                                         %u, \
                                         %u, \
                                         %u \
                                        ) \
                                     ",
                                     entryId,
                                     uuidId,
                                     entityId,
                                     indexType,
                                     name,
                                     timeLastChanged,
                                     userId,
                                     groupId,
                                     permission
                                    );
          }
        }
      }
    }

    return error;
  });

  return error;
}

/***********************************************************************\
* Name   : removeUpdateNewestEntry
* Purpose: remove or update newest entry
* Input  : indexHandle - index handle
*          entryId     - entry database id
*          name        - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     ConstString name
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          newestEntryId;
  uint64              newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest \
                              WHERE id=%lld \
                             ",
                             entryId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "INSERT OR IGNORE INTO entriesNewest \
                                ( \
                                 entryId, \
                                 uuidId, \
                                 entityId, \
                                 type, \
                                 name, \
                                 timeLastChanged, \
                                 userId, \
                                 groupId, \
                                 permission, \
                                ) \
                              SELECT id, \
                                     uuidId, \
                                     entityId, \
                                     type, \
                                     name, \
                                     timeLastChanged, \
                                     userId, \
                                     groupId, \
                                     permission \
                               FROM entries \
                               WHERE name=%'S \
                               ORDER BY timeLastChanged DESC \
                               LIMIT 0,1 \
                             ",
                             name
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}
#endif

/***********************************************************************\
* Name   : indexThreadCode
* Purpose: index upgrade and cleanup thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexThreadCode(void)
{
  IndexHandle         indexHandle;
  Errors              error;
  #ifdef INDEX_IMPORT_OLD_DATABASE
    String              absoluteFileName;
    String              directoryName;
    DirectoryListHandle directoryListHandle;
    uint                i;
    String              oldDatabaseFileName;
    uint                oldDatabaseCount;
    String              failFileName;
  #endif /* INDEX_IMPORT_OLD_DATABASE */
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          storageId,entityId;
  String              storageName;
  uint                sleepTime;

  assert(indexDatabaseFileName != NULL);

  // open index
  do
  {
    error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,INDEX_PURGE_TIMEOUT);
    if ((error != ERROR_NONE) && !quitFlag)
    {
      Misc_mdelay(1*MS_PER_SECOND);
    }
  }
  while ((error != ERROR_NONE) && !quitFlag);
  if (quitFlag)
  {
    if (error == ERROR_NONE) closeIndex(&indexHandle);
    return;
  }
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot open index database '%s' fail: %s",
                indexDatabaseFileName,
                Error_getText(error)
               );
    return;
  }

  // set index handle for thread interruption
  indexThreadIndexHandle = &indexHandle;

  #ifdef INDEX_IMPORT_OLD_DATABASE
    // get absolute database file name
    absoluteFileName = File_getAbsoluteFileNameCString(String_new(),indexDatabaseFileName);

    // open directory where database is located
    directoryName = File_getDirectoryName(String_new(),absoluteFileName);
    error = File_openDirectoryList(&directoryListHandle,directoryName);
    if (error != ERROR_NONE)
    {
      String_delete(directoryName);
      closeIndex(&indexHandle);
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Import index database '%s' fail: %s",
                  indexDatabaseFileName,
                  Error_getText(error)
                 );
      return;
    }
    String_delete(directoryName);

    // process all *.oldNNN files
    #ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
      logImportIndexHandle = fopen(IMPORT_INDEX_LOG_FILENAME,"w");
    #endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */
    i                   = 0;
    oldDatabaseFileName = String_new();
    oldDatabaseCount    = 0;
    while (   !quitFlag
           && (File_readDirectoryList(&directoryListHandle,oldDatabaseFileName) == ERROR_NONE)
          )
    {
      if (   String_startsWith(oldDatabaseFileName,absoluteFileName)\
          && String_matchCString(oldDatabaseFileName,STRING_BEGIN,".*\\.old\\d\\d\\d$",NULL,NULL)
         )
      {
        if (i == 0)
        {
          plogMessage(NULL,  // logHandle
                      LOG_TYPE_INDEX,
                      "INDEX",
                      "Started import old index databases"
                     );
        }
        DIMPORT("import %s -> %s",String_cString(oldDatabaseFileName),indexDatabaseFileName);
        error = importIndex(&indexHandle,oldDatabaseFileName);
        if (error == ERROR_NONE)
        {
          oldDatabaseCount++;
          (void)File_delete(oldDatabaseFileName,FALSE);
        }
        else
        {
          failFileName = String_appendCString(String_duplicate(oldDatabaseFileName),".fail");
          (void)File_rename(oldDatabaseFileName,failFileName,NULL);
          String_delete(failFileName);
        }

        i++;
      }
    }
    #ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
      if (logImportIndexHandle != NULL)
      {
        fclose(logImportIndexHandle);
      }
    #endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */
    if (i > 0)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Done import %d old index databases",
                  oldDatabaseCount
                 );
    }
    String_delete(oldDatabaseFileName);

    // close directory
    File_closeDirectoryList(&directoryListHandle);

    // free resources
    String_delete(absoluteFileName);
  #endif /* INDEX_IMPORT_OLD_DATABASE */

  // index is initialized and ready to use
  indexInitializedFlag = TRUE;

  // regular clean-ups
  storageName = String_new();
  while (!quitFlag)
  {
    #ifdef INDEX_SUPPORT_DELETE
      // remove deleted storages from index if maintenance time
      if (isMaintenanceTime(Misc_getCurrentDateTime()))
      {
        do
        {
          error = ERROR_NONE;

          // wait until index is unused
          WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !isMaintenanceTime(Misc_getCurrentDateTime())
              || quitFlag
             )
          {
            break;
          }

          // find next storage to remove (Note: get single entry for remove to avoid long-running prepare!)
          INDEX_DOX(error,
                    &indexHandle,
          {
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle.databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     "SELECT id, \
                                             entityId, \
                                             name \
                                      FROM storages \
                                      WHERE     state!=%u \
                                            AND deletedFlag=1 \
                                      LIMIT 0,1 \
                                     ",
                                     INDEX_STATE_UPDATE
                                    );
            if (error == ERROR_NONE)
            {
              if (!Database_getNextRow(&databaseStatementHandle,
                                       "%lld %lld %S",
                                       &storageId,
                                       &entityId,
                                       storageName
                                      )
                 )
              {
                storageId = DATABASE_ID_NONE;
              }

              Database_finalize(&databaseStatementHandle);
            }
            else
            {
              storageId = DATABASE_ID_NONE;
            }

            return error;
          });
          if (   !isMaintenanceTime(Misc_getCurrentDateTime())
              || quitFlag
             )
          {
            break;
          }

          // wait until index is unused
          WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !isMaintenanceTime(Misc_getCurrentDateTime())
              || quitFlag
             )
          {
            break;
          }

          if (storageId != DATABASE_ID_NONE)
          {
            // remove storage from database
            do
            {
              // purge storage
              error = purgeStorage(&indexHandle,
                                   storageId,
                                   NULL  // progressInfo
                                  );
              if (error == ERROR_INTERRUPTED)
              {
                // wait until index is unused
                WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,isMaintenanceTime(Misc_getCurrentDateTime()));
              }
            }
            while (   (error == ERROR_INTERRUPTED)
                   && isMaintenanceTime(Misc_getCurrentDateTime())
                   && !quitFlag
                  );

            // prune entity
            if (   (entityId != DATABASE_ID_NONE)
                && (entityId != INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
               )
            {
              do
              {
                error = pruneEntity(&indexHandle,
                                    NULL,  // doneFlag
                                    NULL,  // deletedCounter
                                    entityId
                                   );
                if (error == ERROR_INTERRUPTED)
                {
                  // wait until index is unused
                  WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,isMaintenanceTime(Misc_getCurrentDateTime()));
                }
              }
              while (   (error == ERROR_INTERRUPTED)
                     && isMaintenanceTime(Misc_getCurrentDateTime())
                     && !quitFlag
                    );
            }
          }
        }
        while (   (storageId != DATABASE_ID_NONE)
               && isMaintenanceTime(Misc_getCurrentDateTime())
               && !quitFlag
              );
      }
    #endif /* INDEX_SUPPORT_DELETE */

    // sleep and check quit flag/trigger (min. 10s)
    Misc_udelay(10*US_PER_SECOND);
    sleepTime = 10;
    SEMAPHORE_LOCKED_DO(&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !quitFlag
             && (sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD)
             && !Semaphore_waitModified(&indexThreadTrigger,10*MS_PER_SECOND)
        )
      {
        sleepTime += 10;
      }
    }
  }
  String_delete(storageName);

  // clear index handle for thread interruption
  indexThreadIndexHandle = NULL;

  // free resources
  closeIndex(&indexHandle);
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : assignStorageEntriesToStorage
* Purpose: assign all entries of storage to other storage and remove
*          storage index
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toStorageId - to-storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageEntriesToStorage(IndexHandle *indexHandle,
                                           DatabaseId  storageId,
                                           DatabaseId  toStorageId
                                          )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          toUUIDId,toEntityId;

  /* steps:
     - get to-UUID id, to-entity id
     - assign entries of storage
     - update storage aggregates
     - prune storage
  */

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toStorageId != DATABASE_ID_NONE);

  // init vairables

  error = ERROR_NONE;

  // get to-uuid id, to-entity id
  if (error == ERROR_NONE)
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT uuids.id, \
                                     entities.id \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                              WHERE storages.id=%lld \
                             ",
                             toStorageId
                            );
    if (error == ERROR_NONE)
    {
      if (!Database_getNextRow(&databaseStatementHandle,
                               "%llu %llu",
                               &toUUIDId,
                               &toEntityId
                              )
         )
      {
        error = ERRORX_(DATABASE,0,"assign storage entries");
      }
      Database_finalize(&databaseStatementHandle);
    }
  }

  // set uuid, entity id of all entries of storage
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE id IN (      SELECT entryId FROM entryFragments   WHERE storageId=%lld \
                                           UNION SELECT entryId FROM directoryEntries WHERE storageId=%lld \
                                           UNION SELECT entryId FROM linkEntries      WHERE storageId=%lld \
                                           UNION SELECT entryId FROM specialEntries   WHERE storageId=%lld \
                                          ) \
                             ",
                             toUUIDId,
                             toEntityId,
                             storageId,
                             storageId,
                             storageId,
                             storageId
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entriesNewest \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE id IN (      SELECT entryId FROM entryFragments   WHERE storageId=%lld \
                                           UNION SELECT entryId FROM directoryEntries WHERE storageId=%lld \
                                           UNION SELECT entryId FROM linkEntries      WHERE storageId=%lld \
                                           UNION SELECT entryId FROM specialEntries   WHERE storageId=%lld \
                                          ) \
                             ",
                             toUUIDId,
                             toEntityId,
                             storageId,
                             storageId,
                             storageId,
                             storageId
                            );
  }

  // set storage id of all entry fragments/entries of storage
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entryFragments \
                              SET storageId=%lld \
                              WHERE storageId=%lld \
                             ",
                             toStorageId,
                             storageId
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE directoryEntries \
                              SET storageId=%lld \
                              WHERE storageId=%lld \
                             ",
                             toStorageId,
                             storageId
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE linkEntries \
                              SET storageId=%lld \
                              WHERE storageId=%lld \
                             ",
                             toStorageId,
                             storageId
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE specialEntries \
                              SET storageId=%lld \
                              WHERE storageId=%lld \
                             ",
                             toStorageId,
                             storageId
                            );
  }

  // update aggregates
  if (error == ERROR_NONE)
  {
    error = updateStorageAggregates(indexHandle,toStorageId);
  }

  // delete storage index
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM storages \
                              WHERE id=%lld \
                             ",
                             storageId
                            );
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : assignEntityStoragesToEntity
* Purpose: assign storages of entity to other entity
* Input  : indexHandle - index handle
*          entityId    - entity database id
*          toUUIDId    - to-UUID database id
*          toEntityId  - to-entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityStoragesToEntity(IndexHandle *indexHandle,
                                          DatabaseId  entityId,
                                          DatabaseId  toUUIDId,
                                          DatabaseId  toEntityId
                                         )
{
  Errors error;

  error = ERROR_NONE;

  // set uuid/entity id of all storages of entity
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE storages \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE entityId=%lld \
                             ",
                             toUUIDId,
                             toEntityId,
                             entityId
                            );
  }

  return error;
}

/***********************************************************************\
* Name   : assignEntityEntriesToEntity
* Purpose: assign entries of entity to other entity
* Input  : indexHandle - index handle
*          entityId    - entity database id
*          toUUIDId    - to-UUID database id
*          toEntityId  - to-entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityEntriesToEntity(IndexHandle *indexHandle,
                                         DatabaseId  entityId,
                                         DatabaseId  toUUIDId,
                                         DatabaseId  toEntityId
                                        )
{
  Errors error;

  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE entityId=%lld \
                             ",
                             toUUIDId,
                             toEntityId,
                             entityId
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entriesNewest \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE entryId IN (SELECT entries.id FROM entries LEFT JOIN entriesNewest ON entriesNewest.entryId=entries.id WHERE entries.entityId=%lld) \
                             ",
                             toUUIDId,
                             toEntityId,
                             entityId
                            );
  }

  return error;
}

/***********************************************************************\
* Name   : assignStorageToEntity
* Purpose: assign all entries of storage and storage to other entity
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toEntityId  - to entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : all entries are assigned to new entity where some fragment
*          is inside storage!
\***********************************************************************/

LOCAL Errors assignStorageToEntity(IndexHandle *indexHandle,
                                   DatabaseId  storageId,
                                   DatabaseId  toEntityId
                                  )
{
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entityId,uuidId;
  DatabaseId          toUUIDId;
  Errors              error;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toEntityId != DATABASE_ID_NONE);

  /* steps to do:
     - get entity id, UUID id
     - get to-UUID id
     - assign entries of storage
     - assign storage
     - update entity aggregates
     - prune entity
  */

  // init variables

  error = ERROR_NONE;

  // get entity id, uuid id
  if (error == ERROR_NONE)
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT uuids.id, \
                                     entities.id \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                              WHERE storages.id=%lld \
                             ",
                             storageId
                            );
    if (error == ERROR_NONE)
    {
      if (!Database_getNextRow(&databaseStatementHandle,
                               "%llu %llu",
                               &uuidId,
                               &entityId
                              )
         )
      {
        error = ERRORX_(DATABASE,0,"assign storages");
      }
      Database_finalize(&databaseStatementHandle);
    }
  }

  // get to-uuid id
  if (error == ERROR_NONE)
  {
    error = Database_getId(&indexHandle->databaseHandle,
                           &toUUIDId,
                           "entities",
                           "uuids.id",
                           "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                            WHERE entities.id=%lld \
                           ",
                           toEntityId
                          );
  }

  // assign storage to new entity
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE storages \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE id=%lld \
                             ",
                             toUUIDId,
                             toEntityId,
                             storageId
                            );
  }

  // assign entries of storage to new entity
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET uuidId  =%lld, \
                                  entityId=%lld \
                              WHERE id IN (      SELECT entryId FROM entryFragments   WHERE storageId=%lld \
                                           UNION SELECT entryId FROM directoryEntries WHERE storageId=%lld \
                                           UNION SELECT entryId FROM linkEntries      WHERE storageId=%lld \
                                           UNION SELECT entryId FROM specialEntries   WHERE storageId=%lld \
                                          ) \
                             ",
                             toUUIDId,
                             toEntityId,
                             storageId,
                             storageId,
                             storageId,
                             storageId
                            );
  }

  // update entity aggregates
  if (error == ERROR_NONE)
  {
    error = updateEntityAggregates(indexHandle,toEntityId);
  }

  // prune entity
  if (error == ERROR_NONE)
  {
    error = pruneEntity(indexHandle,
                        NULL,  // doneFlag
                        NULL,  // deletedCounter
                        entityId
                       );
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : assignEntityToEntity
* Purpose: assign entity to other entity
* Input  : indexHandle   - index handle
*          entityId      - entity database id
*          toEntityId    - to entity database id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle  *indexHandle,
                                  DatabaseId   entityId,
                                  DatabaseId   toEntityId,
                                  ArchiveTypes toArchiveType
                                 )
{
  Errors     error;
  DatabaseId toUUIDId;

  assert(indexHandle != NULL);

  /* steps to do:
       - assign all storages of entity
       - assign all entries of entity
       - update entity aggregates
       - prune entity
       - set archive type
  */

  error = ERROR_NONE;

  // assign to entity, update aggregates, prune entity
  if (entityId != toEntityId)
  {
    // get to-uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &toUUIDId,
                             "entities",
                             "uuids.id",
                             "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE entities.id=%lld \
                             ",
                             toEntityId
                            );
    }

    // assign entries of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityEntriesToEntity(indexHandle,
                                          entityId,
                                          toUUIDId,
                                          toEntityId
                                         );
    }

    // assign storages of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityStoragesToEntity(indexHandle,
                                           entityId,
                                           toUUIDId,
                                           toEntityId
                                          );
    }

    // update entities aggregates
    if (error == ERROR_NONE)
    {
      error = updateEntityAggregates(indexHandle,entityId);
    }
    if (error == ERROR_NONE)
    {
      error = updateEntityAggregates(indexHandle,toEntityId);
    }

    // prune entity
    if (error == ERROR_NONE)
    {
      error = pruneEntity(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
    }
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    if (error == ERROR_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET type=%d \
                                WHERE id=%lld \
                               ",
                               toArchiveType,
                               toEntityId
                              );
    }
  }

  return error;
}

/***********************************************************************\
* Name   : assignEntityToJob
* Purpose: assign entity to other job
* Input  : indexHandle   - index handle
*          entityId      - entity index id
*          toJobUUID     - to job UUID
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToJob(IndexHandle  *indexHandle,
                               DatabaseId   entityId,
                               ConstString  toJobUUID,
                               ArchiveTypes toArchiveType
                              )
{
  Errors     error;
  DatabaseId uuidId;
  DatabaseId toUUIDId;

  assert(indexHandle != NULL);
  assert(entityId != DATABASE_ID_NONE);
  assert(toJobUUID != NULL);

  /* steps to do:
       - assign all storages of entity
       - assign entity
       - prune UUID
  */

  error = ERROR_NONE;

  if (toJobUUID != NULL)
  {
    // get uuid id, to-uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
                             "entities",
                             "uuids.id",
                             "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE entities.id=%lld \
                             ",
                             entityId
                            );
    }
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &toUUIDId,
                             "uuids",
                             "id",
                             "WHERE jobUUID=%'S \
                             ",
                             toJobUUID
                            );
    }

    // assign storages of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityStoragesToEntity(indexHandle,
                                           entityId,
                                           toUUIDId,
                                           entityId
                                          );
    }

    // assign entries of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityEntriesToEntity(indexHandle,
                                          entityId,
                                          toUUIDId,
                                          entityId
                                         );
    }

    // assign entity to job
    if (error == ERROR_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET uuidId =%lld, \
                                    jobUUID=%'S \
                                WHERE id=%lld \
                               ",
                               toUUIDId,
                               toJobUUID,
                               entityId
                              );
    }

    // prune UUID
    if (error == ERROR_NONE)
    {
      error = pruneUUID(indexHandle,
                        NULL,  // doneFlag,
                        NULL,  // deletedCounter,
                        uuidId
                       );
    }
  }

  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    // set entity type
    if (error == ERROR_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET type=%d \
                                WHERE id=%lld \
                               ",
                               toArchiveType,
                               entityId
                              );
    }
  }

  return error;
}

/***********************************************************************\
* Name   : assignJobToJob
* Purpose: assign all entities of job to other job
*          entity
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          toJobUUID   - to job UUID
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToJob(IndexHandle  *indexHandle,
                            ConstString  jobUUID,
                            ConstString  toJobUUID,
                            ArchiveTypes toArchiveType
                           )
{
  Errors           error;
  DatabaseId       toUUIDId;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entityId;

  assert(indexHandle != NULL);
  assert(!String_isEmpty(toJobUUID));

  /* steps to do:
     - get to-uuid id
     - assign all entities of job
  */

  error = ERROR_NONE;

  // get to-uuid id
  if (error == ERROR_NONE)
  {
    error = Database_getId(&indexHandle->databaseHandle,
                           &toUUIDId,
                           "uuids",
                           "id",
                           "WHERE jobUUID=%'S \
                           ",
                           toJobUUID
                          );
  }

  // assign all enties of job to other job
  if (error == ERROR_NONE)
  {
    error = Index_initListEntities(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   jobUUID,
                                   NULL,  // scheduleUUID,
                                   ARCHIVE_TYPE_ANY,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name,
                                   INDEX_ENTITY_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    while (   (error == ERROR_NONE)
           && Index_getNextEntity(&indexQueryHandle,
                                  NULL,  // uuidId,
                                  NULL,  // jobUUID,
                                  NULL,  // scheduleUUID,
                                  &entityId,
                                  NULL,  // archiveType,
                                  NULL,  // createdDateTime,
                                  NULL,  // lastErrorMessage
                                  NULL,  // totalSize
                                  NULL,  // totalEntryCount
                                  NULL,  // totalEntrySize
                                  NULL  // lockedCount
                                 )
          )
    {
      error = assignEntityToEntity(indexHandle,
                                   entityId,
                                   toUUIDId,
                                   entityId
                                  );
    }
    Index_doneList(&indexQueryHandle);
  }

  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    if (error == ERROR_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET type=%d \
                                WHERE jobUUID=%'S \
                               ",
                               toArchiveType,
                               jobUUID
                              );
    }
  }

  return error;
}

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  Errors error;

  // init variables
  Semaphore_init(&indexLock,SEMAPHORE_TYPE_BINARY);
  Array_init(&indexUsedBy,sizeof(ThreadInfo),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Semaphore_init(&indexPauseLock,SEMAPHORE_TYPE_BINARY);
  Semaphore_init(&indexThreadTrigger,SEMAPHORE_TYPE_BINARY);
  Semaphore_init(&indexClearStorageLock,SEMAPHORE_TYPE_BINARY);

  // init database
  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    Semaphore_done(&indexClearStorageLock);
    Semaphore_done(&indexThreadTrigger);
    Semaphore_done(&indexPauseLock);
    Array_done(&indexUsedBy);
    Semaphore_done(&indexLock);
    return error;
  }

  #ifdef INDEX_DEBUG_LOCK
    memClear(indexUseCountLPWIds,sizeof(indexUseCountLPWIds));
  #endif /* INDEX_DEBUG_LOCK */

  return ERROR_NONE;
}

void Index_doneAll(void)
{
  Database_doneAll();

  Semaphore_done(&indexClearStorageLock);
  Semaphore_done(&indexThreadTrigger);
  Semaphore_done(&indexPauseLock);
  Array_done(&indexUsedBy);
  Semaphore_done(&indexLock);
}

const char *Index_stateToString(IndexStates indexState, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[i].indexState != indexState)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexState != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[i].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[i].indexMode != indexMode)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[i].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_typeToString(IndexTypes indexType, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_TYPES))
         && (INDEX_TYPES[i].indexType != indexType)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_TYPES))
  {
    name = INDEX_TYPES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseType(const char *name, IndexTypes *indexType)
{
  uint i;

  assert(name != NULL);
  assert(indexType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_TYPES))
         && !stringEqualsIgnoreCase(INDEX_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_TYPES))
  {
    (*indexType) = INDEX_TYPES[i].indexType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseEntitySortMode(const char *name, IndexEntitySortModes *indexEntitySortMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexEntitySortMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_ENTITY_SORT_MODES))
         && !stringEqualsIgnoreCase(INDEX_ENTITY_SORT_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_ENTITY_SORT_MODES))
  {
    (*indexEntitySortMode) = INDEX_ENTITY_SORT_MODES[i].sortMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseStorageSortMode(const char *name, IndexStorageSortModes *indexStorageSortMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexStorageSortMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STORAGE_SORT_MODES))
         && !stringEqualsIgnoreCase(INDEX_STORAGE_SORT_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STORAGE_SORT_MODES))
  {
    (*indexStorageSortMode) = INDEX_STORAGE_SORT_MODES[i].sortMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseEntrySortMode(const char *name, IndexEntrySortModes *indexEntrySortMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexEntrySortMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_ENTRY_SORT_MODES))
         && !stringEqualsIgnoreCase(INDEX_ENTRY_SORT_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_ENTRY_SORT_MODES))
  {
    (*indexEntrySortMode) = INDEX_ENTRY_SORT_MODES[i].sortMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseOrdering(const char *name, DatabaseOrdering *databaseOrdering, void *userData)
{
  assert(name != NULL);
  assert(databaseOrdering != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase("ASCENDING",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_ASCENDING;
    return TRUE;
  }
  else if (stringEqualsIgnoreCase("DESCENDING",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_DESCENDING;
    return TRUE;
  }
  else if (stringEqualsIgnoreCase("NONE",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_NONE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Index_init(const char             *fileName,
                  IndexIsMaintenanceTime isMaintenanceTimeFunction,
                  void                   *isMaintenanceTimeUserData
                 )
{
  bool         createFlag;
  Errors       error;
  int64        indexVersion;
  String       oldDatabaseFileName;
  uint         n;
  IndexHandle  indexHandleReference,indexHandle;
  ProgressInfo progressInfo;

  assert(fileName != NULL);

  // init variables
  indexIsMaintenanceTimeFunction = isMaintenanceTimeFunction;
  indexIsMaintenanceTimeUserData = isMaintenanceTimeUserData;
  quitFlag                       = FALSE;

  // get database file name
  indexDatabaseFileName = stringDuplicate(fileName);
  if (indexDatabaseFileName == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  createFlag = FALSE;

  // check if index exists, check version
  if (!createFlag)
  {
    if (File_existsCString(indexDatabaseFileName))
    {
      // check index version
      error = getIndexVersion(indexDatabaseFileName,&indexVersion);
      if (error == ERROR_NONE)
      {
        if (indexVersion < INDEX_VERSION)
        {
          // rename existing index for upgrade
          oldDatabaseFileName = String_new();
          n = 0;
          do
          {
            oldDatabaseFileName = String_newCString(indexDatabaseFileName);
            String_appendCString(oldDatabaseFileName,".old");
            String_appendFormat(oldDatabaseFileName,"%03d",n);
            n++;
          }
          while (File_exists(oldDatabaseFileName));
          (void)File_renameCString(indexDatabaseFileName,
                                   String_cString(oldDatabaseFileName),
                                   NULL
                                  );
          String_delete(oldDatabaseFileName);

          // upgrade version -> create new
          createFlag = TRUE;
          plogMessage(NULL,  // logHandle
                      LOG_TYPE_ERROR,
                      "INDEX",
                      "Old index database version %d in '%s' - create new",
                      indexVersion,
                      indexDatabaseFileName
                     );
        }
      }
      else
      {
        // unknown version -> create new
        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Unknown index database version in '%s' - create new",
                    indexDatabaseFileName
                   );
      }
    }
    else
    {
      // does not exists -> create new
      createFlag = TRUE;
    }
  }

  if (!createFlag)
  {
    // check if database is outdated or corrupt
    if (File_existsCString(indexDatabaseFileName))
    {
      error = openIndex(&indexHandleReference,NULL,NULL,INDEX_OPEN_MODE_CREATE,NO_WAIT);
      if (error == ERROR_NONE)
      {
        error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
        if (error == ERROR_NONE)
        {
          error = Database_compare(&indexHandleReference.databaseHandle,&indexHandle.databaseHandle);
          closeIndex(&indexHandle);
        }
        closeIndex(&indexHandleReference);
      }
      if (error != ERROR_NONE)
      {
        // rename existing index for upgrade
        oldDatabaseFileName = String_new();
        n = 0;
        do
        {
          String_setCString(oldDatabaseFileName,indexDatabaseFileName);
          String_appendCString(oldDatabaseFileName,".old");
          String_appendFormat(oldDatabaseFileName,"%03d",n);
          n++;
        }
        while (File_exists(oldDatabaseFileName));
        (void)File_renameCString(indexDatabaseFileName,
                                 String_cString(oldDatabaseFileName),
                                 NULL
                                );
        String_delete(oldDatabaseFileName);

        // outdated or corrupt -> create new
        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Outdated or corrupt index database '%s' (error: %s) - create new",
                    indexDatabaseFileName,
                    Error_getText(error)
                   );
      }
    }
  }

  if (createFlag)
  {
    // create new index database
    error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_CREATE,NO_WAIT);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create new index database '%s' fail: %s",
                  indexDatabaseFileName,
                  Error_getText(error)
                 );
      free((char*)indexDatabaseFileName);
      return error;
    }
    closeIndex(&indexHandle);

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Created new index database '%s' (version %d)",
                indexDatabaseFileName,
                INDEX_VERSION
               );
  }
  else
  {
    // get database index version
    error = getIndexVersion(indexDatabaseFileName,&indexVersion);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Cannot get index database version from '%s': %s",
                  indexDatabaseFileName,
                  Error_getText(error)
                 );
      free((char*)indexDatabaseFileName);
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Opened index database '%s' (version %d)",
                indexDatabaseFileName,
                indexVersion
               );
  }

  // initial clean-up
  error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE,NO_WAIT);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot get index database version from '%s': %s",
                indexDatabaseFileName,
                Error_getText(error)
               );
    free((char*)indexDatabaseFileName);
    return error;
  }
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Started initial clean-up index database"
             );

  #ifdef INDEX_INTIIAL_CLEANUP
    initProgress(&progressInfo,"Clean");
    (void)cleanUpDuplicateMeta(&indexHandle);
    (void)cleanUpIncompleteUpdate(&indexHandle);
    (void)cleanUpIncompleteCreate(&indexHandle);
    (void)cleanUpStorageNoName(&indexHandle);
    (void)cleanUpStorageNoEntity(&indexHandle);
    (void)cleanUpStorageInvalidState(&indexHandle);
    (void)cleanUpNoUUID(&indexHandle);
    (void)pruneStorages(&indexHandle,&progressInfo);
    (void)pruneEntities(&indexHandle,NULL,NULL);
    (void)pruneUUIDs(&indexHandle,NULL,NULL);
    doneProgress(&progressInfo);
  #endif /* INDEX_INTIIAL_CLEANUP */
  closeIndex(&indexHandle);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Done initial clean-up index database"
             );

  #ifdef INDEX_INTIIAL_CLEANUP
    // start clean-up thread
    if (!Thread_init(&indexThread,"Index",0,indexThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize index thread!");
    }
  #endif /* INDEX_INTIIAL_CLEANUP */

  return ERROR_NONE;
}

void Index_done(void)
{
  #ifdef INDEX_INTIIAL_CLEANUP
    // stop threads
    quitFlag = TRUE;
    if (!Thread_join(&indexThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop index thread!");
    }
  #endif /* INDEX_INTIIAL_CLEANUP */

  // free resources
  #ifdef INDEX_INTIIAL_CLEANUP
    Thread_done(&indexThread);
  #endif /* INDEX_INTIIAL_CLEANUP */
  free((char*)indexDatabaseFileName);
}

bool Index_isAvailable(void)
{
  return indexDatabaseFileName != NULL;
}

bool Index_isInitialized(void)
{
  return indexInitializedFlag;
}

void Index_setPauseCallback(IndexPauseCallbackFunction pauseCallbackFunction,
                            void                       *pauseCallbackUserData
                           )
{
  SEMAPHORE_LOCKED_DO(&indexPauseLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    indexPauseCallbackFunction = pauseCallbackFunction;
    indexPauseCallbackUserData = pauseCallbackUserData;
  }
}

void Index_beginInUse(void)
{
  addIndexInUseThreadInfo();
}

void Index_endInUse(void)
{
  removeIndexInUseThreadInfo();
}

bool Index_isIndexInUse(void)
{
  return isIndexInUse();
}

#ifdef NDEBUG
IndexHandle *Index_open(ServerIO *masterIO,
                        long     timeout
                       )
#else /* not NDEBUG */
IndexHandle *__Index_open(const char *__fileName__,
                          ulong      __lineNb__,
                          ServerIO   *masterIO,
                          long       timeout
                         )
#endif /* NDEBUG */
{
  IndexHandle *indexHandle;
  Errors      error;

  indexHandle = NULL;

  if (Index_isAvailable())
  {
    indexHandle = (IndexHandle*)malloc(sizeof(IndexHandle));
    if (indexHandle == NULL)
    {
      return NULL;
    }

    #ifdef NDEBUG
      error = openIndex(indexHandle,
                        indexDatabaseFileName,
                        masterIO,
                        INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,
                        timeout
                       );
    #else /* not NDEBUG */
      error = __openIndex(__fileName__,
                          __lineNb__,
                          indexHandle,
                          indexDatabaseFileName,
                          masterIO,
                          INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,
                          timeout
                         );
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      free(indexHandle);
      return NULL;
    }

    #ifdef NDEBUG
      DEBUG_ADD_RESOURCE_TRACE(indexHandle,IndexHandle);
    #else /* not NDEBUG */
      DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,indexHandle,IndexHandle);
    #endif /* NDEBUG */
  }

  return indexHandle;
}

void Index_close(IndexHandle *indexHandle)
{
  if (indexHandle != NULL)
  {
    DEBUG_REMOVE_RESOURCE_TRACE(indexHandle,IndexHandle);

    closeIndex(indexHandle);
    free(indexHandle);
  }
}

void Index_setBusyHandler(IndexHandle              *indexHandle,
                          IndexBusyHandlerFunction busyHandlerFunction,
                          void                     *busyHandlerUserData
                         )
{
  assert(indexHandle != NULL);

  indexHandle->busyHandlerFunction = busyHandlerFunction;
  indexHandle->busyHandlerUserData = busyHandlerUserData;
}

bool Index_isLockPending(IndexHandle *indexHandle, SemaphoreLockTypes lockType)
{
  assert(indexHandle != NULL);

  return Database_isLockPending(&indexHandle->databaseHandle,lockType);
}

void Index_interrupt(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  Database_interrupt(&indexHandle->databaseHandle);
}

#ifdef NDEBUG
Errors Index_beginTransaction(IndexHandle *indexHandle, ulong timeout)
#else /* not NDEBUG */
Errors __Index_beginTransaction(const char  *__fileName__,
                                ulong       __lineNb__,
                                IndexHandle *indexHandle,
                                ulong       timeout
                               )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);

  // begin transaction
  #ifdef NDEBUG
    error = Database_beginTransaction(&indexHandle->databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,timeout);
  #else /* not NDEBUG */
    error = __Database_beginTransaction(__fileName__,__lineNb__,&indexHandle->databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,timeout);
  #endif /* NDEBUG */

  return error;
}

Errors Index_endTransaction(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_endTransaction(&indexHandle->databaseHandle);
}

Errors Index_rollbackTransaction(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_rollbackTransaction(&indexHandle->databaseHandle);
}

Errors Index_flush(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_flush(&indexHandle->databaseHandle);
}

bool Index_containsType(const IndexId indexIds[],
                        ulong         indexIdCount,
                        IndexTypes    indexType
                       )
{
  ulong i;

  assert(indexIds != NULL);

  for (i = 0; i < indexIdCount; i++)
  {
    if (Index_getType(indexIds[i]) == indexType)
    {
      return TRUE;
    }
  }

  return FALSE;
}

bool Index_findUUID(IndexHandle  *indexHandle,
                    ConstString  findJobUUID,
                    ConstString  findScheduleUUID,
                    IndexId      *uuidId,
                    ulong        *executionCountNormal,
                    ulong        *executionCountFull,
                    ulong        *executionCountIncremental,
                    ulong        *executionCountDifferential,
                    ulong        *executionCountContinuous,
                    uint64       *averageDurationNormal,
                    uint64       *averageDurationFull,
                    uint64       *averageDurationIncremental,
                    uint64       *averageDurationDifferential,
                    uint64       *averageDurationContinuous,
                    ulong        *totalEntityCount,
                    ulong        *totalStorageCount,
                    uint64       *totalStorageSize,
                    ulong        *totalEntryCount,
                    uint64       *totalEntrySize
                   )
{
  String              filterString;
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidDatabaseId;
  double              totalStorageSize_,totalEntryCount_,totalEntrySize_;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (indexHandle->masterIO == NULL)
  {
    // filters
    filterString = String_newCString("1");
    filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","uuids.jobUUID=%'S",findJobUUID);
    filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","entities.scheduleUUID=%'S",findScheduleUUID);

    INDEX_DOX(error,
              indexHandle,
    {
//TODO: explain
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT uuids.id, \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%u), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%u), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%u), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%u), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%u), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%u), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%u), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%u), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%u), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%u), \
                                       COUNT(entities.id), \
                                       COUNT(storages.id), \
                                       TOTAL(storages.size), \
                                       TOTAL(storages.totalEntryCount) , \
                                       TOTAL(storages.totalEntrySize) \
                                FROM uuids \
                                  LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                                  LEFT JOIN storages ON storages.entityId=entities.id AND (storages.deletedFlag!=1) \
                                WHERE %S \
                                GROUP BY uuids.id \
                               ",
                               ARCHIVE_TYPE_NORMAL,
                               ARCHIVE_TYPE_FULL,
                               ARCHIVE_TYPE_INCREMENTAL,
                               ARCHIVE_TYPE_DIFFERENTIAL,
                               ARCHIVE_TYPE_CONTINUOUS,
                               ARCHIVE_TYPE_NORMAL,
                               ARCHIVE_TYPE_FULL,
                               ARCHIVE_TYPE_INCREMENTAL,
                               ARCHIVE_TYPE_DIFFERENTIAL,
                               ARCHIVE_TYPE_CONTINUOUS,
                               filterString
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      #ifdef INDEX_DEBUG_LIST_INFO
        Database_debugPrintQueryInfo(&databaseStatementHandle);
      #endif

      if (!Database_getNextRow(&databaseStatementHandle,
                               "%lld %lu %lu %lu %lu %lu %llu %llu %llu %llu %llu %lu %lu %lf %lf %lf",
                               &uuidDatabaseId,
                               executionCountNormal,
                               executionCountFull,
                               executionCountIncremental,
                               executionCountDifferential,
                               executionCountContinuous,
                               averageDurationNormal,
                               averageDurationFull,
                               averageDurationIncremental,
                               averageDurationDifferential,
                               averageDurationContinuous,
                               totalEntityCount,
                               totalStorageCount,
                               &totalStorageSize_,
                               &totalEntryCount_,
                               &totalEntrySize_
                              )
         )
      {
        Database_finalize(&databaseStatementHandle);
        return ERROR_DATABASE_INDEX_NOT_FOUND;
      }
      assert(totalStorageSize_ >= 0.0);
      assert(totalEntryCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalStorageSize != NULL) (*totalStorageSize) = (totalStorageSize_ >= 0.0) ? (uint64)totalStorageSize_ : 0LL;
      if (totalEntryCount  != NULL) (*totalEntryCount ) = (totalEntryCount_  >= 0.0) ? (ulong)totalEntryCount_   : 0L;
      if (totalEntrySize   != NULL) (*totalEntrySize  ) = (totalEntrySize_   >= 0.0) ? (uint64)totalEntrySize_   : 0LL;

      Database_finalize(&databaseStatementHandle);

      return ERROR_NONE;
    });

    if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidDatabaseId);

    String_delete(filterString);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      StringMap_getInt64 (resultMap,"uuidId",uuidId,INDEX_ID_NONE);
                                      if (!INDEX_ID_IS_NONE(*uuidId))
                                      {
                                        if (executionCountNormal        != NULL) StringMap_getULong (resultMap,"executionCountNormal",       executionCountNormal,       0L  );
                                        if (executionCountFull          != NULL) StringMap_getULong (resultMap,"executionCountFull",         executionCountFull,         0L  );
                                        if (executionCountIncremental   != NULL) StringMap_getULong (resultMap,"executionCountIncremental",  executionCountIncremental,  0L  );
                                        if (executionCountDifferential  != NULL) StringMap_getULong (resultMap,"executionCountDifferential", executionCountDifferential, 0L  );
                                        if (executionCountContinuous    != NULL) StringMap_getULong (resultMap,"executionCountContinuous",   executionCountContinuous,   0L  );
                                        if (averageDurationNormal       != NULL) StringMap_getUInt64(resultMap,"averageDurationNormal",      averageDurationNormal,      0LL );
                                        if (averageDurationFull         != NULL) StringMap_getUInt64(resultMap,"averageDurationFull",        averageDurationFull,        0LL );
                                        if (averageDurationIncremental  != NULL) StringMap_getUInt64(resultMap,"averageDurationIncremental", averageDurationIncremental, 0LL );
                                        if (averageDurationDifferential != NULL) StringMap_getUInt64(resultMap,"averageDurationDifferential",averageDurationDifferential,0LL );
                                        if (averageDurationContinuous   != NULL) StringMap_getUInt64(resultMap,"averageDurationContinuous",  averageDurationContinuous,  0LL );
                                        if (totalEntityCount            != NULL) StringMap_getULong (resultMap,"totalEntityCount",           totalEntityCount,           0L  );
                                        if (totalStorageCount           != NULL) StringMap_getULong (resultMap,"totalStorageCount",          totalStorageCount,          0L  );
                                        if (totalStorageSize            != NULL) StringMap_getUInt64(resultMap,"totalStorageSize",           totalStorageSize,           0LL );
                                        if (totalEntryCount             != NULL) StringMap_getULong (resultMap,"totalEntryCount",            totalEntryCount,            0L  );
                                        if (totalEntrySize              != NULL) StringMap_getUInt64(resultMap,"totalEntrySize",             totalEntrySize,             0LL );

                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_DATABASE_INDEX_NOT_FOUND;
                                      }
                                    },NULL),
                                    "INDEX_FIND_UUID jobUUID=%'S scheduleUUID=%'s",
                                    findJobUUID,
                                    (findScheduleUUID != NULL) ? String_cString(findScheduleUUID) : ""
                                   );
  }

  return (error == ERROR_NONE);
}

bool Index_findEntity(IndexHandle  *indexHandle,
                      IndexId      findEntityId,
                      ConstString  findJobUUID,
                      ConstString  findScheduleUUID,
                      ConstString  findHostName,
                      ArchiveTypes findArchiveType,
                      uint64       findCreatedDateTime,
                      String       jobUUID,
                      String       scheduleUUID,
                      IndexId      *uuidId,
                      IndexId      *entityId,
                      ArchiveTypes *archiveType,
                      uint64       *createdDateTime,
                      String       lastErrorMessage,
                      ulong        *totalEntryCount,
                      uint64       *totalEntrySize
                     )
{
  String              filterString;
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  bool                result;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // get filters
  filterString = String_newCString("1");
  filterAppend(filterString,!INDEX_ID_IS_NONE(findEntityId),"AND","entities.id=%lld",Index_getDatabaseId(findEntityId));
  filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","entities.jobUUID=%'S",findJobUUID);
  filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","entities.scheduleUUID=%'S",findScheduleUUID);
  filterAppend(filterString,!String_isEmpty(findHostName),"AND","entities.hostName=%'S",findHostName);
  filterAppend(filterString,findArchiveType != ARCHIVE_TYPE_NONE,"AND","entities.type=%u",findArchiveType);
  filterAppend(filterString,findCreatedDateTime != 0LL,"AND","entities.created=%llu",findCreatedDateTime);

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     entities.scheduleUUID, \
                                     UNIXTIMESTAMP(entities.created), \
                                     entities.type, \
                                     (SELECT storages.errorMessage FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 0,1), \
                                     TOTAL(storages.totalEntryCount), \
                                     TOTAL(storages.totalEntrySize) \
                              FROM entities \
                                LEFT JOIN storages ON storages.entityId=entities.id AND (storages.deletedFlag!=1) \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                              WHERE     entities.deletedFlag!=1 \
                                    AND %S \
                              GROUP BY entities.id \
                              LIMIT 0,1 \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S %lld %S %lld %d %S %lu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 scheduleUUID,
                                 createdDateTime,
                                 archiveType,
                                 lastErrorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(filterString);
    return FALSE;
  }

  if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_UUID(uuidDatabaseId    );
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityDatabaseId);

  // free resources
  String_delete(filterString);

  return result;
}

bool Index_findStorageById(IndexHandle *indexHandle,
                           IndexId     findStorageId,
                           String      jobUUID,
                           String      scheduleUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           uint64      *dateTime,
                           uint64      *size,
                           IndexStates *indexState,
                           IndexModes  *indexMode,
                           uint64      *lastCheckedDateTime,
                           String      errorMessage,
                           ulong       *totalEntryCount,
                           uint64      *totalEntrySize
                          )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  bool                result;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);
  assert(Index_getType(findStorageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIXTIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIXTIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON entities.jobUUID=uuids.jobUUID \
                              WHERE     storages.deletedFlag!=1 \
                                    AND storages.id=%lld \
                              GROUP BY storages.id \
                              LIMIT 0,1 \
                             ",
                             Index_getDatabaseId(findStorageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseStatementHandle);

    result = Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 dateTime,
                                 size,
                                 indexState,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_UUID  (uuidDatabaseId  );
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityDatabaseId);

  return result;
}

bool Index_findStorageByName(IndexHandle            *indexHandle,
                             const StorageSpecifier *findStorageSpecifier,
                             ConstString            findArchiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 scheduleUUID,
                             IndexId                *storageId,
                             uint64                 *dateTime,
                             uint64                 *size,
                             IndexStates            *indexState,
                             IndexModes             *indexMode,
                             uint64                 *lastCheckedDateTime,
                             String                 errorMessage,
                             ulong                  *totalEntryCount,
                             uint64                 *totalEntrySize
                            )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;
  DatabaseId          uuidDatabaseId,entityDatabaseId,storageDatabaseId;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = INDEX_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
//TODO: optimize: search for part of name?
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storages.id, \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIXTIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIXTIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE storages.deletedFlag!=1 \
                              GROUP BY storages.id \
                             "
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseStatementHandle);

    storageName = String_new();
    Storage_initSpecifier(&storageSpecifier);
    foundFlag   = FALSE;
    while (   !foundFlag
           && Database_getNextRow(&databaseStatementHandle,
                                  "%lld %S %lld %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                  &uuidDatabaseId,
                                  jobUUID,
                                  &entityDatabaseId,
                                  &storageDatabaseId,
                                  scheduleUUID,
                                  storageName,
                                  dateTime,
                                  size,
                                  indexState,
                                  indexMode,
                                  lastCheckedDateTime,
                                  errorMessage,
                                  totalEntryCount,
                                  totalEntrySize
                                 )
          )
    {
      if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
      {
        foundFlag = Storage_equalSpecifiers(findStorageSpecifier,findArchiveName,
                                            &storageSpecifier,NULL
                                           );
        if (foundFlag)
        {
          if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );
          if (entityId  != NULL) (*entityId ) = INDEX_ID_ENTITY (entityDatabaseId );
          if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageDatabaseId);
        }
      }
    }
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  return foundFlag;
}

bool Index_findStorageByState(IndexHandle   *indexHandle,
                              IndexStateSet findIndexStateSet,
                              IndexId       *uuidId,
                              String        jobUUID,
                              IndexId       *entityId,
                              String        scheduleUUID,
                              IndexId       *storageId,
                              String        storageName,
                              uint64        *dateTime,
                              uint64        *size,
                              IndexModes    *indexMode,
                              uint64        *lastCheckedDateTime,
                              String        errorMessage,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
  Errors              error;
  String              indexStateSetString;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidDatabaseId,entityDatabaseId,storageDatabaseId;
  bool                result;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  indexStateSetString = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storages.id, \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIXTIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.mode, \
                                     UNIXTIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE     storages.deletedFlag!=1 \
                                    AND (storages.state IN (%S)) \
                              LIMIT 0,1 \
                             ",
                             getIndexStateSetString(indexStateSetString,findIndexStateSet)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S %lld %lld %S %S %llu %llu %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 &storageDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 dateTime,
                                 size,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }

  if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );
  if (entityId  != NULL) (*entityId ) = INDEX_ID_ENTITY (entityDatabaseId );
  if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageDatabaseId);

  // free resources
  String_delete(indexStateSetString);

  return result;
}

Errors Index_getStorageState(IndexHandle *indexHandle,
                             IndexId     storageId,
                             IndexStates *indexState,
                             uint64      *lastCheckedDateTime,
                             String      errorMessage
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    return getStorageState(indexHandle,
                           Index_getDatabaseId(storageId),
                           indexState,
                           lastCheckedDateTime,
                           errorMessage
                          );
  });

  return error;
}

Errors Index_setStorageState(IndexHandle *indexHandle,
                             IndexId     indexId,
                             IndexStates indexState,
                             uint64      lastCheckedDateTime,
                             const char  *errorMessage,
                             ...
                            )
{
  Errors  error;
  va_list arguments;
  String  errorText;

  assert(indexHandle != NULL);
  assert((Index_getType(indexId) == INDEX_TYPE_ENTITY) || (Index_getType(indexId) == INDEX_TYPE_STORAGE));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // format error message (if any)
  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    errorText = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);
  }
  else
  {
    errorText = NULL;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      switch (Index_getType(indexId))
      {
        case INDEX_TYPE_ENTITY:
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "UPDATE storages \
                                    SET state       =%d, \
                                        errorMessage=NULL \
                                    WHERE entityId=%lld \
                                   ",
                                   indexState,
                                   Index_getDatabaseId(indexId)
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET lastChecked=DATETIME(%llu,'unixepoch') \
                                      WHERE entityId=%lld \
                                     ",
                                     lastCheckedDateTime,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET errorMessage=%'S \
                                      WHERE entityId=%lld \
                                     ",
                                     errorText,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        case INDEX_TYPE_STORAGE:
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "UPDATE storages \
                                    SET state       =%d, \
                                        errorMessage=NULL \
                                    WHERE id=%lld \
                                   ",
                                   indexState,
                                   Index_getDatabaseId(indexId)
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET lastChecked=DATETIME(%llu,'unixepoch') \
                                      WHERE id=%lld \
                                     ",
                                     lastCheckedDateTime,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storages \
                                      SET errorMessage=%'S \
                                      WHERE id=%lld \
                                     ",
                                     errorText,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* not NDEBUG */
          break;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_SET_STATE indexId=%lld indexState=%'s lastCheckedDateTime=%llu errorMessage=%'S",
                                    indexId,
                                    Index_stateToString(indexState,NULL),
                                    lastCheckedDateTime,
                                    errorText
                                   );
  }
  if (error != ERROR_NONE)
  {
    if (errorMessage != NULL) String_delete(errorText);
    return error;
  }

  // free resources
  if (errorMessage != NULL) String_delete(errorText);

  return ERROR_NONE;
}

long Index_countStorageState(IndexHandle *indexHandle,
                             IndexStates indexState
                            )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  long                count;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  INDEX_DOX(count,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT COUNT(id) \
                              FROM storages \
                              WHERE state=%u \
                             ",
                             indexState
                            );
    if (error != ERROR_NONE)
    {
      return -1L;
    }

    if (!Database_getNextRow(&databaseStatementHandle,
                             "%ld",
                             &count
                            )
       )
    {
      Database_finalize(&databaseStatementHandle);
      return -1L;
    }

    Database_finalize(&databaseStatementHandle);

    return count;
  });

  return count;
}

Errors Index_getInfos(IndexHandle   *indexHandle,
                      ulong         *totalEntityCount,
                      ulong         *totalDeletedEntityCount,

                      ulong         *totalEntryCount,
                      uint64        *totalEntrySize,
                      uint64        *totalEntryContentSize,
                      ulong         *totalFileCount,
                      uint64        *totalFileSize,
                      ulong         *totalImageCount,
                      uint64        *totalImageSize,
                      ulong         *totalDirectoryCount,
                      ulong         *totalLinkCount,
                      ulong         *totalHardlinkCount,
                      uint64        *totalHardlinkSize,
                      ulong         *totalSpecialCount,

                      ulong         *totalEntryCountNewest,
                      uint64        *totalEntrySizeNewest,
                      uint64        *totalEntryContentSizeNewest,
                      ulong         *totalFileCountNewest,
                      uint64        *totalFileSizeNewest,
                      ulong         *totalImageCountNewest,
                      uint64        *totalImageSizeNewest,
                      ulong         *totalDirectoryCountNewest,
                      ulong         *totalLinkCountNewest,
                      ulong         *totalHardlinkCountNewest,
                      uint64        *totalHardlinkSizeNewest,
                      ulong         *totalSpecialCountNewest,

                      ulong         *totalSkippedEntryCount,

                      ulong         *totalStorageCount,
                      uint64        *totalStorageSize,
                      ulong         *totalDeletedStorageCount
                     )
{
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  double              totalEntrySize_,totalEntryContentSize_,totalFileSize_,totalImageSize_,totalHardlinkSize_;
  double              totalEntrySizeNewest_,totalEntryContentSizeNewest_,totalFileSizeNewest_,totalImageSizeNewest_,totalHardlinkSizeNewest_;
  double              totalStorageSize_;

  assert(indexHandle != NULL);

  // init variables

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables

  INDEX_DOX(error,
            indexHandle,
  {
    if (totalEntityCount != NULL)
    {
      // get total entities count
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(entities.id) \
                                FROM entities \
                                WHERE deletedFlag!=1 \
                               "
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseStatementHandle);
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lu",
                              totalEntityCount
                             )
            )
      {
      }
      Database_finalize(&databaseStatementHandle);
    }

    if (totalDeletedEntityCount != NULL)
    {
      // get total deleted entities count
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(entities.id) \
                                FROM entities \
                                WHERE deletedFlag=1 \
                               "
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseStatementHandle);
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lu",
                              totalDeletedEntityCount
                             )
            )
      {
      }
      Database_finalize(&databaseStatementHandle);
    }

    if (totalSkippedEntryCount != NULL)
    {
      // get total skipped entry count
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(id) \
                                FROM skippedEntries \
                               "
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseStatementHandle);
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lu",
                              totalSkippedEntryCount
                             )
            )
      {
      }
      Database_finalize(&databaseStatementHandle);
    }

    if (   (totalStorageCount != NULL)
        || (totalStorageSize != NULL)
       )
    {
      // get total * count/size, total newest count/size
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT TOTAL(storages.totalEntryCount), \
                                       TOTAL(storages.totalEntrySize), \
                                       0, \
                                       TOTAL(storages.totalFileCount), \
                                       TOTAL(storages.totalFileSize), \
                                       TOTAL(storages.totalImageCount), \
                                       TOTAL(storages.totalImageSize), \
                                       TOTAL(storages.totalDirectoryCount), \
                                       TOTAL(storages.totalLinkCount), \
                                       TOTAL(storages.totalHardlinkCount), \
                                       TOTAL(storages.totalHardlinkSize), \
                                       TOTAL(storages.totalSpecialCount), \
                                       \
                                       TOTAL(storages.totalEntryCountNewest), \
                                       TOTAL(storages.totalEntrySizeNewest), \
                                       0, \
                                       TOTAL(storages.totalFileCountNewest), \
                                       TOTAL(storages.totalFileSizeNewest), \
                                       TOTAL(storages.totalImageCountNewest), \
                                       TOTAL(storages.totalImageSizeNewest), \
                                       TOTAL(storages.totalDirectoryCountNewest), \
                                       TOTAL(storages.totalLinkCountNewest), \
                                       TOTAL(storages.totalHardlinkCountNewest), \
                                       TOTAL(storages.totalHardlinkSizeNewest), \
                                       TOTAL(storages.totalSpecialCountNewest), \
                                       \
                                       COUNT(id), \
                                       TOTAL(storages.size) \
                                FROM storages \
                                WHERE deletedFlag!=1 \
                               "
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseStatementHandle);
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lu %lf %lf %lu %lf %lu %lf %lu %lu %lu %lf %lu  %lu %lf %lf %lu %lf %lu %lf %lu %lu %lu %lf %lu  %lu %lf",
                              totalEntryCount,
                              &totalEntrySize_,
                              &totalEntryContentSize_,
                              totalFileCount,
                              &totalFileSize_,
                              totalImageCount,
                              &totalImageSize_,
                              totalDirectoryCount,
                              totalLinkCount,
                              totalHardlinkCount,
                              &totalHardlinkSize_,
                              totalSpecialCount,

                              totalEntryCountNewest,
                              &totalEntrySizeNewest_,
                              &totalEntryContentSize_,
                              totalFileCountNewest,
                              &totalFileSizeNewest_,
                              totalImageCountNewest,
                              &totalImageSizeNewest_,
                              totalDirectoryCountNewest,
                              totalLinkCountNewest,
                              totalHardlinkCountNewest,
                              &totalHardlinkSizeNewest_,
                              totalSpecialCountNewest,

                              totalStorageCount,
                              &totalStorageSize_
                             )
            )
      {
        assert(totalEntrySize_          >= 0.0);
        assert(totalEntryContentSize_   >= 0.0);
        assert(totalFileSize_           >= 0.0);
        assert(totalImageSize_          >= 0.0);
        assert(totalHardlinkSize_       >= 0.0);
        assert(totalEntrySizeNewest_    >= 0.0);
        assert(totalEntryContentSize_   >= 0.0);
        assert(totalFileSizeNewest_     >= 0.0);
        assert(totalImageSizeNewest_    >= 0.0);
        assert(totalHardlinkSizeNewest_ >= 0.0);
        assert(totalStorageSize_        >= 0.0);
        if (totalEntrySize              != NULL) (*totalEntrySize             ) = (totalEntrySize_              >= 0.0) ? (uint64)totalEntrySize_              : 0LL;
        if (totalEntryContentSize       != NULL) (*totalEntryContentSize      ) = (totalEntryContentSize_       >= 0.0) ? (uint64)totalEntryContentSize_       : 0LL;
        if (totalFileSize               != NULL) (*totalFileSize              ) = (totalFileSize_               >= 0.0) ? (uint64)totalFileSize_               : 0LL;
        if (totalImageSize              != NULL) (*totalImageSize             ) = (totalImageSize_              >= 0.0) ? (uint64)totalImageSize_              : 0LL;
        if (totalHardlinkSize           != NULL) (*totalHardlinkSize          ) = (totalHardlinkSize_           >= 0.0) ? (uint64)totalHardlinkSize_           : 0LL;
        if (totalEntrySizeNewest        != NULL) (*totalEntrySizeNewest       ) = (totalEntrySizeNewest_        >= 0.0) ? (uint64)totalEntrySizeNewest_        : 0LL;
        if (totalEntryContentSizeNewest != NULL) (*totalEntryContentSizeNewest) = (totalEntryContentSizeNewest_ >= 0.0) ? (uint64)totalEntryContentSizeNewest_ : 0LL;
        if (totalFileSizeNewest         != NULL) (*totalFileSizeNewest        ) = (totalFileSizeNewest_         >= 0.0) ? (uint64)totalFileSizeNewest_         : 0LL;
        if (totalImageSizeNewest        != NULL) (*totalImageSizeNewest       ) = (totalImageSizeNewest_        >= 0.0) ? (uint64)totalImageSizeNewest_        : 0LL;
        if (totalHardlinkSizeNewest     != NULL) (*totalHardlinkSizeNewest    ) = (totalHardlinkSizeNewest_     >= 0.0) ? (uint64)totalHardlinkSizeNewest_     : 0LL;
        if (totalStorageSize            != NULL) (*totalStorageSize           ) = (totalStorageSize_            >= 0.0) ? (uint64)totalStorageSize_            : 0LL;

      }
      Database_finalize(&databaseStatementHandle);
    }

    if (totalDeletedStorageCount != NULL)
    {
      // get total deleted storage count
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(id) \
                                FROM storages \
                                WHERE deletedFlag=1 \
                               "
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseStatementHandle);
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lu",
                              totalDeletedStorageCount
                             )
            )
      {
      }
      Database_finalize(&databaseStatementHandle);
    }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

Errors Index_initListHistory(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             ConstString      jobUUID,
                             DatabaseOrdering ordering,
                             uint64           offset,
                             uint64           limit
                            )
{
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  filterString = String_newCString("1");
  orderString  = String_new();

  // get filters
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","history.jobUUID=%'S",jobUUID);

  // get ordering
  appendOrdering(orderString,TRUE,"history.created",ordering);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT history.id, \
                                    IFNULL(uuids.id,0), \
                                    history.jobUUID, \
                                    history.scheduleUUID, \
                                    history.hostName, \
                                    history.userName, \
                                    history.type, \
                                    UNIXTIMESTAMP(history.created), \
                                    history.errorMessage, \
                                    history.duration, \
                                    history.totalEntryCount, \
                                    history.totalEntrySize, \
                                    history.skippedEntryCount, \
                                    history.skippedEntrySize, \
                                    history.errorEntryCount, \
                                    history.errorEntrySize \
                             FROM history \
                               LEFT JOIN uuids ON uuids.jobUUID=history.jobUUID \
                             WHERE %S \
                             %S \
                             LIMIT %llu,%llu \
                            ",
                            filterString,
                            orderString,
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextHistory(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          IndexId          *uuidId,
                          String           jobUUID,
                          String           scheduleUUID,
                          String           hostName,
                          String           userName,
                          ArchiveTypes     *archiveType,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          uint64           *duration,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          ulong            *skippedEntryCount,
                          uint64           *skippedEntrySize,
                          ulong            *errorEntryCount,
                          uint64           *errorEntrySize
                         )
{
  DatabaseId historyDatabaseId;
  DatabaseId uuidDatabaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return indexQueryHandle->indexHandle->upgradeError;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %lld %S %S %S %S %u %llu %S %llu %lu %llu %lu %llu %lu %llu",
                           &historyDatabaseId,
                           &uuidDatabaseId,
                           jobUUID,
                           scheduleUUID,
                           hostName,
                           userName,
                           archiveType,
                           createdDateTime,
                           errorMessage,
                           duration,
                           totalEntryCount,
                           totalEntrySize,
                           skippedEntryCount,
                           skippedEntrySize,
                           errorEntryCount,
                           errorEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(historyDatabaseId);
  if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );

  return TRUE;
}

Errors Index_newHistory(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  scheduleUUID,
                        ConstString  hostName,
                        ConstString  userName,
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        uint64       duration,
                        ulong        totalEntryCount,
                        uint64       totalEntrySize,
                        ulong        skippedEntryCount,
                        uint64       skippedEntrySize,
                        ulong        errorEntryCount,
                        uint64       errorEntrySize,
                        IndexId      *historyId
                       )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      Errors error;

      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                              "INSERT INTO history \
                                 ( \
                                  jobUUID, \
                                  scheduleUUID, \
                                  hostName, \
                                  userName, \
                                  type, \
                                  created, \
                                  errorMessage, \
                                  duration, \
                                  totalEntryCount, \
                                  totalEntrySize, \
                                  skippedEntryCount, \
                                  skippedEntrySize, \
                                  errorEntryCount, \
                                  errorEntrySize \
                                 ) \
                               VALUES \
                                 ( \
                                  %'S, \
                                  %'S, \
                                  %'S, \
                                  %'S, \
                                  %u, \
                                  %llu, \
                                  %'s, \
                                  %llu, \
                                  %lu, \
                                  %llu, \
                                  %lu, \
                                  %llu, \
                                  %lu, \
                                  %llu \
                                 ) \
                              ",
                              jobUUID,
                              scheduleUUID,
                              hostName,
                              userName,
                              archiveType,
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
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      Errors error;

                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      error = ERROR_NONE;

                                      if (historyId != NULL)
                                      {
                                        if (!StringMap_getInt64(resultMap,"historyId",historyId,INDEX_ID_NONE))
                                        {
                                          error = ERROR_EXPECTED_PARAMETER;
                                        }
                                      }

                                      return error;
                                    },NULL),
                                    "INDEX_NEW_HISTORY jobUUID=%S scheduleUUID=%s hostName=%'S userName=%'S archiveType=%s createdDateTime=%llu errorMessage=%'s duration=%llu totalEntryCount=%lu totalEntrySize=%llu skippedEntryCount=%lu skippedEntrySize=%llu errorEntryCount=%lu errorEntrySize=%llu",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    userName,
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
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteHistory(IndexHandle *indexHandle,
                           IndexId     historyId
                          )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete history entry
  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM history WHERE id=%lld",
                             Index_getDatabaseId(historyId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_getUUIDsInfos(IndexHandle   *indexHandle,
                           IndexId       uuidId,
//TODO: remove?
                           ConstString   jobUUID,
                           ConstString   scheduleUUID,
                           ConstString   name,
                           uint64        *lastExecutedDateTime,
                           ulong         *totalEntityCount,
                           ulong         *totalEntryCount,
                           uint64        *totalEntrySize
                          )
{
  String              ftsName;
  String              filterString;
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  double              totalEntryCount_,totalEntrySize_;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // init variables

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");

  // get FTS
  getFTSString(ftsName,name);

  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","uuids.jobUUID='%S'",jobUUID);
  filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","uuids.id IN (SELECT uuidId FROM FTS_uuids WHERE FTS_uuids MATCH '%S')",ftsName);

  INDEX_DOX(error,
            indexHandle,
  {
    // get last executed, total entities count, total entry count, total entry size
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT MAX(UNIXTIMESTAMP(entities.created)), \
                                     COUNT(entities.id), \
                                     TOTAL(storages.totalEntryCount), \
                                     TOTAL(storages.totalEntrySize) \
                              FROM uuids \
                                LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                                LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=1 \
                              WHERE %S \
                             ",
                             filterString
                            );
//Database_debugPrintQueryInfo(&databaseStatementHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseStatementHandle,
                            "%llu %lu %lf %lf",
                            lastExecutedDateTime,
                            totalEntityCount,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL) (*totalEntryCount) = (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
      if (totalEntrySize != NULL) (*totalEntrySize) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
    }
    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(ftsName);

  return ERROR_NONE;
}

Errors Index_updateUUIDInfos(IndexHandle *indexHandle,
                             IndexId     uuidId
                            )
{
// not required? calculate on-the-fly
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
#if 0
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalFileCount;
  double              totalFileSize_;
  uint64              totalFileSize;
  ulong               totalImageCount;
  double              totalImageSize_;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  double              totalHardlinkSize_;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // get file aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,

//TODO: use entries.size?
                               "SELECT COUNT(DISTINCT entries.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN uuids ON uuids.jobUUID=entries.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get image aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
//TODO: use entries.size?
                               "SELECT COUNT(DISTINCT entries.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get directory aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseStatementHandle);

      // get link aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseStatementHandle);

      // get hardlink aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
//TODO: use entries.size?
                               "SELECT COUNT(DISTINCT entries.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get special aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseStatementHandle);

      // update aggregate data
//fprintf(stderr,"%s, %d: aggregate %llu %llu\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE uuids \
                                SET totalEntryCount    =%llu, \
                                    totalEntrySize     =%llu, \
                                    totalFileCount     =%llu, \
                                    totalFileSize      =%llu, \
                                    totalImageCount    =%llu, \
                                    totalImageSize     =%llu, \
                                    totalDirectoryCount=%llu, \
                                    totalLinkCount     =%llu, \
                                    totalHardlinkCount =%llu, \
                                    totalHardlinkSize  =%llu, \
                                    totalSpecialCount  =%llu \
                                WHERE id=%lld \
                               ",
                               totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                               totalFileSize+totalImageSize+totalHardlinkSize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // -----------------------------------------------------------------

      // get newest file aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get newest image aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND entriesNewest.entityId=%lld \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get newest directory aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseStatementHandle);

      // get newest link aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
                               "SELECT COUNT(DISTINCT entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseStatementHandle);

      // get newest hardlink aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT),
//TODO: use entriesNewest.size?
                               "SELECT COUNT(DISTINCT entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseStatementHandle);

      // get newest special aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,

                               "SELECT COUNT(DISTINCT entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%u \
                                      AND uuids.id=%lld \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseStatementHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseStatementHandle);

      // update newest aggregate data
//fprintf(stderr,"%s, %d: newest aggregate %llu %llu\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE uuids \
                                SET totalEntryCountNewest    =%llu, \
                                    totalEntrySizeNewest     =%llu, \
                                    totalFileCountNewest     =%llu, \
                                    totalFileSizeNewest      =%llu, \
                                    totalImageCountNewest    =%llu, \
                                    totalImageSizeNewest     =%llu, \
                                    totalDirectoryCountNewest=%llu, \
                                    totalLinkCountNewest     =%llu, \
                                    totalHardlinkCountNewest =%llu, \
                                    totalHardlinkSizeNewest  =%llu, \
                                    totalSpecialCountNewest  =%llu \
                                WHERE id=%lld \
                               ",
                               totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                               totalFileSize+totalImageSize+totalHardlinkSize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_UPDATE_INFOS uuidId=%lld",
                                    uuidId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  return ERROR_NONE;
}

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           IndexStateSet    indexStateSet,
                           IndexModeSet     indexModeSet,
                           ConstString      name,
                           uint64           offset,
                           uint64           limit
                          )
{
  String ftsName;
  String filterString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");

  // get FTS
  getFTSString(ftsName,name);

  // get filters
  string = String_new();
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT uuids.id, \
                                    uuids.jobUUID, \
                                    (SELECT MAX(UNIXTIMESTAMP(entities.created)) FROM entities WHERE entities.jobUUID=uuids.jobUUID), \
                                    (SELECT storages.errorMessage FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 0,1), \
                                    TOTAL(storages.size), \
                                    TOTAL(storages.totalEntryCount), \
                                    TOTAL(storages.totalEntrySize) \
                             FROM uuids \
                               LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                               LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=1 \
                             WHERE     %S \
                                   AND uuids.jobUUID!='' \
                             GROUP BY uuids.id \
                             LIMIT %llu,%llu \
                            ",
                            filterString,
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);

  // free resources
  String_delete(filterString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           jobUUID,
                       uint64           *lastExecutedDateTime,
                       String           lastErrorMessage,
                       uint64           *size,
                       ulong            *totalEntryCount,
                       uint64           *totalEntrySize
                      )
{
  DatabaseId databaseId;
  double     size_;
  double     totalEntryCount_;
  double     totalEntrySize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %S %llu %S %lf %lf %lf",
                           &databaseId,
                           jobUUID,
                           lastExecutedDateTime,
                           lastErrorMessage,
                           &size_,
                           &totalEntryCount_,
                           &totalEntrySize_
                          )
     )
  {
    return FALSE;
  }
  if (indexId         != NULL) (*indexId        ) = INDEX_ID_UUID(databaseId);
  if (size            != NULL) (*size           ) = (uint64)size_;
  if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
  if (totalEntrySize  != NULL) (*totalEntrySize ) = (uint64)totalEntrySize_;

  return TRUE;
}

Errors Index_newUUID(IndexHandle *indexHandle,
                     ConstString jobUUID,
                     IndexId     *uuidId
                    )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(uuidId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "INSERT INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ) \
                               ",
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*uuidId) = INDEX_ID_UUID(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      if (StringMap_getInt64(resultMap,"uuidId",uuidId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_UUID jobUUID=%S",
                                    jobUUID
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        IndexId     uuidId
                       )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entityId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    // delete entities
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT entities.id \
                              FROM entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE uuids.id=%lld \
                             ",
                             Index_getDatabaseId(uuidId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    while (   Database_getNextRow(&databaseStatementHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,INDEX_ID_ENTITY(entityId));
    }
    Database_finalize(&databaseStatementHandle);
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    // delete UUID index
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM uuids \
                              WHERE id=%lld \
                             ",
                             Index_getDatabaseId(uuidId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

bool Index_isEmptyUUID(IndexHandle *indexHandle,
                       IndexId     uuidId
                      )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return isEmptyUUID(indexHandle,
                       Index_getDatabaseId(uuidId)
                      );
  });

  return emptyFlag;
}

Errors Index_getEntitiesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
//TODO: remove?
                              ConstString   jobUUID,
                              const IndexId indexIds[],
                              ulong         indexIdCount,
                              ConstString   name,
                              ulong         *totalStorageCount,
                              uint64        *totalStorageSize,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
UNUSED_VARIABLE(entityId);
UNUSED_VARIABLE(jobUUID);
UNUSED_VARIABLE(indexIds);
UNUSED_VARIABLE(indexIdCount);
UNUSED_VARIABLE(name);
UNUSED_VARIABLE(totalStorageCount);
UNUSED_VARIABLE(totalStorageSize);
UNUSED_VARIABLE(totalEntryCount);
UNUSED_VARIABLE(totalEntrySize);

return ERROR_STILL_NOT_IMPLEMENTED;
}

Errors Index_updateEntityInfos(IndexHandle *indexHandle,
                               IndexId     entityId
                              )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return updateEntityAggregates(indexHandle,Index_getDatabaseId(entityId));
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ENTITY_UPDATE_INFOS entityId=%lld",
                                    entityId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListEntities(IndexQueryHandle     *indexQueryHandle,
                              IndexHandle          *indexHandle,
                              IndexId              uuidId,
                              ConstString          jobUUID,
                              ConstString          scheduleUUID,
                              ArchiveTypes         archiveType,
                              IndexStateSet        indexStateSet,
                              IndexModeSet         indexModeSet,
                              ConstString          name,
                              IndexEntitySortModes sortMode,
                              DatabaseOrdering     ordering,
                              ulong                offset,
                              uint64               limit
                             )
{
  String ftsName;
  String filterString;
  String string;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_format(String_new(),"entities.id!=%lld",INDEX_DEFAULT_ENTITY_DATABASE_ID);
  orderString  = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get filters
  string = String_newCString("1");
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);
  filterAppend(filterString,archiveType != ARCHIVE_TYPE_ANY,"AND","entities.type=%u",archiveType);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","EXISTS(SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // get sort mode, ordering
  appendOrdering(orderString,sortMode != INDEX_ENTITY_SORT_MODE_NONE,INDEX_ENTITY_SORT_MODE_COLUMNS[sortMode],ordering);

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntities ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: jobUUID=%s\n",__FILE__,__LINE__,String_cString(jobUUID));
    fprintf(stderr,"%s, %d: archiveType=%u\n",__FILE__,__LINE__,archiveType);
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT IFNULL(uuids.id,0), \
                                    entities.jobUUID, \
                                    entities.id, \
                                    entities.scheduleUUID, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entities.type, \
                                    (SELECT errorMessage FROM storages WHERE storages.entityId=entities.id ORDER BY created DESC LIMIT 0,1), \
                                    TOTAL(storages.size), \
                                    TOTAL(storages.totalEntryCount), \
                                    TOTAL(storages.totalEntrySize), \
                                    entities.lockedCount \
                             FROM entities \
                               LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                               LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=1 \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                             GROUP BY entities.id \
                             %S \
                             LIMIT %llu,%llu \
                            ",
                            filterString,
                            orderString,
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *uuidId,
                         String           jobUUID,
                         String           scheduleUUID,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         String           lastErrorMessage,
                         uint64           *totalSize,
                         ulong            *totalEntryCount,
                         uint64           *totalEntrySize,
                         uint             *lockedCount
                        )
{
  DatabaseId uuidDatabaseId,entityDatatabaseId;
  double     totalSize_;
  double     totalEntryCount_;
  double     totalEntrySize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %S %lld %S %llu %u %S %lf %lf %lf %d",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatatabaseId,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
                           lastErrorMessage,
                           &totalSize_,
                           &totalEntryCount_,
                           &totalEntrySize_,
                           lockedCount
                          )
     )
  {
    return FALSE;
  }
  if (uuidId          != NULL) (*uuidId         ) = INDEX_ID_ENTITY(uuidDatabaseId);
  if (entityId        != NULL) (*entityId       ) = INDEX_ID_ENTITY(entityDatatabaseId);
  if (totalSize       != NULL) (*totalSize      ) = (uint64)totalSize_;
  if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
  if (totalEntrySize  != NULL) (*totalEntrySize ) = (uint64)totalEntrySize_;

  return TRUE;
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ConstString  hostName,
                       ConstString  userName,
                       ArchiveTypes archiveType,
                       uint64       createdDateTime,
                       bool         locked,
                       IndexId      *entityId
                      )
{
  Errors     error;
  DatabaseId uuidId;

  assert(indexHandle != NULL);
  assert(jobUUID != NULL);
  assert(entityId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // create UUID (if it does not exists)
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "INSERT OR IGNORE INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ) \
                               ",
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get uuid id
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
                             "uuids",
                             "id",
                             "WHERE jobUUID=%'S \
                             ",
                             jobUUID
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // create entity
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
                                   uuidId, \
                                   jobUUID, \
                                   scheduleUUID, \
                                   hostName, \
                                   userName, \
                                   created, \
                                   type, \
                                   lockedCount \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S, \
                                   %'S, \
                                   %'S, \
                                   %'S, \
                                   %llu, \
                                   %u, \
                                   %d \
                                  ) \
                               ",
                               uuidId,
                               jobUUID,
                               scheduleUUID,
                               hostName,
                               userName,
                               (createdDateTime != 0LL) ? createdDateTime : Misc_getCurrentDateTime(),
                               archiveType,
                               locked ? 1 : 0
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*entityId) = INDEX_ID_ENTITY(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      if (StringMap_getInt64 (resultMap,"entityId",entityId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_ENTITY jobUUID=%S scheduleUUID=%s hostName=%S userName=%S archiveType=%s createdDateTime=%llu locked=%y",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    userName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime,
                                    locked
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_updateEntity(IndexHandle  *indexHandle,
                          IndexId      entityId,
                          ConstString  jobUUID,
                          ConstString  scheduleUUID,
                          ConstString  hostName,
                          ConstString  userName,
                          ArchiveTypes archiveType,
                          uint64       createdDateTime
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET jobUUID=%'S, \
                                    scheduleUUID=%'S, \
                                    hostName=%'S, \
                                    userName=%'S, \
                                    created=%llu, \
                                    type=%u \
                                WHERE id=%lld \
                               ",
                               jobUUID,
                               scheduleUUID,
                               hostName,
                               userName,
                               (createdDateTime != 0LL) ? createdDateTime : Misc_getCurrentDateTime(),
                               archiveType,
                               Index_getDatabaseId(entityId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),
                                    "INDEX_UPDATE_ENTITY jobUUID=%S scheduleUUID=%s hostName=%'S userName=%'S archiveType=%s createdDateTime=%llu",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    userName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_lockEntity(IndexHandle *indexHandle,
                        IndexId     entityId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "UPDATE entities SET lockedCount=lockedCount+1 WHERE id=%lld",
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_unlockEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "UPDATE entities SET lockedCount=lockedCount-1 WHERE id=%lld AND lockedCount>0;",
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors        error;
  DatabaseId    uuidId;
  Array         storageIds;
  ArrayIterator arrayIterator;
  DatabaseId    storageId;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    // init variables
    Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

    INDEX_DOX(error,
              indexHandle,
    {
      // get UUID id
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
                             "entities LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID",
                             "uuids.id",
                             "WHERE entities.id=%lld \
                             ",
                             Index_getDatabaseId(entityId)
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get storages to delete
      error = Database_getIds(&indexHandle->databaseHandle,
                              &storageIds,
                              "storages",
                              "storages.id",
                              "LEFT JOIN entities ON entities.id=storages.entityId \
                               WHERE entities.id=%lld \
                              ",
                              Index_getDatabaseId(entityId)
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    // delete storages
    if (error == ERROR_NONE)
    {
      ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
      {
        error = Index_deleteStorage(indexHandle,INDEX_ID_STORAGE(storageId));
      }
    }
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    INDEX_DOX(error,
              indexHandle,
    {
      // set deleted flag
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE entities \
                                SET deletedFlag=1 \
                                WHERE id=%lld \
                               ",
                               Index_getDatabaseId(entityId)
                              );
      if (error == ERROR_NONE)
      {
        return error;
      }

      // prune UUID
      error = pruneUUID(indexHandle,
                         NULL,  // doneFlag
                         NULL,  // deletedCounter
                         uuidId
                        );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    if (error == ERROR_NONE)
    {
      // trigger clean-up thread
      Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
    }

    // free resources
    Array_done(&storageIds);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ENTITY_DELETE entityId=%lld",
                                    entityId
                                   );
  }

  return error;
}

bool Index_isDeletedEntity(IndexHandle *indexHandle,
                           IndexId     entityId
                          )
{
  bool deletedFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(deletedFlag,
              indexHandle,
    {
      return !Database_exists(&indexHandle->databaseHandle,
                              "entities",
                              "id",
                              "WHERE id=%lld AND deletedFlag!=1",
                              Index_getDatabaseId(entityId)
                             );
    });
  }
  else
  {
    // slave mode: always deleted
    deletedFlag = TRUE;
  }

  return deletedFlag;
}

bool Index_isEmptyEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                        )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return isEmptyEntity(indexHandle,
                         Index_getDatabaseId(entityId)
                        );
  });

  return emptyFlag;
}

Errors Index_getStoragesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
                              ConstString   jobUUID,
                              ConstString   scheduleUUID,
                              const IndexId indexIds[],
                              ulong         indexIdCount,
                              IndexTypeSet  indexTypeSet,
                              IndexStateSet indexStateSet,
                              IndexModeSet  indexModeSet,
                              ConstString   name,
                              ulong         *totalStorageCount,
                              uint64        *totalStorageSize,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize,
                              uint64        *totalEntryContentSize
                             )
{
  String              ftsName;
  String              filterString;
  String              uuidIdsString,entityIdsString,storageIdsString;
  ulong               i;
  String              filterIdsString;
  String              string;
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  double              totalStorageSize_,totalEntryCount_,totalEntrySize_,totalEntryContentSize_;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64              t0,t1;
  #endif /* INDEX_DEBUG_LIST_INFO */

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_NONE(entityId) || INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0L) || (indexIds != NULL));

  // init variables
  if (totalStorageCount     != NULL) (*totalStorageCount    ) = 0L;
  if (totalStorageSize      != NULL) (*totalStorageSize     ) = 0LL;
  if (totalEntryCount       != NULL) (*totalEntryCount      ) = 0L;
  if (totalEntrySize        != NULL) (*totalEntrySize       ) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filterIdsString = String_new();
  string          = String_new();
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","entity.uuidId=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_getStoragesInfos ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: indexTypeSet=%s%s%s\n",__FILE__,__LINE__,
            IN_SET(indexTypeSet,INDEX_TYPE_UUID) ? " UUID" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) ? " entity" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) ? " storage" : ""
           );
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));

    t0 = Misc_getTimestamp();
  #endif /* INDEX_DEBUG_LIST_INFO */

  INDEX_DOX(error,
            indexHandle,
  {
    // get storage count, storage size, entry count, entry size
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
//TODO newest
                             "SELECT COUNT(storages.id), \
                                     TOTAL(storages.size), \
                                     TOTAL(storages.totalEntryCount), \
                                     TOTAL(storages.totalEntrySize) \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                              WHERE     storages.deletedFlag!=1 \
                                    AND %S \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    #ifdef INDEX_DEBUG_LIST_INFO
      Database_debugPrintQueryInfo(&databaseStatementHandle);
    #endif
    if (Database_getNextRow(&databaseStatementHandle,
                            "%lu %lf %lf %lf",
                            totalStorageCount,
                            &totalStorageSize_,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
      if (totalStorageSize != NULL) (*totalStorageSize) = (totalStorageSize_ >= 0.0) ? (uint64)totalStorageSize_ : 0LL;
//TODO: may happen?
//      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount  != NULL) (*totalEntryCount ) = (totalEntryCount_  >= 0.0) ? (ulong)totalEntryCount_   : 0L;
      if (totalEntrySize   != NULL) (*totalEntrySize  ) = (totalEntrySize_   >= 0.0) ? (uint64)totalEntrySize_   : 0LL;
    }
    Database_finalize(&databaseStatementHandle);

    if (totalEntryContentSize != NULL)
    {
      // get entry content size
      if      (!String_isEmpty(uuidIdsString))
      {
        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT),
//TODO newest
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM storages \
                                    LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                    LEFT JOIN entities ON entities.id=storages.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE %S \
                                 ",
                                 filterString
                                );
      }
      else if (   !String_isEmpty(entityIdsString)
               || !INDEX_ID_IS_ANY(uuidId)
               || !INDEX_ID_IS_ANY(entityId)
               || (jobUUID != NULL)
               || (scheduleUUID != NULL)
              )
      {
        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT),
//TODO newest
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM storages \
                                    LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                    LEFT JOIN entities         ON entities.id=storages.entityId \
                                  WHERE %S \
                                 ",
                                 filterString
                                );
      }
      else
      {
        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT),
//TODO newest
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM storages \
                                    LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                  WHERE %S \
                                 ",
                                 filterString
                                );
      }
      #ifdef INDEX_DEBUG_LIST_INFO
        Database_debugPrintQueryInfo(&databaseStatementHandle);
      #endif
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lf",
                              &totalEntryContentSize_
                             )
            )
      {
//TODO: may happen?
//      assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (uint64)totalEntryContentSize_ : 0LL;
      }
      Database_finalize(&databaseStatementHandle);
    }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    t1 = Misc_getTimestamp();
    fprintf(stderr,"%s, %d: totalStorageCount=%lu totalStorageSize=%lf totalEntryCount_=%lf totalEntrySize_=%lf\n",__FILE__,__LINE__,*totalStorageCount,totalStorageSize_,totalEntryCount_,totalEntrySize_);
    fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // free resources
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(filterString);
  String_delete(ftsName);

  return ERROR_NONE;
}

Errors Index_updateStorageInfos(IndexHandle *indexHandle,
                                IndexId     storageId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return updateStorageAggregates(indexHandle,Index_getDatabaseId(storageId));
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_UPDATE_INFOS storageId=%lld",
                                    storageId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListStorages(IndexQueryHandle      *indexQueryHandle,
                              IndexHandle           *indexHandle,
                              IndexId               uuidId,
                              IndexId               entityId,
                              ConstString           jobUUID,
                              ConstString           scheduleUUID,
                              const IndexId         indexIds[],
                              ulong                 indexIdCount,
                              IndexTypeSet          indexTypeSet,
                              IndexStateSet         indexStateSet,
                              IndexModeSet          indexModeSet,
                              ConstString           hostName,
                              ConstString           userName,
//TODO: name+pattern
                              ConstString           name,
                              IndexStorageSortModes sortMode,
                              DatabaseOrdering      ordering,
                              uint64                offset,
                              uint64                limit
                             )
{
  String ftsName;
  String filterString;
  String orderString;
  ulong  i;
  String uuidIdsString,entityIdsString,storageIdsString;
  String filterIdsString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0L) || (indexIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");
  orderString  = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  // get filters
  filterIdsString = String_new();
  string          = String_new();
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(hostName),"AND","entities.hostName LIKE %S",hostName);
  filterAppend(filterString,!String_isEmpty(userName),"AND","storages.userName LIKE %S",userName);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  // get sort mode, ordering
  appendOrdering(orderString,sortMode != INDEX_STORAGE_SORT_MODE_NONE,INDEX_STORAGE_SORT_MODE_COLUMNS[sortMode],ordering);

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListStorages ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: indexTypeSet=%s%s%s\n",__FILE__,__LINE__,
            IN_SET(indexTypeSet,INDEX_TYPE_UUID) ? " UUID" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) ? " entity" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) ? " storage" : ""
           );
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));
    fprintf(stderr,"%s, %d: hostName=%s\n",__FILE__,__LINE__,String_cString(hostName));
    fprintf(stderr,"%s, %d: userName=%s\n",__FILE__,__LINE__,String_cString(userName));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
//TODO newest
                            "SELECT IFNULL(uuids.id,0), \
                                    entities.jobUUID, \
                                    IFNULL(entities.id,0), \
                                    entities.scheduleUUID, \
                                    storages.hostName, \
                                    storages.userName, \
                                    storages.comment, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entities.type, \
                                    storages.id, \
                                    storages.name, \
                                    UNIXTIMESTAMP(storages.created), \
                                    storages.size, \
                                    storages.state, \
                                    storages.mode, \
                                    UNIXTIMESTAMP(storages.lastChecked), \
                                    storages.errorMessage, \
                                    storages.totalEntryCount, \
                                    storages.totalEntrySize \
                             FROM storages \
                               LEFT JOIN entities ON entities.id=storages.entityId \
                               LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                             WHERE     storages.deletedFlag!=1 \
                                   AND %S \
                             GROUP BY storages.id \
                             %S \
                             LIMIT %llu,%llu \
                            ",
                            filterString,
                            orderString,
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           scheduleUUID,
                          String           hostName,
                          String           userName,
                          String           comment,
                          uint64           *createdDateTime,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *dateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize
                         )
{
  DatabaseId uuidDatabaseId,entityDatabaseId,storageDatabaseId;
  uint       indexState_,indexMode_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %S %lld %S %S %S %S %llu %u %lld %S %llu %llu %u %u %llu %S %lu %llu",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           comment,
                           createdDateTime,
                           archiveType,
                           &storageDatabaseId,
                           storageName,
                           dateTime,
                           size,
                           &indexState_,
                           &indexMode_,
                           lastCheckedDateTime,
                           errorMessage,
                           totalEntryCount,
                           totalEntrySize
                          )
    )
  {
    return FALSE;
  }
  if (uuidId     != NULL) (*uuidId    ) = INDEX_ID_UUID   (uuidDatabaseId   );
  if (entityId   != NULL) (*entityId  ) = INDEX_ID_ENTITY (entityDatabaseId );
  if (storageId  != NULL) (*storageId ) = INDEX_ID_STORAGE(storageDatabaseId);
  if (indexState != NULL) (*indexState) = (IndexStates)indexState_;
  if (indexMode  != NULL) (*indexMode ) = (IndexModes)indexMode_;

  return TRUE;
}

Errors Index_newStorage(IndexHandle *indexHandle,
                        IndexId     uuidId,
                        IndexId     entityId,
                        ConstString hostName,
                        ConstString userName,
                        ConstString storageName,
                        uint64      dateTime,
                        uint64      size,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       )
{
  Errors     error;
  DatabaseId databaseId;

  assert(indexHandle != NULL);
  assert(storageId != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      StaticString (s,MISC_UUID_STRING_LENGTH);

//TODO: remove with index version 8 without storage constraint
      Misc_getUUID(s);

      // insert storage
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO storages \
                                  ( \
                                   uuidId, \
                                   entityId, \
                                   hostName, \
                                   userName, \
                                   name, \
                                   created, \
                                   size, \
                                   state, \
                                   mode, \
                                   lastChecked\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %'S, \
                                   %'S, \
                                   %'S, \
                                   DATETIME(%llu,'unixepoch'), \
                                   %llu, \
                                   %d, \
                                   %d, \
                                   DATETIME('now') \
                                  ) \
                               ",
                               Index_getDatabaseId(uuidId),
                               Index_getDatabaseId(entityId),
                               hostName,
                               userName,
//                               storageName,
//TODO: remove with index version 8
String_isEmpty(storageName) ? s : storageName,
                               dateTime,
                               size,
                               indexState,
                               indexMode
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      databaseId = Database_getLastRowId(&indexHandle->databaseHandle);

      // insert FTS storage
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO FTS_storages \
                                  ( \
                                   storageId, \
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S \
                                  ) \
                               ",
                               databaseId,
                               storageName
                              );
      if (error != ERROR_NONE)
      {
        (void)Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM storages \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        return error;
      }

      return ERROR_NONE;
    });

    (*storageId) = INDEX_ID_STORAGE(databaseId);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      if (StringMap_getInt64(resultMap,"storageId",storageId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_STORAGE uuidId=%lld entityId=%lld hostName=%'S userName=%'S storageName=%'S dateTime=%llu size=%llu indexState=%s indexMode=%s",
                                    uuidId,
                                    entityId,
                                    hostName,
                                    userName,
                                    storageName,
                                    dateTime,
                                    size,
                                    Index_stateToString(indexState,NULL),
                                    Index_modeToString(indexMode,NULL)
                                   );
  }

  return error;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return clearStorage(indexHandle,
                      Index_getDatabaseId(storageId),
                      NULL  // progressInfo
                     );
}

Errors Index_updateStorage(IndexHandle  *indexHandle,
                           IndexId      storageId,
                           ConstString  hostName,
                           ConstString  userName,
                           ConstString  storageName,
                           uint64       dateTime,
                           uint64       size,
                           ConstString  comment,
                           bool         updateNewest
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assertx(Index_getType(storageId) == INDEX_TYPE_STORAGE,"storageId=%"PRIi64"",storageId);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      if (hostName != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET hostName=%'S \
                                  WHERE id=%lld \
                                 ",
                                 hostName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      if (userName != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET userName=%'S \
                                  WHERE id=%lld \
                                 ",
                                 userName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      if (storageName != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET name=%'S \
                                  WHERE id=%lld \
                                 ",
                                 storageName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE FTS_storages \
                                  SET name=%'S \
                                  WHERE storageId=%lld \
                                 ",
                                 storageName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      if (dateTime != 0LL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET created=DATETIME(%llu,'unixepoch') \
                                  WHERE id=%lld \
                                 ",
                                 dateTime,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storages \
                                SET size=%llu \
                                WHERE id=%lld \
                               ",
                               size,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (comment != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET comment=%'S \
                                  WHERE id=%lld \
                                 ",
                                 comment,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      return ERROR_NONE;
    });

    if (updateNewest)
    {
      error = addStorageToNewest(indexHandle,
                                 Index_getDatabaseId(storageId),
                                 NULL  // progressInfo
                                );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_UPDATE storageId=%lld hostName=%'S userName=%'S storageName=%'S dateTime=%llu storageSize=%llu comment=%'S updateNewest=%y",
                                    storageId,
                                    hostName,
                                    userName,
                                    storageName,
                                    dateTime,
                                    size,
                                    comment,
                                    updateNewest
                                   );
  }

  return error;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors              error;
  DatabaseId          entityId;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalEntryCount;
  uint64              totalEntrySize;
  ulong               totalFileCount;
  uint64              totalFileSize;
  ulong               totalImageCount;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // get entity id
      error = Database_getId(&indexHandle->databaseHandle,
                             &entityId,
                             "storages",
                             "entityId",
                             "WHERE id=%lld \
                             ",
                             Index_getDatabaseId(storageId)
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT,INT64,INT,INT64,INT,INT64,INT,INT,INT,INT64,INT),
                               "SELECT totalEntryCount, \
                                       totalEntrySize, \
                                       totalFileCount, \
                                       totalFileSize, \
                                       totalImageCount, \
                                       totalImageSize, \
                                       totalDirectoryCount, \
                                       totalLinkCount, \
                                       totalHardlinkCount, \
                                       totalHardlinkSize, \
                                       totalSpecialCount \
                                FROM storages \
                                WHERE     deletedFlag!=1 \
                                      AND id=%lld \
                               ",
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (!Database_getNextRow(&databaseStatementHandle,
                               "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
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
                               &totalSpecialCount
                              )
        )
      {
        totalEntryCount     = 0;
        totalEntrySize      = 0LL;
        totalFileCount      = 0;
        totalFileSize       = 0LL;
        totalImageCount     = 0;
        totalImageSize      = 0LL;
        totalDirectoryCount = 0;
        totalLinkCount      = 0;
        totalHardlinkCount  = 0;
        totalHardlinkSize   = 0LL;
        totalSpecialCount   = 0;
      }
      Database_finalize(&databaseStatementHandle);

      // set deleted flag
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE storages \
                                SET deletedFlag=1 \
                                WHERE id=%lld \
                               ",
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//fprintf(stderr,"%s, %d: deleted storageId=%lld\n",__FILE__,__LINE__,Index_getDatabaseId(storageId));
#if 0
fprintf(stderr,"%s, %d: deleted storage %lld\n",__FILE__,__LINE__,Index_getDatabaseId(storageId));
fprintf(stderr,"%s, %d: totalEntry=%lu %llu  totalFile=%lu %llu  totalImage=%lu %llu  totalDirectory=%lu  totalLink=%lu  totalHardlink=%lu %llu totalSpecial=%lu\n",__FILE__,__LINE__,
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
                               totalSpecialCount
);
#endif

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

//TODO: too slow to do it immediately; postpone to index thread
#if 0
    // remove from newest entries
    error = removeStorageFromNewest(indexHandle,
                                    Index_getDatabaseId(storageId),
                                    NULL  // progressInfo
                                   );
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    INDEX_DOX(error,
              indexHandle,
    {
      // update aggregates
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET totalEntryCount    =totalEntryCount    -%lu, \
                                    totalEntrySize     =totalEntrySize     -%llu, \
                                    totalFileCount     =totalFileCount     -%lu, \
                                    totalFileSize      =totalFileSize      -%llu, \
                                    totalImageCount    =totalImageCount    -%lu, \
                                    totalImageSize     =totalImageSize     -%llu, \
                                    totalDirectoryCount=totalDirectoryCount-%lu, \
                                    totalLinkCount     =totalLinkCount     -%lu, \
                                    totalHardlinkCount =totalHardlinkCount -%lu, \
                                    totalHardlinkSize  =totalHardlinkSize  -%llu, \
                                    totalSpecialCount  =totalSpecialCount  -%lu \
                                WHERE id=%lld \
                               ",
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
                               entityId
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // prune entity
      error = pruneEntity(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // trigger clean-up thread
    Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_DELETE storageId=%lld",
                                    storageId
                                   );
  }

  return error;
}

bool Index_hasDeletedStorages(IndexHandle *indexHandle,
                              ulong       *deletedStorageCount
                             )
{
  Errors error;
  int64  n;

  assert(indexHandle != NULL);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getInteger64(&indexHandle->databaseHandle,
                                   &n,
                                   "storages",
                                   "COUNT(id)",
                                   "WHERE deletedFlag=1"
                                  );
    });
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
  }
  else
  {
    // slave mode: no deleted
    n = 0LL;
  }
  if (deletedStorageCount != NULL) (*deletedStorageCount) = (ulong)n;

  return (n > 0LL);
}

bool Index_isDeletedStorage(IndexHandle *indexHandle,
                            IndexId     storageId
                           )
{
  bool deletedFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(deletedFlag,
              indexHandle,
    {
      return !Database_exists(&indexHandle->databaseHandle,
                              "storages",
                              "id",
                              "WHERE id=%lld AND deletedFlag!=1",
                              Index_getDatabaseId(storageId)
                             );
    });
  }
  else
  {
    // slave mode: always deleted
    deletedFlag = TRUE;
  }

  return deletedFlag;
}

bool Index_isEmptyStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(emptyFlag,
              indexHandle,
    {
      return isEmptyStorage(indexHandle,
                            Index_getDatabaseId(storageId)
                           );
    });
  }
  else
  {
    // slave mode: always empty
    emptyFlag = TRUE;
  }

  return emptyFlag;
}

Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       scheduleUUID,
                        ArchiveTypes archiveType,
                        String       storageName,
                        uint64       *dateTime,
                        uint64       *size,
                        IndexStates  *indexState,
                        IndexModes   *indexMode,
                        uint64       *lastCheckedDateTime,
                        String       errorMessage,
                        uint64       *totalEntryCount,
                        uint64       *totalEntrySize
                       )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT uuids.id, \
                                     uuids.jobUUID, \
                                     entities.id, \
                                     entities.scheduleUUID, \
                                     entities.type, \
                                     storages.name, \
                                     UNIXTIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIXTIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE id=%d \
                             ",
                             Index_getDatabaseId(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (!Database_getNextRow(&databaseStatementHandle,
                             "%llu %S %llu %S %u %S %llu %llu %u %u %llu %S %llu %llu",
                             &uuidDatabaseId,
                             jobUUID,
                             &entityDatabaseId,
                             scheduleUUID,
                             archiveType,
                             storageName,
                             dateTime,
                             size,
                             indexState,
                             indexMode,
                             lastCheckedDateTime,
                             errorMessage,
                             totalEntryCount,
                             totalEntrySize
                            )
       )
    {
      Database_finalize(&databaseStatementHandle);
      return ERROR_DATABASE_INDEX_NOT_FOUND;
    }
    if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_(INDEX_TYPE_UUID,  uuidDatabaseId  );
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityDatabaseId);
    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            ulong         indexIdCount,
                            const IndexId entryIds[],
                            ulong         entryIdCount,
                            IndexTypes    indexType,
                            ConstString   name,
                            bool          newestOnly,
                            ulong         *totalStorageCount,
                            uint64        *totalStorageSize,
                            ulong         *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           )
{
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  String              ftsName;
  String              uuidIdsString,entityIdsString;
  String              entryIdsString;
  ulong               i;
  String              filterString;
  int64               totalStorageCount_,totalEntryCount_;
  double              totalStorageSize_,totalEntrySize_,totalEntryContentSize_;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64              t0,t1;
  #endif

  assert(indexHandle != NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName         = String_new();
  uuidIdsString   = String_new();
  entityIdsString = String_new();
  entryIdsString  = String_new();
  filterString    = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_getEntriesInfo --------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: entryIdsString=%s\n",__FILE__,__LINE__,String_cString(entryIdsString));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));
  #endif /* INDEX_DEBUG_LIST_INFO */

  // get filters
  String_setCString(filterString,"1");
  filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  error = ERROR_NONE;
  if (String_isEmpty(ftsName))
  {
    // no names
    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        // get total entry count, total fragment count, total entry size
        if (newestOnly)
        {
          // all newest entries
          if (String_isEmpty(entryIdsString))
          {
            switch (indexType)
            {
              case INDEX_TYPE_NONE:
              case INDEX_TYPE_ANY:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalEntryCountNewest), \
                                                 TOTAL(entities.totalEntrySizeNewest) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_FILE:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalFileCountNewest), \
                                                 TOTAL(entities.totalFileSizeNewest) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_IMAGE:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalImageCountNewest), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_DIRECTORY:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalDirectoryCountNewest), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_LINK:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalLinkCountNewest), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_HARDLINK:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalHardlinkCountNewest), \
                                                 TOTAL(entities.totalHardlinkSizeNewest) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_SPECIAL:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalSpecialCountNewest), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     %S \
                                                AND entities.deletedFlag!=1 \
                                         ",
                                         filterString
                                        );
                break;
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break;
            }
          }
          else
          {
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     "SELECT 0, \
                                                 0, \
                                                 COUNT(entriesNewest.id), \
                                             TOTAL(entriesNewest.size) \
                                      FROM entriesNewest \
                                        LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                        LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                      WHERE     entities.deletedFlag!=1 \
                                            AND %S \
                                     ",
                                     filterString
                                    );
          }
        }
        else
        {
          // all entries
          if (String_isEmpty(entryIdsString))
          {
            switch (indexType)
            {
              case INDEX_TYPE_NONE:
              case INDEX_TYPE_ANY:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalEntryCount), \
                                                 TOTAL(entities.totalEntrySize) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_FILE:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalEntryCount), \
                                                 TOTAL(entities.totalEntrySize) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_IMAGE:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalImageCount), \
                                                 TOTAL(entities.totalImageSize) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_DIRECTORY:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalDirectoryCount), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_LINK:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalLinkCount), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_HARDLINK:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT 0, \
                                                 0, \
                                                 TOTAL(entities.totalHardlinkCount), \
                                                 TOTAL(entities.totalHardlinkSize) \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              case INDEX_TYPE_SPECIAL:
                error = Database_prepare(&databaseStatementHandle,
                                         &indexHandle->databaseHandle,
                                         DATABASE_COLUMN_TYPES(INT),
                                         "SELECT TOTAL(entities.totalSpecialCount), \
                                                 0 \
                                          FROM entities \
                                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %S \
                                         ",
                                         filterString
                                        );
                break;
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break;
            }
          }
          else
          {
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     "SELECT 0, \
                                             0, \
                                             COUNT(entries.id), \
                                             TOTAL(entries.size) \
                                      FROM entries \
                                        LEFT JOIN entities ON entities.id=entries.entityId \
                                        LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                      WHERE     entities.deletedFlag!=1 \
                                            AND %S \
                                     ",
                                     filterString
                                    );
          }
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%llu %lf %llu %lf",
                                 &totalStorageCount_,
                                 &totalStorageSize_,
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries count/size");
        }
        assert(totalEntrySize_ >= 0.0);
        if (totalStorageCount != NULL) (*totalStorageCount) = (ulong)totalStorageCount_;
        if (totalStorageSize  != NULL) (*totalStorageSize ) = (totalStorageSize_ >= 0.0) ? (uint64)totalStorageSize_ : 0LL;
        if (totalEntryCount   != NULL) (*totalEntryCount  ) = (ulong)totalEntryCount_;
        if (totalEntrySize    != NULL) (*totalEntrySize   ) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalStorageCount_=%"PRIi64" totalStorageSize_=%lf totalEntryCount_=%"PRIi64" totalEntrySize_=%lf\n",__FILE__,__LINE__,totalStorageCount_,totalStorageSize_,totalEntryCount_,totalEntrySize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        // get entry content size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                    FROM entriesNewest \
                                      LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                      LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                      LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                      LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.entryId \
                                      LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                      LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     entities.deletedFlag!=1 \
                                          AND %S \
                                   ",
                                   filterString
                                  );
        }
        else
        {
          if (String_isEmpty(entryIdsString))
          {
            // no storages selected, no entries selected -> get aggregated data from entities
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     "SELECT TOTAL(entities.totalEntrySize) \
                                      FROM entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                      WHERE     entities.deletedFlag!=1 \
                                            AND %S \
                                     ",
                                     filterString
                                    );
          }
          else
          {
            // entries selected -> get aggregated data from entries
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                      FROM entries \
                                        LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                        LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                        LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                        LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                        LEFT JOIN entities         ON entities.id=entries.entityId \
                                        LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                      WHERE     entities.deletedFlag!=1 \
                                            AND %S \
                                     ",
                                     filterString
                                    );
          }
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lf",
                                 &totalEntryContentSize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries content size");
        }
//TODO: may happend?
//        assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryContentSize_=%lf\n",__FILE__,__LINE__,totalEntryContentSize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names selected

    // get filters
    filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH '%S'",ftsName);
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        // get entry count, entry size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   "SELECT COUNT(entriesNewest.id), \
                                           TOTAL(entriesNewest.size) \
                                    FROM FTS_entries \
                                      LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                      LEFT JOIN entities      ON entities.id=entriesNewest.entityId \
                                      LEFT JOIN uuids         ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     entities.deletedFlag!=1 \
                                          AND entriesNewest.id IS NOT NULL \
                                          AND %S \
                                   ",
                                   filterString
                                  );
        }
        else
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   "SELECT COUNT(entries.id), \
                                           TOTAL(entries.size) \
                                    FROM FTS_entries \
                                      LEFT JOIN entries  ON entries.id=FTS_entries.entryId \
                                      LEFT JOIN entities ON entities.id=entries.entityId \
                                      LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     entities.deletedFlag!=1 \
                                          AND entries.id IS NOT NULL \
                                          AND %S \
                                   ",
                                   filterString
                                  );
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lld %lf",
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries count/size");
        }
        assert(totalEntrySize_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
        if (totalEntrySize  != NULL) (*totalEntrySize ) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryCount_=%"PRIi64" totalEntrySize_=%lf\n",__FILE__,__LINE__,totalEntryCount_,totalEntrySize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        // get entry content size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                    FROM FTS_entries \
                                      LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                      LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                      LEFT JOIN entries          ON entries.id=entriesNewest.entryId \
                                      LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                      LEFT JOIN entities         ON entities.id=storages.entityId \
                                      LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND %S \
                                   ",
                                   filterString
                                  );
        }
        else
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                    FROM FTS_entries \
                                      LEFT JOIN directoryEntries ON directoryEntries.entryId=FTS_entries.entryId \
                                      LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                      LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                      LEFT JOIN entities         ON entities.id=storages.entityId \
                                      LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND %S \
                                   ",
                                   filterString
                                  );
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lf",
                                 &totalEntryContentSize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries content size");
        }
//TODO: may happend?
//        assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryContentSize_=%lf\n",__FILE__,__LINE__,totalEntryContentSize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

  return error;
}

Errors Index_initListEntries(IndexQueryHandle    *indexQueryHandle,
                             IndexHandle         *indexHandle,
                             const IndexId       indexIds[],
                             ulong               indexIdCount,
                             const IndexId       entryIds[],
                             ulong               entryIdCount,
                             IndexTypes          indexType,
                             ConstString         name,
                             bool                newestOnly,
                             bool                fragmentsCount,
                             IndexEntrySortModes sortMode,
                             DatabaseOrdering    ordering,
                             uint64              offset,
                             uint64              limit
                            )
{
  String ftsName;
  String uuidIdsString,entityIdsString;
  String entryIdsString;
  ulong  i;
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName          = String_new();
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  entryIdsString   = String_new();
  orderString      = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  // get filters
  filterString = String_newCString("1");
  filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
  if (newestOnly)
  {
    filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
  }
  else
  {
    filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
  }

  // get sort mode, ordering
  if (newestOnly)
  {
    appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[sortMode],ordering);
  }
  else
  {
    appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_SORT_MODE_COLUMNS[sortMode],ordering);
  }

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntries -------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  if (String_isEmpty(ftsName))
  {
    // entries selected

    // get additional filters
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    INDEX_DOX(error,
              indexHandle,
    {
      if (newestOnly)
      {
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(INT),
                                "SELECT uuids.id, \
                                        uuids.jobUUID, \
                                        entities.id, \
                                        entities.scheduleUUID, \
                                        entities.hostName, \
                                        entities.userName, \
                                        entities.type, \
                                        entriesNewest.entryId, \
                                        entriesNewest.type, \
                                        entriesNewest.name, \
                                        entriesNewest.timeLastChanged, \
                                        entriesNewest.userId, \
                                        entriesNewest.groupId, \
                                        entriesNewest.permission, \
                                        entriesNewest.size, \
                                        %s, \
                                        CASE entries.type \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN directoryEntries.storageId \
                                          WHEN %u THEN linkEntries.storageId \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN specialEntries.storageId \
                                        END, \
                                        (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN directoryEntries.storageId \
                                                                              WHEN %u THEN linkEntries.storageId \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN specialEntries.storageId \
                                                                            END \
                                        ), \
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySizeNewest, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM entriesNewest \
                                   LEFT JOIN entities         ON entities.id=entriesNewest.entityid \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                 WHERE     entities.deletedFlag!=1 \
                                       AND entriesNewest.id IS NOT NULL \
                                       AND %S \
                                 %S \
                                 LIMIT %llu,%llu \
                                ",
                                fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0",

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                filterString,
                                orderString,
                                offset,
                                limit
                               );
      }
      else
      {
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(INT),
                                "SELECT uuids.id, \
                                        uuids.jobUUID, \
                                        entities.id, \
                                        entities.scheduleUUID, \
                                        entities.hostName, \
                                        entities.userName, \
                                        entities.type, \
                                        entries.id, \
                                        entries.type, \
                                        entries.name, \
                                        entries.timeLastChanged, \
                                        entries.userId, \
                                        entries.groupId, \
                                        entries.permission, \
                                        entries.size, \
                                        %s, \
                                        CASE entries.type \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN directoryEntries.storageId \
                                          WHEN %u THEN linkEntries.storageId \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN specialEntries.storageId \
                                        END, \
                                        (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN directoryEntries.storageId \
                                                                              WHEN %u THEN linkEntries.storageId \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN specialEntries.storageId \
                                                                            END \
                                        ), \
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySize, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM entries \
                                   LEFT JOIN entities         ON entities.id=entries.entityId \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                 WHERE     entities.deletedFlag!=1 \
                                       AND %S \
                                 %S \
                                 LIMIT %llu,%llu \
                                ",
                                fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0",

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                filterString,
                                orderString,
                                offset,
                                limit
                               );
      }
    });
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names (and optional entries) selected

    // get additional filters
    filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH '%S'",ftsName);
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    if (newestOnly)
    {
      INDEX_DOX(error,
                indexHandle,
      {
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(INT),
                                "SELECT uuids.id, \
                                        uuids.jobUUID, \
                                        entities.id, \
                                        entities.scheduleUUID, \
                                        entities.hostName, \
                                        entities.userName, \
                                        entities.type, \
                                        entriesNewest.entryId, \
                                        entriesNewest.type, \
                                        entriesNewest.name, \
                                        entriesNewest.timeLastChanged, \
                                        entriesNewest.userId, \
                                        entriesNewest.groupId, \
                                        entriesNewest.permission, \
                                        entriesNewest.size, \
                                        %s, \
                                        CASE entries.type \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN directoryEntries.storageId \
                                          WHEN %u THEN linkEntries.storageId \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN specialEntries.storageId \
                                        END, \
                                        (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN directoryEntries.storageId \
                                                                              WHEN %u THEN linkEntries.storageId \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN specialEntries.storageId \
                                                                            END \
                                        ), \
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySizeNewest, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM FTS_entries \
                                   LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                   LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                 WHERE     entities.deletedFlag!=1 \
                                       AND entriesNewest.id IS NOT NULL \
                                       AND %S \
                                 %S \
                                 LIMIT %llu,%llu \
                                ",
                                fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0",

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                filterString,
                                orderString,
                                offset,
                                limit
                               );
      });
    }
    else
    {
      INDEX_DOX(error,
                indexHandle,
      {
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(INT),
                                "SELECT uuids.id, \
                                        uuids.jobUUID, \
                                        entities.id, \
                                        entities.scheduleUUID, \
                                        entities.hostName, \
                                        entities.userName, \
                                        entities.type, \
                                        entries.id, \
                                        entries.type, \
                                        entries.name, \
                                        entries.timeLastChanged, \
                                        entries.userId, \
                                        entries.groupId, \
                                        entries.permission, \
                                        entries.size, \
                                        %s, \
                                        CASE entries.type \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN directoryEntries.storageId \
                                          WHEN %u THEN linkEntries.storageId \
                                          WHEN %u THEN 0 \
                                          WHEN %u THEN specialEntries.storageId \
                                        END, \
                                        (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN directoryEntries.storageId \
                                                                              WHEN %u THEN linkEntries.storageId \
                                                                              WHEN %u THEN 0 \
                                                                              WHEN %u THEN specialEntries.storageId \
                                                                            END \
                                        ), \
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySize, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM FTS_entries \
                                   LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                   LEFT JOIN entities         ON entities.id=entries.entityId \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                 WHERE     entities.deletedFlag!=1 \
                                       AND %S \
                                 %S \
                                 LIMIT %llu,%llu \
                                ",
                                fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0",

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                INDEX_TYPE_FILE,
                                INDEX_TYPE_IMAGE,
                                INDEX_TYPE_DIRECTORY,
                                INDEX_TYPE_LINK,
                                INDEX_TYPE_HARDLINK,
                                INDEX_TYPE_SPECIAL,

                                filterString,
                                orderString,
                                offset,
                                limit
                               );
      });
    }
  }
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntry(IndexQueryHandle *indexQueryHandle,
                        IndexId          *uuidId,
                        String           jobUUID,
                        IndexId          *entityId,
                        String           scheduleUUID,
                        String           hostName,
                        String           userName,
                        ArchiveTypes     *archiveType,
                        IndexId          *entryId,
                        String           entryName,
                        IndexId          *storageId,
                        String           storageName,
                        uint64           *size,
                        uint64           *timeModified,
                        uint32           *userId,
                        uint32           *groupId,
                        uint32           *permission,
                        ulong            *fragmentCount,
                        String           destinationName,
                        FileSystemTypes  *fileSystemType,
                        ulong            *blockSize
                       )
{
  IndexTypes indexType;
  DatabaseId uuidDatabaseId,entityDatabaseId,entryDatabaseId;
  int        fileSystemType_;
  DatabaseId storageDatabaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %S %llu %S %S %S %u %llu %u %S %llu %u %u %u %llu %u %llu %S %llu %llu %u %u %llu %S %llu",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           archiveType,
                           &entryDatabaseId,
                           &indexType,
                           entryName,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           size,
                           fragmentCount,
                           &storageDatabaseId,
                           storageName,
                           NULL,  // fileSize,
                           NULL,  // imageSize,
                           &fileSystemType_,
                           &blockSize,
                           NULL,  // &directorySize,
                           destinationName,
                           NULL  // hardlinkSize
                          )
     )
  {
    return FALSE;
  }
  assert(fileSystemType_ >= 0);
//TODO: may happen
  if (uuidId         != NULL) (*uuidId        ) = INDEX_ID_(INDEX_TYPE_UUID,   uuidDatabaseId   );
  if (entityId       != NULL) (*entityId      ) = INDEX_ID_(INDEX_TYPE_ENTITY, entityDatabaseId );
  if (entryId        != NULL) (*entryId       ) = INDEX_ID_(indexType,         entryDatabaseId  );
  if (storageId      != NULL) (*storageId     ) = INDEX_ID_(INDEX_TYPE_STORAGE,storageDatabaseId);
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;

  return TRUE;
}

Errors Index_initListEntryFragments(IndexQueryHandle *indexQueryHandle,
                                    IndexHandle      *indexHandle,
                                    IndexId          entryId,
                                    uint64           offset,
                                    uint64           limit
                                   )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE)
         || (Index_getType(entryId) == INDEX_TYPE_IMAGE)
         || (Index_getType(entryId) == INDEX_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_TYPE_LINK)
         || (Index_getType(entryId) == INDEX_TYPE_HARDLINK)
         || (Index_getType(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntryFragments ------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: entryId=%ld\n",__FILE__,__LINE__,entryId);
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entryFragments.id, \
                                    storages.id, \
                                    storages.name, \
                                    UNIXTIMESTAMP(storages.created), \
                                    entryFragments.offset, \
                                    entryFragments.size \
                             FROM entryFragments \
                               LEFT JOIN storages ON storages.id=entryFragments.storageId \
                             WHERE     storages.deletedFlag!=1 \
                                   AND entryFragments.entryId=%lld \
                             ORDER BY offset ASC \
                             LIMIT %llu,%llu \
                            ",
                            Index_getDatabaseId(entryId),
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntryFragment(IndexQueryHandle *indexQueryHandle,
                                IndexId          *entryFragmentId,
                                IndexId          *storageId,
                                String           storageName,
                                uint64           *storageDateTime,
                                uint64           *fragmentOffset,
                                uint64           *fragmentSize
                               )
{
  DatabaseId entryFragmentDatabaseId,storageDatabaseId;
  int64      fragmentOffset_,fragmentSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%llu %llu %'S %llu %llu %llu",
                           &entryFragmentDatabaseId,
                           &storageDatabaseId,
                           storageName,
                           storageDateTime,
                           &fragmentOffset_,
                           &fragmentSize_
                          )
     )
  {
    return FALSE;
  }
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  if (entryFragmentId != NULL) (*entryFragmentId) = INDEX_ID_(INDEX_TYPE_STORAGE,entryFragmentDatabaseId);
  if (storageId       != NULL) (*storageId      ) = INDEX_ID_(INDEX_TYPE_STORAGE,storageDatabaseId);
  if (fragmentOffset  != NULL) (*fragmentOffset ) = (uint64)fragmentOffset_;
  if (fragmentSize    != NULL) (*fragmentSize   ) = (uint64)fragmentSize_;

  return TRUE;
}

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE)
         || (Index_getType(entryId) == INDEX_TYPE_IMAGE)
         || (Index_getType(entryId) == INDEX_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_TYPE_LINK)
         || (Index_getType(entryId) == INDEX_TYPE_HARDLINK)
         || (Index_getType(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM fileEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM imageEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM directoryEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_LINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM linkEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM hardlinkEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM specialEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entriesNewest WHERE entryId=%lld",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entries WHERE id=%lld",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageSize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalDirectoryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalLinkCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkSize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalSpecialCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    entityIds[],
                           uint             entityIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_FILE);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.size, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       uint64           *createdDateTime,
                       String           fileName,
                       uint64           *size,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %llu %llu %d %d %d",
                           &databaseId,
                           createdDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_FILE(databaseId);

  return TRUE;
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    entityIds[],
                            uint             entityIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      name
                           )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    imageEntries.fileSystemType, \
                                    imageEntries.blockSize, \
                                    entries.size \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        IndexId          *indexId,
                        uint64           *createdDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint             *blockSize,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  DatabaseId databaseId;
  int        fileSystemType_;
  int        blockSize_;
  int64      blockOffset_,blockCount_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %u %u %llu %llu %llu",
                           &databaseId,
                           createdDateTime,
                           imageName,
                           &fileSystemType_,
                           &blockSize_,
                           size,
                           &blockOffset_,
                           &blockCount_
                          )
     )
  {
    return FALSE;
  }
  assert(fileSystemType_ >= 0);
  assert(blockSize_ >= 0);
  assert(blockOffset_ >= 0LL);
  assert(blockCount_ >= 0LL);
  if (indexId != NULL) (*indexId) = INDEX_ID_IMAGE(databaseId);
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;
  if (blockSize      != NULL) (*blockSize     ) = (blockSize_ >= 0) ? (uint)blockSize_ : 0LL;
  if (blockOffset    != NULL) (*blockOffset   ) = (blockOffset_ >= 0LL) ? (uint64)blockOffset_ : 0LL;
  if (blockCount     != NULL) (*blockCount    ) = (blockCount_ >= 0LL) ? (uint64)blockCount_ : 0LL;

  return TRUE;
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    entityIds[],
                                 uint             entityIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            uint64           *createdDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %llu %d %d %d",
                           &databaseId,
                           createdDateTime,
                           directoryName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_DIRECTORY(databaseId);

  return TRUE;
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    entityIds[],
                           uint             entityIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }
  String_appendCString(entryIdsString,"))");

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    linkEntries.destinationName, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     storages.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       uint64           *createdDateTime,
                       String           linkName,
                       String           destinationName,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %S %llu %d %d %d",
                           &databaseId,
                           createdDateTime,
                           linkName,
                           destinationName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_LINK(databaseId);

  return TRUE;
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    entityIds[],
                               uint             entityIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      name
                              )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entries.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.size, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           IndexId          *indexId,
                           uint64           *createdDateTime,
                           String           fileName,
                           uint64           *size,
                           uint64           *timeModified,
                           uint32           *userId,
                           uint32           *groupId,
                           uint32           *permission
                          )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %llu %llu %d %d %d",
                           &databaseId,
                           createdDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_HARDLINK(databaseId);

  return TRUE;
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    entityIds[],
                             uint             entityIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      name
                            )
{
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(INT),
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     entities.deletedFlag!=1 \
                                   AND %S \
                            ",
                            filterString
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          IndexId          *indexId,
                          uint64           *createdDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %llu %d %d %d",
                           &databaseId,
                           createdDateTime,
                           name,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_SPECIAL(databaseId);

  return TRUE;
}

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  Database_finalize(&indexQueryHandle->databaseStatementHandle);
  doneIndexQueryHandle(indexQueryHandle);
}

Errors Index_addFile(IndexHandle *indexHandle,
                     IndexId     uuidId,
                     IndexId     entityId,
                     IndexId     storageId,
                     ConstString name,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission,
                     uint64      fragmentOffset,
                     uint64      fragmentSize
                    )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_FILE,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT OR IGNORE INTO entries \
                                      ( \
                                       entityId, \
                                       type, \
                                       name, \
                                       timeLastAccess, \
                                       timeModified, \
                                       timeLastChanged, \
                                       userId, \
                                       groupId, \
                                       permission, \
                                       \
                                       uuidId,\
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %u, \
                                       %'S, \
                                       %llu, \
                                       %llu, \
                                       %llu, \
                                       %u, \
                                       %u, \
                                       %u, \
                                       \
                                       %lld, \
                                       %lld \
                                      ) \
                                   ",
                                   Index_getDatabaseId(entityId),
                                   INDEX_TYPE_FILE,
                                   name,
                                   timeLastAccess,
                                   timeModified,
                                   timeLastChanged,
                                   userId,
                                   groupId,
                                   permission,

                                   Index_getDatabaseId(uuidId),
                                   size
                                  );

          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO FTS_entries \
                                        ( \
                                         entryId,\
                                         name \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %'S \
                                        ) \
                                     ",
                                     entryId,
                                     name
                                    );
          }

          // add file entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO fileEntries \
                                        ( \
                                         entryId, \
                                         size \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %llu \
                                        ) \
                                     ",
                                     entryId,
                                     size
                                    );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add file entry fragment
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entryFragments \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   offset, \
                                   size\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %llu, \
                                   %llu\
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               fragmentOffset,
                               fragmentSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
                                               name,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_FILE uuidId=%lld entityId=%lld storageId=%llu name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    size,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    fragmentOffset,
                                    fragmentSize
                                   );
  }

  return error;
}

Errors Index_addImage(IndexHandle     *indexHandle,
                      IndexId         uuidId,
                      IndexId         entityId,
                      IndexId         storageId,
                      ConstString     name,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_IMAGE,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT OR IGNORE INTO entries \
                                      ( \
                                       entityId, \
                                       type, \
                                       name, \
                                       timeLastAccess, \
                                       timeModified, \
                                       timeLastChanged, \
                                       userId, \
                                       groupId, \
                                       permission, \
                                       \
                                       uuidId, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %u, \
                                       %'S, \
                                       %llu, \
                                       %llu, \
                                       %llu, \
                                       %u, \
                                       %u, \
                                       %u, \
                                       \
                                       %lld, \
                                       %lld \
                                      ) \
                                   ",
                                   Index_getDatabaseId(entityId),
                                   INDEX_TYPE_IMAGE,
                                   name,
                                   0LL,
                                   0LL,
                                   0LL,
                                   0,
                                   0,
                                   0,

                                   Index_getDatabaseId(uuidId),
                                   size
                                  );
          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO FTS_entries \
                                        ( \
                                         entryId,\
                                         name \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %'S \
                                        ) \
                                     ",
                                     entryId,
                                     name
                                    );
          }

          // add image entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     DATABASE_COLUMN_TYPES(),
                                     "INSERT INTO imageEntries \
                                        ( \
                                         entryId, \
                                         fileSystemType, \
                                         size, \
                                         blockSize \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %u, \
                                         %llu, \
                                         %u \
                                        ) \
                                     ",
                                     entryId,
                                     fileSystemType,
                                     size,
                                     blockSize
                                    );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add image entry fragment
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "INSERT INTO entryFragments \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   offset, \
                                   size\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %llu, \
                                   %llu \
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               blockOffset*(uint64)blockSize,
                               blockCount*(uint64)blockSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%lld AND totalEntrySize<0",Index_getDatabaseId(storageId));
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%lld AND totalFileSize<0",Index_getDatabaseId(storageId));
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_IMAGE uuidId=%lld entityId=%lld storageId=%llu type=IMAGE name=%'S fileSystemType=%'s size=%llu blockSize=%lu blockOffset=%llu blockCount=%llu",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    FileSystem_fileSystemTypeToString(fileSystemType,"unknown"),
                                    size,
                                    blockSize,
                                    blockOffset,
                                    blockCount
                                   );
  }

  return error;
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          IndexId     uuidId,
                          IndexId     entityId,
                          IndexId     storageId,
                          String      name,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   entityId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission, \
                                   \
                                   uuidId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %u, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u, \
                                   \
                                   %lld, \
                                   0 \
                                  ) \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_DIRECTORY,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission,

                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO FTS_entries \
                                  ( \
                                   entryId,\
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S \
                                  ) \
                               ",
                               entryId,
                               name
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add directory entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO directoryEntries \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %'S\
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               name
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
                                               name,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalDirectoryCount<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_DIRECTORY uuidId=%lld entityId=%lld storageId=%llu type=DIRECTORY name=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission
                                   );
  }

  return error;
}

Errors Index_addLink(IndexHandle *indexHandle,
                     IndexId     uuidId,
                     IndexId     entityId,
                     IndexId     storageId,
                     ConstString linkName,
                     ConstString destinationName,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   entityId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission, \
                                   \
                                   uuidId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %u, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u, \
                                   \
                                   %lld, \
                                   0 \
                                  ) \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_LINK,
                               linkName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission,

                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO FTS_entries \
                                  ( \
                                   entryId,\
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S \
                                  ) \
                               ",
                               entryId,
                               linkName
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add link entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO linkEntries \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   destinationName \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %'S \
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               destinationName
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
                                               linkName,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_LINK uuidId=%lld entityId=%lld storageId=%llu type=LINK name=%'S destinationName=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    linkName,
                                    destinationName,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission
                                   );
  }

  return error;
}

Errors Index_addHardlink(IndexHandle *indexHandle,
                         IndexId     uuidId,
                         IndexId     entityId,
                         IndexId     storageId,
                         ConstString name,
                         uint64      size,
                         uint64      timeLastAccess,
                         uint64      timeModified,
                         uint64      timeLastChanged,
                         uint32      userId,
                         uint32      groupId,
                         uint32      permission,
                         uint64      fragmentOffset,
                         uint64      fragmentSize
                        )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_HARDLINK,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT OR IGNORE INTO entries \
                                      ( \
                                       entityId, \
                                       type, \
                                       name, \
                                       timeLastAccess, \
                                       timeModified, \
                                       timeLastChanged, \
                                       userId, \
                                       groupId, \
                                       permission, \
                                       \
                                       uuidId,\
                                       size\
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %u, \
                                       %'S, \
                                       %llu, \
                                       %llu, \
                                       %llu, \
                                       %u, \
                                       %u, \
                                       %u, \
                                       \
                                       %lld, \
                                       %lld \
                                      ) \
                                   ",
                                   Index_getDatabaseId(entityId),
                                   INDEX_TYPE_HARDLINK,
                                   name,
                                   timeLastAccess,
                                   timeModified,
                                   timeLastChanged,
                                   userId,
                                   groupId,
                                   permission,

                                   Index_getDatabaseId(uuidId),
                                   size
                                  );

          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO FTS_entries \
                                        ( \
                                         entryId,\
                                         name \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %'S \
                                        ) \
                                     ",
                                     entryId,
                                     name
                                    );
          }

          // add hard link entry
          if (error == ERROR_NONE)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "INSERT INTO hardlinkEntries \
                                        ( \
                                         entryId, \
                                         size \
                                        ) \
                                      VALUES \
                                        ( \
                                         %lld, \
                                         %llu \
                                        ) \
                                     ",
                                     entryId,
                                     size
                                    );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add hard link entry fragment
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entryFragments \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   offset, \
                                   size\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %llu, \
                                   %llu\
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               fragmentOffset,
                               fragmentSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
                                               name,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_HARDLINK uuidId=%lld entityId=%lld storageId=%llu type=HARDLINK name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    size,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    fragmentOffset,
                                    fragmentSize
                                   );
  }

  return error;
}

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        IndexId          uuidId,
                        IndexId          entityId,
                        IndexId          storageId,
                        ConstString      name,
                        FileSpecialTypes specialType,
                        uint64           timeLastAccess,
                        uint64           timeModified,
                        uint64           timeLastChanged,
                        uint32           userId,
                        uint32           groupId,
                        uint32           permission,
                        uint32           major,
                        uint32           minor
                       )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   entityId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission, \
                                   \
                                   uuidId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %u, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u,\
                                   \
                                   %lld, \
                                   0 \
                                  ) \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_SPECIAL,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission,

                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO FTS_entries \
                                  ( \
                                   entryId,\
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S \
                                  ) \
                               ",
                               entryId,
                               name
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add special entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO specialEntries \
                                  ( \
                                   entryId, \
                                   storageId, \
                                   specialType, \
                                   major, \
                                   minor \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %u, \
                                   %d, \
                                   %d\
                                  ) \
                               ",
                               entryId,
                               Index_getDatabaseId(storageId),
                               specialType,
                               major,
                               minor
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
                                               name,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_SPECIAL uuidId=%lld entityId=%lld storageId=%llu type=SPECIAL name=%'S specialType=%s timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o major=%u minor=%u",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    File_fileSpecialTypeToString(specialType,""),
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    major,
                                    minor
                                   );
  }

  return error;
}

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      ConstString  toJobUUID,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      if      (toJobUUID != NULL)
      {
        if (!INDEX_ID_IS_NONE(entityId) && !INDEX_ID_IS_DEFAULT_ENTITY(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign entity to other job
          error = assignEntityToJob(indexHandle,
                                    Index_getDatabaseId(entityId),
                                    toJobUUID,
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all entities of job to other job
          error = assignJobToJob(indexHandle,
                                 jobUUID,
                                 toJobUUID,
                                 toArchiveType
                                );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
      else if (!INDEX_ID_IS_NONE(toEntityId))
      {
        // assign to other entity

        if (!INDEX_ID_IS_NONE(storageId))
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage to other entity
          error = assignStorageToEntity(indexHandle,
                                        Index_getDatabaseId(storageId),
                                        Index_getDatabaseId(toEntityId)
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!INDEX_ID_IS_NONE(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign entity to other entity
          error = assignEntityToEntity(indexHandle,
                                       Index_getDatabaseId(entityId),
                                       Index_getDatabaseId(toEntityId),
                                       toArchiveType
                                      );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
#if 0
//TODO: not used
          // assign all entities of job to other entity
          error = assignJobToEntity(indexHandle,
                                    jobUUID,
                                    Index_getDatabaseId(toEntityId),
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignJobToEntity");
#endif
        }
      }
      else if (!INDEX_ID_IS_NONE(toStorageId))
      {
        // assign to other storage

        if (!INDEX_ID_IS_NONE(storageId))
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage entries to other storage
          error = assignStorageEntriesToStorage(indexHandle,
                                                Index_getDatabaseId(storageId),
                                                Index_getDatabaseId(toStorageId)
                                               );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!INDEX_ID_IS_NONE(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

#if 0
//TODO: not used
          // assign all storage entries of entity to other storage
          error = assignEntityToStorage(indexHandle,
                                        Index_getDatabaseId(entityId),
                                        Index_getDatabaseId(toStorageId)
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignEntityToStorage");
#endif
        }

        if (!String_isEmpty(jobUUID))
        {
#if 0
//TODO: not used
          // assign all storage entries of all entities of job to other storage
          error = assignJobToStorage(indexHandle,
                                     jobUUID,
                                     Index_getDatabaseId(toStorageId)
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignJobToStorage");
#endif
        }
      }

      #ifndef NDEBUG
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageSize<0");
        verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ERROR_STILL_NOT_IMPLEMENTED;
  }

  return error;
}

Errors Index_pruneUUID(IndexHandle *indexHandle,
                       IndexId     indexId
                      )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_UUID);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return pruneUUID(indexHandle,
                       NULL,  // doneFlag
                       NULL,  // deletedCounter
                       Index_getDatabaseId(indexId)
                      );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_PRUNE_UUID uuidId=%lld",
                                    indexId
                                   );
  }

  return error;
}

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_ENTITY);
  assert(Index_getDatabaseId(indexId) != INDEX_DEFAULT_ENTITY_DATABASE_ID);

  // prune storages of entity if not default entity
  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return pruneEntity(indexHandle,
                         NULL,  // doneFlag
                         NULL,  // deletedCounter
                         Index_getDatabaseId(indexId)
                        );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_PRUNE_ENTITY entityId=%lld",
                                    indexId
                                   );
  }

  return error;
}

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          IndexId     indexId
                         )
{
  Errors  error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  INDEX_DOX(error,
            indexHandle,
  {
    return pruneStorage(indexHandle,
                        Index_getDatabaseId(indexId),
                        NULL  // progressInfo
                       );
  });

  return error;
}

Errors Index_initListSkippedEntry(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  ulong            indexIdCount,
                                  const IndexId    entryIds[],
                                  ulong            entryIdCount,
                                  IndexTypes       indexType,
                                  ConstString      name,
                                  DatabaseOrdering ordering,
                                  uint64           offset,
                                  uint64           limit
                                 )
{
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(indexIds);
  UNUSED_VARIABLE(indexIdCount);
  UNUSED_VARIABLE(entryIds);
  UNUSED_VARIABLE(entryIdCount);
  UNUSED_VARIABLE(indexType);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(ordering);
  UNUSED_VARIABLE(offset);
  UNUSED_VARIABLE(limit);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

bool Index_getNextSkippedEntry(IndexQueryHandle *indexQueryHandle,
                               IndexId          *uuidId,
                               String           jobUUID,
                               IndexId          *entityId,
                               String           scheduleUUID,
                               ArchiveTypes     *archiveType,
                               IndexId          *storageId,
                               String           storageName,
                               uint64           *storageDateTime,
                               IndexId          *entryId,
                               String           entryName
                              )
{
//TODO
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(uuidId);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(scheduleUUID);
  UNUSED_VARIABLE(archiveType);
  UNUSED_VARIABLE(storageId);
  UNUSED_VARIABLE(storageName);
  UNUSED_VARIABLE(storageDateTime);
  UNUSED_VARIABLE(entryId);
  UNUSED_VARIABLE(entryName);

  return FALSE;
}

Errors Index_addSkippedEntry(IndexHandle *indexHandle,
                             IndexId     entityId,
                             IndexTypes  indexType,
                             ConstString entryName
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
  assert(   (indexType == INDEX_TYPE_FILE           )
         || (indexType == INDEX_CONST_TYPE_IMAGE    )
         || (indexType == INDEX_CONST_TYPE_DIRECTORY)
         || (indexType == INDEX_CONST_TYPE_LINK     )
         || (indexType == INDEX_CONST_TYPE_HARDLINK )
         || (indexType == INDEX_CONST_TYPE_SPECIAL  )
        );
  assert(entryName != NULL);

  // check init errorindexId
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "INSERT INTO skippedEntries \
                                ( \
                                 entityId, \
                                 type, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %u, \
                                 %'S \
                                ) \
                             ",
                             Index_getDatabaseId(entityId),
                             indexType,
                             entryName
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_deleteSkippedEntry(IndexHandle *indexHandle,
                                IndexId     indexId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (Index_getType(indexId) == INDEX_TYPE_FILE           )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_IMAGE    )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_DIRECTORY)
         || (Index_getType(indexId) == INDEX_CONST_TYPE_LINK     )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_HARDLINK )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_SPECIAL  )
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM skippedEntries WHERE id=%lld",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

#ifdef INDEX_DEBUG_LOCK
void Index_debugPrintInUseInfo(void)
{
  ArrayIterator arrayIterator;
  ThreadInfo    threadInfo;

  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    fprintf(stderr,
            "Index in use by: %lu threads\n",
            Array_length(&indexUsedBy)
           );
    ARRAY_ITERATE(&indexUsedBy,arrayIterator,threadInfo)
    {
      fprintf(stderr,
              "  %s: %s\n",
              Thread_getIdString(threadInfo.threadId),
              Thread_getName(threadInfo.threadId)
             );

      #ifndef NDEBUG
        debugDumpStackTrace(stderr,
                            2,
                            DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                            threadInfo.stackTrace,
                            threadInfo.stackTraceSize,
                            0
                           );
      #endif /* NDEBUG */
    }
  }
}
#endif /* not INDEX_DEBUG_LOCK */

#ifdef __cplusplus
  }
#endif

/* end of file */
