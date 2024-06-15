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
#include "common/cstrings.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "common/progressinfo.h"
#include "errors.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

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
const char *DATABASE_SAVE_EXTENSIONS[] =
{
  ".old%03d",
  "_old%03d",
  "_old%03d"
};
const char *DATABASE_SAVE_PATTERNS[] =
{
  ".*\\.old\\d\\d\\d$",
  ".*\\_old\\d\\d\\d$",
  ".*\\_old\\d\\d\\d$"
};

const IndexId INDEX_ID_NONE = {.type = INDEX_TYPE_NONE,.value = DATABASE_ID_NONE};
const IndexId INDEX_ID_ANY  = {.type = INDEX_TYPE_NONE,.value = DATABASE_ID_ANY };

// index open mask
#define INDEX_OPEN_MASK_MODE  0x0000000F
#define INDEX_OPEN_MASK_FLAGS 0xFFFF0000

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
    #ifndef HAVE_BACKTRACE
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
LOCAL Semaphore                  indexOpenLock;
LOCAL Semaphore                  indexPauseLock;
LOCAL IndexPauseCallbackFunction indexPauseCallbackFunction = NULL;
LOCAL void                       *indexPauseCallbackUserData;

LOCAL char outputProgressBuffer[128];
LOCAL uint outputProgressBufferLength;

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
  assert((databaseSpecifier != NULL) || (masterIO != NULL));

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

  if (masterIO == NULL)
  {
    assert(databaseSpecifier != NULL);

    // check and complete database specifier
    switch (databaseSpecifier->type)
    {
      case DATABASE_TYPE_SQLITE3:
        break;
      case DATABASE_TYPE_MARIADB:
        #if defined(HAVE_MARIADB)
          if (String_isEmpty(databaseSpecifier->mariadb.databaseName))
          {
            String_setCString(databaseSpecifier->mariadb.databaseName,DEFAULT_DATABASE_NAME);
          }
        #else /* HAVE_MARIADB */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      case DATABASE_TYPE_POSTGRESQL:
        #if defined(HAVE_POSTGRESQL)
          if (String_isEmpty(databaseSpecifier->postgresql.databaseName))
          {
            String_setCString(databaseSpecifier->postgresql.databaseName,DEFAULT_DATABASE_NAME);
          }
        #else /* HAVE_POSTGRESQL */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_POSTGRESQL */
        break;
    }

    SEMAPHORE_LOCKED_DO(&indexOpenLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      error = ERROR_NONE;

      // open index database
      if ((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_CREATE)
      {
        // create database
        if (error == ERROR_NONE)
        {
          INDEX_DOX(error,
                    indexHandle,
          {
            #ifdef NDEBUG
              return Database_open(&indexHandle->databaseHandle,
                                   databaseSpecifier,
                                   NULL,  // databaseName
                                   DATABASE_OPEN_MODE_FORCE_CREATE,
                                   timeout
                                  );
            #else /* not NDEBUG */
              return __Database_open(__fileName__,__lineNb__,
                                     &indexHandle->databaseHandle,
                                     databaseSpecifier,
                                     NULL,  // databaseName
                                     DATABASE_OPEN_MODE_FORCE_CREATE,
                                     timeout
                                    );
            #endif /* NDEBUG */
          });
        }

        // create tables/indices/triggers
        if (error == ERROR_NONE)
        {
          INDEX_DOX(error,
                    indexHandle,
          {
            INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITIONS[Database_getType(&indexHandle->databaseHandle)],
                                       indexDefinition,
                                       error == ERROR_NONE
                                      )
            {
              error = Database_execute(&indexHandle->databaseHandle,
                                       NULL,  // changedRowCount
                                       DATABASE_FLAG_NONE,
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
          }
        }
      }
      else
      {
        // open database
        if (error == ERROR_NONE)
        {
          INDEX_DOX(error,
                    indexHandle,
          {
            #ifdef NDEBUG
              return Database_open(&indexHandle->databaseHandle,
                                   databaseSpecifier,
                                   NULL,  // databaseName
                                   (((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
                                     ? DATABASE_OPEN_MODE_READWRITE
                                     : DATABASE_OPEN_MODE_READ
                                   ),
                                   timeout
                                  );
            #else /* not NDEBUG */
              return __Database_open(__fileName__,__lineNb__,
                                     &indexHandle->databaseHandle,
                                     databaseSpecifier,
                                     NULL,  // databaseName
                                     (((indexOpenMode & INDEX_OPEN_MASK_MODE) == INDEX_OPEN_MODE_READ_WRITE)
                                       ? DATABASE_OPEN_MODE_READWRITE
                                       : DATABASE_OPEN_MODE_READ
                                     ),
                                     timeout
                                    );
            #endif /* NDEBUG */
          });
        }
      }
    }
    if (error != ERROR_NONE)
    {
      return error;
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
  }

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

  if (indexHandle->masterIO == NULL)
  {
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
  }

  return ERROR_NONE;
}

#if 0
/***********************************************************************\
* Name   : renameIndex
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors renameIndex(DatabaseSpecifier *databaseSpecifier, ConstString newDatabaseName)
{
  DatabaseSpecifier renameToDatabaseSpecifier;
  Errors            error;
  DatabaseHandle    databaseHandle;
  IndexDefinition   indexDefinition;

  // drop triggers (required for some databases before rename)
  error = Database_open(&databaseHandle,
                        databaseSpecifier,
                        NULL,  // databaseName
                        DATABASE_OPEN_MODE_READWRITE,
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
  Database_copySpecifier(&renameToDatabaseSpecifier,databaseSpecifier,NULL);
  error = Database_rename(&renameToDatabaseSpecifier,newDatabaseName);
  if (error != ERROR_NONE)
  {
    Database_doneSpecifier(&renameToDatabaseSpecifier);
    return error;
  }

  // re-create triggers
  error = Database_open(&databaseHandle,
                        &renameToDatabaseSpecifier,
                        NULL,  // databaseName
                        DATABASE_OPEN_MODE_READWRITE,
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
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
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
#endif

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
  error = openIndex(&indexHandle,databaseSpecifier,NULL,INDEX_OPEN_MODE_READ,INDEX_TIMEOUT);
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

#if 0
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
                        DATABASE_VALUES
                        (
                          DATABASE_VALUE("id", "rowId"),
                        ),
                        "id IS NULL",
                        DATABASE_FILTERS
                        (
                        )
                      );
}
#endif

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

/***********************************************************************\
* Name   : formatSubProgressInfo
* Purpose: format sub-progress info call back
* Input  : progress           - progress [%%]
*          estimatedTotalTime - estimated total time [s]
*          estimatedRestTime  - estimated rest time [s]
*          userData           - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void formatSubProgressInfo(uint  progress,
                                 ulong estimatedTotalTime,
                                 ulong estimatedRestTime,
                                 void  *userData
                                )
{
  UNUSED_VARIABLE(estimatedTotalTime);
  UNUSED_VARIABLE(userData);

  if (estimatedRestTime < (999*60*60))
  {
    stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                 "%6.1f%% %2dh:%02dmin:%02ds",
                 (float)progress/10.0,
                 estimatedRestTime/(60*60),
                 estimatedRestTime%(60*60)/60,
                 estimatedRestTime%60
                );
  }
  else
  {
    stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                 "%6.1f%% ---h:--min:--s",
                 (float)progress/10.0
                );
  }
}

/***********************************************************************\
* Name   : outputProgressInit
* Purpose: output progress text
* Input  : text     - text
*          maxSteps - nax. number of steps (not used)
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressInit(const char *text,
                              uint64     maxSteps,
                              void       *userData
                             )
{
  UNUSED_VARIABLE(maxSteps);
  UNUSED_VARIABLE(userData);

  printInfo(2,"%s",text);
}

// TODO: remove
#if 0
/***********************************************************************\
* Name   : outputProgressInfo
* Purpose: output progress info on console
* Input  : progress           - progres [%%]
*          estimatedTotalTime - estimated total time [s]
*          estimatedRestTime  - estimated rest time [s]
*          userData           - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressInfo(uint  progress,
                              ulong estimatedTotalTime,
                              ulong estimatedRestTime,
                              void  *userData
                             )
{
  const char *WHEEL = "|/-\\";
  static uint wheelIndex = 0;

  UNUSED_VARIABLE(estimatedTotalTime);
  UNUSED_VARIABLE(userData);

  if (estimatedRestTime < (99999*60*60))
  {
    stringAppendFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                       " / %7.1f%% %5uh:%02umin:%02us %c",
                       (float)progress/10.0,
                       estimatedRestTime/(60*60),
                       estimatedRestTime%(60*60)/60,
                       estimatedRestTime%60,
                       WHEEL[wheelIndex]
                      );
  }
  else
  {
    stringAppendFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                       " / %7.1f%% -----h:--min:--s %c",
                       (float)progress/10.0,
                       WHEEL[wheelIndex]
                      );
  }
  outputProgressBufferLength = stringLength(outputProgressBuffer);

  printInfo(2,"%s\n",outputProgressBuffer);

  wheelIndex = (wheelIndex+1) % 4;
}
#endif

/***********************************************************************\
* Name   : outputProgressDone
* Purpose: done progress output
* Input  : totalTime - total time [s]
*          userData  - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressDone(ulong totalTime,
                              void  *userData
                             )
{
  UNUSED_VARIABLE(userData);

  stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
               "%2dh:%02dmin:%02ds",
               totalTime/(60*60),
               totalTime%(60*60)/60,
               totalTime%60
              );
  stringFillAppend(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,' ');

  printInfo(2,"%s\n",outputProgressBuffer);

  fflush(stdout);
}

// TODO:
#if 0
#include "index_version1.c"
#include "index_version2.c"
#include "index_version3.c"
#include "index_version4.c"
#include "index_version5.c"
#endif
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

LOCAL Errors importIndex(IndexHandle *indexHandle, ConstString oldDatabaseURI)
{
  DatabaseSpecifier databaseSpecifier;
  Errors            error;
  IndexHandle       oldIndexHandle;
  int64             indexVersion;
  ulong             maxSteps;
  ProgressInfo      progressInfo;
  IndexQueryHandle  indexQueryHandle;
  uint64            t0;
  uint64            t1;
  IndexId           uuidId,entityId,storageId;

  error = Database_parseSpecifier(&databaseSpecifier,String_cString(oldDatabaseURI),INDEX_DEFAULT_DATABASE_NAME);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // open old index (Note: must be read/write to fix errors in database)
  error = openIndex(&oldIndexHandle,&databaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE,INDEX_TIMEOUT);
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

  maxSteps = 0LL;
  switch (indexVersion)
  {
    case 1:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 2:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 3:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 4:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 5:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 6:
      maxSteps = getImportStepsVersion6(&oldIndexHandle.databaseHandle);
      break;
    case INDEX_CONST_VERSION:
      maxSteps = getImportStepsVersion7(&oldIndexHandle.databaseHandle);
      break;
    default:
      // unknown version if index
      error = ERRORX_(DATABASE_VERSION_UNKNOWN,0,"%d",indexVersion);
      break;
  }

  ProgressInfo_init(&progressInfo,
                    NULL,  // parentProgressInfo
                    32,  // filterWindowSize
                    500,  // reportTime
                    maxSteps,
                    CALLBACK_(NULL,NULL),
                    CALLBACK_(NULL,NULL),
                    CALLBACK_INLINE(void,(uint progress, ulong estimatedTotalTime, ulong estimatedRestTime, void *userData),
                    {
                       UNUSED_VARIABLE(estimatedTotalTime);
                       UNUSED_VARIABLE(userData);

                       plogMessage(NULL,  // logHandle
                                   LOG_TYPE_INDEX,
                                   "INDEX",
                                   "%s %0.1f%%, estimated rest time %uh:%02umin:%02us",
                                   (float)progress/10.0,
                                   (uint)((estimatedRestTime/US_PER_SECOND)/3600LL),
                                   (uint)(((estimatedRestTime/US_PER_SECOND)%3600LL)/60),
                                   (uint)((estimatedRestTime/US_PER_SECOND)%60LL)
                                  );
                    },NULL),
                    "Import"
                   );
  switch (indexVersion)
  {
    case 1:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 2:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 3:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 4:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 5:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 6:
      error = importIndexVersion6(&oldIndexHandle.databaseHandle,
                                  &indexHandle->databaseHandle,
                                  &progressInfo
                                 );
      break;
    case INDEX_CONST_VERSION:
      error = importIndexVersion7(&oldIndexHandle.databaseHandle,
                                  &indexHandle->databaseHandle,
                                  &progressInfo
                                 );
      break;
    default:
      // unknown version if index
      error = ERRORX_(DATABASE_VERSION_UNKNOWN,0,"%d",indexVersion);
      break;
  }
  ProgressInfo_done(&progressInfo);
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
                                   INDEX_TYPESET_ALL,
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
          logImportProgress("Aggregated storage #%"PRIi64": (%"PRIu64"s)",
                            storageId,
                            (t1-t0)/US_PER_SECOND
                           );
        }
      }
      Index_doneList(&indexQueryHandle);
    }
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
    if (error == ERROR_NONE)
    {
      while (   (error == ERROR_NONE)
             && Index_getNextEntity(&indexQueryHandle,
                                    NULL,  // uuidId,
                                    NULL,  // jobUUID,
                                    NULL,  // scheduleUUID,
                                    &entityId,
                                    NULL,  // archiveType,
                                    NULL,  // createdDateTime,
                                    NULL,  // lastErrorCode
                                    NULL,  // lastErrorData
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
          logImportProgress("Aggregated entity #%"PRIi64": (%"PRIu64"s)",
                            entityId,
                            (t1-t0)/US_PER_SECOND
                           );
        }
      }
      Index_doneList(&indexQueryHandle);
    }
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
    if (error == ERROR_NONE)
    {
      while (   (error == ERROR_NONE)
             && Index_getNextUUID(&indexQueryHandle,
                                  &uuidId,
                                  NULL,  // jobUUID
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // lastErrorCode
                                  NULL,  // lastErrorData
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
          logImportProgress("Aggregated UUID #%"PRIi64": (%"PRIu64"s)",
                            uuidId,
                            (t1-t0)/US_PER_SECOND
                           );
        }
      }
      Index_doneList(&indexQueryHandle);
    }
  }
  DIMPORT("create aggregates done (error: %s)",Error_getText(error));

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

  error = ERROR_NONE;

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
                            DATABASE_UNLIMITED
                           );
#endif
#if 0
  if (Database_prepare(&databaseStatementHandle,
                       &indexHandle->databaseHandle,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_STRING("name")
                       ),
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
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_STRING("name")
                             ),
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
                               AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=? ORDER BY rowId DESC LIMIT 1)) \
                              ",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_STRING(name)
                                DATABASE_FILTER_STRING(name)
                              ),
                              DATABASE_UNLIMITED
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
                          DATABASE_VALUES
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
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT("state", INDEX_STATE_NONE),
                          ),
                          "deletedFlag=TRUE",
                          DATABASE_FILTERS
                          (
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
                    "Requested update index #%"PRIi64": %s",
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
  Array            storageIds;
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
  Array_init(&storageIds,sizeof(IndexId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  error = ERROR_NONE;
  while (   (error == ERROR_NONE)
         && (Index_findStorageByState(indexHandle,
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
            )
         && !Array_contains(&storageIds,&storageId)
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

    error = IndexStorage_purge(indexHandle,
                               storageId,
                               NULL  // progressInfo
                              );
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Purged incomplete storage #%"PRIi64": '%s'",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }

    Array_append(&storageIds,&storageId);
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);
  Array_done(&storageIds);

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
* Name   : cleanUpOrphanedStorages
* Purpose: clear index storage content
* Input  : indexHandle  - index handle
*          storageId    - database id of storage
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpOrphanedStorages(IndexHandle *indexHandle)
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

  // init variables
  Array_init(&entryIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  error = ERROR_NONE;

UNUSED_VARIABLE(indexHandle);
//TODO: do regulary in index thread?
#if 0
  /* get entries to purge without associated file/image/directory/link/hardlink/special entry
     Note: may be left from interrupted purge of previous run
  */
//l=0; Database_getInteger64(&indexHandle->databaseHandle,&l,"entries","count(id)",""); fprintf(stderr,"%s, %d: l=%"PRIi64"\n",__FILE__,__LINE__,l);
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
                             "entries \
                                LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND fileEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_FILE)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND imageEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND directoryEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_DIRECTORY)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND linkEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_LINK)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND hardlinkEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND specialEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_SPECIAL)
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[5] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries without associated entry to purge: file %"PRIu64"ms, image %"PRIu64"ms, directory %"PRIu64"ms, link %"PRIu64"ms, hardlink %"PRIu64"ms, special %"PRIu64"ms\n",__FILE__,__LINE__,
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
          String_appendFormat(entryIdsString,"%"PRIi64,entryId);
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
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return ERROR_NONE;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged orphaned entries: %"PRIu64"ms\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
#endif

  // free resources
  Array_done(&entryIds);

  return error;
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
                                INDEX_TYPESET_ANY_ENTRY,
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
  Errors     error;
  DatabaseId newestEntryId;
  uint64     newestTimeLastChanged;

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
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             newestEntryId         = values[0].id;
                             newestTimeLastChanged = values[1].dateTime;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest"
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("id"),
                             DATABASE_COLUMN_DATETIME("timeLastChanged")
                           ),
                           "name=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_STRING(name)
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        newestEntryId         = DATABASE_ID_NONE;
        newestTimeLastChanged = 0LL;
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
                                    DATABASE_VALUES
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
                                    NULL,  // insertRowId
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
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
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
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
  #ifndef NDEBUG
    #define TIMEOUT 500
  #else
    #define TIMEOUT (5*MS_PER_SECOND)
  #endif

  IndexHandle indexHandle;
  Errors      error;
  #ifdef INDEX_IMPORT_OLD_DATABASE
    String              absoluteFileName;
    String              directoryName;
    DirectoryListHandle directoryListHandle;
    uint                i;
    String              oldDatabaseFileName;
    uint                oldDatabaseCount;
    String              failFileName;
  #endif /* INDEX_IMPORT_OLD_DATABASE */
  IndexId     storageId,entityId;
  String      storageName;
  ulong       sleepTime;

  assert(indexDatabaseSpecifier != NULL);

  // open index
  do
  {
    error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_KEYS,INDEX_TIMEOUT);
    if ((error != ERROR_NONE) && !indexQuitFlag)
    {
      Misc_mdelay(TIMEOUT);
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

  // import old index files (Sqlite only)
  #ifdef INDEX_IMPORT_OLD_DATABASE
    if (indexDatabaseSpecifier->type == DATABASE_TYPE_SQLITE3)
    {
      // get absolute database file name
      absoluteFileName = File_getAbsoluteFileName(String_new(),indexDatabaseSpecifier->sqlite.fileName);

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
        if (String_matchCString(oldDatabaseFileName,
                                STRING_BEGIN,
                                DATABASE_SAVE_PATTERNS[indexDatabaseSpecifier->type],
                                NULL,
                                NULL
                               )
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
    }
  #endif /* INDEX_IMPORT_OLD_DATABASE */

  // index is initialized and ready to use
  indexInitializedFlag = TRUE;

  // regular clean-ups
  storageName = String_new();
  while (!indexQuitFlag)
  {
    // remove deleted storages from index if maintenance time
    if (IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()))
    {
      #ifdef INDEX_SUPPORT_DELETE
        do
        {
          error = ERROR_NONE;

          // wait until index is unused
          WAIT_NOT_IN_USEX(TIMEOUT,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
              || indexQuitFlag
             )
          {
            break;
          }

          // clean-up
// TODO: activate
//          (void)cleanUpOrphanedStorages(&indexHandle);

          // find next storage to remove (Note: get single entry for remove to avoid long-running prepare!)
          storageId = INDEX_ID_NONE;
          INDEX_DOX(error,
                    &indexHandle,
          {
            return Database_get(&indexHandle.databaseHandle,
                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                {
                                  assert(values != NULL);
                                  assert(valueCount == 3);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(valueCount);

                                  storageId = INDEX_ID_STORAGE(values[0].id);
                                  entityId  = INDEX_ID_ENTITY (values[1].id);
                                  String_set(storageName,values[2].string);

                                  return ERROR_NONE;
                                },NULL),
                                NULL,  // changedRowCount
                                DATABASE_TABLES
                                (
                                  "storages"
                                ),
                                DATABASE_FLAG_NONE,
                                DATABASE_COLUMNS
                                (
                                  DATABASE_COLUMN_KEY   ("id"),
                                  DATABASE_COLUMN_KEY   ("entityId"),
                                  DATABASE_COLUMN_STRING("name")
                                ),
                                "    state!=? \
                                 AND deletedFlag=TRUE \
                                ",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_UINT(INDEX_STATE_UPDATE)
                                ),
                                NULL,  // groupBy
                                NULL,  // orderBy
                                0LL,
                                1LL
                               );
          });
          if (   (error != ERROR_NONE)
              || INDEX_ID_IS_NONE(storageId)
             )
          {
            break;
          }

          // wait until index is unused
          WAIT_NOT_IN_USEX(TIMEOUT,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
          if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
              || indexQuitFlag
             )
          {
            break;
          }

          if (!INDEX_ID_IS_NONE(storageId))
          {
            // remove storage from database
            do
            {
              // delete storage from index
              error = IndexStorage_delete(&indexHandle,
                                          storageId,
                                          NULL  // progressInfo
                                         );
              if (error == ERROR_INTERRUPTED)
              {
                // wait until index is unused
                WAIT_NOT_IN_USEX(TIMEOUT,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
              }
            }
            while (   (error == ERROR_INTERRUPTED)
                   && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                   && !indexQuitFlag
                  );
            if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                || indexQuitFlag
               )
            {
              break;
            }

            // prune entity
            if (   !INDEX_ID_IS_NONE(entityId)
                && !INDEX_ID_IS_DEFAULT_ENTITY(entityId)
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
                  WAIT_NOT_IN_USEX(TIMEOUT,IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime()));
                }
              }
              while (   (error == ERROR_INTERRUPTED)
                     && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                     && !indexQuitFlag
                    );
              if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                  || indexQuitFlag
                 )
              {
                break;
              }
            }

            // prune UUID
// TODO:
            if (FALSE)
            {
              if (   !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
                  || indexQuitFlag
                 )
              {
                break;
              }
            }
          }
        }
        while (   !INDEX_ID_IS_NONE(storageId)
               && IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
               && !indexQuitFlag
              );
      #endif /* INDEX_SUPPORT_DELETE */

      if (!indexQuitFlag) (void)IndexStorage_pruneAll(&indexHandle,NULL,NULL);
      if (!indexQuitFlag) (void)IndexEntry_pruneAll(&indexHandle,NULL,NULL);
      if (!indexQuitFlag) (void)IndexEntity_pruneAll(&indexHandle,NULL,NULL);
      if (!indexQuitFlag) (void)IndexUUID_pruneAll(&indexHandle,NULL,NULL);
    }

    // sleep and check quit flag/trigger
    sleepTime = 0;
    SEMAPHORE_LOCKED_DO(&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !indexQuitFlag
             && (sleepTime < (SLEEP_TIME_INDEX_CLEANUP_THREAD*MS_PER_SECOND))
             && !Semaphore_waitModified(&indexThreadTrigger,TIMEOUT)
            )
      {
        sleepTime += TIMEOUT;
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

Errors Index_initAll(void)
{
  Errors error;

  // init variables
  Semaphore_init(&indexLock,SEMAPHORE_TYPE_BINARY);
  Array_init(&indexUsedBy,sizeof(ThreadInfo),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Semaphore_init(&indexOpenLock,SEMAPHORE_TYPE_BINARY);
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
    Semaphore_done(&indexOpenLock);
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
  Semaphore_done(&indexOpenLock);
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

bool Index_parseType(const char *name, IndexTypes *indexType, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexType != NULL);

  UNUSED_VARIABLE(userData);

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

Errors Index_init(const DatabaseSpecifier *databaseSpecifier,
                  IndexIsMaintenanceTime  IndexCommon_isMaintenanceTimeFunction,
                  void                    *IndexCommon_isMaintenanceTimeUserData
                 )
{
  String      printableDatabaseURI;
  bool        createFlag;
  Errors      error;
  uint        indexVersion;
  IndexHandle indexHandleReference,indexHandle;

  assert(databaseSpecifier != NULL);

  // init variables
  indexIsMaintenanceTimeFunction = IndexCommon_isMaintenanceTimeFunction;
  indexIsMaintenanceTimeUserData = IndexCommon_isMaintenanceTimeUserData;
  indexQuitFlag                  = FALSE;

  // get database specifier
  assert(indexDatabaseSpecifier == NULL);
  indexDatabaseSpecifier = Database_duplicateSpecifier(databaseSpecifier);
  if (indexDatabaseSpecifier == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  printableDatabaseURI = Database_getPrintableName(String_new(),indexDatabaseSpecifier,NULL);

  createFlag = FALSE;

  // check if index exists, check version
  if (!createFlag)
  {
    if (Database_exists(indexDatabaseSpecifier,NULL))
    {
      // check index version
      error = getIndexVersion(&indexVersion,indexDatabaseSpecifier);
      if (error == ERROR_NONE)
      {
        if (indexVersion < INDEX_VERSION)
        {
          // rename existing index for upgrade
          String saveDatabaseName = String_new();

          switch (indexDatabaseSpecifier->type)
          {
            case DATABASE_TYPE_SQLITE3:
              {
                // get directory where database is located
                String absoluteFileName = File_getAbsoluteFileName(String_new(),indexDatabaseSpecifier->sqlite.fileName);
                String directoryName = File_getDirectoryName(String_new(),absoluteFileName);
                String_delete(absoluteFileName);

                // get backup name
                uint n = 0;
                do
                {
                  String_set(saveDatabaseName,directoryName);
                  File_appendFileNameCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
                  String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
                  n++;
                }
                while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));

                // free resources
                String_delete(directoryName);
              }
              break;
            case DATABASE_TYPE_MARIADB:
              {
                // get backup name
                uint n = 0;
                do
                {
                  String_setCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
                  String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
                  n++;
                }
                while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));
              }
              break;
            case DATABASE_TYPE_POSTGRESQL:
              {
                // get backup name
                uint n = 0;
                do
                {
                  String_setCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
                  String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
                  n++;
                }
                while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));
              }
              break;
          }

          // rename database
          error = Database_rename(indexDatabaseSpecifier,NULL,String_cString(saveDatabaseName));
          if (error != ERROR_NONE)
          {
            String_delete(printableDatabaseURI);
            Database_deleteSpecifier(indexDatabaseSpecifier);
            indexDatabaseSpecifier = NULL;
            return error;
          }

          String_delete(saveDatabaseName);

          // upgrade version -> create new
          createFlag = TRUE;
          plogMessage(NULL,  // logHandle
                      LOG_TYPE_ERROR,
                      "INDEX",
                      "Old index database version %d in '%s' - create new",
                      indexVersion,
                      String_cString(printableDatabaseURI)
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
                    String_cString(printableDatabaseURI)
                   );
      }
    }
    else
    {
      // does not exists -> create new
      createFlag = TRUE;
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Index database '%s' does not exist - create new",
                  String_cString(printableDatabaseURI)
                 );
    }
  }

  if (!createFlag)
  {
    // check if database is outdated or corrupt
    if (Database_exists(indexDatabaseSpecifier,NULL))
    {
      DatabaseSpecifier indexDatabaseSpecifierReference;

      Database_copySpecifier(&indexDatabaseSpecifierReference,indexDatabaseSpecifier,NULL);
      switch (indexDatabaseSpecifier->type)
      {
        case DATABASE_TYPE_SQLITE3:
          #ifndef NDEBUG
            String_setCString(indexDatabaseSpecifierReference.sqlite.fileName,"/tmp/reference.db");
          #else
            String_setCString(indexDatabaseSpecifierReference.sqlite.fileName,"");
          #endif
          break;
        case DATABASE_TYPE_MARIADB:
          #if defined(HAVE_MARIADB)
            String_format(indexDatabaseSpecifierReference.mariadb.databaseName,"%s_tmp",DEFAULT_DATABASE_NAME);
          #else /* HAVE_MARIADB */
            Database_doneSpecifier(&indexDatabaseSpecifierReference);

            return ERROR_FUNCTION_NOT_SUPPORTED;
          #endif /* HAVE_MARIADB */
          break;
        case DATABASE_TYPE_POSTGRESQL:
          #if defined(HAVE_POSTGRESQL)
            String_format(indexDatabaseSpecifierReference.postgresql.databaseName,"%s_tmp",DEFAULT_DATABASE_NAME);
          #else /* HAVE_POSTGRESQL */
            Database_doneSpecifier(&indexDatabaseSpecifierReference);

            return ERROR_FUNCTION_NOT_SUPPORTED;
          #endif /* HAVE_POSTGRESQL */
          break;
      }

      Database_drop(&indexDatabaseSpecifierReference,NULL);
      error = openIndex(&indexHandleReference,&indexDatabaseSpecifierReference,NULL,INDEX_OPEN_MODE_CREATE,INDEX_TIMEOUT);
      if (error == ERROR_NONE)
      {
        error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ,INDEX_TIMEOUT);
        if (error == ERROR_NONE)
        {
          error = Database_compare(&indexHandleReference.databaseHandle,
                                   &indexHandle.databaseHandle,
                                   INDEX_CONST_REFERENCE_TABLE_NAMES,
                                   INDEX_CONST_REFERENCE_TABLE_NAME_COUNT,
                                   DATABASE_COMPARE_IGNORE_OBSOLETE|DATABASE_COMPARE_FLAG_INCLUDE_VIEWS
                                  );
          closeIndex(&indexHandle);
        }
        closeIndex(&indexHandleReference);
      }
      Database_drop(&indexDatabaseSpecifierReference,NULL);

      Database_doneSpecifier(&indexDatabaseSpecifierReference);

      if (error != ERROR_NONE)
      {
        // outdated or corrupt -> create new
        uint   n;
        String saveDatabaseName;

        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Outdated or corrupt index database '%s' (reason: %s) - create new",
                    String_cString(printableDatabaseURI),
                    Error_getText(error)
                   );

        saveDatabaseName = String_new();

        // get backup name
        n = 0;
        do
        {
          String_setCString(saveDatabaseName,DEFAULT_DATABASE_NAME);
          String_appendFormat(saveDatabaseName,DATABASE_SAVE_EXTENSIONS[indexDatabaseSpecifier->type],n);
          n++;
        }
        while (Database_exists(indexDatabaseSpecifier,saveDatabaseName));

        // rename database
        error = Database_rename(indexDatabaseSpecifier,NULL,String_cString(saveDatabaseName));
        if (error != ERROR_NONE)
        {
          String_delete(printableDatabaseURI);
          Database_deleteSpecifier(indexDatabaseSpecifier);
          indexDatabaseSpecifier = NULL;
          return error;
        }

        String_delete(saveDatabaseName);
      }
    }
  }

  if (createFlag)
  {
    // create new index database
    error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_CREATE,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      String_delete(printableDatabaseURI);
      Database_deleteSpecifier(indexDatabaseSpecifier);
      indexDatabaseSpecifier = NULL;
      return error;
    }
    closeIndex(&indexHandle);

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Created new index database '%s' (version %d)",
                String_cString(printableDatabaseURI),
                INDEX_VERSION
               );
  }
  else
  {
    // get database index version
    error = getIndexVersion(&indexVersion,indexDatabaseSpecifier);
    if (error != ERROR_NONE)
    {
      String_delete(printableDatabaseURI);
      Database_deleteSpecifier(indexDatabaseSpecifier);
      indexDatabaseSpecifier = NULL;
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Opened index database '%s' (version %d)",
                String_cString(printableDatabaseURI),
                indexVersion
               );
  }

  // initial clean-up
  error = openIndex(&indexHandle,indexDatabaseSpecifier,NULL,INDEX_OPEN_MODE_READ_WRITE,INDEX_TIMEOUT);
  if (error != ERROR_NONE)
  {
    String_delete(printableDatabaseURI);
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
    (void)cleanUpDuplicateMeta(&indexHandle);
    (void)cleanUpIncompleteUpdate(&indexHandle);
    (void)cleanUpIncompleteCreate(&indexHandle);
    (void)IndexStorage_cleanUp(&indexHandle);
    (void)IndexEntity_cleanUp(&indexHandle);
    (void)IndexUUID_cleanUp(&indexHandle);
  #endif /* INDEX_INTIIAL_CLEANUP */
  closeIndex(&indexHandle);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Done initial clean-up index database"
             );

  // start clean-up thread
  if (!Thread_init(&indexThread,"Index",0,indexThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize index thread!");
  }

  // free resources
  String_delete(printableDatabaseURI);

  return ERROR_NONE;
}

void Index_done(void)
{
  // stop threads
  indexQuitFlag = TRUE;
  if (!Thread_join(&indexThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop index thread!");
  }

  // free resources
  Thread_done(&indexThread);
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
Errors Index_open(IndexHandle *indexHandle,
                  ServerIO    *masterIO,
                  long        timeout
                 )
#else /* not NDEBUG */
Errors __Index_open(const char   *__fileName__,
                    ulong        __lineNb__,
                    IndexHandle *indexHandle,
                    ServerIO     *masterIO,
                    long         timeout
                   )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);

  if ((indexDatabaseSpecifier != NULL) || (masterIO != NULL))
  {
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
      return error;
    }

    #ifdef NDEBUG
      DEBUG_ADD_RESOURCE_TRACE(indexHandle,IndexHandle);
    #else /* not NDEBUG */
      DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,indexHandle,IndexHandle);
    #endif /* NDEBUG */
  }
  else
  {
    error = ERROR_DATABASE_NOT_FOUND;
  }

  return error;
}

void Index_close(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexHandle,IndexHandle);

  closeIndex(indexHandle);
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

bool Index_isLockPending(IndexHandle *indexHandle, DatabaseLockTypes lockType)
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

  if (indexHandle->masterIO == NULL)
  {
    // begin transaction
    #ifdef NDEBUG
      error = Database_beginTransaction(&indexHandle->databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,timeout);
    #else /* not NDEBUG */
      error = __Database_beginTransaction(__fileName__,__lineNb__,&indexHandle->databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,timeout);
    #endif /* NDEBUG */
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

Errors Index_endTransaction(IndexHandle *indexHandle)
{
  Errors error;

  assert(indexHandle != NULL);

  if (indexHandle->masterIO == NULL)
  {
    error = Database_endTransaction(&indexHandle->databaseHandle);
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

Errors Index_rollbackTransaction(IndexHandle *indexHandle)
{
  Errors error;

  assert(indexHandle != NULL);

  if (indexHandle->masterIO == NULL)
  {
    error = Database_rollbackTransaction(&indexHandle->databaseHandle);
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

Errors Index_flush(IndexHandle *indexHandle)
{
  Errors error;

  assert(indexHandle != NULL);

  if (indexHandle->masterIO == NULL)
  {
    error = Database_flush(&indexHandle->databaseHandle);
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

bool Index_containsType(const IndexId indexIds[],
                        uint          indexIdCount,
                        IndexTypes    indexType
                       )
{
  ulong i;

  assert(indexIds != NULL);

  for (i = 0; i < indexIdCount; i++)
  {
    if (INDEX_TYPE(indexIds[i]) == indexType)
    {
      return TRUE;
    }
  }

  return FALSE;
}

Errors Index_getInfos(IndexHandle   *indexHandle,
                      uint          *totalEntityCount,
                      uint          *totalDeletedEntityCount,

                      uint          *totalEntryCount,
                      uint64        *totalEntrySize,
                      uint64        *totalEntryContentSize,
                      uint          *totalFileCount,
                      uint64        *totalFileSize,
                      uint          *totalImageCount,
                      uint64        *totalImageSize,
                      uint          *totalDirectoryCount,
                      uint          *totalLinkCount,
                      uint          *totalHardlinkCount,
                      uint64        *totalHardlinkSize,
                      uint          *totalSpecialCount,

                      uint          *totalEntryCountNewest,
                      uint64        *totalEntrySizeNewest,
                      uint64        *totalEntryContentSizeNewest,
                      uint          *totalFileCountNewest,
                      uint64        *totalFileSizeNewest,
                      uint          *totalImageCountNewest,
                      uint64        *totalImageSizeNewest,
                      uint          *totalDirectoryCountNewest,
                      uint          *totalLinkCountNewest,
                      uint          *totalHardlinkCountNewest,
                      uint64        *totalHardlinkSizeNewest,
                      uint          *totalSpecialCountNewest,

                      uint          *totalSkippedEntryCount,

                      uint          *totalStorageCount,
                      uint64        *totalStorageSize,
                      uint          *totalDeletedStorageCount
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  if (totalEntityCount            != NULL) (*totalEntityCount           ) = 0;
  if (totalDeletedEntityCount     != NULL) (*totalDeletedEntityCount    ) = 0;

  if (totalEntryCount             != NULL) (*totalEntryCount            ) = 0;
  if (totalEntrySize              != NULL) (*totalEntrySize             ) = 0LL;
  if (totalEntryContentSize       != NULL) (*totalEntryContentSize      ) = 0LL;

  if (totalFileCount              != NULL) (*totalFileCount             ) = 0;
  if (totalFileSize               != NULL) (*totalFileSize              ) = 0LL;
  if (totalImageCount             != NULL) (*totalImageCount            ) = 0;
  if (totalImageSize              != NULL) (*totalImageSize             ) = 0LL;
  if (totalDirectoryCount         != NULL) (*totalDirectoryCount        ) = 0;
  if (totalLinkCount              != NULL) (*totalLinkCount             ) = 0;
  if (totalHardlinkCount          != NULL) (*totalHardlinkCount         ) = 0;
  if (totalHardlinkSize           != NULL) (*totalHardlinkSize          ) = 0LL;
  if (totalSpecialCount           != NULL) (*totalSpecialCount          ) = 0;

  if (totalEntryCountNewest       != NULL) (*totalEntryCountNewest      ) = 0;
  if (totalEntrySizeNewest        != NULL) (*totalEntrySizeNewest       ) = 0LL;
  if (totalEntryContentSizeNewest != NULL) (*totalEntryContentSizeNewest) = 0LL;

  if (totalFileCountNewest        != NULL) (*totalFileCountNewest       ) = 0;
  if (totalFileSizeNewest         != NULL) (*totalFileSizeNewest        ) = 0LL;
  if (totalImageCountNewest       != NULL) (*totalImageCountNewest      ) = 0;
  if (totalImageSizeNewest        != NULL) (*totalImageSizeNewest       ) = 0LL;
  if (totalDirectoryCountNewest   != NULL) (*totalDirectoryCountNewest  ) = 0;
  if (totalLinkCountNewest        != NULL) (*totalLinkCountNewest       ) = 0;
  if (totalHardlinkCountNewest    != NULL) (*totalHardlinkCountNewest   ) = 0;
  if (totalHardlinkSizeNewest     != NULL) (*totalHardlinkSizeNewest    ) = 0LL;
  if (totalSpecialCountNewest     != NULL) (*totalSpecialCountNewest    ) = 0;

  if (totalStorageCount           != NULL) (*totalStorageCount          ) = 0;
  if (totalStorageSize            != NULL) (*totalStorageSize           ) = 0LL;

  // init variables

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  error = ERROR_NONE;
  INDEX_DOX(error,
            indexHandle,
  {
    if (totalEntityCount != NULL)
    {
      // get total entities count
      error = Database_getUInt(&indexHandle->databaseHandle,
                               totalEntityCount,
                               "entities",
                               "COUNT(entities.id)",
                               "    lockedCount=0 \
                                AND deletedFlag!=TRUE \
                               ",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // orderGroup
                              );
      assert(   (error == ERROR_NONE)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
            );
    }

    if (totalDeletedEntityCount != NULL)
    {
      // get total deleted entities count
      error = Database_getUInt(&indexHandle->databaseHandle,
                               totalDeletedEntityCount,
                               "entities",
                               "COUNT(entities.id)",
                               "deletedFlag=TRUE",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // orderGroup
                              );
      assert(   (error == ERROR_NONE)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
            );
    }

    if (totalSkippedEntryCount != NULL)
    {
      // get total skipped entry count
      error = Database_getUInt(&indexHandle->databaseHandle,
                               totalSkippedEntryCount,
                               "skippedEntries",
                               "COUNT(id)",
                               NULL,
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // orderGroup
                              );
      assert(   (error == ERROR_NONE)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
            );
    }

    // get total * count/size, total newest count/size
    if (   (totalEntryCount             != NULL)
        || (totalEntrySize              != NULL)
        || (totalEntryContentSize       != NULL)
        || (totalFileCount              != NULL)
        || (totalFileSize               != NULL)
        || (totalImageCount             != NULL)
        || (totalImageSize              != NULL)
        || (totalDirectoryCount         != NULL)
        || (totalLinkCount              != NULL)
        || (totalHardlinkCount          != NULL)
        || (totalHardlinkSize           != NULL)
        || (totalSpecialCount           != NULL)
        || (totalEntryCountNewest       != NULL)
        || (totalEntrySizeNewest        != NULL)
        || (totalEntryContentSizeNewest != NULL)
        || (totalFileCountNewest        != NULL)
        || (totalFileSizeNewest         != NULL)
        || (totalImageCountNewest       != NULL)
        || (totalImageSizeNewest        != NULL)
        || (totalDirectoryCountNewest   != NULL)
        || (totalLinkCountNewest        != NULL)
        || (totalHardlinkCountNewest    != NULL)
        || (totalHardlinkSizeNewest     != NULL)
        || (totalSpecialCountNewest     != NULL)
        || (totalStorageCount           != NULL)
        || (totalStorageSize            != NULL)
       )
    {
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 26);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             if (totalEntryCount             != NULL) (*totalEntryCount            ) = values[ 0].u;
                             if (totalEntrySize              != NULL) (*totalEntrySize             ) = values[ 1].u64;
                             if (totalEntryContentSize       != NULL) (*totalEntryContentSize      ) = values[ 2].u64;

                             if (totalFileCount              != NULL) (*totalFileCount             ) = values[ 3].u;
                             if (totalFileSize               != NULL) (*totalFileSize              ) = values[ 4].u64;
                             if (totalImageCount             != NULL) (*totalImageCount            ) = values[ 5].u;
                             if (totalImageSize              != NULL) (*totalImageSize             ) = values[ 6].u64;
                             if (totalDirectoryCount         != NULL) (*totalDirectoryCount        ) = values[ 7].u;
                             if (totalLinkCount              != NULL) (*totalLinkCount             ) = values[ 8].u;
                             if (totalHardlinkCount          != NULL) (*totalHardlinkCount         ) = values[ 9].u;
                             if (totalHardlinkSize           != NULL) (*totalHardlinkSize          ) = values[10].u64;
                             if (totalSpecialCount           != NULL) (*totalSpecialCount          ) = values[11].u;

                             if (totalEntryCountNewest       != NULL) (*totalEntryCountNewest      ) = values[12].u;
                             if (totalEntrySizeNewest        != NULL) (*totalEntrySizeNewest       ) = values[13].u64;
                             if (totalEntryContentSizeNewest != NULL) (*totalEntryContentSizeNewest) = values[14].u64;

                             if (totalFileCountNewest        != NULL) (*totalFileCountNewest       ) = values[15].u;
                             if (totalFileSizeNewest         != NULL) (*totalFileSizeNewest        ) = values[16].u64;
                             if (totalImageCountNewest       != NULL) (*totalImageCountNewest      ) = values[17].u;
                             if (totalImageSizeNewest        != NULL) (*totalImageSizeNewest       ) = values[18].u64;
                             if (totalDirectoryCountNewest   != NULL) (*totalDirectoryCountNewest  ) = values[19].u;
                             if (totalLinkCountNewest        != NULL) (*totalLinkCountNewest       ) = values[20].u;
                             if (totalHardlinkCountNewest    != NULL) (*totalHardlinkCountNewest   ) = values[21].u;
                             if (totalHardlinkSizeNewest     != NULL) (*totalHardlinkSizeNewest    ) = values[22].u64;
                             if (totalSpecialCountNewest     != NULL) (*totalSpecialCountNewest    ) = values[23].u;

                             if (totalStorageCount           != NULL) (*totalStorageCount          ) = values[24].u;
                             if (totalStorageSize            != NULL) (*totalStorageSize           ) = values[25].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "storages"
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_UINT  ("SUM(totalEntryCount)"),
                             DATABASE_COLUMN_UINT64("SUM(totalEntrySize)"),
                             DATABASE_COLUMN_UINT  ("0"),

                             DATABASE_COLUMN_UINT  ("SUM(totalFileCount)"),
                             DATABASE_COLUMN_UINT64("SUM(totalFileSize)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalImageCount)"),
                             DATABASE_COLUMN_UINT64("SUM(totalImageSize)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalDirectoryCount)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalLinkCount)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalHardlinkCount)"),
                             DATABASE_COLUMN_UINT64("SUM(totalHardlinkSize)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalSpecialCount)"),

                             DATABASE_COLUMN_UINT  ("SUM(totalEntryCountNewest)"),
                             DATABASE_COLUMN_UINT64("SUM(totalEntrySizeNewest)"),
                             DATABASE_COLUMN_UINT  ("0"),

                             DATABASE_COLUMN_UINT  ("SUM(totalFileCountNewest)"),
                             DATABASE_COLUMN_UINT64("SUM(totalFileSizeNewest)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalImageCountNewest)"),
                             DATABASE_COLUMN_UINT64("SUM(totalImageSizeNewest)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalDirectoryCountNewest)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalLinkCountNewest)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalHardlinkCountNewest)"),
                             DATABASE_COLUMN_UINT64("SUM(totalHardlinkSizeNewest)"),
                             DATABASE_COLUMN_UINT  ("SUM(totalSpecialCountNewest)"),

                             DATABASE_COLUMN_UINT  ("COUNT(id)"),
                             DATABASE_COLUMN_UINT64("SUM(size)")
                           ),
                           "deletedFlag!=TRUE",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           1LL
                          );
      assert(   (error == ERROR_NONE)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
            );
    }

    if (totalDeletedStorageCount != NULL)
    {
      // get total deleted storage count
      error = Database_getUInt(&indexHandle->databaseHandle,
                               totalDeletedStorageCount,
                               "storages",
                               "COUNT(id)",
                               "deletedFlag=TRUE",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // orderGroup
                              );
      assert(   (error == ERROR_NONE)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
             || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
            );
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

      #if !defined(NDEBUG) && defined(HAVE_BACKTRACE)
        debugDumpStackTrace(stderr,
                            2,
                            DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                            threadInfo.stackTrace,
                            threadInfo.stackTraceSize,
                            0
                           );
      #endif /* !defined(NDEBUG) && defined(HAVE_BACKTRACE) */
    }
  }
}
#endif /* not INDEX_DEBUG_LOCK */

#ifdef __cplusplus
  }
#endif

/* end of file */
