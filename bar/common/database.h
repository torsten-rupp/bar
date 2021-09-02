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
#include <semaphore.h>
#include <assert.h>

#include "common/global.h"
#include "strings.h"
#include "common/arrays.h"
#include "common/threads.h"
#include "semaphores.h"
#include "errors.h"

#include "sqlite3.h"

/****************** Conditional compilation switches *******************/
#define _DATABASE_LOCK_PER_INSTANCE   // if defined use lock per database instance, otherwise a global lock for all database is used

// switch on for debugging only!
#warning remove/revert
#define DATABASE_DEBUG_LOCK
#define _DATABASE_DEBUG_LOCK_PRINT
#define _DATABASE_DEBUG_TIMEOUT
#define _DATABASE_DEBUG_COPY_TABLE
#define _DATABASE_DEBUG_LOG SQLITE_TRACE_STMT

/***************************** Constants *******************************/

// database open mask
#define DATABASE_OPEN_MASK_MODE  0x0000000F
#define DATABASE_OPEN_MASK_FLAGS 0xFFFF0000

// database open modes
typedef enum
{
  DATABASE_OPENMODE_CREATE,
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

// database data types
typedef enum
{
  DATABASE_TYPE_NONE,

  DATABASE_TYPE_PRIMARY_KEY,
  DATABASE_TYPE_FOREIGN_KEY,

  DATABASE_TYPE_INT64,
  DATABASE_TYPE_DOUBLE,
  DATABASE_TYPE_DATETIME,
  DATABASE_TYPE_TEXT,
  DATABASE_TYPE_BLOB,

  DATABASE_TYPE_UNKNOWN
} DatabaseTypes;

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
  String                      fileName;                   // database file name
  uint                        openCount;

  DatabaseLockTypes           type;
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
        DatabaseLockTypes  type;
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

// database handle
typedef struct DatabaseHandle
{
  #ifndef NDEBUG
    LIST_NODE_HEADER(struct DatabaseHandle);
  #endif /* not NDEBUG */

  DatabaseNode                *databaseNode;
  uint                        readLockCount;
  uint                        readWriteLockCount;
  sqlite3                     *handle;                    // SQlite3 handle
  uint                        transcationCount;
  long                        timeout;                    // timeout [ms]
  void                        *busyHandlerUserData;
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
        String     sqlCommand;                            // current SQL command
        #ifdef HAVE_BACKTRACE
          void const *stackTrace[16];
          int        stackTraceSize;
        #endif /* HAVE_BACKTRACE */
      }                         current;
    } debug;
  #endif /* not NDEBUG */
} DatabaseHandle;

// database query handle
typedef struct
{
  DatabaseHandle *databaseHandle;
  sqlite3_stmt   *statementHandle;
  #ifndef NDEBUG
    String sqlString;
    uint64 t0,t1;
    uint64 dt;
  #endif /* not NDEBUG */
} DatabaseQueryHandle;

/***********************************************************************\
* Name   : DatabaseRowFunction
* Purpose: execute row callback function
* Input  : names    - column names
*          values   - column values
*          count    - number of columns
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseRowFunction)(const char* names[], const char* values[], uint count, void *userData);
//TODO: remove typedef Errors(*DatabaseRowFunction)(uint count, const char* names[], const char* values[], void *userData);

// database id
typedef int64 DatabaseId;

// table column definition list
typedef struct DatabaseColumnNode
{
  LIST_NODE_HEADER(struct DatabaseColumnNode);

  char          *name;
  DatabaseTypes type;
  union
  {
    // Note: data values are kept as strings to avoid conversion problems e.g. date/time -> integer
    DatabaseId id;      // primary key
    String     i;       // integer, date/time
    String     d;       // double
    String     text;    // text
    struct
    {
      void  *data;
      ulong length;
    }      blob;
  } value;
  bool          usedFlag;
} DatabaseColumnNode;

typedef struct
{
  LIST_HEADER(DatabaseColumnNode);
} DatabaseColumnList;

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

typedef Errors(*DatabaseCopyTableFunction)(const DatabaseColumnList *fromColumnList,
                                           const DatabaseColumnList *toColumnList,
                                           void                     *userData
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
* Name   : Database_open
* Purpose: open database
* Input  : databaseHandle   - database handle variable
*          fileName         - file name or NULL for "in memory"
*          databaseOpenMode - open mode; see DatabaseOpenModes
*          timeout          - timeout [ms] or WAIT_FOREVER
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode,
                       long              timeout
                      );
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         ulong             __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
                         DatabaseOpenModes databaseOpenMode,
                         long              timeout
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
* Name   : Database_getTableColumnList*
* Purpose: get table column list entry
* Input  : columnList   - column list
*          columnName   - column name
*          defaultValue - default value
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

DatabaseId Database_getTableColumnListId(const DatabaseColumnList *columnList, const char *columnName, DatabaseId defaultValue);
int Database_getTableColumnListInt(const DatabaseColumnList *columnList, const char *columnName, int defaultValue);
uint Database_getTableColumnListUInt(const DatabaseColumnList *columnList, const char *columnName, uint defaultValue);
int64 Database_getTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 defaultValue);
uint64 Database_getTableColumnListUInt64(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue);
double Database_getTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double defaultValue);
uint64 Database_getTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue);
String Database_getTableColumnList(const DatabaseColumnList *columnList, const char *columnName, String value, const char *defaultValue);
const char *Database_getTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *defaultValue);
void Database_getTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, void *data, uint length);

/***********************************************************************\
* Name   : Database_setTableColumnList*
* Purpose: set table column list entry
* Input  : columnList - column list
*          columnName - column name
*          value      - value
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

bool Database_setTableColumnListId(const DatabaseColumnList *columnList, const char *columnName, DatabaseId value);
bool Database_setTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 value);
bool Database_setTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double value);
bool Database_setTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 value);
bool Database_setTableColumnList(const DatabaseColumnList *columnList, const char *columnName, ConstString value);
bool Database_setTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *value);
bool Database_setTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, const void *data, uint length);

/***********************************************************************\
* Name   : Database_addColumn
* Purpose: add column to table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          columnType     - column data type
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_addColumn(DatabaseHandle *databaseHandle,
                          const char     *tableName,
                          const char     *columnName,
                          DatabaseTypes  columnType
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
* Name   : Database_execute, Database_vexecute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
*          command             - SQL command string with %[l]d, %[']S,
*                                %[']s
*          ...                 - optional arguments for SQL command
*                                string
*                                special functions:
*                                  REGEXP(pattern,case-flag,text)
*          arguments           - arguments for SQL command string
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_execute(DatabaseHandle      *databaseHandle,
                        DatabaseRowFunction databaseRowFunction,
                        void                *databaseRowUserData,
                        ulong               *changedRowCount,
                        const char          *command,
                        ...
                       );

Errors Database_vexecute(DatabaseHandle      *databaseHandle,
                         DatabaseRowFunction databaseRowFunction,
                         void                *databaseRowUserData,
                         ulong               *changedRowCount,
                         const char          *command,
                         va_list             arguments
                       );

/***********************************************************************\
* Name   : Database_prepare
* Purpose: prepare database query
* Input  : databaseHandle - database handle
*          command        - SQL command string with %[l]d, %[']S, %[']s
*          ...            - optional arguments for SQL command string
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : databaseQueryHandle - initialized database query handle
* Return : -
* Notes  : Database is locked until Database_finalize() is called
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_prepare(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseHandle      *databaseHandle,
                          const char          *command,
                          ...
                         );
#else /* not NDEBUG */
  Errors __Database_prepare(const char          *__fileName__,
                            ulong               __lineNb__,
                            DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseHandle      *databaseHandle,
                            const char          *command,
                            ...
                           );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_getNextRow
* Purpose: get next row from query result
* Input  : databaseQueryHandle - database query handle
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

bool Database_getNextRow(DatabaseQueryHandle *databaseQueryHandle,
                         const char          *format,
                         ...
                        );

/***********************************************************************\
* Name   : Database_finalize
* Purpose: done database query
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_finalize(DatabaseQueryHandle *databaseQueryHandle);
#else /* not NDEBUG */
  void __Database_finalize(const char        *__fileName__,
                           ulong             __lineNb__,
                           DatabaseQueryHandle *databaseQueryHandle
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

bool Database_exists(DatabaseHandle *databaseHandle,
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
* Name   : Database_getInteger64, Database_vgetInteger64
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
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
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
Errors Database_vgetString(DatabaseHandle *databaseHandle,
                           String         string,
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

DatabaseId Database_getLastRowId(DatabaseHandle *databaseHandle);

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
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

#ifdef NDEBUG
void Database_debugPrintQueryInfo(const DatabaseQueryHandle *databaseQueryHandle);
#else /* not NDEBUG */
void __Database_debugPrintQueryInfo(const char *__fileName__, ulong __lineNb__, const DatabaseQueryHandle *databaseQueryHandle);
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
