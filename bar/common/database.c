/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Database functions
* Systems: all
*
\***********************************************************************/

#define __DATABASE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #error No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <errno.h>
#include <assert.h>

#if defined(HAVE_MARIADB_MYSQL_H) && defined(HAVE_MARIADB_ERRMSG_H)
  #include "mariadb/mysql.h"
  #include "mariadb/errmsg.h"
#endif /* HAVE_MARIADB_MYSQL_H && HAVE_MARIADB_ERRMSG_H */

#include "common/global.h"
#include "common/strings.h"
#include "common/lists.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/misc.h"
#include "common/threads.h"
#include "common/semaphores.h"

#include "sqlite3.h"

#include "errors.h"

#include "database.h"

/****************** Conditional compilation switches *******************/
#define DATABASE_SUPPORT_TRANSACTIONS
#define DATABASE_SUPPORT_INTERRUPT
#define DATABASE_USE_ATOMIC_INCREMENT

// TODO: temporary work-around for lost wait triggers
#define DATABASE_WAIT_TRIGGER_WORK_AROUND
#define DATABASE_WAIT_TRIGGER_WORK_AROUND_TIME 5

/***************************** Constants *******************************/

#define MAX_INTERRUPT_COPY_TABLE_COUNT 128  // max. number of interrupts when copy table

#define MAX_FORCE_CHECKPOINT_TIME (10LL*60LL*1000LL) // timeout for force execution of a checkpoint [ms]
//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

LOCAL const char *TEMPORARY_TABLE_NAMES[] =
{
  "temporary1",
  "temporary2",
  "temporary3",
  "temporary4",
  "temporary5",
  "temporary6",
  "temporary7",
  "temporary8",
  "temporary9"
};

#if 1
  #define DEBUG_WARNING_LOCK_TIME  2ULL*1000ULL      // DEBUG only: warning lock time [ms]
  #define DEBUG_MAX_LOCK_TIME     60ULL*1000ULL      // DEBUG only: max. lock time [ms]
#else
  #define DEBUG_WARNING_LOCK_TIME MAX_UINT64
  #define DEBUG_MAX_LOCK_TIME     MAX_UINT64
#endif

LOCAL const char *DATABASE_DATATYPE_NAMES[] =
{
  "NONE",
  "-",
  "PRIMARY KEY",
  "KEY",
  "BOOL",
  "INT",
  "INT64",
  "UINT",
  "UINT64",
  "DOUBLE",
  "DATETIME",
  "STRING",
  "CSTRING",
  "BLOB",
  "UNKNOWN"
};

// min. MariaDB server version (>= 5.7.7 with key length 3072)
#define MARIADB_MIN_SERVER_VERSION 100200

#define MARIADB_TIMEOUT (5*60)  // [s]

/* MariaDB database character sets to use (descenting order)
   Note: try to create with character set uft8mb4 (4-byte UTF8),
         then utf8 as a fallback for older MariaDB versions.
*/
LOCAL const char *MARIADB_CHARACTER_SETS[] =
{
  "utf8mb4",
  "utf8"
};

/***************************** Datatypes *******************************/

#ifndef NDEBUG
typedef struct
{
  LIST_HEADER(DatabaseHandle);
} DatabaseHandleList;
#endif /* not NDEBUG */

#if 0
// callback function
typedef struct
{
  DatabaseRowFunction function;
  void                *userData;
} DatabaseRowCallback;
#endif

// value
typedef union
{
  bool       b;
  int        i;
  uint       ui;
  long       l;
  ulong      ul;
  int64      ll;
  uint64     ull;
  float      f;
  double     d;
  char       ch;
  const char *s;
  void       *p;
  uint64     dateTime;
  String     string;
} Value;

#ifndef NDEBUG
typedef struct
{
  bool   showHeaderFlag;
  bool   headerPrintedFlag;
  size_t *widths;
} DumpTableData;
#endif /* not NDEBUG */

/***************************** Variables *******************************/

LOCAL DatabaseList databaseList;
#ifndef DATABASE_LOCK_PER_INSTANCE
  LOCAL pthread_mutexattr_t databaseLockAttribute;
  LOCAL pthread_mutex_t     databaseLock;
  LOCAL struct
  {
    ThreadId    threadId;
    const char  *fileName;
    ulong       lineNb;
  } databaseLockBy;
#endif /* DATABASE_LOCK_PER_INSTANCE */

#ifndef NDEBUG
  LOCAL uint                databaseDebugCounter = 0;

  LOCAL pthread_once_t      debugDatabaseInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugDatabaseLockAttribute;
  LOCAL pthread_mutex_t     debugDatabaseLock;
  LOCAL ThreadId            debugDatabaseThreadId;
  LOCAL DatabaseHandleList  debugDatabaseHandleList;
  #ifdef HAVE_SIGQUIT
    LOCAL void                (*debugSignalQuitPrevHandler)(int);
  #endif /* HAVE_SIGQUIT */
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define DATABASE_DEBUG_SQL(databaseHandle,sqlString) \
    do \
    { \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database %s, %d: execute command: %s: %s\n",__FILE__,__LINE__,(databaseHandle)->debug.fileName,String_cString(sqlString)); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_SQLX(databaseHandle,text,sqlString) \
    do \
    { \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database %s, %d: " text ": %s: %s\n",__FILE__,__LINE__,(databaseHandle)->debug.fileName,String_cString(sqlString)); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString) \
    do \
    { \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      \
      if (databaseDebugCounter > 0) \
      { \
        String s = String_new(); \
        String_format(s,"EXPLAIN QUERY PLAN %s",String_cString(sqlString)); \
        fprintf(stderr,"DEBUG database %s, %d: query plan\n",__FILE__,__LINE__); \
        sqlite3_exec(databaseHandle->sqlite.handle, \
                     String_cString(s), \
                     debugPrintQueryPlanCallback, \
                     NULL, /* userData */ \
                     NULL /* errorMsg */ \
                    ); \
        String_delete(s); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_START(databaseStatementHandle) \
    do \
    { \
      assert(databaseStatementHandle != NULL); \
      \
      databaseStatementHandle->t0 = Misc_getTimestamp(); \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_END(databaseStatementHandle) \
    do \
    { \
      assert(databaseStatementHandle != NULL); \
      \
      databaseStatementHandle->t1 = Misc_getTimestamp(); \
      databaseStatementHandle->dt += (databaseStatementHandle->t1-databaseStatementHandle->t0); \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME(databaseStatementHandle) \
    do \
    { \
      assert(databaseStatementHandle != NULL); \
      \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database %s, %d: execution time=%llums\n",__FILE__,__LINE__,databaseStatementHandle->dt/1000ULL); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_LOCK_ASSERT(databaseHandle, condition) \
    do \
    { \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode); \
      \
      if (!(condition)) \
      { \
        debugPrintLockInfo(databaseHandle->databaseNode); \
      } \
      assert(condition); \
    } \
    while (0)
  #define DATABASE_DEBUG_LOCK_ASSERTX(databaseHandle, condition, format, ...) \
    do \
    { \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode); \
      \
      if (!(condition)) \
      { \
        debugPrintLockInfo(databaseHandle->databaseNode); \
      } \
      assertx(condition,format, ## __VA_ARGS__); \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_DEBUG_SQL(databaseHandle,sqlString) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_SQLX(databaseHandle,text,sqlString) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_START(databaseStatementHandle) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_END(databaseStatementHandle) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME(databaseStatementHandle) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_LOCK_ASSERT(databaseHandle, condition) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_LOCK_ASSERTX(databaseHandle, condition, format, ...) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

#ifdef DATABASE_LOCK_PER_INSTANCE
  #define DATABASE_HANDLE_LOCKED_DO(databaseHandle,block) \
    do \
    { \
      int __result; \
      \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      \
      __result = pthread_mutex_lock(databaseHandle->databaseNode->lock); \
      if (__result == 0) \
      { \
        ({ \
          auto void __closure__(void); \
          \
          void __closure__(void)block; __closure__; \
        })(); \
        __result = pthread_mutex_unlock(databaseHandle->databaseNode->lock); \
        assert(__result == 0); \
      } \
      UNUSED_VARIABLE(__result); \
    } \
    while (0)
  #define DATABASE_HANDLE_LOCKED_DOX(result,databaseHandle,block) \
    do \
    { \
      int __result; \
      \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      \
      __result = pthread_mutex_lock(databaseHandle->databaseNode->lock); \
      if (__result == 0) \
      { \
        result = ({ \
                   auto typeof(result) __closure__(void); \
                   \
                   typeof(result) __closure__(void)block; __closure__; \
                 })(); \
        __result = pthread_mutex_unlock(databaseHandle->databaseNode->lock); \
        assert(__result == 0); \
      } \
      else \
      { \
        result = ERROR_X(DATABASE,"lock fail"); \
      } \
      UNUSED_VARIABLE(__result); \
    } \
    while (0)
  #if   defined(PLATFORM_LINUX)
    #define DATABASE_HANDLE_IS_LOCKED(databaseHandle) \
      ((databaseHandle)->databaseNode->lock.__data.__lock > 0)
  #elif defined(PLATFORM_WINDOWS)
//TODO: NYI
    #define DATABASE_HANDLE_IS_LOCKED(databaseHandle) \
      TRUE
  #endif /* PLATFORM_... */
#else /* not DATABASE_LOCK_PER_INSTANCE */
  #define DATABASE_HANDLE_LOCKED_DO(databaseHandle,block) \
    do \
    { \
      int __result; \
      \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      \
      UNUSED_VARIABLE(databaseHandle); \
      \
      __result = pthread_mutex_lock(&databaseLock); \
      if (__result == 0) \
      { \
        databaseLockBy.threadId = Thread_getCurrentId(); \
        databaseLockBy.fileName = __FILE__; \
        databaseLockBy.lineNb   = __LINE__; \
        ({ \
          auto void __closure__(void); \
          \
          void __closure__(void)block; __closure__; \
        })(); \
        databaseLockBy.threadId = THREAD_ID_NONE; \
        databaseLockBy.fileName = NULL; \
        databaseLockBy.lineNb   = 0; \
        __result = pthread_mutex_unlock(&databaseLock); \
        assert(__result == 0); \
      } \
      UNUSED_VARIABLE(__result); \
    } \
    while (0)
  #define DATABASE_HANDLE_LOCKED_DOX(result,databaseHandle,block) \
    do \
    { \
      int __result; \
      \
      assert(databaseHandle != NULL); \
      DEBUG_CHECK_RESOURCE_TRACE(databaseHandle); \
      assert(databaseHandle->databaseNode != NULL); \
      \
      UNUSED_VARIABLE(databaseHandle); \
      \
      __result = pthread_mutex_lock(&databaseLock); \
      if (__result == 0) \
      { \
        databaseLockBy.threadId = Thread_getCurrentId(); \
        databaseLockBy.fileName = __FILE__; \
        databaseLockBy.lineNb   = __LINE__; \
        result = ({ \
                   auto typeof(result) __closure__(void); \
                   \
                   typeof(result) __closure__(void)block; __closure__; \
                 })(); \
        databaseLockBy.threadId = THREAD_ID_NONE; \
        databaseLockBy.fileName = NULL; \
        databaseLockBy.lineNb   = 0; \
        __result = pthread_mutex_unlock(&databaseLock); \
        assert(__result == 0); \
      } \
      UNUSED_VARIABLE(__result); \
    } \
    while (0)
  #if   defined(PLATFORM_LINUX)
    #define DATABASE_HANDLE_IS_LOCKED(databaseHandle) \
      (databaseLock.__data.__lock > 0)
  #elif defined(PLATFORM_WINDOWS)
//TODO: NYI
    #define DATABASE_HANDLE_IS_LOCKED(databaseHandle) \
      TRUE
  #endif /* PLATFORM_... */
#endif /* DATABASE_LOCK_PER_INSTANCE */

/***********************************************************************\
* Name   : DATABASE_DO
* Purpose: database block-operation
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
*          timeout        - timeout [ms] or WAIT_FOREVER
*          block          - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define DATABASE_DO(databaseHandle,lockType,timeout,block) \
    do \
    { \
      if (__begin(__FILE__,__LINE__,databaseHandle,lockType,timeout)) \
      { \
        ({ \
          auto void __closure__(void); \
          \
          void __closure__(void)block; __closure__; \
        })(); \
        __end(__FILE__,__LINE__,databaseHandle,lockType); \
      } \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_DO(databaseHandle,lockType,timeout,block) \
    do \
    { \
      if (begin(databaseHandle,lockType,timeout)) \
      { \
        ({ \
          auto void __closure__(void); \
          \
          void __closure__(void)block; __closure__; \
        })(); \
        end(databaseHandle,lockType); \
      } \
    } \
    while (0)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : DATABASE_DOX
* Purpose: database block-operation
* Input  : defaultResult  - default result
*          databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
*          timeout        - timeout [ms] or WAIT_FOREVER
*          block          - code block
* Output : result - result
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define DATABASE_DOX(result,defaultResult,databaseHandle,lockType,timeout,block) \
    do \
    { \
      if (__begin(__FILE__,__LINE__,databaseHandle,lockType,timeout)) \
      { \
        result = ({ \
                   auto typeof(result) __closure__(void); \
                   \
                   typeof(result) __closure__(void)block; __closure__; \
                 })(); \
        __end(__FILE__,__LINE__,databaseHandle,lockType); \
      } \
      else \
      { \
        result = defaultResult; \
      } \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_DOX(result,defaultResult,ddatabaseHandle,lockType,timeout,block) \
    do \
    { \
      if (begin(databaseHandle,lockType,timeout)) \
      { \
        result = ({ \
                   auto typeof(result) __closure__(void); \
                   \
                   typeof(result) __closure__(void)block; __closure__; \
                 })(); \
        end(databaseHandle,lockType); \
      } \
      else \
      { \
        result = defaultResult; \
      } \
    } \
    while (0)
#endif /* not NDEBUG */

#ifndef NDEBUG
  #define begin(...)                      __begin                     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define end(...)                        __end                       (__FILE__,__LINE__, ## __VA_ARGS__)

  #define pendingReadsIncrement(...)      __pendingReadsIncrement     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define pendingReadWritesIncrement(...) __pendingReadWritesIncrement(__FILE__,__LINE__, ## __VA_ARGS__)
  #define readsIncrement(...)             __readsIncrement            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define readsDecrement(...)             __readsDecrement            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define readWritesIncrement(...)        __readWritesIncrement       (__FILE__,__LINE__, ## __VA_ARGS__)
  #define readWritesDecrement(...)        __readWritesDecrement       (__FILE__,__LINE__, ## __VA_ARGS__)

  #define waitTriggerRead(...)            __waitTriggerRead           (__FILE__,__LINE__, ## __VA_ARGS__)
  #define waitTriggerReadWrite(...)       __waitTriggerReadWrite      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define waitTriggerTransaction(...)     __waitTriggerTransaction    (__FILE__,__LINE__, ## __VA_ARGS__)

  #define triggerUnlockRead(...)          __triggerUnlockRead         (__FILE__,__LINE__, ## __VA_ARGS__)
  #define triggerUnlockReadWrite(...)     __triggerUnlockReadWrite    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define triggerUnlockTransaction(...)   __triggerUnlockTransaction  (__FILE__,__LINE__, ## __VA_ARGS__)

  #define prepareStatement(...)           __prepareStatement          (__FILE__,__LINE__, ## __VA_ARGS__)
  #define finalizeStatement(...)           __finalizeStatement        (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : checkDatabaseInitialized
* Purpose: check if database initialzied
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff initialized
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool checkDatabaseInitialized(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      return (databaseHandle->sqlite.handle != NULL);
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        return (databaseHandle->mysql.handle != NULL);
      #else /* HAVE_MARIADB */
        return FALSE;
      #endif /* HAVE_MARIADB */
  }

  return FALSE;
}

/***********************************************************************\
* Name   :
* Purpose: check if data types are compatible
* Input  : dataType0, dataType1 - data types
* Output : -
* Return : TRUE if compatible
* Notes  : -
\***********************************************************************/

LOCAL bool areCompatibleTypes(DatabaseDataTypes dataType0, DatabaseDataTypes dataType1)
{
  switch (dataType0)
  {
    case DATABASE_DATATYPE_NONE:
      return FALSE;

    case DATABASE_DATATYPE:
      return (dataType1 == DATABASE_DATATYPE);

    case DATABASE_DATATYPE_PRIMARY_KEY:
      return (dataType1 == DATABASE_DATATYPE_PRIMARY_KEY);
    case DATABASE_DATATYPE_KEY:
      return (dataType1 == DATABASE_DATATYPE_KEY);
    case DATABASE_DATATYPE_BOOL:
      return    (dataType1 == DATABASE_DATATYPE_BOOL)
             || (dataType1 == DATABASE_DATATYPE_INT);
    case DATABASE_DATATYPE_INT:
      return    (dataType1 == DATABASE_DATATYPE_BOOL)
             || (dataType1 == DATABASE_DATATYPE_INT)
             || (dataType1 == DATABASE_DATATYPE_INT64)
             || (dataType1 == DATABASE_DATATYPE_DATETIME);
    case DATABASE_DATATYPE_INT64:
      return    (dataType1 == DATABASE_DATATYPE_INT64)
             || (dataType1 == DATABASE_DATATYPE_DATETIME);
    case DATABASE_DATATYPE_UINT:
      return    (dataType1 == DATABASE_DATATYPE_BOOL)
             || (dataType1 == DATABASE_DATATYPE_UINT)
             || (dataType1 == DATABASE_DATATYPE_UINT64)
             || (dataType1 == DATABASE_DATATYPE_DATETIME);
    case DATABASE_DATATYPE_UINT64:
      return    (dataType1 == DATABASE_DATATYPE_UINT64)
             || (dataType1 == DATABASE_DATATYPE_DATETIME);
    case DATABASE_DATATYPE_DOUBLE:
      return (dataType1 == DATABASE_DATATYPE_DOUBLE);
    case DATABASE_DATATYPE_DATETIME:
      return    (dataType1 == DATABASE_DATATYPE_INT)
             || (dataType1 == DATABASE_DATATYPE_INT64)
             || (dataType1 == DATABASE_DATATYPE_DATETIME);
    case DATABASE_DATATYPE_STRING:
      return (dataType1 == DATABASE_DATATYPE_STRING);
    case DATABASE_DATATYPE_CSTRING:
      return (dataType1 == DATABASE_DATATYPE_CSTRING);
    case DATABASE_DATATYPE_BLOB:
      return (dataType1 == DATABASE_DATATYPE_BLOB);

    case DATABASE_DATATYPE_UNKNOWN:
      return FALSE;
  }

  return FALSE;
}

#ifdef DATABASE_DEBUG_LOCK
/***********************************************************************\
* Name   : debugAddThreadLWPId
* Purpose: add LWP thread id to array
* Input  : threadLWPIds    - thread LWP id array
*          threadLWPIdSize - size of thread LWP id array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugAddThreadLWPId(ThreadLWPId threadLWPIds[], uint threadLWPIdSize)
{
  uint i;

  i = 0;
  while ((i < threadLWPIdSize) && threadLWPIds[i] != 0)
  {
    i++;
  }
  if (i < threadLWPIdSize)
  {
    threadLWPIds[i] = Thread_getCurrentLWPId();
  }
}

/***********************************************************************\
* Name   : debugRemoveThreadLWPId
* Purpose: remove LWP thread id from array
* Input  : threadLWPIds    - thread LWP id array
*          threadLWPIdSize - size of thread LWP id array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugRemoveThreadLWPId(ThreadLWPId threadLWPIds[], uint threadLWPIdSize)
{
  ThreadLWPId id;
  uint i;

  id = Thread_getCurrentLWPId();

  i = 0;
  while ((i < threadLWPIdSize) && threadLWPIds[i] != id)
  {
    i++;
  }
  if (i < threadLWPIdSize)
  {
    while (i < threadLWPIdSize-1)
    {
      threadLWPIds[i] = threadLWPIds[i+1];
      i++;
    }
    threadLWPIds[threadLWPIdSize-1] = 0;
  }
}
#endif /* DATABASE_DEBUG_LOCK */

#if !defined(NDEBUG) && defined(DATABASE_DEBUG_LOG)
/***********************************************************************\
* Name   : logTraceCommandHandler
* Purpose: log database trace command into file
* Input  : traceCommand - trace command; see SQLITE_TRACE_...
*          context      - database context
*          p,t          - specific trace command data
* Output : -
* Return : 0
* Notes  : -
\***********************************************************************/

LOCAL int logTraceCommandHandler(unsigned int traceCommand, void *context, void *p, void *t)
{
  FILE *handle;

  UNUSED_VARIABLE(context);

  handle = fopen("database.log","a");
  if (handle == NULL)
  {
    return 0;
  }

  switch (traceCommand)
  {
    case SQLITE_TRACE_STMT:
      {
        sqlite3_stmt *statementHandle = (sqlite3_stmt*)p;
        char         *sqlCommand;

        if (!stringStartsWith((const char*)t,"--"))
        {
          sqlCommand = sqlite3_expanded_sql(statementHandle);
          fprintf(handle,"prepare: %s\n",sqlite3_sql(statementHandle));
          fprintf(handle,"  expanded: %s\n",sqlCommand);
          sqlite3_free(sqlCommand);
        }
        else
        {
          fprintf(handle,"command: %s\n",(const char*)t);
        }
      }
      break;
    default:
      break;
  }

  fclose(handle);

  return 0;
}
#endif /* !defined(NDEBUG) && defined(DATABASE_DEBUG_LOG) */

/***********************************************************************\
* Name   : getTime
* Purpose: get POSIX compatible time
* Input  : -
* Output : timespec - time
* Return : -
* Notes  : -
\***********************************************************************/

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
LOCAL void getTime(struct timespec *timespec)
{
  __int64 windowsTime;

  assert(timespec != NULL);

  GetSystemTimeAsFileTime((FILETIME*)&windowsTime);
  windowsTime -= 116444736000000000LL;  // Jan 1 1601 -> Jan 1 1970
  timespec->tv_sec  = (windowsTime/10000000LL);
  timespec->tv_nsec = (windowsTime%10000000LL)*100LL;
}
#endif /* PLATFORM_... */

#ifndef NDEBUG

//TODO: debug function for logging?
#if 0
/***********************************************************************\
* Name   : debugPrint
* Purpose: callback for debugPrint function (Unix epoch, UTC)
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugPrint(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  const char    *text;

  assert(context != NULL);
  assert(argc >= 1);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  text   = (const char*)sqlite3_value_text(argv[0]);
  fprintf(stderr,"DEBUG database: %s\n",text);
}
#endif

/***********************************************************************\
* Name   : debugPrintQueryPlanCallback
* Purpose: print query plan output
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef WERROR
LOCAL int debugPrintQueryPlanCallback(void *userData, int argc, char *argv[], char *columns[])
{
  int i;

  assert(argc >= 0);
  assert(argv != NULL);
  assert(columns != NULL);

  UNUSED_VARIABLE(userData);

  for (i = 0; i < argc; i++)
  {
    fprintf(stderr,"  %s=%s",columns[i],argv[i]);
  }
  fprintf(stderr," \n");

  return 0;
}
#endif

/***********************************************************************\
* Name   : debugDatabaseSignalHandler
* Purpose: signal handler
* Input  : signalNumber - signal number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGQUIT
LOCAL void debugDatabaseSignalHandler(int signalNumber)
{
  if ((signalNumber == SIGQUIT) && Thread_isCurrentThread(debugDatabaseThreadId))
  {
    Database_debugPrintInfo();
  }

  if (debugSignalQuitPrevHandler != NULL)
  {
    debugSignalQuitPrevHandler(signalNumber);
  }
}
#endif /* HAVE_SIGQUIT */

/***********************************************************************\
* Name   : debugDatabaseInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugDatabaseInit(void)
{
  // init variables
  debugDatabaseThreadId = Thread_getCurrentId();
  List_init(&debugDatabaseHandleList);

  // init lock
  if (pthread_mutexattr_init(&debugDatabaseLockAttribute) != 0)
  {
    HALT_INTERNAL_ERROR("Cannot initialize database debug lock!");
  }
  pthread_mutexattr_settype(&debugDatabaseLockAttribute,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&debugDatabaseLock,&debugDatabaseLockAttribute) != 0)
  {
    HALT_INTERNAL_ERROR("Cannot initialize database debug lock!");
  }

  #ifdef HAVE_SIGQUIT
    // install signal handler for Ctrl-\ (SIGQUIT) for printing debug information
    debugSignalQuitPrevHandler = signal(SIGQUIT,debugDatabaseSignalHandler);
  #endif /* HAVE_SIGQUIT */
}

/***********************************************************************\
* Name   : debugSetDatabaseThreadInfo
* Purpose: set database thread info
* Input  : __fileName__       - file name
*          __lineNb__         - line number
*          databaseThreadInfo - database thread info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugSetDatabaseThreadInfo(const char         *__fileName__,
                                             ulong              __lineNb__,
                                             DatabaseThreadInfo *databaseThreadInfo
                                            )
{
  assert(databaseThreadInfo != NULL);

  databaseThreadInfo->threadId     = Thread_getCurrentId();
  databaseThreadInfo->count        = 1;
  databaseThreadInfo->fileName     = __fileName__;
  databaseThreadInfo->lineNb       = __lineNb__;
  databaseThreadInfo->cycleCounter = getCycleCounter();
  #ifdef HAVE_BACKTRACE
    BACKTRACE(databaseThreadInfo->stackTrace,databaseThreadInfo->stackTraceSize);
  #endif /* HAVE_BACKTRACE */
}

/***********************************************************************\
* Name   : debugIncrementDatabaseThreadInfo
* Purpose: increment database thread info
* Input  : __fileName__       - file name
*          __lineNb__         - line number
*          databaseThreadInfo - database thread info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugIncrementDatabaseThreadInfo(const char         *__fileName__,
                                                   ulong              __lineNb__,
                                                   DatabaseThreadInfo *databaseThreadInfo
                                                  )
{
  assert(databaseThreadInfo != NULL);

  databaseThreadInfo->count++;
  databaseThreadInfo->fileName     = __fileName__;
  databaseThreadInfo->lineNb       = __lineNb__;
  databaseThreadInfo->cycleCounter = getCycleCounter();
  #ifdef HAVE_BACKTRACE
    BACKTRACE(databaseThreadInfo->stackTrace,databaseThreadInfo->stackTraceSize);
  #endif /* HAVE_BACKTRACE */
}

/***********************************************************************\
* Name   : debugAddDatabaseThreadInfo
* Purpose: add database thread info
* Input  : __fileName__           - file name
*          __lineNb__             - line number
*          databaseThreadInfo     - database thread info
*          databaseThreadInfoSize - size of database thread info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugAddDatabaseThreadInfo(const char         *__fileName__,
                                             ulong              __lineNb__,
                                             DatabaseThreadInfo databaseThreadInfo[],
                                             uint               databaseThreadInfoSize
                                            )
{
  uint i;

  // increment existing
  i = 0;
  while (i < databaseThreadInfoSize)
  {
    if (Thread_isCurrentThread(databaseThreadInfo[i].threadId))
    {
       debugIncrementDatabaseThreadInfo(__fileName__,__lineNb__,&databaseThreadInfo[i]);
       return;
    }
    i++;
  }

  // insert new
  i = 0;
  while (i < databaseThreadInfoSize)
  {
    if (Thread_isNone(databaseThreadInfo[i].threadId))
    {
      debugSetDatabaseThreadInfo(__fileName__,__lineNb__,&databaseThreadInfo[i]);
      return;
    }
    i++;
  }

  HALT_INTERNAL_ERROR("Too many locks (max. %d)!",databaseThreadInfoSize);
}

/***********************************************************************\
* Name   : debugClearDatabaseThreadInfo
* Purpose: clear database thread info
* Input  : databaseThreadInfo     - database thread info
*          databaseThreadInfoSize - size of database thread info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugClearDatabaseThreadInfo(DatabaseThreadInfo databaseThreadInfo[],
                                               uint               databaseThreadInfoSize
                                              )
{
  uint i;

  i = 0;
  while (i < databaseThreadInfoSize)
  {
    if (Thread_isCurrentThread(databaseThreadInfo[i].threadId))
    {
       assert(databaseThreadInfo[i].count > 0);

       databaseThreadInfo[i].count--;
       if (databaseThreadInfo[i].count == 0)
       {
         databaseThreadInfo[i].threadId = THREAD_ID_NONE;
       }
       return;
    }
    i++;
  }

  HALT_INTERNAL_ERROR("Current thread %s not found info thread infos!",Thread_getCurrentIdString());
}

/***********************************************************************\
* Name   : debugAddHistoryDatabaseThreadInfo
* Purpose: add history database thread info
* Input  : __fileName__       - file name
*          __lineNb__         - line number
*          databaseThreadInfo - database thread info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugAddHistoryDatabaseThreadInfo(const char                     *__fileName__,
                                                    ulong                          __lineNb__,
                                                    DatabaseHistoryThreadInfo      databaseHistoryThreadInfo[],
                                                    uint                           *index,
                                                    uint                           databaseHistoryThreadInfoSize,
                                                    DatabaseHistoryThreadInfoTypes type
                                                   )
{
  assert(databaseHistoryThreadInfo != NULL);

  databaseHistoryThreadInfo[*index].threadId     = Thread_getCurrentId();
  databaseHistoryThreadInfo[*index].fileName     = __fileName__;
  databaseHistoryThreadInfo[*index].lineNb       = __lineNb__;
  databaseHistoryThreadInfo[*index].cycleCounter = getCycleCounter();
  databaseHistoryThreadInfo[*index].type         = type;
  #ifdef HAVE_BACKTRACE
    BACKTRACE(databaseHistoryThreadInfo[*index].stackTrace,databaseHistoryThreadInfo[*index].stackTraceSize);
  #endif /* HAVE_BACKTRACE */
  (*index) = ((*index)+1) % databaseHistoryThreadInfoSize;
}

/***********************************************************************\
* Name   : debugPrintLockInfo
* Purpose: print lock info
* Input  : databaseNode - database node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugPrintLockInfo(const DatabaseNode *databaseNode)
{
  uint i;

  assert(databaseNode != NULL);

  pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

  pthread_mutex_lock(&debugDatabaseLock);
  {
    pthread_mutex_lock(&debugConsoleLock);
    {
      fprintf(stderr,"Database lock info '%s':\n",String_cString(databaseNode->databaseSpecifier.sqlite.fileName));
      fprintf(stderr,
              "  lock state summary: pending r %2u, locked r %2u, pending rw %2u, locked rw %2u, transactions %2u\n",
              databaseNode->pendingReadCount,
              databaseNode->readCount,
              databaseNode->pendingReadWriteCount,
              databaseNode->readWriteCount,
              databaseNode->transactionCount
             );
      for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.reads); i++)
      {
        if (!Thread_isNone(databaseNode->debug.reads[i].threadId))
        {
          fprintf(stderr,
                  "    locked  r  thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseNode->debug.reads[i].threadId),
                  Thread_getIdString(databaseNode->debug.reads[i].threadId),
                  databaseNode->debug.reads[i].fileName,
                  databaseNode->debug.reads[i].lineNb
                 );
//TODO: reactivate
#if 0
          fprintf(stderr,
                  "    command: %s\n",
                  String_cString(current.sqlCommand)
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,
                                4,
                                DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                current.stackTrace,
                                current.stackTraceSize,
                                0
                               );
          #endif /* HAVE_BACKTRACE */
//          debugDumpStackTrace(stderr,6,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,databaseNode->reads[i].stackTrace,databaseNode->reads[i].stackTraceSize,0);
#endif
        }
      }
      for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.readWrites); i++)
      {
        if (!Thread_isNone(databaseNode->debug.readWrites[i].threadId))
        {
          fprintf(stderr,
                  "    locked  rw thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseNode->debug.readWrites[i].threadId),
                  Thread_getIdString(databaseNode->debug.readWrites[i].threadId),
                  databaseNode->debug.readWrites[i].fileName,
                  databaseNode->debug.readWrites[i].lineNb
                 );
//TODO: reactivate
#if 0
          fprintf(stderr,
                  "    command: %s\n",
                  String_cString(current.sqlCommand)
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,
                                4,
                                DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                databaseHandle->debug.current.stackTrace,
                                databaseHandle->debug.current.stackTraceSize,
                                0
                               );
          #endif /* HAVE_BACKTRACE */
//          debugDumpStackTrace(stderr,6,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,databaseHandle->databaseNode->readWrites[i].stackTrace,databaseHandle->databaseNode->readWrites[i].stackTraceSize,0);
#endif
        }
      }
    }
    pthread_mutex_unlock(&debugConsoleLock);
  }
  pthread_mutex_unlock(&debugDatabaseLock);
}

#endif /* not NDEBUG */

/***********************************************************************\
* Name   : freeDatabaseNode
* Purpose: free database node
* Input  : databaseNode - database node
*          userData     - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDatabaseNode(DatabaseNode *databaseNode, void *userData)
{
  assert(databaseNode != NULL);

  UNUSED_VARIABLE(userData);

  DEBUG_REMOVE_RESOURCE_TRACE(databaseNode,DatabaseNode);

  Semaphore_done(&databaseNode->progressHandlerList.lock);
  List_done(&databaseNode->progressHandlerList,CALLBACK_(NULL,NULL));
  Semaphore_done(&databaseNode->busyHandlerList.lock);
  List_done(&databaseNode->busyHandlerList,CALLBACK_(NULL,NULL));
  pthread_cond_destroy(&databaseNode->readWriteTrigger);
  #ifdef DATABASE_LOCK_PER_INSTANCE
     pthread_mutex_destroy(&databaseNode->lock);
  #endif /* DATABASE_LOCK_PER_INSTANCE */
  Database_doneSpecifier(&databaseNode->databaseSpecifier);
}

/***********************************************************************\
* Name   : progressHandler
* Purpose: SQLite3 progress handler callback
* Input  : userData - user data
*          n        - number of calls
* Output : -
* Return : 0 for abort
* Notes  : -
\***********************************************************************/

LOCAL int progressHandler(void *userData)
{
  DatabaseHandle                    *databaseHandle = (DatabaseHandle*)userData;
  bool                              interruptFlag;
  const DatabaseProgressHandlerNode *progressHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  // execute registered progress handlers
  interruptFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->databaseNode->progressHandlerList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    LIST_ITERATEX(&databaseHandle->databaseNode->progressHandlerList,progressHandlerNode,!interruptFlag)
    {
      interruptFlag = !progressHandlerNode->function(progressHandlerNode->userData);
    }
  }

  return interruptFlag ? 1 : 0;
}

/***********************************************************************\
* Name   : sqlite3UnixTimestamp
* Purpose: callback for UNIX_TIMESTAMP function to convert date/time to
*          Unix timestamp (Unix epoch, UTC)
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : UNIX_TIMESTAMP(text[,format])
\***********************************************************************/

LOCAL void sqlite3UnixTimestamp(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  const char *text,*format;
  const char *s;
  uint64     timestamp;
  #ifdef HAVE_GETDATE_R
    struct tm tmBuffer;
  #endif /* HAVE_GETDATE_R */
  struct tm  *tm;

  assert(context != NULL);
  assert(argc >= 1);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  // get text to convert, optional date/time format
  text   = (const char*)sqlite3_value_text(argv[0]);
  format = (argc >= 2) ? (const char *)argv[1] : NULL;

  // convert to Unix timestamp
  if (text != NULL)
  {
    // try convert number value
    if (stringToUInt64(text,&timestamp))
    {
      // done
    }
    else
    {
      // try convert string value
      #if   defined(HAVE_GETDATE_R)
        tm = (getdate_r(text,&tmBuffer) == 0) ? &tmBuffer : NULL;
      #elif defined(HAVE_GETDATE)
        tm = getdate(text);
      #else
#ifndef WERROR
#warning implement strptime
#endif
//TODO: use http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/time/strptime.c?rev=HEAD
        tm = NULL;
      #endif /* HAVE_GETDATE... */
      if (tm != NULL)
      {
        #ifdef HAVE_TIMEGM
          timestamp = (uint64)timegm(tm);
        #else
#ifndef WERROR
#warning implement timegm
#endif
        #endif
      }
      else
      {
        #ifdef HAVE_STRPTIME
          s = strptime(text,(format != NULL) ? format : "%Y-%m-%d %H:%M:%S",&tmBuffer);
        #else
UNUSED_VARIABLE(format);
#ifndef WERROR
#warning implement strptime
#endif
//TODO: use http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/time/strptime.c?rev=HEAD
          s = NULL;
        #endif
        if ((s != NULL) && stringIsEmpty(s))
        {
          #ifdef HAVE_TIMEGM
            timestamp = (uint64)timegm(&tmBuffer);
          #else
#ifndef WERROR
#warning implement timegm
#endif
          #endif
        }
        else
        {
          timestamp = 0LL;
        }
      }
    }
  }
  else
  {
    timestamp = 0LL;
  }

  sqlite3_result_int64(context,(int64)timestamp);
}

/***********************************************************************\
* Name   : sqlite3FromUnixTime
* Purpose: callback for FROM_UNIXTIME function to convert Unix timestamp
*          (Unix epoch, UTC) to date/time
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : FROM_UNIXTIME(timestamp[,format])
\***********************************************************************/

LOCAL void sqlite3FromUnixTime(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  uint64     timestamp;
  const char *format;
  char       text[64];

  assert(context != NULL);
  assert(argc >= 1);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  // get text to convert, optional date/time format
  timestamp = (uint64)sqlite3_value_int64(argv[0]);
  format    = (argc >= 2) ? (const char *)argv[1] : NULL;

  // convert to Unix timestamp
  Misc_formatDateTimeCString(text,sizeof(text),timestamp,format);

  sqlite3_result_text(context,text,stringLength(text),NULL);
}

/***********************************************************************\
* Name   : sqlite3FromUnixTime
* Purpose: callback for FROM_UNIXTIME function to convert Unix timestamp
*          (Unix epoch, UTC) to date/time
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : FROM_UNIXTIME(timestamp[,format])
\***********************************************************************/

LOCAL void sqlite3Now(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  char text[64];

  assert(context != NULL);
  assert(argc == 0);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);
  UNUSED_VARIABLE(argv);

  // convert to Unix timestamp
  Misc_formatDateTimeCString(text,sizeof(text),Misc_getCurrentDateTime(),DATE_TIME_FORMAT_DEFAULT);

  sqlite3_result_text(context,text,stringLength(text),NULL);
}

/***********************************************************************\
* Name   : sqlite3RegexpDelete
* Purpose: callback for deleting REGEXP data
* Input  : data - data to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sqlite3RegexpDelete(void *data)
{
  assert(data != NULL);

  regfree((regex_t*)data);
  free(data);
}

/***********************************************************************\
* Name   : sqlite3RegexpMatch
* Purpose: callback for REGEXP function
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sqlite3RegexpMatch(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  const char *text;
  bool       caseSensitive;
  const char *patternText;
  int        flags;
  regex_t    *regex;
  int        result;

  assert(context != NULL);
  assert(argc == 3);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  // get text to match
  text = (const char*)sqlite3_value_text(argv[2]);

  // check if pattern already exists, create pattern
  regex = (regex_t*)sqlite3_get_auxdata(context,0);
  if (regex == NULL)
  {
    // get pattern, case-sensitive flag
    patternText   = (const char*)sqlite3_value_text(argv[0]);
    caseSensitive = atoi((const char*)sqlite3_value_text(argv[1])) != 0;

    // allocate pattern
    regex = (regex_t*)malloc(sizeof(regex_t));
    if (regex == NULL)
    {
      sqlite3_result_int(context,0);
      return;
    }

    // compile pattern
    flags = REG_NOSUB;
    if (!caseSensitive) flags |= REG_ICASE;
    if (regcomp(regex,patternText,flags) != 0)
    {
      sqlite3_result_int(context,0);
      return;
    }

    // store for next usage
    sqlite3_set_auxdata(context,0,regex,sqlite3RegexpDelete);
  }

  // match pattern
  result = (regexec(regex,text,0,NULL,0) == 0) ? 1 : 0;

  sqlite3_result_int(context,result);
}

/***********************************************************************\
* Name   : sqlite3Dirname
* Purpose: callback for DIRNAME function
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sqlite3Dirname(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  const char *string;
  String     directoryName;

  assert(context != NULL);
  assert(argc == 1);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  // get string
  string = (const char*)sqlite3_value_text(argv[0]);

  // get directory
  directoryName = File_getDirectoryNameCString(String_new(),string);

  // store result
  sqlite3_result_text(context,String_cString(directoryName),-1,SQLITE_TRANSIENT);

  // free resources
  String_delete(directoryName);
}

/***********************************************************************\
* Name   : sqlite3Query
* Purpose: do SQLite3 query
* Input  : handle     - SQLite3 handle
*          sqlCommand - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sqlite3Exec(sqlite3    *handle,
                         const char *sqlCommand
                        )
{
  int    sqliteResult;
  Errors error;

  assert(handle != NULL);
  assert(sqlCommand != NULL);

  sqliteResult = sqlite3_exec(handle,
                              sqlCommand,
                              CALLBACK_(NULL,NULL),
                              NULL
                             );
  if      (sqliteResult == SQLITE_MISUSE)
  {
    HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                        sqliteResult,
                        sqlite3_extended_errcode(handle)
                       );
  }
  else if (sqliteResult == SQLITE_INTERRUPT)
  {
    error = ERRORX_(INTERRUPTED,sqlite3_errcode(handle),
                    "%s: %s",
                    sqlite3_errmsg(handle),
                    sqlCommand
                   );
  }
  else if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(handle),
                    "%s: %s",
                    sqlite3_errmsg(handle),
                    sqlCommand
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : sqlite3StatementPrepare
* Purpose: prepare SQLite3 statement
* Input  : statementHandle - statement handle variable
*          handle          - SQLite3 handle
*          sqlCommand      - SQL command
* Output : statementHandle - statement handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sqlite3StatementPrepare(sqlite3_stmt **statementHandle,
                                     sqlite3      *handle,
                                     const char   *sqlCommand
                                    )
{
  int    sqliteResult;
  Errors error;

  assert(statementHandle != NULL);
  assert(handle != NULL);
  assert(sqlCommand != NULL);

  sqliteResult = sqlite3_prepare_v2(handle,
                                    sqlCommand,
                                    -1,
                                    statementHandle,
                                    NULL
                                   );
  #ifndef NDEBUG
    if ((*statementHandle) == NULL)
    {
      HALT_INTERNAL_ERROR("SQLite prepare fail %d: %s: %s",
                          sqlite3_errcode(handle),
                          sqlite3_errmsg(handle),
                          sqlCommand
                         );
    }
  #endif /* not NDEBUG */
  if      (sqliteResult == SQLITE_MISUSE)
  {
    HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d: %s",
                        sqliteResult,sqlite3_extended_errcode(handle),
                        sqlCommand
                       );
  }
  else if (sqliteResult == SQLITE_INTERRUPT)
  {
    error = ERRORX_(INTERRUPTED,sqlite3_errcode(handle),
                    "%s: %s",
                    sqlite3_errmsg(handle),
                    sqlCommand
                   );
  }
  else if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(handle),
                    "%s: %s",
                    sqlite3_errmsg(handle),
                    sqlCommand
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : sqlite3UnlockNotifyCallback
* Purpose: SQLite3 unlock notify callback
* Input  : argv - arguments
*          argc - number of arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sqlite3UnlockNotifyCallback(void *argv[], int argc)
{
  sem_t *semaphore;
  assert(argv != NULL);
  assert(argc >= 1);

  UNUSED_VARIABLE(argc);

  semaphore = (sem_t*)argv[0];

  assert(semaphore != NULL);
  sem_post(semaphore);
}

/***********************************************************************\
* Name   : waitUnlockNotify
* Purpose: wait unlock notify
* Input  : handle - SQLite3 handle
* Output : -
* Return : SQLite result code
* Notes  : -
\***********************************************************************/

LOCAL int sqlite3WaitUnlockNotify(sqlite3 *handle)
{
  int   sqliteResult;
  sem_t semaphore;

  // init variables
  sem_init(&semaphore,0,0);

  // register call-back
  sqliteResult = sqlite3_unlock_notify(handle,sqlite3UnlockNotifyCallback,&semaphore);
  if (sqliteResult != SQLITE_OK)
  {
    sem_destroy(&semaphore);
    return sqliteResult;
  }

  // wait for notify
  sem_wait(&semaphore);

  // free resources
  sem_destroy(&semaphore);

  return SQLITE_OK;
}

/***********************************************************************\
* Name   : sqlite3Step
* Purpose: do SQLite3 step
* Input  : statementHandle - statement handle
*          handle          - SQLite3 handle
*          sqlCommand      - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

// TODO: use
LOCAL Errors sqlite3Step(sqlite3_stmt *statementHandle,
                         sqlite3      *handle,
                         long         timeout
                        )
{
  const uint SLEEP_TIME = 250;  // [ms]

  uint   n;
  int    sqliteResult;
  Errors error;

  assert(statementHandle != NULL);
  assert(handle != NULL);

  n = 0;
  do
  {
    sqliteResult = sqlite3_step(statementHandle);
    if (sqliteResult == SQLITE_LOCKED)
    {
      sqlite3WaitUnlockNotify(handle);
      sqlite3_reset(statementHandle);
    }
//TODO: correct? abort here?
    else if (sqliteResult == SQLITE_BUSY)
    {
      Misc_udelay(SLEEP_TIME*US_PER_MS);
      sqlite3_reset(statementHandle);
      n++;
    }
  }
  while (   ((sqliteResult == SQLITE_LOCKED) || (sqliteResult == SQLITE_BUSY))
         && ((timeout == WAIT_FOREVER) || (n < (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME)))
        );

  if      ((sqliteResult == SQLITE_OK) || (sqliteResult == SQLITE_DONE))
  {
    error = ERROR_NONE;
  }
  else if (sqliteResult == SQLITE_LOCKED)
  {
// TODO:
  }
  else if (sqliteResult == SQLITE_MISUSE)
  {
    HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                        sqliteResult,
                        sqlite3_extended_errcode(handle)
                       );
  }
  else if (sqliteResult == SQLITE_INTERRUPT)
  {
    error = ERRORX_(INTERRUPTED,
                    sqlite3_errcode(handle),
                    "%s",
                    sqlite3_errmsg(handle)
                   );
  }
  else
  {
    error = ERRORX_(DATABASE,
                    sqlite3_errcode(handle),
                    "%s",
                    sqlite3_errmsg(handle)
                   );
  }

  return error;
}

/***********************************************************************\
* Name   : mysqlQuery
* Purpose: do MariaDB query
* Input  : handle     - MySQL handle
*          sqlCommand - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors mysqlQuery(MYSQL      *handle,
                        const char *sqlCommand
                       )
{
  int    mysqlResult;
  Errors error;

  assert(handle != NULL);
  assert(sqlCommand != NULL);

  mysqlResult = mysql_query(handle,sqlCommand);
  if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
  {
    HALT_INTERNAL_ERROR("MariaDB library reported misuse %d %s",
                        mysqlResult,
                        mysql_error(handle)
                       );
  }
  else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
  {
    error = ERRORX_(DATABASE_CONNECTION_LOST,
                    mysql_errno(handle),
                    "%s: %s",
                    mysql_error(handle),
                    sqlCommand
                   );
  }
  else if (mysqlResult != 0)
  {
    error = ERRORX_(DATABASE,
                    mysql_errno(handle),
                    "%s: %s",
                    mysql_error(handle),
                    sqlCommand
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : mysqlSelectDatabase_
* Purpose: select MariaDB database
* Input  : handle     - MySQL handle
*          sqlCommand - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors mysqlSelectDatabase(MYSQL      *handle,
                                   const char *databaseName
                                  )
{
  int    mysqlResult;
  Errors error;

  assert(handle != NULL);
  assert(databaseName != NULL);

  mysqlResult = mysql_select_db(handle,databaseName);
  if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
  {
    HALT_INTERNAL_ERROR("MariaDB library reported misuse %d: %s",
                        mysqlResult,
                        mysql_error(handle)
                       );
  }
  else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
  {
    error = ERRORX_(DATABASE_CONNECTION_LOST,
                    mysql_errno(handle),
                    "%s",
                    mysql_error(handle)
                   );
  }
  else if (mysqlResult != 0)
  {
    error = ERRORX_(DATABASE,
                    mysql_errno(handle),
                    "%s",
                    mysql_error(handle)
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : mysqlSetCharacterSet
* Purpose: set MariaDB character set
* Input  : handle       - MySQL handle
*          characterSet - character set name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors mysqlSetCharacterSet(MYSQL      *handle,
                                  const char *characterSet
                                 )
{
  int    mysqlResult;
  Errors error;

  assert(handle != NULL);
  assert(characterSet != NULL);

  mysqlResult = mysql_set_character_set(handle,characterSet);
  if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
  {
    HALT_INTERNAL_ERROR("MariaDB library reported misuse %d: %s",
                        mysqlResult,
                        mysql_error(handle)
                       );
  }
  else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
  {
    error = ERRORX_(DATABASE_CONNECTION_LOST,
                    mysql_errno(handle),
                    "%s",
                    mysql_error(handle)
                   );
  }
  else if (mysqlResult != 0)
  {
    error = ERRORX_(DATABASE,
                    mysql_errno(handle),
                    "%s",
                    mysql_error(handle)
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : mysqlStatementPrepare
* Purpose: prepare MariaDB statement
* Input  : statementHandle - statement handle
*          sqlCommand - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors mysqlStatementPrepare(MYSQL_STMT *statementHandle,
                                     const char *sqlCommand
                                    )
{
  int    mysqlResult;
  Errors error;

  assert(statementHandle != NULL);
  assert(sqlCommand != NULL);

  mysqlResult = mysql_stmt_prepare(statementHandle,
                                   sqlCommand,
                                   stringLength(sqlCommand)
                                  );
  if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
  {
    HALT_INTERNAL_ERROR("MariaDB library reported misuse %d: %s: %s",
                        mysqlResult,
                        mysql_stmt_error(statementHandle),
                        sqlCommand
                       );
  }
  else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
  {
    error = ERRORX_(DATABASE_CONNECTION_LOST,
                    mysql_stmt_errno(statementHandle),
                    "%s: %s",
                    mysql_stmt_error(statementHandle),
                    sqlCommand
                   );
  }
  else if (mysqlResult != 0)
  {
    error = ERRORX_(DATABASE,
                    mysql_stmt_errno(statementHandle),
                    "%s: %s",
                    mysql_stmt_error(statementHandle),
                    sqlCommand
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : mariadbStatementExecute
* Purpose: execute MariaDB statement
* Input  : statementHandle - statement handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors mariadbStatementExecute(MYSQL_STMT *statementHandle)
{
  int    mysqlResult;
  Errors error;

  assert(statementHandle != NULL);

  mysqlResult = mysql_stmt_execute(statementHandle);
  if      (mysqlResult == 0)
  {
    error = ERROR_NONE;
  }
  else if (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
  {
    HALT_INTERNAL_ERROR("MariaDB library reported misuse %d: %s",
                        mysqlResult,
                        mysql_stmt_error(statementHandle)
                       );
  }
  else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
  {
    error = ERRORX_(DATABASE_CONNECTION_LOST,
                    mysql_stmt_errno(statementHandle),
                    "%s",
                    mysql_stmt_error(statementHandle)
                   );
  }
  else if (mysqlResult != 0)
  {
    error = ERRORX_(DATABASE,
                    mysql_stmt_errno(statementHandle),
                    "%s",
                    mysql_stmt_error(statementHandle)
                   );
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

/***********************************************************************\
* Name   : openDatabase
* Purpose: open database
* Input  : databaseHandle    - database handle variable
*          databaseSpecifier - database specifier
*          databaseName      - database name or NULL for database name in
*                              database specifider
*          openDatabaseMode  - open mode; see DatabaseOpenModes
*          timeout           - timeout [ms] or WAIT_FOREVER
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openDatabase(DatabaseHandle          *databaseHandle,
                            const DatabaseSpecifier *databaseSpecifier,
                            ConstString             databaseName,
                            DatabaseOpenModes       openDatabaseMode,
                            long                    timeout
                          )
#else /* not NDEBUG */
  LOCAL Errors openDatabase(const char              *__fileName__,
                            ulong                   __lineNb__,
                            DatabaseHandle          *databaseHandle,
                            const DatabaseSpecifier *databaseSpecifier,
                            ConstString             databaseName,
                            DatabaseOpenModes       openDatabaseMode,
                            long                    timeout
                          )
#endif /* NDEBUG */
{
  String        directoryName;
  Errors        error;
  DatabaseNode  *databaseNode;
  #ifndef NDEBUG
    uint          i;
  #endif /* NDEBUG */

  assert(databaseHandle != NULL);
  assert(databaseSpecifier != NULL);

  // init variables
  databaseHandle->readLockCount           = 0;
  databaseHandle->readWriteLockCount      = 0;
  databaseHandle->sqlite.handle           = NULL;
  databaseHandle->timeout                 = timeout;
  databaseHandle->enabledSync             = FALSE;
  databaseHandle->enabledForeignKeys      = FALSE;
  databaseHandle->lastCheckpointTimestamp = Misc_getTimestamp();
  if (sem_init(&databaseHandle->wakeUp,0,0) != 0)
  {
    return ERRORX_(DATABASE,0,"init locking");
  }

  #if defined(HAVE_MARIADB)
    databaseHandle->mysql.handle = NULL;
  #endif /* HAVE_MARIADB */

  // get database node
  SEMAPHORE_LOCKED_DO(&databaseList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    databaseNode = LIST_FIND(&databaseList,
                             databaseNode,
                             Database_equalSpecifiers(&databaseNode->databaseSpecifier,databaseSpecifier)
                            );
    if (databaseNode != NULL)
    {
      databaseNode->openCount++;
    }
    else
    {
      databaseNode = LIST_NEW_NODE(DatabaseNode);
      if (databaseNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      Database_copySpecifier(&databaseNode->databaseSpecifier,databaseSpecifier);
      databaseNode->openCount               = 1;
      #ifdef DATABASE_LOCK_PER_INSTANCE
        if (pthread_mutexattr_init(&databaseNode->lockAttribute) != 0)
        {
          Database_doneSpecifier(&databaseNode->databaseSpecifier);
          LIST_DELETE_NODE(databaseNode);
          switch (databaseType)
          {
            case DATABASE_TYPE_SQLITE3:
              sqlite3_close(databaseHandle->sqlite.handle);
              break;
            case DATABASE_TYPE_MYSQL:
              #if defined(HAVE_MARIADB)
                mariadb_close(databaseHandle->mysql.handle);
              #else /* HAVE_MARIADB */
              #endif /* HAVE_MARIADB */
              break;
          }
          sem_destroy(&databaseHandle->wakeUp);
          return ERRORX_(DATABASE,0,"init locking");
        }
        pthread_mutexattr_settype(&databaseLockAttribute,PTHREAD_MUTEX_RECURSIVE);
        if (pthread_mutex_init(&databaseNode->lock,&databaseNode->lockAttribute) != 0)
        {
          pthread_mutexattr_destroy(&databaseNode->lockAttribute);
          Database_doneSpecifier(&databaseNode->databaseSpecifier);
          LIST_DELETE_NODE(databaseNode);
          switch (databaseType)
          {
            case DATABASE_TYPE_SQLITE3:
              sqlite3_close(databaseHandle->sqlite.handle);
              break;
            case DATABASE_TYPE_MYSQL:
              #if defined(HAVE_MARIADB)
                mariadb_close(databaseHandle->mysql.handle);
              #else /* HAVE_MARIADB */
              #endif /* HAVE_MARIADB */
              break;
          }
          sem_destroy(&databaseHandle->wakeUp);
          return ERRORX_(DATABASE,0,"init locking");
        }
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      databaseNode->lockType                = DATABASE_LOCK_TYPE_NONE;

      databaseNode->pendingReadCount        = 0;
      databaseNode->readCount               = 0;
      pthread_cond_init(&databaseNode->readTrigger,NULL);

      databaseNode->pendingReadWriteCount   = 0;
      databaseNode->readWriteCount          = 0;
      pthread_cond_init(&databaseNode->readWriteTrigger,NULL);

      databaseNode->pendingTransactionCount = 0;
      databaseNode->transactionCount        = 0;
      pthread_cond_init(&databaseNode->transactionTrigger,NULL);

      List_init(&databaseNode->busyHandlerList);
      Semaphore_init(&databaseNode->busyHandlerList.lock,SEMAPHORE_TYPE_BINARY);

      List_init(&databaseNode->progressHandlerList);
      Semaphore_init(&databaseNode->progressHandlerList.lock,SEMAPHORE_TYPE_BINARY);

      #ifdef DATABASE_DEBUG_LOCK
        memClear(databaseNode->readLPWIds,sizeof(databaseNode->readLPWIds));
        memClear(databaseNode->readWriteLPWIds,sizeof(databaseNode->readWriteLPWIds));
        databaseNode->transactionLPWId = 0;
      #endif /* DATABASE_DEBUG_LOCK */

      #ifndef NDEBUG
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.pendingReads);      i++) databaseNode->debug.pendingReads[i].threadId      = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.reads);             i++) databaseNode->debug.reads[i].threadId             = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.pendingReadWrites); i++) databaseNode->debug.pendingReadWrites[i].threadId = THREAD_ID_NONE;
        databaseNode->debug.readWriteLockedBy               = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.readWrites);        i++) databaseNode->debug.readWrites[i].threadId        = THREAD_ID_NONE;
        databaseNode->debug.lastTrigger.threadInfo.threadId = THREAD_ID_NONE;
        databaseNode->debug.transaction.threadId            = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.history);           i++) databaseNode->debug.history[i].threadId           = THREAD_ID_NONE;
        databaseNode->debug.historyIndex                    = 0;
      #endif /* not NDEBUG */

      List_append(&databaseList,databaseNode);

      #ifdef NDEBUG
        DEBUG_ADD_RESOURCE_TRACE(databaseNode,DatabaseNode);
      #else /* not NDEBUG */
        DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseNode,DatabaseNode);
      #endif /* NDEBUG */
    }

    databaseHandle->databaseNode = databaseNode;
  }
  assert(databaseHandle->databaseNode != NULL);

  // get database type and open/connect
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      {
        ConstString fileName;
        String      sqliteName;
        int         sqliteMode;
        int         sqliteResult;

        // get filename
        if (!String_isEmpty(databaseName))
        {
          fileName = databaseName;
        }
        else
        {
          fileName = databaseSpecifier->sqlite.fileName;
        }

        // create directory if needed
        if (!String_isEmpty(fileName))
        {
          directoryName = File_getDirectoryName(String_new(),fileName);
          if (   !String_isEmpty(directoryName)
              && !File_isDirectory(directoryName)
             )
          {
            error = File_makeDirectory(directoryName,
                                       FILE_DEFAULT_USER_ID,
                                       FILE_DEFAULT_GROUP_ID,
                                       FILE_DEFAULT_PERMISSION,
                                       FALSE
                                      );
            if (error != ERROR_NONE)
            {
              File_deleteFileName(directoryName);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }
          }
          String_delete(directoryName);
        }

        // create database
        if (   !String_isEmpty(fileName)
            && ((openDatabaseMode & DATABASE_OPEN_MASK_MODE) == DATABASE_OPEN_MODE_FORCE_CREATE)
           )
        {
          // delete existing file
          (void)File_delete(fileName,FALSE);
        }

        // check if exists
        if (   !String_isEmpty(fileName)
            && ((openDatabaseMode & DATABASE_OPEN_MASK_MODE) == DATABASE_OPEN_MODE_CREATE)
           )
        {
          if (File_exists(fileName))
          {
            return ERROR_DATABASE_EXISTS;
          }
        }

        // get sqlite database name
        if (!String_isEmpty(fileName))
        {
          // open file
          sqliteName = String_format(String_new(),"file:%S",fileName);
        }
        else
        {
          // open memory
          sqliteName = String_format(String_new(),"file::memory:");
        }

        // get mode
        sqliteMode = SQLITE_OPEN_URI;
        switch (openDatabaseMode & DATABASE_OPEN_MASK_MODE)
        {
          case DATABASE_OPEN_MODE_READ:
            sqliteMode |= SQLITE_OPEN_READONLY;
            String_appendCString(sqliteName,"?immutable=1");
            break;
          case DATABASE_OPEN_MODE_CREATE:
          case DATABASE_OPEN_MODE_FORCE_CREATE:
          case DATABASE_OPEN_MODE_READWRITE:
            sqliteMode |= SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
            break;
          default:
            break;
        }
        if ((openDatabaseMode & DATABASE_OPEN_MASK_FLAGS) != 0)
        {
          if ((openDatabaseMode & DATABASE_OPEN_MODE_MEMORY) == DATABASE_OPEN_MODE_MEMORY) sqliteMode |= SQLITE_OPEN_MEMORY;//String_appendCString(sqliteName,"mode=memory");
          if ((openDatabaseMode & DATABASE_OPEN_MODE_SHARED) == DATABASE_OPEN_MODE_SHARED) sqliteMode |= SQLITE_OPEN_SHAREDCACHE;//String_appendCString(sqliteName,"cache=shared");
        }
//sqliteMode |= SQLITE_OPEN_NOMUTEX;

        // open database
        sqliteResult = sqlite3_open_v2(String_cString(sqliteName),&databaseHandle->sqlite.handle,sqliteMode,NULL);
        if (sqliteResult != SQLITE_OK)
        {
          error = ERRORX_(DATABASE,
                          sqlite3_errcode(databaseHandle->sqlite.handle),
                          "%s: '%s'",
                          sqlite3_errmsg(databaseHandle->sqlite.handle),
                          String_cString(sqliteName)
                         );
          String_delete(sqliteName);
          sem_destroy(&databaseHandle->wakeUp);
          return error;
        }

        // attach aux database
        if ((openDatabaseMode & DATABASE_OPEN_MODE_AUX) == DATABASE_OPEN_MODE_AUX)
        {
          error = sqlite3Exec(databaseHandle->sqlite.handle,
                              "ATTACH DATABASE ':memory:' AS " DATABASE_AUX
                             );
          if (error != ERROR_NONE)
          {
            sqlite3_close(databaseHandle->sqlite.handle);
            String_delete(sqliteName);
            sem_destroy(&databaseHandle->wakeUp);
            return error;
          }
        }

        // free resources
        String_delete(sqliteName);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          union
          {
            bool b;
            uint u;
          }     optionValue;
          ulong serverVersion;
          char  sqlCommand[256];

          SEMAPHORE_LOCKED_DO(&databaseList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            // open database
            databaseHandle->mysql.handle = mysql_init(NULL);
            if (databaseHandle->mysql.handle == NULL)
            {
              error = ERROR_DATABASE;
              Semaphore_unlock(&databaseList.lock);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }
            optionValue.b = TRUE;
            mysql_options(databaseHandle->mysql.handle,MYSQL_OPT_RECONNECT,&optionValue);
            optionValue.u = MARIADB_TIMEOUT;
            mysql_options(databaseHandle->mysql.handle,MYSQL_OPT_READ_TIMEOUT,&optionValue);
            mysql_options(databaseHandle->mysql.handle,MYSQL_OPT_WRITE_TIMEOUT,&optionValue);

            // connect
            if (mysql_real_connect(databaseHandle->mysql.handle,
                                   String_cString(databaseSpecifier->mysql.serverName),
                                   String_cString(databaseSpecifier->mysql.userName),
                                   databaseSpecifier->mysql.password.data,
                                   NULL,  // databaseName
                                   0,
                                   0,
                                   0
                                  ) == NULL
               )
            {
              error = ERRORX_(DATABASE,
                              mysql_errno(databaseHandle->mysql.handle),
                              "%s",
                              mysql_error(databaseHandle->mysql.handle)
                             );
              mysql_close(databaseHandle->mysql.handle);
              Semaphore_unlock(&databaseList.lock);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }

            // check min. version
            serverVersion = mysql_get_server_version(databaseHandle->mysql.handle);
            if (serverVersion < MARIADB_MIN_SERVER_VERSION)
            {
              error = ERRORX_(DATABASE_VERSION,0,"available %lu, required %lu",
                              serverVersion,
                              MARIADB_MIN_SERVER_VERSION
                             );
              mysql_close(databaseHandle->mysql.handle);
              Semaphore_unlock(&databaseList.lock);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }

            // enable UTF8
            error = mysqlSetCharacterSet(databaseHandle->mysql.handle,
                                         "utf8mb4"
                                        );
            if (error != ERROR_NONE)
            {
              mysql_close(databaseHandle->mysql.handle);
              Semaphore_unlock(&databaseList.lock);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }

            // create database if requested
            if ((openDatabaseMode & DATABASE_OPEN_MASK_MODE) == DATABASE_OPEN_MODE_FORCE_CREATE)
            {
              uint i;

              /* try to create with character set uft8mb4 (4-byte UTF8),
                 then utf8 as a fallback for older MariaDB versions.
              */
              i = 0;
              do
              {
                stringFormat(sqlCommand,sizeof(sqlCommand),
                             "CREATE DATABASE IF NOT EXISTS %s \
                              CHARACTER SET '%s' \
                              COLLATE '%s_bin' \
                             ",
                             !String_isEmpty(databaseName)
                               ? String_cString(databaseName)
                               : String_cString(databaseSpecifier->mysql.databaseName),
                             MARIADB_CHARACTER_SETS[i],
                             MARIADB_CHARACTER_SETS[i]
                            );
                error = mysqlQuery(databaseHandle->mysql.handle,
                                   sqlCommand
                                  );
                i++;
              }
              while (   (error != ERROR_NONE)
                     && (i < SIZE_OF_ARRAY(MARIADB_CHARACTER_SETS))
                    );
              if (error != ERROR_NONE)
              {
                mysql_close(databaseHandle->mysql.handle);
                Semaphore_unlock(&databaseList.lock);
                sem_destroy(&databaseHandle->wakeUp);
                return error;
              }
            }

            // select database
            error = mysqlSelectDatabase(databaseHandle->mysql.handle,
                                        !String_isEmpty(databaseName)
                                          ? String_cString(databaseName)
                                          : String_cString(databaseSpecifier->mysql.databaseName)
                                       );
            if (error != ERROR_NONE)
            {
              mysql_close(databaseHandle->mysql.handle);
              Semaphore_unlock(&databaseList.lock);
              sem_destroy(&databaseHandle->wakeUp);
              return error;
            }
          }
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  // set handlers
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      #if !defined(NDEBUG) && defined(DATABASE_DEBUG_LOG)
        fprintf(stderr,"WARNING: datatbase logging is enabled!\n");
        switch (databaseNode->type)
        {
          case DATABASE_TYPE_SQLITE3:
            sqlite3_trace_v2(databaseHandle->sqlite.handle,DATABASE_DEBUG_LOG,logTraceCommandHandler,NULL);
            break;
          case DATABASE_TYPE_MYSQL:
            #if defined(HAVE_MARIADB)
            #else /* HAVE_MARIADB */
            #endif /* HAVE_MARIADB */
            break;
        }
      #endif /* DATABASE_DEBUG_LOG */
// TODO:
#if 0
      // set busy handler
      sqliteResult = sqlite3_busy_handler(databaseHandle->sqlite.handle,busyHandler,databaseHandle);
      assert(sqliteResult == SQLITE_OK);
#endif /* 0 */
      // set progress handler
      sqlite3_progress_handler(databaseHandle->sqlite.handle,50000,progressHandler,databaseHandle);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  // register special functions
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int sqliteResult;

        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "unix_timestamp",
                                               1,
                                               SQLITE_ANY,
                                               NULL,
                                               sqlite3UnixTimestamp,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "from_unixtime",
                                               1,
                                               SQLITE_ANY,
                                               NULL,
                                               sqlite3FromUnixTime,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "now",
                                               0,
                                               SQLITE_ANY,
                                               NULL,
                                               sqlite3Now,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "regexp",
                                               3,
                                               SQLITE_ANY,
                                               NULL,
                                               sqlite3RegexpMatch,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "dirname",
                                               1,
                                               SQLITE_ANY,
                                               NULL,
                                               sqlite3Dirname,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
//TODO: debug function for logging?
#if 0
        sqliteResult = sqlite3_create_function(databaseHandle->sqlite.handle,
                                               "debugPrint",
                                               1,
                                               SQLITE_ANY,
                                               NULL,
                                               debugPrint,
                                               NULL,
                                               NULL
                                              );
        assert(sqliteResult == SQLITE_OK);
#endif

        UNUSED_VARIABLE(sqliteResult);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  // specific settings
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int sqliteResult;

        // enable recursive triggers
        error = sqlite3Exec(databaseHandle->sqlite.handle,
                            "PRAGMA recursive_triggers=ON"
                           );
        assert(error == ERROR_NONE);

        UNUSED_VARIABLE(sqliteResult);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          int mariadbResult;

          // set SQL mode: allow null dates, disable strict to allow automatic cut of too long values
          mariadbResult = mysql_query(databaseHandle->mysql.handle,
  // TODO:
          // ONLY_FULL_GROUP_BY
                                    "SET SESSION sql_mode='ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'"
                                   );
          assert(mariadbResult == 0);
          UNUSED_VARIABLE(mariadbResult);

  // TODO:
  bool b = FALSE;
  mysql_options(databaseHandle->mysql.handle,
                MYSQL_REPORT_DATA_TRUNCATION,
                &b);
        }
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: open '%s'\n",fileName);
  #endif

  #ifndef NDEBUG
    pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

    pthread_mutex_lock(&debugDatabaseLock);
    {
      // init database handle
      databaseHandle->debug.threadId           = Thread_getCurrentId();
      databaseHandle->debug.fileName           = __fileName__;
      databaseHandle->debug.lineNb             = __lineNb__;
      #ifdef HAVE_BACKTRACE
        BACKTRACE(databaseHandle->debug.stackTrace,databaseHandle->debug.stackTraceSize);
      #endif /* HAVE_BACKTRACE */

      databaseHandle->debug.locked.threadId   = THREAD_ID_NONE;
      databaseHandle->debug.locked.lineNb     = 0;
      databaseHandle->debug.locked.t0         = 0ULL;
      databaseHandle->debug.locked.t1         = 0ULL;
      databaseHandle->debug.current.sqlString = String_new();
      #ifdef HAVE_BACKTRACE
        databaseHandle->debug.current.stackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */

      // add to handle-list
      List_append(&debugDatabaseHandleList,databaseHandle);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* not NDEBUG */

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeDatabase
* Purpose: close database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL void closeDatabase(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  LOCAL void closeDatabase(const char     *__fileName__,
                           ulong          __lineNb__,
                           DatabaseHandle *databaseHandle
                          )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DatabaseHandle *debugDatabaseHandle;
  #endif /* not NDEBUG */

  assert(databaseHandle != NULL);
  assert(databaseHandle->readWriteLockCount == 0);
  assert(databaseHandle->readLockCount == 0);
  assert(checkDatabaseInitialized(databaseHandle));

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: close '%s'\n",databaseHandle->name);
  #endif

  #ifndef NDEBUG
    pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

    pthread_mutex_lock(&debugDatabaseLock);
    {
      // check if database opened
      if (!LIST_CONTAINS(&debugDatabaseHandleList,
                         debugDatabaseHandle,
                         debugDatabaseHandle == databaseHandle
                        )
         )
      {
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,
                              0,
                              DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                              databaseHandle->debug.stackTrace,
                              databaseHandle->debug.stackTraceSize,
                              0
                             );
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR_AT(__fileName__,
                               __lineNb__,
                               "Database %p is not opened",
                               databaseHandle
                              );
      }

      // remove from handle-list
      List_remove(&debugDatabaseHandleList,databaseHandle);

      // free resources
      String_delete(databaseHandle->debug.current.sqlString);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* not NDEBUG */

  switch (databaseHandle->databaseNode->databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      // clear progress handler
      sqlite3_progress_handler(databaseHandle->sqlite.handle,0,NULL,NULL);

      // clear busy timeout handler
      sqlite3_busy_handler(databaseHandle->sqlite.handle,NULL,NULL);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  // close database
  switch (databaseHandle->databaseNode->databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      sqlite3_close(databaseHandle->sqlite.handle);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        mysql_close(databaseHandle->mysql.handle);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

//// TODO: free database node
//  Database_doneSpecifier(&databaseNode->databaseSpecifier);
//mysql_close(databaseHandle->mysql.handle);

  // free resources
//TODO: remove?
  sem_destroy(&databaseHandle->wakeUp);
}

/***********************************************************************\
* Name   : isReadLock
* Purpose: check if read lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff read lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isReadLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->databaseNode->readCount >= databaseHandle->readLockCount);

  return (databaseHandle->databaseNode->readCount > 0);
}

//TODO: not used, remove?
#if 0
/***********************************************************************\
* Name   : isPendingReadWriteLock
* Purpose: check if pending read/write lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff read/write lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isPendingReadWriteLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->databaseNode->readWriteCount >= databaseHandle->readWriteLockCount);
  assert(   (databaseHandle->databaseNode->readWriteCount == 0)
         || !Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy)
        );

  return (databaseHandle->databaseNode->pendingReadWriteCount > 0);
}
#endif

/***********************************************************************\
* Name   : isReadWriteLock
* Purpose: check if read/write lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff read/write lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isReadWriteLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->databaseNode->readWriteCount >= databaseHandle->readWriteLockCount);
  assert(   (databaseHandle->databaseNode->readWriteCount == 0)
         || !Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy)
        );

  return (databaseHandle->databaseNode->readWriteCount > 0);
}

#if 0
//TODO: remove?
/***********************************************************************\
* Name   : isTransactionLock
* Purpose: check if transaction lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff transaction lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isTransactionLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  return (databaseHandle->databaseNode->transactionCount > 0);
}
#endif

/***********************************************************************\
* Name   : isOwnReadWriteLock
* Purpose: check if owner of read/write lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff owner of read/write lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isOwnReadLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->databaseNode->readCount >= databaseHandle->readLockCount);

  return (databaseHandle->readLockCount > 0);
}

/***********************************************************************\
* Name   : isOwnReadWriteLock
* Purpose: check if owner of read/write lock
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff owner of read/write lock
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isOwnReadWriteLock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->databaseNode->readWriteCount >= databaseHandle->readWriteLockCount);
  assert(   (databaseHandle->databaseNode->readWriteCount == 0)
         || !Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy)
        );

// TODO: reactivate when each thread has his own index handle
//  return    (databaseHandle->databaseNode->readWriteCount > 0)
//         && Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy);
  return    (databaseHandle->readWriteLockCount > 0);
}

/***********************************************************************\
* Name   : pendingReadsIncrement
* Purpose: increment database pending read
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void pendingReadsIncrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __pendingReadsIncrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  databaseHandle->databaseNode->pendingReadCount++;
  #ifndef NDEBUG
    debugAddDatabaseThreadInfo(__fileName__,__lineNb__,
                               databaseHandle->databaseNode->debug.pendingReads,
                               SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.pendingReads)
                              );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : pendingReadsDecrement
* Purpose: decrement database pending read
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void pendingReadsDecrement(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  assert(databaseHandle->databaseNode->pendingReadCount > 0);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  if (databaseHandle->databaseNode->pendingReadCount <= 0) HALT_INTERNAL_ERROR("pending read count");
  databaseHandle->databaseNode->pendingReadCount--;
  #ifndef NDEBUG
    debugClearDatabaseThreadInfo(databaseHandle->databaseNode->debug.pendingReads,
                                 SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.pendingReads)
                                );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : pendingReadWritesIncrement
* Purpose: increment database pending read/write
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void pendingReadWritesIncrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __pendingReadWritesIncrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  databaseHandle->databaseNode->pendingReadWriteCount++;
  #ifndef NDEBUG
    debugAddDatabaseThreadInfo(__fileName__,__lineNb__,
                               databaseHandle->databaseNode->debug.pendingReadWrites,
                               SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.pendingReadWrites)
                              );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : pendingReadWritesIncrement
* Purpose: increment database pending read/write
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void pendingReadWritesDecrement(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  assert(databaseHandle->databaseNode->pendingReadWriteCount > 0);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  if (databaseHandle->databaseNode->pendingReadWriteCount <= 0) HALT_INTERNAL_ERROR("pending read/write count");
  databaseHandle->databaseNode->pendingReadWriteCount--;
  #ifndef NDEBUG
    debugClearDatabaseThreadInfo(databaseHandle->databaseNode->debug.pendingReadWrites,
                                 SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.pendingReadWrites)
                                );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : readsIncrement
* Purpose: increment database pending read
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void readsIncrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __readsIncrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  assert(databaseHandle->databaseNode->readCount >= databaseHandle->readLockCount);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  databaseHandle->readLockCount++;
  databaseHandle->databaseNode->readCount++;
  #ifdef DATABASE_DEBUG_LOCK
    debugAddThreadLWPId(databaseHandle->databaseNode->readLPWIds,SIZE_OF_ARRAY(databaseHandle->databaseNode->readLPWIds));
  #endif
  #ifndef NDEBUG
    databaseHandle->debug.locked.threadId = Thread_getCurrentId();
    databaseHandle->debug.locked.fileName = __fileName__;
    databaseHandle->debug.locked.lineNb   = __lineNb__;
    databaseHandle->debug.locked.text[0]  = '\0';
    databaseHandle->debug.locked.t0       = Misc_getTimestamp();

    debugAddDatabaseThreadInfo(__fileName__,__lineNb__,
                               databaseHandle->databaseNode->debug.reads,
                               SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.reads)
                              );
    debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                      databaseHandle->databaseNode->debug.history,
                                      &databaseHandle->databaseNode->debug.historyIndex,
                                      SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                      DATABASE_HISTORY_TYPE_LOCK_READ
                                     );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : readsDecrement
* Purpose: decrement database read
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void readsDecrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __readsDecrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  DATABASE_DEBUG_LOCK_ASSERT(databaseHandle,isReadLock(databaseHandle));
  assert(databaseHandle->databaseNode->readCount >= databaseHandle->readLockCount);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  if (databaseHandle->databaseNode->readCount <= 0) HALT_INTERNAL_ERROR("read count");
  databaseHandle->databaseNode->readCount--;
  #ifdef DATABASE_DEBUG_LOCK
    debugRemoveThreadLWPId(databaseHandle->databaseNode->readLPWIds,SIZE_OF_ARRAY(databaseHandle->databaseNode->readLPWIds));
  #endif
  if (databaseHandle->readLockCount <= 0) HALT_INTERNAL_ERROR("read lock count");
  databaseHandle->readLockCount--;
  #ifndef NDEBUG
    databaseHandle->debug.locked.threadId = THREAD_ID_NONE;
    databaseHandle->debug.locked.fileName = NULL;
    databaseHandle->debug.locked.lineNb   = 0;
    databaseHandle->debug.locked.text[0]  = '\0';
    databaseHandle->debug.locked.t1       = Misc_getTimestamp();

    debugClearDatabaseThreadInfo(databaseHandle->databaseNode->debug.reads,
                                 SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.reads)
                                );

    debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                      databaseHandle->databaseNode->debug.history,
                                      &databaseHandle->databaseNode->debug.historyIndex,
                                      SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                      DATABASE_HISTORY_TYPE_UNLOCK
                                     );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : readWritesIncrement
* Purpose: increment database read/write
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void readWritesIncrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __readWritesIncrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  assert(databaseHandle->databaseNode->readWriteCount >= databaseHandle->readWriteLockCount);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  databaseHandle->readWriteLockCount++;
  databaseHandle->databaseNode->readWriteCount++;
  #ifdef DATABASE_DEBUG_LOCK
    debugAddThreadLWPId(databaseHandle->databaseNode->readWriteLPWIds,SIZE_OF_ARRAY(databaseHandle->databaseNode->readWriteLPWIds));
  #endif
  #ifndef NDEBUG
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(   Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy)
           || Thread_isNone(databaseHandle->databaseNode->readWriteLockedBy)
          );
#endif

    databaseHandle->debug.locked.threadId = Thread_getCurrentId();
    databaseHandle->debug.locked.fileName = __fileName__;
    databaseHandle->debug.locked.lineNb   = __lineNb__;
    databaseHandle->debug.locked.text[0]  = '\0';
    databaseHandle->debug.locked.t0       = Misc_getTimestamp();

    if (databaseHandle->databaseNode->readWriteCount == 1)
    {
      databaseHandle->databaseNode->debug.readWriteLockedBy = Thread_getCurrentId();
      debugAddDatabaseThreadInfo(__fileName__,__lineNb__,
                                 databaseHandle->databaseNode->debug.readWrites,
                                 SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.readWrites)
                                );
    }

    debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                      databaseHandle->databaseNode->debug.history,
                                      &databaseHandle->databaseNode->debug.historyIndex,
                                      SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                      DATABASE_HISTORY_TYPE_LOCK_READ_WRITE
                                     );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : readWritesDecrement
* Purpose: decrement database read/write
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void readWritesDecrement(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __readWritesDecrement(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));
  DATABASE_DEBUG_LOCK_ASSERT(databaseHandle,isReadWriteLock(databaseHandle));
  assert(databaseHandle->databaseNode->readWriteCount >= databaseHandle->readWriteLockCount);

//TODO: required/useful?
#ifdef DATABASE_USE_ATOMIC_INCREMENT
#else /* not DATABASE_USE_ATOMIC_INCREMENT */
#endif /* DATABASE_USE_ATOMIC_INCREMENT */
  if (databaseHandle->databaseNode->readWriteCount <= 0) HALT_INTERNAL_ERROR("read/write count");
  databaseHandle->databaseNode->readWriteCount--;
  #ifdef DATABASE_DEBUG_LOCK
    debugRemoveThreadLWPId(databaseHandle->databaseNode->readWriteLPWIds,SIZE_OF_ARRAY(databaseHandle->databaseNode->readWriteLPWIds));
  #endif
  if (databaseHandle->readWriteLockCount <= 0) HALT_INTERNAL_ERROR("read/write lock count");
  databaseHandle->readWriteLockCount--;
  #ifndef NDEBUG
    databaseHandle->debug.locked.threadId = THREAD_ID_NONE;
    databaseHandle->debug.locked.fileName = NULL;
    databaseHandle->debug.locked.lineNb   = 0;
    databaseHandle->debug.locked.text[0]  = '\0';
    databaseHandle->debug.locked.t1       = Misc_getTimestamp();

    if (databaseHandle->databaseNode->readWriteCount == 0)
    {
      databaseHandle->databaseNode->debug.readWriteLockedBy = THREAD_ID_NONE;
      debugClearDatabaseThreadInfo(databaseHandle->databaseNode->debug.readWrites,
                                   SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.readWrites)
                                  );
    }

    debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                      databaseHandle->databaseNode->debug.history,
                                      &databaseHandle->databaseNode->debug.historyIndex,
                                      SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                      DATABASE_HISTORY_TYPE_UNLOCK
                                     );
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : waitTriggerRead
* Purpose: wait trigger database read unlock
* Input  : databaseHandle - database handle
*          timeout        - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if triggered, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE bool waitTriggerRead(DatabaseHandle *databaseHandle,
                                  long           timeout
                                 )
#else /* not NDEBUG */
LOCAL_INLINE bool __waitTriggerRead(const char     *__fileName__,
                                    ulong          __lineNb__,
                                    DatabaseHandle *databaseHandle,
                                    long           timeout
                                   )
#endif /* NDEBUG */
{
  struct timespec timespec;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    #ifdef DATABASE_DEBUG_LOCK_PRINT
      fprintf(stderr,"%s, %lu: %s                wait rw #%3u %p\n",__fileName__,__lineNb__,Thread_getCurrentIdString(),databaseHandle->databaseNode->readWriteCount,&databaseHandle->databaseNode->readWriteTrigger);
    #endif /* DATABASE_DEBUG_LOCK_PRINT */
  #endif /* not NDEBUG */

  #ifdef DATABASE_LOCK_PER_INSTANCE
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->readTrigger,databaseHandle->databaseNode->lock,&timespec) == ETIMEDOUT)
      {
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %ums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->readTrigger,databaseNode->lock);
    }
  #else /* not DATABASE_LOCK_PER_INSTANCE */
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->readTrigger,&databaseLock,&timespec) == ETIMEDOUT)
      {
//TODO
if (pthread_cond_timedwait(&databaseHandle->databaseNode->readTrigger,&databaseLock,&timespec) == 0)
{
fprintf(stderr,"%s, %d: UPS!!!!!!!!!!\n",__FILE__,__LINE__);
return TRUE;
}
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %lums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->readTrigger,&databaseLock);
    }
  #endif /* DATABASE_LOCK_PER_INSTANCE */

  #ifndef NDEBUG
    #ifdef DATABASE_DEBUG_LOCK_PRINT
      fprintf(stderr,"%s, %lu: %s                wait rw #%3u %p done\n",__fileName__,__lineNb__,Thread_getCurrentIdString(),databaseHandle->databaseNode->readWriteCount,&databaseHandle->databaseNode->readWriteTrigger);
    #endif /* DATABASE_DEBUG_LOCK_PRINT */
  #endif /* not NDEBUG */

  return TRUE;
}

/***********************************************************************\
* Name   : waitTriggerReadWrite
* Purpose: wait trigger database read/write unlock
* Input  : databaseNode - database node
*          timeout      - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if triggered, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE bool waitTriggerReadWrite(DatabaseHandle *databaseHandle,
                                       long           timeout
                                      )
#else /* not NDEBUG */
LOCAL_INLINE bool __waitTriggerReadWrite(const char     *__fileName__,
                                         ulong          __lineNb__,
                                         DatabaseHandle *databaseHandle,
                                         long           timeout
                                        )
#endif /* NDEBUG */
{
  struct timespec timespec;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifdef DATABASE_LOCK_PER_INSTANCE
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->readWriteTrigger,databaseHandle->databaseNode->lock,&timespec) == ETIMEDOUT)
      {
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %lums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->readWriteTrigger,databaseNode->lock);
    }
  #else /* not DATABASE_LOCK_PER_INSTANCE */
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->readWriteTrigger,&databaseLock,&timespec) == ETIMEDOUT)
      {
//TODO
if (pthread_cond_timedwait(&databaseHandle->databaseNode->readWriteTrigger,&databaseLock,&timespec) == 0)
{
fprintf(stderr,"%s, %d: UPS!!!!!!!!!!\n",__FILE__,__LINE__);
return TRUE;
}
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %lums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->readWriteTrigger,&databaseLock);
    }
  #endif /* DATABASE_LOCK_PER_INSTANCE */

  #ifndef NDEBUG
    #ifdef DATABASE_DEBUG_LOCK_PRINT
    #endif /* DATABASE_DEBUG_LOCK_PRINT */
  #endif /* not NDEBUG */

  return TRUE;
}

/***********************************************************************\
* Name   : waitTriggerTransaction
* Purpose: wait trigger database transaction unlock
* Input  : databaseNode - database node
*          timeout      - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if triggered, FALSE on timeout
* Notes  : -
\***********************************************************************/

#if 0
//TODO: not used - remove?
#ifdef NDEBUG
LOCAL_INLINE bool waitTriggerTransaction(DatabaseHandle *databaseHandle,
                                         long           timeout
                                        )
#else /* not NDEBUG */
LOCAL_INLINE bool __waitTriggerTransaction(const char     *__fileName__,
                                           ulong          __lineNb__,
                                           DatabaseHandle *databaseHandle,
                                           long           timeout
                                          )
#endif /* NDEBUG */
{
  struct timespec timespec;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifdef DATABASE_LOCK_PER_INSTANCE
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->transactionTrigger,databaseHandle->databaseNode->lock,&timespec) == ETIMEDOUT)
      {
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %lums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->transactionTrigger,databaseHandle->databaseNode->lock);
    }
  #else /* not DATABASE_LOCK_PER_INSTANCE */
    if (timeout != WAIT_FOREVER)
    {
      #if   defined(PLATFORM_LINUX)
        clock_gettime(CLOCK_REALTIME,&timespec);
      #elif defined(PLATFORM_WINDOWS)
        getTime(&timespec);
      #endif /* PLATFORM_... */
      timespec.tv_nsec = timespec.tv_nsec+((timeout)%1000L)*1000000L;
      timespec.tv_sec  = timespec.tv_sec+((timespec.tv_nsec/1000000L)+(timeout))/1000L;
      timespec.tv_nsec %= 1000000L;
      if (pthread_cond_timedwait(&databaseHandle->databaseNode->transactionTrigger,&databaseLock,&timespec) == ETIMEDOUT)
      {
        #ifdef DATABASE_DEBUG_TIMEOUT
          HALT_INTERNAL_ERROR("database timeout %lums",timeout);
        #endif
        return FALSE;
      }
    }
    else
    {
      pthread_cond_wait(&databaseHandle->databaseNode->transactionTrigger,&databaseLock);
    }
  #endif /* DATABASE_LOCK_PER_INSTANCE */

  #ifndef NDEBUG
    #ifdef DATABASE_DEBUG_LOCK_PRINT
    #endif /* DATABASE_DEBUG_LOCK_PRINT */
  #endif /* not NDEBUG */

  return TRUE;
}
#endif // 0

/***********************************************************************\
* Name   : triggerUnlockRead
* Purpose: trigger database read/write unlock all
* Input  : databaseNode - database node
*          lockType     - lock type; see DATABASE_LOCK_TYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void triggerUnlockRead(DatabaseHandle *databaseHandle, DatabaseLockTypes lockType)
#else /* not NDEBUG */
LOCAL_INLINE void __triggerUnlockRead(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle, DatabaseLockTypes lockType)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  UNUSED_VARIABLE(lockType);

  #ifndef NDEBUG
//fprintf(stderr,"%s, %d: trigger r %d\n",__FILE__,__LINE__,databaseNode->readTrigger);
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.threadId     = Thread_getCurrentId();
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.fileName     = __fileName__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.lineNb       = __lineNb__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.cycleCounter = getCycleCounter();
    databaseHandle->databaseNode->debug.lastTrigger.lockType                = lockType;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount;
    databaseHandle->databaseNode->debug.lastTrigger.readCount               = databaseHandle->databaseNode->readCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
    databaseHandle->databaseNode->debug.lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount;
    #ifdef HAVE_BACKTRACE
      BACKTRACE(databaseHandle->databaseNode->debug.lastTrigger.stackTrace,databaseHandle->databaseNode->debug.lastTrigger.stackTraceSize);
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */
//TODO: do while?
//  do
  {
//fprintf(stderr,".");
  if (pthread_cond_broadcast(&databaseHandle->databaseNode->readTrigger) != 0)
  {
    HALT_INTERNAL_ERROR("read trigger fail: %s",strerror(errno));
  }
  }
//  while ((databaseHandle->databaseNode->readCount == 0) && (databaseHandle->databaseNode->pendingReadCount > 0));
}

/***********************************************************************\
* Name   : triggerUnlockReadWrite
* Purpose: trigger database read/write unlock
* Input  : databaseNode - database node
*          lockType     - lock type; see DATABASE_LOCK_TYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void triggerUnlockReadWrite(DatabaseHandle *databaseHandle, DatabaseLockTypes lockType)
#else /* not NDEBUG */
LOCAL_INLINE void __triggerUnlockReadWrite(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle, DatabaseLockTypes lockType)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

  UNUSED_VARIABLE(lockType);

  #ifndef NDEBUG
//fprintf(stderr,"%s, %d: trigger rw %d\n",__FILE__,__LINE__,databaseNode->readWriteTrigger);
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.threadId     = Thread_getCurrentId();
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.fileName     = __fileName__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.lineNb       = __lineNb__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.cycleCounter = getCycleCounter();
    databaseHandle->databaseNode->debug.lastTrigger.lockType                = lockType;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount;
    databaseHandle->databaseNode->debug.lastTrigger.readCount               = databaseHandle->databaseNode->readCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
    databaseHandle->databaseNode->debug.lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount;
    #ifdef HAVE_BACKTRACE
      BACKTRACE(databaseHandle->databaseNode->debug.lastTrigger.stackTrace,databaseHandle->databaseNode->debug.lastTrigger.stackTraceSize);
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */
//TODO: do while?
//  do
  {
//fprintf(stderr,".");
  if (pthread_cond_broadcast(&databaseHandle->databaseNode->readWriteTrigger) != 0)
  {
    HALT_INTERNAL_ERROR("read/write trigger fail: %s",strerror(errno));
  }
  }
//  while ((databaseHandle->databaseNode->readWriteCount == 0) && (databaseHandle->databaseNode->pendingReadWriteCount > 0));
}

/***********************************************************************\
* Name   : triggerUnlockTransaction
* Purpose: trigger database read/write unlock
* Input  : databaseNode - database node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void triggerUnlockTransaction(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
LOCAL_INLINE void __triggerUnlockTransaction(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(DATABASE_HANDLE_IS_LOCKED(databaseHandle));

  #ifndef NDEBUG
//fprintf(stderr,"%s, %d: trigger trans %d\n",__FILE__,__LINE__,databaseNode->readWriteTrigger);
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.threadId     = Thread_getCurrentId();
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.fileName     = __fileName__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.lineNb       = __lineNb__;
    databaseHandle->databaseNode->debug.lastTrigger.threadInfo.cycleCounter = getCycleCounter();
    databaseHandle->databaseNode->debug.lastTrigger.lockType                = DATABASE_LOCK_TYPE_READ_WRITE;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount;
    databaseHandle->databaseNode->debug.lastTrigger.readCount               = databaseHandle->databaseNode->readCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount;
    databaseHandle->databaseNode->debug.lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
    databaseHandle->databaseNode->debug.lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount;
    #ifdef HAVE_BACKTRACE
      BACKTRACE(databaseHandle->databaseNode->debug.lastTrigger.stackTrace,databaseHandle->databaseNode->debug.lastTrigger.stackTraceSize);
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */
  if (pthread_cond_broadcast(&databaseHandle->databaseNode->transactionTrigger) != 0)
  {
    HALT_INTERNAL_ERROR("transaction trigger fail: %s",strerror(errno));
  }
}

/***********************************************************************\
* Name   : begin
* Purpose: begin database write operation
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
*          timeout        - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE bool begin(DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType, long timeout)
#else /* not NDEBUG */
LOCAL_INLINE bool __begin(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType, long timeout)
#endif /* NDEBUG */
{
  bool locked;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifndef NDEBUG
    locked = __Database_lock(__fileName__,__lineNb__,databaseHandle,lockType,timeout);
  #else /* NDEBUG */
    locked = Database_lock(databaseHandle,lockType,timeout);
  #endif /* not NDEBUG */
  if (!locked)
  {
    return FALSE;
  }

  #ifndef NDEBUG
    #ifdef HAVE_BACKTRACE
      BACKTRACE(databaseHandle->debug.current.stackTrace,databaseHandle->debug.current.stackTraceSize);
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */

  return TRUE;
}

/***********************************************************************\
* Name   : end
* Purpose: end database write operation
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void end(DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType)
#else /* not NDEBUG */
LOCAL_INLINE void __end(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifndef NDEBUG
    #ifdef HAVE_BACKTRACE
      databaseHandle->debug.current.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    __Database_unlock(__fileName__,__lineNb__,databaseHandle,lockType);
  #else /* NDEBUG */
    Database_unlock(databaseHandle,lockType);
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : vformatSQLString
* Purpose: format SQL string from command and append
* Input  : sqlString   - SQL string variable
*          command     - command string with %[l][du], %S, %s
*          arguments   - optional argument list
* Output : -
* Return : SQL string
* Notes  : -
\***********************************************************************/

LOCAL String vformatSQLString(String     sqlString,
                              const char *command,
                              va_list    arguments
                             )
{
  const char                *s;
  bool                      longFlag,longLongFlag;
  char                      quoteFlag;
  Value                     value;
  const char                *t;
  ulong                     i;
  char                      ch;
  DatabaseTemporaryTableIds id;

  assert(sqlString != NULL);
  assert(command != NULL);

  s  = command;
  while (!stringIsEmpty(s))
  {
    switch (stringAt(s,0))
    {
      case '\\':
        // escaped character
        String_appendChar(sqlString,'\\');
        s++;
        if (!stringIsEmpty(s))
        {
          String_appendChar(sqlString,stringAt(s,0));
          s++;
        }
        break;
      case '%':
        // format character
        s++;

        // check for longlong/long flag
        longLongFlag = FALSE;
        longFlag     = FALSE;
        if (stringAt(s,0) == 'l')
        {
          s++;
          if (stringAt(s,0) == 'l')
          {
            s++;
            longLongFlag = TRUE;
          }
          else
          {
            longFlag = TRUE;
          }
        }

        // quoting flag (ignore quote char)
        if (   !stringIsEmpty(s)
            && !isalpha(stringAt(s,0))
            && (stringAt(s,0) != '%')
            && (   (stringAt(s,1) == 's')
                || (stringAt(s,1) == 'S')
               )
           )
        {
          quoteFlag = TRUE;
          s++;
        }
        else
        {
          quoteFlag = FALSE;
        }

        if (!stringIsEmpty(s))
        {
          // handle format type
          switch (stringAt(s,0))
          {
            case 'd':
              // integer
              s++;

              if      (longLongFlag)
              {
                value.ll = va_arg(arguments,long long int);
                String_appendFormat(sqlString,"%lld",value.ll);
              }
              else if (longFlag)
              {
                value.l = va_arg(arguments,long int);
                String_appendFormat(sqlString,"%ld",value.l);
              }
              else
              {
                value.i = va_arg(arguments,int);
                String_appendFormat(sqlString,"%d",value.i);
              }
              break;
            case 'u':
              // unsigned integer
              s++;

              if      (longLongFlag)
              {
                value.ull = va_arg(arguments,unsigned long long int);
                String_appendFormat(sqlString,"%llu",value.ull);
              }
              else if (longFlag)
              {
                value.ul = va_arg(arguments,unsigned long);
                String_appendFormat(sqlString,"%lu",value.ul);
              }
              else
              {
                value.ui = va_arg(arguments,uint);
                String_appendFormat(sqlString,"%u",value.ui);
              }
              break;
            case 's':
              // C string
              s++;

              value.s = va_arg(arguments,const char*);

              if (quoteFlag) String_appendChar(sqlString,'\'');
              if (value.s != NULL)
              {
                t = value.s;
                while (!stringIsEmpty(t))
                {
                  switch (stringAt(t,0))
                  {
                    case '\'':
                      if (quoteFlag)
                      {
                        String_appendCString(sqlString,"''");
                      }
                      else
                      {
                        String_appendChar(sqlString,'\'');
                      }
                      break;
                    default:
                      String_appendChar(sqlString,stringAt(t,0));
                      break;
                  }
                  t++;
                }
              }
              if (quoteFlag) String_appendChar(sqlString,'\'');
              break;
            case 'S':
              // string
              s++;

              value.string = va_arg(arguments,String);

              if (quoteFlag) String_appendChar(sqlString,'\'');
              if (value.string != NULL)
              {
                i = 0L;
                while (i < String_length(value.string))
                {
                  ch = String_index(value.string,i);
                  switch (ch)
                  {
                    case '\'':
                      if (quoteFlag)
                      {
                        String_appendCString(sqlString,"''");
                      }
                      else
                      {
                        String_appendChar(sqlString,'\'');
                      }
                      break;
                    default:
                      String_appendChar(sqlString,ch);
                      break;
                  }
                  i++;
                }
              }
              if (quoteFlag) String_appendChar(sqlString,'\'');
              break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              // temporary table name
              id = (DatabaseTemporaryTableIds)(stringAt(s,0)-'1');
              s++;

              String_appendCString(sqlString,DATABASE_AUX ".");
              String_appendCString(sqlString,TEMPORARY_TABLE_NAMES[id]);
              break;
            case '%':
              // %%
              s++;

              String_appendChar(sqlString,'%');
              break;
            default:
              String_appendChar(sqlString,'%');
              String_appendChar(sqlString,stringAt(s,0));
              break;
          }
        }
        break;
      default:
        String_appendChar(sqlString,stringAt(s,0));
        s++;
        break;
    }
  }

  return sqlString;
}

/***********************************************************************\
* Name   : Database_formatSQLString
* Purpose: format SQL string from command
* Input  : sqlString - SQL string variable
*          command   - command string with %[l]d, %S, %s
*          ...       - optional arguments
* Output : -
* Return : SQL string
* Notes  : -
\***********************************************************************/

LOCAL String formatSQLString(String     sqlString,
                             const char *command,
                             ...
                            )
{
  va_list arguments;

  va_start(arguments,command);
  {
    vformatSQLString(sqlString,command,arguments);
  }
  va_end(arguments);

  return sqlString;
}

/***********************************************************************\
* Name   : executeCheckpoint
* Purpose: force execute a checkpoint if timeout expired
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeCheckpoint(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  if (Misc_getTimestamp() > (databaseHandle->lastCheckpointTimestamp+MAX_FORCE_CHECKPOINT_TIME*US_PER_MS))
  {
    DATABASE_DO(databaseHandle,
                DATABASE_LOCK_TYPE_READ_WRITE,
                databaseHandle->timeout,
    {
      (void)sqlite3_wal_checkpoint_v2(databaseHandle->sqlite.handle,NULL,CHECKPOINT_MODE,NULL,NULL);
    });
    databaseHandle->lastCheckpointTimestamp = Misc_getTimestamp();
  }
}

#if 0
/***********************************************************************\
* Name   : busyHandler
* Purpose: SQLite3 busy handler callback
* Input  : userData - user data
*          n        - number of calls
* Output : -
* Return : 0 for abort
* Notes  : -
\***********************************************************************/

LOCAL int busyHandler(void *userData, int n)
{
  #define SLEEP_TIME 500L

  DatabaseHandle *databaseHandle = (DatabaseHandle*)userData;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifndef NDEBUG
    if ((n > 60) && ((n % 60) == 0))
    {
      fprintf(stderr,"Warning: database busy handler called '%s' (%s): %d\n",Thread_getCurrentName(),Thread_getCurrentIdString(),n);
    }
  #endif /* not NDEBUG */

  // execute registered busy handlers
  if (databaseHandle->busyHandlerFunction != NULL)
  {
    databaseHandle->busyHandlerFunction(databaseHandle->busyHandlerUserData);
  }

  // always stop
  return 0;

  #undef SLEEP_TIME
}
#endif /* 0 */

/***********************************************************************\
* Name   : prepareStatement
* Purpose: prepare SQL statement
* Input  : databaseStatementHandle - database query handle variable
*          databaseHandle          - database handle
*          sqlCommand              - SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors prepareStatement(DatabaseStatementHandle *databaseStatementHandle,
                                DatabaseHandle          *databaseHandle,
                                const char              *sqlCommand
                               )
#else /* not NDEBUG */
  LOCAL Errors __prepareStatement(const char              *__fileName__,
                                  ulong                   __lineNb__,
                                  DatabaseStatementHandle *databaseStatementHandle,
                                  DatabaseHandle          *databaseHandle,
                                  const char              *sqlCommand
                                 )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseStatementHandle != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(sqlCommand != NULL);

  // initialize variables
  databaseStatementHandle->databaseHandle = databaseHandle;
  #ifndef NDEBUG
    databaseStatementHandle->sqlString = String_newCString(sqlCommand);
    databaseStatementHandle->dt        = 0LL;
  #endif /* not NDEBUG */

  // lock
  #ifndef NDEBUG
    if (!__Database_lock(__fileName__,__lineNb__,databaseHandle,DATABASE_LOCK_TYPE_READ,databaseHandle->timeout))
  #else /* NDEBUG */
    if (!Database_lock(databaseHandle,DATABASE_LOCK_TYPE_READ,databaseHandle->timeout))
  #endif /* not NDEBUG */
  {
    #ifndef NDEBUG
      String_delete(databaseStatementHandle->sqlString);
    #endif /* not NDEBUG */
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }

  // prepare SQL command execution
// TODO: use C string argument
//  DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString);

  #ifndef NDEBUG
    String_setCString(databaseHandle->debug.current.sqlString,sqlCommand);
  #endif /* not NDEBUG */

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        DATABASE_DEBUG_TIME_START(databaseStatementHandle);
        {
          error = sqlite3StatementPrepare(&databaseStatementHandle->sqlite.statementHandle,
                                          databaseHandle->sqlite.handle,
                                          sqlCommand
                                         );
        }
        DATABASE_DEBUG_TIME_END(databaseStatementHandle);
        if (error != ERROR_NONE)
        {
          Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ);
          #ifndef NDEBUG
            String_delete(databaseStatementHandle->sqlString);
          #endif /* not NDEBUG */
          return error;
        }

        // get value/result count
        databaseStatementHandle->valueCount  = sqlite3_bind_parameter_count(databaseStatementHandle->sqlite.statementHandle);
        databaseStatementHandle->resultCount = sqlite3_column_count(databaseStatementHandle->sqlite.statementHandle);

        // allocate bind data
        databaseStatementHandle->sqlite.bind = (DatabaseValue**)calloc(databaseStatementHandle->resultCount+databaseStatementHandle->valueCount,
                                                                       sizeof(DatabaseValue*)
                                                                      );
        if (databaseStatementHandle->sqlite.bind == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          // prepare SQL statement
          DATABASE_DEBUG_TIME_START(databaseStatementHandle);
          {
            databaseStatementHandle->mysql.statementHandle = mysql_stmt_init(databaseHandle->mysql.handle);
            #ifndef NDEBUG
              if (databaseStatementHandle->mysql.statementHandle == NULL)
              {
                HALT_INTERNAL_ERROR("MariaDB library reported misuse %d %s: %s",
                                    mysql_errno(databaseHandle->mysql.handle),
                                    mysql_stmt_error(databaseStatementHandle->mysql.statementHandle),
                                    sqlCommand
                                   );
              }
            #endif /* not NDEBUG */

    //fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,sqlCommand);
            error = mysqlStatementPrepare(databaseStatementHandle->mysql.statementHandle,
                                          sqlCommand
                                         );
          }
          DATABASE_DEBUG_TIME_END(databaseStatementHandle);
          if (error != ERROR_NONE)
          {
            mysql_stmt_close(databaseStatementHandle->mysql.statementHandle);
            Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ);
            #ifndef NDEBUG
              String_delete(databaseStatementHandle->sqlString);
            #endif /* not NDEBUG */
            return error;
          }

          // get value/result count
          databaseStatementHandle->valueCount  = mysql_stmt_param_count(databaseStatementHandle->mysql.statementHandle);
          databaseStatementHandle->resultCount = mysql_stmt_field_count(databaseStatementHandle->mysql.statementHandle);

          // allocate bind data
          databaseStatementHandle->mysql.values.bind = (MYSQL_BIND*)calloc(databaseStatementHandle->valueCount,
                                                                           sizeof(MYSQL_BIND)
                                                                          );
          if (databaseStatementHandle->mysql.values.bind == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          databaseStatementHandle->mysql.values.time = (MYSQL_TIME*)calloc(databaseStatementHandle->valueCount,
                                                                           sizeof(MYSQL_TIME)
                                                                          );
          if (databaseStatementHandle->mysql.values.time == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          databaseStatementHandle->mysql.results.bind = (MYSQL_BIND*)calloc(databaseStatementHandle->resultCount,
                                                                            sizeof(MYSQL_BIND)
                                                                           );
          if (databaseStatementHandle->mysql.results.bind == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          databaseStatementHandle->mysql.results.time = (MYSQL_TIME*)calloc(databaseStatementHandle->resultCount,
                                                                            sizeof(MYSQL_TIME)
                                                                           );
          if (databaseStatementHandle->mysql.results.time == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

// TODO: remove
#if 0
  databaseStatementHandle->values = (const DatabaseValue**)calloc(databaseStatementHandle->valueCount,
                                                                  sizeof(DatabaseValue*)
                                                                 );
  if (databaseStatementHandle->values == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
#endif
  databaseStatementHandle->valueIndex  = 0;

  databaseStatementHandle->results = (DatabaseValue*)calloc(databaseStatementHandle->resultCount,
                                                            sizeof(DatabaseValue)
                                                           );
  if (databaseStatementHandle->results == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  databaseStatementHandle->resultIndex = 0;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseStatementHandle,DatabaseStatementHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseStatementHandle,DatabaseStatementHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

// TODO: remove
LOCAL void dumpStatementHandle(DatabaseStatementHandle *databaseStatementHandle)
{
#ifndef NDEBUG
  uint i;
//  char buffer[1024];

fprintf(stderr,"%s:%d: sqlString=%s\n",__FILE__,__LINE__,String_cString(databaseStatementHandle->sqlString));
fprintf(stderr,"%s:%d: value Count=%d\n",__FILE__,__LINE__,databaseStatementHandle->valueCount);

        for (i = 0; i < databaseStatementHandle->valueCount; i++)
        {
//          Database_valueToCString(buffer,sizeof(buffer),&databaseStatementHandle->mysql.values[i]);
//fprintf(stderr,"%s:%d: %d: %s=%s\n",__FILE__,__LINE__,i,DATABASE_DATATYPE_NAMES[databaseStatementHandle->values[i].type],buffer);

        }
#else
UNUSED_VARIABLE(databaseStatementHandle);
#endif
}

/***********************************************************************\
* Name   : bindResults
* Purpose: bind SQL result values
* Input  : databaseStatementHandle - database statement query handle variable
*          columns                 - result columns
*          columnsCount            - result columns count
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors bindResults(DatabaseStatementHandle *databaseStatementHandle,
                         const DatabaseColumn    columns[],
                         uint                    columnsCount
                        )
{
  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(checkDatabaseInitialized(databaseStatementHandle->databaseHandle));
  assert((columnsCount == 0) || (columns != NULL));
  assertx((databaseStatementHandle->resultIndex+columnsCount) <= databaseStatementHandle->resultCount,"invalid result count: given %u, expected %u",databaseStatementHandle->resultIndex+columnsCount,databaseStatementHandle->resultCount);

  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        uint i;

        // bind results
        for (i = 0; i < columnsCount; i++)
        {
          databaseStatementHandle->results[databaseStatementHandle->resultIndex].type = columns[i].type;
          switch (columns[i].type)
          {
            case DATABASE_DATATYPE:
              break;
            case DATABASE_DATATYPE_PRIMARY_KEY:
            case DATABASE_DATATYPE_KEY:
              break;
            case DATABASE_DATATYPE_BOOL:
              break;
            case DATABASE_DATATYPE_INT:
              break;
            case DATABASE_DATATYPE_INT64:
              break;
            case DATABASE_DATATYPE_UINT:
              break;
            case DATABASE_DATATYPE_UINT64:
              break;
            case DATABASE_DATATYPE_DOUBLE:
              break;
            case DATABASE_DATATYPE_DATETIME:
              break;
            case DATABASE_DATATYPE_STRING:
              break;
            case DATABASE_DATATYPE_CSTRING:
              break;
            case DATABASE_DATATYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          databaseStatementHandle->resultIndex++;
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          const uint MAX_TEXT_LENGTH = 4096;

          uint i;

          // bind results
          for (i = 0; i < columnsCount; i++)
          {
            databaseStatementHandle->results[databaseStatementHandle->resultIndex].type = columns[i].type;
            switch (columns[i].type)
            {
              case DATABASE_DATATYPE_NONE:
                break;
              case DATABASE_DATATYPE:
                break;
              case DATABASE_DATATYPE_PRIMARY_KEY:
              case DATABASE_DATATYPE_KEY:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].id;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_BOOL:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_TINY;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].b;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_INT:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].i;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_INT64:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].i64;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_UINT:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].u;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_UINT64:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].u64;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_DOUBLE:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_DOUBLE;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].d;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_DATETIME:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)&databaseStatementHandle->results[databaseStatementHandle->resultIndex].dateTime;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_STRING:
              case DATABASE_DATATYPE_CSTRING:
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_type   = MYSQL_TYPE_STRING;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer        = (char*)malloc(MAX_TEXT_LENGTH);
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer_length = MAX_TEXT_LENGTH;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].is_null       = NULL;
                databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].length        = &databaseStatementHandle->results[databaseStatementHandle->resultIndex].text.length;
                if (databaseStatementHandle->mysql.results.bind[databaseStatementHandle->resultIndex].buffer == NULL)
                {
                  HALT_INSUFFICIENT_MEMORY();
                }
                break;
              case DATABASE_DATATYPE_BLOB:
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }

            databaseStatementHandle->resultIndex++;
          }
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : finalizeStatement
* Purpose: finalize SQL statement
* Input  : databaseStatementHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL void finalizeStatement(DatabaseStatementHandle *databaseStatementHandle)
#else /* not NDEBUG */
  LOCAL void __finalizeStatement(const char              *__fileName__,
                                 ulong                   __lineNb__,
                                 DatabaseStatementHandle *databaseStatementHandle
                                )
#endif /* NDEBUG */
{
  uint i;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(checkDatabaseInitialized(databaseStatementHandle->databaseHandle));

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseStatementHandle,DatabaseStatementHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseStatementHandle,DatabaseStatementHandle);
  #endif /* NDEBUG */

  // free bind data
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        // finalize statement
        sqlite3_finalize(databaseStatementHandle->sqlite.statementHandle);

        // free bind data
        for (i = 0; i < (databaseStatementHandle->resultCount); i++)
        {
        }
        if (databaseStatementHandle->sqlite.bind != NULL) free(databaseStatementHandle->sqlite.bind);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          // finalize statement
          mysql_stmt_close(databaseStatementHandle->mysql.statementHandle);

          // free bind data
          for (i = 0; i < (databaseStatementHandle->resultCount); i++)
          {
            switch (databaseStatementHandle->mysql.results.bind[i].buffer_type)
            {
              case MYSQL_TYPE_TINY:
              case MYSQL_TYPE_SHORT:
              case MYSQL_TYPE_LONG:
              case MYSQL_TYPE_INT24:
              case MYSQL_TYPE_LONGLONG:
              case MYSQL_TYPE_DECIMAL:
              case MYSQL_TYPE_NEWDECIMAL:
                break;
              case MYSQL_TYPE_FLOAT:
              case MYSQL_TYPE_DOUBLE:
                break;
              case MYSQL_TYPE_BIT:
                break;
              case MYSQL_TYPE_TIMESTAMP:
              case MYSQL_TYPE_DATE:
              case MYSQL_TYPE_TIME:
              case MYSQL_TYPE_YEAR:
              case MYSQL_TYPE_DATETIME:
                break;
              case MYSQL_TYPE_STRING:
                free(databaseStatementHandle->mysql.results.bind[i].buffer);
                break;
              case MYSQL_TYPE_VAR_STRING:
                break;
              case MYSQL_TYPE_BLOB:
              case MYSQL_TYPE_SET:
              case MYSQL_TYPE_GEOMETRY:
              case MYSQL_TYPE_NULL:
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }
          }
          if (databaseStatementHandle->mysql.results.time != NULL) free(databaseStatementHandle->mysql.results.time);
          if (databaseStatementHandle->mysql.results.bind != NULL) free(databaseStatementHandle->mysql.results.bind);
          if (databaseStatementHandle->mysql.values.time != NULL) free(databaseStatementHandle->mysql.values.time);
          if (databaseStatementHandle->mysql.values.bind != NULL) free(databaseStatementHandle->mysql.values.bind);
        }
      #else /* HAVE_MARIADB */
        return;
      #endif /* HAVE_MARIADB */
      break;
  }
  if (databaseStatementHandle->results != NULL) free(databaseStatementHandle->results);
// TODO:remove  if (databaseStatementHandle->values != NULL) free(databaseStatementHandle->values);

  // unlock
  Database_unlock(databaseStatementHandle->databaseHandle,DATABASE_LOCK_TYPE_READ);

  // free resources
  #ifndef NDEBUG
    String_delete(databaseStatementHandle->sqlString);
  #endif /* not NDEBUG */
}

/***********************************************************************\
* Name   : getNextRow
* Purpose: get next row
* Input  : databaseStatementHandle - statement handle
*          timeout                 - timeout [ms]
* Output : -
* Return : TRUE if next row, FALSE end of data or error
* Notes  : -
\***********************************************************************/

LOCAL bool getNextRow(DatabaseStatementHandle *databaseStatementHandle, long timeout)
{
  #define SLEEP_TIME 500L

  uint n;
  bool result;

  assert(databaseStatementHandle != NULL);
  assert(databaseStatementHandle->databaseHandle != NULL);

  n      = 0;
  result = FALSE;
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int  sqliteResult;
        uint i;

        do
        {
          sqliteResult = sqlite3_step(databaseStatementHandle->sqlite.statementHandle);
          if (sqliteResult == SQLITE_LOCKED)
          {
            sqlite3WaitUnlockNotify(databaseStatementHandle->databaseHandle->sqlite.handle);
            sqlite3_reset(databaseStatementHandle->sqlite.statementHandle);
          }
  //TODO: correct? abort here?
          else if (sqliteResult == SQLITE_BUSY)
          {
            Misc_udelay(SLEEP_TIME*US_PER_MS);
            sqlite3_reset(databaseStatementHandle->sqlite.statementHandle);
            n++;
          }
        }
        while (   ((sqliteResult == SQLITE_LOCKED) || (sqliteResult == SQLITE_BUSY))
               && ((timeout == WAIT_FOREVER) || (n < (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME)))
              );

        if (sqliteResult == SQLITE_ROW)
        {
          for (i = 0; i < databaseStatementHandle->resultCount; i++)
          {
            switch (databaseStatementHandle->results[i].type)
            {
              case DATABASE_DATATYPE_NONE:
                break;
              case DATABASE_DATATYPE:
                break;
              case DATABASE_DATATYPE_PRIMARY_KEY:
              case DATABASE_DATATYPE_KEY:
                databaseStatementHandle->results[i].id = sqlite3_column_int64(databaseStatementHandle->sqlite.statementHandle,
                                                                              i
                                                                             );
                break;
              case DATABASE_DATATYPE_BOOL:
                databaseStatementHandle->results[i].b = (sqlite3_column_int(databaseStatementHandle->sqlite.statementHandle,
                                                                            i
                                                                           ) == 1);
                break;
              case DATABASE_DATATYPE_INT:
                databaseStatementHandle->results[i].i = sqlite3_column_int(databaseStatementHandle->sqlite.statementHandle,
                                                                           i
                                                                          );
                break;
              case DATABASE_DATATYPE_INT64:
                databaseStatementHandle->results[i].i64 = sqlite3_column_int64(databaseStatementHandle->sqlite.statementHandle,
                                                                               i
                                                                              );
                break;
              case DATABASE_DATATYPE_UINT:
                databaseStatementHandle->results[i].u = (uint)sqlite3_column_int(databaseStatementHandle->sqlite.statementHandle,
                                                                           i
                                                                          );
                break;
              case DATABASE_DATATYPE_UINT64:
                databaseStatementHandle->results[i].u64 = (uint64)sqlite3_column_int64(databaseStatementHandle->sqlite.statementHandle,
                                                                                       i
                                                                                      );
                break;
              case DATABASE_DATATYPE_DOUBLE:
                databaseStatementHandle->results[i].d = sqlite3_column_double(databaseStatementHandle->sqlite.statementHandle,
                                                                              i
                                                                             );
                break;
              case DATABASE_DATATYPE_DATETIME:
// TODO: remove
#if 0
                // data/time is stored as an 'integer', but repesented as date/time string
                s = sqlite3_column_text(databaseStatementHandle->sqlite.statementHandle,
                                        i
                                       );
                if (s != NULL)
                {
                  databaseStatementHandle->results[i].dateTime = Misc_parseDateTime((const char *)s);
                }
                else
                {
                  databaseStatementHandle->results[i].dateTime = 0LL;
                }
#else
                databaseStatementHandle->results[i].dateTime = (uint64)sqlite3_column_int64(databaseStatementHandle->sqlite.statementHandle,
                                                                                            i
                                                                                           );
#endif
                break;
              case DATABASE_DATATYPE_STRING:
              case DATABASE_DATATYPE_CSTRING:
                databaseStatementHandle->results[i].text.data   = (char*)sqlite3_column_text(databaseStatementHandle->sqlite.statementHandle,
                                                                                             i
                                                                                            );
                databaseStatementHandle->results[i].text.length = stringLength(databaseStatementHandle->results[i].text.data);
                break;
              case DATABASE_DATATYPE_BLOB:
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }
          }

          result = TRUE;
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          int  mariadbResult;
          uint i;

          mariadbResult = mysql_stmt_fetch(databaseStatementHandle->mysql.statementHandle);
          switch (mariadbResult)
          {
            case 0:
              for (i = 0; i < databaseStatementHandle->resultCount; i++)
              {
                switch (databaseStatementHandle->results[i].type)
                {
                  case DATABASE_DATATYPE_NONE:
                    break;
                  case DATABASE_DATATYPE:
                    break;
                  case DATABASE_DATATYPE_PRIMARY_KEY:
                  case DATABASE_DATATYPE_KEY:
                    break;
                  case DATABASE_DATATYPE_BOOL:
                    break;
                  case DATABASE_DATATYPE_INT:
                    break;
                  case DATABASE_DATATYPE_INT64:
                    break;
                  case DATABASE_DATATYPE_UINT:
                    break;
                  case DATABASE_DATATYPE_UINT64:
                    break;
                  case DATABASE_DATATYPE_DOUBLE:
                    break;
                  case DATABASE_DATATYPE_DATETIME:
  // TODO: remove
  #if 0
                    {
                      uint year,month,day;
                      uint hour,minute,second;
  // TODO: remove
  #if 0
  fprintf(stderr,"%s:%d: %d %d %d %d %d %d\n",__FILE__,__LINE__,
  databaseStatementHandle->mysql.dateTime[i].year,
  databaseStatementHandle->mysql.dateTime[i].month,
  databaseStatementHandle->mysql.dateTime[i].day,
  databaseStatementHandle->mysql.dateTime[i].hour,
  databaseStatementHandle->mysql.dateTime[i].minute,
  databaseStatementHandle->mysql.dateTime[i].second
  );
  #endif
                      year   = (databaseStatementHandle->mysql.results.time[i].year >= 1970)
                                 ? databaseStatementHandle->mysql.results.time[i].year
                                 : 1970;
                      month  = (databaseStatementHandle->mysql.results.time[i].month >= 1)
                                 ? databaseStatementHandle->mysql.results.time[i].month
                                 : 1;
                      day    = (databaseStatementHandle->mysql.results.time[i].day >= 1)
                                 ? databaseStatementHandle->mysql.results.time[i].day
                                 : 1;
                      hour   = databaseStatementHandle->mysql.results.time[i].hour;
                      minute = databaseStatementHandle->mysql.results.time[i].minute;
                      second = databaseStatementHandle->mysql.results.time[i].second;
      //fprintf(stderr,"%s:%d: %d %d %d %d %d %d\n",__FILE__,__LINE__,year,month,day,hour,minute,second);

                      // TODO: day-light-saving?
                      databaseStatementHandle->results[i].dateTime = Misc_makeDateTime(year,month,day,
                                                                                       hour,minute,second,
                                                                                       FALSE
                                                                                      );
                    }
  #endif
                    break;
                  case DATABASE_DATATYPE_STRING:
                  case DATABASE_DATATYPE_CSTRING:
                    databaseStatementHandle->results[i].text.data = databaseStatementHandle->mysql.results.bind[i].buffer;
                    break;
                  case DATABASE_DATATYPE_BLOB:
                    HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                    break;
                  default:
                    #ifndef NDEBUG
                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    #endif /* NDEBUG */
                    break;
                }
              }

              result = TRUE;
              break;
            case 1:
  fprintf(stderr,"%s:%d: error\n",__FILE__,__LINE__);
  abort();
              break;
            case MYSQL_NO_DATA:
              break;
            case MYSQL_DATA_TRUNCATED:
              break;
          }
        }
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  return result;

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : executeRowStatement
* Purpose: insert/update/delete a row
* Input  : handle          - database handle
*          statementHandle - statement handle
*          timeout         - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#if 0
LOCAL Errors executeRowStatement(DatabaseStatementHandle *databaseStatementHandle)
{
  #define SLEEP_TIME 500L

  Errors              error;
  uint                i;
  const DatabaseValue *databaseValue;

  assert(databaseStatementHandle != NULL);
  assert(databaseStatementHandle->databaseHandle != NULL);

  error = ERROR_UNKNOWN;
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int  sqliteResult;

        for (i = 0; i < databaseStatementHandle->valueCount; i++)
        {
          databaseValue = databaseStatementHandle->values[databaseStatementHandle->valueMap[i]];

          switch (databaseValue->type)
          {
            case DATABASE_DATATYPE_NONE:
              break;
            case DATABASE_DATATYPE:
              break;
            case DATABASE_DATATYPE_PRIMARY_KEY:
            case DATABASE_DATATYPE_KEY:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->id);
              break;
            case DATABASE_DATATYPE_BOOL:
              sqlite3_bind_int(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->b ? 1 : 0);
              break;
            case DATABASE_DATATYPE_INT:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->i);
              break;
            case DATABASE_DATATYPE_INT64:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->i64);
              break;
            case DATABASE_DATATYPE_UINT:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,(int)databaseValue->u);
              break;
            case DATABASE_DATATYPE_UINT64:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,(int64)databaseValue->u64);
              break;
            case DATABASE_DATATYPE_DOUBLE:
              sqlite3_bind_double(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->d);
              break;
            case DATABASE_DATATYPE_DATETIME:
              sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->dateTime);
              break;
            case DATABASE_DATATYPE_STRING:
              sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,i,String_cString(databaseValue->string),String_length(databaseValue->string),NULL);
              break;
            case DATABASE_DATATYPE_CSTRING:
              sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,i,databaseValue->s,stringLength(databaseValue->s),NULL);
              break;
            case DATABASE_DATATYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
        }

        sqliteResult = sqlite3_step(databaseStatementHandle->sqlite.statementHandle);
        if      (sqliteResult == SQLITE_OK)
        {
          error = ERROR_NONE;
        }
        else if (sqliteResult == SQLITE_MISUSE)
        {
          HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                              sqliteResult,sqlite3_extended_errcode(databaseStatementHandle->databaseHandle->sqlite.handle)
                             );
        }
        else if (sqliteResult == SQLITE_INTERRUPT)
        {
          error = ERRORX_(INTERRUPTED,sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                          "%s",
                          sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                         );
        }
        else
        {
          error = ERRORX_(DATABASE,sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                          "%s",
                          sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                         );
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          int mariadbResult;

          for (i = 0; i < databaseStatementHandle->valueMapCount; i++)
          {
            databaseValue = databaseStatementHandle->values[databaseStatementHandle->valueMap[i]];

            switch (databaseValue->type)
            {
              case DATABASE_DATATYPE_NONE:
                break;
              case DATABASE_DATATYPE:
                break;
              case DATABASE_DATATYPE_PRIMARY_KEY:
              case DATABASE_DATATYPE_KEY:
                break;
              case DATABASE_DATATYPE_BOOL:
                break;
              case DATABASE_DATATYPE_INT:
                break;
              case DATABASE_DATATYPE_INT64:
                break;
              case DATABASE_DATATYPE_UINT:
                break;
              case DATABASE_DATATYPE_UINT64:
                break;
              case DATABASE_DATATYPE_DOUBLE:
                break;
              case DATABASE_DATATYPE_DATETIME:
                {
                  uint year,month,day;
                  uint hour,minute,second;

                  Misc_splitDateTime(databaseValue->dateTime,
                                     &year,
                                     &month,
                                     &day,
                                     &hour,
                                     &minute,
                                     &second,
                                     NULL,  // weekDay,
                                     NULL  // isDayLightSaving
                                    );
                  databaseStatementHandle->mysql.values.time[i].year   = year;
                  databaseStatementHandle->mysql.values.time[i].month  = month;
                  databaseStatementHandle->mysql.values.time[i].day    = day;
                  databaseStatementHandle->mysql.values.time[i].hour   = hour;
                  databaseStatementHandle->mysql.values.time[i].minute = minute;
                  databaseStatementHandle->mysql.values.time[i].second = second;
                }
                break;
              case DATABASE_DATATYPE_STRING:
  // TODO: handle no text?
                if (databaseValue->string != NULL)
                {
                  memCopyFast(databaseStatementHandle->mysql.values.bind[i].buffer,
                              databaseStatementHandle->mysql.values.bind[i].buffer_length,
                              String_cString(databaseValue->string),
                              String_length(databaseValue->string)
                             );
                }
                else
                {
                  databaseStatementHandle->mysql.values.bind[i].buffer_length = 0;
                }
                break;
              case DATABASE_DATATYPE_CSTRING:
  // TODO: handle no text?
                if (databaseValue->text.data != NULL)
                {
                  memCopyFast(databaseStatementHandle->mysql.values.bind[i].buffer,
                              databaseStatementHandle->mysql.values.bind[i].buffer_length,
                              databaseValue->s,
                              stringLength(databaseValue->s)
                             );
                }
                else
                {
                  databaseStatementHandle->mysql.values.bind[i].buffer_length = 0;
                }
                break;
              case DATABASE_DATATYPE_BLOB:
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }
          }

          error = mariadbStatementExecute(databaseStatementHandle->mysql.statementHandle);
// TODO: remove
if (error != ERROR_NONE)
{
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));
dumpStatementHandle(databaseStatementHandle);
}
        }
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return error;

  #undef SLEEP_TIME
}
#endif

/***********************************************************************\
* Name   : getLastInsertRowId
* Purpose: get id of last insert operation
* Input  : -
* Output : -
* Return : id
* Notes  : -
\***********************************************************************/

LOCAL DatabaseId getLastInsertRowId(DatabaseStatementHandle *databaseStatementHandle)
{
  DatabaseId id;

  assert(databaseStatementHandle != NULL);
  assert(databaseStatementHandle->databaseHandle != NULL);

  id = DATABASE_ID_NONE;
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      id = (DatabaseId)sqlite3_last_insert_rowid(databaseStatementHandle->databaseHandle->sqlite.handle);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        id = (DatabaseId)mysql_stmt_insert_id(databaseStatementHandle->mysql.statementHandle);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }

  return id;
}

/***********************************************************************\
* Name   : vexecuteStatement
* Purpose: execute single database statement with prepared statement or
*          query
* Input  : databaseHandle      - database handle
*          databaseRowFunction - row call-back function (can be NULL)
*          databaseRowUserData - user data for row call-back
*          changedRowCount     - number of changed rows (can be NULL)
*          timeout             - timeout [ms]
*          columnTypes         - result column types; use macro
*                                DATABASE_COLUMN_TYPES()
*          columnTypeCount     - number of result columns
*          sqlCommand          - SQL command string with %[l]d, %[']S,
*                                %[']s
*          arguments           - arguments for SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors vexecuteStatement(DatabaseHandle         *databaseHandle,
                               DatabaseRowFunction     databaseRowFunction,
                               void                    *databaseRowUserData,
                               ulong                   *changedRowCount,
                               long                    timeout,
                               uint                    flags,
                               const DatabaseDataTypes *columnTypes,
                               uint                    columnTypeCount,
                               const char              *sqlCommand,
                               va_list                 arguments
                              )
{
  /* data flow:

     application    ->    values -> database internal     -> values   ->    application

                 insert             sqlite:                          select
                 update             MySQL: bind, dateTime
                 delete             Postgres: ?

   */
  #define SLEEP_TIME 500L  // [ms]

  String                        sqlString;
  bool                          done;
  Errors                        error;
  uint                          maxRetryCount;
  uint                          retryCount;
  DatabaseValue                 *values;
  uint                          valueCount;
  uint                          i;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert ((databaseHandle->databaseNode->readCount > 0) || (databaseHandle->databaseNode->readWriteCount > 0));
  assert(databaseHandle->sqlite.handle != NULL);
  assert(sqlCommand != NULL);

  // format SQL command string
  sqlString = vformatSQLString(String_new(),
                               sqlCommand,
                               arguments
                              );

  done          = FALSE;
  error         = ERROR_NONE;
  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  retryCount    = 0;
  do
  {
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(sqlString));
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy));
#endif

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlString,sqlString);
    #endif /* not NDEBUG */

    switch (databaseHandle->databaseNode->databaseSpecifier.type)
    {
      case DATABASE_TYPE_SQLITE3:
        {
          int          sqliteResult;
          sqlite3_stmt *statementHandle;

          if (databaseRowFunction != NULL)
          {
            // prepare SQL statement
            error = sqlite3StatementPrepare(&statementHandle,
                                            databaseHandle->sqlite.handle,
                                            String_cString(sqlString)
                                           );
            if (error != ERROR_NONE)
            {
              break;
            }
            assert(statementHandle != NULL);

            // allocate call-back data
            valueCount = sqlite3_column_count(statementHandle);
            assertx(valueCount >= columnTypeCount,"valueCount=%d columnTypeCount=%d sqlCommand=%s",valueCount,columnTypeCount,sqlCommand);

            values = (DatabaseValue*)malloc(valueCount*sizeof(DatabaseValue));
            if (values == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            // bind results (use CSTRING for undefined columns)
            for (i = 0; i < valueCount; i++)
            {
              if (IS_SET(flags,DATABASE_FLAG_COLUMN_NAMES))
              {
                values[i].name = sqlite3_column_name(statementHandle,i);
              }
              else
              {
                values[i].name = NULL;
              }
              values[i].type = (i < columnTypeCount) ? columnTypes[i] : DATABASE_DATATYPE_CSTRING;
            }

            // step and process rows
            do
            {
              // step
              do
              {
                sqliteResult = sqlite3_step(statementHandle);
                if      (sqliteResult == SQLITE_LOCKED)
                {
                  sqlite3WaitUnlockNotify(databaseHandle->sqlite.handle);
                  sqlite3_reset(statementHandle);
                }
                else if (sqliteResult == SQLITE_MISUSE)
                {
                  HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->sqlite.handle));
                }
              }
              while (sqliteResult == SQLITE_LOCKED);

              // process row
              if      (sqliteResult == SQLITE_ROW)
              {
                for (i = 0; i < valueCount; i++)
                {
                  switch (values[i].type)
                  {
                    case DATABASE_DATATYPE_NONE:
                      break;
                    case DATABASE_DATATYPE:
                      break;
                    case DATABASE_DATATYPE_PRIMARY_KEY:
                    case DATABASE_DATATYPE_KEY:
                      values[i].id = sqlite3_column_int64(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_BOOL:
                      values[i].b = (sqlite3_column_int(statementHandle,i) == 1);
                      break;
                    case DATABASE_DATATYPE_INT:
                      values[i].i = sqlite3_column_int(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_INT64:
                      values[i].i64 = sqlite3_column_int64(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_UINT:
                      values[i].u = (uint)sqlite3_column_int(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_UINT64:
                      values[i].u64 = (uint64)sqlite3_column_int64(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_DOUBLE:
                      values[i].d = sqlite3_column_double(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_DATETIME:
                      values[i].dateTime = (uint64)sqlite3_column_int64(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_STRING:
                      String_setCString(values[i].string, (char*)sqlite3_column_text(statementHandle,i));
                      break;
                    case DATABASE_DATATYPE_CSTRING:
                      values[i].s = (char*)sqlite3_column_text(statementHandle,i);
                      break;
                    case DATABASE_DATATYPE_BLOB:
                      HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                      break;
                    default:
                      #ifndef NDEBUG
                        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                      #endif /* NDEBUG */
                      break;
                  }
                }
                error = databaseRowFunction(values,valueCount,databaseRowUserData);
              }
            }
            while ((error == ERROR_NONE) && (sqliteResult == SQLITE_ROW));

            // free call-back data
            for (i = 0; i < valueCount; i++)
            {
              switch (values[i].type)
              {
                case DATABASE_DATATYPE_NONE:
                  break;
                case DATABASE_DATATYPE:
                  break;
                case DATABASE_DATATYPE_PRIMARY_KEY:
                case DATABASE_DATATYPE_KEY:
                  break;
                case DATABASE_DATATYPE_BOOL:
                  break;
                case DATABASE_DATATYPE_INT:
                  break;
                case DATABASE_DATATYPE_INT64:
                  break;
                case DATABASE_DATATYPE_UINT:
                  break;
                case DATABASE_DATATYPE_UINT64:
                  break;
                case DATABASE_DATATYPE_DOUBLE:
                  break;
                case DATABASE_DATATYPE_DATETIME:
                  break;
                case DATABASE_DATATYPE_CSTRING:
                  break;
                case DATABASE_DATATYPE_BLOB:
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break;
              }
            }
            free(values);

            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) += (ulong)sqlite3_changes(databaseHandle->sqlite.handle);
            }

            // get last insert id
            databaseHandle->lastInsertId = (DatabaseId)sqlite3_last_insert_rowid(databaseHandle->sqlite.handle);;

            // done SQL statement
            sqlite3_finalize(statementHandle);
          }
          else
          {
            // query SQL statement
            error = sqlite3Exec(databaseHandle->sqlite.handle,
                                String_cString(sqlString)
                               );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get last insert id
            databaseHandle->lastInsertId = (DatabaseId)sqlite3_last_insert_rowid(databaseHandle->sqlite.handle);;
          }
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          {
            const uint MAX_TEXT_LENGTH = 4096;

            MYSQL_STMT *statementHandle;
            int        mariadbResult;
            MYSQL_BIND *bind;
            MYSQL_TIME *dateTime;

            // prepare SQL statement
            statementHandle = mysql_stmt_init(databaseHandle->mysql.handle);
            assert(statementHandle != NULL);
            error = mysqlStatementPrepare(statementHandle,
                                          String_cString(sqlString)
                                         );
            if (error != ERROR_NONE)
            {
              mysql_stmt_close(statementHandle);
              break;
            }

            // allocate call-back data
            valueCount = mysql_stmt_field_count(statementHandle);
            assertx(valueCount >= columnTypeCount,"valueCount=%d columnTypeCount=%d sqlCommand=%s",valueCount,columnTypeCount,sqlCommand);

            bind = (MYSQL_BIND*)calloc(valueCount, sizeof(MYSQL_BIND));
            if (bind == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            dateTime = (MYSQL_TIME*)calloc(valueCount, sizeof(MYSQL_TIME));
            if (dateTime == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            values = (DatabaseValue*)malloc(valueCount*sizeof(DatabaseValue));
            if (values == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            // bind results (use CSTRING for undefined columns)
            for (i = 0; i < valueCount; i++)
            {
              values[i].name = NULL;
              values[i].type = (i < columnTypeCount) ? columnTypes[i] : DATABASE_DATATYPE_CSTRING;
              switch (values[i].type)
              {
                case DATABASE_DATATYPE_NONE:
                  break;
                case DATABASE_DATATYPE:
                  break;
                case DATABASE_DATATYPE_PRIMARY_KEY:
                case DATABASE_DATATYPE_KEY:
                  values[i].id = DATABASE_ID_NONE;
                  bind[i].buffer_type   = MYSQL_TYPE_LONG;
                  bind[i].buffer        = (char *)&values[i].id;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  break;
                case DATABASE_DATATYPE_BOOL:
                  values[i].b = FALSE;
                  bind[i].buffer_type   = MYSQL_TYPE_TINY;
                  bind[i].buffer        = (char *)&values[i].b;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  break;
                case DATABASE_DATATYPE_INT:
                  values[i].i = 0LL;
                  bind[i].buffer_type   = MYSQL_TYPE_LONGLONG;
                  bind[i].buffer        = (char *)&values[i].i;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  bind[i].error         = NULL;
                  break;
                case DATABASE_DATATYPE_INT64:
                  values[i].i = 0LL;
                  bind[i].buffer_type   = MYSQL_TYPE_LONGLONG;
                  bind[i].buffer        = (char *)&values[i].i64;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  bind[i].error         = NULL;
                  break;
                case DATABASE_DATATYPE_UINT:
                  values[i].i = 0LL;
                  bind[i].buffer_type   = MYSQL_TYPE_LONGLONG;
                  bind[i].buffer        = (char *)&values[i].u;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  bind[i].error         = NULL;
                  break;
                case DATABASE_DATATYPE_UINT64:
                  values[i].i = 0LL;
                  bind[i].buffer_type   = MYSQL_TYPE_LONGLONG;
                  bind[i].buffer        = (char *)&values[i].u64;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  bind[i].error         = NULL;
                  break;
                case DATABASE_DATATYPE_DOUBLE:
                  values[i].d = 0.0;
                  bind[i].buffer_type   = MYSQL_TYPE_DOUBLE;
                  bind[i].buffer        = (char *)&values[i].d;
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  break;
                case DATABASE_DATATYPE_DATETIME:
                  values[i].dateTime = 0LL;
                  bind[i].buffer_type   = MYSQL_TYPE_DATETIME;
                  bind[i].buffer        = (char *)&dateTime[i];
                  bind[i].is_null       = NULL;
                  bind[i].length        = NULL;
                  break;
                case DATABASE_DATATYPE_STRING:
                case DATABASE_DATATYPE_CSTRING:
                  values[i].text.data   = NULL;
                  values[i].text.length = 0;
                  bind[i].buffer_type   = MYSQL_TYPE_STRING;
                  bind[i].buffer        = (char*)malloc(MAX_TEXT_LENGTH);
                  bind[i].buffer_length = MAX_TEXT_LENGTH;
                  bind[i].is_null       = NULL;
                  bind[i].length        = &values[i].text.length;
                  break;
                case DATABASE_DATATYPE_BLOB:
                  values[i].blob.data   = NULL;
                  values[i].blob.length = 0;
                  HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break;
              }
            }
            if (valueCount > 0)
            {
              if (mysql_stmt_bind_result(statementHandle, bind) != 0)
              {
                free(values);
                free(dateTime);
                free(bind);
                mysql_stmt_close(statementHandle);
                error = ERRORX_(DATABASE_CONNECTION_LOST,
                                mysql_stmt_errno(statementHandle),
                                "%s: %s",
                                mysql_stmt_error(statementHandle),
                                String_cString(sqlString)
                               );
                break;
              }
            }

            // get column names
            if (IS_SET(flags,DATABASE_FLAG_COLUMN_NAMES))
            {
              MYSQL_RES *result = mysql_stmt_result_metadata(statementHandle);

              for (i = 0; i < valueCount; i++)
              {
                MYSQL_FIELD *field = mysql_fetch_field(result);
                values[i].name = (field != NULL) ? field->name : NULL;
              }
            }

            // step and process rows
            error = mariadbStatementExecute(statementHandle);
            if (error != ERROR_NONE)
            {
              free(values);
              free(dateTime);
              free(bind);
              mysql_stmt_close(statementHandle);
              break;
            }

            if (databaseRowFunction != NULL)
            {
// TODO: required?
#if 0
if (mariadb_stmt_store_result(statementHandle) != 0)
{
fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,mariadb_stmt_error(statementHandle));
abort();
}
#endif
              do
              {
                // step
                mariadbResult = mysql_stmt_fetch(statementHandle);

                // process row
                if      (mariadbResult == 0)
                {
                  for (i = 0; i < valueCount; i++)
                  {
                    switch (values[i].type)
                    {
                      case DATABASE_DATATYPE_NONE:
                        break;
                      case DATABASE_DATATYPE:
                        break;
                      case DATABASE_DATATYPE_PRIMARY_KEY:
                      case DATABASE_DATATYPE_KEY:
                        break;
                      case DATABASE_DATATYPE_BOOL:
                        break;
                      case DATABASE_DATATYPE_INT:
                        break;
                      case DATABASE_DATATYPE_INT64:
                        break;
                      case DATABASE_DATATYPE_UINT:
                        break;
                      case DATABASE_DATATYPE_UINT64:
                        break;
                      case DATABASE_DATATYPE_DOUBLE:
                        break;
                      case DATABASE_DATATYPE_DATETIME:
                        break;
                      case DATABASE_DATATYPE_STRING:
                      case DATABASE_DATATYPE_CSTRING:
                        values[i].text.data = bind[i].buffer;
                        break;
                      case DATABASE_DATATYPE_BLOB:
                        HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break;
                    }
                  }
                  error = databaseRowFunction(values,valueCount,databaseRowUserData);
                }
                else if (mariadbResult == 1)
                {
                  error = ERRORX_(DATABASE,
                                  mysql_stmt_errno(statementHandle),
                                  "%s: %s",
                                  mysql_stmt_error(statementHandle),
                                  String_cString(sqlString)
                                 );
                }
                else if (mariadbResult == MYSQL_DATA_TRUNCATED)
                {
                  error = ERRORX_(DATABASE,
                                  mysql_stmt_errno(statementHandle),
                                  "%s: %s",
                                  mysql_stmt_error(statementHandle),
                                  String_cString(sqlString)
                                 );
                }
              }
              while (   (mariadbResult == 0)
                     && (error == ERROR_NONE)
                    );
            }

            // free call-back data
            for (i = 0; i < valueCount; i++)
            {
              switch (values[i].type)
              {
                case DATABASE_DATATYPE_NONE:
                  break;
                case DATABASE_DATATYPE:
                  break;
                case DATABASE_DATATYPE_PRIMARY_KEY:
                case DATABASE_DATATYPE_KEY:
                  break;
                case DATABASE_DATATYPE_BOOL:
                  break;
                case DATABASE_DATATYPE_INT:
                  break;
                case DATABASE_DATATYPE_INT64:
                  break;
                case DATABASE_DATATYPE_UINT:
                  break;
                case DATABASE_DATATYPE_UINT64:
                  break;
                case DATABASE_DATATYPE_DOUBLE:
                  break;
                case DATABASE_DATATYPE_DATETIME:
                  break;
                case DATABASE_DATATYPE_STRING:
                case DATABASE_DATATYPE_CSTRING:
                  free(bind[i].buffer);
                  break;
                case DATABASE_DATATYPE_BLOB:
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break;
              }
            }
            free(values);
            free(dateTime);
            free(bind);

            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) += (ulong)mysql_stmt_affected_rows(statementHandle);
            }

            // get last insert id
            databaseHandle->lastInsertId = (DatabaseId)mysql_stmt_insert_id(statementHandle);

            // done SQL statement
            mysql_stmt_close(statementHandle);
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif
        break;
    }

    #ifndef NDEBUG
      // clear SQL command, backtrace
      String_clear(databaseHandle->debug.current.sqlString);
    #endif /* not NDEBUG */

    // check result
    if      (error == ERROR_NONE)
    {
      done = TRUE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
    {
//fprintf(stderr,"%s, %d: database busy %ld < %ld\n",__FILE__,__LINE__,retryCount*SLEEP_TIME,timeout);
//Database_debugPrintLockInfo(databaseHandle);
      // execute registered busy handlers
//TODO: lock list?
      LIST_ITERATE(&databaseHandle->databaseNode->busyHandlerList,busyHandlerNode)
      {
        assert(busyHandlerNode->function != NULL);
        busyHandlerNode->function(busyHandlerNode->userData);
      }

      Misc_mdelay(SLEEP_TIME);

      // next retry
      retryCount++;

      error = ERROR_NONE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_INTERRUPTED)
    {
      // report interrupt
      error = ERRORX_(DATABASE,
                      sqlite3_errcode(databaseHandle->sqlite.handle),
                      "%s: %s",
                      sqlite3_errmsg(databaseHandle->sqlite.handle),
                      String_cString(sqlString)
                     );
    }
  }
  while (   !done
         && (error == ERROR_NONE)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        );
  String_delete(sqlString);

  if      (error != ERROR_NONE)
  {
    return error;
  }
  else if (retryCount > maxRetryCount)
  {
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }
  else
  {
    return ERROR_NONE;
  }

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : bindValues
* Purpose: bind values in prepared statement
* Input  : databaseStatementHandle  - database statement handle
*          values                   - values; use macro DATABASE_VALUES()
*          valueCount               - number of values
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors bindValues(DatabaseStatementHandle *databaseStatementHandle,
                        const DatabaseValue     values[],
                        uint                    valueCount
                       )
{
  /* data flow:

     application    ->    values -> database internal

                 select             sqlite:
                 insert             MySQL: bind, dateTime
                 update             Postgres: ?
                 delete

   */

  Errors error;
  uint   i;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(checkDatabaseInitialized(databaseStatementHandle->databaseHandle));
  assert((valueCount == 0) || (values != NULL));

  error = ERROR_NONE;
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int sqliteResult;

        // bind values
        for (i = 0; i < valueCount; i++)
        {
// TODO:remove          databaseStatementHandle->values[databaseStatementHandle->valueIndex] = &values[i];

          switch (values[i].type)
          {
            case DATABASE_DATATYPE_NONE:
              break;
            case DATABASE_DATATYPE:
              break;
            case DATABASE_DATATYPE_PRIMARY_KEY:
            case DATABASE_DATATYPE_KEY:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                values[i].id
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_BOOL:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int(databaseStatementHandle->sqlite.statementHandle,
                                              1+databaseStatementHandle->valueIndex,
                                              values[i].b ? 1 : 0
                                             );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_INT:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                values[i].i
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_INT64:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                values[i].i64
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_UINT:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                (int)values[i].u
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_UINT64:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                (int64)values[i].u64
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_DOUBLE:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_double(databaseStatementHandle->sqlite.statementHandle,
                                                 1+databaseStatementHandle->valueIndex,
                                                 values[i].d
                                                );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_DATETIME:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                values[i].dateTime
                                               );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_STRING:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,
                                               1+databaseStatementHandle->valueIndex,
                                               String_cString(values[i].string),
                                               String_length(values[i].string),NULL
                                              );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_CSTRING:
              assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
              sqliteResult = sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,
                                               1+databaseStatementHandle->valueIndex,
                                               values[i].s,
                                               stringLength(values[i].s),
                                               NULL
                                              );
              databaseStatementHandle->valueIndex++;
              break;
            case DATABASE_DATATYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
          if      (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                                sqliteResult,
                                sqlite3_extended_errcode(databaseStatementHandle->databaseHandle->sqlite.handle)
                               );
          }
          else if (sqliteResult != SQLITE_OK)
          {
            error = ERRORX_(DATABASE,
                            sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                            "%s",
                            sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                           );
            break;
          }


        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          // bind values
          for (i = 0; i < valueCount; i++)
          {
  // TODO:remove          databaseStatementHandle->values[databaseStatementHandle->valueIndex] = &values[i];
            switch (values[i].type)
            {
              case DATABASE_DATATYPE_NONE:
                break;
              case DATABASE_DATATYPE:
                break;
              case DATABASE_DATATYPE_PRIMARY_KEY:
              case DATABASE_DATATYPE_KEY:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].id;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_BOOL:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_TINY;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].b;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_INT:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].i;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_INT64:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].i64;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_UINT:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].u;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_UINT64:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].u64;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_DOUBLE:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_DOUBLE;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&values[i].d;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_DATETIME:
                {
                  uint year,month,day;
                  uint hour,minute,second;

                  assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);

                  // convert to internal MariaDB format
                  Misc_splitDateTime(values[i].dateTime,
                                     &year,
                                     &month,
                                     &day,
                                     &hour,
                                     &minute,
                                     &second,
                                     NULL,  // weekDay,
                                     NULL  // isDayLightSaving
                                    );
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].year   = year;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].month  = month;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].day    = day;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].hour   = hour;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].minute = minute;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].second = second;

                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_DATETIME;
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex];
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;

                  databaseStatementHandle->valueIndex++;
                }
                break;
              case DATABASE_DATATYPE_STRING:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_STRING;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char*)String_cString(values[i].string);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_length = String_length(values[i].string);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = 0;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_CSTRING:
                assertx(databaseStatementHandle->valueIndex < databaseStatementHandle->valueCount,"invalid value count: given %u, expected %u",databaseStatementHandle->valueIndex,databaseStatementHandle->valueCount);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_STRING;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = values[i].s;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_length = stringLength(values[i].s);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = 0;
                databaseStatementHandle->valueIndex++;
                break;
              case DATABASE_DATATYPE_BLOB:
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }
          }
        }
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif
      break;
  }

  return error;
}

/***********************************************************************\
* Name   : bindFilters
* Purpose: bind fiters in prepared statement
* Input  : databaseStatementHandle  - database statement handle
*          filters                  - filters; use macro DATABASE_FILTERS()
*          filterCount              - number of filters
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors bindFilters(DatabaseStatementHandle *databaseStatementHandle,
                         const DatabaseFilter    filters[],
                         uint                    filterCount
                        )
{
  /* data flow:

     application    ->    values -> database internal

                 select             sqlite:
                 insert             MySQL: bind, dateTime
                 update             Postgres: ?
                 delete

   */
  Errors error;
  uint   i;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(filters != NULL);
  assertx((databaseStatementHandle->valueIndex+filterCount) <= databaseStatementHandle->valueCount,"invalid filter count: given %u, expected %u",databaseStatementHandle->valueIndex+filterCount,databaseStatementHandle->valueCount);

  error = ERROR_NONE;
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        int sqliteResult;

        for (i = 0; i < filterCount; i++)
        {
          switch (filters[i].type)
          {
            case DATABASE_DATATYPE_NONE:
              break;
            case DATABASE_DATATYPE:
              break;
            case DATABASE_DATATYPE_PRIMARY_KEY:
            case DATABASE_DATATYPE_KEY:
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                filters[i].id
                                               );
              break;
            case DATABASE_DATATYPE_BOOL:
              sqliteResult = sqlite3_bind_int(databaseStatementHandle->sqlite.statementHandle,
                                              1+databaseStatementHandle->valueIndex,
                                              filters[i].b ? 1 : 0
                                             );
              break;
            case DATABASE_DATATYPE_INT:
              sqliteResult = sqlite3_bind_int(databaseStatementHandle->sqlite.statementHandle,
                                              1+databaseStatementHandle->valueIndex,
                                              filters[i].i
                                             );
              break;
            case DATABASE_DATATYPE_INT64:
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                filters[i].i64
                                               );
              break;
            case DATABASE_DATATYPE_UINT:
              sqliteResult = sqlite3_bind_int(databaseStatementHandle->sqlite.statementHandle,
                                              1+databaseStatementHandle->valueIndex,
                                              (int)filters[i].u
                                             );
              break;
            case DATABASE_DATATYPE_UINT64:
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                (uint64)filters[i].u64
                                               );
              break;
            case DATABASE_DATATYPE_DOUBLE:
              sqliteResult = sqlite3_bind_double(databaseStatementHandle->sqlite.statementHandle,
                                                 1+databaseStatementHandle->valueIndex,
                                                 filters[i].d
                                                );
              break;
            case DATABASE_DATATYPE_DATETIME:
              sqliteResult = sqlite3_bind_int64(databaseStatementHandle->sqlite.statementHandle,
                                                1+databaseStatementHandle->valueIndex,
                                                (int64)filters[i].dateTime
                                               );
              break;
            case DATABASE_DATATYPE_STRING:
              sqliteResult = sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,
                                               1+databaseStatementHandle->valueIndex,
                                               String_cString(filters[i].string),
                                               String_length(filters[i].string),
                                               NULL
                                              );
              break;
            case DATABASE_DATATYPE_CSTRING:
              sqliteResult = sqlite3_bind_text(databaseStatementHandle->sqlite.statementHandle,
                                               1+databaseStatementHandle->valueIndex,
                                               filters[i].s,
                                               stringLength(filters[i].s),
                                               NULL
                                              );
              break;
            case DATABASE_DATATYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
          if      (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                                sqliteResult,
                                sqlite3_extended_errcode(databaseStatementHandle->databaseHandle->sqlite.handle)
                               );
          }
          else if (sqliteResult != SQLITE_OK)
          {
            error = ERRORX_(DATABASE,
                            sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                            "%s",
                            sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                           );
            break;
          }

          databaseStatementHandle->valueIndex++;
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          for (i = 0; i < filterCount; i++)
          {
            switch (filters[i].type)
            {
              case DATABASE_DATATYPE_NONE:
                break;
              case DATABASE_DATATYPE:
                break;
              case DATABASE_DATATYPE_PRIMARY_KEY:
              case DATABASE_DATATYPE_KEY:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].id;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_BOOL:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_TINY;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].b;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_INT:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].i;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_INT64:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].i64;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_UINT:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].u;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_UINT64:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_LONGLONG;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].u64;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].error         = NULL;
                break;
              case DATABASE_DATATYPE_DOUBLE:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_DOUBLE;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&filters[i].d;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                break;
              case DATABASE_DATATYPE_DATETIME:
                {
                  uint year,month,day;
                  uint hour,minute,second;

                  // convert to internal MariaDB format
                  Misc_splitDateTime(filters[i].dateTime,
                                     &year,
                                     &month,
                                     &day,
                                     &hour,
                                     &minute,
                                     &second,
                                     NULL,  // weekDay,
                                     NULL  // isDayLightSaving
                                    );
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].year   = year;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].month  = month;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].day    = day;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].hour   = hour;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].minute = minute;
                  databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex].second = second;

                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_DATETIME;
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char *)&databaseStatementHandle->mysql.values.time[databaseStatementHandle->valueIndex];
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                  databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = NULL;
                }
                break;
              case DATABASE_DATATYPE_STRING:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_STRING;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = (char*)String_cString(filters[i].string);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_length = String_length(filters[i].string);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = 0;
                break;
              case DATABASE_DATATYPE_CSTRING:
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_type   = MYSQL_TYPE_STRING;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer        = filters[i].s;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].buffer_length = stringLength(filters[i].s);
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].is_null       = NULL;
                databaseStatementHandle->mysql.values.bind[databaseStatementHandle->valueIndex].length        = 0;
                break;
              case DATABASE_DATATYPE_BLOB:
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break;
            }

            databaseStatementHandle->valueIndex++;
          }
        }
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
    #ifndef NDEBUG
      default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break;
    #endif
  }

  return error;
}

/***********************************************************************\
* Name   : executeQuery
* Purpose: execute query (insert, update, delete) with prepared
*          statement
* Input  : databaseHandle  - database handle
*          changedRowCount - number of changed rows (can be NULL)
*          timeout         - timeout [ms]
*          values          - values; use macro DATABASE_VALUES()
*          valueCount      - number of result columns
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors executeQuery(DatabaseHandle *databaseHandle,
                          ulong          *changedRowCount,
                          long           timeout,
                          const char     *sqlCommand
                         )
{
  #define SLEEP_TIME 500L  // [ms]

  bool                          done;
  Errors                        error;
  uint                          maxRetryCount;
  uint                          retryCount;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert ((databaseHandle->databaseNode->readCount > 0) || (databaseHandle->databaseNode->readWriteCount > 0));
  assert(databaseHandle->sqlite.handle != NULL);
  assert(sqlCommand != NULL);

  done          = FALSE;
  error         = ERROR_NONE;
  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  retryCount    = 0;
  do
  {
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,sqlCommand);
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy));
#endif

    #ifndef NDEBUG
      String_setCString(databaseHandle->debug.current.sqlString,sqlCommand);
    #endif /* not NDEBUG */

    switch (databaseHandle->databaseNode->databaseSpecifier.type)
    {
      case DATABASE_TYPE_SQLITE3:
        {
//fprintf(stderr,"%s, %d: sqlCommands='%s'\n",__FILE__,__LINE__,String_cString(sqlCommand));
          error = sqlite3Exec(databaseHandle->sqlite.handle,
                              sqlCommand
                             );
          if (error != ERROR_NONE)
          {
            break;
          }

          // get last insert id
          databaseHandle->lastInsertId = (DatabaseId)sqlite3_last_insert_rowid(databaseHandle->sqlite.handle);;
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          {
            MYSQL_RES *result;

  //fprintf(stderr,"%s:%d: sqlString=%s\n",__FILE__,__LINE__,String_cString(sqlString));
            error = mysqlQuery(databaseHandle->mysql.handle,sqlCommand);
            if (error != ERROR_NONE)
            {
              break;
            }

  //fprintf(stderr,"%s:%d: mysql_stmt_num_rows(statementHandle)=%d\n",__FILE__,__LINE__,mysql_stmt_num_rows(statementHandle));

            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) += (ulong)mysql_affected_rows(databaseHandle->mysql.handle);
            }

            // get and discard results
            result = mysql_use_result(databaseHandle->mysql.handle);
            mysql_free_result(result);

            // get last insert id
            databaseHandle->lastInsertId = (DatabaseId)mysql_insert_id(databaseHandle->mysql.handle);
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif
        break;
    }

    #ifndef NDEBUG
      // clear SQL command, backtrace
      String_clear(databaseHandle->debug.current.sqlString);
    #endif /* not NDEBUG */

    // check result
    if      (error == ERROR_NONE)
    {
      done = TRUE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
    {
//fprintf(stderr,"%s, %d: database busy %ld < %ld\n",__FILE__,__LINE__,retryCount*SLEEP_TIME,timeout);
//Database_debugPrintLockInfo(databaseHandle);
      // execute registered busy handlers
//TODO: lock list?
      LIST_ITERATE(&databaseHandle->databaseNode->busyHandlerList,busyHandlerNode)
      {
        assert(busyHandlerNode->function != NULL);
        busyHandlerNode->function(busyHandlerNode->userData);
      }

      Misc_mdelay(SLEEP_TIME);

      // next retry
      retryCount++;

      error = ERROR_NONE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_INTERRUPTED)
    {
      // report interrupt
      error = ERRORX_(DATABASE,
                      sqlite3_errcode(databaseHandle->sqlite.handle),
                      "%s: %s",
                      sqlite3_errmsg(databaseHandle->sqlite.handle),
                      sqlCommand
                     );
    }
  }
  while (   !done
         && (error == ERROR_NONE)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        );

  if      (error != ERROR_NONE)
  {
    return error;
  }
  else if (retryCount > maxRetryCount)
  {
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }
  else
  {
    return ERROR_NONE;
  }

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : executePreparedQuery
* Purpose: execute prepared query (insert, update, delete) with prepared
*          statement
* Input  : databaseHandle  - database handle
*          changedRowCount - number of changed rows (can be NULL)
*          timeout         - timeout [ms]
*          values          - values; use macro DATABASE_VALUES()
*          valueCount      - number of result columns
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors executePreparedQuery(DatabaseStatementHandle *databaseStatementHandle,
                                  ulong                   *changedRowCount,
                                  long                    timeout
                                 )
{
  /* data flow:

     database internal -> database

       sqlite:
       MySQL: bind, dateTime
       Postgres: ?
   */
  #define SLEEP_TIME 500L  // [ms]

  bool                          done;
  Errors                        error;
  uint                          maxRetryCount;
  uint                          retryCount;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);

  done          = FALSE;
  error         = ERROR_NONE;
  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  retryCount    = 0;
  do
  {
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(databaseStatementHandle->sqlString));
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy));
#endif

    switch (Database_getType(databaseStatementHandle->databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          int sqliteResult;

          // do query
          sqliteResult = sqlite3_step(databaseStatementHandle->sqlite.statementHandle);
          if      ((sqliteResult == SQLITE_OK) || (sqliteResult == SQLITE_DONE))
          {
            error = ERROR_NONE;
          }
          else if (sqliteResult == SQLITE_LOCKED)
          {
// TODO:
          }
          else if (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",
                                sqliteResult,
                                sqlite3_extended_errcode(databaseStatementHandle->databaseHandle->sqlite.handle)
                               );
          }
          else if (sqliteResult == SQLITE_INTERRUPT)
          {
            error = ERRORX_(INTERRUPTED,
                            sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                            "%s",
                            sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                           );
          }
          else
          {
            error = ERRORX_(DATABASE,
                            sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                            "%s",
                            sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                           );
          }

          if (error == ERROR_NONE)
          {
            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) += (ulong)sqlite3_changes(databaseStatementHandle->databaseHandle->sqlite.handle);
            }

            // get last insert id
            databaseStatementHandle->databaseHandle->lastInsertId = (DatabaseId)sqlite3_last_insert_rowid(databaseStatementHandle->databaseHandle->sqlite.handle);;
          }
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          {
            // bind values
            if (databaseStatementHandle->valueCount > 0)
            {
              if (mysql_stmt_bind_param(databaseStatementHandle->mysql.statementHandle,
                                        databaseStatementHandle->mysql.values.bind
                                       ) != 0
                 )
              {
                error = ERRORX_(DATABASE_BIND,
                                mysql_stmt_errno(databaseStatementHandle->mysql.statementHandle),
                                "parameters: %s",
                                mysql_stmt_error(databaseStatementHandle->mysql.statementHandle)
                               );
                break;
              }
            }
  // TODO: required? queries do not have an result
            if (databaseStatementHandle->resultCount > 0)
            {
              if (mysql_stmt_bind_result(databaseStatementHandle->mysql.statementHandle,
                                         databaseStatementHandle->mysql.results.bind
                                        ) != 0
                 )
              {
                error = ERRORX_(DATABASE_BIND,
                                mysql_stmt_errno(databaseStatementHandle->mysql.statementHandle),
                                "results: %s",
                                mysql_stmt_error(databaseStatementHandle->mysql.statementHandle)
                               );
                break;
              }
            }

            // do query
            error = mariadbStatementExecute(databaseStatementHandle->mysql.statementHandle);
            if (error != ERROR_NONE)
            {
              break;
            }

            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) = (ulong)mysql_affected_rows(databaseStatementHandle->databaseHandle->mysql.handle);
            }

            // get last insert id
            databaseStatementHandle->databaseHandle->lastInsertId = (DatabaseId)mysql_stmt_insert_id(databaseStatementHandle->mysql.statementHandle);
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    // check result
    if      (error == ERROR_NONE)
    {
      done = TRUE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
    {
//fprintf(stderr,"%s, %d: database busy %ld < %ld\n",__FILE__,__LINE__,retryCount*SLEEP_TIME,timeout);
//Database_debugPrintLockInfo(databaseHandle);
      // execute registered busy handlers
//TODO: lock list?
      LIST_ITERATE(&databaseStatementHandle->databaseHandle->databaseNode->busyHandlerList,busyHandlerNode)
      {
        assert(busyHandlerNode->function != NULL);
        busyHandlerNode->function(busyHandlerNode->userData);
      }

      Misc_mdelay(SLEEP_TIME);

      // next retry
      retryCount++;

      error = ERROR_NONE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_INTERRUPTED)
    {
      // report interrupt
      switch (Database_getType(databaseStatementHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = ERRORX_(DATABASE,
                          sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                          "%s",
                          sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                         );
          break;
        case DATABASE_TYPE_MYSQL:
          #if defined(HAVE_MARIADB)
            error = ERRORX_(DATABASE,
                            mysql_errno(databaseStatementHandle->databaseHandle->mysql.handle),
                            "%s",
                            mysql_error(databaseStatementHandle->databaseHandle->mysql.handle)
                           );
          #else /* HAVE_MARIADB */
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          #endif /* HAVE_MARIADB */
          break;
      }
    }
  }
  while (   !done
         && (error == ERROR_NONE)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        );

  if      (error != ERROR_NONE)
  {
    return error;
  }
  else if (retryCount > maxRetryCount)
  {
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }
  else
  {
    return ERROR_NONE;
  }

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : executePreparedStatement
* Purpose: execute single database statement with prepared statement or
*          query
* Input  : databaseHandle      - database handle
*          databaseRowFunction - row call-back function (can be NULL)
*          databaseRowUserData - user data for row call-back
*          changedRowCount     - number of changed rows (can be NULL)
*          timeout             - timeout [ms]
*          columnTypes         - result column types; use macro
*                                DATABASE_COLUMN_TYPES()
*          columnTypeCount     - number of result columns
*          command             - SQL command string with %[l]d, %[']S,
*                                %[']s
*          arguments           - arguments for SQL command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors executePreparedStatement(DatabaseStatementHandle *databaseStatementHandle,
                                      DatabaseRowFunction     databaseRowFunction,
                                      void                    *databaseRowUserData,
                                      ulong                   *changedRowCount,
                                      long                     timeout
                                     )
{
  assert(databaseStatementHandle != NULL);

  /* data flow:

     application    ->    values -> database internal     -> results   ->    application

                 insert             sqlite:                          select
                 update             MySQL: bind, dateTime
                 delete             Postgres: ?

   */
  #define SLEEP_TIME 500L  // [ms]

  bool                          done;
  Errors                        error;
  uint                          maxRetryCount;
  uint                          retryCount;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(databaseStatementHandle->databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle->databaseHandle);
  assert(databaseStatementHandle->databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle->databaseHandle->databaseNode);
  assert ((databaseStatementHandle->databaseHandle->databaseNode->readCount > 0) || (databaseStatementHandle->databaseHandle->databaseNode->readWriteCount > 0));

  // bind prepared values+results
  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          if (databaseStatementHandle->valueCount > 0)
          {
            if (mysql_stmt_bind_param(databaseStatementHandle->mysql.statementHandle,
                                      databaseStatementHandle->mysql.values.bind
                                     ) != 0
               )
            {
              error = ERRORX_(DATABASE_BIND,
                              mysql_stmt_errno(databaseStatementHandle->mysql.statementHandle),
                              "parameters: %s",
                              mysql_stmt_error(databaseStatementHandle->mysql.statementHandle)
                             );
              break;
            }
          }
          if (databaseStatementHandle->resultCount > 0)
          {
            if (mysql_stmt_bind_result(databaseStatementHandle->mysql.statementHandle,
                                       databaseStatementHandle->mysql.results.bind
                                      ) != 0
               )
            {
              error = ERRORX_(DATABASE_BIND,
                              mysql_stmt_errno(databaseStatementHandle->mysql.statementHandle),
                              "results: %s",
                              mysql_stmt_error(databaseStatementHandle->mysql.statementHandle)
                             );
              break;
            }
          }
        }
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  done          = FALSE;
  error         = ERROR_NONE;
  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  retryCount    = 0;
  do
  {
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(databaseStatementHandle->sqlString));
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(Thread_isCurrentThread(databaseStatementHandle->databaseHandle->databaseNode->readWriteLockedBy));
#endif

    // step and process rows
    switch (Database_getType(databaseStatementHandle->databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          if (databaseRowFunction != NULL)
          {
            // step
            while (getNextRow(databaseStatementHandle,timeout))
            {
              error = databaseRowFunction(databaseStatementHandle->results,
                                          databaseStatementHandle->resultCount,
                                          databaseRowUserData
                                         );
            }

            // get number of changes
            if (changedRowCount != NULL)
            {
              (*changedRowCount) += (ulong)sqlite3_changes(databaseStatementHandle->databaseHandle->sqlite.handle);
            }
          }
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          {
            error = mariadbStatementExecute(databaseStatementHandle->mysql.statementHandle);
            if (error != ERROR_NONE)
            {
              break;
            }
            (void)mysql_stmt_store_result(databaseStatementHandle->mysql.statementHandle);

            if (databaseRowFunction != NULL)
            {
              // step
              while (getNextRow(databaseStatementHandle,timeout))
              {
                error = databaseRowFunction(databaseStatementHandle->results,
                                            databaseStatementHandle->resultCount,
                                            databaseRowUserData
                                           );
              }

              // get number of changes
              if (changedRowCount != NULL)
              {
                (*changedRowCount) += (ulong)mysql_stmt_affected_rows(databaseStatementHandle->mysql.statementHandle);
              }
            }
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    // check result
    if      (error == ERROR_NONE)
    {
      done = TRUE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY)
    {
//fprintf(stderr,"%s, %d: database busy %ld < %ld\n",__FILE__,__LINE__,retryCount*SLEEP_TIME,timeout);
//Database_debugPrintLockInfo(databaseHandle);
      // execute registered busy handlers
//TODO: lock list?
      LIST_ITERATE(&databaseStatementHandle->databaseHandle->databaseNode->busyHandlerList,busyHandlerNode)
      {
        assert(busyHandlerNode->function != NULL);
        busyHandlerNode->function(busyHandlerNode->userData);
      }

      Misc_mdelay(SLEEP_TIME);

      // next retry
      retryCount++;

      error = ERROR_NONE;
    }
    else if (Error_getCode(error) == ERROR_CODE_DATABASE_INTERRUPTED)
    {
// TODO: error
      // report interrupt
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseStatementHandle->databaseHandle->sqlite.handle),
                      "%s",
                      sqlite3_errmsg(databaseStatementHandle->databaseHandle->sqlite.handle)
                     );
    }
  }
  while (   !done
         && (error == ERROR_NONE)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        );

  if      (error != ERROR_NONE)
  {
    return error;
  }
  else if (retryCount > maxRetryCount)
  {
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }
  else
  {
    return ERROR_NONE;
  }

  #undef SLEEP_TIME

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getTableColumns
* Purpose: get table column names+types
* Input  : columnNames    - column names variable (can be NULL)
*          columnTypes    - column types variable (can be NULL)
*          columnCount    - column count variable
*          maxColumnCount - max. columns
*          databaseHandle - database handle
*          tableName      - table name
* Output : columnNames - column names
*          columnTypes - column types
*          columnCount - number of columns
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getTableColumns(DatabaseColumnName columnNames[],
                             DatabaseDataTypes  columnTypes[],
                             uint               *columnCount,
                             uint               maxColumnCount,
                             DatabaseHandle     *databaseHandle,
                             const char         *tableName
                            )
{
  Errors error;
  uint   i;

  assert(columnNames != NULL);
  assert(columnTypes != NULL);
  assert(columnCount != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  i = 0;
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        DATABASE_DOX(error,
                     ERRORX_(DATABASE_TIMEOUT,0,""),
                     databaseHandle,
                     DATABASE_LOCK_TYPE_READ,
                     WAIT_FOREVER,
        {
          char sqlCommand[256];

          return Database_get(databaseHandle,
                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                              {
                                const char *name;
                                const char *type;
                                bool       isPrimaryKey;

                                assert(values != NULL);
                                assert(valueCount == 6);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(valueCount);

                                name         = values[1].text.data;
                                type         = values[2].text.data;
                                isPrimaryKey = values[5].b;

                                if (i < maxColumnCount)
                                {
                                  if (columnNames != NULL) stringSet(columnNames[i],sizeof(columnNames[i]),name);
                                  if (   stringEqualsIgnoreCase(type,"INTEGER")
                                      || stringEqualsIgnoreCase(type,"NUMERIC")
                                     )
                                  {
                                    if (isPrimaryKey)
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_PRIMARY_KEY;
                                    }
                                    else
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_INT;
                                    }
                                  }
                                  else if (stringEqualsIgnoreCase(type,"REAL"))
                                  {
                                    if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_DOUBLE;
                                  }
                                  else if (stringEqualsIgnoreCase(type,"TEXT"))
                                  {
                                    if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_CSTRING;
                                  }
                                  else if (stringEqualsIgnoreCase(type,"BLOB"))
                                  {
                                    if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_BLOB;
                                  }
                                  else
                                  {
                                    if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_NONE;
                                  }
                                  i++;
                                }

                                return ERROR_NONE;
                              },NULL),
                              NULL,  // changedRowCount
                              DATABASE_PLAIN(stringFormat(sqlCommand,sizeof(sqlCommand),
                                                          "PRAGMA table_info(%s)",
                                                          tableName
                                                         )
                                            ),
                              DATABASE_COLUMNS
                              (
                                DATABASE_COLUMN_KEY   ("id"),
                                DATABASE_COLUMN_STRING("name"),
                                DATABASE_COLUMN_STRING("type"),
                                DATABASE_COLUMN_BOOL  ("canBeNull"),
                                DATABASE_COLUMN_STRING("defaultValue"),
                                DATABASE_COLUMN_BOOL  ("isPrimaryKey")
                              ),
                              DATABASE_FILTERS_NONE,
                              NULL,  // orderGroup
                              0LL,
                              DATABASE_UNLIMITED
                             );
        });
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          DATABASE_DOX(error,
                       ERRORX_(DATABASE_TIMEOUT,0,""),
                       databaseHandle,
                       DATABASE_LOCK_TYPE_READ,
                       WAIT_FOREVER,
          {
            char sqlCommand[256];

            return Database_get(databaseHandle,
                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                {
                                  const char *name;
                                  const char *type;
                                  bool       isPrimaryKey;

                                  assert(values != NULL);
                                  assert(valueCount == 6);

                                  UNUSED_VARIABLE(valueCount);
                                  UNUSED_VARIABLE(userData);

                                  name         = values[0].text.data;
                                  type         = values[1].text.data;
                                  isPrimaryKey = stringEqualsIgnoreCase(values[3].text.data,"PRI");

                                  if (i < maxColumnCount)
                                  {
                                    if (columnNames != NULL) stringSet(columnNames[i],sizeof(columnNames[i]),name);
                                    if (stringStartsWith(type,"int"))
                                    {
                                      if (isPrimaryKey)
                                      {
                                        if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_PRIMARY_KEY;
                                      }
                                      else
                                      {
                                        if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_INT;
                                      }
                                    }
                                    else if (stringEquals(type,"tinyint(1)"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_BOOL;
                                    }
                                    else if (stringStartsWith(type,"tinyint"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_INT;
                                    }
                                    else if (stringStartsWith(type,"bigint"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_INT64;
                                    }
                                    else if (stringStartsWith(type,"double"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_DOUBLE;
                                    }
                                    else if (stringStartsWith(type,"datetime"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_DATETIME;
                                    }
                                    else if (   stringStartsWith(type,"varchar")
                                             || stringStartsWith(type,"text")
                                            )
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_CSTRING;
                                    }
                                    else if (stringStartsWith(type,"blob"))
                                    {
                                      if (columnTypes != NULL) columnTypes[i] = DATABASE_DATATYPE_BLOB;
                                    }
                                    else
                                    {
                                      HALT_INTERNAL_ERROR("unknown database type '%s'",type);
                                    }
                                    i++;
                                  }

                                  return ERROR_NONE;
                                },NULL),
                                NULL,  // changedRowCount
                                DATABASE_PLAIN(stringFormat(sqlCommand,sizeof(sqlCommand),
                                                            "SHOW COLUMNS FROM %s",
                                                            tableName
                                                           )
                                              ),
                                DATABASE_COLUMNS
                                (
                                  DATABASE_COLUMN_STRING("name"),
                                  DATABASE_COLUMN_STRING("type"),
                                  DATABASE_COLUMN_BOOL  ("canBeNull"),
                                  DATABASE_COLUMN_STRING("isPrimaryKey"),
                                  DATABASE_COLUMN_STRING("defaultValue"),
                                  DATABASE_COLUMN_STRING("extra")
                                ),
                                DATABASE_FILTERS_NONE,
                                NULL,  // orderGroup
                                0LL,
                                DATABASE_UNLIMITED
                               );
          });
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }
  if (columnCount != NULL) (*columnCount) = i;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : findTableColumn
* Purpose: find table column
* Input  : columns    - table columns
*          columnName - column name
* Output : -
* Return : column value or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DatabaseValue *findTableColumn(const DatabaseColumnInfo *columnInfo, const char *columnName)
{
  uint i;

  assert(columnInfo != NULL);

  for (i = 0; i < columnInfo->count; i++)
  {
    if (stringEquals(columnInfo->names[i],columnName))
    {
      return &columnInfo->values[i];
    }
  }

  return NULL;
}

/*---------------------------------------------------------------------*/

Errors Database_initAll(void)
{
  int sqliteResult;

  #ifndef DATABASE_LOCK_PER_INSTANCE
    // init global lock
    if (pthread_mutexattr_init(&databaseLockAttribute) != 0)
    {
      return ERRORX_(DATABASE,0,"init locking");
    }
    pthread_mutexattr_settype(&databaseLockAttribute,PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&databaseLock,&databaseLockAttribute) != 0)
    {
      pthread_mutexattr_destroy(&databaseLockAttribute);
      return ERRORX_(DATABASE,0,"init locking");
    }
    databaseLockBy.threadId = THREAD_ID_NONE;
    databaseLockBy.fileName = NULL;
    databaseLockBy.lineNb   = 0;
  #endif /* not DATABASE_LOCK_PER_INSTANCE */

  // init database list
  List_init(&databaseList);
  Semaphore_init(&databaseList.lock,SEMAPHORE_TYPE_BINARY);

  // enable sqlite3 multi-threaded support
  sqliteResult = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
  if (sqliteResult != SQLITE_OK)
  {
    Semaphore_done(&databaseList.lock);
    List_done(&databaseList,CALLBACK_((ListNodeFreeFunction)freeDatabaseNode,NULL));
    pthread_mutex_destroy(&databaseLock);
    pthread_mutexattr_destroy(&databaseLockAttribute);
    return ERRORX_(DATABASE,sqliteResult,"enable multi-threading");
  }

  #ifdef HAVE_MARIADB
    // init MariaDB
    mysql_library_init(0,NULL,NULL);
  #endif

  return ERROR_NONE;
}

void Database_doneAll(void)
{
  #ifdef HAVE_MARIADB
    // done MariaDB
    mysql_library_end();
  #endif

  // done database list
  Semaphore_done(&databaseList.lock);
  List_done(&databaseList,CALLBACK_((ListNodeFreeFunction)freeDatabaseNode,NULL));

  #ifndef DATABASE_LOCK_PER_INSTANCE
    // done global lock
    pthread_mutex_destroy(&databaseLock);
    pthread_mutexattr_destroy(&databaseLockAttribute);
  #endif /* not DATABASE_LOCK_PER_INSTANCE */
}

void Database_parseSpecifier(DatabaseSpecifier *databaseSpecifier,
                             const char        *databaseURI,
                             const char        *defaultDatabaseName,
                             bool              *validURIPrefixFlag
                            )
{
  const char *s1,*s2,*s3,*s4;
  size_t     n1,n2,n3,n4;

  assert(databaseURI != NULL);

  // get database type and open/connect data
  if      (   (databaseURI != NULL)
           && stringMatch(databaseURI,
                          "^(sqlite|sqlite3):(.*)",
                          STRING_NO_ASSIGN,
                          STRING_NO_ASSIGN,
                          STRING_NO_ASSIGN,
                          STRING_NO_ASSIGN,
                          &s1,&n1,
                          NULL
                         )
          )
  {
    databaseSpecifier->type            = DATABASE_TYPE_SQLITE3;
    databaseSpecifier->sqlite.fileName = String_setBuffer(String_new(),s1,n1);
    if (validURIPrefixFlag != NULL) (*validURIPrefixFlag) = TRUE;
  }
  else if (   (databaseURI != NULL)
           && stringMatch(databaseURI,
                          "^mariadb:([^:]+):([^:]+):([^:]*):(.*)",
                          STRING_NO_ASSIGN,
                          STRING_NO_ASSIGN,
                          &s1,&n1,
                          &s2,&n2,
                          &s3,&n3,
                          &s4,&n4,
                          NULL
                         )
          )
  {
    #if defined(HAVE_MARIADB)
      databaseSpecifier->type              = DATABASE_TYPE_MYSQL;
      databaseSpecifier->mysql.serverName  = String_setBuffer(String_new(),s1,n1);
      databaseSpecifier->mysql.userName    = String_setBuffer(String_new(),s2,n2);
      Password_init(&databaseSpecifier->mysql.password);
      Password_setBuffer(&databaseSpecifier->mysql.password,s3,n3);
      databaseSpecifier->mysql.databaseName = String_setBuffer(String_new(),s4,n4);
    #else /* HAVE_MARIADB */
// TODO:
    #endif /* HAVE_MARIADB */
    if (validURIPrefixFlag != NULL) (*validURIPrefixFlag) = TRUE;
  }
  else if (   (databaseURI != NULL)
           && stringMatch(databaseURI,
                          "^mariadb:([^:]+):([^:]+):(.*)",
                          STRING_NO_ASSIGN,
                          STRING_NO_ASSIGN,
                          &s1,&n1,
                          &s2,&n2,
                          &s3,&n3,
                          NULL
                         )
          )
  {
    #if defined(HAVE_MARIADB)
      databaseSpecifier->type               = DATABASE_TYPE_MYSQL;
      databaseSpecifier->mysql.serverName   = String_setBuffer(String_new(),s1,n1);
      databaseSpecifier->mysql.userName     = String_setBuffer(String_new(),s2,n2);
      Password_init(&databaseSpecifier->mysql.password);
      Password_setBuffer(&databaseSpecifier->mysql.password,s3,n3);
      databaseSpecifier->mysql.databaseName = String_newCString(defaultDatabaseName);
    #else /* HAVE_MARIADB */
// TODO:
    #endif /* HAVE_MARIADB */
    if (validURIPrefixFlag != NULL) (*validURIPrefixFlag) = TRUE;
  }
  else
  {
    databaseSpecifier->type            = DATABASE_TYPE_SQLITE3;
    databaseSpecifier->sqlite.fileName = String_setCString(String_new(),databaseURI);
    if (validURIPrefixFlag != NULL) (*validURIPrefixFlag) = FALSE;
  }
}

void Database_copySpecifier(DatabaseSpecifier       *databaseSpecifier,
                            const DatabaseSpecifier *fromDatabaseSpecifier
                           )
{
  assert(databaseSpecifier != NULL);
  assert(fromDatabaseSpecifier != NULL);

  databaseSpecifier->type = fromDatabaseSpecifier->type;
  switch (fromDatabaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      databaseSpecifier->sqlite.fileName = String_duplicate(fromDatabaseSpecifier->sqlite.fileName);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        databaseSpecifier->mysql.serverName   = String_duplicate(fromDatabaseSpecifier->mysql.serverName);
        databaseSpecifier->mysql.userName     = String_duplicate(fromDatabaseSpecifier->mysql.userName);
        Password_initDuplicate(&databaseSpecifier->mysql.password,&fromDatabaseSpecifier->mysql.password);
        databaseSpecifier->mysql.databaseName = String_duplicate(fromDatabaseSpecifier->mysql.databaseName);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }
}

void Database_doneSpecifier(DatabaseSpecifier *databaseSpecifier)
{
  assert(databaseSpecifier != NULL);

  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      String_delete(databaseSpecifier->sqlite.fileName);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        String_delete(databaseSpecifier->mysql.databaseName);
        Password_done(&databaseSpecifier->mysql.password);
        String_delete(databaseSpecifier->mysql.userName);
        String_delete(databaseSpecifier->mysql.serverName);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }
}

DatabaseSpecifier *Database_newSpecifier(const char *databaseURI,
                                         const char *defaultDatabaseName,
                                         bool       *validURIPrefixFlag
                                        )
{
  DatabaseSpecifier *databaseSpecifier;

  databaseSpecifier = (DatabaseSpecifier*)malloc(sizeof(DatabaseSpecifier));
  if (databaseSpecifier == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  Database_parseSpecifier(databaseSpecifier,databaseURI,defaultDatabaseName,validURIPrefixFlag);

  return databaseSpecifier;
}

DatabaseSpecifier *Database_duplicateSpecifier(const DatabaseSpecifier *databaseSpecifier)
{
  DatabaseSpecifier *newDatabaseSpecifier;

  newDatabaseSpecifier =(DatabaseSpecifier*)malloc(sizeof(DatabaseSpecifier));
  if (newDatabaseSpecifier == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Database_copySpecifier(newDatabaseSpecifier,databaseSpecifier);

  return newDatabaseSpecifier;
}

void Database_deleteSpecifier(DatabaseSpecifier *databaseSpecifier)
{
  assert(databaseSpecifier != NULL);

  Database_doneSpecifier(databaseSpecifier);
  free(databaseSpecifier);
}

bool Database_exists(const DatabaseSpecifier *databaseSpecifier,
                     ConstString             databaseName
                    )
{
  Errors         error;
  DatabaseHandle databaseHandle;
  bool           existsFlag;

  #ifdef NDEBUG
    error = openDatabase(&databaseHandle,databaseSpecifier,databaseName,DATABASE_OPEN_MODE_READ,NO_WAIT);
  #else /* not NDEBUG */
    error = openDatabase(__FILE__,__LINE__,&databaseHandle,databaseSpecifier,databaseName,DATABASE_OPEN_MODE_READ,NO_WAIT);
  #endif /* NDEBUG */
  if (error == ERROR_NONE)
  {
    existsFlag = TRUE;
    #ifdef NDEBUG
      closeDatabase(&databaseHandle);
    #else /* not NDEBUG */
      closeDatabase(__FILE__,__LINE__,&databaseHandle);
    #endif /* NDEBUG */
  }
  else
  {
    existsFlag = FALSE;
  }

  return existsFlag;
}

bool Database_equalSpecifiers(const DatabaseSpecifier *databaseSpecifier0, const DatabaseSpecifier *databaseSpecifier1)
{
  assert(databaseSpecifier0 != NULL);
  assert(databaseSpecifier0 != NULL);

  return     (databaseSpecifier0->type == databaseSpecifier1->type)
          && (   (   (databaseSpecifier0->type == DATABASE_TYPE_SQLITE3)
                  && String_equals(databaseSpecifier0->sqlite.fileName,databaseSpecifier1->sqlite.fileName)
                 )
             );

  #if defined(HAVE_MARIADB)
    return     (databaseSpecifier0->type == DATABASE_TYPE_MYSQL)
            && String_equals(databaseSpecifier0->mysql.serverName,databaseSpecifier1->mysql.serverName)
            && String_equals(databaseSpecifier0->mysql.userName,databaseSpecifier1->mysql.userName)
            && String_equals(databaseSpecifier0->mysql.databaseName,databaseSpecifier1->mysql.databaseName);
  #else /* HAVE_MARIADB */
  #endif /* HAVE_MARIADB */

  return FALSE;
}

String Database_getPrintableName(String                  string,
                                 const DatabaseSpecifier *databaseSpecifier
                                )
{
  assert(string != NULL);
  assert(databaseSpecifier != NULL);

  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      String_format(string,
                    "sqlite:%S",
                    databaseSpecifier->sqlite.fileName
                   );
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        String_format(string,
                      "mariadb:%S:%S:*:%S",
                      databaseSpecifier->mysql.serverName,
                      databaseSpecifier->mysql.userName,
                      databaseSpecifier->mysql.databaseName
                     );
      #else /* not HAVE_MARIADB */
        String_clear(string);
      #endif /* HAVE_MARIADB */
      break;
  }

  return string;
}

Errors Database_rename(DatabaseSpecifier *databaseSpecifier,
                       ConstString       newDatabaseName
                      )
{
  Errors error;

  assert(databaseSpecifier != NULL);
  assert(newDatabaseName != NULL);

  // init variables

  error = ERROR_UNKNOWN;
  switch (databaseSpecifier->type)
  {
    case DATABASE_TYPE_SQLITE3:
      error = File_rename(databaseSpecifier->sqlite.fileName,
                          newDatabaseName,
                          NULL
                         );
      String_set(databaseSpecifier->sqlite.fileName,newDatabaseName);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          DatabaseHandle     databaseHandle;
          uint               i;
          StringList         tableNameList;
          StringListIterator iterator;
          String             name;

          // open database
          error = Database_open(&databaseHandle,databaseSpecifier,DATABASE_OPEN_MODE_READ,NO_WAIT);
          if (error != ERROR_NONE)
          {
            return error;
          }

          // create new database
          i = 0;
          do
          {
            char sqlCommand[256];

            stringFormat(sqlCommand,sizeof(sqlCommand),
                         "CREATE DATABASE %s \
                          CHARACTER SET '%s' \
                          COLLATE '%s_bin' \
                         ",
                         String_cString(newDatabaseName),
                         MARIADB_CHARACTER_SETS[i],
                         MARIADB_CHARACTER_SETS[i]
                        );
            error = mysqlQuery(databaseHandle.mysql.handle,
                               sqlCommand
                              );
            i++;
          }
          while (   (error != ERROR_NONE)
                 && (i < SIZE_OF_ARRAY(MARIADB_CHARACTER_SETS))
                );
          if (error != ERROR_NONE)
          {
            Database_close(&databaseHandle);
            return error;
          }

          // rename tables
          StringList_init(&tableNameList);
          error = Database_getTableList(&tableNameList,&databaseHandle);
          STRINGLIST_ITERATEX(&tableNameList,iterator,name,error == ERROR_NONE)
          {
            error = Database_execute(&databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     DATABASE_FLAG_NONE,
                                     DATABASE_COLUMN_TYPES(),
                                     "RENAME TABLE %s TO %s.%s",
                                     String_cString(name),
                                     String_cString(newDatabaseName),
                                     String_cString(name)
                                    );
          }
          if (error != ERROR_NONE)
          {
            Database_close(&databaseHandle);
            StringList_done(&tableNameList);
            return error;
          }
          StringList_done(&tableNameList);

          // close database
          Database_close(&databaseHandle);

          // free resources

          String_set(databaseSpecifier->mysql.databaseName,newDatabaseName);
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle          *databaseHandle,
                       const DatabaseSpecifier *databaseSpecifier,
                       DatabaseOpenModes       openDatabaseMode,
                       long                    timeout
                      )
#else /* not NDEBUG */
  Errors __Database_open(const char              *__fileName__,
                         ulong                   __lineNb__,
                         DatabaseHandle          *databaseHandle,
                         const DatabaseSpecifier *databaseSpecifier,
                         DatabaseOpenModes       openDatabaseMode,
                         long                    timeout
                        )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseSpecifier != NULL);

  #ifdef NDEBUG
    error = openDatabase(databaseHandle,databaseSpecifier,NULL,openDatabaseMode,timeout);
  #else /* not NDEBUG */
    error = openDatabase(__fileName__,__lineNb__,databaseHandle,databaseSpecifier,NULL,openDatabaseMode,timeout);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  #ifdef DATABASE_DEBUG
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        fprintf(stderr,
                "Database debug: opened 'sqlite:%s'\n",
                String_cString(databaseHandle->databaseNode->databaseSpecifier.sqlite.fileName)
               );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          fprintf(stderr,
                  "Database debug: opened 'mariadb:%s:%s:*:%s'\n",
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.serverName)
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.userName)
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.databaseName)
                 );
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }
  #endif

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseHandle,DatabaseHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,DatabaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Database_close(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  void __Database_close(const char     *__fileName__,
                        ulong          __lineNb__,
                        DatabaseHandle *databaseHandle
                       )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->readWriteLockCount == 0);
  assert(databaseHandle->readLockCount == 0);
  assert(checkDatabaseInitialized(databaseHandle));

// TODO: remove
#if 0
  (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "COMMIT"
                          );
#endif

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseHandle,DatabaseHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,DatabaseHandle);
  #endif /* NDEBUG */

  #ifdef DATABASE_DEBUG
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        fprintf(stderr,
                "Database debug: close 'sqlite:%s'\n",
                String_cString(databaseHandle->databaseNode->databaseSpecifier.sqlite.fileName)
               );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          fprintf(stderr,
                  "Database debug: close 'mariadb:%s:%s:*:%s'\n",
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.serverName)
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.userName)
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.mariadb.databaseName)
                 );
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }
  #endif

  #ifdef NDEBUG
    closeDatabase(databaseHandle);
  #else /* not NDEBUG */
    closeDatabase(__fileName__,__lineNb__,databaseHandle);
  #endif /* NDEBUG */
}

void Database_addBusyHandler(DatabaseHandle              *databaseHandle,
                             DatabaseBusyHandlerFunction busyHandlerFunction,
                             void                        *busyHandlerUserData
                            )
{
  DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(busyHandlerFunction != NULL);

  SEMAPHORE_LOCKED_DO(&databaseHandle->databaseNode->busyHandlerList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find existing busy handler
    busyHandlerNode = LIST_FIND(&databaseHandle->databaseNode->busyHandlerList,
                                busyHandlerNode,
                                   (busyHandlerNode->function == busyHandlerFunction)
                                && (busyHandlerNode->userData == busyHandlerUserData)
                               );

    // add busy handler
    if (busyHandlerNode == NULL)
    {
      busyHandlerNode = LIST_NEW_NODE(DatabaseBusyHandlerNode);
      if (busyHandlerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      busyHandlerNode->function = busyHandlerFunction;
      busyHandlerNode->userData = busyHandlerUserData;
      List_append(&databaseHandle->databaseNode->busyHandlerList,busyHandlerNode);
    }
  }
}

void Database_removeBusyHandler(DatabaseHandle              *databaseHandle,
                                DatabaseBusyHandlerFunction busyHandlerFunction,
                                void                        *busyHandlerUserData
                               )
{
  DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(busyHandlerFunction != NULL);

  SEMAPHORE_LOCKED_DO(&databaseHandle->databaseNode->busyHandlerList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find existing busy handler
    busyHandlerNode = LIST_FIND(&databaseHandle->databaseNode->busyHandlerList,
                                busyHandlerNode,
                                   (busyHandlerNode->function == busyHandlerFunction)
                                && (busyHandlerNode->userData == busyHandlerUserData)
                               );

    // remove busy handler
    if (busyHandlerNode != NULL)
    {
      List_remove(&databaseHandle->databaseNode->busyHandlerList,busyHandlerNode);
      LIST_DELETE_NODE(busyHandlerNode);
    }
  }
}

void Database_addProgressHandler(DatabaseHandle                  *databaseHandle,
                                 DatabaseProgressHandlerFunction progressHandlerFunction,
                                 void                            *progressHandlerUserData
                                )
{
  DatabaseProgressHandlerNode *progressHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(progressHandlerFunction != NULL);

  SEMAPHORE_LOCKED_DO(&databaseHandle->databaseNode->progressHandlerList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find existing busy handler
    progressHandlerNode = LIST_FIND(&databaseHandle->databaseNode->progressHandlerList,
                                    progressHandlerNode,
                                       (progressHandlerNode->function == progressHandlerFunction)
                                    && (progressHandlerNode->userData == progressHandlerUserData)
                                   );

    // add progress handler
    if (progressHandlerNode == NULL)
    {
      progressHandlerNode = LIST_NEW_NODE(DatabaseProgressHandlerNode);
      if (progressHandlerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      progressHandlerNode->function = progressHandlerFunction;
      progressHandlerNode->userData = progressHandlerUserData;
      List_append(&databaseHandle->databaseNode->progressHandlerList,progressHandlerNode);
    }
  }
}

void Database_removeProgressHandler(DatabaseHandle                  *databaseHandle,
                                    DatabaseProgressHandlerFunction progressHandlerFunction,
                                    void                            *progressHandlerUserData
                                   )
{
  DatabaseProgressHandlerNode *progressHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(progressHandlerFunction != NULL);

  SEMAPHORE_LOCKED_DO(&databaseHandle->databaseNode->progressHandlerList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find existing progress handler
    progressHandlerNode = LIST_FIND(&databaseHandle->databaseNode->progressHandlerList,
                                    progressHandlerNode,
                                       (progressHandlerNode->function == progressHandlerFunction)
                                    && (progressHandlerNode->userData == progressHandlerUserData)
                                   );

    // remove progress handler
    if (progressHandlerNode != NULL)
    {
      List_remove(&databaseHandle->databaseNode->progressHandlerList,progressHandlerNode);
      LIST_DELETE_NODE(progressHandlerNode);
    }
  }
}

void Database_interrupt(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifdef DATABASE_SUPPORT_INTERRUPT
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        sqlite3_interrupt(databaseHandle->sqlite.handle);
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }
  #endif /* DATABASE_SUPPORT_INTERRUPT */
}

Errors Database_getTableList(StringList     *tableList,
                             DatabaseHandle *databaseHandle
                            )
{
  Errors error;

  assert(tableList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_get(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               StringList_appendCString(tableList,values[0].text.data);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "sqlite_master"
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_STRING("name")
                             ),
                             "type='table'",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // orderGroup
                             0LL,
                             DATABASE_UNLIMITED
                            );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          error = Database_get(databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 2);

                                 UNUSED_VARIABLE(valueCount);
                                 UNUSED_VARIABLE(userData);

                                 StringList_appendCString(tableList,values[0].text.data);

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_PLAIN("SHOW FULL TABLES WHERE TABLE_TYPE LIKE 'BASE TABLE'"),
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_STRING("name")
                               ),
                               DATABASE_FILTERS_NONE,
                               NULL,  // orderGroup
                               0LL,
                               DATABASE_UNLIMITED
                              );
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    return error;
  });

  return ERROR_NONE;
}

Errors Database_getViewList(StringList     *viewList,
                            DatabaseHandle *databaseHandle
                           )
{
  Errors error;

  assert(viewList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_get(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               StringList_appendCString(viewList,values[0].text.data);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "sqlite_master"
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_STRING("name")
                             ),
                             "type='view'",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // orderGroup
                             0LL,
                             DATABASE_UNLIMITED
                            );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          error = Database_get(databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 2);

                                 UNUSED_VARIABLE(valueCount);
                                 UNUSED_VARIABLE(userData);

                                 StringList_appendCString(viewList,values[0].text.data);

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_PLAIN("SHOW FULL TABLES WHERE TABLE_TYPE LIKE 'VIEW'"),
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_STRING("name")
                               ),
                               DATABASE_FILTERS_NONE,
                               NULL,  // orderGroup
                               0LL,
                               DATABASE_UNLIMITED
                              );
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    return error;
  });

  return ERROR_NONE;
}

Errors Database_getIndexList(StringList     *indexList,
                             DatabaseHandle *databaseHandle,
                             const char     *tableName
                            )
{
  Errors error;

  assert(indexList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(indexList);

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_get(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               if (!stringStartsWith(values[0].text.data,"sqlite_autoindex"))
                               {
                                 StringList_appendCString(indexList,values[0].text.data);
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "sqlite_master"
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_STRING("name")
                             ),
                             "type='index'",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // orderGroup
                             0LL,
                             DATABASE_UNLIMITED
                            );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          if (tableName != NULL)
          {
             error = Database_execute(databaseHandle,
                                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                      {
                                        assert(values != NULL);
                                        assert(valueCount >= 3);

                                        UNUSED_VARIABLE(valueCount);
                                        UNUSED_VARIABLE(userData);

                                        StringList_appendCString(indexList,values[2].text.data);

                                        return ERROR_NONE;
                                      },NULL),
                                      NULL,  // changedRowCount
                                      DATABASE_FLAG_NONE,
                                      DATABASE_COLUMN_TYPES(CSTRING,CSTRING,CSTRING,CSTRING,CSTRING),
                                      "SHOW INDEXES FROM %s",
                                      tableName
                                     );
          }
          else
          {
            error = Database_execute(databaseHandle,
                                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                     {
                                       const char *tableName;

                                       assert(values != NULL);
                                       assert(valueCount >= 1);

                                       UNUSED_VARIABLE(valueCount);
                                       UNUSED_VARIABLE(userData);

                                       tableName = values[0].text.data;
                                       return Database_execute(databaseHandle,
                                                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                               {
                                                                 String indexName;

                                                                 assert(values != NULL);
                                                                 assert(valueCount == 13);

                                                                 UNUSED_VARIABLE(valueCount);
                                                                 UNUSED_VARIABLE(userData);

                                                                 indexName = String_format(String_new(),
                                                                                           "%s:%s",
                                                                                           values[0].text.data,
                                                                                           values[2].text.data
                                                                                          );
                                                                 if (!StringList_contains(indexList,indexName))
                                                                 {
                                                                   StringList_append(indexList,indexName);
                                                                 }
                                                                 String_delete(indexName);

                                                                 return ERROR_NONE;
                                                               },NULL),
                                                               NULL,  // changedRowCount
                                                               DATABASE_FLAG_NONE,
                                                               DATABASE_COLUMN_TYPES(STRING,
                                                                                     BOOL,
                                                                                     STRING,
                                                                                     UINT,
                                                                                     STRING,
                                                                                     STRING,
                                                                                     STRING,
                                                                                     STRING,
                                                                                     BOOL,
                                                                                     BOOL,
                                                                                     STRING,
                                                                                     STRING,
                                                                                     STRING
                                                                                    ),
                                                               "SHOW INDEXES FROM %s",
                                                               tableName
                                                              );
                                     },NULL),
                                     NULL,  // changedRowCount
                                     DATABASE_FLAG_NONE,
                                     DATABASE_COLUMN_TYPES(CSTRING),
                                     "SHOW TABLES"
                                    );
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    return error;
  });

  return ERROR_NONE;
}

Errors Database_getTriggerList(StringList     *triggerList,
                               DatabaseHandle *databaseHandle
                              )
{
  Errors error;

  assert(triggerList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(triggerList);

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_execute(databaseHandle,
                                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                 {
                                   assert(values != NULL);
                                   assert(valueCount == 1);

                                   UNUSED_VARIABLE(valueCount);
                                   UNUSED_VARIABLE(userData);

                                   StringList_appendCString(triggerList,values[0].text.data);

                                   return ERROR_NONE;
                                 },NULL),
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 DATABASE_COLUMN_TYPES(CSTRING),
                                 "SELECT name FROM sqlite_master where type='trigger'"
                                );
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          error = Database_execute(databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount >= 1);

                                     UNUSED_VARIABLE(valueCount);
                                     UNUSED_VARIABLE(userData);

                                     StringList_appendCString(triggerList,values[0].text.data);

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMN_TYPES(CSTRING),
                                   "SHOW TRIGGERS"
                                  );
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    return error;
  });

  return ERROR_NONE;
}

#ifdef NDEBUG
  bool Database_lock(DatabaseHandle    *databaseHandle,
                     DatabaseLockTypes lockType,
                     long              timeout
                    )
#else /* not NDEBUG */
  bool __Database_lock(const char        *__fileName__,
                       ulong             __lineNb__,
                       DatabaseHandle    *databaseHandle,
                       DatabaseLockTypes lockType,
                       long              timeout
                      )
#endif /* NDEBUG */
{
//TODO: how to handle lost triggers?
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
  #define DT (5*MS_PER_SECOND)
#endif

  bool lockedFlag;
//TODO: how to handle lost triggers?
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
  TimeoutInfo timeoutInfo;
  uint t;
#endif

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  lockedFlag = FALSE;

  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_NONE:
      break;
    case DATABASE_LOCK_TYPE_READ:
      DATABASE_HANDLE_LOCKED_DOX(lockedFlag,
                                 databaseHandle,
      {
#if 0
        assertx(isReadLock(databaseHandle) || isReadWriteLock(databaseHandle) || ((databaseHandle->databaseNode->pendingReadCount == 0) && (databaseHandle->databaseNode->pendingReadWriteCount == 0)),
                "R: readCount=%u readWriteCount=%u pendingReadCount=%u pendingReadWriteCount=%u",
                databaseHandle->databaseNode->readCount,databaseHandle->databaseNode->readWriteCount,databaseHandle->databaseNode->pendingReadCount,databaseHandle->databaseNode->pendingReadWriteCount
               );
#endif

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s LOCK   init: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

        // request read lock
        pendingReadsIncrement(databaseHandle);
        {
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
          Misc_initTimeout(&timeoutInfo,timeout);
#endif
          // check if there is no writer
          if (   !isOwnReadWriteLock(databaseHandle)
              && isReadWriteLock(databaseHandle)
             )
          {
            // wait read/write end
//TODO
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
            if (timeout != WAIT_FOREVER)
            {
              do
              {
                t = MIN(Misc_getRestTimeout(&timeoutInfo),DT);
//fprintf(stderr,"%s, %d: a %ld %lu %u\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo),t);

                waitTriggerReadWrite(databaseHandle,t);
              }
              while (   isReadWriteLock(databaseHandle)
                     && !Misc_isTimeout(&timeoutInfo)
                    );
              if (isReadWriteLock(databaseHandle))
              {
//fprintf(stderr,"%s, %d: stop DATABASE_LOCK_TYPE_READ %d %d\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo)); asm("int3");
                Misc_doneTimeout(&timeoutInfo);
                pendingReadsDecrement(databaseHandle);
                return FALSE;
              }
            }
            else
            {
              // Note: do wait with timeout as a work-around for lost triggers
              do
              {
                waitTriggerReadWrite(databaseHandle,5*MS_PER_SECOND);
              }
              while (isReadWriteLock(databaseHandle));
            }
#else
            do
            {
              DATABASE_DEBUG_LOCK_ASSERT(databaseHandle,isReadWriteLock(databaseHandle));
              if (!waitTriggerReadWrite(databaseHandle,timeout))
              {
                pendingReadsDecrement(databaseHandle);
                return FALSE;
              }
            }
            while (isReadWriteLock(databaseHandle));
#endif
            assert(Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy));
          }
          DATABASE_DEBUG_LOCK_ASSERTX(databaseHandle,
                                      isOwnReadWriteLock(databaseHandle) || !isReadWriteLock(databaseHandle),
                                      "isOwnReadWriteLock=%d !isReadWriteLock=%d",
                                      isOwnReadWriteLock(databaseHandle),
                                      !isReadWriteLock(databaseHandle)
                                     );

          // read lock aquired
          #ifdef NDEBUG
            readsIncrement(databaseHandle);
          #else /* not NDEBUG */
            __readsIncrement(__fileName__,__lineNb__,databaseHandle);
          #endif /* NDEBUG */

#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
          Misc_doneTimeout(&timeoutInfo);
#endif
        }
        pendingReadsDecrement(databaseHandle);

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s LOCK   done: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

//        #ifndef xxxNDEBUG
#if 0
          databaseHandle->debug.locked.threadId = Thread_getCurrentId();
          databaseHandle->debug.locked.fileName = __fileName__;
          databaseHandle->debug.locked.lineNb   = __lineNb__;
          databaseHandle->debug.locked.text[0]  = '\0';
          databaseHandle->debug.locked.t0       = Misc_getTimestamp();
          debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                            databaseHandle->databaseNode->debug.history,
                                            &databaseHandle->databaseNode->debug.historyIndex,
                                            SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                            DATABASE_HISTORY_TYPE_LOCK_READ
                                           );
        #endif /* not NDEBUG */

        return TRUE;
      });
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      DATABASE_HANDLE_LOCKED_DOX(lockedFlag,
                                 databaseHandle,
      {
#if 0
        assertx(isReadLock(databaseHandle) || isReadWriteLock(databaseHandle) || ((databaseHandle->databaseNode->pendingReadCount == 0) && (databaseHandle->databaseNode->pendingReadWriteCount == 0)),
                "RW: readCount=%u readWriteCount=%u pendingReadCount=%u pendingReadWriteCount=%u",
                databaseHandle->databaseNode->readCount,databaseHandle->databaseNode->readWriteCount,databaseHandle->databaseNode->pendingReadCount,databaseHandle->databaseNode->pendingReadWriteCount
               );
#endif

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s LOCK   init: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

        // request read/write lock
        pendingReadWritesIncrement(databaseHandle);
        {
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
          Misc_initTimeout(&timeoutInfo,timeout);
#endif
          // check if there is no other reader
          if (   !isOwnReadLock(databaseHandle)
              && isReadLock(databaseHandle)
             )
          {
            // wait read end
//TODO
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
            if (timeout != WAIT_FOREVER)
            {
              do
              {
                t = MIN(Misc_getRestTimeout(&timeoutInfo),DT);
//fprintf(stderr,"%s, %d: b %ld %lu %u\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo),t);

                waitTriggerRead(databaseHandle,t);
              }
              while (   isReadLock(databaseHandle)
                     && !Misc_isTimeout(&timeoutInfo)
                    );
              if (isReadLock(databaseHandle))
              {
//fprintf(stderr,"%s, %d: stop DATABASE_LOCK_TYPE_READ_WRITE 1: wait read %d %d\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo)); asm("int3");
                Misc_doneTimeout(&timeoutInfo);
                pendingReadWritesDecrement(databaseHandle);
                return FALSE;
              }
            }
            else
            {
              // Note: do wait with timeout as a work-around for lost triggers
              do
              {
                waitTriggerRead(databaseHandle,DT);
              }
              while (isReadLock(databaseHandle));
            }
#else
            do
            {
              DATABASE_DEBUG_LOCK_ASSERT(databaseHandle,isReadLock(databaseHandle));
              if (!waitTriggerRead(databaseHandle,timeout))
              {
                pendingReadWritesDecrement(databaseHandle);
                return FALSE;
              }
            }
            while (isReadLock(databaseHandle));
#endif
          }
          DATABASE_DEBUG_LOCK_ASSERTX(databaseHandle,
                                      isOwnReadLock(databaseHandle) || !isReadLock(databaseHandle),
                                      "d isOwnReadLock=%d !isReadLock=%d",
                                      isOwnReadLock(databaseHandle),
                                      !isReadLock(databaseHandle)
                                     );

          // check if there is no other writer
          if (   !isOwnReadWriteLock(databaseHandle)
              && isReadWriteLock(databaseHandle)
             )
          {
            // wait other read/write end
//TODO
#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
            if (timeout != WAIT_FOREVER)
            {
              do
              {
                t = MIN(Misc_getRestTimeout(&timeoutInfo),DT);
//fprintf(stderr,"%s, %d: c %ld %lu %u\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo),t);

                waitTriggerReadWrite(databaseHandle,t);
              }
              while (   isReadWriteLock(databaseHandle)
                     && !Misc_isTimeout(&timeoutInfo)
                    );
              if (isReadWriteLock(databaseHandle))
              {
//fprintf(stderr,"%s, %d: stop DATABASE_LOCK_TYPE_READ_WRITE 2: wait read/write %d %d\n",__FILE__,__LINE__,timeout,Misc_getRestTimeout(&timeoutInfo)); asm("int3");
                Misc_doneTimeout(&timeoutInfo);
                pendingReadWritesDecrement(databaseHandle);
                return FALSE;
              }
            }
            else
            {
              // Note: do wait with timeout as a work-around for lost triggers
              do
              {
                waitTriggerReadWrite(databaseHandle,5*MS_PER_SECOND);
              }
              while (isReadWriteLock(databaseHandle));
            }
#else
            do
            {
              DATABASE_DEBUG_LOCK_ASSERT(databaseHandle,isReadWriteLock(databaseHandle));
              if (!waitTriggerReadWrite(databaseHandle,timeout))
              {
                Misc_doneTimeout(&timeoutInfo);
                pendingReadWritesDecrement(databaseHandle);
                return FALSE;
              }
            }
            while (isReadWriteLock(databaseHandle));
#endif
            assert(Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy));
          }
          DATABASE_DEBUG_LOCK_ASSERTX(databaseHandle,
                                      isOwnReadWriteLock(databaseHandle) || !isReadWriteLock(databaseHandle),
                                      "e isOwnReadWriteLock=%d !isReadWriteLock=%d",
                                      isOwnReadWriteLock(databaseHandle),
                                      !isReadWriteLock(databaseHandle)
                                     );

          // read/write lock aquired
          #ifdef NDEBUG
            readWritesIncrement(databaseHandle);
          #else /* not NDEBUG */
            __readWritesIncrement(__fileName__,__lineNb__,databaseHandle);
          #endif /* NDEBUG */

#ifdef DATABASE_WAIT_TRIGGER_WORK_AROUND
          Misc_doneTimeout(&timeoutInfo);
#endif
        }
        pendingReadWritesDecrement(databaseHandle);

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s LOCK   done: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

//        #ifndef NDEBUG
#if 0
          databaseHandle->debug.locked.threadId = Thread_getCurrentId();
          databaseHandle->debug.locked.fileName = __fileName__;
          databaseHandle->debug.locked.lineNb   = __lineNb__;
          databaseHandle->debug.locked.text[0]  = '\0';
          databaseHandle->debug.locked.t0       = Misc_getTimestamp();
          debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                            databaseHandle->databaseNode->debug.history,
                                            &databaseHandle->databaseNode->debug.historyIndex,
                                            SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                            DATABASE_HISTORY_TYPE_LOCK_READ_WRITE
                                           );
        #endif /* not NDEBUG */

        return TRUE;
      });
      break;
  }

  return lockedFlag;
}

#ifdef NDEBUG
  void Database_unlock(DatabaseHandle    *databaseHandle,
                       DatabaseLockTypes lockType
                      )
#else /* not NDEBUG */
  void __Database_unlock(const char        *__fileName__,
                         ulong             __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         DatabaseLockTypes lockType
                        )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

//TODO
  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_NONE:
      break;
    case DATABASE_LOCK_TYPE_READ:
      DATABASE_HANDLE_LOCKED_DO(databaseHandle,
      {
//        #ifndef xxxNDEBUG
#if 0
          databaseHandle->debug.locked.threadId = THREAD_ID_NONE;
          databaseHandle->debug.locked.fileName = NULL;
          databaseHandle->debug.locked.lineNb   = 0;
          databaseHandle->debug.locked.text[0]  = '\0';
          databaseHandle->debug.locked.t1       = Misc_getTimestamp();
          debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                            databaseHandle->databaseNode->debug.history,
                                            &databaseHandle->databaseNode->debug.historyIndex,
                                            SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                            DATABASE_HISTORY_TYPE_UNLOCK
                                           );
        #endif /* not NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK init: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

        // decrement read count
        #ifdef NDEBUG
          readsDecrement(databaseHandle);
        #else /* not NDEBUG */
          __readsDecrement(__fileName__,__lineNb__,databaseHandle);
        #endif /* NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK done: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */
#if 0
if (   (databaseHandle->databaseNode->pendingReadCount == 0)
    && (databaseHandle->databaseNode->readCount == 0)
    && (databaseHandle->databaseNode->pendingReadWriteCount == 0)
    && (databaseHandle->databaseNode->readWriteCount == 0)
   )
fprintf(stderr,"%s, %d: --------------------------------------------------------------------------------------------------\n",__FILE__,__LINE__);
fprintf(stderr,"%s, %d: %x trigger R %p %llu %d\n",__FILE__,__LINE__,Thread_getCurrentId(),&databaseHandle->databaseNode->readWriteTrigger,getCycleCounter(),databaseLock.__data.__lock);
#endif
        if (databaseHandle->databaseNode->readCount == 0)
        {
          triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ);
          triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
        }
      });
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      DATABASE_HANDLE_LOCKED_DO(databaseHandle,
      {
        assert(isReadWriteLock(databaseHandle));
        assert(!Thread_isNone(databaseHandle->databaseNode->debug.readWriteLockedBy));

#if 0
          #ifndef xxxNDEBUG
          databaseHandle->debug.locked.threadId = THREAD_ID_NONE;
          databaseHandle->debug.locked.fileName = NULL;
          databaseHandle->debug.locked.lineNb   = 0;
          databaseHandle->debug.locked.text[0]  = '\0';
          databaseHandle->debug.locked.t1       = Misc_getTimestamp();
          debugAddHistoryDatabaseThreadInfo(__fileName__,__lineNb__,
                                            databaseHandle->databaseNode->debug.history,
                                            &databaseHandle->databaseNode->debug.historyIndex,
                                            SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.history),
                                            DATABASE_HISTORY_TYPE_UNLOCK
                                           );
        #endif /* not NDEBUG */
#endif

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK init: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */

        // decrement read/write count
        #ifdef NDEBUG
          readWritesDecrement(databaseHandle);
        #else /* not NDEBUG */
          __readWritesDecrement(__fileName__,__lineNb__,databaseHandle);
        #endif /* NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK_PRINT
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK done: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %lu\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->debug.readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK_PRINT */
#if 0
if (   (databaseHandle->databaseNode->pendingReadCount == 0)
    && (databaseHandle->databaseNode->readCount == 0)
    && (databaseHandle->databaseNode->pendingReadWriteCount == 0)
    && (databaseHandle->databaseNode->readWriteCount == 0)
   )
fprintf(stderr,"%s, %d: --------------------------------------------------------------------------------------------------\n",__FILE__,__LINE__);
fprintf(stderr,"%s, %d: %x trigger RW %p %llu %d\n",__FILE__,__LINE__,Thread_getCurrentId(),&databaseHandle->databaseNode->readWriteTrigger,getCycleCounter(),databaseLock.__data.__lock);
#endif
        if (databaseHandle->databaseNode->readWriteCount == 0)
        {
          triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ);
          triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
        }
      });
      break;
  }
}

bool Database_isLockPending(DatabaseHandle     *databaseHandle,
                            SemaphoreLockTypes lockType
                           )
{
  bool pendingFlag;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_READ:
      pendingFlag = (databaseHandle->databaseNode->pendingReadCount > 0);
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      pendingFlag = (databaseHandle->databaseNode->pendingReadWriteCount > 0);
      break;
    default:
      pendingFlag = FALSE;
      break;
  }

#if 0
if (pendingFlag)
{
fprintf(stderr,"%s, %d: ----> test PENDING pending r %2d, pending rw %2d, pending transaction %2d: result %d\n",__FILE__,__LINE__,
databaseHandle->databaseNode->pendingReadCount,
databaseHandle->databaseNode->pendingReadWriteCount,
databaseHandle->databaseNode->pendingTransactionCount,
(databaseHandle->databaseNode->pendingReadCount > 0) || (databaseHandle->databaseNode->pendingReadWriteCount > 0) || (databaseHandle->databaseNode->pendingTransactionCount > 0)
);
}
#endif

  return pendingFlag;
}

Errors Database_setEnabledSync(DatabaseHandle *databaseHandle,
                               bool           enabled
                              )
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  error = ERROR_NONE;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      if (error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 DATABASE_COLUMN_TYPES(),
                                 "PRAGMA synchronous=%s",
                                 enabled ? "ON" : "OFF"
                                );
      }
      if (error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 DATABASE_COLUMN_TYPES(),
                                 "PRAGMA journal_mode=%s",
                                 enabled ? "ON" : "WAL"
                                );
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
// TODO: required? how to do?
      break;
  }
  if (error == ERROR_NONE)
  {
    databaseHandle->enabledSync = enabled;
  }

  return error;
}

Errors Database_setEnabledForeignKeys(DatabaseHandle *databaseHandle,
                                      bool           enabled
                                     )
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  error = ERROR_NONE;
  switch (databaseHandle->databaseNode->databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMN_TYPES(),
                               "PRAGMA foreign_keys=%s",
                               enabled ? "ON" : "OFF"
                              );
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 DATABASE_COLUMN_TYPES(),
                                 "SET FOREIGN_KEY_CHECKS=%d",
                                 enabled ? 1 : 0
                                );
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }
  if (error == ERROR_NONE)
  {
    databaseHandle->enabledForeignKeys = enabled;
  }

  return error;
}

Errors Database_setTmpDirectory(DatabaseHandle *databaseHandle,
                                const char     *directoryName
                               )
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  error = ERROR_NONE;
  switch (databaseHandle->databaseNode->databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMN_TYPES(),
                               "PRAGMA temp_store_directory='%s'",
                               directoryName
                              );
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        error = ERROR_NONE;  // not supported; ignored
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return error;
}

Errors Database_createTemporaryTable(DatabaseHandle            *databaseHandle,
                                     DatabaseTemporaryTableIds id,
                                     const char                *definition
                                    )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  return Database_execute(databaseHandle,
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          DATABASE_FLAG_NONE,
                          DATABASE_COLUMN_TYPES(),
                          "CREATE TABLE %s.%s \
                           ( \
                             id INT PRIMARY KEY, \
                             %s \
                           ) \
                          ",
                          DATABASE_AUX,
                          TEMPORARY_TABLE_NAMES[id],
                          definition
                         );
}

Errors Database_dropTemporaryTable(DatabaseHandle            *databaseHandle,
                                   DatabaseTemporaryTableIds id
                                  )
{
  return Database_execute(databaseHandle,
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          DATABASE_FLAG_NONE,
                          DATABASE_COLUMN_TYPES(),
                          "DROP TABLE %s.%s",
                          DATABASE_AUX,
                          TEMPORARY_TABLE_NAMES[id]
                         );
}

Errors Database_dropTables(DatabaseHandle *databaseHandle)
{
  StringList         tableNameList;
  Errors             error;
  StringListIterator iteratorTableName;
  String             tableName;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle);
  if ((error == ERROR_NONE) && !StringList_isEmpty(&tableNameList))
  {
    bool savedEnabledForeignKeys;

    savedEnabledForeignKeys = databaseHandle->enabledForeignKeys;
    Database_setEnabledForeignKeys(databaseHandle,FALSE);

    STRINGLIST_ITERATEX(&tableNameList,iteratorTableName,tableName,error == ERROR_NONE)
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMN_TYPES(),
                               "DROP TABLE %s",
                               String_cString(tableName)
                              );
    }

    Database_setEnabledForeignKeys(databaseHandle,savedEnabledForeignKeys);
  }

  return error;
}

Errors Database_dropViews(DatabaseHandle *databaseHandle)
{
  StringList         viewNameList;
  Errors             error;
  StringListIterator iteratorViewName;
  String             viewName;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(&viewNameList);
  error = Database_getViewList(&viewNameList,databaseHandle);
  STRINGLIST_ITERATEX(&viewNameList,iteratorViewName,viewName,error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMN_TYPES(),
                             "DROP VIEW %s",
                             String_cString(viewName)
                            );
  }
  StringList_done(&viewNameList);

  return error;
}

Errors Database_dropIndices(DatabaseHandle *databaseHandle)
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  error = ERROR_NONE;
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      {
        StringList         indexNameList;
        StringListIterator iteratorIndexName;
        String             indexName;

        StringList_init(&indexNameList);
        error = Database_getIndexList(&indexNameList,databaseHandle,NULL);
        STRINGLIST_ITERATEX(&indexNameList,iteratorIndexName,indexName,error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMN_TYPES(),
                                   "DROP INDEX %s",
                                   String_cString(indexName)
                                  );
        }
        StringList_done(&indexNameList);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        {
          StringList         tableNameList,indexNameList;
          StringListIterator iteratorTableName,iteratorIndexName;
          String             tableName,indexName;

          StringList_init(&tableNameList);
          StringList_init(&indexNameList);
          error = Database_getTableList(&tableNameList,databaseHandle);
          STRINGLIST_ITERATEX(&indexNameList,iteratorTableName,tableName,error == ERROR_NONE)
          {
            error = Database_getIndexList(&indexNameList,databaseHandle,String_cString(tableName));
            STRINGLIST_ITERATEX(&indexNameList,iteratorIndexName,indexName,error == ERROR_NONE)
            {
              error = Database_execute(databaseHandle,
                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                       NULL,  // changedRowCount
                                       DATABASE_FLAG_NONE,
                                       DATABASE_COLUMN_TYPES(),
                                       "DROP INDEXES %s FROM %s",
                                       String_cString(indexName),
                                       String_cString(tableName)
                                      );
            }
          }
          StringList_done(&indexNameList);
          StringList_done(&tableNameList);
        }
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return error;
}

Errors Database_dropTriggers(DatabaseHandle *databaseHandle)
{
  StringList         triggerNameList;
  Errors             error;
  StringListIterator iteratorTriggerName;
  String             triggerName;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(&triggerNameList);
  error = Database_getTriggerList(&triggerNameList,databaseHandle);
  STRINGLIST_ITERATEX(&triggerNameList,iteratorTriggerName,triggerName,error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMN_TYPES(),
                             "DROP TRIGGER %s",
                             String_cString(triggerName)
                            );
  }
  if (error != ERROR_NONE)
  {
    StringList_done(&triggerNameList);
    return error;
  }
  StringList_done(&triggerNameList);

  return ERROR_NONE;
}

Errors Database_compare(DatabaseHandle     *referenceDatabaseHandle,
                        DatabaseHandle     *databaseHandle,
                        const char * const tableNames[],
                        uint               tableNameCount,
                        uint               compareFlags
                       )
{
  Errors             error;
  StringList         referenceTableNameList,tableNameList;
  StringListIterator stringListIterator;
  String             tableName;
  DatabaseColumnName referenceColumnNames[DATABASE_MAX_TABLE_COLUMNS],compareColumnNames[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseDataTypes  referenceColumnTypes[DATABASE_MAX_TABLE_COLUMNS],compareColumnTypes[DATABASE_MAX_TABLE_COLUMNS];
  uint               referenceColumnCount,compareColumnCount;
  uint               i,j;

  assert(referenceDatabaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(referenceDatabaseHandle);
//TODO: remove
assert(Thread_isCurrentThread(databaseHandle->debug.threadId));
  assert(checkDatabaseInitialized(referenceDatabaseHandle));
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));

  // get table lists
  StringList_init(&referenceTableNameList);
  error = Database_getTableList(&referenceTableNameList,referenceDatabaseHandle);
  if (error != ERROR_NONE)
  {
    StringList_done(&referenceTableNameList);
    return error;
  }
  if ((compareFlags & DATABASE_COMPARE_FLAG_INCLUDE_VIEWS) != 0)
  {
    error = Database_getViewList(&referenceTableNameList,referenceDatabaseHandle);
    if (error != ERROR_NONE)
    {
      StringList_done(&referenceTableNameList);
      return error;
    }
  }

  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle);
  if (error != ERROR_NONE)
  {
    StringList_done(&tableNameList);
    StringList_done(&referenceTableNameList);
    return error;
  }
  if ((compareFlags & DATABASE_COMPARE_FLAG_INCLUDE_VIEWS) != 0)
  {
    error = Database_getViewList(&tableNameList,databaseHandle);
    if (error != ERROR_NONE)
    {
      StringList_done(&tableNameList);
      StringList_done(&referenceTableNameList);
      return error;
    }
  }

  // compare tables
  STRINGLIST_ITERATEX(&referenceTableNameList,stringListIterator,tableName,error == ERROR_NONE)
  {
    if (   (tableNames == NULL)
        || ARRAY_CONTAINS(tableNames,tableNameCount,i,String_equalsCString(tableName,tableNames[i]))
       )
    {
      if (StringList_contains(&tableNameList,tableName))
      {
        // get column lists
        error = getTableColumns(referenceColumnNames,
                                referenceColumnTypes,
                                &referenceColumnCount,
                                DATABASE_MAX_TABLE_COLUMNS,
                                referenceDatabaseHandle,
                                String_cString(tableName)
                               );
        if (error != ERROR_NONE)
        {
          break;
        }
        error = getTableColumns(compareColumnNames,
                                compareColumnTypes,
                                &compareColumnCount,
                                DATABASE_MAX_TABLE_COLUMNS,
                                databaseHandle,
                                String_cString(tableName)
                               );
        if (error != ERROR_NONE)
        {
          break;
        }

        // compare columns
//  fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__); asm("int3");
        for (i = 0; i < referenceColumnCount; i++)
        {
          // find column
          j = ARRAY_FIND(compareColumnNames,compareColumnCount,j,stringEquals(referenceColumnNames[i],compareColumnNames[j]));
          if (j < compareColumnCount)
          {
            if (   (referenceColumnTypes[j] != DATABASE_DATATYPE_NONE)
                && !areCompatibleTypes(referenceColumnTypes[i],compareColumnTypes[j])
               )
            {
              error = ERRORX_(DATABASE_TYPE_MISMATCH,0,"%s in %s: %d != %d",referenceColumnNames[i],String_cString(tableName),referenceColumnTypes[i],compareColumnTypes[j]);
            }
          }
          else
          {
            error = ERRORX_(DATABASE_MISSING_COLUMN,0,"%s in %s",referenceColumnNames[i],String_cString(tableName));
          }
        }
        if (error != ERROR_NONE)
        {
          break;
        }

        // check for obsolete columns
        for (i = 0; i < compareColumnCount; i++)
        {
          // find column
          j = ARRAY_FIND(referenceColumns,referenceColumnCount,j,stringEquals(compareColumnNames[i],referenceColumnNames[j]));
          if (j >= referenceColumnCount)
          {
            error = ERRORX_(DATABASE_OBSOLETE_COLUMN,0,"%s in %s",referenceColumnNames[j],String_cString(tableName));
          }
        }
        if (error != ERROR_NONE)
        {
          break;
        }

        // free resources
      }
      else
      {
        error = ERRORX_(DATABASE_MISSING_TABLE,0,"%s",String_cString(tableName));
      }
    }
  }

  // check for obsolete tables
  STRINGLIST_ITERATEX(&tableNameList,stringListIterator,tableName,error == ERROR_NONE)
  {
    if (!StringList_contains(&referenceTableNameList,tableName))
    {
      error = ERRORX_(DATABASE_OBSOLETE_TABLE,0,"%s",String_cString(tableName));
    }
  }

  // free resources
  StringList_done(&tableNameList);
  StringList_done(&referenceTableNameList);

  return error;
}

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
                         )
{
  /* mappings:
   *
   * [id|a|b| | | | ] from table
   *                  ^
   *                  | toColumnMap
   *                  |
   * [id|b|a| | | | ] to table
   *                  ^
   *                  | parameterMap
   *                  |
   * [b|a| | | | ]    insert statement (with pimary key)
   */

  #define START_TIMER() \
    do \
    { \
      t = Misc_getTimestamp(); \
    } \
    while (0)
  #define END_TIMER() \
    do \
    { \
      if (duration != NULL) \
      { \
        (*duration) += (Misc_getTimestamp()-t)/US_PER_MS; \
      } \
    } \
    while (0)

  const int UNUSED = -1;

  Errors                  error;
  uint64                  t;

  DatabaseColumnName      fromColumnNames[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseDataTypes       fromColumnTypes[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseColumnName      toColumnNames[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseDataTypes       toColumnTypes[DATABASE_MAX_TABLE_COLUMNS];
  uint                    fromColumnCount,toColumnCount;
  DatabaseColumn          fromColumns[DATABASE_MAX_TABLE_COLUMNS];

  uint                    toColumnMap[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseColumnName      toColumnMapNames[DATABASE_MAX_TABLE_COLUMNS];
  uint                    toColumnMapCount;
  uint                    parameterMap[DATABASE_MAX_TABLE_COLUMNS];
  uint                    parameterMapCount;
  int                     toColumnPrimaryKeyIndex;
  DatabaseValue           toValues[DATABASE_MAX_TABLE_COLUMNS];
  uint                    toValueCount;
  DatabaseValue           parameterValues[DATABASE_MAX_TABLE_COLUMNS];
  uint                    parameterValueCount;

  uint                    i,j;
  uint                    n;
  String                  sqlSelectString,sqlInsertString;

  DatabaseColumnInfo      fromColumnInfo,toColumnInfo;

  DatabaseStatementHandle fromDatabaseStatementHandle,toDatabaseStatementHandle;
  va_list                 arguments;
  DatabaseId              lastRowId;
  #ifdef DATABASE_DEBUG_COPY_TABLE
    uint64 t0,t1;
    ulong  rowCount;
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  assert(fromDatabaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromDatabaseHandle);
// TODO: remove
assert(Thread_isCurrentThread(fromDatabaseHandle->debug.threadId));
  assert(checkDatabaseInitialized(fromDatabaseHandle));
  assert(toDatabaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(toDatabaseHandle);
// TODO: remove
assert(Thread_isCurrentThread(toDatabaseHandle->debug.threadId));
  assert(checkDatabaseInitialized(toDatabaseHandle));
  assert(fromTableName != NULL);
  assert(toTableName != NULL);

  #ifdef DATABASE_DEBUG_COPY_TABLE
    t0 = Misc_getTimestamp();
    rowCount = 0;
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  // get table columns
  START_TIMER();
  error = getTableColumns(fromColumnNames,
                          fromColumnTypes,
                          &fromColumnCount,
                          DATABASE_MAX_TABLE_COLUMNS,
                          fromDatabaseHandle,
                          fromTableName
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s:%d: fromTableName=%s fromColumns=",__FILE__,__LINE__,fromTableName); for (int i = 0; i < fromColumnCount;i++) fprintf(stderr,"%s %s, ",fromColumnNames[i],DATABASE_DATATYPE_NAMES[fromColumnTypes[i]]); fprintf(stderr,"\n");

  error = getTableColumns(toColumnNames,
                          toColumnTypes,
                          &toColumnCount,
                          DATABASE_MAX_TABLE_COLUMNS,
                          toDatabaseHandle,
                          toTableName
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s:%d: toTableName=%s toColumns=",__FILE__,__LINE__,toTableName); for (int i = 0; i < toColumnCount;i++) fprintf(stderr,"%s %s, ",toColumnNames[i],DATABASE_DATATYPE_NAMES[toColumnTypes[i]]); fprintf(stderr,"\n");
  END_TIMER();

  // get column mapping: toColumn[toColumnMap[i]] -> fromColumn[i]
  toColumnMapCount = 0;
  for (i = 0; i < toColumnCount; i++)
  {
    j = ARRAY_FIND(fromColumnNames,fromColumnCount,j,stringEquals(toColumnNames[i],fromColumnNames[j]));
    if (j < fromColumnCount)
    {
      toColumnMap[toColumnMapCount] = j;
      stringSet(toColumnMapNames[toColumnMapCount],sizeof(toColumnMapNames[toColumnMapCount]),toColumnNames[i]);
      toColumnMapCount++;
    }
  }
//fprintf(stderr,"%s:%d: mapping %d %s -> %s: ",__FILE__,__LINE__, toColumnMapCount,fromTableName,toTableName); for (int i = 0; i < toColumnMapCount;i++) { fprintf(stderr,"%d->%d, ",toColumnMap[i],i); } fprintf(stderr,"\n");

  // get parameter mapping/to-table primary key column index
  toColumnPrimaryKeyIndex = UNUSED;
  parameterMapCount = 0;
  for (i = 0; i < toColumnCount; i++)
  {
    if (toColumnTypes[i] != DATABASE_DATATYPE_PRIMARY_KEY)
    {
      parameterMap[parameterMapCount] = i;
      parameterMapCount++;
    }
    else
    {
      toColumnPrimaryKeyIndex = i;
    }
  }
//fprintf(stderr,"%s:%d: parameter %d %s -> %s: ",__FILE__,__LINE__,parameterMapCount,fromTableName,toTableName); for (int i = 0; i < parameterMapCount;i++) { fprintf(stderr,"%d->%d: %s %d, ",parameterMap[i],i,toColumnNames[parameterMap[i]],toColumnTypes[parameterMap[i]]); } fprintf(stderr,"\n");

  // init from/to values
  for (i = 0; i < fromColumnCount; i++)
  {
    fromColumns[i].name = fromColumnNames[i];
    fromColumns[i].type = fromColumnTypes[i];
  }
  for (i = 0; i < toColumnCount; i++)
  {
    toValues[i].type = toColumnTypes[i];
  }
  toValueCount = toColumnCount;

  for (i = 0; i < parameterMapCount; i++)
  {
    parameterValues[i].type = toColumnTypes[parameterMap[i]];
  }
  parameterValueCount = parameterMapCount;

  // create SQL select statement strings
  sqlSelectString = String_format(String_new(),"SELECT ");
  for (i = 0; i < fromColumnCount; i++)
  {
    if (i > 0) String_appendChar(sqlSelectString,',');
    String_appendCString(sqlSelectString,fromColumnNames[i]);
  }
  String_formatAppend(sqlSelectString," FROM %s",fromTableName);
  if (fromAdditional != NULL)
  {
    String_appendChar(sqlSelectString,' ');
    va_start(arguments,fromAdditional);
    vformatSQLString(sqlSelectString,
                     fromAdditional,
                     arguments
                    );
    va_end(arguments);
  }
  DATABASE_DEBUG_SQL(fromDatabaseHandle,sqlSelectString);
//fprintf(stderr,"%s:%d: sqlSelectString=%s\n",__FILE__,__LINE__,String_cString(sqlSelectString));

  sqlInsertString = String_format(String_new(),"INSERT INTO %s (",toTableName);
  for (i = 0; i < parameterMapCount; i++)
  {
    if (i > 0) String_appendChar(sqlInsertString,',');
    String_appendCString(sqlInsertString,fromColumnNames[toColumnMap[parameterMap[i]]]);
  }
  String_formatAppend(sqlInsertString,") VALUES (");
  for (i = 0; i < parameterMapCount; i++)
  {
    if (i > 0) String_appendChar(sqlInsertString,',');
    String_appendChar(sqlInsertString,'?');
  }
  String_formatAppend(sqlInsertString,")");
  DATABASE_DEBUG_SQL(fromDatabaseHandle,sqlInsertString);
//fprintf(stderr,"%s:%d: sqlInsertString=%s\n",__FILE__,__LINE__,String_cString(sqlInsertString));

  // create select+insert statements
  error = prepareStatement(&fromDatabaseStatementHandle,
                           fromDatabaseHandle,
                           String_cString(sqlSelectString)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s:%d: bind from results %d\n",__FILE__,__LINE__,fromColumnCount);
  error = bindResults(&fromDatabaseStatementHandle,fromColumns,fromColumnCount);
  if (error != ERROR_NONE)
  {
    finalizeStatement(&fromDatabaseStatementHandle);
    return error;
  }
  fromColumnInfo.names  = fromColumnNames;
  fromColumnInfo.values = fromDatabaseStatementHandle.results;
  fromColumnInfo.count  = fromColumnCount;

  error = prepareStatement(&toDatabaseStatementHandle,
                           toDatabaseHandle,
                           String_cString(sqlInsertString)
                          );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&fromDatabaseStatementHandle);
    return error;
  }
  toColumnInfo.names  = toColumnMapNames;
  toColumnInfo.values = toValues;
  toColumnInfo.count  = toValueCount;

// TODO: for progress
{
  uint64 nn;
  Database_getUInt64(fromDatabaseHandle,&nn,fromTableName,"COUNT(*)",DATABASE_FILTERS_NONE,NULL);
UNUSED_VARIABLE(nn);
//fprintf(stderr,"%s:%d: %llu\n",__FILE__,__LINE__,nn);
}

  // select rows in from-table and copy to to-table
  BLOCK_DOX(error,
            { begin(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ,WAIT_FOREVER);
              begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
            },
            { end(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);
              end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
            },
  {
    Errors error;

    START_TIMER();

    // begin transaction
    if (transactionFlag)
    {
      error = Database_beginTransaction(toDatabaseHandle,DATABASE_TRANSACTION_TYPE_DEFERRED,WAIT_FOREVER);
      if (error != ERROR_NONE)
      {
        finalizeStatement(&toDatabaseStatementHandle);
        finalizeStatement(&fromDatabaseStatementHandle);
        return error;
      }
    }

    // copy rows
    n = 0;
    while (getNextRow(&fromDatabaseStatementHandle,fromDatabaseHandle->timeout))
    {
//fprintf(stderr,"%s:%d: a\n",__FILE__,__LINE__); dumpStatementHandle(&fromDatabaseStatementHandle);
      #ifdef DATABASE_DEBUG_COPY_TABLE
        rowCount++;
      #endif /* DATABASE_DEBUG_COPY_TABLE */

      // set to values
      for (i = 0; i < parameterMapCount; i++)
      {
        memCopyFast(&parameterValues[i].data,
                    sizeof(parameterValues[i].data),
                    &fromDatabaseStatementHandle.results[parameterMap[toColumnMap[i]]].data,
                    sizeof(fromDatabaseStatementHandle.results[parameterMap[toColumnMap[i]]].data)
                   );
#if 0
fprintf(stderr,"%s:%d: index: f=%d->t=%d->p=%d name: f=%s->t=%s types: f=%s->t=%s values: f=%s->t=%s\n",__FILE__,__LINE__,
(i < parameterMapCount) ? toColumnMap[parameterMap[i]] : -1,
(i < parameterMapCount) ? parameterMap[i] : -1,
i,
fromColumnNames[toColumnMap[parameterMap[i]]],
toColumnNames[parameterMap[i]],
DATABASE_DATATYPE_NAMES[fromColumnTypes[toColumnMap[parameterMap[i]]]],
DATABASE_DATATYPE_NAMES[toColumnTypes[parameterMap[i]]],
debugDatabaseValueToString(buffer1,sizeof(buffer1),&fromValues[toColumnMap[parameterMap[i]]]),
debugDatabaseValueToString(buffer2,sizeof(buffer2),&toValues[parameterMap[i]])
);
#endif
      }

      for (i = 0; i < toColumnMapCount; i++)
      {
        memCopyFast(&toValues[i].data,
                    sizeof(toValues[i].data),
                    &fromDatabaseStatementHandle.results[toColumnMap[i]].data,
                    sizeof(fromDatabaseStatementHandle.results[toColumnMap[i]].data)
                   );
      }

      // call pre-copy callback (if defined)
      if (preCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER),
        {
          return preCopyTableFunction(&fromColumnInfo,
                                      &toColumnInfo,
                                      preCopyTableUserData
                                     );
        });
        if (error != ERROR_NONE)
        {
          finalizeStatement(&toDatabaseStatementHandle);
          finalizeStatement(&fromDatabaseStatementHandle);
          if (transactionFlag)
          {
            (void)Database_rollbackTransaction(toDatabaseHandle);
          }
          return error;
        }

        for (i = 0; i < parameterMapCount; i++)
        {
// TODO:
//fprintf(stderr,"%s:%d: copy %d -> %d: %d\n",__FILE__,__LINE__,i,parameterMap[i],toColumnInfo.values[parameterMap[i]].type);
#if 0
          memCopyFast(&parameterValues[i].data,
                      sizeof(parameterValues[i].data),
                      &toColumnInfo.values[parameterMap[toColumnMap[i]]].data,
                      sizeof(toColumnInfo.values[parameterMap[toColumnMap[i]]].data)
                   );
#else
          memCopyFast(&parameterValues[i].data,
                      sizeof(parameterValues[i].data),
                      &toColumnInfo.values[parameterMap[i]].data,
                      sizeof(toColumnInfo.values[parameterMap[i]].data)
                   );
#endif
        }
      }

      // insert row
//fprintf(stderr,"%s:%d: bind insert parameter values %d\n",__FILE__,__LINE__,parameterValueCount);
/// TODO: implement resetBindValues()
toDatabaseStatementHandle.valueIndex=0;
      error = bindValues(&toDatabaseStatementHandle,parameterValues,parameterValueCount);
      if (error != ERROR_NONE)
      {
        finalizeStatement(&toDatabaseStatementHandle);
        finalizeStatement(&fromDatabaseStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }
//fprintf(stderr,"%s:%d: b\n",__FILE__,__LINE__); dumpStatementHandle(&toDatabaseStatementHandle);
      error = executePreparedQuery(&toDatabaseStatementHandle,
                                   NULL,  // changeRowCount
                                   toDatabaseHandle->timeout
                                  );
      if (error != ERROR_NONE)
      {
        finalizeStatement(&toDatabaseStatementHandle);
        finalizeStatement(&fromDatabaseStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }

      // get insert id
      lastRowId = getLastInsertRowId(&toDatabaseStatementHandle);
      if (toColumnPrimaryKeyIndex != UNUSED)
      {
//fprintf(stderr,"%s:%d: set id %d: %lld\n",__FILE__,__LINE__,toColumnPrimaryKeyIndex,lastRowId);
        toValues[toColumnPrimaryKeyIndex].id = lastRowId;
      }
//fprintf(stderr,"%s:%d: c\n",__FILE__,__LINE__); dumpStatementHandle(&toDatabaseStatementHandle);

      // call post-copy callback (if defined)
      if (postCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER),
        {
          return postCopyTableFunction(&fromColumnInfo,
                                       &toColumnInfo,
                                       postCopyTableUserData
                                      );
        });
        if (error != ERROR_NONE)
        {
          finalizeStatement(&toDatabaseStatementHandle);
          finalizeStatement(&fromDatabaseStatementHandle);
          if (transactionFlag)
          {
            (void)Database_rollbackTransaction(toDatabaseHandle);
          }
          return error;
        }
      }

      n++;

      // progress
      if (copyProgressCallbackFunction != NULL)
      {
        copyProgressCallbackFunction(copyProgressCallbackUserData);
      }

      // pause
      if ((copyPauseCallbackFunction != NULL) && copyPauseCallbackFunction(copyPauseCallbackUserData))
      {
        // end transaction
        if (transactionFlag)
        {
          error = Database_endTransaction(toDatabaseHandle);
          if (error != ERROR_NONE)
          {
            finalizeStatement(&toDatabaseStatementHandle);
            finalizeStatement(&fromDatabaseStatementHandle);
            return error;
          }
        }

        END_TIMER();

        // wait
        BLOCK_DO({ end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
                   end(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);
                 },
                 { begin(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ,WAIT_FOREVER);
                   begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
                 },
        {
          do
          {
            Misc_udelay(10LL*US_PER_SECOND);
          }
          while (copyPauseCallbackFunction(copyPauseCallbackUserData));
        });

        START_TIMER();

        // begin transaction
        if (transactionFlag)
        {
          error = Database_beginTransaction(toDatabaseHandle,DATABASE_TRANSACTION_TYPE_DEFERRED,WAIT_FOREVER);
          if (error != ERROR_NONE)
          {
            finalizeStatement(&toDatabaseStatementHandle);
            finalizeStatement(&fromDatabaseStatementHandle);
            return error;
          }
        }
      }

      // interrupt copy
      if (n > MAX_INTERRUPT_COPY_TABLE_COUNT)
      {
        if (   Database_isLockPending(toDatabaseHandle,DATABASE_LOCK_TYPE_READ)
            || Database_isLockPending(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE)
           )
        {
          // end transaction
          if (transactionFlag)
          {
            error = Database_endTransaction(toDatabaseHandle);
            if (error != ERROR_NONE)
            {
              finalizeStatement(&toDatabaseStatementHandle);
              finalizeStatement(&fromDatabaseStatementHandle);
              return error;
            }
          }

          END_TIMER();

          Thread_yield();

          START_TIMER();

          // begin transaction
          if (transactionFlag)
          {
            error = Database_beginTransaction(toDatabaseHandle,DATABASE_TRANSACTION_TYPE_DEFERRED,WAIT_FOREVER);
            if (error != ERROR_NONE)
            {
              finalizeStatement(&toDatabaseStatementHandle);
              finalizeStatement(&fromDatabaseStatementHandle);
              return error;
            }
          }
        }

        n = 0;
      }
    }  // while

    // end transaction
    if (transactionFlag)
    {
      error = Database_endTransaction(toDatabaseHandle);
      if (error != ERROR_NONE)
      {
        finalizeStatement(&toDatabaseStatementHandle);
        finalizeStatement(&fromDatabaseStatementHandle);
        return error;
      }
    }

    END_TIMER();

    // free resources
    finalizeStatement(&toDatabaseStatementHandle);
    finalizeStatement(&fromDatabaseStatementHandle);

    return ERROR_NONE;
  });
//fprintf(stderr,"%s, %d: -------------------------- do check\n",__FILE__,__LINE__);
//sqlite3_wal_checkpoint_v2(toDatabaseHandle->handle,NULL,SQLITE_CHECKPOINT_FULL,&a,&b);
//fprintf(stderr,"%s, %d: checkpoint a=%d b=%d r=%d: %s\n",__FILE__,__LINE__,a,b,r,sqlite3_errmsg(toDatabaseHandle->handle));

  // free resources
  String_delete(sqlInsertString);
  String_delete(sqlSelectString);

  #ifdef DATABASE_DEBUG_COPY_TABLE
    t1 = Misc_getTimestamp();
    if (rowCount > 0L)
    {
      fprintf(stderr,
              "%s, %d: DEBUG copy table %s->%s: %"PRIu64"ms, %lu rows, %lfms/row\n",
              __FILE__,__LINE__,
              fromTableName,
              toTableName,
              (t1-t0)/1000,
              rowCount,
              (double)(t1-t0)/((double)rowCount*1000L)
             );
    }
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  return error;

  #undef END_TIMER
  #undef START_TIMER
}

DatabaseId Database_getTableColumnId(DatabaseColumnInfo *columnInfo, const char *columnName, DatabaseId defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(   (databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_INT)
          );
    if ((databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY) || (databaseValue->type == DATABASE_DATATYPE_KEY))
    {
      return databaseValue->id;
    }
    else
    {
      return databaseValue->i;
    }
  }
  else
  {
    return defaultValue;
  }
}

int Database_getTableColumnInt(DatabaseColumnInfo *columnInfo, const char *columnName, int defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(   (databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_INT)
           || (databaseValue->type == DATABASE_DATATYPE_INT64)
          );
    if ((databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY) || (databaseValue->type == DATABASE_DATATYPE_KEY))
    {
      return (int)databaseValue->id;
    }
    else
    {
      return (int)databaseValue->i;
    }
  }
  else
  {
    return defaultValue;
  }
}

uint Database_getTableColumnUInt(DatabaseColumnInfo *columnInfo, const char *columnName, uint defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(   (databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_INT)
           || (databaseValue->type == DATABASE_DATATYPE_INT64)
          );
    if ((databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY) || (databaseValue->type == DATABASE_DATATYPE_KEY))
    {
      return (uint)databaseValue->id;
    }
    else
    {
      return (uint)databaseValue->i;
    }
  }
  else
  {
    return defaultValue;
  }
}

int64 Database_getTableColumnInt64(DatabaseColumnInfo *columnInfo, const char *columnName, int64 defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(   (databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_INT)
           || (databaseValue->type == DATABASE_DATATYPE_INT64)
          );
    if ((databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY) || (databaseValue->type == DATABASE_DATATYPE_KEY))
    {
      return databaseValue->id;
    }
    else
    {
      return databaseValue->i64;
    }
  }
  else
  {
    return defaultValue;
  }
}

uint64 Database_getTableColumnUInt64(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(   (databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_KEY)
           || (databaseValue->type == DATABASE_DATATYPE_INT)
           || (databaseValue->type == DATABASE_DATATYPE_INT64)
          );
    if ((databaseValue->type == DATABASE_DATATYPE_PRIMARY_KEY) || (databaseValue->type == DATABASE_DATATYPE_KEY))
    {
      return (uint64)databaseValue->id;
    }
    else
    {
      return (uint64)databaseValue->i64;
    }
  }
  else
  {
    return defaultValue;
  }
}

double Database_getTableColumnDouble(DatabaseColumnInfo *columnInfo, const char *columnName, double defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_DOUBLE);
    return databaseValue->d;
  }
  else
  {
    return defaultValue;
  }
}

uint64 Database_getTableColumnDateTime(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_DATETIME);
    return databaseValue->dateTime;
  }
  else
  {
    return defaultValue;
  }
}

String Database_getTableColumnString(DatabaseColumnInfo *columnInfo, const char *columnName, String value, const char *defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_CSTRING);
    return String_setBuffer(value,databaseValue->text.data,databaseValue->text.length);
  }
  else
  {
    return String_setCString(value,defaultValue);
  }

  return value;
}

const char *Database_getTableColumnCString(DatabaseColumnInfo *columnInfo, const char *columnName, const char *defaultValue)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_CSTRING);
    return databaseValue->text.data;
  }
  else
  {
    return defaultValue;
  }
}

void Database_getTableColumnBlob(DatabaseColumnInfo *columnInfo, const char *columnName, void *data, uint length)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

UNUSED_VARIABLE(data);
UNUSED_VARIABLE(length);
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_BLOB);
//    return columnNode->value.blob.data;
  }
  else
  {
//    return data;
  }
}

bool Database_setTableColumnId(DatabaseColumnInfo *columnInfo, const char *columnName, DatabaseId value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_INT);
    databaseValue->id = value;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnBool(DatabaseColumnInfo *columnInfo, const char *columnName, bool value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_BOOL);
    databaseValue->b = value;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnInt64(DatabaseColumnInfo *columnInfo, const char *columnName, int64 value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_INT);
    databaseValue->i64 = value;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnDouble(DatabaseColumnInfo *columnInfo, const char *columnName, double value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_DOUBLE);
    databaseValue->d = value;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnDateTime(DatabaseColumnInfo *columnInfo, const char *columnName, uint64 value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_DATETIME);
    databaseValue->dateTime = value;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnString(DatabaseColumnInfo *columnInfo, const char *columnName, ConstString value)
{
  assert(columnInfo != NULL);
  assert(columnName != NULL);

  return Database_setTableColumnCString(columnInfo,columnName,String_cString(value));
}

bool Database_setTableColumnCString(DatabaseColumnInfo *columnInfo, const char *columnName, const char *value)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_CSTRING);
    if (databaseValue->text.data != NULL) free(databaseValue->text.data);
    databaseValue->text.length = stringLength(value);
    databaseValue->text.data   = (char*)malloc(databaseValue->text.length);
    memCopyFast(databaseValue->text.data,databaseValue->text.length,value,databaseValue->text.length);
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
bool Database_setTableColumnBlob(DatabaseColumnInfo *columnInfo, const char *columnName, const void *data, uint length)
{
  DatabaseValue *databaseValue;

  assert(columnInfo != NULL);
  assert(columnName != NULL);

  databaseValue = findTableColumn(columnInfo,columnName);
  if (databaseValue != NULL)
  {
    assert(databaseValue->type == DATABASE_DATATYPE_BLOB);
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
UNUSED_VARIABLE(data);
UNUSED_VARIABLE(length);
//    columnNode->value.blob.data   = data;
//    columnNode->value.blob.length = length;
    databaseValue->blob.data   = NULL;
    databaseValue->blob.length = 0;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Database_addColumn(DatabaseHandle    *databaseHandle,
                          const char        *tableName,
                          const char        *columnName,
                          DatabaseDataTypes columnDataType
                         )
{
  const char *columnTypeString;
  Errors     error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // get column type name
  columnTypeString = NULL;
  switch (columnDataType)
  {
    case DATABASE_DATATYPE:
      columnTypeString = "";
      break;
    case DATABASE_DATATYPE_PRIMARY_KEY:
      columnTypeString = "INT PRIMARY KEY";
      break;
    case DATABASE_DATATYPE_KEY:
      columnTypeString = "INT";
      break;
    case DATABASE_DATATYPE_BOOL:
      columnTypeString = "BOOL DEFAULT FALSE";
      break;
    case DATABASE_DATATYPE_INT:
      columnTypeString = "INT DEFAULT 0";
      break;
    case DATABASE_DATATYPE_INT64:
      columnTypeString = "INT DEFAULT 0";
      break;
    case DATABASE_DATATYPE_UINT:
      columnTypeString = "INT DEFAULT 0";
      break;
    case DATABASE_DATATYPE_UINT64:
      columnTypeString = "INT DEFAULT 0";
      break;
    case DATABASE_DATATYPE_DOUBLE:
      columnTypeString = "REAL DEFAULT 0.0";
      break;
    case DATABASE_DATATYPE_DATETIME:
      columnTypeString = "DATETIME DEFAULT 0";
      break;
    case DATABASE_DATATYPE_STRING:
    case DATABASE_DATATYPE_CSTRING:
      columnTypeString = "TEXT DEFAULT ''";
      break;
    case DATABASE_DATATYPE_BLOB:
      columnTypeString = "BLOB";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* not NDEBUG */
      break; // not reached
  }

  // execute SQL command
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "ALTER TABLE %s ADD COLUMN %s %s",
                           tableName,
                           columnName,
                           columnTypeString
                          );

  return error;
}

Errors Database_removeColumn(DatabaseHandle *databaseHandle,
                             const char     *tableName,
                             const char     *columnName
                            )
{
  Errors             error;
  DatabaseColumnName columnNames[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseDataTypes  columnTypes[DATABASE_MAX_TABLE_COLUMNS];
  uint               columnCount;
  String             sqlString,value;
  uint               n;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
// TODO: remove
  assert(Thread_isCurrentThread(databaseHandle->debug.threadId));
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  // get table columns
  error = getTableColumns(columnNames,columnTypes,&columnCount,DATABASE_MAX_TABLE_COLUMNS,databaseHandle,tableName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  sqlString = String_new();
  value     = String_new();
  BLOCK_DOX(error,
            begin(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER),
            end(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
  {
    // create new table
    formatSQLString(String_clear(sqlString),"CREATE TABLE IF NOT EXISTS __new__(");
    n = 0;
    for (uint i = 0; i < columnCount; i++)
    {
      if (!stringEquals(columnNames[i],columnName))
      {
        if (n > 0) String_appendChar(sqlString,',');

        formatSQLString(sqlString,"%s %s",columnNames[i],DATABASE_DATATYPE_NAMES[columnTypes[i]]);
        n++;
      }
    }
    String_appendCString(sqlString,")");

    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    DATABASE_DOX(error,
                 ERRORX_(DATABASE_TIMEOUT,0,""),
                 databaseHandle,
                 DATABASE_LOCK_TYPE_READ_WRITE,
                 WAIT_FOREVER,
    {
      return executeQuery(databaseHandle,
                          NULL,  // changedRowCount
                          databaseHandle->timeout,
                          String_cString(sqlString)
                         );
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // copy old table -> new table
    error = Database_copyTable(databaseHandle,
                               databaseHandle,
                               tableName,
                               "__new__",
                               TRUE,  // transactionFlag
                               NULL,  // duration
                               CALLBACK_(NULL,NULL),
                               CALLBACK_(NULL,NULL),
                               CALLBACK_(NULL,NULL),
                               CALLBACK_(NULL,NULL),
                               NULL  // fromAdditional
                              );
    if (error != ERROR_NONE)
    {
      (void)Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMN_TYPES(),
                             "DROP TABLE __new__"
                            );
      return error;
    }

    return ERROR_NONE;
  });
  String_delete(value);
  String_delete(sqlString);

  // free resources

  // rename tables
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           DATABASE_FLAG_NONE,
                           "ALTER TABLE %s RENAME TO __old__",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "DROP TABLE __new__"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "ALTER TABLE __new__ RENAME TO %s",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "ALTER TABLE __old__ RENAME TO %s",
                           tableName
                          );
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "DROP TABLE __new__"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMN_TYPES(),
                           "DROP TABLE __old__"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_beginTransaction(DatabaseHandle           *databaseHandle,
                                   DatabaseTransactionTypes databaseTransactionType,
                                   long                     timeout
                                  )
#else /* not NDEBUG */
  Errors __Database_beginTransaction(const char               *__fileName__,
                                     uint                     __lineNb__,
                                     DatabaseHandle           *databaseHandle,
                                     DatabaseTransactionTypes databaseTransactionType,
                                     long                     timeout
                                    )
#endif /* NDEBUG */
{
  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    String      sqlString;
    TimeoutInfo timeoutInfo;
    Errors      error;
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(checkDatabaseInitialized(databaseHandle));

  UNUSED_VARIABLE(timeout);
  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifdef DATABASE_SUPPORT_TRANSACTIONS
#if 0
//Note: multiple transactions are excluded by read/write lock!
    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        if (databaseHandle->databaseNode->transaction.fileName != NULL)
        {
          const char *name1,*name2;

          name1 = Thread_getCurrentName();
          name2 = Thread_getName(databaseHandle->databaseNode->transaction.threadId);
          fprintf(stderr,"DEBUG ERROR: multiple transactions requested by thread '%s' (%s) at %s, %u and previously thread '%s' (%s) at %s, %u!\n",
                  (name1 != NULL) ? name1 : "none",
                  Thread_getCurrentIdString(),
                  __fileName__,
                  __lineNb__,
                  (name2 != NULL) ? name2 : "none",
                  Thread_getIdString(databaseHandle->databaseNode->transaction.threadId),
                  databaseHandle->databaseNode->transaction.fileName,
                  databaseHandle->databaseNode->transaction.lineNb
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,databaseHandle->databaseNode->transaction.stackTrace,databaseHandle->databaseNode->transaction.stackTraceSize,0);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR("begin transactions fail");
        }

        databaseHandle->databaseNode->transaction.threadId = Thread_getCurrentId();
        databaseHandle->databaseNode->transaction.fileName = __fileName__;
        databaseHandle->databaseNode->transaction.lineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          BACKTRACE(databaseHandle->databaseNode->transaction.stackTrace,databaseHandle->databaseNode->transaction.stackTraceSize);
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* not NDEBUG */
#endif

    // try to complete all read/write request (with timeout)
    if (   (   (databaseHandle->databaseNode->pendingReadCount > 0)
               || (databaseHandle->databaseNode->readCount > 0)
               || (databaseHandle->databaseNode->pendingReadWriteCount > 0)
               || (databaseHandle->databaseNode->readWriteCount > 0)
              )
        )
    {
      Misc_initTimeout(&timeoutInfo,250);
      while (   (   (databaseHandle->databaseNode->pendingReadCount > 0)
                 || (databaseHandle->databaseNode->readCount > 0)
                 || (databaseHandle->databaseNode->pendingReadWriteCount > 0)
                 || (databaseHandle->databaseNode->readWriteCount > 0)
                )
             && !Misc_isTimeout(&timeoutInfo)
            )
      {
        Thread_delay(50);
      }
      Misc_doneTimeout(&timeoutInfo);
//fprintf(stderr,"%s, %d: rest %lu\n",__FILE__,__LINE__,Misc_getRestTimeout(&timeoutInfo));
    }

    // format SQL command string
    sqlString = String_new();
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        switch (databaseTransactionType)
        {
          case DATABASE_TRANSACTION_TYPE_DEFERRED : String_format(sqlString,"BEGIN DEFERRED TRANSACTION");  break;
          case DATABASE_TRANSACTION_TYPE_IMMEDIATE: String_format(sqlString,"BEGIN IMMEDIATE TRANSACTION"); break;
          case DATABASE_TRANSACTION_TYPE_EXCLUSIVE: String_format(sqlString,"BEGIN EXCLUSIVE TRANSACTION"); break;
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          String_format(sqlString,"START TRANSACTION");
        #else /* HAVE_MARIADB */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    // lock
    #ifndef NDEBUG
      if (!__Database_lock(__fileName__,__lineNb__,databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout))
    #else /* NDEBUG */
      if (!Database_lock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,databaseHandle->timeout))
    #endif /* not NDEBUG */
    {
      String_delete(sqlString);
      return ERRORX_(DATABASE_TIMEOUT,0,"");
    }

    // begin transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = executeQuery(databaseHandle,
                         NULL,  // changedRowCount
                         databaseHandle->timeout,
                         String_cString(sqlString)
                        );
    if (error != ERROR_NONE)
    {
      Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
      String_delete(sqlString);
      return error;
    }

    // free resources
    String_delete(sqlString);

    // debug: store transaction info
    #ifdef DATABASE_USE_ATOMIC_INCREMENT
    #else /* not DATABASE_USE_ATOMIC_INCREMENT */
    #endif /* DATABASE_USE_ATOMIC_INCREMENT */
    DATABASE_HANDLE_LOCKED_DO(databaseHandle,
    {
      assert(databaseHandle->databaseNode->transactionCount == 0);
      databaseHandle->databaseNode->transactionCount++;
      #ifdef DATABASE_DEBUG_LOCK
        databaseHandle->databaseNode->transactionLPWId = Thread_getCurrentLWPId();
      #endif /* DATABASE_DEBUG_LOCK */
    });

    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->debug.transaction.threadId = Thread_getCurrentId();
        databaseHandle->databaseNode->debug.transaction.fileName = __fileName__;
        databaseHandle->databaseNode->debug.transaction.lineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          BACKTRACE(databaseHandle->databaseNode->debug.transaction.stackTrace,databaseHandle->databaseNode->debug.transaction.stackTraceSize);
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* not NDEBUG */

    #ifdef DATABASE_DEBUG_LOCK_PRINT
      fprintf(stderr,
              "%s, %d: %s TRANSACTION begin at %s %d\n",
              __FILE__,__LINE__,
              Thread_getCurrentIdString(),
              __fileName__,__lineNb__
             );
    #endif /* DATABASE_DEBUG_LOCK_PRINT */
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    UNUSED_VARIABLE(databaseHandle);
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_endTransaction(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  Errors __Database_endTransaction(const char     *__fileName__,
                                   uint           __lineNb__,
                                   DatabaseHandle *databaseHandle
                                  )
#endif /* NDEBUG */
{
  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    String sqlString;
    Errors error;
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(checkDatabaseInitialized(databaseHandle));

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    // decrement transaction count
    DATABASE_HANDLE_LOCKED_DO(databaseHandle,
    {
      assert(databaseHandle->databaseNode->transactionCount > 0);
      #ifdef DATABASE_USE_ATOMIC_INCREMENT
      #else /* not DATABASE_USE_ATOMIC_INCREMENT */
      #endif /* DATABASE_USE_ATOMIC_INCREMENT */
      if (databaseHandle->databaseNode->transactionCount <= 0) HALT_INTERNAL_ERROR("transaction count");
      databaseHandle->databaseNode->transactionCount--;
      if (databaseHandle->databaseNode->transactionCount == 0)
      {
        #ifdef DATABASE_DEBUG_LOCK
          databaseHandle->databaseNode->transactionLPWId = 0;
        #endif /* DATABASE_DEBUG_LOCK */
        triggerUnlockTransaction(databaseHandle);
//TODO
#if 0
        if      (databaseHandle->databaseNode->pendingReadCount > 0)
        {
          triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
        }
        else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
        {
          triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
        }
#else
        triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
        triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
#endif
      }
    });

    // format SQL command string
    sqlString = String_new();
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        String_format(sqlString,"END TRANSACTION");
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          String_format(sqlString,"COMMIT");
        #else /* HAVE_MARIADB */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    // end transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = executeQuery(databaseHandle,
                         NULL,  // changedRowCount
                         databaseHandle->timeout,
                         String_cString(sqlString)
                        );
    if (error != ERROR_NONE)
    {
      // Note: unlock even on error to avoid lost lock
      Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
      String_delete(sqlString);
      return error;
    }

    // unlock
    Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);

    // free resources
    String_delete(sqlString);

    // debug: clear transaction info
    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->debug.transaction.threadId = THREAD_ID_NONE;
        databaseHandle->databaseNode->debug.transaction.fileName = NULL;
        databaseHandle->databaseNode->debug.transaction.lineNb   = 0;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->debug.transaction.stackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* not NDEBUG */

    #ifdef DATABASE_DEBUG_LOCK_PRINT
      fprintf(stderr,
              "%s, %d: %s TRANSACTION end at %s %d\n",
              __FILE__,__LINE__,
              Thread_getCurrentIdString(),
              __fileName__,__lineNb__
             );
    #endif /* DATABASE_DEBUG_LOCK_PRINT */

    // post operations
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        // try to execute checkpoint
        executeCheckpoint(databaseHandle);
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
        #else /* HAVE_MARIADB */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    UNUSED_VARIABLE(databaseHandle);
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_rollbackTransaction(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  Errors __Database_rollbackTransaction(const char     *__fileName__,
                                        uint           __lineNb__,
                                        DatabaseHandle *databaseHandle
                                       )
#endif /* NDEBUG */
{
  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    String sqlString;
    Errors error;
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);
  assert(checkDatabaseInitialized(databaseHandle));

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    DATABASE_HANDLE_LOCKED_DO(databaseHandle,
    {
      assert(databaseHandle->databaseNode->transactionCount > 0);
      #ifdef DATABASE_USE_ATOMIC_INCREMENT
      #else /* not DATABASE_USE_ATOMIC_INCREMENT */
      #endif /* DATABASE_USE_ATOMIC_INCREMENT */
      if (databaseHandle->databaseNode->transactionCount <= 0) HALT_INTERNAL_ERROR("transaction count");
      databaseHandle->databaseNode->transactionCount--;
      if (databaseHandle->databaseNode->transactionCount == 0)
      {
        #ifdef DATABASE_DEBUG_LOCK
          databaseHandle->databaseNode->transactionLPWId = 0;
        #endif /* DATABASE_DEBUG_LOCK */
//fprintf(stderr,"%s, %d: trigger transaction %p %d %p\n",__FILE__,__LINE__,databaseHandle->databaseNode,databaseHandle->databaseNode->transactionCount,&databaseHandle->databaseNode->transactionTrigger);
        triggerUnlockTransaction(databaseHandle);
#if 0
        if      (databaseHandle->databaseNode->pendingReadCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger r %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readTrigger);
          triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
        }
        else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger rw %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readWriteTrigger);
          triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
        }
#else
        triggerUnlockReadWrite(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
        triggerUnlockRead(databaseHandle,DATABASE_LOCK_TYPE_READ);
#endif
      }
    });

    // format SQL command string
    sqlString = String_format(String_new(),"ROLLBACK TRANSACTION;");

    // rollback transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = executeQuery(databaseHandle,
                         NULL,  // changedRowCount
                         databaseHandle->timeout,
                         String_cString(sqlString)
                        );
    if (error != ERROR_NONE)
    {
      // Note: unlock even on error to avoid lost lock
      Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
      String_delete(sqlString);
      return error;
    }

    // unlock
    Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);

    // free resources
    String_delete(sqlString);

    // debug: clear transaction info
    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->debug.transaction.threadId = THREAD_ID_NONE;
        databaseHandle->databaseNode->debug.transaction.fileName = NULL;
        databaseHandle->databaseNode->debug.transaction.lineNb   = 0;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->debug.transaction.stackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* not NDEBUG */
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    UNUSED_VARIABLE(databaseHandle);
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  return ERROR_NONE;
}

Errors Database_flush(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      sqlite3_wal_checkpoint(databaseHandle->sqlite.handle,NULL);
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return ERROR_NONE;
}

String Database_valueToString(String string, const DatabaseValue *databaseValue)
{
  assert(databaseValue != NULL);

  switch (databaseValue->type)
  {
    case DATABASE_DATATYPE:
      String_format(string,"");
      break;
    case DATABASE_DATATYPE_PRIMARY_KEY:
    case DATABASE_DATATYPE_KEY:
      String_format(string,"%lld",databaseValue->id);
      break;
    case DATABASE_DATATYPE_BOOL:
      String_format(string,"%s",databaseValue->b ? "TRUE" : "FALSE");
      break;
    case DATABASE_DATATYPE_INT:
      String_format(string,"%lld",databaseValue->i);
      break;
    case DATABASE_DATATYPE_INT64:
      String_format(string,"%"PRIi64,databaseValue->i64);
      break;
    case DATABASE_DATATYPE_UINT:
      String_format(string,"%lld",databaseValue->i);
      break;
    case DATABASE_DATATYPE_UINT64:
      String_format(string,"%"PRIu64,databaseValue->u64);
      break;
    case DATABASE_DATATYPE_DOUBLE:
      String_format(string,"%lf",databaseValue->d);
      break;
    case DATABASE_DATATYPE_DATETIME:
      Misc_formatDateTime(string,databaseValue->dateTime,NULL);
      break;
    case DATABASE_DATATYPE_STRING:
      String_format(string,"%S",databaseValue->string);
      break;
    case DATABASE_DATATYPE_CSTRING:
      String_format(string,"%s",databaseValue->text.data);
      break;
    case DATABASE_DATATYPE_BLOB:
      String_format(string,"");
      break;
    default:
      break;
  }

  return string;
}

const char *Database_valueToCString(char *buffer, uint bufferSize, const DatabaseValue *databaseValue)
{
  assert(databaseValue != NULL);

  switch (databaseValue->type)
  {
    case DATABASE_DATATYPE:
      stringFormat(buffer,bufferSize,"");
      break;
    case DATABASE_DATATYPE_PRIMARY_KEY:
    case DATABASE_DATATYPE_KEY:
      stringFormat(buffer,bufferSize,"%lld",databaseValue->id);
      break;
    case DATABASE_DATATYPE_BOOL:
      stringFormat(buffer,bufferSize,"%s",databaseValue->b ? "TRUE" : "FALSE");
      break;
    case DATABASE_DATATYPE_INT:
      stringFormat(buffer,bufferSize,"%d",databaseValue->i);
      break;
    case DATABASE_DATATYPE_INT64:
      stringFormat(buffer,bufferSize,"%"PRIi64,databaseValue->i64);
      break;
    case DATABASE_DATATYPE_UINT:
      stringFormat(buffer,bufferSize,"%u",databaseValue->i);
      break;
    case DATABASE_DATATYPE_UINT64:
      stringFormat(buffer,bufferSize,"%"PRIu64,databaseValue->i64);
      break;
    case DATABASE_DATATYPE_DOUBLE:
      stringFormat(buffer,bufferSize,"%lf",databaseValue->d);
      break;
    case DATABASE_DATATYPE_DATETIME:
      Misc_formatDateTimeCString(buffer,bufferSize,databaseValue->dateTime,NULL);
      break;
    case DATABASE_DATATYPE_STRING:
      stringFormat(buffer,bufferSize,"%s",String_cString(databaseValue->string));
      break;
    case DATABASE_DATATYPE_CSTRING:
      stringFormat(buffer,bufferSize,"%s",databaseValue->s);
      break;
    case DATABASE_DATATYPE_BLOB:
      stringFormat(buffer,bufferSize,"");
      break;
    default:
      break;
  }

  return buffer;
}

Errors Database_execute(DatabaseHandle          *databaseHandle,
                        DatabaseRowFunction     databaseRowFunction,
                        void                    *databaseRowUserData,
                        ulong                   *changedRowCount,
                        uint                    flags,
                        const DatabaseDataTypes columnTypes[],
                        uint                    columnTypeCount,
                        const char              *command,
                        ...
                       )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(command != NULL);

  va_start(arguments,command);
  {
    DATABASE_DOX(error,
                 ERRORX_(DATABASE_TIMEOUT,0,""),
                 databaseHandle,
                 DATABASE_LOCK_TYPE_READ_WRITE,
                 databaseHandle->timeout,
    {
      return vexecuteStatement(databaseHandle,
                               databaseRowFunction,
                               databaseRowUserData,
                               changedRowCount,
                               databaseHandle->timeout,
                               flags,
                               columnTypes,
                               columnTypeCount,
                               command,
                               arguments
                              );
    });
  }
  va_end(arguments);

  return error;
}

Errors Database_vexecute(DatabaseHandle         *databaseHandle,
                         DatabaseRowFunction     databaseRowFunction,
                         void                    *databaseRowUserData,
                         ulong                   *changedRowCount,
                         uint                    flags,
                         const DatabaseDataTypes *columnTypes,
                         uint                    columnTypeCount,
                         const char              *command,
                         va_list                 arguments
                        )
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(command != NULL);

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    return vexecuteStatement(databaseHandle,
                             databaseRowFunction,
                             databaseRowUserData,
                             changedRowCount,
                             databaseHandle->timeout,
                             flags,
                             columnTypes,
                             columnTypeCount,
                             command,
                             arguments
                            );
  });

  return error;
}

#if 0
#ifdef NDEBUG
  Errors Database_prepare(DatabaseStatementHandle *databaseStatementHandle,
                          DatabaseHandle          *databaseHandle,
                          const DatabaseDataTypes *resultDataTypes,
                          uint                    resultDataTypeCount,
                          const char              *sqlCommand,
                          const DatabaseValue     values[],
                          uint                    nameValueCount,
                          const DatabaseFilter    filters[],
                          uint                    filterCount
                         )
#else /* not NDEBUG */
  Errors __Database_prepare(const char              *__fileName__,
                            ulong                   __lineNb__,
                            DatabaseStatementHandle *databaseStatementHandle,
                            DatabaseHandle          *databaseHandle,
                            const DatabaseDataTypes *resultDataTypes,
                            uint                    resultDataTypeCount,
                            const char              *sqlCommand,
                            const DatabaseValue     values[],
                            uint                    nameValueCount,
                            const DatabaseFilter    filters[],
                            uint                    filterCount
                           )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseStatementHandle != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(sqlCommand != NULL);

DatabaseColumn columns[64];
for (uint i = 0; i < resultDataTypeCount; i++) {
  columns[i].type = resultDataTypes[i];
}

  error = prepareStatement(databaseStatementHandle,
                           databaseHandle,
                           sqlCommand
                          );

  // bind values, filters, results
  if (error == ERROR_NONE)
  {
    error = bindValues(databaseStatementHandle,values,nameValueCount);
  }
  if (filters != NULL)
  {
    if (error == ERROR_NONE)
    {
      error = bindFilters(databaseStatementHandle,filters,filterCount);
    }
  }
  if (error == ERROR_NONE)
  {
    error = bindResults(databaseStatementHandle,columns,resultDataTypeCount);
  }

  if (error == ERROR_NONE)
  {
    error = executePreparedStatement(databaseStatementHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              NO_WAIT
                             );
  }

  // free resources

// TODO: debug version
#ifdef NDEBUG
#else /* not NDEBUG */
(void)__fileName__;
(void)__lineNb__;
#endif /* NDEBUG */

  return ERROR_NONE;
}
#else
#ifdef NDEBUG
  Errors Database_prepare(DatabaseStatementHandle *databaseStatementHandle,
                          DatabaseHandle          *databaseHandle,
                          const DatabaseColumn    *columns,
                          uint                    columnCount,
                          const char              *sqlCommand,
                          const DatabaseValue     values[],
                          uint                    nameValueCount,
                          const DatabaseFilter    filters[],
                          uint                    filterCount
                         )
#else /* not NDEBUG */
  Errors __Database_prepare(const char              *__fileName__,
                            ulong                   __lineNb__,
                            DatabaseStatementHandle *databaseStatementHandle,
                            DatabaseHandle          *databaseHandle,
                            const DatabaseColumn    *columns,
                            uint                    columnCount,
                            const char              *sqlCommand,
                            const DatabaseValue     values[],
                            uint                    nameValueCount,
                            const DatabaseFilter    filters[],
                            uint                    filterCount
                           )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseStatementHandle != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(sqlCommand != NULL);

  // prepare statement
  error = prepareStatement(databaseStatementHandle,
                           databaseHandle,
                           sqlCommand
                          );

  // bind values, filters, results
  if (error == ERROR_NONE)
  {
    error = bindValues(databaseStatementHandle,values,nameValueCount);
  }
  if (filters != NULL)
  {
    if (error == ERROR_NONE)
    {
      error = bindFilters(databaseStatementHandle,filters,filterCount);
    }
  }
  if (error == ERROR_NONE)
  {
    error = bindResults(databaseStatementHandle,columns,columnCount);
  }

  // execute statement
  if (error == ERROR_NONE)
  {
    error = executePreparedStatement(databaseStatementHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     NO_WAIT
                                    );
  }

  // free resources

// TODO: debug version
#ifdef NDEBUG
#else /* not NDEBUG */
(void)__fileName__;
(void)__lineNb__;
#endif /* NDEBUG */

  return ERROR_NONE;
}
#endif

bool Database_getNextRow(DatabaseStatementHandle *databaseStatementHandle,
                         ...
                        )
{
  bool    result;
  va_list arguments;
  union
  {
    bool   *b;
    int    *i;
    int64  *i64;
    uint   *u;
    uint64 *u64;
    float  *f;
    double *d;
    char   *ch;
    char   **s;
    String string;
  }       value;

  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(databaseStatementHandle->databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle->databaseHandle);
  assert(checkDatabaseInitialized(databaseStatementHandle->databaseHandle));

  result = FALSE;

  DATABASE_DEBUG_TIME_START(databaseStatementHandle);
  if (getNextRow(databaseStatementHandle,NO_WAIT))
  {
    va_start(arguments,databaseStatementHandle);
    for (uint i = 0; i < databaseStatementHandle->resultCount; i++)
    {
      switch (databaseStatementHandle->results[i].type)
      {
        case DATABASE_DATATYPE_NONE:
          break;
        case DATABASE_DATATYPE:
          break;
        case DATABASE_DATATYPE_PRIMARY_KEY:
        case DATABASE_DATATYPE_KEY:
          value.i64 = va_arg(arguments,int64*);
          if (value.i64 != NULL)
          {
            (*value.i64) = (int64)databaseStatementHandle->results[i].i64;
          }
          break;
        case DATABASE_DATATYPE_BOOL:
          value.b = va_arg(arguments,bool*);
          if (value.b != NULL)
          {
            (*value.b) = databaseStatementHandle->results[i].b;
          }
          break;
        case DATABASE_DATATYPE_INT:
          value.i = va_arg(arguments,int*);
          if (value.i != NULL)
          {
            (*value.i) = (int)databaseStatementHandle->results[i].i;
          }
          break;
        case DATABASE_DATATYPE_INT64:
          value.i64 = va_arg(arguments,int64*);
          if (value.i64 != NULL)
          {
            (*value.i64) = databaseStatementHandle->results[i].i64;
          }
          break;
        case DATABASE_DATATYPE_UINT:
          value.u = va_arg(arguments,uint*);
          if (value.u != NULL)
          {
            (*value.u) = (int)databaseStatementHandle->results[i].u;
          }
          break;
        case DATABASE_DATATYPE_UINT64:
          value.u64 = va_arg(arguments,uint64*);
          if (value.u64 != NULL)
          {
            (*value.u64) = databaseStatementHandle->results[i].u64;
          }
          break;
        case DATABASE_DATATYPE_DOUBLE:
          value.d = va_arg(arguments,double*);
          if (value.d != NULL)
          {
            (*value.d) = (ulong)databaseStatementHandle->results[i].d;
          }
          break;
        case DATABASE_DATATYPE_DATETIME:
          value.u64 = va_arg(arguments,uint64*);
          if (value.u64 != NULL)
          {
            (*value.u64) = databaseStatementHandle->results[i].dateTime;
          }
          break;
        case DATABASE_DATATYPE_STRING:
          value.string = va_arg(arguments,String);
          if (value.string != NULL)
          {
            String_setBuffer(value.string,
                             databaseStatementHandle->results[i].text.data,
                             databaseStatementHandle->results[i].text.length
                            );
          }
          break;
        case DATABASE_DATATYPE_CSTRING:
          value.s = va_arg(arguments,char**);
          if (value.s != NULL)
          {
            (*value.s) = databaseStatementHandle->results[i].text.data;
          }
          break;
        case DATABASE_DATATYPE_BLOB:
          HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }

    result = TRUE;
  }
  DATABASE_DEBUG_TIME_END(databaseStatementHandle);

  return result;
}

Errors Database_insert(DatabaseHandle      *databaseHandle,
                       ulong               *changedRowCount,
                       const char          *tableName,
                       uint                flags,
                       const DatabaseValue values[],
                       uint                valueCount
                      )
{
  String                  sqlString;
  DatabaseStatementHandle databaseStatementHandle;
  Errors                  error;

  // create SQL string
  sqlString = String_new();
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      String_setCString(sqlString,"INSERT ");
      if      (IS_SET(flags,DATABASE_FLAG_IGNORE))
      {
        String_appendCString(sqlString," OR IGNORE ");
      }
      else if (IS_SET(flags,DATABASE_FLAG_REPLACE))
      {
        String_appendCString(sqlString," OR IGNORE ");
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        if      (IS_SET(flags,DATABASE_FLAG_IGNORE))
        {
          String_setCString(sqlString,"INSERT IGNORE ");
        }
        else if (IS_SET(flags,DATABASE_FLAG_REPLACE))
        {
          String_setCString(sqlString,"REPLACE ");
        }
        else
        {
          String_setCString(sqlString,"INSERT ");
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }
  String_formatAppend(sqlString,"INTO %s (",tableName);
  for (uint i = 0; i < valueCount; i++)
  {
    if (i > 0) String_appendChar(sqlString,',');
    String_appendCString(sqlString,values[i].name);
  }
  String_appendCString(sqlString,") VALUES (");
  for (uint i = 0; i < valueCount; i++)
  {
    if (i > 0) String_appendChar(sqlString,',');
    String_formatAppend(sqlString,"%s",values[i].value);
  }
  String_appendChar(sqlString,')');
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(&databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // bind values
  error = bindValues(&databaseStatementHandle,
                     values,
                     valueCount
                    );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // execute statement
  error = executePreparedQuery(&databaseStatementHandle,
                               changedRowCount,
                               WAIT_FOREVER
                              );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // finalize statementHandle
  finalizeStatement(&databaseStatementHandle);

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

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
                             const char           *orderGroup,
                             uint64               offset,
                             uint64               limit
                            )
{
  String                  sqlString;
  DatabaseStatementHandle databaseStatementHandle;
  Errors                  error;

  assert(databaseHandle != NULL);
  assert(toColumns != NULL);
  assert(toColumnCount > 0);
  assert(tableName != NULL);
  assert(fromColumns != NULL);
  assert(fromColumnCount > 0);
  assert(toColumnCount == fromColumnCount);

  // create SQL string
  sqlString = String_new();
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      String_setCString(sqlString,"INSERT ");
      if      (IS_SET(flags,DATABASE_FLAG_IGNORE))
      {
        String_appendCString(sqlString," OR IGNORE ");
      }
      else if (IS_SET(flags,DATABASE_FLAG_REPLACE))
      {
        String_appendCString(sqlString," OR IGNORE ");
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        if      (IS_SET(flags,DATABASE_FLAG_IGNORE))
        {
          String_setCString(sqlString,"INSERT IGNORE ");
        }
        else if (IS_SET(flags,DATABASE_FLAG_REPLACE))
        {
          String_setCString(sqlString,"REPLACE ");
        }
        else
        {
          String_setCString(sqlString,"INSERT ");
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }
  String_formatAppend(sqlString,"INTO %s (",tableName);
  for (uint i = 0; i < toColumnCount; i++)
  {
    if (i > 0) String_appendChar(sqlString,',');
    String_appendCString(sqlString,toColumns[i].name);
  }
  String_appendCString(sqlString,") ");

  for (uint i = 0; i < tableNameCount; i++)
  {
    if (i > 0)
    {
      String_appendCString(sqlString," UNION SELECT ");
    }
    else
    {
      String_appendCString(sqlString,"SELECT ");
    }
    for (uint j = 0; j < fromColumnCount; j++)
    {
      if (j > 0) String_appendChar(sqlString,',');
      String_formatAppend(sqlString,"%s",fromColumns[j].name);
    }
    String_formatAppend(sqlString," FROM %s ",tableNames[i]);
    if (filter != NULL)
    {
      String_formatAppend(sqlString," WHERE %s",filter);
    }
  }
  if (orderGroup != NULL)
  {
    String_formatAppend(sqlString," %s",orderGroup);
  }
  if      (limit > 0LL)
  {
    String_formatAppend(sqlString," LIMIT %"PRIu64",%"PRIu64,offset,limit);
  }
  else if (offset > 0LL)
  {
    String_formatAppend(sqlString," LIMIT %"PRIu64",%"PRIu64,offset,DATABASE_UNLIMITED);
  }
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(&databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // bind filters
  error = bindFilters(&databaseStatementHandle,
                      filters,
                      filterCount
                     );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // execute statement
  error = executePreparedQuery(&databaseStatementHandle,
                               changedRowCount,
                               WAIT_FOREVER
                              );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // finalize statementHandle
  finalizeStatement(&databaseStatementHandle);

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_update(DatabaseHandle       *databaseHandle,
                       ulong                *changedRowCount,
                       const char           *tableName,
                       uint                 flags,
                       const DatabaseValue  values[],
                       uint                 valueCount,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      )
{
  String                  sqlString;
  DatabaseStatementHandle databaseStatementHandle;
  Errors                  error;

  assert(databaseHandle != NULL);
  assert(tableName != NULL);
  assert(values != NULL);
  assert(valueCount > 0);

  // create SQL string
  sqlString = String_newCString("UPDATE ");
  if (IS_SET(flags,DATABASE_FLAG_IGNORE))
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        String_appendCString(sqlString," OR IGNORE ");
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          String_appendCString(sqlString," IGNORE ");
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }
  }
  String_formatAppend(sqlString,"%s SET ",tableName);
  for (uint i = 0; i < valueCount; i++)
  {
    if (i > 0) String_appendChar(sqlString,',');
    String_formatAppend(sqlString,"%s=%s",values[i].name,values[i].value);
  }
  if (filter != NULL)
  {
    String_formatAppend(sqlString," WHERE %s",filter);
  }
//fprintf(stderr,"%s:%d: sqlString=%s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(&databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // bind values, filters
  error = bindValues(&databaseStatementHandle,
                     values,
                     valueCount
                    );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }
  if (filter != NULL)
  {
    error = bindFilters(&databaseStatementHandle,
                        filters,
                        filterCount
                       );
    if (error != ERROR_NONE)
    {
      finalizeStatement(&databaseStatementHandle);
      String_delete(sqlString);
      return error;
    }
  }

  // execute statement
  error = executePreparedQuery(&databaseStatementHandle,
                               changedRowCount,
                               WAIT_FOREVER
                              );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // finalize statementHandle
  finalizeStatement(&databaseStatementHandle);

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_delete(DatabaseHandle       *databaseHandle,
                       ulong                *changedRowCount,
                       const char           *tableName,
                       uint                 flags,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount,
                       uint64               limit
                      )
{
  String                  sqlString;
  DatabaseStatementHandle databaseStatementHandle;
  Errors                  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

// TODO:
(void)flags;
  // create SQL string
  sqlString = String_newCString("DELETE FROM ");
  String_appendCString(sqlString,tableName);
  if (filter != NULL)
  {
    String_formatAppend(sqlString," WHERE %s",filter);
  }
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      if (limit > 0)
      {
        String_formatAppend(sqlString," LIMIT 0,%"PRIu64,limit);
      }
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }
//fprintf(stderr,"%s:%d: sql delete: %s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(&databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  if (filter != NULL)
  {
    error = bindFilters(&databaseStatementHandle,
                        filters,
                        filterCount
                       );
    if (error != ERROR_NONE)
    {
      finalizeStatement(&databaseStatementHandle);
      String_delete(sqlString);
      return error;
    }
  }

  // execute statement
  error = executePreparedQuery(&databaseStatementHandle,
                               changedRowCount,
                               WAIT_FOREVER
                              );

  // finalize statementHandle
  finalizeStatement(&databaseStatementHandle);

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Database_finalize(DatabaseStatementHandle *databaseStatementHandle)
#else /* not NDEBUG */
  void __Database_finalize(const char        *__fileName__,
                           ulong             __lineNb__,
                           DatabaseStatementHandle *databaseStatementHandle
                          )
#endif /* NDEBUG */
{
  assert(databaseStatementHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle);
  assert(databaseStatementHandle->databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseStatementHandle->databaseHandle);
  assert(checkDatabaseInitialized(databaseStatementHandle->databaseHandle));

  #ifndef NDEBUG
    String_clear(databaseStatementHandle->databaseHandle->debug.current.sqlString);
    #ifdef HAVE_BACKTRACE
      databaseStatementHandle->databaseHandle->debug.current.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */

  #ifdef NDEBUG
    finalizeStatement(databaseStatementHandle);
  #else /* not NDEBUG */
    __finalizeStatement(__fileName__,__lineNb__,databaseStatementHandle);
  #endif /* NDEBUG */

  #ifndef NDEBUG
    DATABASE_DEBUG_TIME(databaseStatementHandle);
  #endif /* not NDEBUG */
}

Errors Database_select(DatabaseStatementHandle *databaseStatementHandle,
                       DatabaseHandle          *databaseHandle,
// TODO: use DatabaseTable
                       const char              *tableName,
                       uint                    flags,
                       DatabaseColumn          columns[],
                       uint                    columnCount,
                       const char              *filter,
                       const DatabaseFilter    filters[],
                       uint                    filterCount
// TODO: separate order/group, offset, limit
                      )
{
  String sqlString;
  Errors error;

  assert(databaseStatementHandle != NULL);
  assert(databaseHandle != NULL);
  assert(tableName != NULL);
  assert((columnCount == 0) || (columns != NULL));

  // create SQL string
  sqlString = String_newCString("SELECT ");
(void)flags;
// TODO: distinct
#if 0
  if (IS_SET(flags,DATABASE_FLAG_DISTINCT))
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        String_appendCString(sqlString,"OR IGNORE ");
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          String_appendCString(sqlString,"IGNORE ");
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }
  }
#endif

  for (uint i = 0; i < columnCount; i++)
  {
    if (i > 0) String_appendChar(sqlString,',');
    switch (columns[i].type)
    {
      case DATABASE_DATATYPE_DATETIME:
        String_formatAppend(sqlString,"UNIX_TIMESTAMP(%s)",columns[i].name);
        break;
      default:
        String_formatAppend(sqlString,"%s",columns[i].name);
        break;
    }
  }
  String_formatAppend(sqlString," FROM %s ",tableName);
  if (filter != NULL)
  {
    String_formatAppend(sqlString," WHERE %s",filter);
  }
//fprintf(stderr,"%s:%d: sqlString=%s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // bind values
  if (filter != NULL)
  {
    error = bindFilters(databaseStatementHandle,
                        filters,
                        filterCount
                       );
    if (error != ERROR_NONE)
    {
      finalizeStatement(databaseStatementHandle);
      String_delete(sqlString);
      return error;
    }
  }
  error = bindResults(databaseStatementHandle,
                      columns,
                      columnCount
                     );
  if (error != ERROR_NONE)
  {
    finalizeStatement(databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // execute statement
  error = executePreparedStatement(databaseStatementHandle,
NULL,                           //databaseRowFunction,
NULL,//                           databaseRowUserData,
NULL,  //                         changedRowCount,
                           WAIT_FOREVER
                          );
  if (error != ERROR_NONE)
  {
    finalizeStatement(databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

bool Database_existsValue(DatabaseHandle      *databaseHandle,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        )
{
  bool existsFlag;

  existsFlag = FALSE;

  return    (Database_get(databaseHandle,
                          CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                          {
                            assert(values != NULL);
                            assert(valueCount == 1);

                            UNUSED_VARIABLE(values);
                            UNUSED_VARIABLE(valueCount);
                            UNUSED_VARIABLE(userData);

                            existsFlag = TRUE;

                            return ERROR_NONE;
                          },NULL),
                          NULL,  // changedRowCount
                          DATABASE_TABLES
                          (
                            tableName
                          ),
                          DATABASE_FLAG_NONE,
                          DATABASE_COLUMNS
                          (
                            DATABASE_COLUMN_KEY   (columnName)
                          ),
                          filter,
                          filters,
                          filterCount,
                          NULL,  // orderGroup
                          0LL,
                          1LL
                         ) == ERROR_NONE)
         && existsFlag;
}

Errors Database_get(DatabaseHandle       *databaseHandle,
                    DatabaseRowFunction  databaseRowFunction,
                    void                 *databaseRowUserData,
                    ulong                *changedRowCount,
                    const char           *tableNames[],
                    uint                 tableNameCount,
                    uint                 flags,
                    DatabaseColumn       columns[],
                    uint                 columnCount,
                    const char           *filter,
                    const DatabaseFilter filters[],
                    uint                 filterCount,
                    const char           *orderGroup,
                    uint64               offset,
                    uint64               limit
                   )
{
  String                  sqlString;
  Errors                  error;
  DatabaseStatementHandle databaseStatementHandle;

  assert(databaseHandle != NULL);
  assert((columnCount == 0) || (columns != NULL));

  // create SQL string
  sqlString = String_new();
  if (!IS_SET(flags,DATABASE_FLAG_PLAIN))
  {
    for (uint i = 0; i < tableNameCount; i++)
    {
      if (i > 0)
      {
        String_appendCString(sqlString," UNION SELECT ");
      }
      else
      {
        String_appendCString(sqlString,"SELECT ");
      }
      for (uint j = 0; j < columnCount; j++)
      {
        if (j > 0) String_appendChar(sqlString,',');
        switch (columns[j].type)
        {
          case DATABASE_DATATYPE_DATETIME:
            String_formatAppend(sqlString,"UNIX_TIMESTAMP(%s)",columns[j].name);
            break;
          default:
            String_formatAppend(sqlString,"%s",columns[j].name);
            break;
        }
        if (columns[j].alias != NULL)
        {
          String_formatAppend(sqlString," AS %s",columns[j].alias);
        }
      }
      String_formatAppend(sqlString," FROM %s ",tableNames[i]);
      if (filter != NULL)
      {
        String_formatAppend(sqlString," WHERE %s",filter);
      }
    }
    if (orderGroup != NULL)
    {
      String_formatAppend(sqlString," %s",orderGroup);
    }
    if      (limit > 0LL)
    {
      String_formatAppend(sqlString," LIMIT %"PRIu64",%"PRIu64,offset,limit);
    }
    else if (offset > 0LL)
    {
      String_formatAppend(sqlString," LIMIT %"PRIu64",%"PRIu64,offset,DATABASE_UNLIMITED);
    }
  }
  else
  {
    assert(tableNameCount == 1);
    String_formatAppend(sqlString," %s",tableNames[0]);
  }
//fprintf(stderr,"%s:%d: sqlString=%s\n",__FILE__,__LINE__,String_cString(sqlString));

  // prepare statement
  error = prepareStatement(&databaseStatementHandle,
                           databaseHandle,
                           String_cString(sqlString)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // bind values
  if (filter != NULL)
  {
    error = bindFilters(&databaseStatementHandle,
                        filters,
                        filterCount
                       );
    if (error != ERROR_NONE)
    {
      finalizeStatement(&databaseStatementHandle);
      String_delete(sqlString);
      return error;
    }
  }
  error = bindResults(&databaseStatementHandle,
                      columns,
                      columnCount
                     );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // execute statement
  error = executePreparedStatement(&databaseStatementHandle,
                                   databaseRowFunction,
                                   databaseRowUserData,
                                   changedRowCount,
                                   WAIT_FOREVER
                                  );
  if (error != ERROR_NONE)
  {
    finalizeStatement(&databaseStatementHandle);
    String_delete(sqlString);
    return error;
  }

  // free resources
  finalizeStatement(&databaseStatementHandle);
  String_delete(sqlString);

  return error;
}

Errors Database_getId(DatabaseHandle       *databaseHandle,
                      DatabaseId           *value,
                      const char           *tableName,
                      const char           *columnName,
                      const char           *filter,
                      const DatabaseFilter filters[],
                      uint                 filterCount
                     )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = DATABASE_ID_NONE;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].id;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      NULL,  // orderGroup
                      0LL,
                      1LL
                     );
}

Errors Database_getIds(DatabaseHandle      *databaseHandle,
                       Array                *ids,
                       const char           *tableName,
                       const char           *columnName,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(ids != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  Array_clear(ids);

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        Array_append(ids,&values[0].id);

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      NULL,  // orderGroup
                      0,
                      DATABASE_UNLIMITED
                     );
}

Errors Database_getMaxId(DatabaseHandle       *databaseHandle,
                         DatabaseId           *value,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = DATABASE_ID_NONE;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].id;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      NULL,  // orderGroup
                      0LL,
                      1LL
                     );
}

Errors Database_getInt(DatabaseHandle       *databaseHandle,
                       int                  *value,
                       const char           *tableName,
                       const char           *columnName,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount,
                       const char           *group
                      )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = 0;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].i;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      group,
                      0LL,
                      1LL
                     );
}

Errors Database_setInt(DatabaseHandle       *databaseHandle,
                       const char           *tableName,
                       uint                 flags,
                       const char           *columnName,
                       int                  value,
                       const char           *filter,
                       const DatabaseFilter filters[],
                       uint                 filterCount
                      )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_INT(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_getUInt(DatabaseHandle       *databaseHandle,
                        uint                 *value,
                        const char           *tableName,
                        const char           *columnName,
                        const char           *filter,
                        const DatabaseFilter filters[],
                        uint                 filterCount,
                        const char           *group
                       )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = 0;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].u;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_UINT(columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      group,
                      0LL,
                      1LL
                     );
}

Errors Database_setUInt(DatabaseHandle       *databaseHandle,
                        const char           *tableName,
                        uint                 flags,
                        const char           *columnName,
                        uint                 value,
                        const char           *filter,
                        const DatabaseFilter filters[],
                        uint                 filterCount
                       )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_UINT(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_getInt64(DatabaseHandle       *databaseHandle,
                         int64                *value,
                         const char           *tableName,
                         const char           *columnName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount,
                         const char           *group
                        )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = 0LL;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].i64;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_INT64(columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      group,
                      0LL,
                      1LL
                     );
}

Errors Database_setInt64(DatabaseHandle       *databaseHandle,
                         const char           *tableName,
                         uint                 flags,
                         const char           *columnName,
                         int64                value,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_INT64(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_getUInt64(DatabaseHandle       *databaseHandle,
                          uint64               *value,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount,
                          const char           *group
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  (*value) = 0LL;

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].u64;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_UINT64(columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      group,
                      0LL,
                      1LL
                     );
}

Errors Database_setUInt64(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          uint64               value,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_UINT64(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_getDouble(DatabaseHandle       *databaseHandle,
                          double               *value,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount,
                          const char           *group
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        (*value) = values[0].id;

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      group,
                      0LL,
                      1LL
                     );
}

Errors Database_setDouble(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          double               value,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_DOUBLE(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_getString(DatabaseHandle      *databaseHandle,
                          String               string,
                          const char           *tableName,
                          const char           *columnName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  String_clear(string);

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        String_setBuffer(string,values[0].text.data,values[0].text.length);

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      NULL,  // orderGroup
                      0LL,
                      1LL
                     );
}

Errors Database_getCString(DatabaseHandle       *databaseHandle,
                           char                 *string,
                           uint                 maxStringLength,
                           const char           *tableName,
                           const char           *columnName,
                           const char           *filter,
                           const DatabaseFilter filters[],
                           uint                 filterCount
                          )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_get(databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        stringSet(string,maxStringLength,values[0].text.data);

                        return ERROR_NONE;
                      },NULL),
                      NULL,  // changedRowCount
                      DATABASE_TABLES
                      (
                        tableName
                      ),
                      DATABASE_FLAG_NONE,
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_KEY   (columnName)
                      ),
                      filter,
                      filters,
                      filterCount,
                      NULL,  // orderGroup
                      0LL,
                      1LL
                     );
}

Errors Database_setString(DatabaseHandle       *databaseHandle,
                          const char           *tableName,
                          uint                 flags,
                          const char           *columnName,
                          const String         value,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(checkDatabaseInitialized(databaseHandle));
  assert(tableName != NULL);
  assert(columnName != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         tableName,
                         flags,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE_STRING(columnName, value)
                         ),
                         filter,
                         filters,
                         filterCount
                        );
}

Errors Database_check(DatabaseHandle *databaseHandle, DatabaseChecks databaseCheck)
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  error = ERROR_UNKNOWN;

  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        switch (databaseCheck)
        {
          case DATABASE_CHECK_QUICK:
            return executeQuery(databaseHandle,
                                NULL,  // changedRowCount
                                databaseHandle->timeout,
                                "PRAGMA quick_check"
                               );
            break;
          case DATABASE_CHECK_KEYS:
            return executeQuery(databaseHandle,
                                NULL,  // changedRowCount
                                databaseHandle->timeout,
                                "PRAGMA foreign_key_check"
                               );
            break;
          case DATABASE_CHECK_FULL:
            return executeQuery(databaseHandle,
                                NULL,  // changedRowCount
                                databaseHandle->timeout,
                                "PRAGMA integrity_check"
                               );
            break;
        }
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          switch (databaseCheck)
          {
            case DATABASE_CHECK_QUICK:
              return ERROR_NONE;
              break;
            case DATABASE_CHECK_KEYS:
              return ERROR_NONE;
              break;
            case DATABASE_CHECK_FULL:
              {
                StringList         tableNameList;
                StringListIterator stringListIterator;
                ConstString        tableName;
                char               sqlCommand[256];

                // get table names
                StringList_init(&tableNameList);
                error = Database_getTableList(&tableNameList,databaseHandle);
                if (error != ERROR_NONE)
                {
                  StringList_done(&tableNameList);
                  return error;
                }

                // check tables
                STRINGLIST_ITERATEX(&tableNameList,stringListIterator,tableName,error == ERROR_NONE)
                {
                  error = executeQuery(databaseHandle,
                                       NULL,  // changedRowCount
                                       databaseHandle->timeout,
                                       stringFormat(sqlCommand,sizeof(sqlCommand),
                                                    "CHECK TABLE %s",
                                                    String_cString(tableName)
                                                   )
                                      );
                }

                // free resources
                StringList_done(&tableNameList);
              }
              break;
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    return error;
  });

  return error;
}

Errors Database_reindex(DatabaseHandle *databaseHandle)
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  error = ERROR_UNKNOWN;
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMN_TYPES(),
                               "REINDEX"
                              );
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
  }

  return error;
}

#ifdef DATABASE_DEBUG_LOCK
void Database_debugPrintSimpleLockInfo(void)
{
  const DatabaseNode *databaseNode;
  uint               i;

  // Note: debug only, no locking
  LIST_ITERATE(&databaseList,databaseNode)
  {
    switch (databaseNode->databaseSpecifier.type)
    {
      case DATABASE_TYPE_SQLITE3:
        printf("Database: 'sqlite:%s'\n",String_cString(databaseNode->databaseSpecifier.sqlite.fileName));
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
          printf("Database: 'mariadb:%s:%s'\n",String_cString(databaseNode->databaseSpecifier.mysql.serverName),String_cString(databaseNode->databaseSpecifier.mysql.userName));
        #else /* HAVE_MARIADB */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
    }

    printf("  Read locks:");
    for (i = 0; i < SIZE_OF_ARRAY(databaseNode->readLPWIds); i++)
    {
      if (databaseNode->readLPWIds[i] != 0) printf(" %u",databaseNode->readLPWIds[i]);
    }
    printf("\n");
    printf("  Read/write locks:");
    for (i = 0; i < SIZE_OF_ARRAY(databaseNode->readWriteLPWIds); i++)
    {
      if (databaseNode->readWriteLPWIds[i] != 0) printf(" %u",databaseNode->readWriteLPWIds[i]);
    }
    printf("\n");
    printf("  Transaction lock:");
    if (databaseNode->transactionLPWId != 0) printf(" %u",databaseNode->transactionLPWId);
    printf("\n");
  }
}
#endif /* DATABASE_DEBUG_LOCK */

#ifndef NDEBUG

void Database_debugEnable(DatabaseHandle *databaseHandle, bool enabled)
{
  if (enabled)
  {
    databaseDebugCounter++;

    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
/*
//TODO
        sqlite3Exec(databaseHandle->sqlite.handle,
                    "PRAGMA vdbe_trace=ON"
                   );
*/
        break;
      case DATABASE_TYPE_MYSQL:
        #if defined(HAVE_MARIADB)
        #else /* HAVE_MARIADB */
        #endif /* HAVE_MARIADB */
        break;
    }

  }
  else
  {
    assert(databaseDebugCounter>0);

    databaseDebugCounter--;
    if (databaseDebugCounter == 0)
    {
      switch (Database_getType(databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          sqlite3Exec(databaseHandle->sqlite.handle,
                      "PRAGMA vdbe_trace=OFF"
                     );
          break;
        case DATABASE_TYPE_MYSQL:
          #if defined(HAVE_MARIADB)
          #else /* HAVE_MARIADB */
          #endif /* HAVE_MARIADB */
          break;
      }
    }
  }
}

void Database_debugPrintInfo(void)
{
  const DatabaseHandle *databaseHandle;
  const DatabaseNode   *databaseNode;
  uint                 i;
  uint                 index;
  const char           *s;

  pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

  pthread_mutex_lock(&debugDatabaseLock);
  {
    pthread_mutex_lock(&debugConsoleLock);
    {
      fprintf(stderr,"Database debug info:\n");
      LIST_ITERATE(&databaseList,databaseNode)
      {
        switch (Database_getType(databaseHandle))
        {
          case DATABASE_TYPE_SQLITE3:
            fprintf(stderr,
                    "  opened 'sqlite:%s': %u\n",
                    String_cString(databaseNode->databaseSpecifier.sqlite.fileName),
                    databaseNode->openCount
                   );
            break;
          case DATABASE_TYPE_MYSQL:
            #if defined(HAVE_MARIADB)
              fprintf(stderr,
                      "  opened 'mariadb:%s:%s': %u\n",
                      String_cString(databaseNode->databaseSpecifier.mysql.serverName),
                      String_cString(databaseNode->databaseSpecifier.mysql.userName),
                      databaseNode->openCount
                     );
            #else /* HAVE_MARIADB */
            #endif /* HAVE_MARIADB */
            break;
        }
        LIST_ITERATE(&debugDatabaseHandleList,databaseHandle)
        {
          assert(databaseHandle->databaseNode != NULL);
          if (databaseHandle->databaseNode == databaseNode)
          {

            fprintf(stderr,
                    "    %s at %s, %lu\n",
                    Thread_getName(databaseHandle->debug.threadId),
                    databaseHandle->debug.fileName,
                    databaseHandle->debug.lineNb
                   );
          }
        }
        fprintf(stderr,
                "  lock state summary: pending r %2u, locked r %2u, pending rw %2u, locked rw %2u, transactions %2u\n",
                databaseNode->pendingReadCount,
                databaseNode->readCount,
                databaseNode->pendingReadWriteCount,
                databaseNode->readWriteCount,
                databaseNode->transactionCount
               );
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.pendingReads); i++)
        {
          if (!Thread_isNone(databaseNode->debug.pendingReads[i].threadId))
          {
            fprintf(stderr,
                    "    pending r  %16"PRIu64"lu thread '%s' (%s) at %s, %u\n",
                    databaseNode->debug.pendingReads[i].cycleCounter,
                    Thread_getName(databaseNode->debug.pendingReads[i].threadId),
                    Thread_getIdString(databaseNode->debug.pendingReads[i].threadId),
                    databaseNode->debug.pendingReads[i].fileName,
                    databaseNode->debug.pendingReads[i].lineNb
                   );
            #ifdef HAVE_BACKTRACE
              debugDumpStackTrace(stderr,
                                  6,
                                  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                  databaseNode->debug.pendingReads[i].stackTrace,
                                  databaseNode->debug.pendingReads[i].stackTraceSize,
                                  0
                                 );
            #endif /* HAVE_BACKTRACE */
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.reads); i++)
        {
          if (!Thread_isNone(databaseNode->debug.reads[i].threadId))
          {
            fprintf(stderr,
                    "    locked  r  %16"PRIu64" thread '%s' (%s) at %s, %u\n",
                    databaseNode->debug.reads[i].cycleCounter,
                    Thread_getName(databaseNode->debug.reads[i].threadId),
                    Thread_getIdString(databaseNode->debug.reads[i].threadId),
                    databaseNode->debug.reads[i].fileName,
                    databaseNode->debug.reads[i].lineNb
                   );
            #ifdef HAVE_BACKTRACE
              debugDumpStackTrace(stderr,
                                  6,
                                  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                  databaseNode->debug.reads[i].stackTrace,
                                  databaseNode->debug.reads[i].stackTraceSize,
                                  0
                                 );
            #endif /* HAVE_BACKTRACE */
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.pendingReadWrites); i++)
        {
          if (!Thread_isNone(databaseNode->debug.pendingReadWrites[i].threadId))
          {
            fprintf(stderr,
                    "    pending rw %16"PRIu64" thread '%s' (%s) at %s, %u\n",
                    databaseNode->debug.pendingReadWrites[i].cycleCounter,
                    Thread_getName(databaseNode->debug.pendingReadWrites[i].threadId),
                    Thread_getIdString(databaseNode->debug.pendingReadWrites[i].threadId),
                    databaseNode->debug.pendingReadWrites[i].fileName,
                    databaseNode->debug.pendingReadWrites[i].lineNb
                   );
            #ifdef HAVE_BACKTRACE
              debugDumpStackTrace(stderr,
                                  6,
                                  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                  databaseNode->debug.pendingReadWrites[i].stackTrace,
                                  databaseNode->debug.pendingReadWrites[i].stackTraceSize,
                                  0
                                 );
            #endif /* HAVE_BACKTRACE */
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.readWrites); i++)
        {
          if (!Thread_isNone(databaseNode->debug.readWrites[i].threadId))
          {
            fprintf(stderr,
                    "    locked  rw %16"PRIu64" thread '%s' (%s) at %s, %u\n",
                    databaseNode->debug.readWrites[i].cycleCounter,
                    Thread_getName(databaseNode->debug.readWrites[i].threadId),
                    Thread_getIdString(databaseNode->debug.readWrites[i].threadId),
                    databaseNode->debug.readWrites[i].fileName,
                    databaseNode->debug.readWrites[i].lineNb
                   );
            #ifdef HAVE_BACKTRACE
              debugDumpStackTrace(stderr,
                                  6,
                                  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                  databaseNode->debug.readWrites[i].stackTrace,
                                  databaseNode->debug.readWrites[i].stackTraceSize,
                                  0
                                 );
            #endif /* HAVE_BACKTRACE */
          }
        }
        if (!Thread_isNone(databaseNode->debug.transaction.threadId))
        {
          fprintf(stderr,
                  "  transaction: thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseNode->debug.transaction.threadId),
                  Thread_getIdString(databaseNode->debug.transaction.threadId),
                  databaseNode->debug.transaction.fileName,
                  databaseNode->debug.transaction.lineNb
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,
                                4,
                                DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                databaseNode->debug.transaction.stackTrace,
                                databaseNode->debug.transaction.stackTraceSize,
                                0
                               );
          #endif /* HAVE_BACKTRACE */
        }
        else
        {
          fprintf(stderr,
                  "  transaction: none\n"
                 );
        }
        if (!Thread_isNone(databaseNode->debug.lastTrigger.threadInfo.threadId))
        {
          s = "-";
          switch (databaseNode->debug.lastTrigger.lockType)
          {
            case DATABASE_LOCK_TYPE_NONE      : s = "- "; break;
            case DATABASE_LOCK_TYPE_READ      : s = "R "; break;
            case DATABASE_LOCK_TYPE_READ_WRITE: s = "RW"; break;
          }
          fprintf(stderr,
                  "  last trigger %s %16"PRIu64" thread '%s' (%s) at %s, %u\n",
                  s,
                  databaseNode->debug.lastTrigger.threadInfo.cycleCounter,
                  Thread_getName(databaseNode->debug.lastTrigger.threadInfo.threadId),
                  Thread_getIdString(databaseNode->debug.lastTrigger.threadInfo.threadId),
                  databaseNode->debug.lastTrigger.threadInfo.fileName,
                  databaseNode->debug.lastTrigger.threadInfo.lineNb
                 );
          fprintf(stderr,
                  "                pending r %2u, locked r %2u, pending rw %2u, locked rw %2u, transactions %2u\n",
databaseNode->debug.lastTrigger.pendingReadCount,
databaseNode->debug.lastTrigger.readCount              ,
databaseNode->debug.lastTrigger.pendingReadWriteCount  ,
databaseNode->debug.lastTrigger.readWriteCount         ,
databaseNode->debug.lastTrigger.transactionCount
                 );
          #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,
                              4,
                              DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                              databaseNode->debug.lastTrigger.stackTrace,
                              databaseNode->debug.lastTrigger.stackTraceSize,
                              0
                             );
          #endif /* HAVE_BACKTRACE */
        }
        else
        {
          fprintf(stderr,
                  "  transaction: none\n"
                 );
        }
        fprintf(stderr,"\n");
        fprintf(stderr,
                "  lock history (ascending):\n"
               );
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->debug.history); i++)
        {
          index = (databaseNode->debug.historyIndex+i) % SIZE_OF_ARRAY(databaseNode->debug.history);

          if (!Thread_isNone(databaseNode->debug.history[index].threadId))
          {
            switch (databaseNode->debug.history[index].type)
            {
              case DATABASE_HISTORY_TYPE_LOCK_READ:       s = "locked read"; break;
              case DATABASE_HISTORY_TYPE_LOCK_READ_WRITE: s = "locked read/write"; break;
              case DATABASE_HISTORY_TYPE_UNLOCK:          s = "unlocked"; break;
            }
            fprintf(stderr,
                    "    %-18s %16"PRIu64" thread '%s' (%s) at %s, %u\n",
                    s,
                    databaseNode->debug.history[index].cycleCounter,
                    Thread_getName(databaseNode->debug.history[index].threadId),
                    Thread_getIdString(databaseNode->debug.history[index].threadId),
                    databaseNode->debug.history[index].fileName,
                    databaseNode->debug.history[index].lineNb
                   );
// optional
#if 0
            #ifdef HAVE_BACKTRACE
              debugDumpStackTrace(stderr,
                                  6,
                                  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                  databaseNode->debug.history[index].stackTrace,
                                  databaseNode->debug.history[index].stackTraceSize,
                                  0
                                 );
            #endif /* HAVE_BACKTRACE */
#endif
          }
        }
        fprintf(stderr,"\n");
      }
      fprintf(stderr,"\n");
    }
    pthread_mutex_unlock(&debugConsoleLock);
  }
  pthread_mutex_unlock(&debugDatabaseLock);
}

void Database_debugPrintLockInfo(const DatabaseHandle *databaseHandle)
{
  uint i;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle->databaseNode);

  pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

//TODO: use debugPrintLockInfo()?
  pthread_mutex_lock(&debugDatabaseLock);
  {
    pthread_mutex_lock(&debugConsoleLock);
    {
      switch (Database_getType(databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          fprintf(stderr,
                  "Database lock info 'sqlite:%s':\n",
                  String_cString(databaseHandle->databaseNode->databaseSpecifier.sqlite.fileName)
                 );
          break;
        case DATABASE_TYPE_MYSQL:
          #if defined(HAVE_MARIADB)
            fprintf(stderr,
                    "Database lock info 'mariadb:%s:%s':\n",
                    String_cString(databaseHandle->databaseNode->databaseSpecifier.mysql.serverName),
                    String_cString(databaseHandle->databaseNode->databaseSpecifier.mysql.userName)
                   );
          #else /* HAVE_MARIADB */
          #endif /* HAVE_MARIADB */
          break;
      }
      fprintf(stderr,
              "  lock state summary: pending r %2u, locked r %2u, pending rw %2u, locked rw %2u, transactions %2u\n",
              databaseHandle->databaseNode->pendingReadCount,
              databaseHandle->databaseNode->readCount,
              databaseHandle->databaseNode->pendingReadWriteCount,
              databaseHandle->databaseNode->readWriteCount,
              databaseHandle->databaseNode->transactionCount
             );
      for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.reads); i++)
      {
        if (!Thread_isNone(databaseHandle->databaseNode->debug.reads[i].threadId))
        {
          fprintf(stderr,
                  "    locked  r  thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseHandle->databaseNode->debug.reads[i].threadId),
                  Thread_getIdString(databaseHandle->databaseNode->debug.reads[i].threadId),
                  databaseHandle->databaseNode->debug.reads[i].fileName,
                  databaseHandle->databaseNode->debug.reads[i].lineNb
                 );
          fprintf(stderr,
                  "    command: %s\n",
                  String_cString(databaseHandle->debug.current.sqlString)
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,
                                4,
                                DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                databaseHandle->debug.current.stackTrace,
                                databaseHandle->debug.current.stackTraceSize,
                                0
                               );
          #endif /* HAVE_BACKTRACE */
//          debugDumpStackTrace(stderr,6,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,databaseHandle->databaseNode->reads[i].stackTrace,databaseHandle->databaseNode->reads[i].stackTraceSize,0);
        }
      }
      for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->debug.readWrites); i++)
      {
        if (!Thread_isNone(databaseHandle->databaseNode->debug.readWrites[i].threadId))
        {
          fprintf(stderr,
                  "    locked  rw thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseHandle->databaseNode->debug.readWrites[i].threadId),
                  Thread_getIdString(databaseHandle->databaseNode->debug.readWrites[i].threadId),
                  databaseHandle->databaseNode->debug.readWrites[i].fileName,
                  databaseHandle->databaseNode->debug.readWrites[i].lineNb
                 );
          fprintf(stderr,
                  "    command: %s\n",
                  String_cString(databaseHandle->debug.current.sqlString)
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,
                                4,
                                DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                                databaseHandle->debug.current.stackTrace,
                                databaseHandle->debug.current.stackTraceSize,
                                0
                               );
          #endif /* HAVE_BACKTRACE */
//          debugDumpStackTrace(stderr,6,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,databaseHandle->databaseNode->readWrites[i].stackTrace,databaseHandle->databaseNode->readWrites[i].stackTraceSize,0);
        }
      }
    }
    pthread_mutex_unlock(&debugConsoleLock);
  }
  pthread_mutex_unlock(&debugDatabaseLock);
}

void __Database_debugPrintQueryInfo(const char *__fileName__, ulong __lineNb__, const DatabaseStatementHandle *databaseStatementHandle)
{
  assert(databaseStatementHandle != NULL);
  assert(databaseStatementHandle->databaseHandle != NULL);
  assert(databaseStatementHandle->databaseHandle->databaseNode != NULL);

  switch (Database_getType(databaseStatementHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      fprintf(stderr,
              "DEBUG database %s, %lu: 'sqlite:%s': %s\n",
              __fileName__,__lineNb__,
              String_cString(databaseStatementHandle->databaseHandle->databaseNode->databaseSpecifier.sqlite.fileName),
              String_cString(databaseStatementHandle->sqlString)
             );
      break;
    case DATABASE_TYPE_MYSQL:
      #if defined(HAVE_MARIADB)
        fprintf(stderr,
                "DEBUG database %s, %lu: 'mariadb:%s:%s': %s\n",
                __fileName__,__lineNb__,
                String_cString(databaseStatementHandle->databaseHandle->databaseNode->databaseSpecifier.mysql.serverName),
                String_cString(databaseStatementHandle->databaseHandle->databaseNode->databaseSpecifier.mysql.userName),
                String_cString(databaseStatementHandle->sqlString)
               );
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
  }
}

/***********************************************************************\
* Name   : debugFreeColumnsWidth
* Purpose: get columns width
* Input  : widths - column widths
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugFreeColumnsWidth(size_t widths[])
{
  if (widths != NULL) free(widths);
}

/***********************************************************************\
* Name   : debugPrintSpaces
* Purpose: print spaces
* Input  : n - number of spaces
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugPrintSpaces(int n)
{
  int i;

  for (i = 0; i < n; i++)
  {
    printf(" ");
  }
}

/***********************************************************************\
* Name   : debugGetColumnsWidth
* Purpose: get columns width
* Input  : columns - database columns
* Output : -
* Return : widths
* Notes  : -
\***********************************************************************/

LOCAL size_t* debugGetColumnsWidth(const DatabaseValue values[], uint valueCount)
{
  size_t *widths;
  uint   i;
  char   buffer[1024];

  assert(values != NULL);

  widths = (size_t*)malloc(valueCount*sizeof(size_t));
  assert(widths != NULL);

  for (i = 0; i < valueCount; i++)
  {
    widths[i] = 0;
    Database_valueToCString(buffer,sizeof(buffer),&values[i]);
    if (stringLength(buffer) > widths[i])
    {
      widths[i] = stringLength(buffer);
    }
  }

  return widths;
}

void Database_debugDumpTable(DatabaseHandle *databaseHandle, const char *tableName, bool showHeaderFlag)
{
  Errors             error;
  DatabaseColumnName columnNames[DATABASE_MAX_TABLE_COLUMNS];
  DatabaseDataTypes  columnTypes[DATABASE_MAX_TABLE_COLUMNS];
  uint               columnCount;
  String             sqlString;
  DumpTableData      dumpTableData;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  // get table columns
  error = getTableColumns(columnNames,columnTypes,&columnCount,DATABASE_MAX_TABLE_COLUMNS,databaseHandle,tableName);
  if (error != ERROR_NONE)
  {
    printf("ERROR: cannot dump table (error: %s)\n",Error_getText(error));
    return;
  }

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT * FROM %s",
                              tableName
                             );

  // print table
  dumpTableData.showHeaderFlag    = showHeaderFlag;
  dumpTableData.headerPrintedFlag = FALSE;
  dumpTableData.widths            = NULL;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DumpTableData *dumpTableData = (DumpTableData*)userData;
                             uint          i;
                             char          buffer[1024];

                             assert(values != NULL);
                             assert(dumpTableData != NULL);

                             UNUSED_VARIABLE(userData);

                             if (dumpTableData->widths == NULL) dumpTableData->widths = debugGetColumnsWidth(values,valueCount);
                             assert(dumpTableData->widths != NULL);

                             for (i = 0; i < valueCount; i++)
                             {
                               Database_valueToCString(buffer,sizeof(buffer),&values[i]);
                               dumpTableData->widths[i] = MAX(stringLength(buffer),dumpTableData->widths[i]);
                             }

                             return ERROR_NONE;
                           },&dumpTableData),
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           columnTypes,
                           columnCount,
                           "%S",
                           sqlString
                          );
  if (error != ERROR_NONE)
  {
    printf("ERROR: cannot dump table (error: %s)\n",Error_getText(error));
    return;
  }

  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DumpTableData *dumpTableData = (DumpTableData*)userData;
                             uint          i;
                             char          buffer[1024];

                             assert(values != NULL);
                             assert(dumpTableData != NULL);
                             assert(dumpTableData->widths != NULL);

                             UNUSED_VARIABLE(userData);

                             if (dumpTableData->showHeaderFlag && !dumpTableData->headerPrintedFlag)
                             {
                               for (i = 0; i < columnCount; i++)
                               {
                                 printf("%s ",columnNames[i]); debugPrintSpaces(dumpTableData->widths[i]-stringLength(columnNames[i]));
                               }
                               printf("\n");

                               dumpTableData->headerPrintedFlag = TRUE;
                             }
                             for (i = 0; i < valueCount; i++)
                             {
                               Database_valueToCString(buffer,sizeof(buffer),&values[i]);
                               printf("%s ",buffer);
                               if (dumpTableData->showHeaderFlag)
                               {
                                 debugPrintSpaces(dumpTableData->widths[i]-stringLength(buffer));
                               }
                             }
                             printf("\n");

                             return ERROR_NONE;
                           },&dumpTableData),
                           NULL,  // changedRowCount
                           DATABASE_FLAG_NONE,
                           columnTypes,
                           columnCount,
                           "%S",
                           sqlString
                          );
  if (error != ERROR_NONE)
  {
    printf("ERROR: cannot dump table (error: %s)\n",Error_getText(error));
    return;
  }
  debugFreeColumnsWidth(dumpTableData.widths);

  // free resources
  String_delete(sqlString);
}

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
