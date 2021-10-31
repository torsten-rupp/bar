/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: database functions (SQLite3)
* Systems: all
*
\***********************************************************************/

#ifndef __DATABASE__
#define __DATABASE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <semaphore.h>
#include <assert.h>

#include "common/global.h"
#include "common/passwords.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/threads.h"
#include "common/semaphores.h"
#include "errors.h"

#include "sqlite3.h"
#include "mysql/mysql.h"

/****************** Conditional compilation switches *******************/
#define _DATABASE_LOCK_PER_INSTANCE   // if defined use lock per database instance, otherwise a global lock for all database is used

// switch on for debugging only!
#ifndef WERROR
#warning remove/revert
#endif
#define DATABASE_DEBUG_LOCK
#define _DATABASE_DEBUG_LOCK_PRINT
#define _DATABASE_DEBUG_TIMEOUT
#define _DATABASE_DEBUG_COPY_TABLE
#define _DATABASE_DEBUG_LOG SQLITE_TRACE_STMT

/***************************** Constants *******************************/

// database type
typedef enum
{
  DATABASE_TYPE_SQLITE3 = 0,
  DATABASE_TYPE_MYSQL   = 1
} DatabaseTypes;

// database specifier
typedef struct
{
  DatabaseTypes type;
  union
  {
    intptr_t p;
    struct
    {
      String fileName;
    } sqlite;
    struct
    {
      String   serverName;
      String   userName;
      Password password;
      String   databaseName;
    } mysql;
  };
} DatabaseSpecifier;

// database open mask
#define DATABASE_OPEN_MASK_MODE  0x0000000F
#define DATABASE_OPEN_MASK_FLAGS 0xFFFF0000

// database open modes
typedef enum
{
  DATABASE_OPENMODE_CREATE,
  DATABASE_OPENMODE_FORCE_CREATE,
  DATABASE_OPENMODE_READ,
  DATABASE_OPENMODE_READWRITE,
} DatabaseOpenModes;

// additional database open mode flags
#define DATABASE_OPENMODE_MEMORY (1 << 16)
#define DATABASE_OPENMODE_SHARED (1 << 17)
#define DATABASE_OPENMODE_AUX    (1 << 18)

// database lock types
typedef enum
{
  DATABASE_LOCK_TYPE_NONE,
  DATABASE_LOCK_TYPE_READ,
  DATABASE_LOCK_TYPE_READ_WRITE
} DatabaseLockTypes;

#define DATABASE_MAX_TABLE_COLUMNS      64
#define DATABASE_MAX_COLUMN_NAME_LENGTH 63

// database datatypes
typedef enum
{
  DATABASE_DATATYPE_NONE,

  DATABASE_DATATYPE_PRIMARY_KEY,
  DATABASE_DATATYPE_KEY,

  DATABASE_DATATYPE_BOOL,
  DATABASE_DATATYPE_INT,
// TODO:`
  DATABASE_DATATYPE_UINT = DATABASE_DATATYPE_INT,
// TODO:`
  DATABASE_DATATYPE_INT64,
  DATABASE_DATATYPE_UINT64 = DATABASE_DATATYPE_INT64,
  DATABASE_DATATYPE_DOUBLE,
  DATABASE_DATATYPE_DATETIME,
  DATABASE_DATATYPE_STRING,
  DATABASE_DATATYPE_CSTRING,
  DATABASE_DATATYPE_BLOB,

  DATABASE_DATATYPE_UNKNOWN
} DatabaseDataTypes;

#define DATABASE_FLAG_NONE    0
#define DATABASE_FLAG_IGNORE  (1 << 0)
#define DATABASE_FLAG_REPLACE (1 << 1)

// special database ids
#define DATABASE_ID_NONE  0x0000000000000000LL
#define DATABASE_ID_ANY   0xFFFFFFFFFFFFFFFFLL

// ordering mode
typedef enum
{
  DATABASE_ORDERING_NONE,
  DATABASE_ORDERING_ASCENDING,
  DATABASE_ORDERING_DESCENDING
} DatabaseOrdering;

// transaction types
typedef enum
{
  DATABASE_TRANSACTION_TYPE_DEFERRED,
  DATABASE_TRANSACTION_TYPE_IMMEDIATE,
  DATABASE_TRANSACTION_TYPE_EXCLUSIVE
} DatabaseTransactionTypes;

#ifndef NDEBUG
typedef enum
{
  DATABASE_HISTORY_TYPE_LOCK_READ,
  DATABASE_HISTORY_TYPE_LOCK_READ_WRITE,
  DATABASE_HISTORY_TYPE_UNLOCK
} DatabaseHistoryThreadInfoTypes;
#endif /* NDEBUG */

#define DATABASE_AUX "aux"

// ids of tempory table in "aux"
typedef enum
{
  DATABASE_TEMPORARY_TABLE1,
  DATABASE_TEMPORARY_TABLE2,
  DATABASE_TEMPORARY_TABLE3,
  DATABASE_TEMPORARY_TABLE4,
  DATABASE_TEMPORARY_TABLE5,
  DATABASE_TEMPORARY_TABLE6,
  DATABASE_TEMPORARY_TABLE7,
  DATABASE_TEMPORARY_TABLE8,
  DATABASE_TEMPORARY_TABLE9
} DatabaseTemporaryTableIds;

typedef enum
{
  DATABASE_CHECK_QUICK,
  DATABASE_CHECK_KEYS,
  DATABASE_CHECK_FULL
} DatabaseChecks;

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : Database_BusyHandler
* Purpose: database busy handler
* Input  : userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*DatabaseBusyHandlerFunction)(void *userData);

// database busy handler list
typedef struct DatabaseBusyHandlerNode
{
  LIST_NODE_HEADER(struct DatabaseBusyHandlerNode);

  DatabaseBusyHandlerFunction function;                   // busy handler function
  void                        *userData;

} DatabaseBusyHandlerNode;

typedef struct
{
  LIST_HEADER(DatabaseBusyHandlerNode);
  Semaphore lock;
} DatabaseBusyHandlerList;

/***********************************************************************\
* Name   : DatabaseProgressHandlerFunction
* Purpose: database progress handler
* Input  : userData - user data
* Output : -
* Return : TRUE to continue, FALSE to interrupt
* Notes  : -
\***********************************************************************/

typedef bool(*DatabaseProgressHandlerFunction)(void *userData);

// database progress handler list
typedef struct DatabaseProgressHandlerNode
{
  LIST_NODE_HEADER(struct DatabaseProgressHandlerNode);

  DatabaseProgressHandlerFunction function;               // progress handler function
  void                            *userData;

} DatabaseProgressHandlerNode;

typedef struct
{
  LIST_HEADER(DatabaseProgressHandlerNode);
  Semaphore lock;
} DatabaseProgressHandlerList;

#ifndef NDEBUG
  typedef struct
  {
    ThreadId   threadId;
    uint       count;
    const char *fileName;
    uint       lineNb;
    uint64     cycleCounter;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  } DatabaseThreadInfo;

  typedef struct
  {
    ThreadId                       threadId;
    const char                     *fileName;
    uint                           lineNb;
    uint64                         cycleCounter;
    DatabaseHistoryThreadInfoTypes type;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  } DatabaseHistoryThreadInfo;
#endif /* not NDEBUG */

// database list
typedef struct DatabaseNode
{
  LIST_NODE_HEADER(struct DatabaseNode);

  #ifdef DATABASE_LOCK_PER_INSTANCE
    pthread_mutex_t           lock;
  #endif /* DATABASE_LOCK_PER_INSTANCE */
  DatabaseSpecifier           databaseSpecifier;
  uint                        openCount;

  DatabaseLockTypes           lockType;
  uint                        pendingReadCount;
  uint                        readCount;
  pthread_cond_t              readTrigger;

  uint                        pendingReadWriteCount;
  uint                        readWriteCount;
  pthread_cond_t              readWriteTrigger;

  uint                        pendingTransactionCount;
  uint                        transactionCount;
  pthread_cond_t              transactionTrigger;

  DatabaseBusyHandlerList     busyHandlerList;
  DatabaseProgressHandlerList progressHandlerList;

  union
  {
    #ifdef HAVE_MYSQL
      MYSQL mysql;
    #endif /* HAVE_MYSQL */
  };

  // simple locking information: LWP ids only
  #ifdef DATABASE_DEBUG_LOCK
    ThreadLWPId readLPWIds[32];
    ThreadLWPId readWriteLPWIds[32];
    ThreadLWPId transactionLPWId;
  #endif /* DATABASE_DEBUG_LOCK */

  // full locking information
  #ifndef NDEBUG
    struct
    {
      // pending reads
      DatabaseThreadInfo pendingReads[32];
      // reads
      DatabaseThreadInfo reads[32];
      // pending read/writes
      DatabaseThreadInfo pendingReadWrites[32];
      // read/write
      ThreadId           readWriteLockedBy;
      DatabaseThreadInfo readWrites[32];
      struct
      {
        DatabaseThreadInfo threadInfo;
        DatabaseLockTypes  lockType;
        uint               pendingReadCount;
        uint               readCount;
        uint               pendingReadWriteCount;
        uint               readWriteCount;
        uint               pendingTransactionCount;
        uint               transactionCount;
        #ifdef HAVE_BACKTRACE
          void const *stackTrace[16];
          uint       stackTraceSize;
        #endif /* HAVE_BACKTRACE */
      }                     lastTrigger;
      // running transaction
      struct
      {
        ThreadId   threadId;
        const char *fileName;
        uint       lineNb;
        #ifdef HAVE_BACKTRACE
          void const *stackTrace[16];
          uint       stackTraceSize;
        #endif /* HAVE_BACKTRACE */
      }                     transaction;
      // history
      DatabaseHistoryThreadInfo history[32];
      uint                      historyIndex;
    } debug;
  #endif /* not NDEBUG */
} DatabaseNode;

typedef struct
{
  LIST_HEADER(DatabaseNode);

  Semaphore lock;
} DatabaseList;

// database id
typedef int64 DatabaseId;

// database handle
typedef struct DatabaseHandle
{
  #ifndef NDEBUG
    LIST_NODE_HEADER(struct DatabaseHandle);
  #endif /* not NDEBUG */

  DatabaseNode                *databaseNode;
  union
  {
    struct
    {
      sqlite3                 *handle;
    } sqlite;
    struct
    {
      MYSQL                   *handle;
    }
    mysql;
  };
  uint                        readLockCount;
  uint                        readWriteLockCount;
  uint                        transcationCount;
  long                        timeout;                    // timeout [ms]
  void                        *busyHandlerUserData;
  uint64                      lastCheckpointTimestamp;    // last time forced execution of a checkpoint
  sem_t                       wakeUp;                     // unlock wake-up
  DatabaseId                  lastInsertId;

  #ifndef NDEBUG
    struct
    {
      ThreadId                  threadId;                 // id of thread who opened/created database
      const char                *fileName;                // open/create location
      ulong                     lineNb;
      #ifdef HAVE_BACKTRACE
        void const              *stackTrace[16];
        int                     stackTraceSize;
      #endif /* HAVE_BACKTRACE */

      struct
      {
        ThreadId   threadId;                              // thread who aquired lock
        const char *fileName;
        uint       lineNb;
        char       text[8*1024];
        uint64     t0,t1;                                 // lock start/end timestamp [s]
      }                         locked;
      struct
      {
        String     sqlString;                             // current SQL string
        #ifdef HAVE_BACKTRACE
          void const *stackTrace[16];
          int        stackTraceSize;
        #endif /* HAVE_BACKTRACE */
      }                         current;
    } debug;
  #endif /* not NDEBUG */
} DatabaseHandle;

typedef char DatabaseColumnName[DATABASE_MAX_COLUMN_NAME_LENGTH+1];

// databaes column
typedef struct
{
  const char        *name;
  DatabaseDataTypes type;
} DatabaseColumn;

// database value
typedef struct
{
  const char        *name;
  DatabaseDataTypes type;
  union
  {
    intptr_t p;

    uint64   id;
    bool     b;
    int      i;
    int64    i64;
    uint32   u;
    uint64   u64;
    double   d;
    uint64   dateTime;
    String   string;
    char     *s;
// TODO: use String?
    struct
    {
      char  *data;
      ulong length;
    } text;
    struct
    {
      void  *data;
      ulong length;
    } blob;
// TODO: remove
    struct
    {
      void  *p;
      ulong length;
    } data;
  };
} DatabaseValue;

// database filter
typedef struct
{
  DatabaseDataTypes type;
  union
  {
    intptr_t p;

    uint64   id;
    bool     b;
    int      i;
    int64    i64;
    uint32   u;
    uint64   u64;
    double   d;
    uint64   dateTime;
    String   string;
    char     *s;
// TODO: use String?
    struct
    {
      char  *data;
      ulong length;
    } text;
    struct
    {
      void  *data;
      ulong length;
    } blob;
// TODO: remove
    struct
    {
      void  *p;
      ulong length;
    } data;
  };
} DatabaseFilter;

// database statement handle
typedef struct
{
  DatabaseHandle    *databaseHandle;
  union
  {
    struct
    {
      sqlite3_stmt      *statementHandle;
      DatabaseValue     **bind;
    }
    sqlite;
    struct
    {
      MYSQL_STMT        *statementHandle;
DatabaseDataTypes *dataTypes;
      struct
      {
        MYSQL_BIND      *bind;
        MYSQL_TIME      *time;
      }                 values;
      struct
      {
        MYSQL_BIND      *bind;
        MYSQL_TIME      *time;
      }                 results;
      bool xxxexecutedFlag;
    }
    mysql;
  };

//  const DatabaseValue **values;
  uint                valueCount;
  uint                valueIndex;

#if 0
  const DatabaseFilter **filters;
  uint                filterCount;
  uint                filterIndex;
#endif

  DatabaseValue       *results;
  uint                resultCount;
  uint                resultIndex;

  uint          *valueMap;
  uint          valueMapCount;

//  DatabaseValue *results;
  #ifndef NDEBUG
    String sqlString;
    uint64 t0,t1;
    uint64 dt;
  #endif /* not NDEBUG */
} DatabaseStatementHandle;

/***********************************************************************\
* Name   : DatabaseRowFunction
* Purpose: execute row callback function
* Input  : values     - column values
*          valueCount - number of values
*          userData   - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseRowFunction)(const DatabaseValue values[], uint valueCount, void *userData);

// tables columns
typedef struct
{
  const DatabaseColumnName *names;
  DatabaseValue            *values;
  uint                     count;
} DatabaseColumns;

/***********************************************************************\
* Name   : DatabaseCopyTableFunction
* Purpose: execute copy table row callback function
* Input  : fromColumnList - from column list
*          toColumnList   - to column list
*          userData       - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseCopyTableFunction)(DatabaseColumns *fromColumns,
                                           DatabaseColumns *toColumns,
                                           void            *userData
                                          );

/***********************************************************************\
* Name   : DatabaseCopyPauseCallbackFunction
* Purpose: call back to check for pausing table copy
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef bool(*DatabaseCopyPauseCallbackFunction)(void *userData);

/***********************************************************************\
* Name   : DatabaseCopyProgressCallbackFunction
* Purpose: call back to report progress of table copy
* Input  : userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*DatabaseCopyProgressCallbackFunction)(void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define __DATABASE_COLUMN_TYPE(type) DATABASE_DATATYPE_ ## type,
#define DATABASE_COLUMN_TYPES(...) \
  (DatabaseDataTypes[]){_ITERATOR_EVAL(_ITERATOR_MAP(__DATABASE_COLUMN_TYPE, __VA_ARGS__))}, \
  _ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0

#define DATABASE_VALUES(...) \
  (DatabaseValue[]){ __VA_ARGS__ }, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/3


// column macros
#define _DATABASE_COLUMNS_ITERATOR_ARRAY0(x,...) \
  _ITERATOR_EVAL1(DATABASE_COLUMN_NAME_TYPE_##x), \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__DATABASE_COLUMNS_ITERATOR_ARRAY0)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __DATABASE_COLUMNS_ITERATOR_ARRAY0() _DATABASE_COLUMNS_ITERATOR_ARRAY0

#define _DATABASE_COLUMNS_ITERATOR_ARRAY1(x,...) \
  _ITERATOR_EVAL1(DATABASE_COLUMN_NAME_##x), \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__DATABASE_COLUMNS_ITERATOR_ARRAY1)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __DATABASE_COLUMNS_ITERATOR_ARRAY1() _DATABASE_COLUMNS_ITERATOR_ARRAY1

#define DATABASE_COLUMNS(...) \
  (DatabaseColumn[]){_ITERATOR_EVAL(_DATABASE_COLUMNS_ITERATOR_ARRAY0(__VA_ARGS__))}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)


#define DATABASE_COLUMN_NAME_TYPE_KEY(name)     { name, DATABASE_DATATYPE_KEY     }
#define DATABASE_COLUMN_NAME_TYPE_BOOL(name)    { name, DATABASE_DATATYPE_BOOL    }
#define DATABASE_COLUMN_NAME_TYPE_INT(name)     { name, DATABASE_DATATYPE_INT     }
#define DATABASE_COLUMN_NAME_TYPE_INT64(name)   { name, DATABASE_DATATYPE_INT64   }
#define DATABASE_COLUMN_NAME_TYPE_UINT(name)    { name, DATABASE_DATATYPE_UINT    }
#define DATABASE_COLUMN_NAME_TYPE_UINT64(name)  { name, DATABASE_DATATYPE_UINT64  }
#define DATABASE_COLUMN_NAME_TYPE_STRING(name)  { name, DATABASE_DATATYPE_STRING  }
#define DATABASE_COLUMN_NAME_TYPE_CSTRING(name) { name, DATABASE_DATATYPE_CSTRING }

#define DATABASE_COLUMN_NAME_KEY(name)     name
#define DATABASE_COLUMN_NAME_BOOL(name)    name
#define DATABASE_COLUMN_NAME_INT(name)     name
#define DATABASE_COLUMN_NAME_INT64(name)   name
#define DATABASE_COLUMN_NAME_UINT(name)    name
#define DATABASE_COLUMN_NAME_UINT64(name)  name
#define DATABASE_COLUMN_NAME_STRING (name) name
#define DATABASE_COLUMN_NAME_CSTRING(name) name

// value macros
#define __DATABASE_ITERATOR_VALUES_TYPE_VALUE(x,...) \
  _ITERATOR_EVAL1(DATABASE_VALUE_TYPE_VALUE_##x), \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(___DATABASE_ITERATOR_VALUES_TYPE_VALUE)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define ___DATABASE_ITERATOR_VALUES_TYPE_VALUE() __DATABASE_ITERATOR_VALUES_TYPE_VALUE

#define _DATABASE_ITERATOR_VALUES_TYPE_VALUE(...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_EVAL(__DATABASE_ITERATOR_VALUES_TYPE_VALUE(__VA_ARGS__)) \
  ) \
  ( \
    /* nothing to do */ \
  )

#define __DATABASE_ITERATOR_VALUES_NAME(x,...) \
  _ITERATOR_EVAL1(DATABASE_VALUE_NAME_##x), \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(___DATABASE_ITERATOR_VALUES_NAME)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define ___DATABASE_ITERATOR_VALUES_NAME() __DATABASE_ITERATOR_VALUES_NAME

#define _DATABASE_ITERATOR_VALUES_NAME(...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_EVAL(__DATABASE_ITERATOR_VALUES_NAME(__VA_ARGS__)) \
  ) \
  ( \
    /* nothing to do */ \
  )


#define DATABASE_VALUES2(...) \
  (const char*[]){_ITERATOR_EVAL(_DATABASE_ITERATOR_VALUES_NAME(__VA_ARGS__))}, \
  (DatabaseValue[]){_ITERATOR_EVAL(_DATABASE_ITERATOR_VALUES_TYPE_VALUE(__VA_ARGS__))}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)

#define DATABASE_VALUE_TYPE_VALUE_KEY(name,value)      { name, DATABASE_DATATYPE_KEY,      { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_BOOL(name,value)     { name, DATABASE_DATATYPE_BOOL,     { (intptr_t)((value)==true) } }
#define DATABASE_VALUE_TYPE_VALUE_INT(name,value)      { name, DATABASE_DATATYPE_INT,      { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_UINT(name,value)     { name, DATABASE_DATATYPE_UINT,     { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_INT64(name,value)    { name, DATABASE_DATATYPE_INT64,    { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_UINT64(name,value)   { name, DATABASE_DATATYPE_UINT64,   { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_DATETIME(name,value) { name, DATABASE_DATATYPE_DATETIME, { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_STRING(name,value)   { name, DATABASE_DATATYPE_STRING,   { (intptr_t)(value) } }
#define DATABASE_VALUE_TYPE_VALUE_CSTRING(name,value)  { name, DATABASE_DATATYPE_CSTRING,  { (intptr_t)(value) } }

#define DATABASE_VALUE_NAME_KEY(name,value)      name
#define DATABASE_VALUE_NAME_BOOL(name,value)     name
#define DATABASE_VALUE_NAME_INT(name,value)      name
#define DATABASE_VALUE_NAME_INT64(name,value)    name
#define DATABASE_VALUE_NAME_UINT(name,value)     name
#define DATABASE_VALUE_NAME_UINT64(name,value)   name
#define DATABASE_VALUE_NAME_DATETIME(name,value) name
#define DATABASE_VALUE_NAME_STRING(name,value)   name
#define DATABASE_VALUE_NAME_CSTRING(name,value)  name

#define DATABASE_VALUES2_NONE (const char**)NULL,(DatabaseValue*)NULL,0

// filter macros
#define __DATABASE_FILTERS_ITERATOR_ARRAY0(x,...) \
  _ITERATOR_EVAL1(DATABASE_FILTER_TYPE_VALUE_##x), \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(___DATABASE_FILTERS_ITERATOR_ARRAY0)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define ___DATABASE_FILTERS_ITERATOR_ARRAY0() __DATABASE_FILTERS_ITERATOR_ARRAY0
#define _DATABASE_FILTERS_ITERATOR_ARRAY0(...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_EVAL(__DATABASE_FILTERS_ITERATOR_ARRAY0(__VA_ARGS__)) \
  ) \
  ( \
    /* nothing to do */ \
  )

#define DATABASE_FILTERS(...) \
  (DatabaseFilter[]){_ITERATOR_EVAL(_DATABASE_FILTERS_ITERATOR_ARRAY0(__VA_ARGS__))}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)

#define DATABASE_FILTER_TYPE_VALUE_KEY(value)      { DATABASE_DATATYPE_KEY,      { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_BOOL(value)     { DATABASE_DATATYPE_BOOL,     { (intptr_t)((value)==true) } }
#define DATABASE_FILTER_TYPE_VALUE_INT(value)      { DATABASE_DATATYPE_INT,      { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_UINT(value)     { DATABASE_DATATYPE_UINT,     { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_INT64(value)    { DATABASE_DATATYPE_INT64,    { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_UINT64(value)   { DATABASE_DATATYPE_UINT64,   { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_DATETIME(value) { DATABASE_DATATYPE_DATETIME, { (intptr_t)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_STRING(value)   { DATABASE_DATATYPE_STRING,   { (intptr_t)(void*)(value) } }
#define DATABASE_FILTER_TYPE_VALUE_CSTRING(value)  { DATABASE_DATATYPE_CSTRING,  { (intptr_t)(value) } }

#define DATABASE_FILTERS_NONE (const char*)NULL,(DatabaseFilter*)NULL,0

/***********************************************************************\
* Name   : DATABASE_LOCKED_DO
* Purpose: execute block with database locked
* Input  : databaseHandle    - database handle
*          semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            DATABASE_LOCKED_DO(databaseHandle,SEMAPHORE_LOCK_TYPE_READ,1000)
*            {
*              ...
*            }
\***********************************************************************/

#define DATABASE_LOCKED_DO(databaseHandle,semaphoreLockType,timeout) \
  for (bool __databaseLock ## __COUNTER__ = Database_lock(databaseHandle,semaphoreLockType,timeout); \
       __databaseLock ## __COUNTER__; \
       Database_unlock(databaseHandle,semaphoreLockType), __databaseLock ## __COUNTER__ = FALSE \
      )

/***********************************************************************\
* Name   : DATABASE_TRANSACTION_DO
* Purpose: execute block with database transaction
* Input  : databaseHandle          - database handle
*          databaseTransactionType - transaction type; see
*                                    DatabaseTransactionTypes
*          timeout                 - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,1000)
*            {
*              ...
*            }
\***********************************************************************/

#define DATABASE_TRANSACTION_DO(databaseHandle,databaseTransactionType,timeout) \
  for (bool __databaseTransaction ## __COUNTER__ = (Database_beginTransaction(databaseHandle,databaseTransactionType,timeout) == ERROR_NONE); \
       __databaseTransaction ## __COUNTER__; \
       Database_endTransaction(databaseHandle), __databaseTransaction ## __COUNTER__ = FALSE \
      )

//TODO: rollback
#define DATABASE_TRANSACTION_ABORT(databaseHandle) \
  continue

#ifndef NDEBUG
  #define Database_open(...)                __Database_open               (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_close(...)               __Database_close              (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_lock(...)                __Database_lock               (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_unlock(...)              __Database_unlock             (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_beginTransaction(...)    __Database_beginTransaction   (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_endTransaction(...)      __Database_endTransaction     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_rollbackTransaction(...) __Database_rollbackTransaction(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_prepare(...)             __Database_prepare            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_finalize(...)            __Database_finalize           (__FILE__,__LINE__, ## __VA_ARGS__)

  #define Database_debugPrintQueryInfo(...) __Database_debugPrintQueryInfo(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Database_initAll
* Purpose: init database
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_initAll(void);

/***********************************************************************\
* Name   : Database_doneAll
* Purpose: done database
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_doneAll(void);

/***********************************************************************\
* Name   : Database_parseSpecifier
* Purpose: init and parse datatabase URI into database specifier
* Input  : databaseSpecifier - database specifier variable
*          uriString         - database URI string:
*                                - [(sqlite|sqlite3):]file name, NULL for
*                                  sqlite3 "in memory",
*                                - mysql:<server>:<user>:<database>
* Output : databaseSpecifier - database specifier
* Return : -
* Notes  : -
\***********************************************************************/

void Database_parseSpecifier(DatabaseSpecifier *databaseSpecifier,
                             const char        *uriString
                            );

/***********************************************************************\
* Name   : Database_copySpecifier
* Purpose: copy database specifier
* Input  : databaseSpecifier     - database specifier variable
*          fromDatabaseSpecifier - from database specifier variable
*          newDatabaseName       - new database name (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_copySpecifier(DatabaseSpecifier       *databaseSpecifier,
                            const DatabaseSpecifier *fromDatabaseSpecifier
                           );

/***********************************************************************\
* Name   : Database_doneSpecifier
* Purpose: done database specifier
* Input  : databaseSpecifier - database specifier
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_doneSpecifier(DatabaseSpecifier *databaseSpecifier);

/***********************************************************************\
* Name   : Database_newSpecifier
* Purpose: allocate and parse datatabase URI
* Input  : uriString - database URI string:
*                        - [(sqlite|sqlite3):]file name, NULL for
*                          sqlite3 "in memory",
*                        - mysql:<server>:<user>:<database>
* Output : -
* Return : database specifier or NULL
* Notes  : -
\***********************************************************************/

DatabaseSpecifier *Database_newSpecifier(const char *uriString);

/***********************************************************************\
* Name   : Database_duplicateSpecifier
* Purpose: duplicate database specifier
* Input  : databaseSpecifier - database specifier
* Output : -
* Return : duplicate database specifier
* Notes  : -
\***********************************************************************/

DatabaseSpecifier *Database_duplicateSpecifier(const DatabaseSpecifier *databaseSpecifier);

/***********************************************************************\
* Name   : Database_doneSpecifier
* Purpose: done database specifier
* Input  : databaseSpecifier - database specifier
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_deleteSpecifier(DatabaseSpecifier *databaseSpecifier);

/***********************************************************************\
* Name   : Database_equalSpecifiers
* Purpose: compare database specifiers if equals
* Input  : databaseSpecifier0,databaseSpecifier1 - database specifiers
* Output : -
* Return : TRUE iff equals (except passwords)
* Notes  : -
\***********************************************************************/

bool Database_equalSpecifiers(const DatabaseSpecifier *databaseSpecifier0, const DatabaseSpecifier *databaseSpecifier1);

/***********************************************************************\
* Name   : Database_exists
* Purpose: check if database exists
* Input  : databaseSpecifier - database specifier
*          databaseName      - database name or NULL for database name in
*                              specifier
* Output : -
* Return : TRUE iff exists
* Notes  : -
\***********************************************************************/

bool Database_exists(const DatabaseSpecifier *databaseSpecifier,
                     ConstString             databaseName
                    );

/***********************************************************************\
* Name   : Database_rename
* Purpose: rename database
* Input  : databaseSpecifier - database specifier
*          newDatabaseName   - new database name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_rename(DatabaseSpecifier *databaseSpecifier,
                       ConstString       newDatabaseName
                      );

//Database_delete(indexDatabaseSpecifier,databaseName);

/***********************************************************************\
* Name   : Database_open
* Purpose: open database
* Input  : databaseHandle   - database handle variable
*          uri              - database URI:
*                               - [(sqlite|sqlite3):]file name, NULL for
*                                 sqlite3 "in memory",
*                               - mysql:<server>:<user>[:<database>]
*          databaseOpenMode - open mode; see DatabaseOpenModes
*          timeout          - timeout [ms] or WAIT_FOREVER
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle          *databaseHandle,
                       const DatabaseSpecifier *databaseSpecifier,
                       DatabaseOpenModes       databaseOpenMode,
                       long                    timeout
                      );
#else /* not NDEBUG */
  Errors __Database_open(const char              *__fileName__,
                         ulong                   __lineNb__,
                         DatabaseHandle          *databaseHandle,
                         const DatabaseSpecifier *databaseSpecifier,
                         DatabaseOpenModes       databaseOpenMode,
                         long                    timeout
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_close
* Purpose: close database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_close(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  void __Database_close(const char     *__fileName__,
                        ulong          __lineNb__,
                        DatabaseHandle *databaseHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_getType
* Purpose: get database type
* Input  : databaseHandle - database handle
* Output : -
* Return : database type; see DATABASE_TYPE_...
* Notes  : -
\***********************************************************************/

INLINE DatabaseTypes Database_getType(const DatabaseHandle *databaseHandle);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE DatabaseTypes Database_getType(const DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  assert(databaseHandle->databaseNode != NULL);

  return databaseHandle->databaseNode->databaseSpecifier.type;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_addBusyHandler
* Purpose: add database busy handler
* Input  : databaseHandle      - database handle
*          busyHandlerFunction - busy handler function
*          busyHandlerUserData - user data for busy handler functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_addBusyHandler(DatabaseHandle              *databaseHandle,
                             DatabaseBusyHandlerFunction busyHandlerFunction,
                             void                        *busyHandlerUserData
                            );

/***********************************************************************\
* Name   : Database_removeBusyHandler
* Purpose: remove database busy handler
* Input  : databaseHandle      - database handle
*          busyHandlerFunction - busy handler function
*          busyHandlerUserData - user data for busy handler functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_removeBusyHandler(DatabaseHandle              *databaseHandle,
                                DatabaseBusyHandlerFunction busyHandlerFunction,
                                void                        *busyHandlerUserData
                               );

/***********************************************************************\
* Name   : Database_addProgressHandler
* Purpose: add database progress handler
* Input  : databaseHandle          - database handle
*          progressHandlerFunction - progress handler function
*          progressHandlerUserData - user data for progress handler
*                                    functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_addProgressHandler(DatabaseHandle                  *databaseHandle,
                                 DatabaseProgressHandlerFunction progressHandlerFunction,
                                 void                            *progressHandlerUserData
                                );

/***********************************************************************\
* Name   : Database_removeProgressHandler
* Purpose: remove database progress handler
* Input  : databaseHandle          - database handle
*          progressHandlerFunction - progress handler function
*          progressHandlerUserData - user data for progress handler
*                                    functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_removeProgressHandler(DatabaseHandle                  *databaseHandle,
                                    DatabaseProgressHandlerFunction progressHandlerFunction,
                                    void                            *progressHandlerUserData
                                   );

/***********************************************************************\
* Name   : Database_interrupt
* Purpose: interrupt currently running database command
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_interrupt(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_getTableList
* Purpose: get table list
* Input  : tableList      - table list variable
*          databaseHandle - database handle
* Output : tableList - table list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getTableList(StringList     *tableList,
                             DatabaseHandle *databaseHandle
                            );

/***********************************************************************\
* Name   : Database_getIndexList
* Purpose: get index list
* Input  : indexList      - index list variable
*          databaseHandle - database handle
*          tableName      - table name (can be NULL)
* Output : indexList - index list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getIndexList(StringList     *indexList,
                             DatabaseHandle *databaseHandle,
                             const char     *tableName
                            );

/***********************************************************************\
* Name   : Database_getTriggerList
* Purpose: get trigger list
* Input  : triggerList    - trigger list variable
*          databaseHandle - database handle
* Output : triggerList - trigger list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getTriggerList(StringList     *triggerList,
                               DatabaseHandle *databaseHandle
                              );

//TODO: remove
#if 0
/***********************************************************************\
* Name   : Database_request
* Purpose: request long-run database access
* Input  : databaseHandle - database handle
*          timeout        - timeout request long-run [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Database_request(DatabaseHandle *databaseHandle, ulong timeout);

/***********************************************************************\
* Name   : Database_release
* Purpose: release long-run database access
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_release(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_yield
* Purpose: yield long-run database access if access with higher priority
*          is pending
* Input  : databaseHandle - database handle
* Output : yieldStart     - yield start call-back code (can be NULL)
*          userDataStart  - yield start user data
*          yieldEnd       - yield end call-back code (can be NULL)
*          userDataEnd    - yield end user data
* Return : -
* Notes  : -
\***********************************************************************/

void Database_yield(DatabaseHandle *databaseHandle,
                    void           (*yieldStart)(void*),
                    void           *userDataStart,
                    void           (*yieldEnd)(void*),
                    void           *userDataEnd
                   );
#endif

/***********************************************************************\
* Name   : Database_lock
* Purpose: lock database exclusive
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_...
*          timeout        - timeout [ms[ or WAIT_FOREVER
* Output : -
* Return : TRUE iff locked
* Notes  : lock is aquired for all database handles sharing the same
*          database file
\***********************************************************************/

#ifdef NDEBUG
  bool Database_lock(DatabaseHandle    *databaseHandle,
                     DatabaseLockTypes lockType,
                     long              timeout
                    );
#else /* not NDEBUG */
  bool __Database_lock(const char        *__fileName__,
                       ulong             __lineNb__,
                       DatabaseHandle    *databaseHandle,
                       DatabaseLockTypes lockType,
                       long              timeout
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_unlock
* Purpose: unlock database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_unlock(DatabaseHandle    *databaseHandle,
                       DatabaseLockTypes lockType
                      );
#else /* not NDEBUG */
  void __Database_unlock(const char        *__fileName__,
                         ulong             __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         DatabaseLockTypes lockType
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_isLocked
* Purpose: check if database is locked
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

INLINE bool Database_isLocked(DatabaseHandle    *databaseHandle,
                              DatabaseLockTypes lockType
                             );
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE bool Database_isLocked(DatabaseHandle    *databaseHandle,
                              DatabaseLockTypes lockType
                             )
{
  bool isLocked;

  assert(databaseHandle != NULL);
  assert(databaseHandle->databaseNode != NULL);

  isLocked = FALSE;
  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_NONE:
      break;
    case DATABASE_LOCK_TYPE_READ:
      isLocked = (databaseHandle->readLockCount > 0);
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      isLocked = (databaseHandle->databaseNode->readWriteCount > 0);
      break;
  }

  return isLocked;
}
#endif /* NDEBUG || __DATABASE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_isLockPending
* Purpose: check if database lock is pending
* Input  : databaseHandle - database handle
*          lockType       - lock type; see SEMAPHORE_LOCK_TYPE_*
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

bool Database_isLockPending(DatabaseHandle     *databaseHandle,
                            SemaphoreLockTypes lockType
                           );

/***********************************************************************\
* Name   : Database_setEnabledSync
* Purpose: enable/disable synchronous mode
* Input  : databaseHandle - database handle
*          enabled        - TRUE to enable synchronous mode
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_setEnabledSync(DatabaseHandle *databaseHandle,
                               bool           enabled
                              );

/***********************************************************************\
* Name   : Database_setEnabledForeignKeys
* Purpose: enable/disable foreign key checks
* Input  : databaseHandle - database handle
*          enabled        - TRUE to enable foreign key checks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_setEnabledForeignKeys(DatabaseHandle *databaseHandle,
                                      bool           enabled
                                     );

/***********************************************************************\
* Name   : Database_setTmpDirectory
* Purpose: set directory for temporary files
* Input  : databaseHandle - database handle
*          directoryName  - directory name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_setTmpDirectory(DatabaseHandle *databaseHandle,
                                const char     *directoryName
                               );

/***********************************************************************\
* Name   : Database_createTemporaryTable
* Purpose: create temporary database table
* Input  : databaseHandle - database handle
*          id             - table id
*          definition     - table definition
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_createTemporaryTable(DatabaseHandle            *databaseHandle,
                                     DatabaseTemporaryTableIds id,
                                     const char                *definition
                                    );

/***********************************************************************\
* Name   : Database_dropTemporaryTable
* Purpose: drop temporary database table
* Input  : databaseHandle - database handle
*          id             - table id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTemporaryTable(DatabaseHandle            *databaseHandle,
                                   DatabaseTemporaryTableIds id
                                  );

/***********************************************************************\
* Name   : Database_dropTables
* Purpose: drop tables
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTables(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_dropIndices
* Purpose: drop indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropIndices(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_dropTriggers
* Purpose: drop triggers
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTriggers(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_compare
* Purpose: compare database structure
* Input  : databaseHandleReference - reference database handle
*          databaseHandle          - database handle
* Output : -
* Return : ERROR_NONE if databases equals or mismatch code
* Notes  : -
\***********************************************************************/

Errors Database_compare(DatabaseHandle *databaseHandleReference,
                        DatabaseHandle *databaseHandle
                       );

/***********************************************************************\
* Name   : Database_copyTable
* Purpose: copy table content
* Input  : fromDatabaseHandle           - from-database handle
*          toDatabaseHandle             - fo-database handle
*          fromTableName                - from-table name
*          toTableName                  - to-table name
*          transactionFlag              - copy with transaction
*          duration                     - duration variable or NULL
*          preCopyTableFunction         - pre-copy call-back function
*          preCopyTableUserData         - user data for pre-copy
*                                         call-back
*          postCopyTableFunction        - pre-copy call-back function
*          postCopyTableUserData        - user data for post-copy
*                                         call-back
*          copyPauseCallbackFunction    - pause call-back
*          copyPauseCallbackUserData    - user data for pause call-back
*          copyProgressCallbackFunction - pause call-back
*          copyProgressCallbackUserData - user data for pause call-back
*          fromAdditional               - additional SQL condition
*          ...                          - optional arguments for
*                                         additional SQL condition
* Output : duration - duration [ms]
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_copyTable(DatabaseHandle                       *fromDatabaseHandle,
                          DatabaseHandle                       *toDatabaseHandle,
                          const char                           *fromTableName,
                          const char                           *toTableName,
                          bool                                 transactionFlag,
                          uint64                               *duration,
                          DatabaseCopyTableFunction            preCopyTableFunction,
                          void                                 *preCopyTableUserData,
                          DatabaseCopyTableFunction            postCopyTableFunction,
                          void                                 *postCopyTableUserData,
                          DatabaseCopyPauseCallbackFunction    copyPauseCallbackFunction,
                          void                                 *copyPauseCallbackUserData,
                          DatabaseCopyProgressCallbackFunction copyProgressCallbackFunction,
                          void                                 *copyProgressCallbackUserData,
                          const char                           *fromAdditional,
                          ...
                         );

/***********************************************************************\
* Name   : Database_getTableColumn*
* Purpose: get table column
* Input  : columns      - columns
*          columnName   - column name
*          defaultValue - default value
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

DatabaseId Database_getTableColumnId(DatabaseColumns *columns, const char *columnName, DatabaseId defaultValue);
int Database_getTableColumnInt(DatabaseColumns *columns, const char *columnName, int defaultValue);
uint Database_getTableColumnUInt(DatabaseColumns *columns, const char *columnName, uint defaultValue);
int64 Database_getTableColumnInt64(DatabaseColumns *columns, const char *columnName, int64 defaultValue);
uint64 Database_getTableColumnUInt64(DatabaseColumns *columns, const char *columnName, uint64 defaultValue);
double Database_getTableColumnDouble(DatabaseColumns *columns, const char *columnName, double defaultValue);
uint64 Database_getTableColumnDateTime(DatabaseColumns *columns, const char *columnName, uint64 defaultValue);
String Database_getTableColumnString(DatabaseColumns *columns, const char *columnName, String value, const char *defaultValue);
const char *Database_getTableColumnCString(DatabaseColumns *columns, const char *columnName, const char *defaultValue);
void Database_getTableColumnBlob(DatabaseColumns *columns, const char *columnName, void *data, uint length);

/***********************************************************************\
* Name   : Database_setTableColumn*
* Purpose: set table column
* Input  : columns    - columns
*          columnName - column name
*          value      - value
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Database_setTableColumnId(DatabaseColumns *columns, const char *columnName, DatabaseId value);
bool Database_setTableColumnBool(DatabaseColumns *columns, const char *columnName, bool value);
bool Database_setTableColumnInt64(DatabaseColumns *columns, const char *columnName, int64 value);
bool Database_setTableColumnDouble(DatabaseColumns *columns, const char *columnName, double value);
bool Database_setTableColumnDateTime(DatabaseColumns *columns, const char *columnName, uint64 value);
bool Database_setTableColumnString(DatabaseColumns *columns, const char *columnName, ConstString value);
bool Database_setTableColumnCString(DatabaseColumns *columns, const char *columnName, const char *value);
bool Database_setTableColumnBlob(DatabaseColumns *columns, const char *columnName, const void *data, uint length);

/***********************************************************************\
* Name   : Database_addColumn
* Purpose: add column to table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          columnDataType - column data type
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_addColumn(DatabaseHandle    *databaseHandle,
                          const char        *tableName,
                          const char        *columnName,
                          DatabaseDataTypes columnDataType
                         );

/***********************************************************************\
* Name   : Database_addColumn
* Purpose: remove column from table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_removeColumn(DatabaseHandle *databaseHandle,
                             const char     *tableName,
                             const char     *columnName
                            );

/***********************************************************************\
* Name   : Database_beginTransaction
* Purpose: begin transaction
* Input  : databaseHandle          - database handle
*          databaseTransactionType - transaction type; see
*                                    DATABASE_TRANSACTION_TYPE_*
*          timeout                 - timeout [ms] or WAIT_FOREVER (still
*                                    not used)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_beginTransaction(DatabaseHandle           *databaseHandle,
                                   DatabaseTransactionTypes databaseTransactionType,
                                   long                     timeout
                                  );
#else /* not NDEBUG */
  Errors __Database_beginTransaction(const char               *__fileName__,
                                     uint                     __lineNb__,
                                     DatabaseHandle           *databaseHandle,
                                     DatabaseTransactionTypes databaseTransactionType,
                                     long                     timeout
                                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_endTransaction
* Purpose: end transaction (commit)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_endTransaction(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  Errors __Database_endTransaction(const char     *__fileName__,
                                   uint           __lineNb__,
                                   DatabaseHandle *databaseHandle
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_rollbackTransaction
* Purpose: rollback transcation (discard)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_rollbackTransaction(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  Errors __Database_rollbackTransaction(const char     *__fileName__,
                                        uint           __lineNb__,
                                        DatabaseHandle *databaseHandle
                                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_flush
* Purpose: flush database
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_flush(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_valueToString, Database_valueToCString
* Purpose: convert database value to string
* Input  : string        - string variable
*          buffer        - buffer
*          bufferSize    - size of buffer
*          databaseValue - database value
* Output : -
* Return : string/buffer
* Notes  : -
\***********************************************************************/

String Database_valueToString(String string, const DatabaseValue *databaseValue);
const char *Database_valueToCString(char *buffer, uint bufferSize, const DatabaseValue *databaseValue);

/***********************************************************************\
* Name   : Database_execute, Database_vexecute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
*          columnTypes         - result column types; use macro
*                                DATABASE_COLUMN_TYPES()
*          columnTypeCount     - number of result columns
*          command             - SQL command string with %[l]d, %[']S,
*                                %[']s
*          ...                 - optional arguments for SQL command
*                                string
*                                special functions:
*                                  REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_execute(DatabaseHandle          *databaseHandle,
                        DatabaseRowFunction     databaseRowFunction,
                        void                    *databaseRowUserData,
                        ulong                   *changedRowCount,
                        const DatabaseDataTypes columnTypes[],
                        uint                    columnTypeCount,
                        const char              *command,
                        ...
                       );

Errors Database_vexecute(DatabaseHandle          *databaseHandle,
                         DatabaseRowFunction     databaseRowFunction,
                         void                    *databaseRowUserData,
                         ulong                   *changedRowCount,
                         const DatabaseDataTypes columnTypes[],
                         uint                    columnTypeCount,
                         const char              *command,
                         va_list                 arguments
                       );

/***********************************************************************\
* Name   : Database_prepare
* Purpose: prepare database query
* Input  : databaseHandle - database handle
*          command        - SQL command string with %[l]d, %[']S, %[']s
*          ...            - optional arguments for SQL command string
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : databaseStatementHandle - initialized database statement handle
* Return : -
* Notes  : Database is locked until Database_finalize() is called
\***********************************************************************/

// TODO: comment
#ifdef NDEBUG
  Errors Database_prepare(DatabaseStatementHandle *databaseStatementHandle,
                          DatabaseHandle          *databaseHandle,
                          const DatabaseDataTypes *resultDataTypes,
                          uint                    resultDataTypeCount,
                          const char              *sqlCommand,
                          const char              *names[],
                          const DatabaseValue     values[],
                          uint                    nameValueCount,
                          const DatabaseFilter    filters[],
                          uint                    filterCount
                         );
#else /* not NDEBUG */
  Errors __Database_prepare(const char              *__fileName__,
                            ulong                   __lineNb__,
                            DatabaseStatementHandle *databaseStatementHandle,
                            DatabaseHandle          *databaseHandle,
                            const DatabaseDataTypes *resultDataTypes,
                            uint                    resultDataTypeCount,
                            const char              *sqlCommand,
                            const char              *names[],
                            const DatabaseValue     values[],
                            uint                    nameValueCount,
                            const DatabaseFilter    filters[],
                            uint                    filterCount
                           );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_getNextRow
* Purpose: get next row from query result
* Input  : databaseStatementHandle - database statment handle
*          format - format string
* Output : ... - variables
* Return : TRUE if row read, FALSE if not more rows
* Notes  : Support format types:
*            %b              - bool
*            %[<n>][(ll|l)\d - int
*            %[<n>][(ll|l)\u - unsigned int
*            %[<n>][l\f      - float/double
*            %c              - char
*            %[<n>]s         - char*
*            %S              - string
*            %p              - pointer
\***********************************************************************/

bool Database_getNextRow(DatabaseStatementHandle *databaseStatementHandle,
                         const char              *format,
                         ...
                        );

/***********************************************************************\
* Name   : Database_insert
* Purpose: insert row into database table
* Input  : databaseHandle   - database handle
*          changedRowCount  - row count variable (can be NULL)
*          tableName        - table name,
*          flags            - insert flags; see DATABASE_FLAGS_...
*          filter           - SQL filter expression
*          filterValues     - filter values
*          filterValueCount - filter values count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_insert(DatabaseHandle      *databaseHandle,
                       ulong               *changedRowCount,
                       const char          *tableName,
                       uint                flags,
                       const char          *names[],
                       const DatabaseValue values[],
                       uint                nameValueCount
                      );

/***********************************************************************\
* Name   : Database_insert
* Purpose: insert row into database table
* Input  : databaseHandle   - database handle
*          changedRowCount  - row count variable (can be NULL)
*          flags            - insert flags; see DATABASE_FLAGS_...
*          values           - values to insert
*          valueCount       - value count
*          filter           - SQL filter
*          filterValues     - values to insert
*          filterValueCount - value count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_update(DatabaseHandle       *databaseHandle,
                       ulong                *changedRowCount,
                       const char           *tableName,
                       uint                 flags,
                       const char           *names[],
                       const DatabaseValue  values[],
                       uint                 nameValueCount,
                       const char           *filter,
                       const DatabaseFilter filterValues[],
                       uint                 filterValueCount
                      );

/***********************************************************************\
* Name   : Database_delete
* Purpose: delete rows from database table
* Input  : databaseHandle   - database handle
*          changedRowCount  - row count variable (can be NULL)
*          tableName        - table name,
*          flags            - insert flags; see DATABASE_FLAGS_...
*          filter           - SQL filter expression
*          filterValues     - filter values
*          filterValueCount - filter values count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_delete(DatabaseHandle *databaseHandle,
                       ulong          *changedRowCount,
                       const char     *tableName,
                       uint           flags,
                       const char     *filter,
                       DatabaseValue  filterValues[],
                       uint           filterValueCount
                      );

/***********************************************************************\
* Name   : Database_select
* Purpose: select rows in database table
* Input  : databaseHandle      - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
*          tableName           - table name,
*          flags               - insert flags; see DATABASE_FLAGS_...
*          values              - values
*          valueCount          - values couont
*          filter              - SQL filter expression
*          filterValues        - filter values
*          filterValueCount    - filter values count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_select(DatabaseHandle      *databaseHandle,
                       DatabaseRowFunction databaseRowFunction,
                       void                *databaseRowUserData,
                       ulong               *changedRowCount,
                       const char          *tableName,
                       uint                flags,
// TODO: select value datatype: name, type
                       DatabaseValue       selectValues[],
                       uint                selectValueCount,
                       const char          *filter,
                       DatabaseValue       filterValues[],
                       uint                filterValueCount
                      );
Errors Database_select2(DatabaseHandle      *databaseHandle,
                       DatabaseRowFunction databaseRowFunction,
                       void                *databaseRowUserData,
                       ulong               *changedRowCount,
                       const char          *tableName,
                       uint                flags,
// TODO: select value datatype: name, type
                       DatabaseColumn      selectColumn[],
                       uint                selectColumnCount,
                       const char          *filter,
                       DatabaseFilter      filterValues[],
                       uint                filterValueCount
                      );

/***********************************************************************\
* Name   : Database_finalize
* Purpose: done database query
* Input  : databaseStatementHandle - database statement handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_finalize(DatabaseStatementHandle *databaseStatementHandle);
#else /* not NDEBUG */
  void __Database_finalize(const char        *__fileName__,
                           ulong             __lineNb__,
                           DatabaseStatementHandle *databaseStatementHandle
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_exists
* Purpose: check if value exists database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : TRUE iff value exists
* Notes  : -
\***********************************************************************/

bool Database_existsValue(DatabaseHandle *databaseHandle,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );

/***********************************************************************\
* Name   : Database_getId, Database_vgetId
* Purpose: get database id of value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - database id or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getId(DatabaseHandle *databaseHandle,
                      DatabaseId     *value,
                      const char     *tableName,
                      const char     *columnName,
                      const char     *additional,
                      ...
                     );
Errors Database_vgetId(DatabaseHandle *databaseHandle,
                       DatabaseId     *value,
                       const char     *tableName,
                       const char     *columnName,
                       const char     *additional,
                       va_list        arguments
                      );

/***********************************************************************\
* Name   : Database_getIds, Database_vgetIds
* Purpose: get database ids from database table
* Input  : databaseHandle - database handle
*          values         - database ids array
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : values - database ids array
* Return : ERROR_NONE or error code
* Notes  : values are added to array!
\***********************************************************************/

Errors Database_getIds(DatabaseHandle *databaseHandle,
                       Array          *values,
                       const char     *tableName,
                       const char     *columnName,
                       const char     *additional,
                       ...
                      );
Errors Database_vgetIds(DatabaseHandle *databaseHandle,
                        Array          *values,
                        const char     *tableName,
                        const char     *columnName,
                        const char     *additional,
                        va_list        arguments
                       );

/***********************************************************************\
* Name   : Database_getMaxId, Database_vgetMaxId
* Purpose: get max. database id of value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - max. database id or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getMaxId(DatabaseHandle *databaseHandle,
                         DatabaseId     *value,
                         const char     *tableName,
                         const char     *columnName,
                         const char     *additional,
                         ...
                        );
Errors Database_vgetMaxId(DatabaseHandle *databaseHandle,
                          DatabaseId     *value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          va_list        arguments
                         );

/***********************************************************************\
* Name   : Database_getInt
* Purpose: get int value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInt(DatabaseHandle       *databaseHandle,
                       int                 *value,
                       const char           *tableName,
                       const char           *columnName,
                       const char           *filter,
                       const DatabaseFilter filterValues[],
                       uint                 filterValueCount
                      );

/***********************************************************************\
* Name   : Database_setInt
* Purpose: insert or update int value in database table
* Input  : databaseHandle - database handle
*          value          - int64 value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setInt(DatabaseHandle       *databaseHandle,
                       const char           *tableName,
                       uint                 flags,
                       const char           *columnName,
                       int                  value,
                       const char           *filter,
                       const DatabaseFilter filterValues[],
                       uint                 filterValueCount
                      );

/***********************************************************************\
* Name   : Database_getUInt
* Purpose: get uint value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getUInt(DatabaseHandle       *databaseHandle,
                        uint                 *value,
                        const char           *tableName,
                        const char           *columnName,
                        const char           *filter,
                        const DatabaseFilter filterValues[],
                        uint                 filterValueCount
                       );

/***********************************************************************\
* Name   : Database_setUint
* Purpose: insert or update uint value in database table
* Input  : databaseHandle - database handle
*          value          - int64 value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setUInt(DatabaseHandle       *databaseHandle,
                        const char           *tableName,
                        uint                 flags,
                        const char           *columnName,
                        uint                 value,
                        const char           *filter,
                        const DatabaseFilter filterValues[],
                        uint                 filterValueCount
                       );

/***********************************************************************\
* Name   : Database_getInt64
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInt64(DatabaseHandle       *databaseHandle,
                         int64                *value,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filterValues[],
                         uint                 filterValueCount
                        );
// TODO: replace by Database_getInt64
Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            );
Errors Database_vgetInteger64(DatabaseHandle *databaseHandle,
                              int64          *value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             );

/***********************************************************************\
* Name   : Database_setInteger64, Database_vsetInteger64
* Purpose: insert or update int64 value in database table
* Input  : databaseHandle - database handle
*          value          - int64 value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setInt64(DatabaseHandle       *databaseHandle,
                         const char           *tableName,
                         uint                 flags,
                         const char           *columnName,
                         int64                value,
                         const char           *filter,
                         const DatabaseFilter filterValues[],
                         uint                 filterValueCount
                        );
// TODO: rename to Database_setInt64
Errors Database_setInteger64(DatabaseHandle *databaseHandle,
                             int64          value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            );
Errors Database_vsetInteger64(DatabaseHandle *databaseHandle,
                              int64          value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             );

/***********************************************************************\
* Name   : Database_getUInt64
* Purpose: get uint64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getUInt64(DatabaseHandle       *databaseHandle,
                          uint64               *value,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filterValues[],
                          uint                 filterValueCount
                         );

/***********************************************************************\
* Name   : Database_setUInt64
* Purpose: insert or update uint64 value in database table
* Input  : databaseHandle - database handle
*          value          - int64 value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setUInt64(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          uint64               value,
                          const char           *filter,
                          const DatabaseFilter filterValues[],
                          uint                 filterValueCount
                         );

/***********************************************************************\
* Name   : Database_getDouble, Database_vgetDouble
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - double value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getDouble(DatabaseHandle *databaseHandle,
                          double         *value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_vgetDouble(DatabaseHandle *databaseHandle,
                           double         *value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_setDouble, Database_vsetDouble
* Purpose: insert or update double value in database table
* Input  : databaseHandle - database handle
*          value          - double value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setDouble(DatabaseHandle *databaseHandle,
                          double         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_vsetDouble(DatabaseHandle *databaseHandle,
                           double         value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_getString, Database_vgetString
* Purpose: get string value from database table
* Input  : databaseHandle  - database handle
*          tableName       - table name
*          string          - string variable
*          maxStringLength - max. length of string
*          columnName      - column name
*          additional      - additional string (e. g. WHERE...)
*                            special functions:
*                              REGEXP(pattern,case-flag,text)
* Output : string - string value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getString(DatabaseHandle *databaseHandle,
                          String         string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_getCString(DatabaseHandle *databaseHandle,
                           char           *string,
                           uint           maxStringLength,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           ...
                          );
Errors Database_vgetString(DatabaseHandle *databaseHandle,
                           String         string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );
Errors Database_vgetCString(DatabaseHandle *databaseHandle,
                            char           *string,
                            uint           maxStringLength,
                            const char     *tableName,
                            const char     *columnName,
                            const char     *additional,
                            va_list        arguments
                           );

/***********************************************************************\
* Name   : Database_setString, Database_vsetString
* Purpose: insert or update string value in database table
* Input  : databaseHandle - database handle
*          string         - string value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setString(DatabaseHandle *databaseHandle,
                          const String   string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_vsetString(DatabaseHandle *databaseHandle,
                           const String   string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_getLastRowId
* Purpose: get row id of last insert command
* Input  : databaseHandle - database handle
* Output : -
* Return : row id
* Notes  : -
\***********************************************************************/

INLINE DatabaseId Database_getLastRowId(DatabaseHandle *databaseHandle);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE DatabaseId Database_getLastRowId(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  return databaseHandle->lastInsertId;
}
#endif /* NDEBUG || __CONFIGURATION_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_check
* Purpose: check database
* Input  : databaseHandle - database handle
*          databaseCheck  - database check to execute
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_check(DatabaseHandle *databaseHandle, DatabaseChecks databaseCheck);

/***********************************************************************\
* Name   : Database_reindex
* Purpose: recreate all database indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_reindex(DatabaseHandle *databaseHandle);

#ifdef DATABASE_DEBUG_LOCK
/***********************************************************************\
* Name   : Database_debugPrintSimpleLockInfo
* Purpose: print debug simple lock info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_debugPrintSimpleLockInfo(void);
#endif /* DATABASE_DEBUG_LOCK */

#ifndef NDEBUG

/***********************************************************************\
* Name   : Database_debugEnable
* Purpose: enable/disable debug output
* Input  : enabled - TRUE to enable debug output
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

void Database_debugEnable(DatabaseHandle *databaseHandle, bool enabled);

/***********************************************************************\
* Name   : Database_debugPrintInfo
* Purpose: print debug info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_debugPrintInfo(void);

/***********************************************************************\
* Name   : Database_debugPrintInfo
* Purpose: print debug lock info
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_debugPrintLockInfo(const DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_debugPrintQueryInfo
* Purpose: print query info
* Input  : databaseStatementHandle - database statement handle
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

#ifdef NDEBUG
void Database_debugPrintQueryInfo(const DatabaseStatementHandle *databaseStatementHandle);
#else /* not NDEBUG */
void __Database_debugPrintQueryInfo(const char *__fileName__, ulong __lineNb__, const DatabaseStatementHandle *databaseStatementHandle);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_debugDumpTable
* Purpose: dump database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          showHeaderFlag - TRUE to dump header
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

void Database_debugDumpTable(DatabaseHandle *databaseHandle, const char *tableName, bool showHeaderFlag);

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __DATABASE__ */

/* end of file */
