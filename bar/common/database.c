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

#include "common/global.h"
#include "common/strings.h"
#include "common/lists.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/misc.h"
#include "common/threads.h"
#include "common/semaphores.h"

#include "sqlite3.h"
#include "mysql/mysql.h"
#include "mysql/errmsg.h"

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

LOCAL MYSQL mysql;

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
        sqlite3_exec(databaseHandle->handle.sqlite, \
                     String_cString(s), \
                     debugPrintQueryPlanCallback, \
                     NULL, /* userData */ \
                     NULL /* errorMsg */ \
                    ); \
        String_delete(s); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_START(databaseQueryHandle) \
    do \
    { \
      assert(databaseQueryHandle != NULL); \
      \
      databaseQueryHandle->t0 = Misc_getTimestamp(); \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_END(databaseQueryHandle) \
    do \
    { \
      assert(databaseQueryHandle != NULL); \
      \
      databaseQueryHandle->t1 = Misc_getTimestamp(); \
      databaseQueryHandle->dt += (databaseQueryHandle->t1-databaseQueryHandle->t0); \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME(databaseQueryHandle) \
    do \
    { \
      assert(databaseQueryHandle != NULL); \
      \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database %s, %d: execution time=%llums\n",__FILE__,__LINE__,databaseQueryHandle->dt/1000ULL); \
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
  #define DATABASE_DEBUG_TIME_START(databaseQueryHandle) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME_END(databaseQueryHandle) \
    do \
    { \
    } \
    while (0)
  #define DATABASE_DEBUG_TIME(databaseQueryHandle) \
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
      assert(databaseHandle->databaseNode != NULL); \
      \
      __result = pthread_mutex_lock(databaseHandle->databaseNode->lock); \
      if (__result == 0) \
      {
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
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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
  String_delete(databaseNode->sqlite.fileName);
}

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
      fprintf(stderr,"Database lock info '%s':\n",String_cString(databaseNode->sqlite.fileName));
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
//TODO:
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
//TODO
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
  vformatSQLString(sqlString,command,arguments);
  va_end(arguments);

  return sqlString;
}

/***********************************************************************\
* Name   : unixTimestamp
* Purpose: callback for UNIXTIMESTAMP function (Unix epoch, UTC)
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void unixTimestamp(sqlite3_context *context, int argc, sqlite3_value *argv[])
{
  const char    *text,*format;
  long long int n;
  uint64        timestamp;
  char          *s;
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
    n = strtoll(text,&s,10);
    if (stringIsEmpty(s))
    {
      timestamp = (uint64)n;
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
* Name   : regexpDelete
* Purpose: callback for deleting REGEXP data
* Input  : data - data to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void regexpDelete(void *data)
{
  assert(data != NULL);

  regfree((regex_t*)data);
  free(data);
}

/***********************************************************************\
* Name   : regexpMatch
* Purpose: callback for REGEXP function
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void regexpMatch(sqlite3_context *context, int argc, sqlite3_value *argv[])
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
    sqlite3_set_auxdata(context,0,regex,regexpDelete);
  }

  // match pattern
  result = (regexec(regex,text,0,NULL,0) == 0) ? 1 : 0;

  sqlite3_result_int(context,result);
}

/***********************************************************************\
* Name   : dirname
* Purpose: callback for DIRNAME function
* Input  : context - SQLite3 context
*          argc    - number of arguments
*          argv    - argument array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dirname(sqlite3_context *context, int argc, sqlite3_value *argv[])
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
      (void)sqlite3_wal_checkpoint_v2(databaseHandle->handle.sqlite,NULL,CHECKPOINT_MODE,NULL,NULL);
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
* Name   : unlockNotifyCallback
* Purpose: SQLite3 unlock notify callback
* Input  : argv - arguments
*          argc - number of arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void unlockNotifyCallback(void *argv[], int argc)
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

LOCAL int waitUnlockNotify(sqlite3 *handle)
{
  int   sqliteResult;
  sem_t semaphore;

  // init variables
  sem_init(&semaphore,0,0);

  // register call-back
  sqliteResult = sqlite3_unlock_notify(handle,unlockNotifyCallback,&semaphore);
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
* Name   : sqliteStep
* Purpose: step statement
* Input  : handle          - database handle
*          statementHandle - statement handle
*          timeout         - timeout [ms]
* Output : -
* Return : SQLite result code
* Notes  : -
\***********************************************************************/

LOCAL int sqliteStep(sqlite3 *handle, sqlite3_stmt *statementHandle, long timeout)
{
  #define SLEEP_TIME 500L

  uint n;
  int  sqliteResult;

  assert(handle != NULL);
  assert(statementHandle != NULL);

  n = 0;
  do
  {
    sqliteResult = sqlite3_step(statementHandle);
    if (sqliteResult == SQLITE_LOCKED)
    {
      waitUnlockNotify(handle);
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

  return sqliteResult;

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : databaseExecute
* Purpose: execute database statement
* Input  : databaseHandle      - database handle
*          sqlString           - SQL string
*          databaseRowFunction - row call-back function (can be NULL)
*          databaseRowUserData - user data for row call-back
*          changedRowCount     - number of changed rows (can be NULL)
*          timeout             - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors databaseExecute(DatabaseHandle      *databaseHandle,
                             const char          *sqlString,
                             DatabaseRowFunction databaseRowFunction,
                             void                *databaseRowUserData,
                             ulong               *changedRowCount,
                             long                timeout
                            )
{
  #define SLEEP_TIME 500L  // [ms]

  uint                          maxRetryCount;
  uint                          retryCount;
  StringTokenizer               stringTokenizer;
  String                        sqlCommand;
  Errors                        error;
  uint                          count;
  const char                    **names,**values;
  uint                          i;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert ((databaseHandle->databaseNode->readCount > 0) || (databaseHandle->databaseNode->readWriteCount > 0));
  assert(databaseHandle->handle.sqlite != NULL);

  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  String_initTokenizerCString(&stringTokenizer,
                              stringTrimBegin(sqlString),
                              ";",
                              STRING_QUOTES,
                              TRUE
                             );
  error         = ERROR_NONE;
  retryCount    = 0;
  while (   (error == ERROR_NONE)
         && String_getNextToken(&stringTokenizer,&sqlCommand,NULL)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        )
  {
fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(sqlCommand));
// TODO: reactivate when each thread has his own index handle
#if 0
    assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy));
#endif

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlCommand);
    #endif /* not NDEBUG */

    switch (databaseHandle->databaseNode->type)
    {
      case DATABASE_TYPE_SQLITE3:
        {
          int          sqliteResult;
          sqlite3_stmt *statementHandle;
  //fprintf(stderr,"%s, %d: sqlCommands='%s'\n",__FILE__,__LINE__,String_cString(sqlCommand));
          // prepare SQL statement
          sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                            String_cString(sqlCommand),
                                            -1,
                                            &statementHandle,
                                            NULL  // nextSqlCommand
                                           );
          if      (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
          }
          else if (sqliteResult == SQLITE_INTERRUPT)
          {
            return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlCommand));
          }
          else if (sqliteResult != SQLITE_OK)
          {
            return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlCommand));
          }
          assert(statementHandle != NULL);

          // allocate call-back data
          names  = NULL;
          values = NULL;
          count  = 0;
          if (databaseRowFunction != NULL)
          {
            count = sqlite3_column_count(statementHandle);
            names = (const char**)malloc(count*sizeof(const char*));
            if (names == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            values = (const char**)malloc(count*sizeof(const char*));
            if (values == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
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
                waitUnlockNotify(databaseHandle->handle.sqlite);
                sqlite3_reset(statementHandle);
              }
              else if (sqliteResult == SQLITE_MISUSE)
              {
                HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
              }
            }
            while (sqliteResult == SQLITE_LOCKED);

            // process row
            if      (sqliteResult == SQLITE_ROW)
            {
              if (databaseRowFunction != NULL)
              {
                for (i = 0; i < count; i++)
                {
                  names[i]  = sqlite3_column_name(statementHandle,i);
                  values[i] = (const char*)sqlite3_column_text(statementHandle,i);
                }
                error = databaseRowFunction(names,values,count,databaseRowUserData);
              }
            }
          }
          while ((error == ERROR_NONE) && (sqliteResult == SQLITE_ROW));

          // free call-back data
          if (databaseRowFunction != NULL)
          {
            free(values);
            free(names);
          }

          // get number of changes
          if (changedRowCount != NULL)
          {
            (*changedRowCount) += (ulong)sqlite3_changes(databaseHandle->handle.sqlite);
          }

          // done SQL statement
          sqlite3_finalize(statementHandle);
        }
        break;
      case DATABASE_TYPE_MYSQL:
        {
fprintf(stderr,"%s:%d: sqlCommand=%s\n",__FILE__,__LINE__,String_cString(sqlCommand));
          int mysqlResult;

          // prepare SQL statement
          MYSQL_STMT *statementHandle = mysql_stmt_init(databaseHandle->handle.mysql);
          assert(statementHandle != NULL);
          mysqlResult = mysql_stmt_prepare(statementHandle,
                                           String_cString(sqlCommand),
                                           String_length(sqlCommand)
                                          );
          if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
          {
            HALT_INTERNAL_ERROR("MySQL library reported misuse %d %s",mysqlResult,mysql_stmt_error(statementHandle));
          }
          else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
          {
            mysql_stmt_close(statementHandle);
            return ERRORX_(DATABASE_CONNECTION_LOST,mysql_errno(&mysql),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
          }
          else if (mysqlResult != 0)
          {
            mysql_stmt_close(statementHandle);
            return ERRORX_(DATABASE,mysql_errno(&mysql),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
          }
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);

          // allocate call-back data
          names  = NULL;
          values = NULL;
          count  = 0;
          if (databaseRowFunction != NULL)
          {
            //// TODO: msgql_stmt?parameter_count
            count = mysql_stmt_field_count(statementHandle);
            names = (const char**)malloc(count*sizeof(const char*));
            if (names == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            values = (const char**)malloc(count*sizeof(const char*));
            if (values == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
          }

fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
          // step and process rows
          mysqlResult = mysql_stmt_execute(statementHandle);
          if      (mysqlResult == CR_COMMANDS_OUT_OF_SYNC)
          {
            HALT_INTERNAL_ERROR("MySQL library reported misuse %d %s",mysqlResult,mysql_stmt_error(statementHandle));
          }
          else if ((mysqlResult == CR_SERVER_GONE_ERROR) || (mysqlResult == CR_SERVER_LOST))
          {
            mysql_stmt_close(statementHandle);
            return ERRORX_(DATABASE_CONNECTION_LOST,mysql_stmt_errno(statementHandle),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
          }
          else if (mysqlResult != 0)
          {
            mysql_stmt_close(statementHandle);
            return ERRORX_(DATABASE,mysql_stmt_errno(statementHandle),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
          }

fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
          if (mysql_stmt_num_rows(statementHandle) > 0)
          {
            do
            {
              // process row
              mysqlResult = mysql_stmt_fetch(statementHandle);
              if      (mysqlResult == 0)
              {
                if (databaseRowFunction != NULL)
                {
                  for (i = 0; i < count; i++)
                  {
  // TODO:                  names[i]  = sqlite3_column_name(statementHandle,i);
  names[i] = "xxx";
                    values[i] = (const char*)sqlite3_column_text(statementHandle,i);
                  }
  fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
                  error = databaseRowFunction(names,values,count,databaseRowUserData);
                }
              }
              else if (mysqlResult == 1)
              {
                error = ERRORX_(DATABASE,mysql_stmt_errno(statementHandle),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
              }
              else if (mysqlResult == MYSQL_DATA_TRUNCATED)
              {
                error = ERRORX_(DATABASE,mysql_stmt_errno(statementHandle),"%s: %s",mysql_stmt_error(statementHandle),String_cString(sqlCommand));
              }
  fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
            }
            while ((error == ERROR_NONE) && (mysqlResult == 0));
          }
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));

          // free call-back data
          if (databaseRowFunction != NULL)
          {
            free(values);
            free(names);
          }

          // get number of changes
          if (changedRowCount != NULL)
          {
            (*changedRowCount) += (ulong)mysql_stmt_affected_rows(statementHandle);
          }

          // done SQL statement
          mysql_stmt_close(statementHandle);
        }
        break;
      #ifndef NDEBUG
        default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
      #endif
    }

    #ifndef NDEBUG
      // clear SQL command, backtrace
      String_clear(databaseHandle->debug.current.sqlCommand);
    #endif /* not NDEBUG */

    // check result
    if      (Error_getCode(error) == ERROR_DATABASE_BUSY)
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

      // retry
      retryCount++;
      continue;
    }
    else if (Error_getCode(error) == ERROR_DATABASE_INTERRUPTED)
    {
      // report interrupt
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlCommand));
    }
    else if (error != ERROR_NONE)
    {
      // report error
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlCommand));
    }
  }
  String_doneTokenizer(&stringTokenizer);
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));

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
* Name   : freeColumnNode
* Purpose: free column node
* Input  : columnNode - column node
*          userData   - user data (no used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeColumnNode(DatabaseColumnNode *columnNode, void *userData)
{
  assert(columnNode != NULL);
  assert(columnNode->name != NULL);

  UNUSED_VARIABLE(userData);

  switch (columnNode->type)
  {
    case DATABASE_DATATYPE_PRIMARY_KEY:
      break;
    case DATABASE_DATATYPE_FOREIGN_KEY:
      break;
    case DATABASE_DATATYPE_INT64:
      String_delete(columnNode->value.i);
      break;
    case DATABASE_DATATYPE_DOUBLE:
      String_delete(columnNode->value.d);
      break;
    case DATABASE_DATATYPE_DATETIME:
      String_delete(columnNode->value.i);
      break;
    case DATABASE_DATATYPE_TEXT:
      String_delete(columnNode->value.text);
      break;
    case DATABASE_DATATYPE_BLOB:
      if (columnNode->value.blob.data != NULL)
      {
        free(columnNode->value.blob.data);
      }
      break;
    case DATABASE_DATATYPE_UNKNOWN:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* not NDEBUG */
      break; // not reached
  }
  free(columnNode->name);
}

/***********************************************************************\
* Name   : getTableList
* Purpose: get table list
* Input  : tableList      - table list variable
*          databaseHandle - database handle
* Output : tableList - table list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getTableList(StringList     *tableList,
                          DatabaseHandle *databaseHandle
                         )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  const char          *name;

  assert(tableList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  StringList_init(tableList);

//TODO: remove
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    error = Database_prepare(&databaseQueryHandle,
                             databaseHandle,
                             "SELECT name FROM sqlite_master where type='table'"
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (Database_getNextRow(&databaseQueryHandle,
                               "%p",
                               &name
                              )
          )
    {
      StringList_appendCString(tableList,name);
    }
    Database_finalize(&databaseQueryHandle);

    return error;
  });

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getTableColumnList
* Purpose: get table column list
* Input  : columnList     - column list variable
*          databaseHandle - database handle
*          tableName      - table name
* Output : columnList - column list
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getTableColumnList(DatabaseColumnList *columnList,
                                DatabaseHandle     *databaseHandle,
                                const char         *tableName
                               )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle1;
  const char          *name,*type;
  bool                primaryKey;
  DatabaseColumnNode  *columnNode;

  assert(columnList != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  List_init(columnList);

//TODO: remove
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               WAIT_FOREVER,
  {
    error = Database_prepare(&databaseQueryHandle1,
                             databaseHandle,
                             "PRAGMA table_info(%s) \
                             ",
                             tableName
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%d %p %p %d %d %b",
                               NULL,  // id
                               &name,
                               &type,
                               NULL,  // canBeNULL
                               NULL,  // defaultValue
                               &primaryKey
                              )
          )
    {
      columnNode = LIST_NEW_NODE(DatabaseColumnNode);
      if (columnNode == NULL)
      {
        List_done(columnList,CALLBACK_((ListNodeFreeFunction)freeColumnNode,NULL));
        return ERROR_INSUFFICIENT_MEMORY;
      }

      columnNode->name = stringDuplicate(name);
      if (   stringEqualsIgnoreCase(type,"INTEGER")
          || stringEqualsIgnoreCase(type,"NUMERIC")
         )
      {
        if (primaryKey)
        {
          columnNode->type     = DATABASE_DATATYPE_PRIMARY_KEY;
          columnNode->value.id = 0LL;
        }
        else
        {
          columnNode->type    = DATABASE_DATATYPE_INT64;
          columnNode->value.i = String_new();
        }
      }
      else if (stringEqualsIgnoreCase(type,"REAL"))
      {
        columnNode->type    = DATABASE_DATATYPE_DOUBLE;
        columnNode->value.d = String_new();
      }
      else if (stringEqualsIgnoreCase(type,"TEXT"))
      {
        columnNode->type       = DATABASE_DATATYPE_TEXT;
        columnNode->value.text = String_new();
      }
      else if (stringEqualsIgnoreCase(type,"BLOB"))
      {
        columnNode->type              = DATABASE_DATATYPE_BLOB;
        columnNode->value.blob.data   = NULL;
        columnNode->value.blob.length = 0;
      }
      else
      {
        columnNode->type = DATABASE_DATATYPE_UNKNOWN;
      }
      columnNode->usedFlag = FALSE;

      List_append(columnList,columnNode);
    }
    Database_finalize(&databaseQueryHandle1);

    return error;
  });

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : freeTableColumnList
* Purpose: free table column list
* Input  : columnList - column list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeTableColumnList(DatabaseColumnList *columnList)
{
  assert(columnList != NULL);

  List_done(columnList,CALLBACK_((ListNodeFreeFunction)freeColumnNode,NULL));
}

/***********************************************************************\
* Name   : findTableColumnNode
* Purpose: find table column list node
* Input  : columnList - column list
*          columnName - column name
* Output : -
* Return : column node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DatabaseColumnNode *findTableColumnNode(const DatabaseColumnList *columnList, const char *columnName)
{
  DatabaseColumnNode *columnNode;

  LIST_ITERATE(columnList,columnNode)
  {
    if (stringEquals(columnNode->name,columnName)) return columnNode;
  }

  return NULL;
}

/***********************************************************************\
* Name   : getDatabaseTypeString
* Purpose: get database type string
* Input  : type - database type
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

LOCAL const char *getDatabaseTypeString(DatabaseDataTypes type)
{
  const char *string;

  string = NULL;
  switch (type)
  {
    case DATABASE_DATATYPE_PRIMARY_KEY:
      string = "INTEGER PRIMARY KEY";
      break;
    case DATABASE_DATATYPE_INT64:
      string = "INTEGER";
      break;
    case DATABASE_DATATYPE_DOUBLE:
      string = "REAL";
      break;
    case DATABASE_DATATYPE_DATETIME:
      string = "INTEGER";
      break;
    case DATABASE_DATATYPE_TEXT:
      string = "TEXT";
      break;
    case DATABASE_DATATYPE_BLOB:
      string = "BLOB";
      break;
    case DATABASE_DATATYPE_UNKNOWN:
      string = "unknown";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* not NDEBUG */
      break; // not reached
  }

  return string;
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

  // init MySQL
  mysql_init(&mysql);

  return ERROR_NONE;
}

void Database_doneAll(void)
{
  // done database list
  Semaphore_done(&databaseList.lock);
  List_done(&databaseList,CALLBACK_((ListNodeFreeFunction)freeDatabaseNode,NULL));

  #ifndef DATABASE_LOCK_PER_INSTANCE
    // done global lock
    pthread_mutex_destroy(&databaseLock);
    pthread_mutexattr_destroy(&databaseLockAttribute);
  #endif /* not DATABASE_LOCK_PER_INSTANCE */
}

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *uri,
                       DatabaseOpenModes databaseOpenMode,
                       long              timeout
                      )
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         ulong             __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *uri,
                         DatabaseOpenModes databaseOpenMode,
                         long              timeout
                        )
#endif /* NDEBUG */
{
  String        directoryName;
  Errors        error;
  String        databaseFileName;
  int           sqliteMode;
  int           sqliteResult;
  DatabaseNode  *databaseNode;
  #ifndef NDEBUG
    uint          i;
  #endif /* NDEBUG */

fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
  assert(databaseHandle != NULL);

  // init variables
  databaseHandle->readLockCount           = 0;
  databaseHandle->readWriteLockCount      = 0;
  databaseHandle->handle.sqlite           = NULL;
  databaseHandle->handle.mysql            = NULL;
  databaseHandle->timeout                 = timeout;
  databaseHandle->lastCheckpointTimestamp = Misc_getTimestamp();
  if (sem_init(&databaseHandle->wakeUp,0,0) != 0)
  {
    return ERRORX_(DATABASE,0,"init locking");
  }

  // get database type and open/connect data
  DatabaseTypes type;
  String        fileName   = String_new();
  String        serverName = String_new();
  String        userName   = String_new();
  String        password   = String_new();
  if      (String_matchCString(uri,
                               STRING_BEGIN,
                               "^sqlite3:(.*)",
                               NULL,
                               STRING_NO_ASSIGN,
                               fileName,
                               NULL
                              )
          )
  {
    type = DATABASE_TYPE_SQLITE3;
  }
  else if (String_matchCString(uri,
                               STRING_BEGIN,
                               "^mysql:([^:]+):([^:]+):(.*)",
                               NULL,
                               STRING_NO_ASSIGN,
                               serverName,
                               userName,
                               password,
                               NULL
                              )
          )
  {
    type = DATABASE_TYPE_MYSQL;
  }
  else
  {
    type = DATABASE_TYPE_SQLITE3;
  }

  switch (type)
  {
    case DATABASE_TYPE_SQLITE3:
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
                                     FILE_DEFAULT_PERMISSION
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

      // get sqlite name
      if (!String_isEmpty(fileName))
      {
        // open file
        String_format(fileName,"file:%s",fileName);
      }
      else
      {
        // open memory
        String_format(fileName,"file::memory:");
      }

      // get mode
      sqliteMode = SQLITE_OPEN_URI;
      switch (databaseOpenMode & DATABASE_OPEN_MASK_MODE)
      {
        case DATABASE_OPENMODE_READ:
          sqliteMode |= SQLITE_OPEN_READONLY;
          String_appendCString(databaseFileName,"?immutable=1");
          break;
        case DATABASE_OPENMODE_READWRITE:
          sqliteMode |= SQLITE_OPEN_READWRITE;
          break;
        case DATABASE_OPENMODE_CREATE:
          sqliteMode |= SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
          break;
      }
      if ((databaseOpenMode & DATABASE_OPEN_MASK_FLAGS) != 0)
      {
        if ((databaseOpenMode & DATABASE_OPENMODE_MEMORY) == DATABASE_OPENMODE_MEMORY) sqliteMode |= SQLITE_OPEN_MEMORY;//String_appendCString(databaseFileName,"mode=memory");
        if ((databaseOpenMode & DATABASE_OPENMODE_SHARED) == DATABASE_OPENMODE_SHARED) sqliteMode |= SQLITE_OPEN_SHAREDCACHE;//String_appendCString(databaseFileName,"cache=shared");
      }
    //sqliteMode |= SQLITE_OPEN_NOMUTEX;

      // open database
      sqliteResult = sqlite3_open_v2(String_cString(fileName),&databaseHandle->handle.sqlite,sqliteMode,NULL);
      if (sqliteResult != SQLITE_OK)
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s",sqlite3_errmsg(databaseHandle->handle.sqlite));
        String_delete(databaseFileName);
        sem_destroy(&databaseHandle->wakeUp);
        return error;
      }

      // attach aux database
      if ((databaseOpenMode & DATABASE_OPENMODE_AUX) == DATABASE_OPENMODE_AUX)
      {
        sqliteResult = sqlite3_exec(databaseHandle->handle.sqlite,
                                    "ATTACH DATABASE ':memory:' AS " DATABASE_AUX,
                                    CALLBACK_(NULL,NULL),
                                    NULL
                                   );
        if (sqliteResult != SQLITE_OK)
        {
          error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s",sqlite3_errmsg(databaseHandle->handle.sqlite));
          sqlite3_close(databaseHandle->handle.sqlite);
          String_delete(password);
          String_delete(userName);
          String_delete(serverName);
          String_delete(fileName);
          sem_destroy(&databaseHandle->wakeUp);
          return error;
        }
      }
      break;
    case DATABASE_TYPE_MYSQL:
      // open database
      databaseHandle->handle.mysql = mysql_real_connect(&mysql,
                                                        String_cString(serverName),
                                                        String_cString(userName),
                                                        String_cString(password),
                                                        "bar",
                                                        0,
                                                        0,
                                                        0
                                                       );
      if (databaseHandle->handle.mysql == NULL)
      {
        error = ERRORX_(DATABASE,mysql_errno(&mysql),"%s",mysql_error(&mysql));
        String_delete(password);
        String_delete(userName);
        String_delete(serverName);
        String_delete(fileName);
        sem_destroy(&databaseHandle->wakeUp);
        return error;
      }
      break;
  }

  // get database node
  SEMAPHORE_LOCKED_DO(&databaseList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    databaseNode = LIST_FIND(&databaseList,
                             databaseNode,
                                (databaseNode->type == type)
                             && String_equals(databaseNode->sqlite.fileName,databaseFileName)
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
      databaseNode->type                    = type;
      databaseNode->sqlite.fileName         = String_duplicate(fileName);
      databaseNode->mysql.serverName        = String_duplicate(serverName);
      databaseNode->mysql.userName          = String_duplicate(userName);
      databaseNode->openCount               = 1;
      #ifdef DATABASE_LOCK_PER_INSTANCE
        if (pthread_mutexattr_init(&databaseNode->lockAttribute) != 0)
        {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
          sqlite3_close(databaseHandle->handle.sqlite);
          String_delete(databaseFileName);
          sem_destroy(&databaseHandle->wakeUp);
          return ERRORX_(DATABASE,0,"init locking");
        }
        pthread_mutexattr_settype(&databaseLockAttribute,PTHREAD_MUTEX_RECURSIVE);
        if (pthread_mutex_init(&databaseNode->lock,&databaseNode->lockAttribute) != 0)
        {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
          pthread_mutexattr_destroy(&databaseNode->lockAttribute);
          sqlite3_close(databaseHandle->handle.sqlite);
          String_delete(databaseFileName);
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

  // set handlers
  switch (type)
  {
    case DATABASE_TYPE_SQLITE3:
      #if !defined(NDEBUG) && defined(DATABASE_DEBUG_LOG)
        fprintf(stderr,"WARNING: datatbase logging is enabled!\n");
        switch (databaseNode->type)
        {
          case DATABASE_TYPE_SQLITE3:
            sqlite3_trace_v2(databaseHandle->handle.sqlite,DATABASE_DEBUG_LOG,logTraceCommandHandler,NULL);
            break;
          case DATABASE_TYPE_MYSQL:
            break;
        }
      #endif /* DATABASE_DEBUG_LOG */
#if 0
      // set busy handler
      sqliteResult = sqlite3_busy_handler(databaseHandle->handle.sqlite,busyHandler,databaseHandle);
      assert(sqliteResult == SQLITE_OK);
#endif /* 0 */
      // set progress handler
      sqlite3_progress_handler(databaseHandle->handle.sqlite,50000,progressHandler,databaseHandle);
      break;
    case DATABASE_TYPE_MYSQL:
      break;
  }

  // register special functions
  switch (type)
  {
    case DATABASE_TYPE_SQLITE3:
      sqliteResult = sqlite3_create_function(databaseHandle->handle.sqlite,
                                             "unixtimestamp",
                                             1,
                                             SQLITE_ANY,
                                             NULL,
                                             unixTimestamp,
                                             NULL,
                                             NULL
                                            );
      assert(sqliteResult == SQLITE_OK);
      sqliteResult = sqlite3_create_function(databaseHandle->handle.sqlite,
                                             "regexp",
                                             3,
                                             SQLITE_ANY,
                                             NULL,
                                             regexpMatch,
                                             NULL,
                                             NULL
                                            );
      assert(sqliteResult == SQLITE_OK);
      sqliteResult = sqlite3_create_function(databaseHandle->handle.sqlite,
                                             "dirname",
                                             1,
                                             SQLITE_ANY,
                                             NULL,
                                             dirname,
                                             NULL,
                                             NULL
                                            );
      assert(sqliteResult == SQLITE_OK);
//TODO: debug function for logging?
#if 0
      sqliteResult = sqlite3_create_function(databaseHandle->handle.sqlite,
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
      break;
    case DATABASE_TYPE_MYSQL:
      break;
  }

  switch (type)
  {
    case DATABASE_TYPE_SQLITE3:
      // enable recursive triggers
      sqliteResult = sqlite3_exec(databaseHandle->handle.sqlite,
                                  "PRAGMA recursive_triggers=ON",
                                  CALLBACK_(NULL,NULL),
                                  NULL
                                 );
      assert(sqliteResult == SQLITE_OK);
      break;
    case DATABASE_TYPE_MYSQL:
      break;
  }

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: open '%s'\n",fileName);
  #endif

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseHandle,DatabaseHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,DatabaseHandle);
  #endif /* NDEBUG */

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

      databaseHandle->debug.locked.threadId    = THREAD_ID_NONE;
      databaseHandle->debug.locked.lineNb      = 0;
      databaseHandle->debug.locked.t0          = 0ULL;
      databaseHandle->debug.locked.t1          = 0ULL;
      databaseHandle->debug.current.sqlCommand = String_new();
      #ifdef HAVE_BACKTRACE
        databaseHandle->debug.current.stackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */

      // add to handle-list
      List_append(&debugDatabaseHandleList,databaseHandle);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* not NDEBUG */

fprintf(stderr,"%s:%d: %d\n",__FILE__,__LINE__,databaseHandle->databaseNode->type);
  // free resources
  String_delete(databaseFileName);

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
  #ifndef NDEBUG
    DatabaseHandle *debugDatabaseHandle;
  #endif /* not NDEBUG */

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->readWriteLockCount == 0);
  assert(databaseHandle->readLockCount == 0);
  assert(databaseHandle->handle.sqlite != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseHandle,DatabaseHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,DatabaseHandle);
  #endif /* NDEBUG */

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
      String_delete(databaseHandle->debug.current.sqlCommand);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* not NDEBUG */

  switch (databaseHandle->databaseNode->type)
  {
    case DATABASE_TYPE_SQLITE3:
      // clear progress handler
      sqlite3_progress_handler(databaseHandle->handle.sqlite,0,NULL,NULL);

      // clear busy timeout handler
      sqlite3_busy_handler(databaseHandle->handle.sqlite,NULL,NULL);
      break;
    case DATABASE_TYPE_MYSQL:
      break;
  }

  // close database
  switch (databaseHandle->databaseNode->type)
  {
    case DATABASE_TYPE_SQLITE3:
      sqlite3_close(databaseHandle->handle.sqlite);
      break;
    case DATABASE_TYPE_MYSQL:
      mysql_close(databaseHandle->handle.mysql);
      break;
  }

  // free resources
//TODO: remove?
  sem_destroy(&databaseHandle->wakeUp);
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
  assert(databaseHandle->handle.sqlite != NULL);
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
  assert(databaseHandle->handle.sqlite != NULL);
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
  assert(databaseHandle->handle.sqlite != NULL);
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
  assert(databaseHandle->handle.sqlite != NULL);
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
    sqlite3_interrupt(databaseHandle->handle.sqlite);
  #endif /* DATABASE_SUPPORT_INTERRUPT */
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

  if (error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "PRAGMA synchronous=%s;",
                             enabled ? "ON" : "OFF"
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "PRAGMA journal_mode=%s;",
                             enabled ? "ON" : "WAL"
                            );
  }

  return ERROR_NONE;
}

Errors Database_setEnabledForeignKeys(DatabaseHandle *databaseHandle,
                                      bool           enabled
                                     )
{
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  switch (databaseHandle->databaseNode->type)
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "PRAGMA foreign_keys=%s;",
                               enabled ? "ON" : "OFF"
                              );
      break;
    case DATABASE_TYPE_MYSQL:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "SET FOREIGN_KEY_CHECKS=%d;",
                               enabled ? 1 : 0
                              );
      break;
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

  switch (databaseHandle->databaseNode->type)
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "PRAGMA temp_store_directory='%s';",
                               directoryName
                              );
      break;
    case DATABASE_TYPE_MYSQL:
      error = ERROR_NONE;  // not supported; ignored
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
                          "CREATE TABLE %s.%s \
                           ( \
                             id INTEGER PRIMARY KEY, \
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
                          "DROP TABLE %s.%s",
                          DATABASE_AUX,
                          TEMPORARY_TABLE_NAMES[id]
                         );
}

Errors Database_compare(DatabaseHandle *databaseHandleReference,
                        DatabaseHandle *databaseHandle
                       )
{
  Errors             error;
  StringList         tableListReference,tableList;
  DatabaseColumnList columnListReference,columnList;
  StringNode         *tableNameNodeReference,*tableNameNode;
  String             tableNameReference,tableName;
  DatabaseColumnNode *columnNodeReference,*columnNode;

  assert(databaseHandleReference != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandleReference);
//TODO: remove
assert(Thread_isCurrentThread(databaseHandle->debug.threadId));
  assert(databaseHandleReference->handle.sqlite != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);

  // get table lists
  error = getTableList(&tableListReference,databaseHandleReference);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = getTableList(&tableList,databaseHandle);
  if (error != ERROR_NONE)
  {
    StringList_done(&tableListReference);
    return error;
  }

  // compare tables
  STRINGLIST_ITERATEX(&tableListReference,tableNameNodeReference,tableNameReference,error == ERROR_NONE)
  {
    if (StringList_contains(&tableList,tableNameReference))
    {
      // get column lists
      error = getTableColumnList(&columnListReference,databaseHandleReference,String_cString(tableNameReference));
      if (error != ERROR_NONE)
      {
        break;
      }
      error = getTableColumnList(&columnList,databaseHandle,String_cString(tableNameReference));
      if (error != ERROR_NONE)
      {
        freeTableColumnList(&columnListReference);
        break;
      }

      // compare columns
      LIST_ITERATEX(&columnListReference,columnNodeReference,error == ERROR_NONE)
      {
        columnNode = LIST_FIND(&columnList,columnNode,stringEquals(columnNodeReference->name,columnNode->name));
        if (columnNode != NULL)
        {
          if (columnNodeReference->type != columnNode->type)
          {
            error = ERRORX_(DATABASE_TYPE_MISMATCH,0,"%s",columnNodeReference->name);
          }
        }
        else
        {
          error = ERRORX_(DATABASE_MISSING_COLUMN,0,"%s",columnNodeReference->name);
        }
      }

      // check for obsolete columns
      LIST_ITERATEX(&columnList,columnNode,error == ERROR_NONE)
      {
        if (LIST_FIND(&columnListReference,columnNodeReference,stringEquals(columnNodeReference->name,columnNode->name)) == NULL)
        {
          error = ERRORX_(DATABASE_OBSOLETE_COLUMN,0,"%s",columnNode->name);
        }
      }

      // free resources
      freeTableColumnList(&columnList);
      freeTableColumnList(&columnListReference);
    }
    else
    {
      error = ERRORX_(DATABASE_MISSING_TABLE,0,"%s",String_cString(tableNameReference));
    }
  }

  // check for obsolete tables
  STRINGLIST_ITERATEX(&tableList,tableNameNode,tableName,error == ERROR_NONE)
  {
    if (!StringList_contains(&tableListReference,tableName))
    {
      error = ERRORX_(DATABASE_OBSOLETE_TABLE,0,"%s",String_cString(tableName));
    }
  }

  // free resources
  StringList_done(&tableList);
  StringList_done(&tableListReference);

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

  Errors             error;
  uint               n;
  uint64             t;
  DatabaseColumnList fromColumnList,toColumnList;
  DatabaseColumnNode *columnNode;
  String             sqlSelectString,sqlInsertString;
  va_list            arguments;
  sqlite3_stmt       *fromStatementHandle,*toStatementHandle;
  int                sqliteResult;
  uint               i;
  DatabaseColumnNode *toColumnNode;
  DatabaseId         lastRowId;
  #ifdef DATABASE_DEBUG_COPY_TABLE
    uint64 t0,t1;
    ulong  rowCount;
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  assert(fromDatabaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromDatabaseHandle);
// TODO: remove
assert(Thread_isCurrentThread(fromDatabaseHandle->debug.threadId));
  assert(fromDatabaseHandle->handle.sqlite != NULL);
  assert(toDatabaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(toDatabaseHandle);
// TODO: remove
assert(Thread_isCurrentThread(toDatabaseHandle->debug.threadId));
  assert(toDatabaseHandle->handle.sqlite != NULL);
  assert(fromTableName != NULL);
  assert(toTableName != NULL);

  #ifdef DATABASE_DEBUG_COPY_TABLE
    t0 = Misc_getTimestamp();
    rowCount = 0;
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  // get table columns
  START_TIMER();
  error = getTableColumnList(&fromColumnList,fromDatabaseHandle,fromTableName);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (List_isEmpty(&fromColumnList))
  {
    freeTableColumnList(&fromColumnList);
    return ERRORX_(DATABASE_MISSING_TABLE,0,"%s",fromTableName);
  }
  error = getTableColumnList(&toColumnList,toDatabaseHandle,toTableName);
  if (error != ERROR_NONE)
  {
    freeTableColumnList(&fromColumnList);
    return error;
  }
  if (List_isEmpty(&toColumnList))
  {
    freeTableColumnList(&toColumnList);
    freeTableColumnList(&fromColumnList);
    return ERRORX_(DATABASE_MISSING_TABLE,0,"%s",toTableName);
  }
  END_TIMER();

  // create SQL select statement string
  sqlSelectString = formatSQLString(String_new(),"SELECT ");
  i = 0;
  LIST_ITERATE(&fromColumnList,columnNode)
  {
    if (i > 0) String_appendChar(sqlSelectString,',');
    String_appendCString(sqlSelectString,columnNode->name);
    i++;
  }
  formatSQLString(sqlSelectString," FROM %s",fromTableName);
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

  // select rows in from-table and copy to to-table
  sqlInsertString = String_new();
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
        return error;
      }
    }

    // create select statement
    sqliteResult = sqlite3_prepare_v2(fromDatabaseHandle->handle.sqlite,
                                      String_cString(sqlSelectString),
                                      -1,
                                      &fromStatementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      // nothing to do
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(fromDatabaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      error = ERRORX_(INTERRUPTED,sqlite3_errcode(fromDatabaseHandle->handle.sqlite),"%s: %s",String_cString(sqlSelectString),sqlite3_errmsg(fromDatabaseHandle->handle.sqlite));
      if (transactionFlag)
      {
        (void)Database_rollbackTransaction(toDatabaseHandle);
      }
      return error;
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(fromDatabaseHandle->handle.sqlite),"%s: %s",String_cString(sqlSelectString),sqlite3_errmsg(fromDatabaseHandle->handle.sqlite));
      if (transactionFlag)
      {
        (void)Database_rollbackTransaction(toDatabaseHandle);
      }
      return error;
    }
    #ifndef NDEBUG
      if (fromStatementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(fromDatabaseHandle->handle.sqlite),sqlite3_errmsg(fromDatabaseHandle->handle.sqlite),String_cString(sqlSelectString));
        abort();
      }
    #endif /* not NDEBUG */

    // copy rows
    n = 0;
    while ((sqliteResult = sqliteStep(fromDatabaseHandle->handle.sqlite,fromStatementHandle,fromDatabaseHandle->timeout)) == SQLITE_ROW)
    {
      #ifdef DATABASE_DEBUG_COPY_TABLE
        rowCount++;
      #endif /* DATABASE_DEBUG_COPY_TABLE */

      // reset to-data
      LIST_ITERATE(&toColumnList,columnNode)
      {
        columnNode->usedFlag = FALSE;
      }

      // get from values, set in toColumnList
      i = 0;
      LIST_ITERATE(&fromColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_DATATYPE_PRIMARY_KEY:
            columnNode->value.id = sqlite3_column_int64(fromStatementHandle,i);
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              toColumnNode->value.id = DATABASE_ID_NONE;
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_DATATYPE_INT64:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,i));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_DATATYPE_DOUBLE:
            String_setCString(columnNode->value.d,(const char*)sqlite3_column_text(fromStatementHandle,i));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.d,columnNode->value.d);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_DATATYPE_DATETIME:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,i));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_DATATYPE_TEXT:
            String_setCString(columnNode->value.text,(const char*)sqlite3_column_text(fromStatementHandle,i));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.text,columnNode->value.text);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_DATATYPE_BLOB:
            HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX("from column '%s'",columnNode->name);
            #endif /* not NDEBUG */
            break; // not reached
        }
        i++;
      }

      // call pre-copy callback (if defined)
      if (preCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER),
        {
          return preCopyTableFunction(&fromColumnList,&toColumnList,preCopyTableUserData);
        });
        if (error != ERROR_NONE)
        {
          sqlite3_finalize(fromStatementHandle);
          if (transactionFlag)
          {
            (void)Database_rollbackTransaction(toDatabaseHandle);
          }
          return error;
        }
      }

      // create SQL insert statement string
      formatSQLString(String_clear(sqlInsertString),"INSERT INTO %s (",toTableName);
      i = 0;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        if (columnNode->usedFlag && (columnNode->type != DATABASE_DATATYPE_PRIMARY_KEY))
        {
          if (i > 0) String_appendChar(sqlInsertString,',');
          String_appendCString(sqlInsertString,columnNode->name);
          i++;
        }
      }
      String_appendCString(sqlInsertString,") VALUES (");
      i = 0;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        if (columnNode->usedFlag && (columnNode->type != DATABASE_DATATYPE_PRIMARY_KEY))
        {
          if (i > 0) String_appendChar(sqlInsertString,',');
          String_appendChar(sqlInsertString,'?');
          i++;
        }
      }
      String_appendChar(sqlInsertString,')');
//      DATABASE_DEBUG_SQL(toDatabaseHandle,sqlInsertString);

      // create insert statement
      sqliteResult = sqlite3_prepare_v2(toDatabaseHandle->handle.sqlite,
                                        String_cString(sqlInsertString),
                                        -1,
                                        &toStatementHandle,
                                        NULL
                                       );
      if      (sqliteResult == SQLITE_OK)
      {
        // nothing to do
      }
      else if (sqliteResult == SQLITE_MISUSE)
      {
        HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(toDatabaseHandle->handle.sqlite));
      }
      else if (sqliteResult == SQLITE_INTERRUPT)
      {
        error = ERRORX_(INTERRUPTED,sqlite3_errcode(toDatabaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle.sqlite),String_cString(sqlInsertString));
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }
      else
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle.sqlite),String_cString(sqlInsertString));
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }
      #ifndef NDEBUG
        if (toStatementHandle == NULL)
        {
          fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(toDatabaseHandle->handle.sqlite),sqlite3_errmsg(toDatabaseHandle->handle.sqlite),String_cString(sqlInsertString));
          abort();
        }
      #endif /* not NDEBUG */

      // set to value
      i = 1;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        if (columnNode->usedFlag)
        {
          switch (columnNode->type)
          {
            case DATABASE_DATATYPE_PRIMARY_KEY:
              // can not be set
              break;
            case DATABASE_DATATYPE_INT64:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.i),-1,NULL);
              i++;
              break;
            case DATABASE_DATATYPE_DOUBLE:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.d),-1,NULL);
              i++;
              break;
            case DATABASE_DATATYPE_DATETIME:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.i),-1,NULL);
              i++;
              break;
            case DATABASE_DATATYPE_TEXT:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.text),-1,NULL);
              i++;
              break;
            case DATABASE_DATATYPE_BLOB:
              i++;
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX("to column '%s'",columnNode->name);
              #endif /* not NDEBUG */
              break; // not reached
          }
        }
      }

      // insert row
      if (sqliteStep(toDatabaseHandle->handle.sqlite,toStatementHandle,toDatabaseHandle->timeout) != SQLITE_DONE)
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle.sqlite),String_cString(sqlInsertString));
        sqlite3_finalize(toStatementHandle);
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }
      lastRowId = (DatabaseId)sqlite3_last_insert_rowid(toDatabaseHandle->handle.sqlite);
      LIST_ITERATE(&toColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_DATATYPE_PRIMARY_KEY:
            columnNode->value.id = lastRowId;
            break;
          case DATABASE_DATATYPE_INT64:
          case DATABASE_DATATYPE_DOUBLE:
          case DATABASE_DATATYPE_DATETIME:
          case DATABASE_DATATYPE_TEXT:
          case DATABASE_DATATYPE_BLOB:
          case DATABASE_DATATYPE_UNKNOWN:
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX("to column '%s'",columnNode->name);
            #endif /* not NDEBUG */
            break; // not reached
        }
      }

      // free insert statement
      sqlite3_finalize(toStatementHandle);

      // call post-copy callback (if defined)
      if (postCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER),
        {
          return postCopyTableFunction(&fromColumnList,&toColumnList,postCopyTableUserData);
        });
        if (error != ERROR_NONE)
        {
          sqlite3_finalize(fromStatementHandle);
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
            sqlite3_finalize(fromStatementHandle);
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
            sqlite3_finalize(fromStatementHandle);
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
              sqlite3_finalize(fromStatementHandle);
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
              sqlite3_finalize(fromStatementHandle);
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
        sqlite3_finalize(fromStatementHandle);
        return error;
      }
    }

    END_TIMER();

    // free resources
    sqlite3_finalize(fromStatementHandle);

    return ERROR_NONE;
  });
  String_delete(sqlInsertString);

//fprintf(stderr,"%s, %d: -------------------------- do check\n",__FILE__,__LINE__);
//sqlite3_wal_checkpoint_v2(toDatabaseHandle->handle,NULL,SQLITE_CHECKPOINT_FULL,&a,&b);
//fprintf(stderr,"%s, %d: checkpoint a=%d b=%d r=%d: %s\n",__FILE__,__LINE__,a,b,r,sqlite3_errmsg(toDatabaseHandle->handle));

  // free resources
  String_delete(sqlSelectString);
  freeTableColumnList(&toColumnList);
  freeTableColumnList(&fromColumnList);

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

DatabaseId Database_getTableColumnListId(const DatabaseColumnList *columnList, const char *columnName, DatabaseId defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_DATATYPE_INT64));
    if (columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY)
    {
      return columnNode->value.id;
    }
    else
    {
      return (DatabaseId)String_toInteger64(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
    }
  }
  else
  {
    return defaultValue;
  }
}

int Database_getTableColumnListInt(const DatabaseColumnList *columnList, const char *columnName, int defaultValue)
{
  DatabaseColumnNode *columnNode;

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_DATATYPE_INT64));
    if (columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY)
    {
      return columnNode->value.id;
    }
    else
    {
      return String_toInteger(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
    }
  }
  else
  {
    return defaultValue;
  }
}

uint Database_getTableColumnListUInt(const DatabaseColumnList *columnList, const char *columnName, uint defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_DATATYPE_INT64));
    if (columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY)
    {
      return columnNode->value.id;
    }
    else
    {
      return (uint)String_toInteger(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
    }
  }
  else
  {
    return defaultValue;
  }
}

int64 Database_getTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_DATATYPE_INT64));
    if (columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY)
    {
      return columnNode->value.id;
    }
    else
    {
      return String_toInteger64(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
    }
  }
  else
  {
    return defaultValue;
  }
}

uint64 Database_getTableColumnListUInt64(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_DATATYPE_INT64));
    if (columnNode->type == DATABASE_DATATYPE_PRIMARY_KEY)
    {
      return columnNode->value.id;
    }
    else
    {
      return (uint64)String_toInteger64(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
    }
  }
  else
  {
    return defaultValue;
  }
}

double Database_getTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_DOUBLE);
    return String_toDouble(columnNode->value.d,STRING_BEGIN,NULL,NULL,0);
  }
  else
  {
    return defaultValue;
  }
}

uint64 Database_getTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_DATETIME);
    return String_toInteger64(columnNode->value.i,STRING_BEGIN,NULL,NULL,0);
  }
  else
  {
    return defaultValue;
  }
}

String Database_getTableColumnList(const DatabaseColumnList *columnList, const char *columnName, String value, const char *defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_TEXT);
    return String_set(value,columnNode->value.text);
  }
  else
  {
    return String_setCString(value,defaultValue);
  }

  return value;
}

const char *Database_getTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *defaultValue)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_TEXT);
    return String_cString(columnNode->value.text);
  }
  else
  {
    return defaultValue;
  }
}

void Database_getTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, void *data, uint length)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

UNUSED_VARIABLE(data);
UNUSED_VARIABLE(length);
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_BLOB);
//    return columnNode->value.blob.data;
  }
  else
  {
//    return data;
  }
}

bool Database_setTableColumnListId(const DatabaseColumnList *columnList, const char *columnName, DatabaseId value)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_INT64);
    String_format(columnNode->value.i,"%ld",value);
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 value)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_INT64);
    String_format(columnNode->value.i,"%"PRIi64,value);
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double value)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_DOUBLE);
    String_format(columnNode->value.d,"%f",value);
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 value)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_DATETIME);
    String_format(columnNode->value.i,"%"PRIi64,value);
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnList(const DatabaseColumnList *columnList, const char *columnName, ConstString value)
{
  assert(columnList != NULL);
  assert(columnName != NULL);

  return Database_setTableColumnListCString(columnList,columnName,String_cString(value));
}

bool Database_setTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *value)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_TEXT);
    String_setCString(columnNode->value.text,value);
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, const void *data, uint length)
{
  DatabaseColumnNode *columnNode;

  assert(columnList != NULL);
  assert(columnName != NULL);

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_DATATYPE_BLOB);
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
UNUSED_VARIABLE(data);
UNUSED_VARIABLE(length);
//    columnNode->value.blob.data   = data;
//    columnNode->value.blob.length = length;
    columnNode->value.blob.data   = NULL;
    columnNode->value.blob.length = 0;
    columnNode->usedFlag = TRUE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Database_addColumn(DatabaseHandle *databaseHandle,
                          const char     *tableName,
                          const char     *columnName,
                          DatabaseDataTypes  columnType
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
  switch (columnType)
  {
    case DATABASE_DATATYPE_PRIMARY_KEY:
      columnTypeString = "INTEGER PRIMARY KEY";
      break;
    case DATABASE_DATATYPE_FOREIGN_KEY:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_DATATYPE_INT64:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_DATATYPE_DOUBLE:
      columnTypeString = "REAL DEFAULT 0.0";
      break;
    case DATABASE_DATATYPE_DATETIME:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_DATATYPE_TEXT:
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
                           "ALTER TABLE %s ADD COLUMN %s %s; \
                           ",
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
  Errors                   error;
  DatabaseColumnList       columnList;
  const DatabaseColumnNode *columnNode;
  String                   sqlString,value;
  sqlite3_stmt             *statementHandle;
  int                      sqliteResult;
  uint                     n;
  uint                     column;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
// TODO: remove
  assert(Thread_isCurrentThread(databaseHandle->debug.threadId));
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // get table columns
  error = getTableColumnList(&columnList,databaseHandle,tableName);
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
    LIST_ITERATE(&columnList,columnNode)
    {
      if (!stringEquals(columnNode->name,columnName))
      {
        if (n > 0) String_appendChar(sqlString,',');

        formatSQLString(sqlString,"%s %s",columnNode->name,getDatabaseTypeString(columnNode->type));
        n++;
      }
    }
    String_appendCString(sqlString,");");

    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    DATABASE_DOX(error,
                 ERRORX_(DATABASE_TIMEOUT,0,""),
                 databaseHandle,
                 DATABASE_LOCK_TYPE_READ_WRITE,
                 WAIT_FOREVER,
    {
      return databaseExecute(databaseHandle,
                             String_cString(sqlString),
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             databaseHandle->timeout
                            );
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // copy old table -> new table
    formatSQLString(String_clear(sqlString),"SELECT * FROM %s;",tableName);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      // nothing to do
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),sqlString);
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    // copy table rows
    while (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      // create SQL command string
      String_setCString(sqlString,"INSERT INTO __new__ (");
      n = 0;
      LIST_ITERATE(&columnList,columnNode)
      {
        if (!stringEquals(columnNode->name,columnName))
        {
          if (n > 0) String_appendChar(sqlString,',');

          String_appendCString(sqlString,columnNode->name);
          n++;
        }
      }
      String_appendCString(sqlString,")");

      String_appendCString(sqlString," VALUES (");
      column = 0;
      n = 0;
      LIST_ITERATE(&columnList,columnNode)
      {
        if (!stringEquals(columnNode->name,columnName))
        {
          if (n > 0) String_appendChar(sqlString,',');

          switch (columnNode->type)
          {
            case DATABASE_DATATYPE_PRIMARY_KEY:
            case DATABASE_DATATYPE_INT64:
            case DATABASE_DATATYPE_DOUBLE:
            case DATABASE_DATATYPE_DATETIME:
            case DATABASE_DATATYPE_TEXT:
              formatSQLString(sqlString,"%'s",sqlite3_column_text(statementHandle,column));
              break;
            case DATABASE_DATATYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* not NDEBUG */
              break; // not reached
          }
          n++;
        }

        column++;
      }
      String_appendCString(sqlString,");");

      // execute SQL command
      DATABASE_DEBUG_SQL(databaseHandle,sqlString);
      DATABASE_DOX(error,
                   ERRORX_(DATABASE_TIMEOUT,0,""),
                   databaseHandle,
                   DATABASE_LOCK_TYPE_READ_WRITE,
                   WAIT_FOREVER,
      {
        return databaseExecute(databaseHandle,
                               String_cString(sqlString),
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               databaseHandle->timeout
                              );
      });
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // done table
    sqlite3_finalize(statementHandle);

    return ERROR_NONE;
  });
  String_delete(value);
  String_delete(sqlString);

  // free resources
  freeTableColumnList(&columnList);

  // rename tables
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE %s RENAME TO __old__;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE __new__ RENAME TO %s;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE __old__ RENAME TO %s;",
                           tableName
                          );
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DROP TABLE __old__;"
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
  assert(databaseHandle->handle.sqlite != NULL);

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

    // format SQL command string
    sqlString = String_new();
    switch (databaseTransactionType)
    {
      case DATABASE_TRANSACTION_TYPE_DEFERRED : String_format(sqlString,"BEGIN DEFERRED TRANSACTION;");  break;
      case DATABASE_TRANSACTION_TYPE_IMMEDIATE: String_format(sqlString,"BEGIN IMMEDIATE TRANSACTION;"); break;
      case DATABASE_TRANSACTION_TYPE_EXCLUSIVE: String_format(sqlString,"BEGIN EXCLUSIVE TRANSACTION;"); break;
    }

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
    error = databaseExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
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
  assert(databaseHandle->handle.sqlite != NULL);

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
    sqlString = String_format(String_new(),"END TRANSACTION;");

    // end transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = databaseExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
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

    // try to execute checkpoint
    executeCheckpoint(databaseHandle);
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    UNUSED_VARIABLE(databaseHandle);
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */
//fprintf(stderr,"%s, %d: Database_endTransaction\n",__FILE__,__LINE__);

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
  assert(databaseHandle->handle.sqlite != NULL);

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
    error = databaseExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
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
  assert(databaseHandle->handle.sqlite != NULL);

  sqlite3_wal_checkpoint(databaseHandle->handle.sqlite,NULL);

  return ERROR_NONE;
}

Errors Database_execute(DatabaseHandle      *databaseHandle,
                        DatabaseRowFunction databaseRowFunction,
                        void                *databaseRowUserData,
                        ulong               *changedRowCount,
                        const char          *command,
                        ...
                       )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(command != NULL);

  va_start(arguments,command);
  {
    error = Database_vexecute(databaseHandle,
                              databaseRowFunction,
                              databaseRowUserData,
                              changedRowCount,
                              command,
                              arguments
                             );
  }
  va_end(arguments);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Database_vexecute(DatabaseHandle      *databaseHandle,
                         DatabaseRowFunction databaseRowFunction,
                         void                *databaseRowUserData,
                         ulong               *changedRowCount,
                         const char          *command,
                         va_list             arguments
                        )
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(command != NULL);

  // format SQL command string
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );

  // execute SQL command
  DATABASE_DEBUG_SQL(databaseHandle,sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    return databaseExecute(databaseHandle,
                           String_cString(sqlString),
                           CALLBACK_(databaseRowFunction,databaseRowUserData),
                           changedRowCount,
                           databaseHandle->timeout
                          );
  });
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_prepare(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseHandle      *databaseHandle,
                          const char          *command,
                          ...
                         )
#else /* not NDEBUG */
  Errors __Database_prepare(const char          *__fileName__,
                            ulong               __lineNb__,
                            DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseHandle      *databaseHandle,
                            const char          *command,
                            ...
                           )
#endif /* NDEBUG */
{
  String  sqlString;
  va_list arguments;
  Errors  error;
  int     sqliteResult;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(command != NULL);

  // initialize variables
  databaseQueryHandle->databaseHandle = databaseHandle;

  // format SQL command string
  va_start(arguments,command);
  {
    sqlString = vformatSQLString(String_new(),
                                 command,
                                 arguments
                                );
  }
  va_end(arguments);
  #ifndef NDEBUG
    databaseQueryHandle->sqlString = String_duplicate(sqlString);
    databaseQueryHandle->dt        = 0LL;
  #endif /* not NDEBUG */

  // lock
  #ifndef NDEBUG
    if (!__Database_lock(__fileName__,__lineNb__,databaseHandle,DATABASE_LOCK_TYPE_READ,databaseHandle->timeout))
  #else /* NDEBUG */
    if (!Database_lock(databaseHandle,DATABASE_LOCK_TYPE_READ,databaseHandle->timeout))
  #endif /* not NDEBUG */
  {
    #ifndef NDEBUG
      String_delete(databaseQueryHandle->sqlString);
    #endif /* not NDEBUG */
    String_delete(sqlString);
    return ERRORX_(DATABASE_TIMEOUT,0,"");
  }

  // prepare SQL command execution
  DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString);

  #ifndef NDEBUG
    String_set(databaseHandle->debug.current.sqlCommand,sqlString);
  #endif /* not NDEBUG */

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  {
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &databaseQueryHandle->statementHandle,
                                      NULL
                                     );
  }
  DATABASE_DEBUG_TIME_END(databaseQueryHandle);
  if      (sqliteResult == SQLITE_OK)
  {
    error = ERROR_NONE;
  }
  else if (sqliteResult == SQLITE_MISUSE)
  {
    HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
  }
  else if (sqliteResult == SQLITE_INTERRUPT)
  {
    error = ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),sqlString);
    Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ);
    #ifndef NDEBUG
      String_delete(databaseQueryHandle->sqlString);
    #endif /* not NDEBUG */
    String_delete(sqlString);
    return error;
  }
  else
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    Database_unlock(databaseHandle,DATABASE_LOCK_TYPE_READ);
    #ifndef NDEBUG
      String_delete(databaseQueryHandle->sqlString);
    #endif /* not NDEBUG */
    String_delete(sqlString);
    return error;
  }
  #ifndef NDEBUG
    if (databaseQueryHandle->statementHandle == NULL)
    {
      fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
      abort();
    }
  #endif /* not NDEBUG */

  // free resources
  String_delete(sqlString);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseQueryHandle,DatabaseQueryHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseQueryHandle,DatabaseQueryHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

bool Database_getNextRow(DatabaseQueryHandle *databaseQueryHandle,
                         const char          *format,
                         ...
                        )
{
  bool    result;
  uint    column;
  va_list arguments;
  bool    longFlag,longLongFlag;
  bool    quoteFlag;
  int     maxLength;
  union
  {
    bool               *b;
    int                *i;
    uint               *ui;
    long               *l;
    ulong              *ul;
    long long          *ll;
    unsigned long long *ull;
    float              *f;
    double             *d;
    char               *ch;
    char               *s;
    void               **p;
    String string;
  }       value;

  assert(databaseQueryHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseQueryHandle);
  assert(databaseQueryHandle->databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseQueryHandle->databaseHandle);
  assert(databaseQueryHandle->databaseHandle->handle.sqlite != NULL);
  assert(format != NULL);

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  if (sqliteStep(databaseQueryHandle->databaseHandle->handle.sqlite,databaseQueryHandle->statementHandle,databaseQueryHandle->databaseHandle->timeout) == SQLITE_ROW)
  {
    // get data
    va_start(arguments,format);
    column = 0;
    while ((*format) != '\0')
    {
      // find next format specifier
      while (((*format) != '\0') && ((*format) != '%'))
      {
        format++;
      }

      if ((*format) == '%')
      {
        format++;

        // skip align specifier
        if (    ((*format) != '\0')
             && (   ((*format) == '-')
                 || ((*format) == '-')
                )
           )
        {
          format++;
        }

        // get length specifier
        maxLength = -1;
        if (    ((*format) != '\0')
             && isdigit(*format)
           )
        {
          maxLength = 0;
          while (   ((*format) != '\0')
                 && isdigit(*format)
                )
          {
            maxLength = maxLength*10+(uint)((*format)-'0');
            format++;
          }
        }

        // check for longlong/long flag
        longLongFlag = FALSE;
        longFlag     = FALSE;
        if ((*format) == 'l')
        {
          format++;
          if ((*format) == 'l')
          {
            format++;
            longLongFlag = TRUE;
          }
          else
          {
            longFlag = TRUE;
          }
        }

        // quoting flag (ignore quote char)
        if (   ((*format) != '\0')
            && !isalpha(*format)
            && ((*format) != '%')
            && (   ((*(format+1)) == 's')
                || ((*(format+1)) == 'S')
               )
           )
        {
          quoteFlag = TRUE;
          format++;
        }
        else
        {
          quoteFlag = FALSE;
        }
        UNUSED_VARIABLE(quoteFlag);

        // handle format type
        switch (*format)
        {
          case 'b':
            // bool
            format++;

            value.b = va_arg(arguments,bool*);
            if (value.b != NULL)
            {
              (*value.b) = (sqlite3_column_int(databaseQueryHandle->statementHandle,column) == 1);
            }
            break;
          case 'd':
            // integer
            format++;

            if      (longLongFlag)
            {
              value.ll = va_arg(arguments,long long*);
              if (value.ll != NULL)
              {
                (*value.ll) = (int64)sqlite3_column_int64(databaseQueryHandle->statementHandle,column);
              }
            }
            else if (longFlag)
            {
              value.l = va_arg(arguments,long*);
              if (value.l != NULL)
              {
                (*value.l) = (long)sqlite3_column_int64(databaseQueryHandle->statementHandle,column);
              }
            }
            else
            {
              value.i = va_arg(arguments,int*);
              if (value.i != NULL)
              {
                (*value.i) = sqlite3_column_int(databaseQueryHandle->statementHandle,column);
              }
            }
            break;
          case 'u':
            // unsigned integer
            format++;

            if      (longLongFlag)
            {
              value.ull = va_arg(arguments,unsigned long long*);
              if (value.ull != NULL)
              {
                (*value.ull) = (uint64)sqlite3_column_int64(databaseQueryHandle->statementHandle,column);
              }
            }
            else if (longFlag)
            {
              value.ul = va_arg(arguments,ulong*);
              if (value.ul != NULL)
              {
                (*value.ul) = (ulong)sqlite3_column_int64(databaseQueryHandle->statementHandle,column);
              }
            }
            else
            {
              value.ui = va_arg(arguments,uint*);
              if (value.ui != NULL)
              {
                (*value.ui) = (uint)sqlite3_column_int(databaseQueryHandle->statementHandle,column);
              }
            }
            break;
          case 'f':
            // float/double
            format++;

            if (longFlag)
            {
              value.d = va_arg(arguments,double*);
              if (value.d != NULL)
              {
                (*value.d) = atof((const char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column));
              }
            }
            else
            {
              value.f = va_arg(arguments,float*);
              if (value.f != NULL)
              {
                (*value.f) = (float)atof((const char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column));
              }
            }
            break;
          case 'c':
            // char
            format++;

            value.ch = va_arg(arguments,char*);
            if (value.ch != NULL)
            {
              (*value.ch) = ((char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column))[0];
            }
            break;
          case 's':
            // C string
            format++;

            value.s = va_arg(arguments,char*);
            if (value.s != NULL)
            {
              if (maxLength >= 0)
              {
                strncpy(value.s,(const char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column),maxLength-1);
                value.s[maxLength-1] = '\0';
              }
              else
              {
                strcpy(value.s,(const char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column));
              }
            }
            break;
          case 'S':
            // string
            format++;

            value.string = va_arg(arguments,String);
            if (value.string != NULL)
            {
              String_setCString(value.string,(const char*)sqlite3_column_text(databaseQueryHandle->statementHandle,column));
            }
            break;
          case 'p':
            // text via pointer
            format++;

            value.p = va_arg(arguments,void*);
            if (value.p != NULL)
            {
              (*value.p) = (void*)sqlite3_column_text(databaseQueryHandle->statementHandle,column);
            }
            break;
          default:
            return FALSE;
            break; /* not reached */
        }

        column++;
      }
    }
    va_end(arguments);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  DATABASE_DEBUG_TIME_END(databaseQueryHandle);

  return result;
}

#ifdef NDEBUG
  void Database_finalize(DatabaseQueryHandle *databaseQueryHandle)
#else /* not NDEBUG */
  void __Database_finalize(const char        *__fileName__,
                           ulong             __lineNb__,
                           DatabaseQueryHandle *databaseQueryHandle
                          )
#endif /* NDEBUG */
{
  assert(databaseQueryHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseQueryHandle);
  assert(databaseQueryHandle->databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseQueryHandle->databaseHandle);
  assert(databaseQueryHandle->databaseHandle->handle.sqlite != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseQueryHandle,DatabaseQueryHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseQueryHandle,DatabaseQueryHandle);
  #endif /* NDEBUG */

  #ifndef NDEBUG
    String_clear(databaseQueryHandle->databaseHandle->debug.current.sqlCommand);
    #ifdef HAVE_BACKTRACE
      databaseQueryHandle->databaseHandle->debug.current.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  {
    sqlite3_finalize(databaseQueryHandle->statementHandle);
  }
  DATABASE_DEBUG_TIME_END(databaseQueryHandle);
  #ifndef NDEBUG
    DATABASE_DEBUG_TIME(databaseQueryHandle);
  #endif /* not NDEBUG */

  // unlock
  #ifndef NDEBUG
    __Database_unlock(__fileName__,__lineNb__,databaseQueryHandle->databaseHandle,DATABASE_LOCK_TYPE_READ);
  #else /* NDEBUG */
    Database_unlock(databaseQueryHandle->databaseHandle,DATABASE_LOCK_TYPE_READ);
  #endif /* not NDEBUG */

  // free resources
  #ifndef NDEBUG
    String_delete(databaseQueryHandle->sqlString);
  #endif /* not NDEBUG */
}

bool Database_exists(DatabaseHandle *databaseHandle,
                     const char     *tableName,
                     const char     *columnName,
                     const char     *additional,
                     ...
                    )
{
  bool         existsFlag;
  String       sqlString;
  va_list      arguments;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  existsFlag = FALSE;

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
  DATABASE_DOX(existsFlag,
               FALSE,
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    existsFlag = FALSE;

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
      {
        existsFlag = TRUE;
      }
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else
    {
      // nothing to do
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return existsFlag;
  });

  // free resources
  String_delete(sqlString);

  return existsFlag;
}

Errors Database_getId(DatabaseHandle *databaseHandle,
                      DatabaseId     *value,
                      const char     *tableName,
                      const char     *columnName,
                      const char     *additional,
                      ...
                     )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);

  va_start(arguments,additional);
  error = Database_vgetId(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetId(DatabaseHandle *databaseHandle,
                       DatabaseId     *value,
                       const char     *tableName,
                       const char     *columnName,
                       const char     *additional,
                       va_list        arguments
                      )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);

  // init variables
  (*value) = DATABASE_ID_NONE;

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get id",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (DatabaseId)sqlite3_column_int64(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

  return error;
}

Errors Database_getIds(DatabaseHandle *databaseHandle,
                       Array          *values,
                       const char     *tableName,
                       const char     *columnName,
                       const char     *additional,
                       ...
                      )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(values != NULL);
  assert(tableName != NULL);

  va_start(arguments,additional);
  error = Database_vgetIds(databaseHandle,values,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetIds(DatabaseHandle *databaseHandle,
                        Array          *values,
                        const char     *tableName,
                        const char     *columnName,
                        const char     *additional,
                        va_list        arguments
                       )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;
  DatabaseId   value;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(values != NULL);
  assert(tableName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT DISTINCT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get id",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    while (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      value = (DatabaseId)sqlite3_column_int64(statementHandle,0);
      Array_append(values,&value);
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

  return error;
}

Errors Database_getMaxId(DatabaseHandle *databaseHandle,
                         DatabaseId     *value,
                         const char     *tableName,
                         const char     *columnName,
                         const char     *additional,
                         ...
                        )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);

  va_start(arguments,additional);
  error = Database_vgetMaxId(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetMaxId(DatabaseHandle *databaseHandle,
                          DatabaseId     *value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          va_list        arguments
                         )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);

  // init variables
  (*value) = DATABASE_ID_NONE;

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  formatSQLString(sqlString,
                  " ORDER BY %s DESC LIMIT 0,1",
                  columnName
                 );
fprintf(stderr,"%s, %d: sqlString=%s\n",__FILE__,__LINE__,String_cString(sqlString));

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get max. id",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (DatabaseId)sqlite3_column_int64(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

  return error;
}

Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vgetInteger64(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetInteger64(DatabaseHandle *databaseHandle,
                              int64          *value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  String_appendCString(sqlString," LIMIT 0,1");
fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(sqlString));

#if 0
  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (int64)sqlite3_column_int64(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });
#else
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
  error = databaseExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
#endif

  // free resources
  String_delete(sqlString);
fprintf(stderr,"%s:%d: error=%s\n",__FILE__,__LINE__,Error_getText(error));
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__); asm("int3");

  return error;
}

Errors Database_setInteger64(DatabaseHandle *databaseHandle,
                             int64          value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vsetInteger64(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vsetInteger64(DatabaseHandle *databaseHandle,
                              int64          value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             )
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  sqlString = String_new();

  // try update
  String_clear(sqlString);
  formatSQLString(sqlString,
                  "UPDATE %s \
                   SET %s=%ld \
                  ",
                  tableName,
                  columnName,
                  value
                 );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    error = databaseExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
                           );
    if (error != ERROR_NONE)
    {
      // insert
      String_clear(sqlString);
      formatSQLString(sqlString,
                      "INSERT INTO %s \
                       (%s) VALUES (%ld) \
                      ",
                      tableName,
                      columnName,
                      value
                     );
      DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
      error = databaseExecute(databaseHandle,
                              String_cString(sqlString),
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              databaseHandle->timeout
                             );
    }

    return error;
  });
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_getDouble(DatabaseHandle *databaseHandle,
                          double         *value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vgetDouble(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetDouble(DatabaseHandle *databaseHandle,
                           double         *value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(value != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get double",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = sqlite3_column_double(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

  return error;
}

Errors Database_setDouble(DatabaseHandle *databaseHandle,
                          double         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vsetDouble(databaseHandle,value,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vsetDouble(DatabaseHandle *databaseHandle,
                           double         value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          )
{
  String  sqlString;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  sqlString = String_new();

  // try update
  String_clear(sqlString);
  formatSQLString(sqlString,
                  "UPDATE %s \
                   SET %s=%lf \
                  ",
                  tableName,
                  columnName,
                  value
                 );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  DATABASE_DEBUG_SQLX(databaseHandle,"set double",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    error = databaseExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK_(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
                           );
    if (error != ERROR_NONE)
    {
      // insert
      String_clear(sqlString);
      formatSQLString(sqlString,
                      "INSERT INTO %s \
                       (%s) VALUES (%lf) \
                      ",
                      tableName,
                      columnName,
                      value
                     );
      DATABASE_DEBUG_SQLX(databaseHandle,"set double",sqlString);
      error = databaseExecute(databaseHandle,
                              String_cString(sqlString),
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              databaseHandle->timeout
                             );
    }

    return error;
  });
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_getString(DatabaseHandle *databaseHandle,
                          String         string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vgetString(databaseHandle,string,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vgetString(DatabaseHandle *databaseHandle,
                           String         string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          )
{
  String       sqlString;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT %s \
                               FROM %s \
                              ",
                              columnName,
                              tableName
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"get string",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
               databaseHandle->timeout,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle.sqlite,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle.sqlite));
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      return ERRORX_(INTERRUPTED,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle.sqlite),"%s: %s",sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle.sqlite),sqlite3_errmsg(databaseHandle->handle.sqlite),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle.sqlite,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      String_setCString(string,(const char*)sqlite3_column_text(statementHandle,0));
    }
    sqlite3_finalize(statementHandle);

    #ifndef NDEBUG
      String_set(databaseHandle->debug.current.sqlCommand,sqlString);
    #endif /* not NDEBUG */

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

  return error;
}

Errors Database_setString(DatabaseHandle *databaseHandle,
                          const String   string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  va_start(arguments,additional);
  error = Database_vsetString(databaseHandle,string,tableName,columnName,additional,arguments);
  va_end(arguments);

  return error;
}

Errors Database_vsetString(DatabaseHandle *databaseHandle,
                           const String   string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          )
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "UPDATE %s \
                               SET %s=%'S \
                              ",
                              tableName,
                              columnName,
                              string
                             );
  if (additional != NULL)
  {
    String_appendChar(sqlString,' ');
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
  }

  // execute SQL command
  DATABASE_DEBUG_SQLX(databaseHandle,"set string",sqlString);
  DATABASE_DOX(error,
               ERRORX_(DATABASE_TIMEOUT,0,""),
               databaseHandle,
               DATABASE_LOCK_TYPE_READ_WRITE,
               databaseHandle->timeout,
  {
    return databaseExecute(databaseHandle,
                           String_cString(sqlString),
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           databaseHandle->timeout
                          );
  });
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

DatabaseId Database_getLastRowId(DatabaseHandle *databaseHandle)
{
  DatabaseId databaseId;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle.sqlite != NULL);

  databaseId = (DatabaseId)sqlite3_last_insert_rowid(databaseHandle->handle.sqlite);

  return databaseId;
}

#ifdef DATABASE_DEBUG_LOCK
void Database_debugPrintSimpleLockInfo(void)
{
  const DatabaseNode *databaseNode;
  uint  i;

  // Note: debug only, no locking
  LIST_ITERATE(&databaseList,databaseNode)
  {
    switch (databaseNode->type)
    {
      case DATABASE_TYPE_SQLITE3:
        printf("Database: %s\n",String_cString(databaseNode->sqlite.fileName));
        break;
      case DATABASE_TYPE_MYSQL:
// TODO:
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
/*
//TODO
sqlite3_exec(databaseHandle->handle.sqlite,
                              "PRAGMA vdbe_trace=ON",
                              CALLBACK_(NULL,NULL),
                              NULL
                             );
*/
  }
  else
  {
    assert(databaseDebugCounter>0);

    databaseDebugCounter--;
if (databaseDebugCounter == 0) sqlite3_exec(databaseHandle->handle.sqlite,
                              "PRAGMA vdbe_trace=OFF",
                              CALLBACK_(NULL,NULL),
                              NULL
                             );

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
        fprintf(stderr,
                "  opened '%s': %u\n",
                String_cString(databaseNode->sqlite.fileName),
                databaseNode->openCount
               );
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
  assert(databaseHandle->databaseNode != NULL);

  pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

//TODO: use debugPrintLockInfo()?
  pthread_mutex_lock(&debugDatabaseLock);
  {
    pthread_mutex_lock(&debugConsoleLock);
    {
      fprintf(stderr,"Database lock info '%s':\n",String_cString(databaseHandle->databaseNode->sqlite.fileName));
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
                  String_cString(databaseHandle->debug.current.sqlCommand)
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
                  String_cString(databaseHandle->debug.current.sqlCommand)
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

void __Database_debugPrintQueryInfo(const char *__fileName__, ulong __lineNb__, const DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

//  DATABASE_DEBUG_SQLX(databaseQueryHandle->databaseHandle,"SQL query",databaseQueryHandle->sqlString);
  fprintf(stderr,"DEBUG database %s, %lu: %s: %s\n",__fileName__,__lineNb__,String_cString(databaseQueryHandle->databaseHandle->databaseNode->sqlite.fileName),String_cString(databaseQueryHandle->sqlString)); \
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
* Input  : columns - column names
*          count   - number of values
* Output : -
* Return : widths
* Notes  : -
\***********************************************************************/

LOCAL size_t* debugGetColumnsWidth(const char *columns[], uint count)
{
  size_t *widths;
  uint   i;

  assert(columns != NULL);

  widths = (size_t*)malloc(count*sizeof(size_t));
  assert(widths != NULL);

  for (i = 0; i < count; i++)
  {
    widths[i] = 0;
    if ((columns[i] != NULL) && (stringLength(columns[i]) > widths[i])) widths[i] = stringLength(columns[i]);
  }

  return widths;
}

/***********************************************************************\
* Name   : debugCalculateColumnWidths
* Purpose: calculate column width call back
* Input  : columns  - column names
*          values   - values
*          count    - number of values
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors debugCalculateColumnWidths(const char *columns[], const char *values[], uint count, void *userData)
{
  DumpTableData *dumpTableData = (DumpTableData*)userData;
  uint          i;

  assert(columns != NULL);
  assert(values != NULL);
  assert(dumpTableData != NULL);

  UNUSED_VARIABLE(userData);

  if (dumpTableData->widths == NULL) dumpTableData->widths = debugGetColumnsWidth(columns,count);
  assert(dumpTableData->widths != NULL);

  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      dumpTableData->widths[i] = MAX(stringLength(values[i]),dumpTableData->widths[i]);
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : debugPrintRow
* Purpose: print row call back
* Input  : columns  - column names
*          values   - values
*          count    - number of values
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors debugPrintRow(const char *columns[], const char *values[], uint count, void *userData)
{
  DumpTableData *dumpTableData = (DumpTableData*)userData;
  uint          i;

  assert(columns != NULL);
  assert(values != NULL);
  assert(dumpTableData != NULL);
  assert(dumpTableData->widths != NULL);

  UNUSED_VARIABLE(userData);

  if (dumpTableData->showHeaderFlag && !dumpTableData->headerPrintedFlag)
  {
    for (i = 0; i < count; i++)
    {
      printf("%s ",columns[i]); debugPrintSpaces(dumpTableData->widths[i]-stringLength(columns[i]));
    }
    printf("\n");

    dumpTableData->headerPrintedFlag = TRUE;
  }
  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      printf("%s ",!stringIsEmpty(values[i]) ? values[i] : "''"); if (dumpTableData->showHeaderFlag) { debugPrintSpaces(dumpTableData->widths[i]-(!stringIsEmpty(values[i]) ? stringLength(values[i]) : 2)); }
    }
    else
    {
      printf("- "); if (dumpTableData->showHeaderFlag) { debugPrintSpaces(dumpTableData->widths[i]-1); }
    }
  }
  printf("\n");

  return ERROR_NONE;
}

void Database_debugDumpTable(DatabaseHandle *databaseHandle, const char *tableName, bool showHeaderFlag)
{
  String     sqlString;
  DumpTableData dumpTableData;

  assert(databaseHandle != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "SELECT * FROM %s",
                              tableName
                             );

  // print table
  dumpTableData.showHeaderFlag    = showHeaderFlag;
  dumpTableData.headerPrintedFlag = FALSE;
  dumpTableData.widths            = NULL;
  Database_execute(databaseHandle,
                           CALLBACK_(debugCalculateColumnWidths,&dumpTableData),
                           NULL,  // changedRowCount
                           String_cString(sqlString)
                          );
  Database_execute(databaseHandle,
                           CALLBACK_(debugPrintRow,&dumpTableData),
                           NULL,  // changedRowCount
                           String_cString(sqlString)
                          );
  debugFreeColumnsWidth(dumpTableData.widths);

  // free resources
  String_delete(sqlString);
}

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
