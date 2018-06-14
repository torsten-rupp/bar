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

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "arrays.h"
#include "files.h"
#include "misc.h"
#include "semaphores.h"
#include "errors.h"

#include "sqlite3.h"

#include "database.h"

/****************** Conditional compilation switches *******************/
#define DATABASE_SUPPORT_TRANSACTIONS
#define DATABASE_SUPPORT_INTERRUPT
#define _DATABASE_DEBUG_COPY_TABLE

#define DATABASE_USE_ATOMIC_INCREMENT
#define _DATABASE_DEBUG_LOCK

#define DATABASE_SINGLE_LOCK

/***************************** Constants *******************************/
#define MAX_FORCE_CHECKPOINT_TIME (10LL*60LL*1000LL) // timeout for force execution of a checkpoint [ms]
//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

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

/***************************** Variables *******************************/

LOCAL DatabaseList databaseList;
#ifndef DATABASE_LOCK_PER_INSTANCE
  LOCAL pthread_mutex_t databaseLock;
#endif /* DATABASE_LOCK_PER_INSTANCE */

//TODO: remove
#if 0
LOCAL Semaphore          databaseRequestLock;
LOCAL DatabaseHandleList databaseRequestList;
LOCAL uint               databaseRequestHighestPiority;
uint transactionCount = 0;
#endif

#ifndef NDEBUG
  LOCAL uint                databaseDebugCounter = 0;

  LOCAL pthread_once_t      debugDatabaseInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugDatabaseLockAttribute;
  LOCAL pthread_mutex_t     debugDatabaseLock;
  LOCAL ThreadId            debugDatabaseThreadId;
  LOCAL DatabaseHandleList  debugDatabaseHandleList;
  LOCAL void                (*debugSignalQuitPrevHandler)(int);
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
        fprintf(stderr,"DEBUG database: execute command: %s: %s\n",(databaseHandle)->fileName,String_cString(sqlString)); \
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
        fprintf(stderr,"DEBUG database: " text ": %s: %s\n",(databaseHandle)->fileName,String_cString(sqlString)); \
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
        fprintf(stderr,"DEBUG database: query plan\n"); \
        sqlite3_exec(databaseHandle->handle, \
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
        fprintf(stderr,"DEBUG database: execution time=%llums\n",databaseQueryHandle->dt/1000ULL); \
      } \
    } \
    while (0)
#else /* not NDEBUG */
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
#endif /* not NDEBUG */

#ifdef DATABASE_LOCK_PER_INSTANCE
  #define DATABASE_LOCK(databaseHandle) \
    do \
    { \
      assert(databaseHandle->databaseNode != NULL); \
      \
      pthread_mutex_lock(databaseHandle->databaseNode->lock); \
    } \
    while (0)

  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
      assert(databaseHandle->databaseNode != NULL); \
      \
      pthread_mutex_unlock(databaseHandle->databaseNode->lock); \
    } \
    while (0)

  #define DATABASE_WAIT_TRIGGER(databaseHandle,trigger) \
    do \
    { \
      assert(databaseHandle->databaseNode != NULL); \
      \
      pthread_cond_wait(trigger,databaseHandle->databaseNode->lock); \
    } \
    while (0)
#else /* not DATABASE_LOCK_PER_INSTANCE */
  #define DATABASE_LOCK(databaseHandle) \
    do \
    { \
      UNUSED_VARIABLE(databaseHandle); \
      \
      pthread_mutex_lock(&databaseLock); \
    } \
    while (0)

  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
      UNUSED_VARIABLE(databaseHandle); \
      \
      pthread_mutex_unlock(&databaseLock); \
    } \
    while (0)

  #define DATABASE_WAIT_TRIGGER(databaseHandle,trigger) \
    do \
    { \
      UNUSED_VARIABLE(databaseHandle); \
      \
      pthread_cond_wait(trigger,&databaseLock); \
    } \
    while (0)
#endif /* DATABASE_LOCK_PER_INSTANCE */

/***********************************************************************\
* Name   : DATABASE_DO
* Purpose: database block-operation
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
*          block          - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define DATABASE_DO(databaseHandle,lockType,block) \
    do \
    { \
      __begin(__FILE__,__LINE__,databaseHandle,lockType); \
      ({ \
        auto void __closure__(void); \
        \
        void __closure__(void)block; __closure__; \
      })(); \
      __end(__FILE__,__LINE__,databaseHandle,lockType); \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_DO(databaseHandle,lockType,block) \
    do \
    { \
      begin(databaseHandle,lockType); \
      ({ \
        auto void __closure__(void); \
        \
        void __closure__(void)block; __closure__; \
      })(); \
      end(databaseHandle,lockType); \
    } \
    while (0)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : DATABASE_DOX
* Purpose: database block-operation
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
*          block          - code block
* Output : result - result
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define DATABASE_DOX(result,databaseHandle,lockType,block) \
    do \
    { \
      __begin(__FILE__,__LINE__,databaseHandle,lockType); \
      result = ({ \
                 auto typeof(result) __closure__(void); \
                 \
                 typeof(result) __closure__(void)block; __closure__; \
               })(); \
      __end(__FILE__,__LINE__,databaseHandle,lockType); \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_DOX(result,databaseHandle,lockType,block) \
    do \
    { \
      begin(databaseHandle,lockType); \
      result = ({ \
                 auto typeof(result) __closure__(void); \
                 \
                 typeof(result) __closure__(void)block; __closure__; \
               })(); \
      end(databaseHandle,lockType); \
    } \
    while (0)
#endif /* not NDEBUG */

#ifndef NDEBUG
  #define begin(...) __begin(__FILE__,__LINE__, ## __VA_ARGS__)
  #define end(...)   __end  (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

  List_done(&databaseNode->busyHandlerList,CALLBACK(NULL,NULL));
  pthread_cond_destroy(&databaseNode->readWriteTrigger);
  #ifdef DATABASE_LOCK_PER_INSTANCE
     pthread_mutex_destroy(&databaseNode->lock);
  #endif /* DATABASE_LOCK_PER_INSTANCE */
  String_delete(databaseNode->fileName);
}

/***********************************************************************\
* Name   : begin
* Purpose: begin database write operation
* Input  : databaseHandle - database handle
*          lockType       - lock type; see DATABASE_LOCK_TYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void begin(DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType)
#else /* not NDEBUG */
LOCAL_INLINE void __begin(const char *__fileName__, ulong __lineNb__, DatabaseHandle *databaseHandle, SemaphoreLockTypes lockType)
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifndef NDEBUG
    __Database_lock(__fileName__,__lineNb__,databaseHandle,lockType);
  #else /* NDEBUG */
    Database_lock(databaseHandle,lockType);
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    #ifdef HAVE_BACKTRACE
      databaseHandle->current.stackTraceSize = backtrace((void*)databaseHandle->current.stackTrace,SIZE_OF_ARRAY(databaseHandle->current.stackTrace));
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */
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
    // clear SQL command, backtrace
    String_clear(databaseHandle->current.sqlCommand);
    #ifdef HAVE_BACKTRACE
      databaseHandle->current.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    __Database_unlock(__fileName__,__lineNb__,databaseHandle,lockType);
  #else /* NDEBUG */
    Database_unlock(databaseHandle,lockType);
  #endif /* not NDEBUG */
}

#ifndef NDEBUG
//TODO
#ifndef WERROR
/***********************************************************************\
* Name   : debugPrintQueryPlanCallback
* Purpose: print query plan output
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

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
  pthread_mutexattr_init(&debugDatabaseLockAttribute);
  pthread_mutexattr_settype(&debugDatabaseLockAttribute,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&debugDatabaseLock,&debugDatabaseLockAttribute) != 0)
  {
    HALT_INTERNAL_ERROR("Cannot initialize database debug lock!");
  }

  // install signal handler for Ctrl-\ (SIGQUIT) for printing debug information
  debugSignalQuitPrevHandler = signal(SIGQUIT,debugDatabaseSignalHandler);
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : vformatSQLString
* Purpose: format SQL string from command and append
* Input  : sqlString - SQL string variable
*          command   - command string with %[l]d, %S, %s
*          arguments - optional argument list
* Output : -
* Return : SQL string
* Notes  : -
\***********************************************************************/

LOCAL String vformatSQLString(String     sqlString,
                              const char *command,
                              va_list    arguments
                             )
{
  const char *s;
  char       ch;
  bool       longFlag,longLongFlag;
  char       quoteFlag;
  Value      value;
  const char *t;
  ulong      i;

  assert(sqlString != NULL);
  assert(command != NULL);

  s = command;
  while ((ch = (*s)) != '\0')
  {
    switch (ch)
    {
      case '\\':
        // escaped character
        String_appendChar(sqlString,'\\');
        s++;
        if ((*s) != '\0')
        {
          String_appendChar(sqlString,*s);
          s++;
        }
        break;
      case '%':
        // format character
        s++;

        // check for longlong/long flag
        longLongFlag = FALSE;
        longFlag     = FALSE;
        if ((*s) == 'l')
        {
          s++;
          if ((*s) == 'l')
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
        if (   ((*s) != '\0')
            && !isalpha(*s)
            && ((*s) != '%')
            && (   ((*(s+1)) == 's')
                || ((*(s+1)) == 'S')
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

        // handle format type
        switch (*s)
        {
          case 'd':
            // integer
            s++;

            if      (longLongFlag)
            {
              value.ll = va_arg(arguments,int64);
              String_appendFormat(sqlString,"%lld",value.ll);
            }
            else if (longFlag)
            {
              value.l = va_arg(arguments,int64);
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
              value.ull = va_arg(arguments,uint64);
              String_appendFormat(sqlString,"%llu",value.ull);
            }
            else if (longFlag)
            {
              value.ul = va_arg(arguments,ulong);
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
              while ((ch = (*t)) != '\0')
              {
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
          case '%':
            // %%
            s++;

            String_appendChar(sqlString,'%');
            break;
          default:
            String_appendChar(sqlString,'%');
            String_appendChar(sqlString,*s);
            break;
        }
        break;
      default:
        String_appendChar(sqlString,ch);
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
  const char *text,*format;
  uint64     timestamp;
  char       *s;
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
    timestamp = strtol(text,&s,10);
    if ((*s) != '\0')
    {
      #ifdef HAVE_GETDATE_R
        tm = (getdate_r(text,&tmBuffer) == 0) ? &tmBuffer : NULL;
      #else /* not HAVE_GETDATE_R */
        tm = getdate(text);
      #endif /* HAVE_GETDATE_R */
      if (tm != NULL)
      {
        timestamp = (uint64)timegm(tm);
      }
      else
      {
        s = strptime(text,(format != NULL) ? format : "%Y-%m-%d %H:%M:%S",&tmBuffer);
        if ((s != NULL) && ((*s) == '\0'))
        {
          timestamp = (uint64)timegm(&tmBuffer);
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
    {
      (void)sqlite3_wal_checkpoint_v2(databaseHandle->handle,NULL,CHECKPOINT_MODE,NULL,NULL);
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

  // execute registered busy handler
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
* Name   : sqliteExecute
* Purpose: excute SQLite3 statement
* Input  : handle              - SQLite3 handle
*          sqlString           - SQL string
*          databaseRowFunction - row call-back function (can be NULL)
*          databaseRowUserData - user data for row call-back
*          changedRowCount     - number of changed rows (can be NULL)
*          timeout             - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sqliteExecute(DatabaseHandle      *databaseHandle,
                           const char          *sqlString,
                           DatabaseRowFunction databaseRowFunction,
                           void                *databaseRowUserData,
                           ulong               *changedRowCount,
                           long                timeout
                          )
{
  #define SLEEP_TIME 500L

  uint                          maxRetryCount;
  uint                          retryCount;
  const char                    *sqlCommand,*nextSqlCommand;
  Errors                        error;
  int                           sqliteResult;
  sqlite3_stmt                  *statementHandle;
  uint                          count;
  const char                    **names,**values;
  uint                          i;
  const DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->handle != NULL);

  if (changedRowCount != NULL) (*changedRowCount) = 0L;

  maxRetryCount = (timeout != WAIT_FOREVER) ? (uint)((timeout+SLEEP_TIME-1L)/SLEEP_TIME) : 0;
  sqlCommand    = stringTrimBegin(sqlString);
  error         = ERROR_NONE;
  retryCount    = 0;
  while (   (error == ERROR_NONE)
         && !stringIsEmpty(sqlCommand)
         && ((timeout == WAIT_FOREVER) || (retryCount <= maxRetryCount))
        )
  {
    DATABASE_DOX(error,
                 databaseHandle,
                 DATABASE_LOCK_TYPE_READ_WRITE,
    {
      assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy));
      #ifndef NDEBUG
        String_setCString(databaseHandle->current.sqlCommand,sqlString);
      #endif /* NDEBUG */

//fprintf(stderr,"%s, %d: sqlCommands='%s'\n",__FILE__,__LINE__,sqlCommand);
      // prepare SQL statement
      sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                        sqlCommand,
                                        -1,
                                        &statementHandle,
                                        &nextSqlCommand
                                       );
      if      (sqliteResult == SQLITE_MISUSE)
      {
        HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
      }
      else if (sqliteResult != SQLITE_OK)
      {
        return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),sqlString);
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
            waitUnlockNotify(databaseHandle->handle);
            sqlite3_reset(statementHandle);
          }
          else if (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
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
            error = databaseRowFunction(count,names,values,databaseRowUserData);
//TODO callback
          }
        }

        if (changedRowCount != NULL)
        {
          (*changedRowCount) += (ulong)sqlite3_changes(databaseHandle->handle);
        }
      }
      while ((error == ERROR_NONE) && (sqliteResult == SQLITE_ROW));

      // free call-back data
      if (databaseRowFunction != NULL)
      {
        free(values);
        free(names);
      }

      // done SQL statement
      sqlite3_finalize(statementHandle);

      return error;
    });

    // check result
    if      (sqliteResult == SQLITE_BUSY)
    {
//fprintf(stderr,"%s, %d: database busy %ld < %ld\n",__FILE__,__LINE__,retryCount*SLEEP_TIME,timeout);
      // execute registered busy handlers
//TODO: lock list?
      LIST_ITERATE(&databaseHandle->databaseNode->busyHandlerList,busyHandlerNode)
      {
        assert(busyHandlerNode->function != NULL);
        busyHandlerNode->function(busyHandlerNode->userData);
      }

      // retry
      retryCount++;
      continue;
    }
    else if (sqliteResult != SQLITE_DONE)
    {
      // report error
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),sqlString);
    }
    else if (sqliteResult == SQLITE_INTERRUPT)
    {
      // report interrupt
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),sqlString);
    }
    else
    {
      // next SQL command part
      sqlCommand = stringTrimBegin(nextSqlCommand);
    }
  }

  if      (error != ERROR_NONE)
  {
    return error;
  }
  else if (retryCount > maxRetryCount)
  {
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); asm("int3");
    return ERROR_DATABASE_TIMEOUT;
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
    case DATABASE_TYPE_PRIMARY_KEY:
      break;
    case DATABASE_TYPE_FOREIGN_KEY:
      break;
    case DATABASE_TYPE_INT64:
      String_delete(columnNode->value.i);
      break;
    case DATABASE_TYPE_DOUBLE:
      String_delete(columnNode->value.d);
      break;
    case DATABASE_TYPE_DATETIME:
      break;
    case DATABASE_TYPE_TEXT:
      String_delete(columnNode->value.text);
      break;
    case DATABASE_TYPE_BLOB:
      if (columnNode->value.blob.data != NULL)
      {
        free(columnNode->value.blob.data);
      }
      break;
    case DATABASE_TYPE_UNKNOWN:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
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

  DATABASE_DOX(error,
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
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

  DATABASE_DOX(error,
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
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
        List_done(columnList,CALLBACK((ListNodeFreeFunction)freeColumnNode,NULL));
        return ERROR_INSUFFICIENT_MEMORY;
      }

      columnNode->name = strdup(name);
      if (   stringEqualsIgnoreCase(type,"INTEGER")
          || stringEqualsIgnoreCase(type,"NUMERIC")
         )
      {
        if (primaryKey)
        {
          columnNode->type     = DATABASE_TYPE_PRIMARY_KEY;
          columnNode->value.id = 0LL;
        }
        else
        {
          columnNode->type    = DATABASE_TYPE_INT64;
          columnNode->value.i = String_new();
        }
      }
      else if (stringEqualsIgnoreCase(type,"REAL"))
      {
        columnNode->type    = DATABASE_TYPE_DOUBLE;
        columnNode->value.d = String_new();
      }
      else if (stringEqualsIgnoreCase(type,"TEXT"))
      {
        columnNode->type       = DATABASE_TYPE_TEXT;
        columnNode->value.text = String_new();
      }
      else if (stringEqualsIgnoreCase(type,"BLOB"))
      {
        columnNode->type              = DATABASE_TYPE_BLOB;
        columnNode->value.blob.data   = NULL;
        columnNode->value.blob.length = 0;
      }
      else
      {
        columnNode->type = DATABASE_TYPE_UNKNOWN;
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

  List_done(columnList,CALLBACK((ListNodeFreeFunction)freeColumnNode,NULL));
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

LOCAL const char *getDatabaseTypeString(DatabaseTypes type)
{
  const char *string;

  string = NULL;
  switch (type)
  {
    case DATABASE_TYPE_PRIMARY_KEY:
      string = "INTEGER PRIMARY KEY";
      break;
    case DATABASE_TYPE_INT64:
      string = "INTEGER";
      break;
    case DATABASE_TYPE_DOUBLE:
      string = "REAL";
      break;
    case DATABASE_TYPE_DATETIME:
      string = "INTEGER";
      break;
    case DATABASE_TYPE_TEXT:
      string = "TEXT";
      break;
    case DATABASE_TYPE_BLOB:
      string = "BLOB";
      break;
    case DATABASE_TYPE_UNKNOWN:
      string = "unknown";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; // not reached
  }

  return string;
}

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

  return (databaseHandle->databaseNode->readWriteCount > 0);
}

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

  return    (databaseHandle->databaseNode->readWriteCount > 0)
         && Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy);
}

/*---------------------------------------------------------------------*/

Errors Database_initAll(void)
{
  int sqliteResult;

  #ifndef DATABASE_LOCK_PER_INSTANCE
    // init global lock
    pthread_mutex_init(&databaseLock,NULL);
  #endif /* not DATABASE_LOCK_PER_INSTANCE */

  // init database list
  List_init(&databaseList);
  Semaphore_init(&databaseList.lock,SEMAPHORE_TYPE_BINARY);

  sqliteResult = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
  if (sqliteResult != SQLITE_OK)
  {
    return ERRORX_(DATABASE,sqliteResult,"enable multi-threading");
  }

  return ERROR_NONE;
}

void Database_doneAll(void)
{
  // done database list
  Semaphore_done(&databaseList.lock);
  List_done(&databaseList,CALLBACK((ListNodeFreeFunction)freeDatabaseNode,NULL));

  #ifndef DATABASE_LOCK_PER_INSTANCE
    // done global lock
    pthread_mutex_destroy(&databaseLock);
  #endif /* not DATABASE_LOCK_PER_INSTANCE */
}

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode,
                       long              timeout
                      )
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         ulong             __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
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
  SemaphoreLock semaphoreLock;
  DatabaseNode  *databaseNode;
  uint          i;

  assert(databaseHandle != NULL);

  // init variables
  Semaphore_init(&databaseHandle->lock,SEMAPHORE_TYPE_BINARY);
  databaseHandle->handle                  = NULL;
  databaseHandle->busyHandlerFunction     = NULL;
  databaseHandle->busyHandlerUserData     = NULL;
  databaseHandle->timeout                 = timeout;
  databaseHandle->lastCheckpointTimestamp = Misc_getTimestamp();
  sem_init(&databaseHandle->wakeUp,0,0);

  // create directory if needed
  if (!stringIsEmpty(fileName))
  {
    directoryName = File_getDirectoryNameCString(String_new(),fileName);
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
        Semaphore_done(&databaseHandle->lock);
        sem_destroy(&databaseHandle->wakeUp);
        return error;
      }
    }
    String_delete(directoryName);
  }

  // get filename
  databaseFileName = String_new();
  if (!stringIsEmpty(fileName))
  {
    // open file
    String_format(databaseFileName,"file:%s",fileName);
  }
  else
  {
    // open memory
    String_format(databaseFileName,"file::memory:");
  }

  // get mode
  sqliteMode = SQLITE_OPEN_URI;
  switch (databaseOpenMode & DATABASE_OPEN_MASK_MODE)
  {
    case DATABASE_OPENMODE_CREATE:    sqliteMode |= SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE; break;
    case DATABASE_OPENMODE_READ:      sqliteMode |= SQLITE_OPEN_READONLY;                     break;
    case DATABASE_OPENMODE_READWRITE: sqliteMode |= SQLITE_OPEN_READWRITE;                    break;
  }
  if ((databaseOpenMode & DATABASE_OPEN_MASK_FLAGS) != 0)
  {
    if ((databaseOpenMode & DATABASE_OPENMODE_MEMORY) == DATABASE_OPENMODE_MEMORY) sqliteMode |= SQLITE_OPEN_MEMORY;//String_appendCString(databaseFileName,"mode=memory");
    if ((databaseOpenMode & DATABASE_OPENMODE_SHARED) == DATABASE_OPENMODE_SHARED) sqliteMode |= SQLITE_OPEN_SHAREDCACHE;//String_appendCString(databaseFileName,"cache=shared");
  }
//sqliteMode |= SQLITE_OPEN_NOMUTEX;

  // open database
  if (stringIsEmpty(fileName))
  {
    fileName   = ":memory:";
    sqliteMode |= SQLITE_OPEN_SHAREDCACHE;
  }
  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    String_delete(databaseFileName);
    Semaphore_done(&databaseHandle->lock);
    sem_destroy(&databaseHandle->wakeUp);
    return error;
  }

  // get database node
  SEMAPHORE_LOCKED_DO(semaphoreLock,&databaseList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
#ifdef DATABASE_SINGLE_LOCK
    databaseNode = databaseList.head;
#else
    databaseNode = LIST_FIND(&databaseList,databaseNode,String_equalsCString(databaseNode->fileName,fileName));
#endif
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
      databaseNode->fileName                = String_duplicate(databaseFileName);
      databaseNode->openCount               = 1;
      #ifdef DATABASE_LOCK_PER_INSTANCE
        pthread_mutex_init(&databaseNode->lock,NULL);
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      databaseNode->type                    = DATABASE_LOCK_TYPE_NONE;

      databaseNode->pendingReadCount        = 0;
      databaseNode->readCount               = 0;
      pthread_cond_init(&databaseNode->readTrigger,NULL);

      databaseNode->pendingReadWriteCount   = 0;
      databaseNode->readWriteCount          = 0;
      pthread_cond_init(&databaseNode->readWriteTrigger,NULL);
//TODO: debug info
      databaseNode->readWriteLockedBy       = THREAD_ID_NONE;

      databaseNode->pendingTransactionCount = 0;
      databaseNode->transactionCount        = 0;
      pthread_cond_init(&databaseNode->transactionTrigger,NULL);

      List_init(&databaseNode->busyHandlerList);

      #ifndef NDEBUG
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->pendingReads);      i++) databaseNode->pendingReads[i].threadId      = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->reads);             i++) databaseNode->reads[i].threadId             = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->pendingReadWrites); i++) databaseNode->pendingReadWrites[i].threadId = THREAD_ID_NONE;
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->readWrites);        i++) databaseNode->readWrites[i].threadId        = THREAD_ID_NONE;
        databaseNode->lastTrigger.threadId = THREAD_ID_NONE;
        databaseNode->transaction.threadId = THREAD_ID_NONE;
      #endif /* not NDEBUG */

      List_append(&databaseList,databaseNode);
    }

    databaseHandle->databaseNode = databaseNode;
  }

#if 0
  // set busy handler
  sqliteResult = sqlite3_busy_handler(databaseHandle->handle,busyHandler,databaseHandle);
  assert(sqliteResult == SQLITE_OK);
#endif /* 0 */

  // register special functions
  sqliteResult = sqlite3_create_function(databaseHandle->handle,
                                         "unixtimestamp",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         unixTimestamp,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_create_function(databaseHandle->handle,
                                         "regexp",
                                         3,
                                         SQLITE_ANY,
                                         NULL,
                                         regexpMatch,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_create_function(databaseHandle->handle,
                                         "dirname",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         dirname,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);

  // enable recursive triggers
  sqliteResult = sqlite3_exec(databaseHandle->handle,
                              "PRAGMA recursive_triggers=ON",
                              CALLBACK(NULL,NULL),
                              NULL
                             );
  assert(sqliteResult == SQLITE_OK);

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: open '%s'\n",fileName);
  #endif

//TODO: remove
  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseHandle,sizeof(DatabaseHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,sizeof(DatabaseHandle));
  #endif /* NDEBUG */

  #ifndef NDEBUG
    pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

    pthread_mutex_lock(&debugDatabaseLock);
    {
      // init database handle
      databaseHandle->threadId                 = Thread_getCurrentId();
      databaseHandle->fileName                 = __fileName__;
      databaseHandle->lineNb                   = __lineNb__;
      #ifdef HAVE_BACKTRACE
        databaseHandle->stackTraceSize = backtrace((void*)databaseHandle->stackTrace,SIZE_OF_ARRAY(databaseHandle->stackTrace));
      #endif /* HAVE_BACKTRACE */
      databaseHandle->stackTraceSize           = 0;
      databaseHandle->locked.threadId          = THREAD_ID_NONE;
      databaseHandle->locked.lineNb            = 0;
      databaseHandle->locked.t0                = 0ULL;
      databaseHandle->locked.t1                = 0ULL;
      databaseHandle->current.sqlCommand       = String_new();
      Semaphore_init(&databaseHandle->current.lock,SEMAPHORE_TYPE_BINARY);
      #ifdef HAVE_BACKTRACE
        databaseHandle->current.stackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */

      // add to handle-list
      List_append(&debugDatabaseHandleList,databaseHandle);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* NDEBUG */

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
  DatabaseHandle *debugDatabaseHandle;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle != NULL);

//TODO: remove
  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseHandle,sizeof(DatabaseHandle));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,sizeof(DatabaseHandle));
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
          debugDumpStackTrace(stderr,0,databaseHandle->stackTrace,databaseHandle->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR_AT(__fileName__,
                               __lineNb__,
                               "Database %p is not opened",
                               databaseHandle
                              );
      }

//TODO: check
#if 1
      // check if transaction pending
      if (databaseHandle->databaseNode->transaction.fileName != NULL)
      {
        HALT_INTERNAL_ERROR_AT(__fileName__,
                               __lineNb__,
                               "Pending transaction at %s, line %u in database %p",
                               databaseHandle->databaseNode->transaction.fileName,
                               databaseHandle->databaseNode->transaction.lineNb,
                               databaseHandle
                              );
      }
#endif

      // remove from handle-list
      List_remove(&debugDatabaseHandleList,databaseHandle);

      // free resources
      Semaphore_done(&databaseHandle->current.lock);
      String_delete(databaseHandle->current.sqlCommand);
    }
    pthread_mutex_unlock(&debugDatabaseLock);
  #endif /* NDEBUG */

  // clear busy timeout handler
  sqlite3_busy_handler(databaseHandle->handle,NULL,NULL);

  // close database
  sqlite3_close(databaseHandle->handle);

  // free resources
//TODO: remove?
  Semaphore_done(&databaseHandle->lock);
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
  assert(databaseHandle->handle != NULL);
  assert(busyHandlerFunction != NULL);

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

void Database_removeBusyHandler(DatabaseHandle              *databaseHandle,
                                DatabaseBusyHandlerFunction busyHandlerFunction,
                                void                        *busyHandlerUserData
                               )
{
  DatabaseBusyHandlerNode *busyHandlerNode;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);
  assert(databaseHandle->handle != NULL);
  assert(busyHandlerFunction != NULL);

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

void Database_interrupt(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  #ifdef DATABASE_SUPPORT_INTERRUPT
    sqlite3_interrupt(databaseHandle->handle);
  #endif /* DATABASE_SUPPORT_INTERRUPT */
}

#ifdef NDEBUG
  void Database_lock(DatabaseHandle    *databaseHandle,
                     DatabaseLockTypes lockType
                    )
#else /* not NDEBUG */
  void __Database_lock(const char        *__fileName__,
                       ulong             __lineNb__,
                       DatabaseHandle    *databaseHandle,
                       DatabaseLockTypes lockType
                      )
#endif /* NDEBUG */
{
  #ifdef DATABASE_DEBUG_LOCK
    ulong debugLockCounter = 0L;
  #endif /* DATABASE_DEBUG_LOCK */
  #ifndef NDEBUG
    uint  i;
  #endif /* not NDEBUG */

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_NONE:
      break;
    case DATABASE_LOCK_TYPE_READ:
      DATABASE_LOCK(&databaseHandle);
      {
        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s LOCK   init: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */

        // check if there is no writer
        if (   (isReadWriteLock(databaseHandle) || isTransactionLock(databaseHandle))
            && !isOwnReadWriteLock(databaseHandle)
           )
        {
          // request read lock
          #ifdef DATABASE_USE_ATOMIC_INCREMENT
          #else /* not DATABASE_USE_ATOMIC_INCREMENT */
          #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//          ATOMIC_INCREMENT(databaseHandle->databaseNode->pendingReadCount);
          databaseHandle->databaseNode->pendingReadCount++;
          #ifndef NDEBUG
            for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->pendingReads); i++)
            {
              if (Thread_equalThreads(databaseHandle->databaseNode->pendingReads[i].threadId,THREAD_ID_NONE))
              {
                 databaseHandle->databaseNode->pendingReads[i].threadId = Thread_getCurrentId();
                 databaseHandle->databaseNode->pendingReads[i].fileName = __fileName__;
                 databaseHandle->databaseNode->pendingReads[i].lineNb   = __lineNb__;
                 BACKTRACE(databaseHandle->databaseNode->pendingReads[i].stackTrace,databaseHandle->databaseNode->pendingReads[i].stackTraceSize);
                 break;
              }
            }
          #endif /* not NDEBUG */

          // wait read/write end, transaction end
          do
          {
            #ifdef DATABASE_DEBUG_LOCK
              fprintf(stderr,"%s, %d: %s                wait rw %p\n",__FILE__,__LINE__,Thread_getCurrentIdString(),&databaseHandle->databaseNode->readTrigger);
            #endif /* DATABASE_DEBUG_LOCK */ 
            DATABASE_WAIT_TRIGGER(&databaseHandle,&databaseHandle->databaseNode->readTrigger);
            #ifdef DATABASE_DEBUG_LOCK
              fprintf(stderr,"%s, %d: %s                wait rw done %p\n",__FILE__,__LINE__,Thread_getCurrentIdString(),&databaseHandle->databaseNode->readTrigger);
            #endif /* DATABASE_DEBUG_LOCK */ 
//TODO: remove
#if 0
fprintf(stderr,"%s, %d: wakup\n",__FILE__,__LINE__);
if (isReadWriteLock(databaseHandle) || isTransactionLock(databaseHandle))
{
fprintf(stderr,"%s, %d: !!!!!!!!!!!!!\n",__FILE__,__LINE__);
          fprintf(stderr,
                  "%s, %d: r -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
}
#endif
          }
          while (isReadWriteLock(databaseHandle) || isTransactionLock(databaseHandle));

          // done request read lock
          assert(databaseHandle->databaseNode->pendingReadCount > 0);
          #ifdef DATABASE_USE_ATOMIC_INCREMENT
          #else /* not DATABASE_USE_ATOMIC_INCREMENT */
          #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//          ATOMIC_DECREMENT(databaseHandle->databaseNode->pendingReadCount);
          databaseHandle->databaseNode->pendingReadCount--;
          #ifndef NDEBUG
            for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->pendingReads); i++)
            {
              if (Thread_isCurrentThread(databaseHandle->databaseNode->pendingReads[i].threadId))
              {
                 databaseHandle->databaseNode->pendingReads[i].threadId = THREAD_ID_NONE;
                 break;
              }
            }
          #endif /* not NDEBUG */
        }
        assert(isOwnReadWriteLock(databaseHandle) || (databaseHandle->databaseNode->readWriteCount == 0));
        assert(isOwnReadWriteLock(databaseHandle) || (databaseHandle->databaseNode->transactionCount == 0));

        // read lock aquired
        #ifdef DATABASE_USE_ATOMIC_INCREMENT
        #else /* not DATABASE_USE_ATOMIC_INCREMENT */
        #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//        ATOMIC_INCREMENT(databaseHandle->databaseNode->readCount);
        databaseHandle->databaseNode->readCount++;
        #ifndef NDEBUG
          for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->reads); i++)
          {
            if (Thread_equalThreads(databaseHandle->databaseNode->reads[i].threadId,THREAD_ID_NONE))
            {
               databaseHandle->databaseNode->reads[i].threadId = Thread_getCurrentId();
               databaseHandle->databaseNode->reads[i].fileName = __fileName__;
               databaseHandle->databaseNode->reads[i].lineNb   = __lineNb__;
               BACKTRACE(databaseHandle->databaseNode->reads[i].stackTrace,databaseHandle->databaseNode->reads[i].stackTraceSize);
               break;
            }
          }
        #endif /* not NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s LOCK   done: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */
      }
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_UNLOCK(&databaseHandle);
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_LOCK(&databaseHandle);
      {
        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s LOCK   init: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */

        // check if there is no other writer
        if (   isReadWriteLock(databaseHandle)
            && !isOwnReadWriteLock(databaseHandle)
           )
        {
          // request read/write lock
          #ifdef DATABASE_USE_ATOMIC_INCREMENT
          #else /* not DATABASE_USE_ATOMIC_INCREMENT */
          #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//          ATOMIC_INCREMENT(databaseHandle->databaseNode->pendingReadWriteCount);
          databaseHandle->databaseNode->pendingReadWriteCount++;
          #ifndef NDEBUG
            for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->pendingReadWrites); i++)
            {
              if (Thread_equalThreads(databaseHandle->databaseNode->pendingReadWrites[i].threadId,THREAD_ID_NONE))
              {
                 databaseHandle->databaseNode->pendingReadWrites[i].threadId = Thread_getCurrentId();
                 databaseHandle->databaseNode->pendingReadWrites[i].fileName = __fileName__;
                 databaseHandle->databaseNode->pendingReadWrites[i].lineNb   = __lineNb__;
                 BACKTRACE(databaseHandle->databaseNode->pendingReadWrites[i].stackTrace,databaseHandle->databaseNode->pendingReadWrites[i].stackTraceSize);
                 break;
              }
            }
          #endif /* not NDEBUG */

          // wait read/write end
          do
          {
            #ifdef DATABASE_DEBUG_LOCK
              fprintf(stderr,"%s, %d: %s                 wait rw %p\n",__FILE__,__LINE__,Thread_getCurrentIdString(),&databaseHandle->databaseNode->readWriteTrigger);
            #endif /* DATABASE_DEBUG_LOCK */ 
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
            DATABASE_WAIT_TRIGGER(&databaseHandle,&databaseHandle->databaseNode->readWriteTrigger);
            #ifdef DATABASE_DEBUG_LOCK
              fprintf(stderr,"%s, %d: %s                 wait rw done %p\n",__FILE__,__LINE__,Thread_getCurrentIdString(),&databaseHandle->databaseNode->readWriteTrigger);
            #endif /* DATABASE_DEBUG_LOCK */
//TODO: remove
#if 0
fprintf(stderr,"%s, %d: wakup\n",__FILE__,__LINE__);
if (isReadWriteLock(databaseHandle)) 
{
          fprintf(stderr,
                  "%s, %d: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
}
#endif
          }
          while (isReadWriteLock(databaseHandle));
          assert(Thread_equalThreads(databaseHandle->databaseNode->readWriteLockedBy,THREAD_ID_NONE));

          // done request read/write lock
          assert(databaseHandle->databaseNode->pendingReadWriteCount > 0);
          #ifdef DATABASE_USE_ATOMIC_INCREMENT
          #else /* not DATABASE_USE_ATOMIC_INCREMENT */
          #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//          ATOMIC_DECREMENT(databaseHandle->databaseNode->pendingReadWriteCount);
          databaseHandle->databaseNode->pendingReadWriteCount--;
          assert(isOwnReadWriteLock(databaseHandle) || (databaseHandle->databaseNode->readWriteCount == 0));
          assert(isOwnReadWriteLock(databaseHandle) || Thread_equalThreads(databaseHandle->databaseNode->readWriteLockedBy,THREAD_ID_NONE));
          #ifndef NDEBUG
            for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->pendingReadWrites); i++)
            {
              if (Thread_isCurrentThread(databaseHandle->databaseNode->pendingReadWrites[i].threadId))
              {
                 databaseHandle->databaseNode->pendingReadWrites[i].threadId = THREAD_ID_NONE;
                 break;
              }
            }
          #endif /* not NDEBUG */
        }

        // read/write lock aquired
        #ifdef DATABASE_USE_ATOMIC_INCREMENT
        #else /* not DATABASE_USE_ATOMIC_INCREMENT */
        #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//        ATOMIC_INCREMENT(databaseHandle->databaseNode->readWriteCount);
        databaseHandle->databaseNode->readWriteCount++;
        assert(Thread_isCurrentThread(databaseHandle->databaseNode->readWriteLockedBy) || Thread_equalThreads(databaseHandle->databaseNode->readWriteLockedBy,THREAD_ID_NONE));
        databaseHandle->databaseNode->readWriteLockedBy = Thread_getCurrentId();
        #ifndef NDEBUG
          for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->readWrites); i++)
          {
            if (Thread_equalThreads(databaseHandle->databaseNode->readWrites[i].threadId,THREAD_ID_NONE))
            {
               databaseHandle->databaseNode->readWrites[i].threadId = Thread_getCurrentId();
               databaseHandle->databaseNode->readWrites[i].fileName = __fileName__;
               databaseHandle->databaseNode->readWrites[i].lineNb   = __lineNb__;
               BACKTRACE(databaseHandle->databaseNode->readWrites[i].stackTrace,databaseHandle->databaseNode->readWrites[i].stackTraceSize);
               break;
            }
          }
        #endif /* not NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s LOCK   done: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */
      }
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_UNLOCK(&databaseHandle);
      break;
  }
  #ifndef NDEBUG
    databaseHandle->locked.threadId = Thread_getCurrentId();
    databaseHandle->locked.fileName = __fileName__;
    databaseHandle->locked.lineNb   = __lineNb__;
    databaseHandle->locked.text[0]  = '\0';
    databaseHandle->locked.t0       = Misc_getTimestamp();
  #endif /* not NDEBUG */
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
  uint i;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->databaseNode != NULL);

  #ifndef NDEBUG
    databaseHandle->locked.threadId = THREAD_ID_NONE;
    databaseHandle->locked.fileName = NULL;
    databaseHandle->locked.lineNb   = 0;
    databaseHandle->locked.text[0]  = '\0';
    databaseHandle->locked.t1       = Misc_getTimestamp();
  #endif /* not NDEBUG */
//TODO
  switch (lockType)
  {
    case DATABASE_LOCK_TYPE_NONE:
      break;
    case DATABASE_LOCK_TYPE_READ:
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_LOCK(&databaseHandle);
      {
        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK init: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */

        // decrement read count
        assert(databaseHandle->databaseNode->readCount > 0);
        #ifdef DATABASE_USE_ATOMIC_INCREMENT
        #else /* not DATABASE_USE_ATOMIC_INCREMENT */
        #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//        ATOMIC_DECREMENT(databaseHandle->databaseNode->readCount);
        databaseHandle->databaseNode->readCount--;
        if (databaseHandle->databaseNode->readCount == 0)
        {
//TODO
        }
        #ifndef NDEBUG
          for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->reads); i++)
          {
            if (Thread_isCurrentThread(databaseHandle->databaseNode->reads[i].threadId))
            {
               databaseHandle->databaseNode->reads[i].threadId = THREAD_ID_NONE;
               break;
            }
          }
        #endif /* not NDEBUG */

//        if (databaseHandle->databaseNode->transactionCount == 0)
        {
          if      ((databaseHandle->databaseNode->transactionCount == 0) && (databaseHandle->databaseNode->pendingReadCount > 0))
          {
//fprintf(stderr,"%s, %d: trigger r %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readTrigger);
            #ifndef NDEBUG
              databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
              databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
              databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
              databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
              BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
            #endif /* NDEBUG */
            pthread_cond_broadcast(&databaseHandle->databaseNode->readTrigger);
          }
          else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
          {
//fprintf(stderr,"%s, %d: trigger rw %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readWriteTrigger);
            #ifndef NDEBUG
              databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
              databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
              databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
              databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ_WRITE;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
              BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
            #endif /* NDEBUG */
            pthread_cond_signal(&databaseHandle->databaseNode->readWriteTrigger);
          }
        }

        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK done: r  -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */
#if 0
  if (   (databaseHandle->databaseNode->pendingReadCount == 0)
      && (databaseHandle->databaseNode->readCount == 0)
      && (databaseHandle->databaseNode->pendingReadWriteCount == 0)
      && (databaseHandle->databaseNode->readWriteCount == 0)
     )
  fprintf(stderr,"%s, %d: --------------------------------------------------------------------------------------------------\n",__FILE__,__LINE__);
#endif
      }
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_UNLOCK(&databaseHandle);
      break;
    case DATABASE_LOCK_TYPE_READ_WRITE:
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_LOCK(&databaseHandle);
      {
        assert(isReadWriteLock(databaseHandle));
        assert(!Thread_equalThreads(databaseHandle->databaseNode->readWriteLockedBy,THREAD_ID_NONE));

        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK init: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */

        // decrement read/write count
        assert(databaseHandle->databaseNode->readWriteCount > 0);
        #ifdef DATABASE_USE_ATOMIC_INCREMENT
        #else /* not DATABASE_USE_ATOMIC_INCREMENT */
        #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//        ATOMIC_DECREMENT(databaseHandle->databaseNode->readWriteCount);
        databaseHandle->databaseNode->readWriteCount--;
        if (databaseHandle->databaseNode->readWriteCount == 0)
        {
          databaseHandle->databaseNode->readWriteLockedBy = THREAD_ID_NONE;
//fprintf(stderr,"%s, %d: %s release read/write lock by %s\n",__FILE__,__LINE__,Thread_getCurrentIdString(),Thread_getIdString(Thread_getCurrentId()));

          if      ((databaseHandle->databaseNode->transactionCount == 0) && (databaseHandle->databaseNode->pendingReadCount > 0))
          {
//fprintf(stderr,"%s, %d: trigger r %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readTrigger);
            #ifndef NDEBUG
              databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
              databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
              databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
              databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
              BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
            #endif /* NDEBUG */
            pthread_cond_broadcast(&databaseHandle->databaseNode->readTrigger);
          }
          else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
          {
//fprintf(stderr,"%s, %d: trigger rw %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readWriteTrigger);
            #ifndef NDEBUG
              databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
              databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
              databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
              databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ_WRITE;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
              BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
            #endif /* NDEBUG */
            pthread_cond_signal(&databaseHandle->databaseNode->readWriteTrigger);
          }
        }
        #ifndef NDEBUG
          for (i = 0; i < SIZE_OF_ARRAY(databaseHandle->databaseNode->readWrites); i++)
          {
            if (Thread_isCurrentThread(databaseHandle->databaseNode->readWrites[i].threadId))
            {
               databaseHandle->databaseNode->readWrites[i].threadId = THREAD_ID_NONE;
               break;
            }
          }
        #endif /* not NDEBUG */

        #ifdef DATABASE_DEBUG_LOCK
          fprintf(stderr,
                  "%s, %d: %s          UNLOCK done: rw -- pending r %2d, r %2d, pending rw %2d, rw %2d, rw current locked by %s, transaction %2d at %s %d\n",
                  __FILE__,__LINE__,
                  Thread_getCurrentIdString(),
                  databaseHandle->databaseNode->pendingReadCount,
                  databaseHandle->databaseNode->readCount,
                  databaseHandle->databaseNode->pendingReadWriteCount,
                  databaseHandle->databaseNode->readWriteCount,
                  (databaseHandle->databaseNode->readWriteCount > 0) ? Thread_getIdString(databaseHandle->databaseNode->readWriteLockedBy) : "none",
                  databaseHandle->databaseNode->transactionCount,
                  __fileName__,__lineNb__
                 );
        #endif /* DATABASE_DEBUG_LOCK */
#if 0
  if (   (databaseHandle->databaseNode->pendingReadCount == 0)
      && (databaseHandle->databaseNode->readCount == 0)
      && (databaseHandle->databaseNode->pendingReadWriteCount == 0)
      && (databaseHandle->databaseNode->readWriteCount == 0)
     )
  fprintf(stderr,"%s, %d: --------------------------------------------------------------------------------------------------\n",__FILE__,__LINE__);
#endif
      }
      #ifdef DATABASE_LOCK_PER_INSTANCE
      #endif /* DATABASE_LOCK_PER_INSTANCE */
      DATABASE_UNLOCK(&databaseHandle);
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

//TODO: still not implemented
UNUSED_VARIABLE(lockType);

  pendingFlag = (databaseHandle->databaseNode->pendingReadCount > 0);

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

  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "PRAGMA synchronous=%s;",
                           enabled ? "ON" : "OFF"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "PRAGMA journal_mode=%s;",
                           enabled ? "ON" : "WAL"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Database_setEnabledForeignKeys(DatabaseHandle *databaseHandle,
                                      bool           enabled
                                     )
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);

  return Database_execute(databaseHandle,
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          "PRAGMA foreign_keys=%s;",
                          enabled ? "ON" : "OFF"
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
  assert(databaseHandleReference->handle != NULL);
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle != NULL);

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

Errors Database_copyTable(DatabaseHandle                *fromDatabaseHandle,
                          DatabaseHandle                *toDatabaseHandle,
                          const char                    *fromTableName,
                          const char                    *toTableName,
                          bool                          transactionFlag,
                          DatabaseCopyTableFunction     preCopyTableFunction,
                          void                          *preCopyTableUserData,
                          DatabaseCopyTableFunction     postCopyTableFunction,
                          void                          *postCopyTableUserData,
                          DatabasePauseCallbackFunction pauseCallbackFunction,
                          void                          *pauseCallbackUserData,
                          const char                    *fromAdditional,
                          ...
                         )
{
  Errors             error;
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
  assert(fromDatabaseHandle->handle != NULL);
  assert(toDatabaseHandle != NULL);
  assert(toDatabaseHandle->handle != NULL);
  assert(fromTableName != NULL);
  assert(toTableName != NULL);

  #ifdef DATABASE_DEBUG_COPY_TABLE
    t0       = Misc_getTimestamp();
    rowCount = 0;
  #endif /* DATABASE_DEBUG_COPY_TABLE */

  // get table columns
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
  String_appendCString(sqlSelectString,";");
  DATABASE_DEBUG_SQL(fromDatabaseHandle,sqlSelectString);

  // select rows in from-table and copy to to-table
  sqlInsertString = String_new();
  BLOCK_DOX(error,
            { begin(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);
              begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
            },
            { end(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);
              end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
            },
  {
    Errors error;

    // begin transaction
    if (transactionFlag)
    {
      error = Database_beginTransaction(toDatabaseHandle,DATABASE_TRANSACTION_TYPE_DEFERRED);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // create select statement
    sqliteResult = sqlite3_prepare_v2(fromDatabaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(fromDatabaseHandle->handle));
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(fromDatabaseHandle->handle),"%s: %s",String_cString(sqlSelectString),sqlite3_errmsg(fromDatabaseHandle->handle));
      if (transactionFlag)
      {
        (void)Database_rollbackTransaction(toDatabaseHandle);
      }
      return error;
    }
    #ifndef NDEBUG
      if (fromStatementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(fromDatabaseHandle->handle),sqlite3_errmsg(fromDatabaseHandle->handle),String_cString(sqlSelectString));
        abort();
      }
    #endif /* not NDEBUG */

    // copy rows
    while ((sqliteResult = sqliteStep(fromDatabaseHandle->handle,fromStatementHandle,fromDatabaseHandle->timeout)) == SQLITE_ROW)
    {
      #ifdef DATABASE_DEBUG_COPY_TABLE
        rowCount++;
      #endif /* DATABASE_DEBUG_COPY_TABLE */

      // reset to data
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
          case DATABASE_TYPE_PRIMARY_KEY:
            columnNode->value.id = sqlite3_column_int64(fromStatementHandle,i);
//fprintf(stderr,"%s, %d: DATABASE_TYPE_PRIMARY_KEY %d %s: %lld\n",__FILE__,__LINE__,n,columnNode->name,columnNode->value.id);
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              toColumnNode->value.id = DATABASE_ID_NONE;
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_INT64:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,i));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_INT64 %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_DOUBLE:
            String_setCString(columnNode->value.d,(const char*)sqlite3_column_text(fromStatementHandle,i));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DOUBLE %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.d,columnNode->value.d);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_DATETIME:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,i));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DATETIME %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_TEXT:
            String_setCString(columnNode->value.text,(const char*)sqlite3_column_text(fromStatementHandle,i));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_TEXT %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.text,columnNode->value.text);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_BLOB:
            HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break; // not reached
        }
        i++;
      }

      // call pre-copy callback (if defined)
      if (preCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
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
        if (columnNode->usedFlag && (columnNode->type != DATABASE_TYPE_PRIMARY_KEY))
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
        if (columnNode->usedFlag && (columnNode->type != DATABASE_TYPE_PRIMARY_KEY))
        {
          if (i > 0) String_appendChar(sqlInsertString,',');
          String_appendChar(sqlInsertString,'?');
          i++;
        }
      }
      String_appendCString(sqlInsertString,");");
//      DATABASE_DEBUG_SQL(toDatabaseHandle,sqlInsertString);

      // create insert statement
      sqliteResult = sqlite3_prepare_v2(toDatabaseHandle->handle,
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
        HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(toDatabaseHandle->handle));
      }
      else
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
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
          fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(toDatabaseHandle->handle),sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
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
            case DATABASE_TYPE_PRIMARY_KEY:
              // can not be set
              break;
            case DATABASE_TYPE_INT64:
//fprintf(stderr,"%s, %d: DATABASE_TYPE_INT64 %d %s: %s %d\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.i),sqlite3_column_type(fromStatementHandle,n));
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.i),-1,NULL);
              i++;
              break;
            case DATABASE_TYPE_DOUBLE:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.d),-1,NULL);
              i++;
              break;
            case DATABASE_TYPE_DATETIME:
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.i),-1,NULL);
              i++;
              break;
            case DATABASE_TYPE_TEXT:
//fprintf(stderr,"%s, %d: DATABASE_TYPE_TEXT %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
              sqlite3_bind_text(toStatementHandle,i,String_cString(columnNode->value.text),-1,NULL);
              i++;
              break;
            case DATABASE_TYPE_BLOB:
              i++;
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; // not reached
          }
        }
      }

      // insert row
      if (sqliteStep(toDatabaseHandle->handle,toStatementHandle,toDatabaseHandle->timeout) != SQLITE_DONE)
      {
//fprintf(stderr,"%s, %d: 4 %s %s\n",__FILE__,__LINE__,sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
        sqlite3_finalize(toStatementHandle);
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle);
        }
        return error;
      }
      lastRowId = (DatabaseId)sqlite3_last_insert_rowid(toDatabaseHandle->handle);
      LIST_ITERATE(&toColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_TYPE_PRIMARY_KEY:
            columnNode->value.id = lastRowId;
            break;
          case DATABASE_TYPE_INT64:
          case DATABASE_TYPE_DOUBLE:
          case DATABASE_TYPE_DATETIME:
          case DATABASE_TYPE_TEXT:
          case DATABASE_TYPE_BLOB:
          case DATABASE_TYPE_UNKNOWN:
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
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
                  begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
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

      // pause
      if ((pauseCallbackFunction != NULL) && pauseCallbackFunction(pauseCallbackUserData))
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

        // wait
        BLOCK_DO({ end(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
                   end(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);                   
                 },
                 { begin(fromDatabaseHandle,DATABASE_LOCK_TYPE_READ);
                   begin(toDatabaseHandle,DATABASE_LOCK_TYPE_READ_WRITE);
                 },
        {
          do
          {
            Misc_udelay(10LL*US_PER_SECOND);
          }
          while (pauseCallbackFunction(pauseCallbackUserData));
        });

        // begin transaction
        if (transactionFlag)
        {
          error = Database_beginTransaction(toDatabaseHandle,DATABASE_TRANSACTION_TYPE_DEFERRED);
          if (error != ERROR_NONE)
          {
            sqlite3_finalize(fromStatementHandle);
            return error;
          }
        }

      }
    }

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
              "%s, %d: DEBUG copy table %s->%s: %llums, %lu rows, %lfms/row\n",
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
}

int Database_getTableColumnListInt(const DatabaseColumnList *columnList, const char *columnName, int defaultValue)
{
  DatabaseColumnNode *columnNode;

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_TYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_TYPE_INT64));
    if (columnNode->type == DATABASE_TYPE_PRIMARY_KEY)
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_TYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_TYPE_INT64));
    if (columnNode->type == DATABASE_TYPE_PRIMARY_KEY)
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_TYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_TYPE_INT64));
    if (columnNode->type == DATABASE_TYPE_PRIMARY_KEY)
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert((columnNode->type == DATABASE_TYPE_PRIMARY_KEY) || (columnNode->type == DATABASE_TYPE_INT64));
    if (columnNode->type == DATABASE_TYPE_PRIMARY_KEY)
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_DOUBLE);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_DATETIME);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_TEXT);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_TEXT);
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

UNUSED_VARIABLE(data);
UNUSED_VARIABLE(length);
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_BLOB);
//    return columnNode->value.blob.data;
  }
  else
  {
//    return data;
  }
}

bool Database_setTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 value)
{
  DatabaseColumnNode *columnNode;

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_INT64);
    String_format(columnNode->value.i,"%lld",value);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_DOUBLE);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_DATETIME);
    String_format(columnNode->value.i,"%lld",value);
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
  return Database_setTableColumnListCString(columnList,columnName,String_cString(value));
}

bool Database_setTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *value)
{
  DatabaseColumnNode *columnNode;

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_TEXT);
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

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_BLOB);
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
                          DatabaseTypes  columnType
                         )
{
  const char *columnTypeString;
  Errors     error;

  // get column type name
  columnTypeString = NULL;
  switch (columnType)
  {
    case DATABASE_TYPE_PRIMARY_KEY:
      columnTypeString = "INTEGER PRIMARY KEY";
      break;
    case DATABASE_TYPE_FOREIGN_KEY:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_TYPE_INT64:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_TYPE_DOUBLE:
      columnTypeString = "REAL DEFAULT 0.0";
      break;
    case DATABASE_TYPE_DATETIME:
      columnTypeString = "INTEGER DEFAULT 0";
      break;
    case DATABASE_TYPE_TEXT:
      columnTypeString = "TEXT DEFAULT ''";
      break;
    case DATABASE_TYPE_BLOB:
      columnTypeString = "BLOB";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; // not reached
  }

  // execute SQL command
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE %s ADD COLUMN %s %s; \
                           ",
                           tableName,
                           columnName,
                           columnTypeString
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
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
  assert(databaseHandle->handle != NULL);
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
            begin(databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE),
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
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // copy old table -> new table
    formatSQLString(String_clear(sqlString),"SELECT * FROM %s;",tableName);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    // copy table rows
    while (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
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
            case DATABASE_TYPE_PRIMARY_KEY:
            case DATABASE_TYPE_INT64:
            case DATABASE_TYPE_DOUBLE:
            case DATABASE_TYPE_DATETIME:
            case DATABASE_TYPE_TEXT:
              formatSQLString(sqlString,"%'s",sqlite3_column_text(statementHandle,column));
              break;
            case DATABASE_TYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; // not reached
          }
          n++;
        }

        column++;
      }
      String_appendCString(sqlString,");");

      // execute SQL command
      DATABASE_DEBUG_SQL(databaseHandle,sqlString);
      error = sqliteExecute(databaseHandle,
                            String_cString(sqlString),
                            CALLBACK(NULL,NULL),  // databaseRowFunction
                            NULL,  // changedRowCount
                            databaseHandle->timeout
                           );
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
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE %s RENAME TO __old__;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE __new__ RENAME TO %s;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "ALTER TABLE __old__ RENAME TO %s;",
                           tableName
                          );
    (void)Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                   DatabaseTransactionTypes databaseTransactionType
                                  )
#else /* not NDEBUG */
  Errors __Database_beginTransaction(const char               *__fileName__,
                                     uint                     __lineNb__,
                                     DatabaseHandle           *databaseHandle,
                                     DatabaseTransactionTypes databaseTransactionType
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
  assert(databaseHandle->handle != NULL);

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
            debugDumpStackTrace(stderr,0,databaseHandle->databaseNode->transaction.stackTrace,databaseHandle->databaseNode->transaction.stackTraceSize,0);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR("begin transactions fail");
        }

        databaseHandle->databaseNode->transaction.threadId = Thread_getCurrentId();
        databaseHandle->databaseNode->transaction.fileName = __fileName__;
        databaseHandle->databaseNode->transaction.lineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->transaction.stackTraceSize = backtrace((void*)databaseHandle->databaseNode->transaction.stackTrace,SIZE_OF_ARRAY(databaseHandle->databaseNode->transaction.stackTrace));
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* NDEBUG */
#endif

    // format SQL command string
    sqlString = String_new();
    switch (databaseTransactionType)
    {
      case DATABASE_TRANSACTION_TYPE_DEFERRED : String_format(sqlString,"BEGIN DEFERRED TRANSACTION;");  break;
      case DATABASE_TRANSACTION_TYPE_IMMEDIATE: String_format(sqlString,"BEGIN IMMEDIATE TRANSACTION;"); break;
      case DATABASE_TRANSACTION_TYPE_EXCLUSIVE: String_format(sqlString,"BEGIN EXCLUSIVE TRANSACTION;"); break;
    }

    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      String_delete(sqlString);
      return error;
    }

    // free resources
    String_delete(sqlString);

    // debug: store transaction info
    #ifdef DATABASE_USE_ATOMIC_INCREMENT
    #else /* not DATABASE_USE_ATOMIC_INCREMENT */
    #endif /* DATABASE_USE_ATOMIC_INCREMENT */
    DATABASE_LOCK(&databaseHandle);
    {
      assert(databaseHandle->databaseNode->transactionCount == 0);
//      ATOMIC_INCREMENT(databaseHandle->databaseNode->transactionCount);
      databaseHandle->databaseNode->transactionCount++;

    }
    DATABASE_UNLOCK(&databaseHandle);

    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->transaction.threadId = Thread_getCurrentId();
        databaseHandle->databaseNode->transaction.fileName = __fileName__;
        databaseHandle->databaseNode->transaction.lineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->transaction.stackTraceSize = backtrace((void*)databaseHandle->databaseNode->transaction.stackTrace,SIZE_OF_ARRAY(databaseHandle->databaseNode->transaction.stackTrace));
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* NDEBUG */

    #ifdef DATABASE_DEBUG_LOCK
      fprintf(stderr,
              "%s, %d: %s TRANSACTION begin at %s %d\n",
              __FILE__,__LINE__,
              Thread_getCurrentIdString(),
              __fileName__,__lineNb__
             );
    #endif /* DATABASE_DEBUG_LOCK */
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    #ifndef NDEBUG
      UNUSED_VARIABLE(__fileName__);
      UNUSED_VARIABLE(__lineNb__);
    #endif
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
  assert(databaseHandle->handle != NULL);

  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    // decrement transaction count
    DATABASE_LOCK(&databaseHandle);
    {
      assert(databaseHandle->databaseNode->transactionCount > 0);
      #ifdef DATABASE_USE_ATOMIC_INCREMENT
      #else /* not DATABASE_USE_ATOMIC_INCREMENT */
      #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//      ATOMIC_DECREMENT(databaseHandle->databaseNode->transactionCount);
      databaseHandle->databaseNode->transactionCount--;
      if (databaseHandle->databaseNode->transactionCount == 0)
      {
//fprintf(stderr,"%s, %d: trigger transaction %p %d %p\n",__FILE__,__LINE__,databaseHandle->databaseNode,databaseHandle->databaseNode->transactionCount,&databaseHandle->databaseNode->transactionTrigger);
        pthread_cond_broadcast(&databaseHandle->databaseNode->transactionTrigger);

        if      (databaseHandle->databaseNode->pendingReadCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger r %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readTrigger);
          #ifndef NDEBUG
            databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
            databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
            databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
            databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
            BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
          #endif /* NDEBUG */
          pthread_cond_broadcast(&databaseHandle->databaseNode->readTrigger);
        }
        else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger rw %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readWriteTrigger);
          #ifndef NDEBUG
            databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
            databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
            databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
            databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ_WRITE;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
            BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
          #endif /* NDEBUG */
          pthread_cond_signal(&databaseHandle->databaseNode->readWriteTrigger);
        }
      }
    }
    DATABASE_UNLOCK(&databaseHandle);

    // format SQL command string
    sqlString = String_format(String_new(),"END TRANSACTION;");

    // end transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      String_delete(sqlString);
      return error;
    }

    // free resources
    String_delete(sqlString);
    
    // debug: clear transaction info
    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->transaction.threadId = THREAD_ID_NONE;
        databaseHandle->databaseNode->transaction.fileName = NULL;
        databaseHandle->databaseNode->transaction.lineNb   = 0;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->transaction.stackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* NDEBUG */

    #ifdef DATABASE_DEBUG_LOCK
      fprintf(stderr,
              "%s, %d: %s TRANSACTION end at %s %d\n",
              __FILE__,__LINE__,
              Thread_getCurrentIdString(),
              __fileName__,__lineNb__
             );
    #endif /* DATABASE_DEBUG_LOCK */

    // try to execute checkpoint
    executeCheckpoint(databaseHandle);
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
  assert(databaseHandle->handle != NULL);

  #ifdef DATABASE_SUPPORT_TRANSACTIONS
    DATABASE_LOCK(&databaseHandle);
    {
      assert(databaseHandle->databaseNode->transactionCount > 0);
      #ifdef DATABASE_USE_ATOMIC_INCREMENT
      #else /* not DATABASE_USE_ATOMIC_INCREMENT */
      #endif /* DATABASE_USE_ATOMIC_INCREMENT */
//      ATOMIC_DECREMENT(databaseHandle->databaseNode->transactionCount);
      databaseHandle->databaseNode->transactionCount--;
      if (databaseHandle->databaseNode->transactionCount == 0)
      {
//fprintf(stderr,"%s, %d: trigger transaction %p %d %p\n",__FILE__,__LINE__,databaseHandle->databaseNode,databaseHandle->databaseNode->transactionCount,&databaseHandle->databaseNode->transactionTrigger);
        pthread_cond_broadcast(&databaseHandle->databaseNode->transactionTrigger);

        if      (databaseHandle->databaseNode->pendingReadCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger r %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readTrigger);
          #ifndef NDEBUG
            databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
            databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
            databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
            databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
            BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
          #endif /* NDEBUG */
          pthread_cond_broadcast(&databaseHandle->databaseNode->readTrigger);
        }
        else if (databaseHandle->databaseNode->pendingReadWriteCount > 0)
        {
//fprintf(stderr,"%s, %d: trigger rw %p\n",__FILE__,__LINE__,&databaseHandle->databaseNode->readWriteTrigger);
          #ifndef NDEBUG
            databaseHandle->databaseNode->lastTrigger.threadId = Thread_getCurrentId();
            databaseHandle->databaseNode->lastTrigger.fileName = __fileName__;
            databaseHandle->databaseNode->lastTrigger.lineNb   = __lineNb__;
            databaseHandle->databaseNode->lastTrigger.type     = DATABASE_LOCK_TYPE_READ_WRITE;
databaseHandle->databaseNode->lastTrigger.pendingReadCount        = databaseHandle->databaseNode->pendingReadCount       ;
databaseHandle->databaseNode->lastTrigger.readCount               = databaseHandle->databaseNode->readCount              ;
databaseHandle->databaseNode->lastTrigger.pendingReadWriteCount   = databaseHandle->databaseNode->pendingReadWriteCount  ;
databaseHandle->databaseNode->lastTrigger.readWriteCount          = databaseHandle->databaseNode->readWriteCount         ;
databaseHandle->databaseNode->lastTrigger.pendingTransactionCount = databaseHandle->databaseNode->pendingTransactionCount;
databaseHandle->databaseNode->lastTrigger.transactionCount        = databaseHandle->databaseNode->transactionCount       ;
            BACKTRACE(databaseHandle->databaseNode->lastTrigger.stackTrace,databaseHandle->databaseNode->lastTrigger.stackTraceSize);
          #endif /* NDEBUG */
          pthread_cond_signal(&databaseHandle->databaseNode->readWriteTrigger);
        }
      }
    }
    DATABASE_UNLOCK(&databaseHandle);

    // format SQL command string
    sqlString = String_format(String_new(),"ROLLBACK TRANSACTION;");

    // rollback transaction
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      String_delete(sqlString);
      return error;
    }

    // free resources
    String_delete(sqlString);

    // debug: clear transaction info
    #ifndef NDEBUG
      pthread_once(&debugDatabaseInitFlag,debugDatabaseInit);

      pthread_mutex_lock(&debugDatabaseLock);
      {
        databaseHandle->databaseNode->transaction.threadId = THREAD_ID_NONE;
        databaseHandle->databaseNode->transaction.fileName = NULL;
        databaseHandle->databaseNode->transaction.lineNb   = 0;
        #ifdef HAVE_BACKTRACE
          databaseHandle->databaseNode->transaction.stackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
      }
      pthread_mutex_unlock(&debugDatabaseLock);
    #endif /* NDEBUG */
  #else /* not DATABASE_SUPPORT_TRANSACTIONS */
    UNUSED_VARIABLE(databaseHandle);
  #endif /* DATABASE_SUPPORT_TRANSACTIONS */

  return ERROR_NONE;
}

Errors Database_flush(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle != NULL);

  sqlite3_wal_checkpoint(databaseHandle->handle,NULL);

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
  String  sqlString;
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle != NULL);
  assert(command != NULL);

  // init variables
  if (changedRowCount != NULL) (*changedRowCount) = 0L;

  // format SQL command string
  va_start(arguments,command);
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );
  va_end(arguments);

  // execute SQL command
  DATABASE_DEBUG_SQL(databaseHandle,sqlString);
  error = sqliteExecute(databaseHandle,
                        String_cString(sqlString),
                        CALLBACK(databaseRowFunction,databaseRowUserData),
                        changedRowCount,
                        databaseHandle->timeout
                       );
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
  assert(databaseHandle->handle != NULL);
  assert(command != NULL);

  // initialize variables
  databaseQueryHandle->databaseHandle = databaseHandle;

  // format SQL command string
  va_start(arguments,command);
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );
  va_end(arguments);
  #ifndef NDEBUG
    databaseQueryHandle->sqlString = String_duplicate(sqlString);
    databaseQueryHandle->dt        = 0LL;
  #endif /* NDEBUG */

  // lock
  Database_lock(databaseHandle,DATABASE_LOCK_TYPE_READ);

  // prepare SQL command execution
  DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//  DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString);

//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(sqlString));
  #ifndef NDEBUG
    String_set(databaseHandle->current.sqlCommand,sqlString);
  #endif /* NDEBUG */

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  {
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
    HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
  }
  else
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
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
      fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
      abort();
    }
  #endif /* not NDEBUG */

  // free resources
  String_delete(sqlString);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseQueryHandle,sizeof(DatabaseQueryHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseQueryHandle,sizeof(DatabaseQueryHandle));
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
    bool   *b;
    int    *i;
    uint   *ui;
    long   *l;
    ulong  *ul;
    int64  *ll;
    uint64 *ull;
    float  *f;
    double *d;
    char   *ch;
    char   *s;
    void   **p;
    String string;
  }       value;

  assert(databaseQueryHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseQueryHandle);
  assert(databaseQueryHandle->databaseHandle != NULL);
  assert(databaseQueryHandle->databaseHandle->handle != NULL);
  assert(format != NULL);

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  if (sqliteStep(databaseQueryHandle->databaseHandle->handle,databaseQueryHandle->statementHandle,databaseQueryHandle->databaseHandle->timeout) == SQLITE_ROW)
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
              value.ll = va_arg(arguments,int64*);
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
              value.ull = va_arg(arguments,uint64*);
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
  assert(databaseQueryHandle->databaseHandle != NULL);
  assert(databaseQueryHandle->databaseHandle->handle != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseQueryHandle,sizeof(DatabaseQueryHandle));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseQueryHandle,sizeof(DatabaseQueryHandle));
  #endif /* NDEBUG */

  #ifndef NDEBUG
    String_clear(databaseQueryHandle->databaseHandle->current.sqlCommand);
    #ifdef HAVE_BACKTRACE
      databaseQueryHandle->databaseHandle->current.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* NDEBUG */

  DATABASE_DEBUG_TIME_START(databaseQueryHandle);
  {
    sqlite3_finalize(databaseQueryHandle->statementHandle);
  }
  DATABASE_DEBUG_TIME_END(databaseQueryHandle);
  #ifndef NDEBUG
    DATABASE_DEBUG_TIME(databaseQueryHandle);
  #endif /* not NDEBUG */

  // unlock
  Database_unlock(databaseQueryHandle->databaseHandle,DATABASE_LOCK_TYPE_READ);

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
  assert(databaseHandle->handle != NULL);
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
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    existsFlag = FALSE;

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &statementHandle,
                                      NULL
                                     );
    if      (sqliteResult == SQLITE_OK)
    {
      if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
      {
        existsFlag = TRUE;
      }
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      // nothing to do
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */
    sqlite3_finalize(statementHandle);
    
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (DatabaseId)sqlite3_column_int64(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);
    
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
  assert(values != NULL);
  assert(tableName != NULL);

  // init variables
  Array_clear(values);

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
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    while (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      value = (DatabaseId)sqlite3_column_int64(statementHandle,0);
      Array_append(values,&value);
    }
    sqlite3_finalize(statementHandle);
    
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
  DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
  DATABASE_DOX(error,
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (int64)sqlite3_column_int64(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);
    
    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlString);

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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
  error = sqliteExecute(databaseHandle,
                        String_cString(sqlString),
                        CALLBACK(NULL,NULL),  // databaseRowFunction
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
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      String_delete(sqlString);
      return error;
    }
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = sqlite3_column_double(statementHandle,0);
    }
    sqlite3_finalize(statementHandle);

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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
  error = sqliteExecute(databaseHandle,
                        String_cString(sqlString),
                        CALLBACK(NULL,NULL),  // databaseRowFunction
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
    error = sqliteExecute(databaseHandle,
                          String_cString(sqlString),
                          CALLBACK(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          databaseHandle->timeout
                         );
    if (error != ERROR_NONE)
    {
      String_delete(sqlString);
      return error;
    }
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
               databaseHandle,
               DATABASE_LOCK_TYPE_READ,
  {
    #ifndef NDEBUG
      String_set(databaseHandle->current.sqlCommand,sqlString);
    #endif /* NDEBUG */

    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
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
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      String_setCString(string,(const char*)sqlite3_column_text(statementHandle,0));
    }
    sqlite3_finalize(statementHandle);
    
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
  assert(databaseHandle->handle != NULL);
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
  assert(databaseHandle->handle != NULL);
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
  error = sqliteExecute(databaseHandle,
                        String_cString(sqlString),
                        CALLBACK(NULL,NULL),  // databaseRowFunction
                        NULL,  // changedRowCount
                        databaseHandle->timeout
                       );
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
  assert(databaseHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(databaseHandle);
  assert(databaseHandle->handle != NULL);

  return (DatabaseId)sqlite3_last_insert_rowid(databaseHandle->handle);
}

#ifndef NDEBUG

void Database_debugEnable(bool enabled)
{
  if (enabled)
  {
    databaseDebugCounter++;
  }
  else
  {
    assert(databaseDebugCounter>0);

    databaseDebugCounter--;
  }
}

void Database_debugPrintInfo(void)
{
  const DatabaseHandle *databaseHandle;
  const DatabaseNode   *databaseNode;
  uint                 i;
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
                String_cString(databaseNode->fileName),
                databaseNode->openCount
               );
        LIST_ITERATE(&debugDatabaseHandleList,databaseHandle)
        {
          assert(databaseHandle->databaseNode != NULL);
          if (databaseHandle->databaseNode == databaseNode)
          {

            fprintf(stderr,
                    "    %s at %s, %lu\n",
                    Thread_getName(databaseHandle->threadId),
                    databaseHandle->fileName,
                    databaseHandle->lineNb
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
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->pendingReads); i++)
        {
          if (!Thread_equalThreads(databaseNode->pendingReads[i].threadId,THREAD_ID_NONE))
          {
            fprintf(stderr,
                    "    pending r  thread '%s' (%s) at %s, %u\n",
                    Thread_getName(databaseNode->pendingReads[i].threadId),
                    Thread_getIdString(databaseNode->pendingReads[i].threadId),
                    databaseNode->pendingReads[i].fileName,
                    databaseNode->pendingReads[i].lineNb
                   );
            debugDumpStackTrace(stderr,6,databaseNode->pendingReads[i].stackTrace,databaseNode->pendingReads[i].stackTraceSize,0);
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->reads); i++)
        {
          if (!Thread_equalThreads(databaseNode->reads[i].threadId,THREAD_ID_NONE))
          {
            fprintf(stderr,
                    "    locked  r  thread '%s' (%s) at %s, %u\n",
                    Thread_getName(databaseNode->reads[i].threadId),
                    Thread_getIdString(databaseNode->reads[i].threadId),
                    databaseNode->reads[i].fileName,
                    databaseNode->reads[i].lineNb
                   );
            debugDumpStackTrace(stderr,6,databaseNode->reads[i].stackTrace,databaseNode->reads[i].stackTraceSize,0);
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->pendingReadWrites); i++)
        {
          if (!Thread_equalThreads(databaseNode->pendingReadWrites[i].threadId,THREAD_ID_NONE))
          {
            fprintf(stderr,
                    "    pending rw thread '%s' (%s) at %s, %u\n",
                    Thread_getName(databaseNode->pendingReadWrites[i].threadId),
                    Thread_getIdString(databaseNode->pendingReadWrites[i].threadId),
                    databaseNode->pendingReadWrites[i].fileName,
                    databaseNode->pendingReadWrites[i].lineNb
                   );
            debugDumpStackTrace(stderr,6,databaseNode->pendingReadWrites[i].stackTrace,databaseNode->pendingReadWrites[i].stackTraceSize,0);
          }
        }
        for (i = 0; i < SIZE_OF_ARRAY(databaseNode->readWrites); i++)
        {
          if (!Thread_equalThreads(databaseNode->readWrites[i].threadId,THREAD_ID_NONE))
          {
            fprintf(stderr,
                    "    locked  rw thread '%s' (%s) at %s, %u\n",
                    Thread_getName(databaseNode->readWrites[i].threadId),
                    Thread_getIdString(databaseNode->readWrites[i].threadId),
                    databaseNode->readWrites[i].fileName,
                    databaseNode->readWrites[i].lineNb
                   );
            debugDumpStackTrace(stderr,6,databaseNode->readWrites[i].stackTrace,databaseNode->readWrites[i].stackTraceSize,0);
          }
        }
        if (!Thread_equalThreads(databaseNode->transaction.threadId,THREAD_ID_NONE))
        {
          fprintf(stderr,
                  "  transaction: thread '%s' (%s) at %s, %u\n",
                  Thread_getName(databaseNode->transaction.threadId),
                  Thread_getIdString(databaseNode->transaction.threadId),
                  databaseNode->transaction.fileName,
                  databaseNode->transaction.lineNb
                 );
          debugDumpStackTrace(stderr,4,databaseNode->transaction.stackTrace,databaseNode->transaction.stackTraceSize,0);
        }
        else
        {
          fprintf(stderr,
                  "  transaction: none\n"
                 );
        }
        if (!Thread_equalThreads(databaseNode->lastTrigger.threadId,THREAD_ID_NONE))
        {
          s = "-";
          switch (databaseNode->lastTrigger.type)
          {
            case DATABASE_LOCK_TYPE_NONE      : s = "NONE"; break;
            case DATABASE_LOCK_TYPE_READ      : s = "R";    break;
            case DATABASE_LOCK_TYPE_READ_WRITE: s = "RW";   break;
          }
          fprintf(stderr,
                  "  last trigger: %s thread '%s' (%s) at %s, %u\n",
                  s,
                  Thread_getName(databaseNode->lastTrigger.threadId),
                  Thread_getIdString(databaseNode->lastTrigger.threadId),
                  databaseNode->lastTrigger.fileName,
                  databaseNode->lastTrigger.lineNb
                 );
          fprintf(stderr,
                  "                r %u %u, rw %u %u, trans %u %u\n",
databaseNode->lastTrigger.pendingReadCount,
databaseNode->lastTrigger.readCount              ,
databaseNode->lastTrigger.pendingReadWriteCount  ,
databaseNode->lastTrigger.readWriteCount         ,
databaseNode->lastTrigger.pendingTransactionCount,
databaseNode->lastTrigger.transactionCount       
                 );
          debugDumpStackTrace(stderr,4,databaseNode->lastTrigger.stackTrace,databaseNode->lastTrigger.stackTraceSize,0);
        }
        else
        {
          fprintf(stderr,
                  "  transaction: none\n"
                 );
        }
        fprintf(stderr,"\n");
      }
      fprintf(stderr,"\n");
    }
    pthread_mutex_unlock(&debugConsoleLock);
  }
  pthread_mutex_unlock(&debugDatabaseLock);
}

void Database_debugPrintQueryInfo(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

//  DATABASE_DEBUG_SQLX(databaseQueryHandle->databaseHandle,"SQL query",databaseQueryHandle->sqlString);
  fprintf(stderr,"DEBUG database: %s: %s\n",String_cString(databaseQueryHandle->databaseHandle->databaseNode->fileName),String_cString(databaseQueryHandle->sqlString)); \
}

/***********************************************************************\
* Name   : debugPrintRow
* Purpose: print row call back
* Input  : userData - user data
*          count    - number of values
*          values   - values
*          columns  - column names
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL int debugPrintRow(void *userData, int count, char *values[], char *columns[])
{
  int i;

  assert(count >= 0);
  assert(values != NULL);
  assert(columns != NULL);

  UNUSED_VARIABLE(userData);

  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      printf("%s ",!stringIsEmpty(values[i]) ? values[i] : "''");
    }
    else
    {
      printf("- ");
    }
  }
  printf("\n");

  return SQLITE_OK;
}

void Database_debugDump(DatabaseHandle *databaseHandle, const char *tableName)
{
  String     sqlString;
  int        sqliteResult;
  const char *errorMessage;

  assert(databaseHandle != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
//                              "PRAGMA table_info(%s) \
//                              ",
//                              "names"
"SELECT name FROM sqlite_master WHERE type='table';"
                             );
  if (tableName != NULL)
  {
    String_appendChar(sqlString,' ');
    String_appendCString(sqlString,tableName);
  }
fprintf(stderr,"%s, %d: ss=%s\n",__FILE__,__LINE__,String_cString(sqlString));

  // execute SQL command
  DATABASE_DO(databaseHandle,
              DATABASE_LOCK_TYPE_READ,
  {
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                CALLBACK(debugPrintRow,NULL),
                                (char**)&errorMessage
                               );

    if      (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else if (sqliteResult != SQLITE_OK)
    {
      HALT_INTERNAL_ERROR("SQLite error: %s %s",errorMessage,sqlite3_errmsg(databaseHandle->handle));
    }
  });
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  // free resources
  String_delete(sqlString);
}

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
