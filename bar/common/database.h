/***********************************************************************\
*
* Contents: database functions (SQLite3, MariaDB, PostgreSQL)
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

#include "sqlite3.h"
#ifdef HAVE_MARIADB_MYSQL_H
  #include "mariadb/mysql.h"
#endif
#ifdef HAVE_LIBPQ_FE_H
  #include "libpq-fe.h"
#endif

#include "common/arrays.h"
#include "common/dictionaries.h"
#include "common/global.h"
#include "common/hashtables.h"
#include "common/passwords.h"
#include "common/semaphores.h"
#include "common/stringlists.h"
#include "common/strings.h"
#include "common/threads.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/
#define _DATABASE_LOCK_PER_INSTANCE   // if defined use lock per database instance, otherwise a global lock for all database is used

// switch on for debugging only!
#define _DATABASE_DEBUG_LOCK
#define _DATABASE_DEBUG_LOCK_PRINT
#define _DATABASE_DEBUG_TIMEOUT
#define _DATABASE_DEBUG_COPY_TABLE
#define _DATABASE_DEBUG_LOG SQLITE_TRACE_STMT

/***************************** Constants *******************************/

// database type
typedef enum
{
  DATABASE_TYPE_SQLITE3,
  DATABASE_TYPE_MARIADB,
  DATABASE_TYPE_POSTGRESQL
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
    }
    sqlite;
    #if defined(HAVE_MARIADB)
    struct
    {
      String   serverName;
      String   userName;
      Password password;
      String   databaseName;
    }
    mariadb;
    #endif /* HAVE_MARIADB */
    #if defined(HAVE_POSTGRESQL)
    struct
    {
      String   serverName;
      String   userName;
      Password password;
      String   databaseName;
    }
    postgresql;
    #endif /* HAVE_POSTGRESQL */
  };
} DatabaseSpecifier;

// database open mask
#define DATABASE_OPEN_MASK_MODE  0x0000000F
#define DATABASE_OPEN_MASK_FLAGS 0xFFFF0000

// database open modes
typedef enum
{
  DATABASE_OPEN_MODE_CREATE,
  DATABASE_OPEN_MODE_FORCE_CREATE,
  DATABASE_OPEN_MODE_READ,
  DATABASE_OPEN_MODE_READWRITE,
} DatabaseOpenModes;

// additional database open mode flags
#define DATABASE_OPEN_MODE_MEMORY (1 << 16)
#define DATABASE_OPEN_MODE_SHARED (1 << 17)
#define DATABASE_OPEN_MODE_AUX    (1 << 18)

// database lock types
typedef enum
{
  DATABASE_LOCK_TYPE_NONE,
  DATABASE_LOCK_TYPE_READ,
  DATABASE_LOCK_TYPE_READ_WRITE
} DatabaseLockTypes;

#define DATABASE_MAX_TABLE_COLUMNS      64
#define DATABASE_MAX_COLUMN_NAME_LENGTH 63

#define DATABASE_UNLIMITED 9223372036854775807LL

// database datatypes
typedef enum
{
  DATABASE_DATATYPE_NONE,

  DATABASE_DATATYPE,

  DATABASE_DATATYPE_PRIMARY_KEY,
  DATABASE_DATATYPE_KEY,

  DATABASE_DATATYPE_BOOL,
  DATABASE_DATATYPE_INT,
  DATABASE_DATATYPE_INT64,
  DATABASE_DATATYPE_UINT,
  DATABASE_DATATYPE_UINT64,
  DATABASE_DATATYPE_DOUBLE,
  DATABASE_DATATYPE_ENUM,
  DATABASE_DATATYPE_DATETIME,
  DATABASE_DATATYPE_STRING,
  DATABASE_DATATYPE_CSTRING,
  DATABASE_DATATYPE_BLOB,

  DATABASE_DATATYPE_ARRAY,
  DATABASE_DATATYPE_FTS,

  DATABASE_DATATYPE_UNKNOWN,
} DatabaseDataTypes;

#define DATABASE_FLAG_NONE         0
#define DATABASE_FLAG_IGNORE       (1 <<  0)
#define DATABASE_FLAG_REPLACE      (1 <<  1)
#define DATABASE_FLAG_PLAIN        (1 <<  2)
#define DATABASE_FLAG_FETCH_ALL    (1 <<  3)
#define DATABASE_FLAG_COLUMN_NAMES (1 <<  4)

#define DATABASE_FLAG_DEBUG        (1 << 31)  // print SQL statement to console

// special database ids
#define DATABASE_ID_NONE ((DatabaseId)0x0000000000000000LL)
#define DATABASE_ID_ANY  ((DatabaseId)0xFFFFFFFFFFFFFFFFLL)

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

#define DATABASE_COMPARE_FLAG_NONE          0
#define DATABASE_COMPARE_IGNORE_OBSOLETE    (1 << 0)
#define DATABASE_COMPARE_FLAG_INCLUDE_VIEWS (1 << 1)

// database check types
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

// locked by info
typedef struct
{
// TODO: remove volatile
  volatile ThreadId threadId;
  #ifndef NDEBUG
    volatile const char *fileName;
    volatile ulong      lineNb;
  #endif
} DatabaseLockedBy;

// database list
typedef struct DatabaseNode
{
  LIST_NODE_HEADER(struct DatabaseNode);

  #ifdef DATABASE_LOCK_PER_INSTANCE
    pthread_mutex_t             lock;
    DatabaseLockedBy            lockedBy;
  #endif /* DATABASE_LOCK_PER_INSTANCE */
  DatabaseSpecifier           databaseSpecifier;
  uint                        openCount;

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
      DatabaseThreadInfo        pendingReads[32];
      // reads
      DatabaseThreadInfo        reads[32];
      // pending read/writes
      DatabaseThreadInfo        pendingReadWrites[32];
      // read/write
      ThreadId                  readWriteLockedBy;
      DatabaseThreadInfo        readWrites[32];
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
      }                         lastTrigger;
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
      }                         transaction;
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
    #if defined(HAVE_MARIADB)
    struct
    {
      MYSQL                   *handle;
    }
    mariadb;
    #endif /* HAVE_MARIADB */
    #if defined(HAVE_POSTGRESQL)
    struct
    {
      PGconn                  *handle;
      HashTable               sqlStringHashTable;
    }
    postgresql;
    #endif /* HAVE_POSTGRESQL */
  };
  uint                        readLockCount;
  uint                        readWriteLockCount;
  uint                        transactionCount;
  long                        timeout;                    // timeout [ms]
  void                        *busyHandlerUserData;
  bool                        enabledSync;
  bool                        enabledForeignKeys;
  uint64                      lastCheckpointTimestamp;    // last time forced execution of a checkpoint
  sem_t                       wakeUp;                     // unlock wake-up

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

// TODO: remove
//typedef char DatabaseColumnName[DATABASE_MAX_COLUMN_NAME_LENGTH+1];

// database column
typedef struct
{
  const char        *name;
  const char        *alias;
  DatabaseDataTypes type;
} DatabaseColumn;

// database parameter (value without name)
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
} DatabaseParameter;

// database value with name
typedef struct
{
  const char        *name;   // column name
  const char        *value;  // value, e. g. <column name>
  DatabaseDataTypes type;
  union
  {
    intptr_t   p;

    DatabaseId id;
    bool       b;
    int        i;
    int64      i64;
    uint32     u;
    uint64     u64;
    double     d;
    uint64     dateTime;
    String     string;
    const char *s;
    struct
    {
      void  *data;
      ulong length;
    }          blob;
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
  const void *data;
  ulong      length;
} DatabaseFilterBlob;

typedef struct
{
  const void *data;
  ulong      length;
  uint       elementSize;
  const char *(*toString)(const void *data);
} DatabaseFilterArray;

typedef struct
{
  DatabaseDataTypes type;
  union
  {
    intptr_t            p;

    DatabaseId          id;
    bool                b;
    int                 i;
    int64               i64;
    uint32              u;
    uint64              u64;
    double              d;
    uint64              dateTime;
    ConstString         string;
    const char          *s;
    DatabaseFilterBlob  blob;
    DatabaseFilterArray array;
  };
} DatabaseFilter;

typedef struct
{
  sqlite3_stmt      *statementHandle;
  DatabaseValue     **bind;
} SQLiteStatement;

#if defined(HAVE_MARIADB)
  typedef struct
  {
    MYSQL_STMT        *statementHandle;
    struct
    {
      MYSQL_BIND      *bind;
      MYSQL_TIME      *time;
    }                 values;
    struct
    {
      MYSQL_BIND      *bind;
      MYSQL_TIME      *time;
      unsigned long   *lengths;
    }                 results;
  } MySQLStatement;
#endif /* HAVE_MARIADB */

#if defined(HAVE_POSTGRESQL)
  // database statement handle
  typedef union
  {
    char       *p;

    DatabaseId id;
    uint8      b;
    int32      i;
    int64      i64;
    uint32     u;
    uint64     u64;
    double     d;
    uint64     dateTime;
    char       data[64];
  } PostgreSQLBind;

  typedef struct
  {
    HashTableEntry *hashTableEntry;
    char           name[1+16+1];

    PostgreSQLBind *bind;
    const char     **parameterValues;
    int            *parameterLengths;
    int            *parameterFormats;

    PGresult       *result;
    ulong          rowIndex;
    ulong          rowCount;
  } PostgresSQLStatement;
#endif /* HAVE_POSTGRESQL */

typedef struct
{
  DatabaseHandle *databaseHandle;
  union
  {
    SQLiteStatement sqlite;
    #if defined(HAVE_MARIADB)
      MySQLStatement mariadb;
    #endif /* HAVE_MARIADB */
    #if defined(HAVE_POSTGRESQL)
      PostgresSQLStatement postgresql;
    #endif /* HAVE_POSTGRESQL */
  };

  // values+filters
  uint           parameterCount;
  uint           parameterIndex;

  // results
  char           **columnNames;
  DatabaseValue  *results;
  uint           resultCount;
  uint           resultIndex;

  uint           *valueMap;
  uint           valueMapCount;

  DatabaseId     lastInsertId;

  #ifndef NDEBUG
    struct
    {
      String sqlString;
      #ifdef HAVE_BACKTRACE
        void const *stackTrace[16];
        int        stackTraceSize;
      #endif /* HAVE_BACKTRACE */
      uint64 t0,t1;
      uint64 dt;
    } debug;
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

// table column info
typedef struct
{
  DatabaseValue        *values;
  uint                 count;
} DatabaseColumnInfo;

/***********************************************************************\
* Name   : DatabaseCopyTableFunction
* Purpose: execute copy table row callback function
* Input  : fromColumnInfo - from column list
*          toColumnList   - to column list
*          userData       - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseCopyTableFunction)(DatabaseColumnInfo *fromColumns,
                                           DatabaseColumnInfo *toColumns,
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

#define DATABASE_PLAIN(sqlString) \
  (const char*[]){sqlString}, \
  1, \
  DATABASE_FLAG_PLAIN|DATABASE_FLAG_FETCH_ALL

#define DATABASE_TABLES(...) \
  (const char*[]){__VA_ARGS__}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/1

// column type macros
#define __DATABASE_COLUMN_TYPE(type) DATABASE_DATATYPE_ ## type,
#define DATABASE_COLUMN_TYPES(...) \
  (DatabaseDataTypes[]){__VA_ARGS__}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/1

// column macros
#define DATABASE_COLUMNS(...) \
  (DatabaseColumn[]){__VA_ARGS__}, \
  (_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/3

// Note: NULL (expand to "((void *)0)") cannot be used with _ITERATOR_MAP_COUNT because of the space and braces; use 0 instead
#define DATABASE_COLUMN_KEY(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_KEY \
    ) \
    ( \
      0, DATABASE_DATATYPE_KEY \
    ) \
  }
#define DATABASE_COLUMN_BOOL(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_BOOL \
    ) \
    ( \
      0, DATABASE_DATATYPE_BOOL \
    ) \
  }
#define DATABASE_COLUMN_INT(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_INT \
    ) \
    ( \
      0, DATABASE_DATATYPE_INT \
    ) \
  }
#define DATABASE_COLUMN_INT64(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_INT64 \
    ) \
    ( \
      0, DATABASE_DATATYPE_INT64 \
    ) \
  }
#define DATABASE_COLUMN_UINT(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_UINT \
    ) \
    ( \
      0, DATABASE_DATATYPE_UINT \
    ) \
  }
#define DATABASE_COLUMN_UINT64(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_UINT64 \
    ) \
    ( \
      0, DATABASE_DATATYPE_UINT64 \
    ) \
  }
#define DATABASE_COLUMN_DOUBLE(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_DOUBLE \
    ) \
    ( \
      0, DATABASE_DATATYPE_DOUBLE \
    ) \
  }
#define DATABASE_COLUMN_ENUM(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_ENUM \
    ) \
    ( \
      0, DATABASE_DATATYPE_ENUM \
    ) \
  }
#define DATABASE_COLUMN_DATETIME(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_DATETIME \
    ) \
    ( \
      0, DATABASE_DATATYPE_DATETIME \
    ) \
  }
#define DATABASE_COLUMN_STRING(name,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      __VA_ARGS__, DATABASE_DATATYPE_STRING \
    ) \
    ( \
      0, DATABASE_DATATYPE_STRING \
    ) \
  }

#define DATABASE_COLUMNS_NONE (DatabaseColumn*)NULL,0
#define DATABASE_COLUMNS_AUTO (DatabaseColumn*)NULL,0

// parameter macros
#define DATABASE_PARAMETERS(...) \
  (DatabaseParameter[]){__VA_ARGS__}, \
  ((_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/2)

#define DATABASE_PARAMETER_KEY(value)      { DATABASE_DATATYPE_KEY,      { (intptr_t)(value) } }
#define DATABASE_PARAMETER_BOOL(value)     { DATABASE_DATATYPE_BOOL,     { (intptr_t)((value) == true) } }
#define DATABASE_PARAMETER_INT(value)      { DATABASE_DATATYPE_INT,      { (intptr_t)(value) } }
#define DATABASE_PARAMETER_UINT(value)     { DATABASE_DATATYPE_UINT,     { (intptr_t)(value) } }
#define DATABASE_PARAMETER_INT64(value)    { DATABASE_DATATYPE_INT64,    { (intptr_t)(value) } }
#define DATABASE_PARAMETER_UINT64(value)   { DATABASE_DATATYPE_UINT64,   { (intptr_t)(value) } }
#define DATABASE_PARAMETER_DOUBLE(value)   { DATABASE_DATATYPE_DOUBLE,   { (intptr_t)(value) } }
#define DATABASE_PARAMETER_DATETIME(value) { DATABASE_DATATYPE_DATETIME, { (intptr_t)(value) } }
#define DATABASE_PARAMETER_STRING(value)   { DATABASE_DATATYPE_STRING,   { (intptr_t)(void*)(value) } }
#define DATABASE_PARAMETER_CSTRING(value)  { DATABASE_DATATYPE_CSTRING,  { (intptr_t)(value) } }

#define DATABASE_PARAMETERS_NONE (DatabaseParameter*)NULL,0

// value macros
#define DATABASE_VALUES(...) \
  (DatabaseValue[]){__VA_ARGS__}, \
  ((_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/4)

#define DATABASE_VALUE(name,value) \
  { name, value, DATABASE_DATATYPE, { (intptr_t)(0) } }
#define DATABASE_VALUE_KEY(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_KEY, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_KEY, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_BOOL(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_BOOL, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_BOOL, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_INT(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_INT, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_INT, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_UINT(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_UINT, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_UINT, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_INT64(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_INT64, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_INT64, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_UINT64(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_UINT64, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_UINT64, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_DOUBLE(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_DOUBLE, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_DOUBLE, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_ENUM(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_UINT, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_UINT, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_DATETIME(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_DATETIME, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_DATETIME, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_STRING(name,data,...) \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_STRING, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_STRING, { (intptr_t)(data) } \
    ) \
  }
#define DATABASE_VALUE_CSTRING(name,data,...)  \
  { name, \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
      data, DATABASE_DATATYPE_CSTRING, { (intptr_t)(__VA_ARGS__) } \
    ) \
    ( \
      0, DATABASE_DATATYPE_CSTRING, { (intptr_t)(data) } \
    ) \
  }

#define DATABASE_VALUES_NONE (DatabaseValue*)NULL,0

// filter macros
#define DATABASE_FILTERS(...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    (DatabaseFilter[]){__VA_ARGS__}, \
  ) \
  ( \
    NULL, \
  ) \
  ((_ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0)/2)

/***********************************************************************\
* Name   : __DatabaseFilterBlob
* Purpose: helper function to declare blob filter
* Input  : data   - blob data
*          length - blob data length
* Output : -
* Return : blob filter
* Notes  : -
\***********************************************************************/

#pragma GCC diagnostic ignored "-Wunused-function"
LOCAL_INLINE DatabaseFilterBlob __DatabaseFilterBlob(void *data, ulong length)
{
  return (DatabaseFilterBlob){ data, length };
}
#pragma GCC diagnostic warning "-Wunused-function"

/***********************************************************************\
* Name   : __DatabaseFilterArray
* Purpose: helper function to declare array filter
* Input  : data        - array data
*          length      - array data length
*          elementSize - element size
*          toString    - to-string callback
* Output : -
* Return : array filter
* Notes  : -
\***********************************************************************/

#pragma GCC diagnostic ignored "-Wunused-function"
LOCAL_INLINE DatabaseFilterArray __DatabaseFilterArray(void *data, ulong length, uint elementSize, const char*(*toString)(const void *data))
{
  return (DatabaseFilterArray){ data, length, elementSize, toString };
}
#pragma GCC diagnostic warning "-Wunused-function"

#define DATABASE_FILTER_KEY(value)           { .type = DATABASE_DATATYPE_KEY,      { .id = value } }
#define DATABASE_FILTER_BOOL(value)          { .type = DATABASE_DATATYPE_BOOL,     { .b  = ((value) == true) } }
#define DATABASE_FILTER_INT(value)           { .type = DATABASE_DATATYPE_INT,      { .i  = value } }
#define DATABASE_FILTER_UINT(value)          { .type = DATABASE_DATATYPE_UINT,     { .u  = value } }
#define DATABASE_FILTER_INT64(value)         { .type = DATABASE_DATATYPE_INT64,    { .i64 = value } }
#define DATABASE_FILTER_UINT64(value)        { .type = DATABASE_DATATYPE_UINT64,   { .u64 = value } }
#define DATABASE_FILTER_DOUBLE(value)        { .type = DATABASE_DATATYPE_DOUBLE,   { .d   = value } }
#define DATABASE_FILTER_DATETIME(value)      { .type = DATABASE_DATATYPE_DATETIME, { .dateTime = value } }
#define DATABASE_FILTER_STRING(value)        { .type = DATABASE_DATATYPE_STRING,   { .string = value } }
#define DATABASE_FILTER_CSTRING(value)       { .type = DATABASE_DATATYPE_CSTRING,  { .s = value } }
#define DATABASE_FILTER_BLOB(value,_length)  { .type = DATABASE_DATATYPE_BLOB,     { .blob  = __DatabaseFilterArray value } }
#define DATABASE_FILTER_ARRAY(value)         { .type = DATABASE_DATATYPE_ARRAY,    { .array = __DatabaseFilterArray value } }

#define DATABASE_FILTERS_NONE (const char*)NULL,(DatabaseFilter*)NULL,0


/***********************************************************************\
* Name   : DATABASE_LOCKED_DO
* Purpose: execute block with database locked
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DatabaseLockTypes
*          timeout        - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            DATABASE_LOCKED_DO(databaseHandle,DATABASE_LOCK_TYPE_READ,1000)
*            {
*              ...
*            }
\***********************************************************************/

#define DATABASE_LOCKED_DO(databaseHandle,lockType,timeout) \
  for (bool __databaseLock ## __COUNTER__ = Database_lock(databaseHandle,lockType,timeout); \
       __databaseLock ## __COUNTER__; \
       Database_unlock(databaseHandle,lockType), __databaseLock ## __COUNTER__ = FALSE \
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
  #define Database_select(...)              __Database_select             (__FILE__,__LINE__, ## __VA_ARGS__)
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
* Purpose: init database specifier and parse datatabase URI
* Input  : databaseSpecifier   - database specifier variable
*          databaseURI         - database URI string:
*                                  - [(sqlite|sqlite3):]file name, NULL
*                                    for sqlite3 "in memory",
*                                  - mariadb:<server>:<user>:<passwored>[:<database>]
*                                  - postgresql:<server>:<user>:<passwored>[:<database>]
*                                if NULL use sqlite3 in memory
*          defaultDatabaseName - default database name
* Output : databaseSpecifier - database specifier
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_parseSpecifier(DatabaseSpecifier *databaseSpecifier,
                               const char        *databaseURI,
                               const char        *defaultDatabaseName
                              );

/***********************************************************************\
* Name   : Database_copySpecifier
* Purpose: copy database specifier
* Input  : databaseSpecifier     - database specifier variable
*          fromDatabaseSpecifier - from database specifier variable
*          fromDatabaseName      - from database name or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_copySpecifier(DatabaseSpecifier       *databaseSpecifier,
                            const DatabaseSpecifier *fromDatabaseSpecifier,
                            const char              *fromDatabaseName
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
* Input  : datbaseURI          - database URI string:
*                                  - [(sqlite|sqlite3):]file name, NULL
*                                    for sqlite3 "in memory",
*                                  - mariadb:<server>:<user>:<password>[:<database>]
*                                  - postgresql:<server>:<user>:<password>[:<database>]
           defaultDatabaseName - default database name
* Return : validURIPrefixFlag - TRUE iff valid URI prefix is given (can
*                               be NULL)
* Return : database specifier or NULL
* Notes  : -
\***********************************************************************/

DatabaseSpecifier *Database_newSpecifier(const char *databaseURI,
                                         const char *defaultDatabaseName,
                                         bool       *validURIPrefixFlag
                                        );

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
*          databaseName0,databaseName1           - database name or NULL
* Output : -
* Return : TRUE iff equals (except passwords)
* Notes  : -
\***********************************************************************/

bool Database_equalSpecifiers(const DatabaseSpecifier *databaseSpecifier0,
                              const char              *databaseName0,
                              const DatabaseSpecifier *databaseSpecifier1,
                              const char              *databaseName1
                             );

/***********************************************************************\
* Name   : Database_getPrintableName
* Purpose: get printable database name (without password)
* Input  : string            - name variable (can be NULL)
*          databaseSpecifier - database specifier
* Output : -
* Return : printable database name
* Notes  : -
\***********************************************************************/

String Database_getPrintableName(String                  string,
                                 const DatabaseSpecifier *databaseSpecifier,
                                 const char              *databaseName
                                );

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
*          databaseName      - database name or NULL for name from
*                              database specifier
*          newDatabaseName   - new database name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_rename(const DatabaseSpecifier *databaseSpecifier,
                       const char              *databaseName,
                       const char              *newDatabaseName
                      );

//Database_delete(indexDatabaseSpecifier,databaseName);

/***********************************************************************\
* Name   : Database_create
* Purpose: create database
* Input  : databaseSpecifier - database specifier
*          databaseName      - database name or NULL for name from
*                              database specifier
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_create(const DatabaseSpecifier *databaseSpecifier,
                       const char              *databaseName
                      );

/***********************************************************************\
* Name   : Database_drop
* Purpose: drop database
* Input  : databaseSpecifier - database specifier
*          databaseName      - database name or NULL for name from
*                              database specifier
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_drop(const DatabaseSpecifier *databaseSpecifier,
                     const char              *databaseName
                    );

/***********************************************************************\
* Name   : Database_open
* Purpose: open database
* Input  : databaseHandle    - database handle variable
*          databaseSpecifier - database specifier
*          databaseName      - database name or NULL for name from
*                              database specifier
*          databaseOpenMode  - open mode; see DatabaseOpenModes
*          timeout           - timeout [ms] or WAIT_FOREVER
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle          *databaseHandle,
                       const DatabaseSpecifier *databaseSpecifier,
                       const char              *databaseName,
                       DatabaseOpenModes       databaseOpenMode,
                       long                    timeout
                      );
#else /* not NDEBUG */
  Errors __Database_open(const char              *__fileName__,
                         ulong                   __lineNb__,
                         DatabaseHandle          *databaseHandle,
                         const DatabaseSpecifier *databaseSpecifier,
                         const char              *databaseName,
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
*          databaseName   - database name
* Output : tableList - table list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getTableList(StringList     *tableList,
                             DatabaseHandle *databaseHandle,
                             const char     *databaseName
                            );

/***********************************************************************\
* Name   : Database_getViewList
* Purpose: get view list
* Input  : viewList       - view list variable
*          databaseHandle - database handle
*          databaseName   - database name
* Output : viewList - view list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getViewList(StringList     *viewList,
                            DatabaseHandle *databaseHandle,
                             const char     *databaseName
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
*          databaseName   - database name
* Output : triggerList - trigger list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getTriggerList(StringList     *triggerList,
                               DatabaseHandle *databaseHandle,
                               const char     *databaseName
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
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

bool Database_isLockPending(DatabaseHandle    *databaseHandle,
                            DatabaseLockTypes lockType
                           );

/***********************************************************************\
* Name   : Database_setEnabledSync
* Purpose: enable/disable synchronous mode
* Input  : databaseHandle - database handle
*          enabled        - TRUE to enable synchronous mode
* Output : -
* Return : ERROR_NONE or error code
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
* Return : ERROR_NONE or error code
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
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setTmpDirectory(DatabaseHandle *databaseHandle,
                                const char     *directoryName
                               );

/***********************************************************************\
* Name   : Database_dropTable
* Purpose: drop table
* Input  : databaseHandle - database handle
*          tableName      - table name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTable(DatabaseHandle *databaseHandle,
                          const char     *tableName
                         );

/***********************************************************************\
* Name   : Database_dropTables
* Purpose: drop all tables
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTables(DatabaseHandle *databaseHandle);

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
* Name   : Database_dropView
* Purpose: drop view
* Input  : databaseHandle - database handle
*          viewName       - view name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropView(DatabaseHandle *databaseHandle,
                         const char     *viewName
                        );

/***********************************************************************\
* Name   : Database_dropViews
* Purpose: drop all views
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropViews(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_dropIndex
* Purpose: drop index
* Input  : databaseHandle - database handle
*          indexName      - index name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropIndex(DatabaseHandle *databaseHandle,
                          const char     *indexName
                         );

/***********************************************************************\
* Name   : Database_dropIndices
* Purpose: drop all indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropIndices(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_dropTrigger
* Purpose: drop trigger
* Input  : databaseHandle - database handle
*          triggerName    - trigger name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_dropTrigger(DatabaseHandle *databaseHandle,
                            const char     *triggerName
                           );

/***********************************************************************\
* Name   : Database_dropTriggers
* Purpose: drop all triggers
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
*          tableNames              - table names (can be NULL)
*          columnNameCount         - number of table names
*          compareFlags            - compare flags; see
*                                    DATABASE_COMPARE_FLAG_*
* Output : -
* Return : ERROR_NONE if databases equals or mismatch code
* Notes  : -
\***********************************************************************/

Errors Database_compare(DatabaseHandle     *databaseHandleReference,
                        DatabaseHandle     *databaseHandle,
                        const char * const tableNames[],
                        uint               columnNameCount,
                        uint               compareFlags
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
*          filter                       - filter string
*          filters                      - filter values
*          filterCount                  - filter values count
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
                          const char                           *filter,
                          const DatabaseFilter                 filters[],
                          uint                                 filterCount,
                          const char                           *groupBy,
                          const char                           *orderBy,
                          uint64                               offset,
                          uint64                               limit
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

DatabaseId Database_getTableColumnId(DatabaseColumnInfo *columnInfo, const char *columnName, DatabaseId defaultValue);
int Database_getTableColumnInt(DatabaseColumnInfo *columnInfo, const char *columnName, int defaultValue);
uint Database_getTableColumnUInt(DatabaseColumnInfo *columnInfo, const char *columnName, uint defaultValue);
int64 Database_getTableColumnInt64(DatabaseColumnInfo *columnInfo, const char *columnName, int64 defaultValue);
uint64 Database_getTableColumnUInt64(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 defaultValue);
double Database_getTableColumnDouble(DatabaseColumnInfo *columnInfo, const char *columnName, double defaultValue);
uint Database_getTableColumnEnum(DatabaseColumnInfo *columnInfo, const char *columnName, uint defaultValue);
uint64 Database_getTableColumnDateTime(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 defaultValue);
String Database_getTableColumnString(DatabaseColumnInfo *columnInfo, const char *columnName, String value, const char *defaultValue);
const char *Database_getTableColumnCString(DatabaseColumnInfo *columnInfo, const char *columnName, const char *defaultValue);
void Database_getTableColumnBlob(DatabaseColumnInfo *columnInfo, const char *columnName, void *data, uint length);

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

bool Database_setTableColumnId(DatabaseColumnInfo *columnInfo, const char *columnName, DatabaseId value);
bool Database_setTableColumnBool(DatabaseColumnInfo *columnInfo, const char *columnName, bool value);
bool Database_setTableColumnInt(DatabaseColumnInfo *columnInfo, const char *columnName, int value);
bool Database_setTableColumnUInt(DatabaseColumnInfo *columnInfo, const char *columnName, uint value);
bool Database_setTableColumnInt64(DatabaseColumnInfo *columnInfo, const char *columnName, int64 value);
bool Database_setTableColumnUInt64(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 value);
bool Database_setTableColumnDouble(DatabaseColumnInfo *columnInfo, const char *columnName, double value);
bool Database_setTableColumnEnum(DatabaseColumnInfo *columnInfo, const char *columnName, uint value);
bool Database_setTableColumnDateTime(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 value);
bool Database_setTableColumnString(DatabaseColumnInfo *columnInfo, const char *columnName, ConstString value);
bool Database_setTableColumnCString(DatabaseColumnInfo *columnInfo, const char *columnName, const char *value);
bool Database_setTableColumnBlob(DatabaseColumnInfo *columnInfo, const char *columnName, const void *data, uint length);

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
* Name   : Database_newFilter
* Purpose: create new SQL filter string (with 'TRUE')
* Input  : -
* Output : -
* Return : filter string
* Notes  : -
\***********************************************************************/

INLINE String Database_newFilter(void);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE String Database_newFilter(void)
{
  return String_newCString("TRUE");
}
#endif /* NDEBUG || __DATABASE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_newFilter
* Purpose: create new SQL filter string (with 'TRUE')
* Input  : filterString - filter string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Database_deleteFilter(String filterString);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE void Database_deleteFilter(String filterString)
{
  assert(filterString != NULL);

  String_delete(filterString);
}
#endif /* NDEBUG || __DATABASE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_resetFilter
* Purpose: reset SQL filter string (with 'TRUE')
* Input  : filterString - filter string
* Output : -
* Return : filter string
* Notes  : -
\***********************************************************************/

INLINE String Database_resetFilter(String filterString);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENTATION__)
INLINE String Database_resetFilter(String filterString)
{
  assert(filterString != NULL);

  return String_setCString(filterString,"TRUE");
}
#endif /* NDEBUG || __DATABASE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Database_filter
* Purpose: format SQL filter string
* Input  : filterString - filter string
*          format       - format string (printf-style)
*          ...          - optional arguments for format
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_filter(String filterString, const char *format, ...);

/***********************************************************************\
* Name   : Database_filterAppend
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

void Database_filterAppend(String filterString, bool condition, const char *concatenator, const char *format, ...);

/***********************************************************************\
* Name   : Database_filterDateString
* Purpose: get date filter string
* Input  : columnName - column name
* Output : -
* Return : filter string with date as timestamp (Unix epoch)
* Notes  : -
\***********************************************************************/

char *Database_filterDateString(const DatabaseHandle *databaseHandle,
                                const char           *columnName
                               );

/***********************************************************************\
* Name   : Database_filterTimeString
* Purpose: get time filter string
* Input  : columnName - column name
* Output : -
* Return : filter string with time as timestamp (seconds since midnight)
* Notes  : -
\***********************************************************************/

char *Database_filterTimeString(const DatabaseHandle *databaseHandle,
                                const char           *columnName
                               );

/***********************************************************************\
* Name   : Database_execute
* Purpose: execute SQL statement
* Input  : databaseHandle  - database handle
*          changedRowCount - number of changd rows (can be NULL)
*          flags           - execute flags; see DATABASE_FLAG_...
*          sqlCommand      - SQL command string
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_execute(DatabaseHandle *databaseHandle,
                        ulong          *changedRowCount,
                        uint           flags,
                        const char     *sqlCommand
                       );

/***********************************************************************\
* Name   : Database_insert
* Purpose: insert row into database table
* Input  : databaseHandle      - database handle
*          insertRowId         - insert row id variable (can be NULL)
*          tableName           - table name,
*          flags               - insert flags; see DATABASE_FLAG_...
*          values              - values to insert
*          valueCount          - value count
*          conflictColumns     - conflict columns
*          conflictColumnCount - conflict columns count
*          filter              - SQL filter
*          filters             - values to insert
*          filterCount         - value count
* Output : insertRowId - insert row id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_insert(DatabaseHandle       *databaseHandle,
                       DatabaseId           *insertRowId,
                       const char           *tableName,
                       uint                 flags,
                       const DatabaseValue  values[],
                       uint                 valueCount,
                       const DatabaseColumn conflictColumns[],
                       uint                 conflictColumnCount,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      );

/***********************************************************************\
* Name   : Database_insertSelect
* Purpose: insert row into database table from seledt
* Input  : databaseHandle  - database handle
*          changedRowCount - row count variable (can be NULL)
*          tableName       - table name,
*          flags           - insert flags; see DATABASE_FLAG__...
*          toColumns       - columns to insert
*          toColumnsCount  - columns to insert count
*          tableNames      - select table names
*          tableNameCount  - select table names count
*          fromColumns     - select columns
*          fromColumnCount - select columns couont
*          filter          - SQL filter expression
*          filters         - filter values
*          filterCount     - filter values count
*          groupBy         - group-by SQL string or NULL
*          order           - order SQL string or NULL
*          offset          - offset or 0
*          limit           - limit or 0
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_insertSelect(DatabaseHandle       *databaseHandle,
                             ulong                *changedRowCount,
                             const char           *tableName,
                             uint                 flags,
                             const DatabaseColumn toColumns[],
                             uint                 toColumnCount,
                             const char           *tableNames[],
                             uint                 tableNameCount,
                             DatabaseColumn       fromColumns[],
                             uint                 fromColumnCount,
                             const char           *filter,
                             const DatabaseFilter filters[],
                             uint                 filterCount,
                             const char           *groupBy,
                             const char           *order,
                             uint64               offset,
                             uint64               limit
                            );

/***********************************************************************\
* Name   : Database_update
* Purpose: update row in database table
* Input  : databaseHandle  - database handle
*          changedRowCount - row count variable (can be NULL)
*          flags           - insert flags; see DATABASE_FLAG__...
*          values          - values to insert
*          valueCount      - value count
*          filter          - SQL filter
*          filters         - values to insert
*          filterCount     - value count
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_update(DatabaseHandle       *databaseHandle,
                       ulong                *changedRowCount,
                       const char           *tableName,
                       uint                 flags,
                       const DatabaseValue  values[],
                       uint                 valueCount,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      );

/***********************************************************************\
* Name   : Database_delete
* Purpose: delete rows from database table
* Input  : databaseHandle  - database handle
*          changedRowCount - row count variable (can be NULL)
*          tableName       - table name,
*          flags           - insert flags; see DATABASE_FLAG__...
*          filter          - SQL filter expression
*          filters         - filter values
*          filterCount     - filter values count
*          limit           - delete limit (if supported) or 0
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_delete(DatabaseHandle       *databaseHandle,
                       ulong                *changedRowCount,
                       const char           *tableName,
                       uint                 flags,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount,
                       uint64               limit
                      );

/***********************************************************************\
* Name   : Database_deleteArray
* Purpose: delete rows from database table by array
* Input  : databaseHandle    - database handle
*          changedRowCount   - row count variable (can be NULL)
*          tableName         - table name,
*          flags             - insert flags; see DATABASE_FLAG__...
*          filter            - SQL filter expression
*          filterDataType    - filter value type
*          filterArrayData   - filter array data
*          filterArrayLength - filter array length
*          limit             - delete limit (if supported) or 0
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_deleteArray(DatabaseHandle       *databaseHandle,
                            ulong                *changedRowCount,
                            const char           *tableName,
                            uint                 flags,
                            const char           *filter,
                            DatabaseDataTypes    filterDataType,
                            const void           *filterArrayData,
                            ulong                filterArrayLength,
                            uint64               limit
                           );

/***********************************************************************\
* Name   : Database_deleteByIds
* Purpose: delete rows from database table by ids array
* Input  : databaseHandle    - database handle
*          changedRowCount   - row count variable (can be NULL)
*          tableName         - table name,
*          columnName        - column name,
*          flags             - insert flags; see DATABASE_FLAG__...
*          databaseIds       - database ids array
*          databaseIdCount   - length of database ids array
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_deleteByIds(DatabaseHandle   *databaseHandle,
                            ulong            *changedRowCount,
                            const char       *tableName,
                            const char       *columnName,
                            uint             flags,
                            const DatabaseId databaseIds[],
                            ulong            databaseIdCount
                           );

/***********************************************************************\
* Name   : Database_select
* Purpose: select rows in database table
* Input  : databaseHandle      - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
*          tableName           - table name
*          flags               - insert flags; see DATABASE_FLAG__...
*          columns             - select columns
*          columnCount         - select columns count
*          filter              - SQL filter expression
*          filters             - filter values
*          filterCount         - filter values count
*          groupBy             - group-by SQL string or NULL
*          orderBy             - order-by SQL string or NULL
*          offset              - offset or 0
*          limit               - limit or 0
* Output : -
* Return : ERROR_NONE or error code
* Notes  : Database is locked until Database_finalize() is called
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_select(DatabaseStatementHandle *databaseStatementHandle,
                         DatabaseHandle          *databaseHandle,
// TODO: use DatabaseTable
                         const char              *tableName,
                         uint                    flags,
                         DatabaseColumn          columns[],
                         uint                    columnCount,
                         const char              *filter,
                         const DatabaseFilter    filters[],
                         uint                    filterCount,
                         const char              *groupBy,
                         const char              *orderBy,
                         uint64                  offset,
                         uint64                  limit
                        );
#else /* not NDEBUG */
  Errors __Database_select(const char              *__fileName__,
                           ulong                   __lineNb__,
                           DatabaseStatementHandle *databaseStatementHandle,
                           DatabaseHandle          *databaseHandle,
// TODO: use DatabaseTable
                           const char              *tableName,
                           uint                    flags,
                           DatabaseColumn          columns[],
                           uint                    columnCount,
                           const char              *filter,
                           const DatabaseFilter    filters[],
                           uint                    filterCount,
                           const char              *groupBy,
                           const char              *orderBy,
                           uint64                  offset,
                           uint64                  limit
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_getNextRow
* Purpose: get next row from query result
* Input  : databaseStatementHandle - database statment handle
* Output : ... - values
* Return : TRUE if row read, FALSE if no more rows
* Notes  : -
\***********************************************************************/

bool Database_getNextRow(DatabaseStatementHandle *databaseStatementHandle,
                         ...
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
* Name   : Database_existsValue
* Purpose: check if value exists in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : -
* Return : TRUE iff value exists
* Notes  : use DATABASE_FILTERS() for filters
\***********************************************************************/

bool Database_existsValue(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );

/***********************************************************************\
* Name   : Database_get
* Purpose: get values from database table
* Input  : databaseHandle      - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
*          tableNames          - table names
*          tableNameCount      - table names count
*          flags               - flags; see DATABASE_FLAG__...
*          columns             - select columns
*          columnCount         - select columns count
*          filter              - filter string
*          filters             - filter values
*          filterCount         - filter values count
*          groupBy             - group-by SQL string or NULL
*          orderBy             - order-by SQL string or NULL
*          offset              - offset or 0
*          limit               - limit or 0
* Output : value - database    d or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : use DATABASE_FILTERS() for filters
\***********************************************************************/

Errors Database_get(DatabaseHandle       *databaseHandle,
                    DatabaseRowFunction  databaseRowFunction,
                    void                 *databaseRowUserData,
                    ulong                *changedRowCount,
                    const char           *tableNames[],
                    uint                 tableNameCount,
                    uint                 flags,
                    const DatabaseColumn columns[],
                    uint                 columnCount,
                    const char           *filter,
                    const DatabaseFilter filters[],
                    uint                 filterCount,
                    const char           *groupBy,
                    const char           *orderBy,
                    uint64               offset,
                    uint64               limit
                   );

/***********************************************************************\
* Name   : Database_getId
* Purpose: get database id of value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : value - database id or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getId(DatabaseHandle       *databaseHandle,
                      DatabaseId           *value,
                      const char           *tableName,
                      const char           *columnName,
                      const char           *filter,
                      const DatabaseFilter filters[],
                      uint                 filterCount
                     );

/***********************************************************************\
* Name   : Database_getIds
* Purpose: get database ids from database table
* Input  : databaseHandle - database handle
*          ids            - database ids array
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
*          limit          - limit
* Output : ids - database ids array
* Return : ERROR_NONE or error code
* Notes  : values are added to array!
\***********************************************************************/

Errors Database_getIds(DatabaseHandle       *databaseHandle,
                       Array                *ids,
                       const char           *tableName,
                       const char           *columnName,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount,
                       uint64               limit
                      );

/***********************************************************************\
* Name   : Database_getMaxId
* Purpose: get max. database id of value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : value - max. database id or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getMaxId(DatabaseHandle       *databaseHandle,
                         DatabaseId           *value,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        );

/***********************************************************************\
* Name   : Database_getInt
* Purpose: get int value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
*          group          - group SQL string or NULL
* Output : value - int value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInt(DatabaseHandle       *databaseHandle,
                       int                  *value,
                       const char           *tableName,
                       const char           *columnName,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount,
                       const char           *group
                      );

/***********************************************************************\
* Name   : Database_setInt
* Purpose: insert or update int value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          value          - int64 value
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
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
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      );

/***********************************************************************\
* Name   : Database_getUInt
* Purpose: get uint value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getUInt(DatabaseHandle       *databaseHandle,
                        uint                 *value,
                        const char           *tableName,
                        const char           *columnName,
                        const char           *filter,
                        const DatabaseFilter filters[],
                        uint                 filterCount,
                        const char           *group
                       );

/***********************************************************************\
* Name   : Database_setUint
* Purpose: insert or update uint value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          value          - int64 value
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
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
                        const DatabaseFilter filters[],
                        uint                 filterCount
                       );

/***********************************************************************\
* Name   : Database_getInt64
* Purpose: get int64 value from database table
* Input  : databaseHandle  - database handle
*          tableName       - table name
*          columnName      - column name
*          filter          - filter string
*          filters         - filter values
*          filterCount     - filter values count
*          group           - group SQL string or NULL
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInt64(DatabaseHandle       *databaseHandle,
                         int64                *value,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount,
                         const char           *group
                        );

/***********************************************************************\
* Name   : Database_setInt64
* Purpose: insert or update int64 value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          value          - int64 value
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
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
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        );

/***********************************************************************\
* Name   : Database_getUInt64
* Purpose: get uint64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
*          group          - group SQL string or NULL
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getUInt64(DatabaseHandle       *databaseHandle,
                          uint64               *value,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount,
                          const char           *group
                         );

/***********************************************************************\
* Name   : Database_setUInt64
* Purpose: insert or update uint64 value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          value          - int64 value
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
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
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );

/***********************************************************************\
* Name   : Database_getDouble
* Purpose: get double value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
*          group          - group SQL string or NULL
* Output : value - double value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getDouble(DatabaseHandle       *databaseHandle,
                          double               *value,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount,
                          const char           *group
                         );

/***********************************************************************\
* Name   : Database_setDouble
* Purpose: insert or update double value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          value          - double value
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setDouble(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          double               value,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );

/***********************************************************************\
* Name   : Database_getString, Database_getCString
* Purpose: get string value from database table
* Input  : databaseHandle  - database handle
*          tableName       - table name
*          string          - string variable
*          maxStringLength - max. length of string
*          columnName      - column name
*          filter          - filter string
*          filters         - filter values
*          filterCount     - filter values count
* Output : string - string value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getString(DatabaseHandle       *databaseHandle,
                          String               string,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );
Errors Database_getCString(DatabaseHandle       *databaseHandle,
                           char                 *string,
                           uint                 maxStringLength,
                           const char           *tableName,
                           const char           *columnName,
                           const char           *filter,
                           const DatabaseFilter filters[],
                           uint                 filterCount
                          );

/***********************************************************************\
* Name   : Database_setString
* Purpose: insert or update string value in database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          flags          - flags; see DATABASE_FLAG__...
*          columnName     - column name
*          value          - string value
*          filter         - filter string
*          filters        - filter values
*          filterCount    - filter values count
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setString(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          const String         value,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );

/***********************************************************************\
* Name   : Database_vacuum
* Purpose: vacuum data base: remove not used storage space
* Input  : databaseHandle - database handle
*          tableNames     - tables to vacuum
*          tableNameCount - tables count
*          toDatabaseURI  - to-database URI or NULL
*          forceFlag      - to force vacuum
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_vacuum(DatabaseHandle     *databaseHandle,
                       const char * const tableNames[],
                       uint               tableNameCount,
                       const char         *toDatabaseURI,
                       bool               force
                      );

/***********************************************************************\
* Name   : Database_check
* Purpose: check database integrity
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
