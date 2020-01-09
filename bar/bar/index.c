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
#define INDEX_INTIIAL_CLEANUP            // switch off for debugging only!
#define INDEX_IMPORT_OLD_DATABASE        // switch off for debugging only!
#define _INDEX_DEBUG_IMPORT_OLD_DATABASE  // switch off for debugging only!
#define INDEX_SUPPORT_DELETE             // switch off for debugging only!

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
const IndexId INDEX_ID_NONE = {INDEX_TYPE_NONE,0LL };
const IndexId INDEX_ID_ANY  = {INDEX_TYPE_NONE,-1LL};
#endif

// index open mask
#define INDEX_OPEN_MASK_MODE  0x0000000F
#define INDEX_OPEN_MASK_FLAGS 0xFFFF0000

#define DATABASE_TIMEOUT (30L*MS_PER_SECOND)

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
  { "FILE",      INDEX_TYPE_FILE      },
  { "IMAGE",     INDEX_TYPE_IMAGE     },
  { "DIRECTORY", INDEX_TYPE_DIRECTORY },
  { "LINK",      INDEX_TYPE_LINK      },
  { "HARDLINK",  INDEX_TYPE_HARDLINK  },
  { "SPECIAL",   INDEX_TYPE_SPECIAL   },
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
  [INDEX_ENTRY_SORT_MODE_SIZE        ] = "entries.size",
  [INDEX_ENTRY_SORT_MODE_FRAGMENT    ] = "entryFragments.offset",
  [INDEX_ENTRY_SORT_MODE_LAST_CHANGED] = "entries.timeLastChanged"
};
LOCAL const char *INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE        ] = NULL,

  [INDEX_ENTRY_SORT_MODE_ARCHIVE     ] = "storages.name",
  [INDEX_ENTRY_SORT_MODE_NAME        ] = "entriesNewest.name",
  [INDEX_ENTRY_SORT_MODE_TYPE        ] = "entriesNewest.type",
  [INDEX_ENTRY_SORT_MODE_SIZE        ] = "entriesNewest.size",
  [INDEX_ENTRY_SORT_MODE_FRAGMENT    ] = "entryFragments.offset",
  [INDEX_ENTRY_SORT_MODE_LAST_CHANGED] = "entriesNewest.timeLastChanged"
};

// time for index clean-up [s]
#define TIME_INDEX_CLEANUP (4*S_PER_HOUR)

// sleep time [s]
#define SLEEP_TIME_INDEX_CLEANUP_THREAD 20L

// server i/o
#define SERVER_IO_DEBUG_LEVEL 1
#define SERVER_IO_TIMEOUT     (30LL*MS_PER_SECOND)

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
#define INDEX_OPEN_MODE_FOREIGN_KEYS (1 << 17)

/***************************** Variables *******************************/
LOCAL const char                 *indexDatabaseFileName = NULL;
LOCAL bool                       indexInitializedFlag = FALSE;
LOCAL Semaphore                  indexBusyLock;
LOCAL ThreadId                   indexBusyThreadId;
LOCAL Semaphore                  indexLock;
LOCAL uint                       indexUseCount = 0;
LOCAL Semaphore                  indexPauseLock;
LOCAL IndexPauseCallbackFunction indexPauseCallbackFunction = NULL;
LOCAL void                       *indexPauseCallbackUserData;
LOCAL Semaphore                  indexThreadTrigger;
LOCAL Thread                     indexThread;    // upgrad/clean-up thread
LOCAL bool                       quitFlag;

LOCAL uint64                     importStartTimestamp;
LOCAL uint64                     importSteps,importMaxSteps;
LOCAL ulong                      importLastProgressSum;  // last progress sum [1/1000]
LOCAL uint                       importLastProgressCount;
LOCAL uint64                     importLastProgressTimestamp;

#ifndef NDEBUG
  void const *indexBusyStackTrace[32];
  uint       indexBusyStackTraceSize;
#endif /* not NDEBUG */

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
      fprintf(stderr,"DEBUG import %s, %4d: ",__FILE__,__LINE__); \
      fprintf(stderr,format, ## __VA_ARGS__); \
      fprintf(stderr,"\n"); \
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
    ATOMIC_INCREMENT(indexUseCount); \
    ({ \
      auto void __closure__(void); \
      \
      void __closure__(void)block; __closure__; \
    })(); \
    ATOMIC_DECREMENT(indexUseCount); \
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
    ATOMIC_INCREMENT(indexUseCount); \
    result = ({ \
               auto typeof(result) __closure__(void); \
               \
               typeof(result) __closure__(void)block; __closure__; \
             })(); \
    ATOMIC_DECREMENT(indexUseCount); \
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
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              INDEX_DEFINITION
                             );
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
                             ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE) ? DATABASE_OPENMODE_READWRITE : DATABASE_OPENMODE_READ,
                             timeout
                            );
      #else /* not NDEBUG */
        return __Database_open(__fileName__,__lineNb__,
                               &indexHandle->databaseHandle,
                               databaseFileName,
                               ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE) ? DATABASE_OPENMODE_READWRITE : DATABASE_OPENMODE_READ,
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
    if ((indexOpenMode & INDEX_OPEN_MODE_FOREIGN_KEYS) != 0)
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
                         "UPDATE %s SET id=rowId WHERE id IS NULL;",
                         tableName
                        );
}

/***********************************************************************\
* Name   : initImportProgress
* Purpose: init import progress
* Input  : maxSteps - max. number of steps
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initImportProgress(uint64 maxSteps)
{
  importStartTimestamp        = Misc_getTimestamp();
  importSteps                 = 0;
  importMaxSteps              = maxSteps;
  importLastProgressSum       = 0L;
  importLastProgressCount     = 0;
  importLastProgressTimestamp = 0LL;
}

/***********************************************************************\
* Name   : doneImportProgress
* Purpose: done import progress
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneImportProgress(void)
{
}

/***********************************************************************\
* Name   : importProgress
* Purpose: log import progress
* Input  : userData - user data (not used)
* Output : -
* Return : -
* Notes  : increment step counter for each call!
\***********************************************************************/

LOCAL void importProgress(void *userData)
{
  uint   progress;
  uint   importLastProgress;
  uint64 now;
  uint64 elapsedTime,importTotalTime;
  uint64 estimatedRestTime;

  UNUSED_VARIABLE(userData);

  importSteps++;

  progress           = (importSteps*1000)/importMaxSteps;
  importLastProgress = (importLastProgressCount > 0) ? (uint)(importLastProgressSum/(ulong)importLastProgressCount) : 0;
  now                = Misc_getTimestamp();
  if (   (progress > importLastProgress)
      && (now > (importLastProgressTimestamp+60*US_PER_SECOND))
     )
  {
    elapsedTime       = now-importStartTimestamp;
    importTotalTime   = (elapsedTime*importMaxSteps)/importSteps;
    estimatedRestTime = importTotalTime-elapsedTime;

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Imported %0.1f%%, estimated rest time %uh:%02umin:%02us",
                (float)progress/10.0,
                (uint)((estimatedRestTime/US_PER_SECOND)/3600LL),
                (uint)(((estimatedRestTime/US_PER_SECOND)%3600LL)/60),
                (uint)((estimatedRestTime/US_PER_SECOND)%60LL)
               );
    importLastProgressSum       += progress;
    importLastProgressCount     += 1;
    importLastProgressTimestamp = now;
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

  importProgress(NULL);
}

#include "index_version1.c"
#include "index_version2.c"
#include "index_version3.c"
#include "index_version4.c"
#include "index_version5.c"
#include "index_version6.c"
#include "index_version7.c"

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
  IndexQueryHandle indexQueryHandle;
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
  switch (indexVersion)
  {
    case 1:
      error = upgradeFromVersion1(&oldIndexHandle,indexHandle);
      break;
    case 2:
      error = upgradeFromVersion2(&oldIndexHandle,indexHandle);
      break;
    case 3:
      error = upgradeFromVersion3(&oldIndexHandle,indexHandle);
      break;
    case 4:
      error = upgradeFromVersion4(&oldIndexHandle,indexHandle);
      break;
    case 5:
      error = upgradeFromVersion5(&oldIndexHandle,indexHandle);
      break;
    case 6:
      error = upgradeFromVersion6(&oldIndexHandle,indexHandle);
      break;
    case INDEX_CONST_VERSION:
      error = importCurrentVersion(&oldIndexHandle,indexHandle);
      break;
    default:
      // unknown version if index
      error = ERROR_DATABASE_VERSION_UNKNOWN;
      break;
  }
  DIMPORT("import index done (error: %s)",Error_getText(error));

  DIMPORT("create aggregates");
fprintf(stderr,"%s, %d: +++++++++++++++++++++++++++\n",__FILE__,__LINE__);
  if (error == ERROR_NONE)
  {
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entityId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // storageIdCount,
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
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName,
                                   NULL,  // createdDateTime
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
fprintf(stderr,"%s, %d: aggre stoage %ld\n",__FILE__,__LINE__,storageId);
      error = Index_updateStorageInfos(indexHandle,storageId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
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
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
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
fprintf(stderr,"%s, %d: aggre entiy %ld\n",__FILE__,__LINE__,entityId);
      error = Index_updateEntityInfos(indexHandle,entityId);
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
fprintf(stderr,"%s, %d: aggre uuid %ld\n",__FILE__,__LINE__,uuidId);
      error = Index_updateUUIDInfos(indexHandle,uuidId);
    }
    Index_doneList(&indexQueryHandle);
  }
  DIMPORT("create aggregates done (error: %s)",Error_getText(error));

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
//  DatabaseQueryHandle databaseQueryHandle;

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
    (void)Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DELETE FROM meta \
                            WHERE ROWID NOT IN (SELECT MIN(rowid) FROM meta GROUP BY name); \
                           "
                          );
#if 0
  if (Database_prepare(&databaseQueryHandle,
                       &indexHandle->databaseHandle,
                       "SELECT name FROM meta GROUP BY name"
                      ) == ERROR_NONE
     )
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT name FROM meta GROUP BY name"
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseQueryHandle,
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
                                      AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1)); \
                               ",
                               name,
                               name
                              );
      }
      Database_finalize(&databaseQueryHandle);
    }
#endif

    return error;
  });
//TODO: error?

  // free resources
//  String_delete(name);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up duplicate meta data"
             );

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
    (void)Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entities SET lockedCount=0"
                          );

    error = ERROR_NONE;
    while (Index_findStorageByState(indexHandle,
                                    INDEX_STATE_SET(INDEX_STATE_UPDATE),
                                    NULL,  // uuidId
                                    NULL,  // jobUUID
                                    NULL,  // entityId
                                    NULL,  // scheduleUUID
                                    &indexId,
                                    storageName,
                                    NULL,  // createdDateTime
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

      error = Index_setState(indexHandle,
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
                "Clean-up incomplete updates"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up incomplete updates fail (error: %s)",
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
              "Start clean-up incomplete created entries"
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
                                  &indexId,
                                  storageName,
                                  NULL,  // createdDateTime
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

    error = Index_deleteStorage(indexHandle,indexId);
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Deleted incomplete index #%lld: '%s'",
                  indexId,
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
                "Clean-up incomplete created entries"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up incomplete created entries fail (error: %s)",
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
                                 0,  // storageIdCount,
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
                                NULL,  // archiveType
                                &storageId,
                                storageName,
                                NULL,  // createdDateTime
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

  if (n > 0L)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Cleaned %lu indizes without name",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without name"
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStorageNoEntity
* Purpose: assign entity to storage entries without any entity
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
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
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
    error = Database_prepare(&databaseQueryHandle1,
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
      while (Database_getNextRow(&databaseQueryHandle1,
                                 "%S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseQueryHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=%'S \
                                 ",
                                 uuid
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseQueryHandle2,
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
                                      WHERE id=%lld; \
                                     ",
                                     entityDatabaseId,
                                     storageDatabaseId
                                    );
            }
          }
          Database_finalize(&databaseQueryHandle2);
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
                                    ); \
                                 ",
                                 uuid,
                                 createdDateTime,
                                 ARCHIVE_TYPE_FULL
                                );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);
fprintf(stderr,"%s, %d: new entityId=%d\n",__FILE__,__LINE__,entityId);

          // assign entity id for all storage entries with same uuid and matching name (equals except digits)
          error = Database_prepare(&databaseQueryHandle2,
                                   &oldIndexHandle->databaseHandle,
                                   "SELECT id, \
                                           name \
                                    FROM storages \
                                    WHERE uuid=%'S \
                                   ",
                                   uuid
                                  );
          if (error == ERROR_NONE)
          {
            while (Database_getNextRow(&databaseQueryHandle2,
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
                                        WHERE id=%lld; \
                                       ",
                                       entityId,
                                       storageId
                                      );
              }
            }
            Database_finalize(&databaseQueryHandle2);
          }
        }
      }
      Database_finalize(&databaseQueryHandle1);
    }

    return ERROR_NONE;
  });

  // free resources
  String_delete(name2);
  String_delete(name1);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up no entity-entries"
             );
#else
UNUSED_VARIABLE(indexHandle);
return ERROR_NONE;
#endif
}

/***********************************************************************\
* Name   : purge
* Purpose: purge with delay/check if index-usage
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          tableName      - table name
*          filter         - filter string
*          ...            - optional arguments for filter
* Output : doneFlag - set to FALSE if delete not completely done
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

//fprintf(stderr,"%s, %d: indexUseCount=%d tableName=%s filterString=%s\n",__FILE__,__LINE__,indexUseCount,tableName,String_cString(filterString));
  error = ERROR_NONE;
  do
  {
    changedRowCount = 0;
//Database_debugEnable(&indexHandle->databaseHandle,TRUE);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             &changedRowCount,
                             "DELETE FROM %s \
                              WHERE %S \
                              LIMIT 64 \
                             ",
                             tableName,
                             filterString
                            );
//Database_debugEnable(&indexHandle->databaseHandle,FALSE);
    if (error == ERROR_NONE)
    {
      if (deletedCounter != NULL)(*deletedCounter) += changedRowCount;
    }
//fprintf(stderr,"%s, %d: tableName=%s indexUseCount=%d changedRowCount=%d doneFlag=%d\n",__FILE__,__LINE__,tableName,indexUseCount,changedRowCount,(doneFlag != NULL) ? *doneFlag : -1);
  }
  while (   (indexUseCount == 0)
         && (error == ERROR_NONE)
         && (changedRowCount > 0)
        );
  if ((error == ERROR_NONE) && (doneFlag != NULL))
  {
    if (Database_exists(&indexHandle->databaseHandle,tableName,"id","WHERE %S",filterString))
    {
      (*doneFlag) = FALSE;
    }
  }

  // free resources
  String_delete(filterString);

  return error;
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
  assert(storageId != DATABASE_ID_NONE);

  return    !Database_exists(&indexHandle->databaseHandle,
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
* Name   : pruneStorage
* Purpose: prune empty storage
* Input  : indexHandle - index handle
*          storageId   - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorage(IndexHandle *indexHandle,
                          DatabaseId  storageId
                         )
{
  Errors error;
  String name;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // prune storage if empty and not in use
//TODO: how to check in use?
  if (isEmptyStorage(indexHandle,storageId))
  {
    // get storage name
    name = String_new();
    error = Database_getString(&indexHandle->databaseHandle,
                               name,
                               "storages",
                               "name",
                               "WHERE id=%lld",
                               storageId
                              );
    if (error != ERROR_NONE)
    {
      String_clear(name);
    }

    // purge storage
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM storages \
                              WHERE id=%lld \
                             ",
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      String_delete(name);
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Purged storage #%llu, %s: no entries",
                storageId,
                String_cString(name)
               );

    String_delete(name);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneStorages
* Purpose: prune all empty storages which have state OK
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorages(IndexHandle *indexHandle)
{
  Array         storageIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);

  Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
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
      return error;
    }

    ARRAY_ITERATEX(&storageIds,arrayIterator,databaseId,error == ERROR_NONE)
    {
      error = pruneStorage(indexHandle,databaseId);
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });
  Array_done(&storageIds);

  return error;
}

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

  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
    Array_clear(&databaseIds);
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "fileEntries",
                            "fileEntries.id",
                            "LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.id WHERE entryFragments.id IS NULL \
                            "
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

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
      return error;
    }

    Array_clear(&databaseIds);
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "imageEntries",
                            "imageEntries.id",
                            "LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.id WHERE entryFragments.id IS NULL \
                            "
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

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
      return error;
    }

    Array_clear(&databaseIds);
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "hardlinkEntries",
                            "hardlinkEntries.id",
                            "LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.id WHERE entryFragments.id IS NULL \
                            "
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

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
      return error;
    }

    return ERROR_NONE;
  });
  Array_done(&databaseIds);

  return error;
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
  assert(entityId != DATABASE_ID_NONE);

  return !Database_exists(&indexHandle->databaseHandle,
                          "storages",
                          "id",
                          "WHERE entityId=%lld",
                          entityId
                         );
}

/***********************************************************************\
* Name   : pruneEntity
* Purpose: prune entity if empty
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
  int64               lockedCount;
  Errors              error;
  Array               databaseIds;
  ArrayIterator       arrayIterator;
  DatabaseId          databaseId;
  DatabaseQueryHandle databaseQueryHandle;
  StaticString        (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  ArchiveTypes        archiveType;

  assert(indexHandle != NULL);
  assert(entityId != DATABASE_ID_NONE);

  if (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
  {
#warning requried?
//TODO: required?
//TODO: race condition! change entity lock while list storages
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
      return error;
    }

    if (lockedCount == 0LL)
    {
      Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
      error = Database_getIds(&indexHandle->databaseHandle,
                              &databaseIds,
                              "storages",
                              "id",
                              "WHERE     entityId=%lld \
                                     AND state IN (%u,%u) \
                              ",
                              entityId,
                              INDEX_STATE_OK,
                              INDEX_STATE_ERROR
                             );
      if (error != ERROR_NONE)
      {
        Array_done(&databaseIds);
        return error;
      }
      ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
      {
        error = pruneStorage(indexHandle,databaseId);
      }
      if (error != ERROR_NONE)
      {
        Array_done(&databaseIds);
        return error;
      }
      Array_done(&databaseIds);

      // prune entity if empty and not default entity
      if (isEmptyEntity(indexHandle,entityId))
      {
        // get entity type, date
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT entities.jobUUID, \
                                         UNIXTIMESTAMP(entities.created), \
                                         entities.type, \
                                  FROM entities \
                                  WHERE id=%lld \
                                 ",
                                 entityId
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%S %llu %u",
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

        Database_finalize(&databaseQueryHandle);

        if (error != ERROR_NONE)
        {
          String_clear(jobUUID);
          createdDateTime = 0LL;
          archiveType     = ARCHIVE_TYPE_NONE;
        }

        // purge entity
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM entities WHERE id=%lld",
                                 entityId
                                );
        if (error != ERROR_NONE)
        {
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
          return error;
        }

        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "Purged entity #%llu, %s, %llu, %u: no archives",
                    entityId,
                    String_cString(jobUUID),
                    createdDateTime,
                    archiveType
                   );
      }
    }
  }

  return error;
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
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);

  // Note: keep default entity!
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "entities",
                          "id",
                          "WHERE id!=%lld \
                          ",
                          INDEX_DEFAULT_ENTITY_DATABASE_ID
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
  {
    error = pruneEntity(indexHandle,doneFlag,deletedCounter,databaseId);
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  Array_done(&databaseIds);

  return ERROR_NONE;
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
  assert(uuidId != DATABASE_ID_NONE);

  return !Database_exists(&indexHandle->databaseHandle,
                          "entities",
                          "entities.id",
                          "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID WHERE uuids.id=%lld",
                          uuidId
                         );
}

/***********************************************************************\
* Name   : pruneUUID
* Purpose: prune empty UUID
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          uuidId         - UUID index id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneUUID(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       IndexId     uuidId
                      )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  // prune entities of uuid
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "entities",
                          "entities.id",
                          "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                           WHERE uuids.id=%lld \
                          ",
                          Index_getDatabaseId(uuidId)
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
  {
    error = pruneEntity(indexHandle,doneFlag,deletedCounter,databaseId);
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  Array_done(&databaseIds);

  // delete uuid if empty
  if (isEmptyUUID(indexHandle,Index_getDatabaseId(uuidId)))
  {
    // purge UUID
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM uuids WHERE id=%lld;",
                             Index_getDatabaseId(uuidId)
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
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);

  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "uuids",
                          "id",
                          ""
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
  {
    error = pruneUUID(indexHandle,
                      doneFlag,
                      deletedCounter,
                      INDEX_ID_UUID(databaseId)
                     );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }
  Array_done(&databaseIds);

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
                                 0,  // storageIdCount,
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
                                NULL,  // archiveType
                                &storageId,
                                NULL,  // storageName,
                                NULL,  // createdDateTime
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
                                0,  // indexIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                FALSE,  // newestOnly
                                TRUE, // fragmentsFlag
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
* Name   : indexThreadCode
* Purpose: index upgrade and cleanup thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexThreadCode(void)
{
  /***********************************************************************\
  * Name   : delayThread
  * Purpose: delay thread and check quit flag
  * Input  : sleepTime - sleep time [s]
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void delayThread(uint sleepTime);
  void delayThread(uint sleepTime)
  {
    uint n;

    n = 0;
    while (   !quitFlag
           && (n < sleepTime)
          )
    {
      Misc_udelay(10LL*US_PER_SECOND);
      n += 10;
    }
  }

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
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          databaseId;
  bool                doneFlag;
  ulong               deletedCounter;
  String              storageName;
  uint                sleepTime;

  assert(indexDatabaseFileName != NULL);

  // open index
  error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE,INDEX_PURGE_TIMEOUT);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot open index database '$s' fail: %s",
                indexDatabaseFileName,
                Error_getText(error)
               );
    return;
  }

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
        error = importIndex(&indexHandle,oldDatabaseFileName);
        if (error == ERROR_NONE)
        {
          oldDatabaseCount++;
#warning revert
//          (void)File_delete(oldDatabaseFileName,FALSE);
        }
        else
        {
          failFileName = String_appendCString(String_duplicate(oldDatabaseFileName),".fail");
#warning revert
//          (void)File_rename(oldDatabaseFileName,failFileName,NULL);
          String_delete(failFileName);
        }

        i++;
      }
    }
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
      // remove deleted storages from index
      do
      {
        error = ERROR_NONE;

        // find next storage to remove (Note: get single entry for remove to avoid long-running prepare!)
        INDEX_DOX(error,
                  &indexHandle,
        {
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle.databaseHandle,
                                   "SELECT id,name FROM storages \
                                    WHERE     state!=%u \
                                          AND deletedFlag=1 \
                                    LIMIT 0,1 \
                                   ",
                                   INDEX_STATE_UPDATE
                                  );
          if (error == ERROR_NONE)
          {
            if (!Database_getNextRow(&databaseQueryHandle,
                                     "%lld %S",
                                     &databaseId,
                                     storageName
                                    )
               )
            {
              databaseId = DATABASE_ID_NONE;
            }

            Database_finalize(&databaseQueryHandle);
          }
          else
          {
            databaseId = DATABASE_ID_NONE;
          }

          return error;
        });
        if (quitFlag)
        {
          break;
        }

        // remove from database
        if (databaseId != DATABASE_ID_NONE)
        {
          doneFlag       = FALSE;
          deletedCounter = 0LL;
          do
          {
            if (indexUseCount == 0)
            {
              // Note: do not use INDEX_DOX because of indexUseCount
              error = Index_beginTransaction(&indexHandle,WAIT_FOREVER);
              if (error == ERROR_NONE)
              {
                doneFlag = TRUE;

                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
//fprintf(stderr,"%s, %d: remove databaseId=%"PRIi64"\n",__FILE__,__LINE__,databaseId);
                  error = purge(&indexHandle,
                                &doneFlag,
                                &deletedCounter,
                                "entryFragments",
                                "storageId=%lld",
                                databaseId
                               );
                  assert(   !doneFlag
                         || !Database_exists(&indexHandle.databaseHandle,
                                             "entryFragments",
                                             "id",
                                             "WHERE storageId=%lld",
                                             databaseId
                                            )
                        );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" entryFragments done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }
                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
                  error = purge(&indexHandle,
                                &doneFlag,
                                &deletedCounter,
                                "directoryEntries",
                                "storageId=%lld",
                                databaseId
                               );
                  assert(   !doneFlag
                         || !Database_exists(&indexHandle.databaseHandle,
                                             "directoryEntries",
                                             "id",
                                             "WHERE storageId=%lld",
                                             databaseId
                                            )
                        );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" directoryEntries done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }
                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
                  error = purge(&indexHandle,
                                &doneFlag,
                                &deletedCounter,
                                "linkEntries",
                                "storageId=%lld",
                                databaseId
                               );
                  assert(   !doneFlag
                         || !Database_exists(&indexHandle.databaseHandle,
                                             "linkEntries",
                                             "id",
                                             "WHERE storageId=%lld",
                                             databaseId
                                            )
                        );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" linkEntries done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }
                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
                  error = purge(&indexHandle,
                                &doneFlag,
                                &deletedCounter,
                                "specialEntries",
                                "storageId=%lld",
                                databaseId
                               );
                  assert(   !doneFlag
                         || !Database_exists(&indexHandle.databaseHandle,
                                             "specialEntries",
                                             "id",
                                             "WHERE storageId=%lld",
                                             databaseId
                                            )
                        );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" specialEntries done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }

                // purge storage
                assert(doneFlag || (deletedCounter > 0));
                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
                  error = purge(&indexHandle,
                                &doneFlag,
                                &deletedCounter,
                                "storages",
                                "id=%lld",
                                databaseId
                               );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" storage done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }

                // prune entity
                if ((error == ERROR_NONE) && doneFlag && !quitFlag)
                {
                  error = pruneEntities(&indexHandle,
                                        &doneFlag,
                                        &deletedCounter
                                       );
//fprintf(stderr,"%s, %d: databaseId=%"PRIi64" pruneEntities done=%d deletedCounter=%lu error=%s\n",__FILE__,__LINE__,databaseId,doneFlag,deletedCounter,Error_getText(error));
                }

                (void)Index_endTransaction(&indexHandle);
              }

              if ((error == ERROR_NONE) && doneFlag && !quitFlag)
              {
                // done
                if (!String_isEmpty(storageName))
                {
                  plogMessage(NULL,  // logHandle
                              LOG_TYPE_INDEX,
                              "INDEX",
                              "Removed deleted storage #%llu from index: '%s'",
                              databaseId,
                              String_cString(storageName)
                             );
                }
              }
              else
              {
                // sleep a short time
                delayThread(5LL);
              }
            }
            else
            {
              // sleep a short time
              delayThread(5LL);
            }
          }
          while ((error == ERROR_NONE) && !doneFlag && !quitFlag);
        }
        if ((indexUseCount > 0) || quitFlag)
        {
          break;
        }
      }
      while (databaseId != DATABASE_ID_NONE);
    #endif /* INDEX_SUPPORT_DELETE */

    // sleep and check quit flag/trigger
    sleepTime = 0;
    SEMAPHORE_LOCKED_DO(&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !quitFlag
             && (sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD)
             && !Semaphore_waitModified(&indexThreadTrigger,5*MS_PER_SECOND)
        )
      {
        sleepTime += 5;
      }
    }
  }
  String_delete(storageName);

  // free resources
  closeIndex(&indexHandle);
}

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
* Name   : getIndexTypeSetString
* Purpose: get index type filter string
* Input  : string       - string variable
*          indexTypeSet - index type set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexTypeSetString(String string, IndexTypeSet indexTypeSet)
{
  IndexTypes indexType;

  String_clear(string);
  for (indexType = INDEX_TYPE_MIN; indexType <= INDEX_TYPE_MAX; indexType++)
  {
    if (IN_SET(indexTypeSet,indexType))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_appendFormat(string,"%d",indexType);
    }
  }

  return string;
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
      if (!String_isEmpty(string)) String_appendCString(string,",");
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
      if (!String_isEmpty(string)) String_appendCString(string,",");
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
                         "WHERE entryId=%llu",
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
                              WHERE storageId=%llu \
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
                                WHERE storageId=%llu \
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

/***********************************************************************\
* Name   : updateEntityAggregates
* Purpose: update entity count/size
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
  DatabaseQueryHandle databaseQueryHandle;
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

  INDEX_DOX(error,
            indexHandle,
  {
    // get file aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO: use entries.size?
                             "SELECT COUNT(entries.id), \
                                     TOTAL(entryFragments.size) \
                              FROM entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_FILE,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalFileCount,
                        &totalFileSize_
                       );
    assert(totalFileSize_ >= 0.0);
    totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get image aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO: use entries.size?
                             "SELECT COUNT(entries.id),\
                                     TOTAL(entryFragments.size) \
                              FROM entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_IMAGE,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalImageCount,
                        &totalImageSize_
                       );
    assert(totalImageSize_ >= 0.0);
    totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get directory aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entries.id) \
                              FROM entries \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_DIRECTORY,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalDirectoryCount
                       );
    Database_finalize(&databaseQueryHandle);

    // get link aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entries.id) \
                              FROM entries \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_LINK,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalLinkCount
                       );
    Database_finalize(&databaseQueryHandle);

    // get hardlink aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO: use entries.size?
                             "SELECT COUNT(entries.id), \
                                     TOTAL(entryFragments.size) \
                              FROM entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_HARDLINK,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalHardlinkCount,
                        &totalHardlinkSize_
                       );
    assert(totalHardlinkSize_ >= 0.0);
    totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get special aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entries.id) \
                              FROM entries \
                              WHERE     entries.type=%d \
                                    AND entries.entityId=%lld; \
                             ",
                             INDEX_TYPE_SPECIAL,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalSpecialCount
                       );
    Database_finalize(&databaseQueryHandle);

    // update entity aggregate data
#warning
fprintf(stderr,"%s, %d: aggregate entityId=%ld: %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,entityId,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
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
                              WHERE id=%lld; \
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entriesNewest.id), \
                                     TOTAL(entryFragments.size) \
                              FROM entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_FILE,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalFileCount,
                        &totalFileSize_
                       );
    assert(totalFileSize_ >= 0.0);
    totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get newest image aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entriesNewest.id), \
                                     TOTAL(entryFragments.size) \
                              FROM entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_IMAGE,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalImageCount,
                        &totalImageSize_
                       );
    assert(totalImageSize_ >= 0.0);
    totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get newest directory aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entriesNewest.id) \
                              FROM entriesNewest \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_DIRECTORY,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalDirectoryCount
                       );
    Database_finalize(&databaseQueryHandle);

    // get newest link aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entriesNewest.id) \
                              FROM entriesNewest \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_LINK,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalLinkCount
                       );
    Database_finalize(&databaseQueryHandle);

    // get newest hardlink aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO: use entriesNewest.size?
                             "SELECT COUNT(entriesNewest.id), \
                                     TOTAL(entryFragments.size) \
                              FROM entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_HARDLINK,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu %lf",
                        &totalHardlinkCount,
                        &totalHardlinkSize_
                       );
    assert(totalHardlinkSize_ >= 0.0);
    totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
    Database_finalize(&databaseQueryHandle);

    // get newest special aggregate data
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(entriesNewest.id) \
                              FROM entriesNewest \
                              WHERE     entriesNewest.type=%d \
                                    AND entriesNewest.entityId=%lld; \
                             ",
                             INDEX_TYPE_SPECIAL,
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Database_getNextRow(&databaseQueryHandle,
                        "%lu",
                        &totalSpecialCount
                       );
    Database_finalize(&databaseQueryHandle);

    // update newest aggregate data
fprintf(stderr,"%s, %d: aggregate newest entityId=%ld: %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,entityId,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
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
                              WHERE id=%lld; \
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
  });

  return error;
}

/***********************************************************************\
* Name   : updateStorageAggregates
* Purpose: update storage count/size
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
  DatabaseQueryHandle databaseQueryHandle;
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
  assert(storageId != DATABASE_ID_NONE);

  // get file aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO: use entries.size?
                           "SELECT COUNT(entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_FILE,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalFileCount,
                      &totalFileSize_
                     );
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get image aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO: use entries.size?
                           "SELECT COUNT(entries.id),\
                                   TOTAL(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_IMAGE,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalImageCount,
                      &totalImageSize_
                     );
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get directory aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entries.id) \
                            FROM entries \
                              LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND directoryEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_DIRECTORY,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalDirectoryCount
                     );
  Database_finalize(&databaseQueryHandle);

  // get link aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entries.id) \
                            FROM entries \
                              LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND linkEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_LINK,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalLinkCount
                     );
  Database_finalize(&databaseQueryHandle);

  // get hardlink aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO: use entries.size?
                           "SELECT COUNT(entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_HARDLINK,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalHardlinkCount,
                      &totalHardlinkSize_
                     );
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get special aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entries.id) \
                            FROM entries \
                              LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                            WHERE     entries.type=%d \
                                  AND specialEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_SPECIAL,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalSpecialCount
                     );
  Database_finalize(&databaseQueryHandle);

  // update aggregate data
fprintf(stderr,"%s, %d: aggregate storageId=%ld: %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,storageId,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
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
                            WHERE id=%lld; \
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
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_FILE,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalFileCount,
                      &totalFileSize_
                     );
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get newest image aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_IMAGE,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalImageCount,
                      &totalImageSize_
                     );
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get newest directory aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entriesNewest.id) \
                            FROM entriesNewest \
                              LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND directoryEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_DIRECTORY,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalDirectoryCount
                     );
  Database_finalize(&databaseQueryHandle);

  // get newest link aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entriesNewest.id) \
                            FROM entriesNewest \
                              LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND linkEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_LINK,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalLinkCount
                     );
  Database_finalize(&databaseQueryHandle);

  // get newest hardlink aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO: use entriesNewest.size?
                           "SELECT COUNT(entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM entriesNewest \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND entryFragments.storageId=%lld; \
                           ",
                           INDEX_TYPE_HARDLINK,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu %lf",
                      &totalHardlinkCount,
                      &totalHardlinkSize_
                     );
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseQueryHandle);

  // get newest special aggregate data
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(entriesNewest.id) \
                            FROM entriesNewest \
                              LEFT JOIN specialEntries ON specialEntries.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.type=%d \
                                  AND specialEntries.storageId=%lld; \
                           ",
                           INDEX_TYPE_SPECIAL,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Database_getNextRow(&databaseQueryHandle,
                      "%lu",
                      &totalSpecialCount
                     );
  Database_finalize(&databaseQueryHandle);

  // update newest aggregate data
fprintf(stderr,"%s, %d: aggregate newest storageId=%ld: %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,storageId,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
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
                            WHERE id=%lld; \
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

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : assignStorageToStorage
* Purpose: assign storage entries to other storage
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toStorageId - to storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageToStorage(IndexHandle *indexHandle,
                                    DatabaseId  storageId,
                                    DatabaseId  toStorageId
                                   )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toStorageId != DATABASE_ID_NONE);

  // assign storage entries to other storage
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET storageId=%lld \
                              WHERE storageId=%lld; \
                             ",
                             toStorageId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entriesNewest \
                              SET storageId=%lld \
                              WHERE storageId=%lld; \
                             ",
                             toStorageId,
                             storageId
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

  // delete storage if empty
  error = pruneStorage(indexHandle,storageId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignStorageToEntity
* Purpose: assign storage entries to other entity
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
  Array         databaseIds;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;
  Errors        error;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toEntityId != DATABASE_ID_NONE);

  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  INDEX_DOX(error,
            indexHandle,
  {
    // get all entity ids for update aggregates
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entries",
                            "entityId",
                            "WHERE     id IN (SELECT entryId FROM entryFragments WHERE storageId=%lld) \
                                   AND entityId!=0 \
                            ",
                            storageId
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entries",
                            "entityId",
                            "WHERE     id IN (SELECT entryId FROM directoryEntries WHERE storageId=%lld) \
                                   AND entityId!=0 \
                            ",
                            storageId
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entries",
                            "entityId",
                            "WHERE     id IN (SELECT entryId FROM linkEntries WHERE storageId=%lld) \
                                   AND entityId!=0 \
                            ",
                            storageId
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entries",
                            "entityId",
                            "WHERE     id IN (SELECT entryId FROM specialEntries WHERE storageId=%lld) \
                                   AND entityId!=0 \
                            ",
                            storageId
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    Array_append(&databaseIds,&toEntityId);
//ARRAY_ITERATE(&databaseIds,arrayIterator,databaseId) { fprintf(stderr,"%s, %d: assign entity id %ld\n",__FILE__,__LINE__,databaseId); }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    // assign storage to new entity
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE storages \
                              SET entityId=%lld \
                              WHERE id=%lld; \
                             ",
                             toEntityId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // assign entries to new entity
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET entityId=%lld \
                              WHERE id IN (SELECT entryId FROM entryFragments WHERE storageId=%lld); \
                             ",
                             toEntityId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET entityId=%lld \
                              WHERE id IN (SELECT entryId FROM directoryEntries WHERE storageId=%lld); \
                             ",
                             toEntityId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET entityId=%lld \
                              WHERE id IN (SELECT entryId FROM linkEntries WHERE storageId=%lld); \
                             ",
                             toEntityId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entries \
                              SET entityId=%lld \
                              WHERE id IN (SELECT entryId FROM specialEntries WHERE storageId=%lld); \
                             ",
                             toEntityId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update entities aggregates
    ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
    {
fprintf(stderr,"%s, %d: dp updateEntityAggregates %ld\n",__FILE__,__LINE__,databaseId);
      error = updateEntityAggregates(indexHandle,databaseId);
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  Array_done(&databaseIds);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : assignEntityToStorage
* Purpose: assign all storage entries of entity to other storage
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toStorageId - to storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToStorage(IndexHandle *indexHandle,
                                   DatabaseId  entityId,
                                   DatabaseId  toStorageId
                                  )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(entityId != DATABASE_ID_NONE);
  assert(toStorageId != DATABASE_ID_NONE);

  // assign storage entries to other storage
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "storages",
                            "id",
                            "WHERE entityId=%lld \
                            ",
                            entityId
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
    {
      error = assignStorageToStorage(indexHandle,databaseId,toStorageId);
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });
  Array_done(&databaseIds);

  return error;
}

/***********************************************************************\
* Name   : assignEntityToEntity
* Purpose: assign all storage entries of entity to other entity
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
  Errors error;

  assert(indexHandle != NULL);

  // assign to entity
  if (entityId != toEntityId)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE storages \
                               SET entityId=%lld \
                               WHERE entityId=%lld; \
                              ",
                              toEntityId,
                              entityId
                             );
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete entity if empty and not default entity
    error = pruneEntity(indexHandle,
                        NULL,  // doneFlag
                        NULL,  // deletedCounter
                        entityId
                       );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE entities \
                               SET type=%d \
                               WHERE id=%lld; \
                              ",
                              toArchiveType,
                              toEntityId
                             );
    });
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: error=%s\n",__FILE__,__LINE__,Error_getText(error));
      return error;
    }
  }

  return ERROR_NONE;
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
  Errors error;

  assert(indexHandle != NULL);
  assert(entityId != DATABASE_ID_NONE);
  assert(toJobUUID != NULL);

  // assign to job
  if (toJobUUID != NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE entities \
                               SET jobUUID=%'S \
                               WHERE id=%lld; \
                              ",
                              toJobUUID,
                              entityId
                             );
    });
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE entities \
                               SET type=%d \
                               WHERE id=%lld; \
                              ",
                              toArchiveType,
                              entityId
                             );
    });
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : assignJobToStorage
* Purpose: assign all storage entries of all entities of job to other
*          storage
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          toStorageId - to storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToStorage(IndexHandle *indexHandle,
                                ConstString jobUUID,
                                DatabaseId  toStorageId
                               )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(toStorageId != DATABASE_ID_NONE);

  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entities",
                            "id",
                            "WHERE entities.jobUUID=%'S \
                            ",
                            jobUUID
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
    {
      error = assignEntityToStorage(indexHandle,databaseId,toStorageId);
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });
  Array_done(&databaseIds);

  return error;
}

/***********************************************************************\
* Name   : assignJobToEntity
* Purpose: assign all entities of job to other entity
* Input  : indexHandle   - index handle
*          jobUUID       - job UUID
*          toEntityId    - to entity database id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToEntity(IndexHandle  *indexHandle,
                               ConstString  jobUUID,
                               DatabaseId   toEntityId,
                               ArchiveTypes toArchiveType
                              )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(toEntityId != DATABASE_ID_NONE);

  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_getIds(&indexHandle->databaseHandle,
                            &databaseIds,
                            "entities",
                            "id",
                            "WHERE entities.jobUUID=%'S \
                            ",
                            jobUUID
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE)
    {
      error = assignEntityToEntity(indexHandle,databaseId,toEntityId,toArchiveType);
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });
  Array_done(&databaseIds);

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
  Errors error;

  assert(indexHandle != NULL);
  assert(!String_isEmpty(toJobUUID));

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities \
                              SET jobUUID=%'S \
                              WHERE jobUUID=%'S; \
                             ",
                             toJobUUID,
                             jobUUID
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (toArchiveType != ARCHIVE_TYPE_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entities \
                                SET type=%d \
                                WHERE jobUUID=%'S; \
                               ",
                               toArchiveType,
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    return ERROR_NONE;
  });

  return error;
}

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  Errors error;

  // init variables
  Semaphore_init(&indexBusyLock,SEMAPHORE_TYPE_BINARY);
  indexBusyThreadId = THREAD_ID_NONE;
  Semaphore_init(&indexLock,SEMAPHORE_TYPE_BINARY);
  Semaphore_init(&indexPauseLock,SEMAPHORE_TYPE_BINARY);
  Semaphore_init(&indexThreadTrigger,SEMAPHORE_TYPE_BINARY);

  // init database
  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    Semaphore_done(&indexThreadTrigger);
    Semaphore_done(&indexPauseLock);
    Semaphore_done(&indexLock);
    Semaphore_done(&indexBusyLock);
    return error;
  }

  return ERROR_NONE;
}

void Index_doneAll(void)
{
  Database_doneAll();

  Semaphore_done(&indexThreadTrigger);
  Semaphore_done(&indexPauseLock);
  Semaphore_done(&indexLock);
  Semaphore_done(&indexBusyLock);
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

bool Index_parseEntrySortMode(const char *name, IndexEntrySortModes *indexEntrySortMode)
{
  uint i;

  assert(name != NULL);
  assert(indexEntrySortMode != NULL);

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

Errors Index_init(const char *fileName)
{
  bool        createFlag;
  Errors      error;
  int64       indexVersion;
  String      oldDatabaseFileName;
  uint        n;
  IndexHandle indexHandleReference,indexHandle;

  assert(fileName != NULL);

  // init variables
  quitFlag = FALSE;

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

        // corrupt -> create new
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
    // create new index
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
    // get index version
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
    (void)cleanUpDuplicateMeta(&indexHandle);
    (void)cleanUpIncompleteUpdate(&indexHandle);
    (void)cleanUpIncompleteCreate(&indexHandle);
    (void)cleanUpStorageNoName(&indexHandle);
    (void)cleanUpStorageNoEntity(&indexHandle);
    (void)pruneStorages(&indexHandle);
    (void)pruneEntities(&indexHandle,NULL,NULL);
    (void)pruneUUIDs(&indexHandle,NULL,NULL);
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
  ATOMIC_INCREMENT(indexUseCount);
}

void Index_endInUse(void)
{
  ATOMIC_DECREMENT(indexUseCount);
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
                        INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_FOREIGN_KEYS,
                        timeout
                       );
    #else /* not NDEBUG */
      error = __openIndex(__fileName__,
                          __lineNb__,
                          indexHandle,
                          indexDatabaseFileName,
                          masterIO,
                          INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_FOREIGN_KEYS,
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
                        uint          indexIdCount,
                        IndexTypes    indexType
                       )
{
  uint i;

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
                    uint64       *lastExecutedDateTime,
                    String       lastErrorMessage,
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
  DatabaseQueryHandle databaseQueryHandle;
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
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       (SELECT UNIXTIMESTAMP(storages.created) FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 0,1), \
                                       (SELECT storages.errorMessage FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 0,1), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%d), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%d), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%d), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%d), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
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
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
        return error;
      }
//#warning
//#ifndef NDEBUG
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
//#endif

      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lld %lld %S %lu %lu %lu %lu %lu %llu %llu %llu %llu %llu %lu %lu %llu %lu %llu",
                               &uuidDatabaseId,
                               lastExecutedDateTime,
                               lastErrorMessage,
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
        Database_finalize(&databaseQueryHandle);
        return ERROR_DATABASE_INDEX_NOT_FOUND;
      }
      assert(totalStorageSize_ >= 0.0);
      assert(totalEntryCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalStorageSize != NULL) (*totalStorageSize) = (totalStorageSize_ >= 0.0) ? (uint64)totalStorageSize_ : 0LL;
      if (totalEntryCount  != NULL) (*totalEntryCount ) = (totalEntryCount_  >= 0.0) ? (ulong)totalEntryCount_   : 0L;
      if (totalEntrySize   != NULL) (*totalEntrySize  ) = (totalEntrySize_   >= 0.0) ? (uint64)totalEntrySize_   : 0LL;

      Database_finalize(&databaseQueryHandle);

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
                                        if (lastExecutedDateTime        != NULL) StringMap_getUInt64(resultMap,"lastExecutedDateTime",       lastExecutedDateTime,       0LL );
                                        if (lastErrorMessage            != NULL) StringMap_getString(resultMap,"lastErrorMessage",           lastErrorMessage,           NULL);
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
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  filterString = String_newCString("1");

  // get filters
  filterAppend(filterString,!INDEX_ID_IS_NONE(findEntityId),"AND","entities.id=%lld",Index_getDatabaseId(findEntityId));
  filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","entities.jobUUID=%'S",findJobUUID);
  filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","entities.scheduleUUID=%'S",findScheduleUUID);
  filterAppend(filterString,!String_isEmpty(findHostName),"AND","entities.hostName=%'S",findHostName);
  filterAppend(filterString,findArchiveType != ARCHIVE_TYPE_NONE,"AND","entities.type=%u",findArchiveType);
  filterAppend(filterString,findCreatedDateTime != 0LL,"AND","entities.created=%llu",findCreatedDateTime);

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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
                              WHERE %S \
                              GROUP BY entities.id \
                              LIMIT 0,1 \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
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

    Database_finalize(&databaseQueryHandle);

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
                           uint64      *createdDateTime,
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
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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
                              WHERE     storages.id=%lld \
                                    AND storages.deletedFlag!=1 \
                              GROUP BY storages.id \
                              LIMIT 0,1 \
                             ",
                             Index_getDatabaseId(findStorageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseQueryHandle);

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %S %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 createdDateTime,
                                 size,
                                 indexState,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

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
                             uint64                 *createdDateTime,
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
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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

    storageName = String_new();
    Storage_initSpecifier(&storageSpecifier);
    foundFlag   = FALSE;
    while (   !foundFlag
           && Database_getNextRow(&databaseQueryHandle,
                                  "%lld %S %lld %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                  &uuidDatabaseId,
                                  jobUUID,
                                  &entityDatabaseId,
                                  &storageDatabaseId,
                                  scheduleUUID,
                                  storageName,
                                  createdDateTime,
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

    Database_finalize(&databaseQueryHandle);

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
                              uint64        *createdDateTime,
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
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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
                              WHERE     (storages.state IN (%S)) \
                                    AND storages.deletedFlag!=1 \
                              LIMIT 0,1 \
                             ",
                             getIndexStateSetString(indexStateSetString,findIndexStateSet)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %S %lld %lld %S %S %llu %llu %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 &storageDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 createdDateTime,
                                 size,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

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

Errors Index_getState(IndexHandle *indexHandle,
                      IndexId     storageId,
                      IndexStates *indexState,
                      uint64      *lastCheckedDateTime,
                      String      errorMessage
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT state, \
                                     UNIXTIMESTAMP(lastChecked), \
                                     errorMessage \
                              FROM storages \
                              WHERE id=%lld \
                             ",
                             Index_getDatabaseId(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (!Database_getNextRow(&databaseQueryHandle,
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

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_setState(IndexHandle *indexHandle,
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
                                    WHERE entityId=%lld; \
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
                                      WHERE entityId=%lld; \
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
                                      WHERE entityId=%lld; \
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
                                    WHERE id=%lld; \
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
                                      WHERE id=%lld; \
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
                                      WHERE id=%lld; \
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

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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

    if (!Database_getNextRow(&databaseQueryHandle,
                             "%ld",
                             &count
                            )
       )
    {
      Database_finalize(&databaseQueryHandle);
      return -1L;
    }

    Database_finalize(&databaseQueryHandle);

    return count;
  });

  return count;
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
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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
                                 ); \
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
                             "DELETE FROM history WHERE id=%lld;",
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
  DatabaseQueryHandle databaseQueryHandle;
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
    // get storage count, entry count, entry size
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT MAX(UNIXTIMESTAMP(entities.created)), \
                                     COUNT(entities.id),\
                                     TOTAL(storages.totalEntryCount), \
                                     TOTAL(storages.totalEntrySize) \
                              FROM uuids \
                                LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                                LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=1 \
                              WHERE %S \
                             ",
                             filterString
                            );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle,
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
    Database_finalize(&databaseQueryHandle);

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
  DatabaseQueryHandle databaseQueryHandle;
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
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
//TODO: use entries.size?
                               "SELECT COUNT(entries.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN uuids ON uuids.jobUUID=entries.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get image aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
//TODO: use entries.size?
                               "SELECT COUNT(entries.id),\
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get directory aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get link aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get hardlink aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
//TODO: use entries.size?
                               "SELECT COUNT(entries.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entries \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get special aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entries.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseQueryHandle);

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
                                WHERE id=%lld; \
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
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest image aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.entityId=%lld; \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest directory aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get newest link aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get newest hardlink aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
//TODO: use entriesNewest.size?
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(entryFragments.size) \
                                FROM entriesNewest \
                                  LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest special aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                WHERE     entriesNewest.type=%d \
                                      AND uuids.id=%lld; \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseQueryHandle);

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
                                WHERE id=%lld; \
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
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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
                             WHERE %S \
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
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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
                               "INSERT INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ); \
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
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,INDEX_ID_ENTITY(entityId));
    }
    Database_finalize(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    // delete UUID
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM uuids \
                              WHERE id=%lld; \
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
                              uint          indexIdCount,
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

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              ArchiveTypes     archiveType,
                              IndexStateSet    indexStateSet,
                              IndexModeSet     indexModeSet,
                              ConstString      name,
                              DatabaseOrdering ordering,
                              ulong            offset,
                              uint64           limit
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
  filterString = String_newCString("entities.id!=0");
  orderString  = String_new();

  // get FTS
  getFTSString(ftsName,name);

  // get filters
  string = String_new();
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);
  filterAppend(filterString,archiveType != ARCHIVE_TYPE_ANY,"AND","entities.type=%u",archiveType);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","EXISTS(SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // get ordering
  appendOrdering(orderString,TRUE,"entities.created",ordering);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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
                             WHERE %S \
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
//#warning
//#ifndef NDEBUG
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
//#endif

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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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
  Errors error;

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
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT OR IGNORE INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ); \
                               ",
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
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
                                   %'S, \
                                   %'S, \
                                   %'S, \
                                   %'S, \
                                   %llu, \
                                   %u, \
                                   %d \
                                  ); \
                               ",
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
fprintf(stderr,"%s, %d: new entityId=%ld\n",__FILE__,__LINE__,Index_getDatabaseId(*entityId));

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
                                    "INDEX_NEW_ENTITY jobUUID=%S scheduleUUID=%s hostName=%S archiveType=%s createdDateTime=%llu locked=%y",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime,
                                    locked
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }
fprintf(stderr,"%s, %d: new entity %"PRIi64"\n",__FILE__,__LINE__,*entityId);

  return ERROR_NONE;
}

Errors Index_updateEntity(IndexHandle  *indexHandle,
                          IndexId      entityId,
                          ConstString  jobUUID,
                          ConstString  scheduleUUID,
                          ConstString  hostName,
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
                                    created=%llu, \
                                    type=%u \
                                WHERE id=%lld; \
                               ",
                               jobUUID,
                               scheduleUUID,
                               hostName,
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
                                    "INDEX_UPDATE_ENTITY jobUUID=%S scheduleUUID=%s hostName=%S archiveType=%s createdDateTime=%llu",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
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
                             "UPDATE entities SET lockedCount=lockedCount+1 WHERE id=%lld;",
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
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE entities \
                               SET deletedFlag=1 \
                               WHERE id=%lld; \
                              ",
                              Index_getDatabaseId(entityId)
                             );
    });
    if (error == ERROR_NONE)
    {
      // trigger clean-up thread
      Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
    }
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
                              uint          indexIdCount,
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
  uint                i;
  String              filterIdsString;
  String              indexStateSetString;
  String              indexModeSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  double              totalStorageSize_,totalEntryCount_,totalEntrySize_,totalEntryContentSize_;

uint64 t0,t1;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0) || (indexIds != NULL));

t0=Misc_getTimestamp();
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
fprintf(stderr,"%s, %d: Index_getStoragesInfos ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n",__FILE__,__LINE__);
fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
//fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));

  filterIdsString     = String_new();
  indexStateSetString = String_new();
  indexModeSetString  = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
//  filterAppend(filterIdsString,!String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","entity.uuidId=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));
  filterAppend(filterString,TRUE,"AND","storages.mode IN (%S)",getIndexModeSetString(indexModeSetString,indexModeSet));
  String_delete(indexModeSetString);
  String_delete(indexStateSetString);
  String_delete(filterIdsString);

  INDEX_DOX(error,
            indexHandle,
  {
    // get storage count, storage size, entry count, entry size
t1=Misc_getTimestamp();
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO newest
                             "SELECT COUNT(storages.id),\
                                     TOTAL(storages.size), \
                                     TOTAL(storages.totalEntryCount), \
                                     TOTAL(storages.totalEntrySize) \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                              WHERE %S \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//#warning
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (Database_getNextRow(&databaseQueryHandle,
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
fprintf(stderr,"%s, %d: totalStorageCount=%lu totalStorageSize=%lf totalEntryCount_=%lf totalEntrySize_=%lf\n",__FILE__,__LINE__,*totalStorageCount,totalStorageSize_,totalEntryCount_,totalEntrySize_);
    }
    Database_finalize(&databaseQueryHandle);

    if (totalEntryContentSize != NULL)
    {
      // get entry content size
t1=Misc_getTimestamp();
      if      (!String_isEmpty(uuidIdsString))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
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
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
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
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
//TODO newest
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM storages \
                                    LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                  WHERE %S \
                                 ",
                                 filterString
                                );
      }
//#warning
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (Database_getNextRow(&databaseQueryHandle,
                              "%lf",
                              &totalEntryContentSize_
                             )
            )
      {
  //TODO: may happen?
  //      assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (uint64)totalEntryContentSize_ : 0LL;
      }
      Database_finalize(&databaseQueryHandle);
fprintf(stderr,"%s, %d: b %"PRIu64"us\n",__FILE__,__LINE__,(Misc_getTimestamp()-t1));
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
t1=Misc_getTimestamp();
fprintf(stderr,"%s, %d: %"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));

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
                              uint                  indexIdCount,
                              IndexStateSet         indexStateSet,
                              IndexModeSet          indexModeSet,
                              ConstString           hostName,
                              ConstString           userName,
                              ConstString           name,
                              IndexStorageSortModes sortMode,
                              DatabaseOrdering      ordering,
                              uint64                offset,
                              uint64                limit
                             )
{
  String ftsName;
  String filterString;
  String string;
  String orderString;
  String uuidIdsString,entityIdsString,storageIdsString;
  String filterIdsString;
  Errors error;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0) || (indexIds != NULL));

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
fprintf(stderr,"%s, %d: Index_initListStorages ------------------------------------------------------\n",__FILE__,__LINE__);
fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));

  // get filters
  filterIdsString = String_new();
  string = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,!String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!String_isEmpty(hostName),"AND","entities.hostName LIKE %S",hostName);
  filterAppend(filterString,!String_isEmpty(userName),"AND","storages.userName LIKE %S",userName);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  // get sort mode, ordering
  appendOrdering(orderString,sortMode != INDEX_STORAGE_SORT_MODE_NONE,INDEX_STORAGE_SORT_MODE_COLUMNS[sortMode],ordering);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
//TODO newest
                            "SELECT IFNULL(uuids.id,0), \
                                    entities.jobUUID, \
                                    IFNULL(entities.id,0), \
                                    entities.scheduleUUID, \
                                    entities.hostName, \
                                    storages.userName, \
                                    storages.comment, \
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
                             WHERE     %S \
                                   AND storages.deletedFlag!=1 \
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
#warning
#ifndef NDEBUG
Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
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
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *createdDateTime,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %lld %S %S %S %S %u %lld %S %llu %llu %u %u %llu %S %lu %llu",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           comment,
                           archiveType,
                           &storageDatabaseId,
                           storageName,
                           createdDateTime,
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
                        IndexId     entityId,
                        ConstString hostName,
                        ConstString userName,
                        ConstString storageName,
                        uint64      createdDateTime,
                        uint64      size,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       )
{
  Errors error;

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
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO storages \
                                  ( \
                                   entityId, \
                                   name, \
                                   created, \
                                   size, \
                                   state, \
                                   mode, \
                                   lastChecked\
                                  ) \
                                VALUES \
                                  ( \
                                   %d, \
                                   %'S, \
                                   DATETIME(%llu,'unixepoch'), \
                                   %llu, \
                                   %d, \
                                   %d, \
                                   DATETIME('now') \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
//TODO
//                               hostName,
                               storageName,
                               createdDateTime,
                               size,
                               indexState,
                               indexMode
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*storageId) = INDEX_ID_STORAGE(Database_getLastRowId(&indexHandle->databaseHandle));

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

#if 0
                                      if (StringMap_getInt64(resultMap,"storageId",storageId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
#else
return ERROR_UNKNOWN;
#endif
                                    },NULL),
                                    "INDEX_NEW_STORAGE entityId=%lld hostName=%'S userName=%'S storageName=%'S createdDateTime=%llu size=%llu indexState=%s indexMode=%s",
                                    entityId,
//TODO: remove hostName, userName -> entity
                                    hostName,
                                    userName,
                                    storageName,
                                    createdDateTime,
                                    size,
                                    Index_stateToString(indexState,NULL),
                                    Index_modeToString(indexMode,NULL)
                                   );
  }

  return error;
}

Errors Index_updateStorage(IndexHandle  *indexHandle,
                           IndexId      storageId,
                           ConstString  userName,
                           ConstString  storageName,
                           uint64       createdDateTime,
                           uint64       size,
                           ConstString  comment
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

  INDEX_DOX(error,
            indexHandle,
  {
    if (userName != NULL)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storages \
                                  SET userName=%'S \
                                  WHERE id=%lld; \
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
                                  WHERE id=%lld; \
                               ",
                               storageName,
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
                                SET created=DATETIME(%llu,'unixepoch'), \
                                    size=%llu \
                                WHERE id=%lld; \
                             ",
                             createdDateTime,
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
                                  WHERE id=%lld; \
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

  return error;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
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

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              "UPDATE storages \
                               SET deletedFlag=1 \
                               WHERE id=%lld; \
                              ",
                              Index_getDatabaseId(storageId)
                             );
    });
    if (error == ERROR_NONE)
    {
      // trigger clean-up thread
      Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
    }
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
    emptyFlag = isEmptyStorage(indexHandle,
                               Index_getDatabaseId(storageId)
                              );
  }
  else
  {
    // slave mode: always empty
    emptyFlag = TRUE;
  }

  return emptyFlag;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  Array         entryIds;
  Errors        error;
  bool          doneFlag;
  #ifndef NDEBUG
    ulong deletedCounter;
  #endif
  ArrayIterator arrayIterator;
  DatabaseId    entryId;
fprintf(stderr,"%s, %d: Index_clearStorage storageId=%lld\n",__FILE__,__LINE__,storageId);

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  Array_init(&entryIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  INDEX_DOX(error,
            indexHandle,
  {
    do
    {
      doneFlag = TRUE;

      error = Index_beginTransaction(indexHandle,WAIT_FOREVER);
      if (error == ERROR_NONE)
      {
        #ifndef NDEBUG
          deletedCounter = 0;
        #endif

        // get file/image/hardlink entry ids to check for purge
        Array_clear(&entryIds);
        if ((error == ERROR_NONE) && doneFlag)
        {
          error = Database_getIds(&indexHandle->databaseHandle,
                                  &entryIds,
                                  "entryFragments",
                                  "entryId",
                                  "WHERE storageId=%lld",
                                  Index_getDatabaseId(storageId)
                                 );
        }

        // purge file/image/hardlink entry fragments
        if ((error == ERROR_NONE) && doneFlag)
        {
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entryFragments",
                        "storageId=%lld",
                        Index_getDatabaseId(storageId)
                       );
        }

        // purge file/image/hardlink entries
        if ((error == ERROR_NONE) && doneFlag)
        {
          ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
          {
            error = purge(indexHandle,
                          &doneFlag,
                          #ifndef NDEBUG
                            &deletedCounter,
                          #else
                            NULL,  // deletedCounter
                          #endif
                          "entries",
                          "    id=%lld \
                           AND (SELECT count(id) FROM entryFragments WHERE entryId=%lld)=0 \
                          ",
                          entryId,
                          entryId
                         );
          }
        }
//fprintf(stderr,"%s, %d: 5 file/image/hardlink done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));

        // purge directory entries
        if ((error == ERROR_NONE) && doneFlag)
        {
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (SELECT entryId FROM directoryEntries WHERE storageId=%lld)",
                        Index_getDatabaseId(storageId)
                       );
        }
//fprintf(stderr,"%s, %d: 7 directory done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));

        // purge link entries
        if ((error == ERROR_NONE) && doneFlag)
        {
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (SELECT entryId FROM linkEntries WHERE storageId=%lld)",
                        Index_getDatabaseId(storageId)
                       );
        }
//fprintf(stderr,"%s, %d: 8 link done=%d deletedCounter=%lu\n",__FILE__,__LINE__,doneFlag,deletedCounter);

        // purge special entries
        if ((error == ERROR_NONE) && doneFlag)
        {
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (SELECT entryId FROM specialEntries WHERE storageId=%lld)",
                        Index_getDatabaseId(storageId)
                       );
        }
//fprintf(stderr,"%s, %d: 10 special done=%d deletedCounter=%lu\n",__FILE__,__LINE__,doneFlag,deletedCounter);

        error = Index_endTransaction(indexHandle);
      }
    }
    while ((error == ERROR_NONE) && !doneFlag);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update aggregates
    error = updateStorageAggregates(indexHandle,Index_getDatabaseId(storageId));
    if (error != ERROR_NONE)
    {
      return error;
    }
    //TODO: entities

    return ERROR_NONE;
  });
  Array_done(&entryIds);

  return error;
}

Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       scheduleUUID,
                        ArchiveTypes archiveType,
                        String       storageName,
                        uint64       *createdDateTime,
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
  DatabaseQueryHandle databaseQueryHandle;
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
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
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
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      return error;
    }
    if (!Database_getNextRow(&databaseQueryHandle,
                             "%llu %S %llu %S %u %S %llu %llu %u %u %llu %S %llu %llu",
                             &uuidDatabaseId,
                             jobUUID,
                             &entityDatabaseId,
                             scheduleUUID,
                             archiveType,
                             storageName,
                             createdDateTime,
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
      Database_finalize(&databaseQueryHandle);
      return ERROR_DATABASE_INDEX_NOT_FOUND;
    }
    if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_(INDEX_TYPE_UUID,  uuidDatabaseId  );
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityDatabaseId);
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_storageUpdate(IndexHandle *indexHandle,
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      storageSize
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

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // update name
      if (storageName != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storages \
                                  SET name=%'S \
                                  WHERE id=%lld; \
                                 ",
                                 storageName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      // update size
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storages \
                                SET size=%llu \
                                WHERE id=%lld; \
                               ",
                               storageSize,
                               Index_getDatabaseId(storageId)
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
                                    "INDEX_STORAGE_UPDATE storageId=%lld storageName=%'S storageSize=%llu",
                                    storageId,
                                    storageName,
                                    storageSize
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            uint          indexIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            ConstString   name,
                            bool          newestOnly,
                            bool          fragmentsFlag,
                            ulong         *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              ftsName;
  String              uuidIdsString,entityIdsString;
  String              entryIdsString;
  uint                i;
  String              filterString,filterIdsString;
  String              indexTypeSetString;
  ulong               totalEntryCount_;
double totalEntryCount_x;
  double              totalEntryFragmentCount_,totalEntrySize_,totalEntryContentSize_;

uint64 t0,t1;

  assert(indexHandle != NULL);
  assert((indexIdCount == 0) || (indexIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

t0=Misc_getTimestamp();
  // init variables
  if (totalEntryCount       != NULL) (*totalEntryCount      ) = 0L;
  if (totalEntrySize        != NULL) (*totalEntrySize       ) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName            = String_new();
  uuidIdsString      = String_new();
  entityIdsString    = String_new();
  entryIdsString     = String_new();
  filterString       = String_new();
  indexTypeSetString = String_new();

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
fprintf(stderr,"%s, %d: Index_getEntriesInfo **************************************************** %d\n",__FILE__,__LINE__,newestOnly);
fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
fprintf(stderr,"%s, %d: entryIdsString=%s\n",__FILE__,__LINE__,String_cString(entryIdsString));

  // get filters
  String_setCString(filterString,"1");
  filterIdsString = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  String_delete(filterIdsString);
fprintf(stderr,"%s, %d: entryIdCount=%d filterString=%s\n",__FILE__,__LINE__,entryIdCount,String_cString(filterString));

  error = ERROR_NONE;
  if (String_isEmpty(ftsName))
  {
    // no names

    // get filters
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    INDEX_DOX(error,
              indexHandle,
    {
      // get total entry count, total fragment count, total entry size
      if (newestOnly)
      {
        if (String_isEmpty(entryIdsString))
        {
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
//TODO
                                   "SELECT TOTAL(entities.totalEntryCountNewest), \
0,\
                                           TOTAL(entities.totalEntrySizeNewest) \
                                    FROM entities \
                                      LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                    %s \
                                   ",
                                   filterString,
//TODO
""//                                   !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                  );
        }
        else
        {
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
//TODO
                                   "SELECT COUNT(entriesNewest.id), \
0,\
                                           TOTAL(entriesNewest.size) \
                                    FROM entriesNewest \
                                      LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                      LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                    %s \
                                   ",
                                   filterString,
//TODO
""//                                   !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                  );
        }
      }
      else
      {
        // storages, entries selected -> get data from entries
        if (String_isEmpty(entryIdsString))
        {
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
//TODO
                                   "SELECT TOTAL(entities.totalEntryCount), \
0,\
                                           TOTAL(entities.totalEntrySize) \
                                    FROM entities \
                                      LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                    %s \
                                   ",
                                   filterString,
//TODO
""//                                   !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                  );
        }
        else
        {
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
//TODO
                                   "SELECT COUNT(entries.id), \
0,\
                                           TOTAL(entries.size) \
                                    FROM entries \
                                      LEFT JOIN entities ON entities.id=entries.entityId \
                                      LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                    %s \
                                   ",
                                   filterString,
//TODO
""//                                   !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                  );
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
#if 1
#warning
#ifndef NDEBUG
Database_debugPrintQueryInfo(&databaseQueryHandle);
#endif
#endif
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lf %lf %lf",
                               &totalEntryCount_x,
                               &totalEntryFragmentCount_,
                               &totalEntrySize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_DATABASE;
      }
fprintf(stderr,"%s, %d: totalEntryCount_=%lf totalEntryFragmentCount_=%lf totalEntrySize_=%lf\n",__FILE__,__LINE__,totalEntryCount_x,totalEntryFragmentCount_,totalEntrySize_);
      assert(totalEntryFragmentCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL)
      {
        (*totalEntryCount) += fragmentsFlag ? (uint64)totalEntryFragmentCount_ : totalEntryCount_x;
      }
      if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

#if 0
      // get entry content size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM entriesNewest \
                                    LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN entities         ON entities.id=storages.entityId \
                                    LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND entities.deletedFlag!=1 \
                                 ",
                                 filterString
                                );
      }
      else
      {
        if (String_isEmpty(entryIdsString))
        {
fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
          // no storages selected, no entries selected -> get aggregated data from entities
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
                                   "SELECT TOTAL(entities.totalEntrySize) \
                                    FROM entities \
                                      LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                   ",
                                   filterString
                                  );
        }
        else
        {
          // entries selected -> get aggregated data from entries
          error = Database_prepare(&databaseQueryHandle,
                                   &indexHandle->databaseHandle,
                                   "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                    FROM entries \
                                      LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                      LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                      LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                      LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                      LEFT JOIN entities         ON entities.id=storages.entityId \
                                      LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                    WHERE     %S \
                                          AND entities.deletedFlag!=1 \
                                   ",
                                   filterString
                                  );
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//#warning
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lf",
                               &totalEntryContentSize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_DATABASE;
      }
//TODO: may happend?
//      assert(totalEntryContentSize_ >= 0.0);
      if (totalEntryContentSize != NULL) (*totalEntryContentSize) += (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
      Database_finalize(&databaseQueryHandle);
#endif

      return ERROR_NONE;
    });
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names selected

    // get filters
    filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH '%S'",ftsName);
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    INDEX_DOX(error,
              indexHandle,
    {
      // get entry count, entry size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entriesNewest.id), \
0,\
                                         TOTAL(entriesNewest.size) \
                                  FROM FTS_entries \
                                    LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                    LEFT JOIN entities      ON entities.id=entriesNewest.entityId \
                                    LEFT JOIN uuids         ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     entriesNewest.id IS NOT NULL \
                                        AND %S \
                                        AND entities.deletedFlag!=1 \
                                  %s \
                                 ",
                                 filterString,
//TODO
""//                                 !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entries.id), \
0,\
                                         TOTAL(entries.size) \
                                  FROM FTS_entries \
                                    LEFT JOIN entries  ON entries.id=FTS_entries.entryId \
                                    LEFT JOIN entities ON entities.id=entries.entityId \
                                    LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND entities.deletedFlag!=1 \
                                  %s \
                                 ",
                                 filterString,
//TODO
""//                                 !fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : ""
                                );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//#warning
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lu %lf %lf",
                               &totalEntryCount_,
                               &totalEntryFragmentCount_,
                               &totalEntrySize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_DATABASE;
      }
      assert(totalEntryFragmentCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL)
      {
        (*totalEntryCount) += fragmentsFlag ? (ulong)totalEntryFragmentCount_ : totalEntryCount_;
      }
      if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get entry content size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM FTS_entries \
                                    LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN entries          ON entries.id=entriesNewest.entryId \
                                    LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                    LEFT JOIN entities         ON entities.id=storages.entityId \
                                    LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storages.deletedFlag!=1 \
                                 ",
                                 filterString
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM FTS_entries \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=FTS_entries.entryId \
                                    LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                    LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                    LEFT JOIN entities         ON entities.id=storages.entityId \
                                    LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storages.deletedFlag!=1 \
                                 ",
                                 filterString
                                );
      }
      if (error != ERROR_NONE)
      {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
        return error;
      }
//#warning
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lf",
                               &totalEntryContentSize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
        return ERROR_DATABASE;
      }
//TODO: may happend?
//      assert(totalEntryContentSize_ >= 0.0);
      if (totalEntryContentSize != NULL) (*totalEntryContentSize) += (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
      Database_finalize(&databaseQueryHandle);

      return ERROR_NONE;
    });
  }
t1=Misc_getTimestamp();
fprintf(stderr,"%s, %d: %"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));

  // free resources
  String_delete(indexTypeSetString);
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
                             uint                indexIdCount,
                             const IndexId       entryIds[],
                             uint                entryIdCount,
                             IndexTypeSet        indexTypeSet,
                             ConstString         name,
                             bool                newestOnly,
                             bool                fragmentsFlag,
                             IndexEntrySortModes sortMode,
                             DatabaseOrdering    ordering,
                             uint64              offset,
                             uint64              limit
                            )
{
  String              ftsName;
  String              uuidIdsString,entityIdsString;
  String              entryIdsString;
//  DatabaseQueryHandle databaseQueryHandle;
//  DatabaseId          storageId;
  uint                i;
  String              filterString,filterIdsString;
  String              orderString;
  String              string;
  Errors              error;

uint64 t0,t1;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((indexIdCount == 0) || (indexIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

t0=Misc_getTimestamp();
  // init variables
  ftsName          = String_new();
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  entryIdsString   = String_new();
  filterString     = String_new();
  orderString      = String_new();
  string           = String_new();

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
fprintf(stderr,"%s, %d: Index_initListEntries ------------------------------------------------------\n",__FILE__,__LINE__);
fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));

  // get filters
  String_setCString(filterString,"1");
  filterIdsString = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  String_delete(filterIdsString);
  if (newestOnly)
  {
    filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(string,indexTypeSet));
  }
  else
  {
    filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(string,indexTypeSet));
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
        return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                                &indexHandle->databaseHandle,
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
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySizeNewest, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM uuids \
                                   LEFT JOIN entities         ON entities.jobUUID=uuids.jobUUID \
                                   LEFT JOIN entriesNewest    ON entriesNewest.entityId=entities.id \
                                   LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                 WHERE     entriesNewest.id IS NOT NULL \
                                       AND %S \
                                 %s \
                                 %S \
                                 LIMIT %llu,%llu; \
                                ",
                                !fragmentsFlag ? "1" : "COUNT(entriesNewest.id)",
                                filterString,
                                fragmentsFlag ? "GROUP BY entities.id,entriesNewest.type,entriesNewest.name" : "",
                                orderString,
                                offset,
                                limit
                               );
      }
      else
      {
        return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                                &indexHandle->databaseHandle,
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
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySize, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM uuids \
                                   LEFT JOIN entities         ON uuids.jobUUID=uuids.jobUUID \
                                   LEFT JOIN entries          ON entries.entityId=entities.id \
                                   LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                 WHERE %S \
                                 %s \
                                 %S \
                                 LIMIT %llu,%llu; \
                                ",
//TODO: avoid count() to make it faster?
                                fragmentsFlag ? "COUNT(entries.id)" : "1",
                                filterString,
                                fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : "",
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
        return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                                &indexHandle->databaseHandle,
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
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySizeNewest, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM FTS_entries \
                                   LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                   LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                   LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                 WHERE     entriesNewest.id IS NOT NULL \
                                       AND %S \
                                 %s \
                                 %S \
                                 LIMIT %llu,%llu; \
                                ",
                                !fragmentsFlag ? "1" : "COUNT(entriesNewest.id)",
                                filterString,
                                fragmentsFlag ? "GROUP BY entities.id,entriesNewest.type,entriesNewest.name" : "",
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
        return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                                &indexHandle->databaseHandle,
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
                                        fileEntries.size, \
                                        imageEntries.size, \
                                        imageEntries.fileSystemType, \
                                        imageEntries.blockSize, \
                                        directoryEntries.totalEntrySize, \
                                        linkEntries.destinationName, \
                                        hardlinkEntries.size \
                                 FROM FTS_entries \
                                   LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                   LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                   LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                   LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                   LEFT JOIN entities         ON entities.id=entries.entityId \
                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                 WHERE %S \
                                 %s \
                                 %S \
                                 LIMIT %llu,%llu; \
                                ",
                                !fragmentsFlag ? "1" : "COUNT(entries.id)",
                                filterString,
                                fragmentsFlag ? "GROUP BY entities.id,entries.type,entries.name" : "",
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
    String_delete(string);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsName);
    return error;
  }
#warning
#ifndef NDEBUG
Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
#endif
t1=Misc_getTimestamp();
fprintf(stderr,"%s, %d: %"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));

  // free resources
  String_delete(string);
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

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %S %S %u %llu %u %S %llu %u %u %u %llu %u %llu %llu %u %u %llu %S %llu",
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
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;

  return TRUE;
}

Errors Index_initListEntryFragments(IndexQueryHandle    *indexQueryHandle,
                                    IndexHandle         *indexHandle,
                                    IndexId             entryId,
                                    IndexEntrySortModes sortMode,
                                    DatabaseOrdering    ordering,
                                    uint64              offset,
                                    uint64              limit
                                   )
{
  String              entryName;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          entityDatabaseId;
  IndexTypes          indexType;
  String              orderString;
  String              string;
  Errors              error;

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
  entryName    = String_new();
  orderString  = String_new();
  string       = String_new();

  // get entity id, entry type and name
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT entities.id, \
                                     entries.type, \
                                     entries.name \
                              FROM entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN storages       ON storages.id=entryFragments.storageId \
                                LEFT JOIN entities       ON entities.id=storages.entityId \
                              WHERE     entries.id=%lld \
                                    AND storages.deletedFlag!=1 \
                             ",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (!Database_getNextRow(&databaseQueryHandle,
                             "%llu %u %S",
                             &entityDatabaseId,
                             &indexType,
                             entryName
                            )
       )
    {
      entityDatabaseId = DATABASE_ID_NONE;
      indexType        = INDEX_TYPE_NONE;
      String_clear(entryName);
    }
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(orderString);
    String_delete(entryName);
    return error;
  }

  // get sort mode, ordering
  appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_SORT_MODE_COLUMNS[sortMode],ordering);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
                            "SELECT uuids.id, \
                                    uuids.jobUUID, \
                                    entities.id, \
                                    entities.scheduleUUID, \
                                    entities.hostName, \
                                    storages.userName, \
                                    entities.type, \
                                    storages.id, \
                                    storages.name, \
                                    UNIXTIMESTAMP(storages.created), \
                                    entries.id, \
                                    entries.type, \
                                    entries.name, \
                                    entries.timeLastChanged, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission, \
                                    entries.size, \
                                    fileEntries.size, \
                                    imageEntries.size, \
                                    imageEntries.fileSystemType, \
                                    imageEntries.blockSize, \
                                    directoryEntries.totalEntrySize, \
                                    linkEntries.destinationName, \
                                    hardlinkEntries.size \
                             FROM entries \
                               LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                               LEFT JOIN storages         ON storages.id=entryFragments.storageId \
                               LEFT JOIN entities         ON entities.id=storages.entityId \
                               LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                               LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                               LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                               LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                               LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                               LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                             WHERE     entities.id=%lld \
                                   AND entries.type=%u \
                                   AND entries.name=%'S \
                             %S \
                             LIMIT %llu,%llu; \
                            ",
                            entityDatabaseId,
                            indexType,
                            entryName,
                            orderString,
                            offset,
                            limit
                           );
  });
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(string);
    String_delete(orderString);
    String_delete(entryName);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

  // free resources
  String_delete(string);
  String_delete(orderString);
  String_delete(entryName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntryFragment(IndexQueryHandle  *indexQueryHandle,
                                IndexId           *uuidId,
                                String            jobUUID,
                                IndexId           *entityId,
                                String            scheduleUUID,
                                String            userName,
                                String            hostName,
                                ArchiveTypes      *archiveType,
                                IndexId           *storageId,
                                String            storageName,
                                uint64            *storageDateTime,
                                IndexId           *entryId,
                                String            entryName,
                                String            destinationName,
                                FileSystemTypes   *fileSystemType,
                                uint64            *size,
//TODO: use timeLastChanged
                                uint64            *timeModified,
                                uint32            *userId,
                                uint32            *groupId,
                                uint32            *permission,
                                uint64            *fragmentOffset,
                                uint64            *fragmentSize
                               )
{
  IndexTypes indexType;
  DatabaseId uuidDatabaseId,entityDatabaseId,entryDatabaseId,storageDatabaseId;
  int64      fragmentOffset_,fragmentSize_;
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %S %S %u %llu %S %llu %llu %u %S %llu %u %u %u %llu %llu %llu %llu %llu %u %u %llu %llu %llu %S %llu",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           archiveType,
                           &storageDatabaseId,
                           storageName,
                           storageDateTime,
                           &entryDatabaseId,
                           &indexType,
                           entryName,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           size,
                           NULL,  // fileSize,
                           &fragmentOffset_,
                           &fragmentSize_,
                           NULL,  // imageSize,
                           &fileSystemType_,
                           &blockSize_,
                           &blockOffset_,
                           &blockCount_,
                           NULL,  // &directorySize,
                           destinationName,
                           NULL  // hardlinkSize
                          )
     )
  {
    return FALSE;
  }
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  assert(fileSystemType_ >= 0);
  assert(blockSize_ >= 0);
  assert(blockOffset_ >= 0LL);
  assert(blockCount_ >= 0LL);
//TODO: may happen
  if (uuidId         != NULL) (*uuidId        ) = INDEX_ID_(INDEX_TYPE_UUID,   uuidDatabaseId   );
  if (entityId       != NULL) (*entityId      ) = INDEX_ID_(INDEX_TYPE_ENTITY, entityDatabaseId );
  if (storageId      != NULL) (*storageId     ) = INDEX_ID_(INDEX_TYPE_STORAGE,storageDatabaseId);
  if (entryId        != NULL) (*entryId       ) = INDEX_ID_(indexType,         entryDatabaseId  );
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;
  if (fragmentOffset != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentOffset) = (uint64)fragmentOffset_;                 break;
      case INDEX_TYPE_IMAGE: (*fragmentOffset) = (uint64)blockOffset_*(uint64)blockSize_; break;
      default:               (*fragmentOffset) = 0LL;                                     break;
    }
  }
  if (fragmentSize != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentSize) = (uint64)fragmentSize_;                  break;
      case INDEX_TYPE_IMAGE: (*fragmentSize) = (uint64)blockCount_*(uint64)blockSize_; break;
      default:               (*fragmentSize) = 0LL;                                    break;
    }
  }

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
                                 "DELETE FROM fileEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM imageEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM directoryEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_LINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM linkEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM specialEntries WHERE entryId=%lld;",
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
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
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
                             "DELETE FROM entries WHERE id=%lld;",
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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_FILE);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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
                             WHERE     %S \
                                   AND entities.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteFile(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_FILE);

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
                             "DELETE FROM fileEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
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

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    imageEntries.fileSystemType, \
                                    imageEntries.blockSize, \
                                    entries.size \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     %S \
                                   AND entities.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteImage(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_IMAGE);

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
                             "DELETE FROM imageEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageSize<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     %S \
                                   AND entities.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             IndexId     indexId
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_DIRECTORY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
            SEMAPHORE_LOCK_TYPE_READ_WRITE,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM directoryEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalDirectoryCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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
                             WHERE     %S \
                                   AND storages.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteLink(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_LINK);

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
                             "DELETE FROM linkEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalLinkCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entries.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
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
                             WHERE     %S \
                                   AND entities.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            IndexId     indexId
                           )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_HARDLINK);

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
                             "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkSize<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

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
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_prepare(&indexQueryHandle->databaseQueryHandle,
                            &indexHandle->databaseHandle,
                            "SELECT entries.id, \
                                    UNIXTIMESTAMP(entities.created), \
                                    entries.name, \
                                    entries.timeModified, \
                                    entries.userId, \
                                    entries.groupId, \
                                    entries.permission \
                             FROM entries \
                               LEFT JOIN entities ON entities.id=entries.entityId \
                             WHERE     %S \
                                   AND entities.deletedFlag!=1 \
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

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
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

#if 0
//TODO: obsolete
Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           IndexId     indexId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_SPECIAL);

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
                             "DELETE FROM specialEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalSpecialCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  Database_finalize(&indexQueryHandle->databaseQueryHandle);
  doneIndexQueryHandle(indexQueryHandle);
}

Errors Index_addFile(IndexHandle *indexHandle,
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
  DatabaseId entryId,fileEntryId;

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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_FILE,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
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
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add file entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT OR IGNORE INTO fileEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %llu \
                                  ); \
                               ",
                               entryId,
                               size
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get file entry id
      error = Database_getId(&indexHandle->databaseHandle,
                             &fileEntryId,
                             "fileEntries",
                             "id",
                             "WHERE entryId=%lld \
                             ",
                             entryId
                            );
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
                                  ); \
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
                                    "INDEX_ADD_FILE storageId=%llu name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
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
  DatabaseId entryId,imageEntryId;

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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_IMAGE,
                               name,
                               0LL,
                               0LL,
                               0LL,
                               0,
                               0,
                               0
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
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
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add image entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT OR IGNORE INTO imageEntries \
                                  ( \
                                   entryId, \
                                   fileSystemType, \
                                   size, \
                                   blockSize \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %llu, \
                                   %u \
                                  ); \
                               ",
                               entryId,
                               fileSystemType,
                               size,
                               blockSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get image entry id
      error = Database_getId(&indexHandle->databaseHandle,
                             &imageEntryId,
                             "imageEntries",
                             "id",
                             "WHERE entryId=%lld \
                             ",
                             entryId
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add image entry fragment
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
                                  ); \
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
                                    "INDEX_ADD_IMAGE storageId=%llu type=IMAGE name=%'S fileSystemType=%'s size=%llu blockSize=%lu blockOffset=%llu blockCount=%llu",
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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_DIRECTORY,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

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
                                  ); \
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
                                    "INDEX_ADD_DIRECTORY storageId=%llu type=DIRECTORY name=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_LINK,
                               linkName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

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
                                  ); \
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
                                    "INDEX_ADD_LINK storageId=%llu type=LINK name=%'S destinationName=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
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
  DatabaseId entryId,hardlinkEntryId;

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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_HARDLINK,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
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
      if (error != ERROR_NONE)
      {
        return error;
      }


      // add hard link entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT OR IGNORE INTO hardlinkEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %llu \
                                  ); \
                               ",
                               entryId,
                               size
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get hard link entry id
      error = Database_getId(&indexHandle->databaseHandle,
                             &hardlinkEntryId,
                             "hardlinkEntries",
                             "id",
                             "WHERE entryId=%lld \
                             ",
                             entryId
                            );
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
                                  ); \
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
                                    "INDEX_ADD_HARDLINK storageId=%llu type=HARDLINK name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
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
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_SPECIAL,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

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
                                   %d, \
                                   %d, \
                                   %d\
                                  ); \
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
                                    "INDEX_ADD_SPECIAL storageId=%llu type=SPECIAL name=%'S specialType=%s timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o major=%u minor=%u",
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

          // assign all storage entries of entity to other entity
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
        }
      }
      else if (!INDEX_ID_IS_NONE(toStorageId))
      {
        // assign to other storage

        if (!INDEX_ID_IS_NONE(storageId))
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage entries to other storage
          error = assignStorageToStorage(indexHandle,
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

          // assign all storage entries of entity to other storage
          error = assignEntityToStorage(indexHandle,
                                        Index_getDatabaseId(entityId),
                                        Index_getDatabaseId(toStorageId)
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all storage entries of all entities of job to other storage
          error = assignJobToStorage(indexHandle,
                                     jobUUID,
                                     Index_getDatabaseId(toStorageId)
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }
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
                       indexId
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
  assert(Index_getDatabaseId(indexId) !=  INDEX_DEFAULT_ENTITY_DATABASE_ID);

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
    return pruneStorage(indexHandle,Index_getDatabaseId(indexId));
  });

  return error;
}

Errors Index_initListSkippedEntry(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  uint             indexIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  IndexTypeSet     indexTypeSet,
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
  UNUSED_VARIABLE(indexTypeSet);
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
                             IndexTypes  type,
                             ConstString entryName
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
  assert(   (type == INDEX_TYPE_FILE           )
         || (type == INDEX_CONST_TYPE_IMAGE    )
         || (type == INDEX_CONST_TYPE_DIRECTORY)
         || (type == INDEX_CONST_TYPE_LINK     )
         || (type == INDEX_CONST_TYPE_HARDLINK )
         || (type == INDEX_CONST_TYPE_SPECIAL  )
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
                             "INSERT INTO skippedEntries \
                                ( \
                                 entityId, \
                                 type, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %d, \
                                 %'S \
                                ); \
                             ",
                             Index_getDatabaseId(entityId),
                             type,
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
                             "DELETE FROM skippedEntries WHERE id=%lld;",
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

#ifdef __cplusplus
  }
#endif

/* end of file */
