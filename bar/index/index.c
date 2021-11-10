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

#include "index/index_common.h"
#include "index/index_entities.h"
#include "index/index_entries.h"
#include "index/index_storages.h"
#include "index/index_uuids.h"

#include "index/index.h"

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
#define DEFAULT_DATABASE_NAME "bar"

const char *DATABASE_SAVE_EXTENSIONS[] =
{
  ".old%03d",
  "_old%03d"
};
const char *DATABASE_SAVE_PATTERNS[] =
{
  ".*\\.old\\d\\d\\d$",
  ".*\\_old\\d\\d\\d$"
};

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

// TODO:
#define MAX_SQL_COMMAND_LENGTH (2*4096)

const struct
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

const struct
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

const struct
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

const struct
{
  const char           *name;
  IndexEntitySortModes sortMode;
} INDEX_ENTITY_SORT_MODES[] =
{
  { "JOB_UUID",INDEX_ENTITY_SORT_MODE_JOB_UUID },
  { "CREATED", INDEX_ENTITY_SORT_MODE_CREATED  },
};

const struct
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

const struct
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

const char *INDEX_ENTITY_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTITY_SORT_MODE_NONE    ] = NULL,

  [INDEX_ENTITY_SORT_MODE_JOB_UUID] = "entities.jobUUID",
  [INDEX_ENTITY_SORT_MODE_CREATED ] = "entities.created",
};

const char *INDEX_STORAGE_SORT_MODE_COLUMNS[] =
{
  [INDEX_STORAGE_SORT_MODE_NONE    ] = NULL,

  [INDEX_STORAGE_SORT_MODE_HOSTNAME] = "entities.hostName",
  [INDEX_STORAGE_SORT_MODE_NAME    ] = "storages.name",
  [INDEX_STORAGE_SORT_MODE_SIZE    ] = "storages.totalEntrySize",
  [INDEX_STORAGE_SORT_MODE_CREATED ] = "storages.created",
  [INDEX_STORAGE_SORT_MODE_USERNAME] = "storages.userName",
  [INDEX_STORAGE_SORT_MODE_STATE   ] = "storages.state"
};

const char *INDEX_ENTRY_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE        ] = NULL,

  [INDEX_ENTRY_SORT_MODE_ARCHIVE     ] = "storages.name",
  [INDEX_ENTRY_SORT_MODE_NAME        ] = "entries.name",
  [INDEX_ENTRY_SORT_MODE_TYPE        ] = "entries.type",
  [INDEX_ENTRY_SORT_MODE_SIZE        ] = "entries.type,entries.size",
  [INDEX_ENTRY_SORT_MODE_FRAGMENT    ] = "entryFragments.offset",
  [INDEX_ENTRY_SORT_MODE_LAST_CHANGED] = "entries.timeLastChanged"
};
const char *INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[] =
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
//const uint SINGLE_STEP_PURGE_LIMIT = 4096;

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define IMPORT_INDEX_LOG_FILENAME "import_index.log"
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

/***************************** Datatypes *******************************/

#if 0
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
#endif

/***************************** Variables *******************************/
//TODO
LOCAL DatabaseSpecifier          *indexDatabaseSpecifier = NULL;
LOCAL Semaphore                  indexPauseLock;
LOCAL IndexPauseCallbackFunction indexPauseCallbackFunction = NULL;
LOCAL void                       *indexPauseCallbackUserData;

LOCAL ProgressInfo               importProgressInfo;

#ifndef NDEBUG
  LOCAL void const *indexBusyStackTrace[32];
  LOCAL uint       indexBusyStackTraceSize;
#endif /* not NDEBUG */

#ifdef INDEX_DEBUG_LOCK
  LOCAL ThreadLWPId indexUseCountLPWIds[32];
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

#ifndef NDEBUG
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

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle             *indexHandle,
                         const DatabaseSpecifier *databaseSpecifier,
                         ServerIO                *masterIO,
                         IndexOpenModes          indexOpenMode,
                         long                    timeout
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char              *__fileName__,
                           ulong                   __lineNb__,
                           IndexHandle             *indexHandle,
                           const DatabaseSpecifier *databaseSpecifier,
                           ServerIO                *masterIO,
                           IndexOpenModes          indexOpenMode,
                           long                    timeout
                          )
#endif /* NDEBUG */
{
  Errors          error;
  IndexDefinition indexDefinition;

  assert(indexHandle != NULL);
  assert(databaseSpecifier != NULL);

  // init variables
  indexHandle->masterIO            = masterIO;
// TODO:
  indexHandle->uriString           = NULL;
  indexHandle->busyHandlerFunction = NULL;
  indexHandle->busyHandlerUserData = NULL;
  indexHandle->upgradeError        = ERROR_NONE;
  #ifndef NDEBUG
    indexHandle->threadId = pthread_self();
  #endif /* NDEBUG */

  // check and complete database specifier
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      break;
    case DATABASE_TYPE_MYSQL:
      if (String_isEmpty(databaseSpecifier->mysql.databaseName)) String_setCString(databaseSpecifier->mysql.databaseName,DEFAULT_DATABASE_NAME);
      break;
  }

  // open index database
  if ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_CREATE)
  {
    // open database
    INDEX_DOX(error,
              indexHandle,
    {
      #ifdef NDEBUG
        return Database_open(&indexHandle->databaseHandle,
                             databaseSpecifier,
                             DATABASE_OPENMODE_FORCE_CREATE|DATABASE_OPENMODE_AUX,
                             DATABASE_TIMEOUT
                            );
      #else /* not NDEBUG */
        return __Database_open(__fileName__,__lineNb__,
                               &indexHandle->databaseHandle,
                               databaseSpecifier,
                               DATABASE_OPENMODE_FORCE_CREATE|DATABASE_OPENMODE_AUX,
                               DATABASE_TIMEOUT
                              );
      #endif /* NDEBUG */
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // create tables/indicees/triggers
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
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));

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
                             databaseSpecifier,
                             (((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
                               ? DATABASE_OPENMODE_READWRITE
                               : DATABASE_OPENMODE_READ
                             )|DATABASE_OPENMODE_AUX,
                             timeout
                            );
      #else /* not NDEBUG */
        return __Database_open(__fileName__,__lineNb__,
                               &indexHandle->databaseHandle,
                               databaseSpecifier,
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

  // free resources

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

LOCAL Errors renameIndex(DatabaseSpecifier *databaseSpecifier, ConstString newDatabaseName)
{
  DatabaseSpecifier renameToDatabaseSpecifier;
  Errors            error;
  DatabaseHandle    databaseHandle;
  IndexDefinition   indexDefinition;

  // drop triggers (required for some databases before rename)
  error = Database_open(&databaseHandle,
                        databaseSpecifier,
                        DATABASE_OPENMODE_READWRITE,
                        NO_WAIT
                       );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_dropTriggers(&databaseHandle);
  if (error != ERROR_NONE)
  {
    Database_close(&databaseHandle);
    return error;
  }
  Database_close(&databaseHandle);

  // rename
  Database_copySpecifier(&renameToDatabaseSpecifier,databaseSpecifier);
  error = Database_rename(&renameToDatabaseSpecifier,newDatabaseName);
  if (error != ERROR_NONE)
  {
    Database_doneSpecifier(&renameToDatabaseSpecifier);
    return error;
  }

  // re-create triggers
  error = Database_open(&databaseHandle,
                        &renameToDatabaseSpecifier,
                        DATABASE_OPENMODE_READWRITE,
                        NO_WAIT
                       );
  if (error != ERROR_NONE)
  {
    Database_doneSpecifier(&renameToDatabaseSpecifier);
    return error;
  }
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_TRIGGERS[Database_getType(&databaseHandle)], indexDefinition, error == ERROR_NONE)
  {
    error = Database_execute(&databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             indexDefinition
                            );
  }
  if (error != ERROR_NONE)
  {
    Database_close(&databaseHandle);
    Database_doneSpecifier(&renameToDatabaseSpecifier);
    return error;
  }
  Database_close(&databaseHandle);

  // free resources
  Database_doneSpecifier(&renameToDatabaseSpecifier);

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
* Input  : databaseSpecifier - database specifier
* Output : indexVersion - index version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getIndexVersion(uint *indexVersion, const DatabaseSpecifier *databaseSpecifier)
{
  Errors      error;
  IndexHandle indexHandle;

  // open index database
  error = openIndex(&indexHandle,databaseSpecifier,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getUInt(&indexHandle.databaseHandle,
                           indexVersion,
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
    (void)closeIndex(&indexHandle);
    return error;
  }

  // close index database
  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

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

  (void)Database_update(&indexHandle->databaseHandle,
                        NULL,  // changedRowCount
                        tableName,
                        DATABASE_FLAG_NONE,
                        DATABASE_VALUES2
                        (
                          DATABASE_VALUE("id", "rowId"),
                        ),
                        "id IS NULL",
                        DATABASE_FILTERS
                        (
                        )
                      );
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
(void)oldIndexHandle;
(void)uuidCountFactor;
(void)entityCountFactor;
(void)storageCountFactor;
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
(void)oldIndexHandle;
(void)newIndexHandle;
return ERROR_STILL_NOT_IMPLEMENTED;
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

LOCAL Errors importIndex(IndexHandle *indexHandle, ConstString oldDatabaseURI)
{
  DatabaseSpecifier databaseSpecifier;
  Errors            error;
  IndexHandle       oldIndexHandle;
  int64             indexVersion;
//  ulong             maxSteps;
  IndexQueryHandle  indexQueryHandle;
  uint64            t0;
  uint64            t1;
  IndexId           uuidId,entityId,storageId;

  Database_parseSpecifier(&databaseSpecifier,String_cString(oldDatabaseURI));

  // open old index (Note: must be read/write to fix errors in database)
  error = openIndex(&oldIndexHandle,&databaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE,NO_WAIT);
  if (error != ERROR_NONE)
  {
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  // get index version
  error = Database_getInt64(&oldIndexHandle.databaseHandle,
                            &indexVersion,
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
    (void)closeIndex(&oldIndexHandle);
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  // upgrade index structure
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Import index database '%s' (version %d)",
              String_cString(oldDatabaseURI),
              indexVersion
             );
  DIMPORT("import index %"PRIi64"",indexVersion);
// TODO:
#if 0
  maxSteps = 0L;
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
  IndexCommon_initProgress(&importProgressInfo,"Import");
  IndexCommon_resetProgress(&importProgressInfo,maxSteps);
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
      IndexCommon_progressStep(&importProgressInfo);
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
      IndexCommon_progressStep(&importProgressInfo);
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
      IndexCommon_progressStep(&importProgressInfo);
    }
    Index_doneList(&indexQueryHandle);
  }
  DIMPORT("create aggregates done (error: %s)",Error_getText(error));

  IndexCommon_doneProgress(&importProgressInfo);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Imported old index database '%s' (version %d)",
                String_cString(oldDatabaseURI),
                indexVersion
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Import old index database '%s' (version %d) fail: %s",
                String_cString(oldDatabaseURI),
                indexVersion,
                Error_getText(error)
               );
  }

  // close old index
  (void)closeIndex(&oldIndexHandle);

  // free resources
  Database_doneSpecifier(&databaseSpecifier);

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
// TODO: remove
#if 0
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "meta",
                            DATABASE_FLAG_NONE,
                            "ROWID NOT IN (SELECT MIN(rowid) FROM meta GROUP BY name)",
                            DATABASE_VALUES
                            (
                            ),
                            0
                           );
#endif
#if 0
  if (Database_prepare(&databaseStatementHandle,
                       &indexHandle->databaseHandle,
                       DATABASE_COLUMN_TYPES(STRING),
                       "SELECT name FROM meta GROUP BY name"
                       DATABASE_VALUES
                       (
                       ),
                       DATABASE_FILTERS
                       (
                       )
                      ) == ERROR_NONE
     )
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(STRING),
                             "SELECT name FROM meta GROUP BY name"
                             DATABASE_VALUES
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle,
                                 "%S",
                                 name
                                )
            )
      {
        (void)Database_delete(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "meta",
                              DATABASE_FLAG_NONE,
                              "    name=? \
                               AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=? ORDER BY rowId DESC LIMIT 0,1)) \
                              ",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_STRING(name)
                                DATABASE_FILTER_STRING(name)
                              ),
                              0
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
    (void)Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "entities",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES2
                          (
                            DATABASE_VALUE_UINT("lockedCount", 0),
                          ),
                          NULL,
                          DATABASE_FILTERS
                          (
                          )
                         );

    // clear state of deleted storages
    (void)Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES2
                          (
                            DATABASE_VALUE_UINT("state", INDEX_STATE_NONE),
                          ),
                          "deletedFlag=?",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_BOOL(TRUE),
                          )
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
  Errors                  error;
  String                  name1,name2;
  DatabaseStatementHandle databaseStatementHandle1,databaseStatementHandle2;
  DatabaseId              storageDatabaseId;
  StaticString            (uuid,MISC_UUID_STRING_LENGTH);
  uint64                  createdDateTime;
  DatabaseId              entityDatabaseId;
  bool                    equalsFlag;
  ulong                   i;
  String                  oldDatabaseFileName;

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
                             DATABASE_COLUMN_TYPES(STRING,STRING,UINT64),
                             "SELECT uuid, \
                                     name, \
                                     UNIX_TIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY id,created ASC \
                             ",
                             DATABASE_VALUES
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,results
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
                                 DATABASE_COLUMN_TYPES(KEY,STRING),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=? \
                                 ",
                                 DATABASE_VALUES
                                 (
                                 ).
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_STRING(uuid)
                                 )
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
// TODO:
              (void)Database_update(&newIndexHandle->databaseHandle,
                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_KEY("entityId", entityDatabaseId),
                                    ),
                                    NULL,
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(storageDatabaseId)
                                    )
                                   );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_insert(&newIndexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "entities",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("jobUUID", uuid),
                                  UINT64("created", createdDateTime),
                                  UINT  ("type",    ARCHIVE_TYPE_FULL),
                                )
                               );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

          // assign entity id for all storage entries with same uuid and matching name (equals except digits)
          error = Database_prepare(&databaseStatementHandle2,
                                   &oldIndexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(KEY,STRING),
                                   "SELECT id, \
                                           name \
                                    FROM storages \
                                    WHERE uuid=? \
                                   ",
                                   DATABASE_VALUES
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                     STRING(uuid)
                                   )
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
// TODO:
                (void)Database_update(&newIndexHandle->databaseHandle,
                                      NULL,  // changedRowCount
                                      "storages",
                                      DATABASE_FLAG_NONE,
                                      DATABASE_VALUES2
                                      (
                                        DATABASE_VALUE_KEY("entityId", entityId),
                                      ),
                                      NULL,
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(storageId)
                                      )
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
                             "    ((state<?) OR (state>?)) \
                              AND deletedFlag!=1 \
                             ",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_STATE_MIN),
                               DATABASE_FILTER_UINT(INDEX_STATE_MAX)
                             )
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
                             DATABASE_COLUMN_TYPES(KEY,STRING,UINT64),
                             "SELECT id, \
                                     name, \
                                     UNIX_TIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY name ASC \
                             "
                             DATABASE_VALUES
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,
                                 "%llu %S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseStatementHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(KEY,STRING),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=? \
                                 ",
                                 DATABASE_VALUES
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_STRING(uuid)
                                 )
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
// TODO:
              (void)Database_update(&newIndexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_UINT("entityId", entityDatabaseId),
                                    ),
                                    NULL,
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(storageDatabaseId)
                                    )
                                   );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_insert(&newIndexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "entities",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES
                                (
                                  STRING("jobUUID", uuid},
                                  UINT64("created", createdDateTime},
                                  UINT  ("type",    ARCHIVE_TYPE_FULL},
                                )
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
    return Database_delete(&indexHandle->databaseHandle,
                           &n,
                           "uuids",
                           DATABASE_FLAG_NONE,
                           "uuids.jobUUID='' \
                           ",
                           DATABASE_FILTERS
                           (
                           ),
                           0
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

// ---------------------------------------------------------------------

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
                               DATABASE_COLUMN_TYPES(KEY,UINT64),
                               "SELECT id, \
                                       UNIX_TIMESTAMP(timeLastChanged) \
                                FROM entriesNewest\
                                WHERE name=? \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_STRING(name)
                               )
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
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_UINT("entryId",         entryId),
                                      DATABASE_VALUE_UINT("uuidId",          uuidId),
                                      DATABASE_VALUE_UINT("entityId",        entityId),
                                      DATABASE_VALUE_UINT("type",            indexType),
                                      DATABASE_VALUE_UINT("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT("userId",          userId),
                                      DATABASE_VALUE_UINT("groupId",         groupId),
                                      DATABASE_VALUE_UINT("permission",      permission),
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(newestEntryId)
                                    )
                                   );
          }
          else
          {
            // insert
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_KEY   ("entryId",         entryId),
                                      DATABASE_VALUE_KEY   ("uuidId",          uuidId),
                                      DATABASE_VALUE_KEY   ("entityId",        entityId),
                                      DATABASE_VALUE_UINT  ("type",            indexType),
                                      DATABASE_VALUE_STRING("name",            name),
                                      DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT  ("userId",          userId),
                                      DATABASE_VALUE_UINT  ("groupId",         groupId),
                                      DATABASE_VALUE_UINT  ("permission",      permission)
                                    )
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
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            "entryId=? \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            0
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entryId")
                                    DATABASE_VALUE_KEY   ("uuidId")
                                    DATABASE_VALUE_KEY   ("entityId")
                                    DATABASE_VALUE_UINT  ("type")
                                    DATABASE_VALUE_STRING("name")
                                    DATABASE_VALUE_UINT64("timeLastChanged")
                                    DATABASE_VALUE_UINT  ("userId")
                                    DATABASE_VALUE_UINT  ("groupId")
                                    DATABASE_VALUE_UINT  ("permission")
                                  ),
                                  DATABASE_TABLES
                                  (
                                    "entries"
                                  ),
                                  DATABASE_COLUMNS
                                  (
                                    DATABASE_COLUMN_KEY   ("id"),
                                    DATABASE_COLUMN_KEY   ("uuidId"),
                                    DATABASE_COLUMN_KEY   ("entityId"),
                                    DATABASE_COLUMN_UINT  ("type"),
                                    DATABASE_COLUMN_STRING("name"),
                                    DATABASE_COLUMN_UINT64("timeLastChanged"),
                                    DATABASE_COLUMN_UINT  ("userId"),
                                    DATABASE_COLUMN_UINT  ("groupId"),
                                    DATABASE_COLUMN_UINT  ("permission")
                                  ),
                                  "name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(name)
                                  ),
                                  "ORDER BY timeLastChanged DESC",
                                  0LL,
                                  1LL
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
#if 0
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
#endif

  assert(indexDatabaseSpecifier != NULL);

  // open index
  do
  {
    error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,INDEX_PURGE_TIMEOUT);
    if ((error != ERROR_NONE) && !indexQuitFlag)
    {
      Misc_mdelay(1*MS_PER_SECOND);
    }
  }
  while ((error != ERROR_NONE) && !indexQuitFlag);
  if (indexQuitFlag)
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
                indexDatabaseSpecifier,
                Error_getText(error)
               );
    return;
  }

  // set index handle for thread interruption
  indexThreadIndexHandle = &indexHandle;

// TODO: remove #if 0
#if 0
  #ifdef INDEX_IMPORT_OLD_DATABASE
    // get absolute database file name
// TODO:
    absoluteFileName = File_getAbsoluteFileNameCString(String_new(),indexDatabaseSpecifier);

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
                  indexDatabaseSpecifier,
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
    while (   !indexQuitFlag
           && (File_readDirectoryList(&directoryListHandle,oldDatabaseFileName) == ERROR_NONE)
          )
    {
      if (   String_startsWith(oldDatabaseFileName,absoluteFileName)\
          && String_matchCString(oldDatabaseFileName,STRING_BEGIN,DATABASE_SAVE_PATTERNS[indexDatabaseSpecifier->type],NULL,NULL)
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
        DIMPORT("import %s -> %s",String_cString(oldDatabaseFileName),indexDatabaseSpecifier);
        error = importIndex(&indexHandle,oldDatabaseFileName);
        if (error == ERROR_NONE)
        {
          oldDatabaseCount++;
          (void)File_delete(oldDatabaseFileName,FALSE);
        }
        else
        {
          failFileName = String_appendCString(String_duplicate(oldDatabaseFileName),".fail");
// TODO:
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
#endif

  // index is initialized and ready to use
  indexInitializedFlag = TRUE;

// TODO: remove #if 0
#if 0
  // regular clean-ups
  storageName = String_new();
  while (!indexQuitFlag)
  {
    #ifdef INDEX_SUPPORT_DELETE
      // remove deleted storages from index if maintenance time
      if (IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()))
      {
        do
        {
          error = ERROR_NONE;

          // wait until index is unused
          WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
              || indexQuitFlag
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
                                     DATABASE_COLUMN_TYPES(KEY,KEY,STRING),
                                     "SELECT id, \
                                             entityId, \
                                             name \
                                      FROM storages \
                                      WHERE     state!=? \
                                            AND deletedFlag=1 \
                                      LIMIT 0,1 \
                                     ",
                                     DATABASE_VALUES2
                                     (
                                     ),
                                     DATABASE_FILTERS
                                     (
                                       UINT(INDEX_STATE_UPDATE)
                                     )
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
          if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
              || indexQuitFlag
             )
          {
            break;
          }

          // wait until index is unused
          WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
              || indexQuitFlag
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
                WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
              }
            }
            while (   (error == ERROR_INTERRUPTED)
                   && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                   && !indexQuitFlag
                  );

            // prune entity
            if (   (entityId != DATABASE_ID_NONE)
                && (entityId != INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
               )
            {
              do
              {
                error = IndexEntity_prune(&indexHandle,
                                    NULL,  // doneFlag
                                    NULL,  // deletedCounter
                                    entityId
                                   );
                if (error == ERROR_INTERRUPTED)
                {
                  // wait until index is unused
                  WAIT_NOT_IN_USEX(5LL*MS_PER_SECOND,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
                }
              }
              while (   (error == ERROR_INTERRUPTED)
                     && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                     && !indexQuitFlag
                    );
            }
          }
        }
        while (   (storageId != DATABASE_ID_NONE)
               && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
               && !indexQuitFlag
              );
      }
    #endif /* INDEX_SUPPORT_DELETE */

    // sleep and check quit flag/trigger (min. 10s)
    Misc_udelay(10*US_PER_SECOND);
    sleepTime = 10;
    SEMAPHORE_LOCKED_DO(&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !indexQuitFlag
             && (sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD)
             && !Semaphore_waitModified(&indexThreadTrigger,10*MS_PER_SECOND)
        )
      {
        sleepTime += 10;
      }
    }
  }
  String_delete(storageName);
#endif

  // clear index handle for thread interruption
  indexThreadIndexHandle = NULL;

  // free resources
  closeIndex(&indexHandle);
}

// ----------------------------------------------------------------------

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

Errors Index_init(const char             *uriString,
                  IndexIsMaintenanceTime IndexCommon_isMaintenanceTimeFunction,
                  void                   *IndexCommon_isMaintenanceTimeUserData
                 )
{
  bool              createFlag;
  Errors            error;
  uint              indexVersion;
//  String            saveDatabaseName;
//  uint              n;
//  DatabaseSpecifier databaseSpecifierReference = { DATABASE_TYPE_SQLITE3, {(intptr_t)NULL} };
//  IndexHandle       indexHandleReference,indexHandle;
IndexHandle       indexHandle;
//  ProgressInfo      progressInfo;

  assert(uriString != NULL);

  // init variables
  indexIsMaintenanceTimeFunction = IndexCommon_isMaintenanceTimeFunction;
  indexIsMaintenanceTimeUserData = IndexCommon_isMaintenanceTimeUserData;
  indexQuitFlag                  = FALSE;

  // get database specifier
  assert(indexDatabaseSpecifier == NULL);
  indexDatabaseSpecifier = Database_newSpecifier(uriString);
  if (indexDatabaseSpecifier == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  createFlag = FALSE;

  // check if index exists, check version
  if (!createFlag)
  {
// TODO: remove #if 0
#if 0
    if (Database_exists(indexDatabaseSpecifier,NULL))
    {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
      // check index version
      error = getIndexVersion(&indexVersion,indexDatabaseSpecifier);
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));
fprintf(stderr,"%s:%d: indexVersion=%lld\n",__FILE__,__LINE__,indexVersion);
      if (error == ERROR_NONE)
      {
        if (indexVersion < INDEX_VERSION)
        {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
          // rename existing index for upgrade
          saveDatabaseName = String_new();
          n = 0;
          do
          {
            String_setCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
            String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
            n++;
          }
          while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));
          error = renameIndex(indexDatabaseSpecifier,saveDatabaseName);
          if (error != ERROR_NONE)
          {
            String_delete(saveDatabaseName);
            Database_deleteSpecifier(indexDatabaseSpecifier);
            return error;
          }
          String_delete(saveDatabaseName);

          // upgrade version -> create new
          createFlag = TRUE;
fprintf(stderr,"%s:%d: upgare_\n",__FILE__,__LINE__);
          plogMessage(NULL,  // logHandle
                      LOG_TYPE_ERROR,
                      "INDEX",
                      "Old index database version %d in '%s' - create new",
                      indexVersion,
                      indexDatabaseSpecifier
                     );
        }
      }
      else
      {
        // unknown version -> create new
        createFlag = TRUE;
fprintf(stderr,"%s:%d: unknow_\n",__FILE__,__LINE__);
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Unknown index database version in '%s' - create new",
                    indexDatabaseSpecifier
                   );
      }
    }
    else
    {
      // does not exists -> create new
      createFlag = TRUE;
    }
#endif
  }

  if (!createFlag)
  {
// TODO: remove #if 0
#if 0
    // check if database is outdated or corrupt
    if (Database_exists(indexDatabaseSpecifier,NULL))
    {
      error = openIndex(&indexHandleReference,&databaseSpecifierReference,NULL,INDEX_OPEN_MODE_CREATE,NO_WAIT);
      if (error == ERROR_NONE)
      {
        error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
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
        saveDatabaseName = String_new();
        n = 0;
        do
        {
          String_setCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
          String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
          n++;
        }
        while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));
        error = renameIndex(indexDatabaseSpecifier,saveDatabaseName);
        if (error != ERROR_NONE)
        {
          String_delete(saveDatabaseName);
          Database_deleteSpecifier(indexDatabaseSpecifier);
          return error;
        }
        String_delete(saveDatabaseName);

        // outdated or corrupt -> create new
        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Outdated or corrupt index database '%s' (error: %s) - create new",
                    indexDatabaseSpecifier,
                    Error_getText(error)
                   );
      }
    }
#endif
  }

  if (createFlag)
  {
    // create new index database
    error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_CREATE,NO_WAIT);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create new index database '%s' fail: %s",
                  indexDatabaseSpecifier,
                  Error_getText(error)
                 );
      Database_deleteSpecifier(indexDatabaseSpecifier);
      return error;
    }
    closeIndex(&indexHandle);

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Created new index database '%s' (version %d)",
                indexDatabaseSpecifier,
                INDEX_VERSION
               );
  }
  else
  {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
    // get database index version
    error = getIndexVersion(&indexVersion,indexDatabaseSpecifier);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Cannot get index database version from '%s': %s",
                  indexDatabaseSpecifier,
                  Error_getText(error)
                 );
      Database_deleteSpecifier(indexDatabaseSpecifier);
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Opened index database '%s' (version %d)",
                indexDatabaseSpecifier,
                indexVersion
               );
  }

// TODO: remove #if 0
#if 0
  // initial clean-up
  error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE,NO_WAIT);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot get index database version from '%s': %s",
                indexDatabaseSpecifier,
                Error_getText(error)
               );
    Database_deleteSpecifier(indexDatabaseSpecifier);
    indexDatabaseSpecifier = NULL;
    return error;
  }
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Started initial clean-up index database"
             );

  #ifdef INDEX_INTIIAL_CLEANUP
    IndexCommon_initProgress(&progressInfo,"Clean");
    (void)cleanUpDuplicateMeta(&indexHandle);
    (void)cleanUpIncompleteUpdate(&indexHandle);
    (void)cleanUpIncompleteCreate(&indexHandle);
    (void)cleanUpStorageNoName(&indexHandle);
    (void)cleanUpStorageNoEntity(&indexHandle);
    (void)cleanUpStorageInvalidState(&indexHandle);
    (void)cleanUpNoUUID(&indexHandle);
    (void)pruneStorages(&indexHandle,&progressInfo);
    (void)IndexEntity_pruneAll(&indexHandle,NULL,NULL);
    (void)IndexUUID_pruneAlls(&indexHandle,NULL,NULL);
    IndexCommon_doneProgress(&progressInfo);
  #endif /* INDEX_INTIIAL_CLEANUP */
  closeIndex(&indexHandle);
#endif

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
    indexQuitFlag = TRUE;
    if (!Thread_join(&indexThread))
    {
      HALT_INTERNAL_ERROR("Cannot stop index thread!");
    }
  #endif /* INDEX_INTIIAL_CLEANUP */

  // free resources
  #ifdef INDEX_INTIIAL_CLEANUP
    Thread_done(&indexThread);
  #endif /* INDEX_INTIIAL_CLEANUP */
  Database_deleteSpecifier(indexDatabaseSpecifier);
  indexDatabaseSpecifier = NULL;
}

bool Index_isAvailable(void)
{
  return indexDatabaseSpecifier != NULL;
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
  IndexCommon_addIndexInUseThreadInfo();
}

void Index_endInUse(void)
{
  IndexCommon_removeIndexInUseThreadInfo();
}

bool Index_isIndexInUse(void)
{
  return IndexCommon_isIndexInUse();
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
    assert(indexDatabaseSpecifier != NULL);

    indexHandle = (IndexHandle*)malloc(sizeof(IndexHandle));
    if (indexHandle == NULL)
    {
      return NULL;
    }

    #ifdef NDEBUG
      error = openIndex(indexHandle,
                        indexDatabaseSpecifier,
                        masterIO,
                        INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,
                        timeout
                       );
    #else /* not NDEBUG */
      error = __openIndex(__fileName__,
                          __lineNb__,
                          indexHandle,
                          indexDatabaseSpecifier,
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
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                               )
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
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                               )
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
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                               )
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
                               DATABASE_COLUMN_TYPES(INT,INT64,
                                                     INT,
                                                     INT,INT64,INT,INT64,INT,INT,INT,INT64,INT,
                                                     INT,INT64,
                                                     INT,INT64,INT,INT64,INT,INT,INT,INT64,INT,
                                                     INT,INT64
                                                    ),
                               "SELECT SUM(storages.totalEntryCount), \
                                       SUM(storages.totalEntrySize), \
                                       0, \
                                       SUM(storages.totalFileCount), \
                                       SUM(storages.totalFileSize), \
                                       SUM(storages.totalImageCount), \
                                       SUM(storages.totalImageSize), \
                                       SUM(storages.totalDirectoryCount), \
                                       SUM(storages.totalLinkCount), \
                                       SUM(storages.totalHardlinkCount), \
                                       SUM(storages.totalHardlinkSize), \
                                       SUM(storages.totalSpecialCount), \
                                       \
                                       SUM(storages.totalEntryCountNewest), \
                                       SUM(storages.totalEntrySizeNewest), \
                                       0, \
                                       SUM(storages.totalFileCountNewest), \
                                       SUM(storages.totalFileSizeNewest), \
                                       SUM(storages.totalImageCountNewest), \
                                       SUM(storages.totalImageSizeNewest), \
                                       SUM(storages.totalDirectoryCountNewest), \
                                       SUM(storages.totalLinkCountNewest), \
                                       SUM(storages.totalHardlinkCountNewest), \
                                       SUM(storages.totalHardlinkSizeNewest), \
                                       SUM(storages.totalSpecialCountNewest), \
                                       \
                                       COUNT(id), \
                                       SUM(storages.size) \
                                FROM storages \
                                WHERE deletedFlag!=1 \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                               )
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
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                               )
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

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  Database_finalize(&indexQueryHandle->databaseStatementHandle);
  IndexCommon_doneIndexQueryHandle(indexQueryHandle);
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
