/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Database functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
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
#include "files.h"
#include "misc.h"
#include "semaphores.h"
#include "errors.h"

#include "sqlite3.h"

#include "database.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#if 1
  #define DEBUG_WARNING_LOCK_TIME  2ULL*1000ULL    // DEBUG only: warning lock time [ms]
  #define DEBUG_MAX_LOCK_TIME     60ULL*1000ULL    // DEBUG only: max. lock time [ms]
#else
  #define DEBUG_WARNING_LOCK_TIME MAX_UINT64
  #define DEBUG_MAX_LOCK_TIME     MAX_UINT64
#endif

#ifndef NDEBUG
  #define MAX_THREADS 64
#endif /* not NDEBUG */

/***************************** Datatypes *******************************/

// callback function
typedef struct
{
  DatabaseRowFunction function;
  void                *userData;
} DatabaseRowCallback;

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
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */
    void  *(*startCode)(void*);
    void  *argument;
  } StackTraceThreadInfo;
#endif /* not NDEBUG */

/***************************** Variables *******************************/

#ifndef NDEBUG
  LOCAL uint  databaseDebugCounter = 0;

  LOCAL pthread_mutex_t databaseThreadInfoLock = PTHREAD_MUTEX_INITIALIZER;
  LOCAL struct
  {
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  }                     databaseThreadInfo[MAX_THREADS];
  LOCAL uint            databaseThreadInfoCount = 0;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define DATABASE_LOCK(databaseHandle,format,...) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_enter(databaseHandle->lock); \
      if (format != NULL) \
      { \
        stringFormat(databaseHandle->locked.text,sizeof(databaseHandle->locked.text),format, ## __VA_ARGS__); \
      } \
      else \
      { \
        stringClear(databaseHandle->locked.text); \
      } \
      databaseHandle->locked.lineNb = __LINE__; \
      databaseHandle->locked.t0     = Misc_getTimestamp(); \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database: locked for '%s'\n\n",databaseHandle->locked.text); \
      } \
    } \
    while (0)
  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      databaseHandle->locked.t1 = Misc_getTimestamp(); \
      assert(databaseHandle->locked.t1 >= databaseHandle->locked.t0); \
      if ((databaseDebugCounter > 0) && (((databaseHandle->locked.t1-databaseHandle->locked.t0)/1000LL) > 100LL)) \
      { \
        fprintf(stderr, \
                "DEBUG database: locked time %llums: %s\n", \
                (databaseHandle->locked.t1-databaseHandle->locked.t0)/1000ULL, \
                databaseHandle->locked.text \
               ); \
      } \
      if      (!stringIsEmpty(databaseHandle->locked.text) && (((databaseHandle->locked.t1-databaseHandle->locked.t0)/1000ULL) > DEBUG_WARNING_LOCK_TIME)) \
      { \
        fprintf(stderr, \
                "DEBUG database: warning long locked time %llums: %s\n", \
                (databaseHandle->locked.t1-databaseHandle->locked.t0)/1000ULL, \
                databaseHandle->locked.text \
               ); \
      } \
      else if (!stringIsEmpty(databaseHandle->locked.text) && (((databaseHandle->locked.t1-databaseHandle->locked.t0)/1000ULL) > DEBUG_MAX_LOCK_TIME)) \
      { \
        HALT(128, \
             "DEBUG database: lock time exceeded %llums: %s\n", \
             (databaseHandle->locked.t1-databaseHandle->locked.t0)/1000ULL, \
             databaseHandle->locked.text \
            ); \
      } \
      databaseHandle->locked.lineNb = 0; \
      sqlite3_mutex_leave(databaseHandle->lock); \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_LOCK(databaseHandle,format,...) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_enter(databaseHandle->lock); \
    } \
    while (0)

  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_leave(databaseHandle->lock); \
    } \
    while (0)
#endif /* not NDEBUG */

#ifndef NDEBUG
  #define DATABASE_DEBUG_SQL(databaseHandle,sqlString) \
    do \
    { \
      assert(databaseHandle != NULL); \
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
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
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
* Name   : vformatSQLString
* Purpose: format SQL string from command
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

        // get format char
        switch (*s)
        {
          case 'd':
            // integer
            s++;

            if      (longLongFlag)
            {
              value.ll = va_arg(arguments,int64);
              String_format(sqlString,"%lld",value.ll);
            }
            else if (longFlag)
            {
              value.l = va_arg(arguments,int64);
              String_format(sqlString,"%ld",value.l);
            }
            else
            {
              value.i = va_arg(arguments,int);
              String_format(sqlString,"%d",value.i);
            }
            break;
          case 'u':
            // unsigned integer
            s++;

            if      (longLongFlag)
            {
              value.ull = va_arg(arguments,uint64);
              String_format(sqlString,"%llu",value.ull);
            }
            else if (longFlag)
            {
              value.ul = va_arg(arguments,ulong);
              String_format(sqlString,"%lu",value.ul);
            }
            else
            {
              value.ui = va_arg(arguments,uint);
              String_format(sqlString,"%u",value.ui);
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

            if (quoteFlag) String_appendChar(sqlString,'\'');
            value.string = va_arg(arguments,String);
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
* Name   : vformatSQLString
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
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm  *tm;

  assert(context != NULL);
  assert(argc >= 1);
  assert(argv != NULL);

  UNUSED_VARIABLE(argc);

  // get text to convert, optional date/time format
  text   = (const char*)sqlite3_value_text(argv[0]);
  format = (argc >= 2) ? argv[1] : NULL;

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
  directoryName = File_getFilePathNameCString(String_new(),string);

  // store result
  sqlite3_result_text(context,String_cString(directoryName),-1,SQLITE_TRANSIENT);

  // free resources
  String_delete(directoryName);
}

/***********************************************************************\
* Name   : delay
* Purpose: delay program execution
* Input  : time - delay time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delay(ulong time)
{
  #if   defined(USLEEP)
  #elif defined(HAVE_NANOSLEEP)
    struct timespec ts;
  #endif /* HAVE_NANOSLEEP */

  #if   defined(USLEEP)
    usleep(time*1000L);
  #elif defined(HAVE_NANOSLEEP)
    ts.tv_sec  = time/1000L;
    ts.tv_nsec = (time%1000L)*1000000L;
    while (   (nanosleep(&ts,&ts) == -1)
           && (errno == EINTR)
          )
    {
      // nothing to do
    }
  #elif defined(PLATFORM_WINDOWS)
    Sleep(time);
  #else
    #error usleep()/nanosleep() not available nor Windows system!
  #endif
}

#if 0
//TODO
/***********************************************************************\
* Name   : executeCallback
* Purpose: SQLite3 call-back wrapper
* Input  : userData - user data
*          count    - number of columns
*          values   - value array
*          names    - column names array
* Output : -
* Return : TRUE if OK, FALSE for abort
* Notes  : -
\***********************************************************************/

LOCAL bool executeCallback(void *userData
                           int  count,
                           char *values[],
                           char *names[]
                          )
{
  DatabaseRowCallback *databaseRowCallback = (DatabaseRowCallback*)userData;

  assert(databaseRowCallback != NULL);

  return databaseRowCallback->function((int)count,
                                       (const char**)names,
                                       (const char**)values,
                                       databaseRowCallback->userData
                                      )
          ? 0
          : 1;
}
#endif

/***********************************************************************\
* Name   : busyHandlerCallback
* Purpose: SQLite3 busy handler callback
* Input  : userData - user data
*          n        - number of calls
* Output : -
* Return : 1 for wait, 0 for abort
* Notes  : -
\***********************************************************************/

LOCAL int busyHandlerCallback(void *userData, int n)
{
  #define SLEEP_TIME 1000L

  DatabaseHandle *databaseHandle = (DatabaseHandle*)userData;

  assert(databaseHandle != NULL);

  #ifndef NDEBUG
//    fprintf(stderr,"Warning: database busy handler called %p: %d\n",pthread_self(),n);
  #endif /* not NDEBUG */
if (n > 10)
{
fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,n);
//asm("int3");
}

  delay(SLEEP_TIME);

  return ((databaseHandle->timeout == WAIT_FOREVER) || (n < databaseHandle->timeout)) ? 1 : 0;

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : unlockCallback
* Purpose: SQLite3 busy handler callback
* Input  : statementHandle - statement ahndle
* Output : -
* Return : SQLite result code
* Notes  : -
\***********************************************************************/

LOCAL void unlockNotifyCallback(void *argv[], int argc)
{
  sem_t *semaphore;
  assert(argv != NULL);
  assert(argc >= 1);

  semaphore = (sem_t*)argv[0];

  assert(semaphore != NULL);
fprintf(stderr,"%s, %d: jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj\n",__FILE__,__LINE__);
  sem_post(semaphore);
}

/***********************************************************************\
* Name   : waitUnlockNotify
* Purpose: SQLite3 busy handler callback
* Input  : statementHandle - statement ahndle
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
* Purpose: SQLite3 busy handler callback
* Input  : handle          - database handle
*          statementHandle - statement handle
*          timeout         - timeout [ms]
* Output : -
* Return : SQLite result code
* Notes  : -
\***********************************************************************/

LOCAL int sqliteStep(sqlite3 *handle, sqlite3_stmt *statementHandle, long timeout)
{
  #define SLEEP_TIME 1000L

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
    else if (sqliteResult == SQLITE_BUSY)
    {
      delay(SLEEP_TIME);
      sqlite3_reset(statementHandle);
      n++;
    }
  }
  while (   ((sqliteResult == SQLITE_LOCKED) || (sqliteResult == SQLITE_BUSY))
         && ((timeout == WAIT_FOREVER) || (n < (timeout+SLEEP_TIME-1L)/SLEEP_TIME))
        );

  return sqliteResult;

  #undef SLEEP_TIME
}

/***********************************************************************\
* Name   : sqliteExecute
* Purpose: excute SQLite3 statement
* Input  : handle              - SQLite3 handle
*          sqlString           - SQL string
*          databaseRowFunction - row call-back function
*          databaseRowUserData - user data for row call-back
*          timeout             - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sqliteExecute(DatabaseHandle      *databaseHandle,
                           const char          *sqlString,
                           DatabaseRowFunction databaseRowFunction,
                           void                *databaseRowUserData,
                           long                timeout
                          )
{
  #define SLEEP_TIME 1000L

  const char          *nextSqlCommand;
  DatabaseRowCallback databaseRowCallback;
  Errors              error;
  int                 sqliteResult;
  sqlite3_stmt        *statementHandle;
  uint                count;
  const char          **names,**values;
  uint                n;
  uint                i;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

#if 0
  databaseRowCallback.function = databaseRowFunction;
  databaseRowCallback.userData = databaseRowUserData;
  do
  {
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                (databaseRowFunction != NULL) ? executeCallback : NULL,
                                (databaseRowFunction != NULL) ? &databaseRowCallback : NULL,
                                NULL
                               );
    if (sqliteResult == SQLITE_LOCKED)
    {
      xxx=0;
      do
      {
        sqlite3_unlock_notify(databaseHandle->handle,unlockCallback,0);
        delay(SLEEP_TIME);
      }
      while (xxx==0);
    }
  }
  while (sqliteResult == SQLITE_LOCKED);
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
    error = ERRORX_(DATABASE,sqlite3_errcode(handle),"%s: %s",sqlite3_errmsg(handle),String_cString(sqlString));
  }
#else
  nextSqlCommand = stringTrim(sqlString);
  error          = ERROR_NONE;
  while (   (error == ERROR_NONE)
         && !stringIsEmpty(nextSqlCommand)
        )
  {
//fprintf(stderr,"%s, %d: sqlCommands='%s'\n",__FILE__,__LINE__,sqlCommands);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      nextSqlCommand,
                                      -1,
                                      &statementHandle,
                                      &nextSqlCommand
                                     );
    if (sqliteResult == SQLITE_OK)
    {
      assert(statementHandle != NULL);

      // allocate call-back data
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
        n = 0;
        do
        {
          sqliteResult = sqlite3_step(statementHandle);
          if      (sqliteResult == SQLITE_LOCKED)
          {
fprintf(stderr,"%s, %d:gsdgsdfgdfg \n",__FILE__,__LINE__);
            waitUnlockNotify(databaseHandle->handle);
            sqlite3_reset(statementHandle);
          }
          else if (sqliteResult == SQLITE_BUSY)
          {
            delay(SLEEP_TIME);
            sqlite3_reset(statementHandle);
            n++;
          }
          else if (sqliteResult == SQLITE_MISUSE)
          {
            HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
          }
        }
        while (   ((sqliteResult == SQLITE_LOCKED) || (sqliteResult == SQLITE_BUSY))
               && ((timeout == WAIT_FOREVER) || (n < (timeout+SLEEP_TIME-1L)/SLEEP_TIME))
              );
if (sqliteResult == SQLITE_BUSY)
{
fprintf(stderr,"%s, %d: still SQLITE_BUSY %d %ld\n",__FILE__,__LINE__,n,timeout);
debugDumpCurrentStackTrace(stderr,0,0);
asm("int3");
sqlite3_reset(statementHandle);
sqliteResult = sqlite3_step(statementHandle);
}

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
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
asm("int3");
            error = databaseRowFunction(count,names,values,databaseRowUserData);
  //TODO callback
          }
        }
      }
      while ((error == ERROR_NONE) && (sqliteResult == SQLITE_ROW));
      if (sqliteResult != SQLITE_DONE)
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),sqlString);
fprintf(stderr,"%s, %d: %d %s -- %s\n",__FILE__,__LINE__,sqliteResult,Error_getText(error),sqlString);
      }

      // free call-back data
      if (databaseRowFunction != NULL)
      {
        free(values);
        free(names);
      }
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse %d %d",sqliteResult,sqlite3_extended_errcode(databaseHandle->handle));
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),sqlString);
    }

    nextSqlCommand = stringTrim(nextSqlCommand);
  }

  sqlite3_finalize(statementHandle);
#endif

  return error;

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
      break;
    case DATABASE_TYPE_DOUBLE:
      break;
    case DATABASE_TYPE_DATETIME:
      break;
    case DATABASE_TYPE_TEXT:
      String_delete(columnNode->value.text);
      break;
    case DATABASE_TYPE_BLOB:
//TODO: blob
      HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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

  List_init(columnList);

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
      HALT_INTERNAL_ERROR("Unknown database data type '%s' for '%s'",type,name);
    }
    columnNode->usedFlag = FALSE;

    List_append(columnList,columnNode);
  }
  Database_finalize(&databaseQueryHandle1);

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
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; // not reached
  }

  return string;
}

/*---------------------------------------------------------------------*/

Errors Database_initAll(void)
{
  int sqliteResult;

  sqliteResult = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
  if (sqliteResult != SQLITE_OK)
  {
    return ERRORX_(DATABASE,sqliteResult,"enable multi-threading");
  }

  return ERROR_NONE;
}

void Database_doneAll(void)
{
}

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode,
                       long              timeout
                      )
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         uint              __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
                         DatabaseOpenModes databaseOpenMode,
                         long              timeout
                        )
#endif /* NDEBUG */
{
  String directory;
  Errors error;
  int    sqliteMode;
  int    sqliteResult;

  assert(databaseHandle != NULL);

  // init variables
  databaseHandle->lock    = NULL;
  databaseHandle->handle  = NULL;
  databaseHandle->timeout = timeout;
  sem_init(&databaseHandle->wakeUp,0,0);
  #ifndef NDEBUG
    stringClear(databaseHandle->fileName);
    databaseHandle->locked.lineNb              = 0;
    databaseHandle->locked.t0                  = 0ULL;
    databaseHandle->locked.t1                  = 0ULL;
    databaseHandle->transaction.fileName       = NULL;
    databaseHandle->transaction.lineNb         = 0;
    databaseHandle->transaction.stackTraceSize = 0;
  #endif /* not NDEBUG */

  // create lock
  databaseHandle->lock = sqlite3_mutex_alloc(SQLITE_MUTEX_RECURSIVE);
  if (databaseHandle->lock == NULL)
  {
    return ERRORX_(DATABASE,0,"create lock fail");
  }

  // create directory if needed
  if (fileName != NULL)
  {
    directory = File_getFilePathNameCString(String_new(),fileName);
    if (   !String_isEmpty(directory)
        && !File_isDirectory(directory)
       )
    {
      error = File_makeDirectory(directory,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSION
                                );
      if (error != ERROR_NONE)
      {
        File_deleteFileName(directory);
        sqlite3_mutex_free(databaseHandle->lock);
        sem_destroy(&databaseHandle->wakeUp);
        return error;
      }
    }
    String_delete(directory);
  }

  // get mode
  sqliteMode = 0;
  switch (databaseOpenMode)
  {
    case DATABASE_OPENMODE_CREATE:    sqliteMode |= SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE; break;
    case DATABASE_OPENMODE_READ:      sqliteMode |= SQLITE_OPEN_READONLY;                     break;
    case DATABASE_OPENMODE_READWRITE: sqliteMode |= SQLITE_OPEN_READWRITE;                    break;
  }
//sqliteMode |= SQLITE_OPEN_PRIVATECACHE;
//sqliteMode |= SQLITE_OPEN_NOMUTEX;

  // open database
  if (fileName == NULL) fileName = ":memory:";
  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    sqlite3_mutex_free(databaseHandle->lock);
    sem_destroy(&databaseHandle->wakeUp);
    return error;
  }
  #ifndef NDEBUG
    strncpy(databaseHandle->fileName,fileName,sizeof(databaseHandle->fileName)); databaseHandle->fileName[sizeof(databaseHandle->fileName)-1] = '\0';
  #endif /* not NDEBUG */

  // set busy timeout handler
  sqliteResult = sqlite3_busy_handler(databaseHandle->handle,busyHandlerCallback,databaseHandle);
  assert(sqliteResult == SQLITE_OK);

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

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(databaseHandle,sizeof(DatabaseHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,sizeof(DatabaseHandle));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Database_close(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  void __Database_close(const char   *__fileName__,
                        uint         __lineNb__,
                        DatabaseHandle *databaseHandle
                       )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(databaseHandle,sizeof(DatabaseHandle));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle,sizeof(DatabaseHandle));
  #endif /* NDEBUG */

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: close '%s'\n",databaseHandle->fileName);
  #endif

  // clear busy timeout handler
  sqlite3_busy_handler(databaseHandle->handle,NULL,NULL);

  // close database
  sqlite3_close(databaseHandle->handle);

  // free resources
  sqlite3_mutex_free(databaseHandle->lock);
  sem_destroy(&databaseHandle->wakeUp);
}

#ifdef NDEBUG
  void Database_lock(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  void __Database_lock(const char   *__fileName__,
                       uint         __lineNb__,
                       DatabaseHandle *databaseHandle
                      )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);

  sqlite3_mutex_enter(databaseHandle->lock);
  #ifdef NDEBUG
    databaseHandle->locked.fileName = __FILE__;
    databaseHandle->locked.lineNb   = __LINE__;
  #else /* not NDEBUG */
    databaseHandle->locked.fileName = __fileName__;
    databaseHandle->locked.lineNb   = __lineNb__;
  #endif /* NDEBUG */
  databaseHandle->locked.text[0] = '\0';
  databaseHandle->locked.t0      = Misc_getTimestamp();
}
void Database_unlock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  databaseHandle->locked.t1       = Misc_getTimestamp();
  databaseHandle->locked.lineNb   = 0; \
  databaseHandle->locked.fileName = NULL;
  sqlite3_mutex_leave(databaseHandle->lock); \
}

Errors Database_setEnabledSync(DatabaseHandle *databaseHandle,
                               bool           enabled
                              )
{
  Errors error;

  assert(databaseHandle != NULL);

  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),
                           "PRAGMA synchronous=%s;",
                           enabled ? "ON" : "OFF"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),
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

  return Database_execute(databaseHandle,
                          CALLBACK(NULL,NULL),
                          "PRAGMA foreign_keys=%s;",
                          enabled ? "ON" : "OFF"
                         );
}

Errors Database_copyTable(DatabaseHandle            *fromDatabaseHandle,
                          DatabaseHandle            *toDatabaseHandle,
                          const char                *fromTableName,
                          const char                *toTableName,
                          bool                      transactionFlag,
                          DatabaseCopyTableFunction preCopyTableFunction,
                          void                      *preCopyTableUserData,
                          DatabaseCopyTableFunction postCopyTableFunction,
                          void                      *postCopyTableUserData,
                          const char                *fromAdditional,
                          ...
                         )
{
  #define TRANSACTION_NAME "COPY_TABLE"

  Errors             error;
  DatabaseColumnList fromColumnList,toColumnList;
  DatabaseColumnNode *columnNode;
  String             sqlSelectString,sqlInsertString;
  va_list            arguments;
  sqlite3_stmt       *fromStatementHandle,*toStatementHandle;
  int                sqliteResult;
  uint               n;
  DatabaseColumnNode *toColumnNode;
  DatabaseId         lastRowId;

uint64 t0 = Misc_getTimestamp();
ulong xxx=0;

  assert(fromDatabaseHandle != NULL);
  assert(fromDatabaseHandle->handle != NULL);
  assert(toDatabaseHandle != NULL);
  assert(toDatabaseHandle->handle != NULL);
  assert(fromTableName != NULL);
  assert(toTableName != NULL);

  // get table columns
  error = getTableColumnList(&fromColumnList,fromDatabaseHandle,fromTableName);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = getTableColumnList(&toColumnList,toDatabaseHandle,toTableName);
  if (error != ERROR_NONE)
  {
    freeTableColumnList(&fromColumnList);
    return error;
  }

  // create SQL select statement string
  sqlSelectString = formatSQLString(String_new(),"SELECT ");
  n = 0;
  LIST_ITERATE(&fromColumnList,columnNode)
  {
    if (n > 0) String_appendChar(sqlSelectString,',');
    String_appendCString(sqlSelectString,columnNode->name);
    n++;
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
            { DATABASE_LOCK(fromDatabaseHandle,NULL);
              DATABASE_LOCK(toDatabaseHandle,"copy table to '%s'",toTableName);
            },
            { DATABASE_UNLOCK(fromDatabaseHandle);
              DATABASE_UNLOCK(toDatabaseHandle);
            },
  {
    Errors error;

#if 1
    if (transactionFlag)
    {
      error = Database_beginTransaction(toDatabaseHandle,TRANSACTION_NAME);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
#endif

    // create select statement
    sqliteResult = sqlite3_prepare_v2(fromDatabaseHandle->handle,
                                      String_cString(sqlSelectString),
                                      -1,
                                      &fromStatementHandle,
                                      NULL
                                     );
    if (sqliteResult != SQLITE_OK)
    {
fprintf(stderr,"%s, %d: 1\n",__FILE__,__LINE__);
      error = ERRORX_(DATABASE,sqlite3_errcode(fromDatabaseHandle->handle),"%s: %s",String_cString(sqlSelectString),sqlite3_errmsg(fromDatabaseHandle->handle));
      if (transactionFlag)
      {
        (void)Database_rollbackTransaction(toDatabaseHandle,TRANSACTION_NAME);
      }
      return error;
    }
    #ifndef NDEBUG
      if (fromStatementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//      assert(fromStatementHandle != NULL);
      }
    #endif /* not NDEBUG */

    // copy rows
    while ((sqliteResult = sqliteStep(fromDatabaseHandle->handle,fromStatementHandle,fromDatabaseHandle->timeout)) == SQLITE_ROW)
    {
xxx++;
      // reset to data
      LIST_ITERATE(&toColumnList,columnNode)
      {
        columnNode->usedFlag = FALSE;
      }
//      sqlite3_reset(toStatementHandle);

      // get from values, set in toColumnList
      n = 0;
      LIST_ITERATE(&fromColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_TYPE_PRIMARY_KEY:
            columnNode->value.id = sqlite3_column_int64(fromStatementHandle,n);
//fprintf(stderr,"%s, %d: DATABASE_TYPE_PRIMARY_KEY %d %s: %lld\n",__FILE__,__LINE__,n,columnNode->name,columnNode->value.id);
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              toColumnNode->value.id = DATABASE_ID_NONE;
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_INT64:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_INT64 %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_DOUBLE:
            String_setCString(columnNode->value.d,(const char*)sqlite3_column_text(fromStatementHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DOUBLE %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.d,columnNode->value.d);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_DATETIME:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromStatementHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DATETIME %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
              toColumnNode->usedFlag = TRUE;
            }
            break;
          case DATABASE_TYPE_TEXT:
            String_setCString(columnNode->value.text,(const char*)sqlite3_column_text(fromStatementHandle,n));
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
        n++;
      }

      // call pre-copy callback (if defined)
      if (preCopyTableFunction != NULL)
      {
        BLOCK_DOX(error,
                  DATABASE_UNLOCK(toDatabaseHandle),
                  DATABASE_LOCK(toDatabaseHandle,"copy table to '%s'",toTableName),
        {
          return preCopyTableFunction(&fromColumnList,&toColumnList,preCopyTableUserData);
        });
        if (error != ERROR_NONE)
        {
fprintf(stderr,"%s, %d: 2\n",__FILE__,__LINE__);
//          sqlite3_finalize(toStatementHandle);
          sqlite3_finalize(fromStatementHandle);
          if (transactionFlag)
          {
            (void)Database_rollbackTransaction(toDatabaseHandle,TRANSACTION_NAME);
          }
          return error;
        }

      }

      // create SQL insert statement string
      formatSQLString(String_clear(sqlInsertString),"INSERT INTO %s (",toTableName);
      n = 0;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        if (columnNode->usedFlag && (columnNode->type != DATABASE_TYPE_PRIMARY_KEY))
        {
          if (n > 0) String_appendChar(sqlInsertString,',');
          String_appendCString(sqlInsertString,columnNode->name);
          n++;
        }
      }
      String_appendCString(sqlInsertString,") VALUES (");
      n = 0;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        if (columnNode->usedFlag && (columnNode->type != DATABASE_TYPE_PRIMARY_KEY))
        {
          if (n > 0) String_appendChar(sqlInsertString,',');
          String_appendChar(sqlInsertString,'?');
          n++;
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
      if (sqliteResult != SQLITE_OK)
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle,TRANSACTION_NAME);
        }
        return error;
      }
      #ifndef NDEBUG
        if (toStatementHandle == NULL)
        {
          fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
          abort();
//          assert(toStatementHandle != NULL);
        }
      #endif /* not NDEBUG */

      // set to value
      n = 0;
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
              sqlite3_bind_text(toStatementHandle,n,String_cString(columnNode->value.i),-1,NULL);
              break;
            case DATABASE_TYPE_DOUBLE:
              sqlite3_bind_text(toStatementHandle,n,String_cString(columnNode->value.d),-1,NULL);
              break;
            case DATABASE_TYPE_DATETIME:
              sqlite3_bind_text(toStatementHandle,n,String_cString(columnNode->value.i),-1,NULL);
              break;
            case DATABASE_TYPE_TEXT:
  //fprintf(stderr,"%s, %d: DATABASE_TYPE_TEXT %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
              sqlite3_bind_text(toStatementHandle,n,String_cString(columnNode->value.text),-1,NULL);
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
      }

      // insert row
      if (sqliteStep(toDatabaseHandle->handle,toStatementHandle,toDatabaseHandle->timeout) != SQLITE_DONE)
      {
fprintf(stderr,"%s, %d: 4 %s %s\n",__FILE__,__LINE__,sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
        error = ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s: %s",sqlite3_errmsg(toDatabaseHandle->handle),String_cString(sqlInsertString));
        sqlite3_finalize(toStatementHandle);
        sqlite3_finalize(fromStatementHandle);
        if (transactionFlag)
        {
          (void)Database_rollbackTransaction(toDatabaseHandle,TRANSACTION_NAME);
        }
        return error;
      }
      lastRowId = (uint64)sqlite3_last_insert_rowid(toDatabaseHandle->handle);
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
                  DATABASE_UNLOCK(toDatabaseHandle),
                  DATABASE_LOCK(toDatabaseHandle,"copy table to '%s'",toTableName),
        {
          return postCopyTableFunction(&fromColumnList,&toColumnList,postCopyTableUserData);
        });
        if (error != ERROR_NONE)
        {
fprintf(stderr,"%s, %d: 5\n",__FILE__,__LINE__);
//          sqlite3_finalize(toStatementHandle);
          sqlite3_finalize(fromStatementHandle);
          if (transactionFlag)
          {
            (void)Database_rollbackTransaction(toDatabaseHandle,TRANSACTION_NAME);
          }
          return error;
        }
      }
    }

#if 1
    if (transactionFlag)
    {
      error = Database_endTransaction(toDatabaseHandle,TRANSACTION_NAME);
      if (error != ERROR_NONE)
      {
//        sqlite3_finalize(toStatementHandle);
        sqlite3_finalize(fromStatementHandle);
        return error;
      }
    }
#endif

    // free resources
//    sqlite3_finalize(toStatementHandle);
    sqlite3_finalize(fromStatementHandle);

if (xxx > 0)
{
uint64 t1 = Misc_getTimestamp();
fprintf(stderr,"%s, %d: %s->%s %llums xxx=%lu -> %lfms/trans\n",__FILE__,__LINE__,fromTableName,toTableName,(t1-t0)/1000,xxx,(double)(t1-t0)/((double)xxx*1000));
//exit(12);
}

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
    String_format(String_clear(columnNode->value.i),"%lld",value);
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
    String_format(String_clear(columnNode->value.d),"%f",value);
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
    String_format(String_clear(columnNode->value.i),"%lld",value);
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
    columnNode->value.blob.data   = data;
    columnNode->value.blob.length = length;
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
                           NULL,
                           NULL,
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
            DATABASE_LOCK(databaseHandle,"remove column"),
            DATABASE_UNLOCK(databaseHandle),
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
                          CALLBACK(NULL,NULL),
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
    if (sqliteResult != SQLITE_OK)
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//        assert(statementHandle != NULL);
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
              formatSQLString(sqlString,"%'s",sqlite3_column_text(databaseHandle->handle,column));
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
                            CALLBACK(NULL,NULL),
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
                           NULL,
                           NULL,
                           "ALTER TABLE %s RENAME TO __old__;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "ALTER TABLE __new__ RENAME TO %s;",
                           tableName
                          );
  if (error != ERROR_NONE)
  {
    (void)Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "ALTER TABLE __old__ RENAME TO %s;",
                           tableName
                          );
    (void)Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DROP TABLE __new__;"
                          );
    return error;
  }
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DROP TABLE __old__;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Database_beginTransaction(DatabaseHandle *databaseHandle,
                                   const char *name
                                  )
#else /* not NDEBUG */
  Errors __Database_beginTransaction(const char   *__fileName__,
                                     uint         __lineNb__,
                                     DatabaseHandle *databaseHandle,
                                     const char *name
                                    )
#endif /* NDEBUG */
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

  #ifndef NDEBUG
    if (databaseHandle->transaction.fileName != NULL)
    {
      fprintf(stderr,"DEBUG ERROR: multiple transactions requested thread %p at %s, %ld and previously thread %p at %s, %ld!\n",
              pthread_self(),
              __fileName__,
              __lineNb__,
              databaseHandle->transaction.threadId,
              databaseHandle->transaction.fileName,
              databaseHandle->transaction.lineNb
             );
      #ifdef HAVE_BACKTRACE
        debugDumpStackTrace(stderr,4,databaseHandle->transaction.stackTrace,databaseHandle->transaction.stackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("begin transactions fail");
    }
  #endif /* NDEBUG */

  // format SQL command string
//  sqlString = String_format(String_new(),"BEGIN TRANSACTION EXCLUSIVE;");
  sqlString = String_format(String_new(),"BEGIN TRANSACTION;");

  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
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

  #ifndef NDEBUG
    databaseHandle->transaction.threadId = pthread_self();
    databaseHandle->transaction.fileName = __fileName__;
    databaseHandle->transaction.lineNb   = __lineNb__;
    #ifdef HAVE_BACKTRACE
      databaseHandle->transaction.stackTraceSize = backtrace((void*)databaseHandle->transaction.stackTrace,SIZE_OF_ARRAY(databaseHandle->transaction.stackTrace));
    #endif /* HAVE_BACKTRACE */
  #endif /* NDEBUG */

  return ERROR_NONE;
}

Errors Database_endTransaction(DatabaseHandle *databaseHandle, const char *name)
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);
  assert(databaseHandle->transaction.fileName != NULL);

  #ifndef NDEBUG
    databaseHandle->transaction.fileName = NULL;
    databaseHandle->transaction.lineNb   = 0;
    #ifdef HAVE_BACKTRACE
      databaseHandle->transaction.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* NDEBUG */

  // format SQL command string
  sqlString = String_format(String_new(),"END TRANSACTION;");

  // end transaction
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
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

  // try to execute checkpoint
  sqlite3_wal_checkpoint(databaseHandle->handle,NULL);

  return ERROR_NONE;
}

Errors Database_rollbackTransaction(DatabaseHandle *databaseHandle, const char *name)
{
  String sqlString;
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);
  assert(databaseHandle->transaction.fileName != NULL);

fprintf(stderr,"%s, %d: Database_rollbackTransaction\n",__FILE__,__LINE__);
  #ifndef NDEBUG
    databaseHandle->transaction.fileName = NULL;
    databaseHandle->transaction.lineNb   = 0;
    #ifdef HAVE_BACKTRACE
      databaseHandle->transaction.stackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
  #endif /* NDEBUG */

  // format SQL command string
  sqlString = String_format(String_new(),"ROLLBACK TRANSACTION;");

  // rollback transaction
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
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

Errors Database_execute(DatabaseHandle      *databaseHandle,
                        DatabaseRowFunction databaseRowFunction,
                        void                *databaseRowUserData,
                        const char          *command,
                        ...
                       )
{
  String              sqlString;
  va_list             arguments;
  Errors              error;
//  DatabaseRowCallback databaseRowCallback;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);
  assert(command != NULL);

  // format SQL command string
  va_start(arguments,command);
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );
  va_end(arguments);

  // execute SQL command
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//    databaseRowCallback.function = databaseRowFunction;
//    databaseRowCallback.userData = databaseRowUserData;

    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
//TODO:
//                         (databaseRowFunction != NULL) ? executeCallback : NULL,
//                         (databaseRowFunction != NULL) ? &databaseRowCallback : NULL,
databaseRowFunction,
databaseRowUserData,
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
                            uint                __lineNb__,
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
  #endif /* not NDEBUG */

  // prepare SQL command execution
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseQueryHandle->databaseHandle,"%s",String_cString(databaseQueryHandle->sqlString)),
            DATABASE_UNLOCK(databaseQueryHandle->databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//    DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString);

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
    }
    #ifndef NDEBUG
      if (databaseQueryHandle->statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//      assert(databaseQueryHandle->statementHandle != NULL);
      }
    #endif /* not NDEBUG */

    return error;
  });
  if (error != ERROR_NONE)
  {
    #ifndef NDEBUG
      String_delete(databaseQueryHandle->sqlString);
    #endif /* not NDEBUG */
    String_delete(sqlString);
    return error;
  }

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
                           uint              __lineNb__,
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

  BLOCK_DO(DATABASE_LOCK(databaseQueryHandle->databaseHandle,"%s",String_cString(databaseQueryHandle->sqlString)),
           DATABASE_UNLOCK(databaseQueryHandle->databaseHandle),
  {
    DATABASE_DEBUG_TIME_START(databaseQueryHandle);
    {
      sqlite3_finalize(databaseQueryHandle->statementHandle);
    }
    DATABASE_DEBUG_TIME_END(databaseQueryHandle);
  });
  #ifndef NDEBUG
    DATABASE_DEBUG_TIME(databaseQueryHandle);
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
  BLOCK_DOX(existsFlag,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    bool existsFlag = FALSE;

    DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
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
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//        assert(statementHandle != NULL);
      }
    #endif /* not NDEBUG */

    sqlite3_finalize(statementHandle);

    return existsFlag;
  });

  // free resources
  String_delete(sqlString);

  return existsFlag;
}

Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            )
{
  String       sqlString;
  va_list      arguments;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
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
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//        assert(statementHandle != NULL);
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = (int64)sqlite3_column_int64(statementHandle,0);
    }

    sqlite3_finalize(statementHandle);

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

Errors Database_setInteger64(DatabaseHandle *databaseHandle,
                             int64          value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            )
{
  String  sqlString;
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
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
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
                         databaseHandle->timeout
                        );
  });
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
    BLOCK_DOX(error,
              DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
              DATABASE_UNLOCK(databaseHandle),
    {
      DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
      return sqliteExecute(databaseHandle,
                           String_cString(sqlString),
                           CALLBACK(NULL,NULL),
                           databaseHandle->timeout
                          );
    });

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
  String       sqlString;
  va_list      arguments;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
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
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"get double",sqlString);
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//        assert(statementHandle != NULL);
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      (*value) = sqlite3_column_double(statementHandle,0);
    }

    sqlite3_finalize(statementHandle);

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

Errors Database_setDouble(DatabaseHandle *databaseHandle,
                          double         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  String  sqlString;
  va_list arguments;
  Errors  error;

  assert(databaseHandle != NULL);
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
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"set double",sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
                         databaseHandle->timeout
                        );
  });
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
    BLOCK_DOX(error,
              DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
              DATABASE_UNLOCK(databaseHandle),
    {
      DATABASE_DEBUG_SQLX(databaseHandle,"set double",sqlString);
      return sqliteExecute(databaseHandle,
                           String_cString(sqlString),
                           CALLBACK(NULL,NULL),
                           databaseHandle->timeout
                          );
    });

    String_delete(sqlString);

    return error;
  }

  // free resources
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_getString(DatabaseHandle *databaseHandle,
                          String         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  String       sqlString;
  va_list      arguments;
  Errors       error;
  sqlite3_stmt *statementHandle;
  int          sqliteResult;

  assert(databaseHandle != NULL);
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
    va_start(arguments,additional);
    vformatSQLString(sqlString,
                     additional,
                     arguments
                    );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  // execute SQL command
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"get string",sqlString);
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s: %s",sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
    }
    #ifndef NDEBUG
      if (statementHandle == NULL)
      {
        fprintf(stderr,"%s, %d: SQLite prepare fail %d: %s\n%s\n",__FILE__,__LINE__,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle),String_cString(sqlString));
        abort();
//        assert(statementHandle != NULL);
      }
    #endif /* not NDEBUG */

    if (sqliteStep(databaseHandle->handle,statementHandle,databaseHandle->timeout) == SQLITE_ROW)
    {
      String_setCString(value,(const char*)sqlite3_column_text(statementHandle,0));
    }

    sqlite3_finalize(statementHandle);

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

Errors Database_setString(DatabaseHandle *databaseHandle,
                          const String   value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         )
{
  String  sqlString;
  va_list arguments;
  Errors  error;
  int     sqliteResult;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // format SQL command string
  sqlString = formatSQLString(String_new(),
                              "UPDATE %s \
                               SET %s=%'S \
                              ",
                              tableName,
                              columnName,
                              value
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

  // execute SQL command
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,"%s",String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"set string",sqlString);
    return sqliteExecute(databaseHandle,
                         String_cString(sqlString),
                         CALLBACK(NULL,NULL),
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

int64 Database_getLastRowId(DatabaseHandle *databaseHandle)
{
  int64 databaseId;

  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

  databaseId = DATABASE_ID_NONE;
  BLOCK_DO(DATABASE_LOCK(databaseHandle,"last row id"),
           DATABASE_UNLOCK(databaseHandle),
  {
    databaseId = (uint64)sqlite3_last_insert_rowid(databaseHandle->handle);
  });

  return databaseId;
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

void Database_debugPrintQueryInfo(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

//  DATABASE_DEBUG_SQLX(databaseQueryHandle->databaseHandle,"SQL query",databaseQueryHandle->sqlString);
}

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
