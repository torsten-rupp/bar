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
#include "errors.h"

#include "sqlite3.h"

#include "database.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

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

/***************************** Variables *******************************/

#ifndef NDEBUG
  LOCAL uint databaseDebugCounter = 0;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define DATABASE_LOCK(databaseHandle,text) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)); \
fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,databaseHandle->locked.lineNb); \
      assert(databaseHandle->locked.lineNb == 0); \
      databaseHandle->locked.lineNb = __LINE__; \
      databaseHandle->locked.t0     = Misc_getTimestamp(); \
fprintf(stderr,"%s, %d: locked %s\n",__FILE__,__LINE__,text); \
    } \
    while (0)

  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
fprintf(stderr,"%s, %d: unlocked\n",__FILE__,__LINE__); \
      assert(databaseHandle != NULL); \
      \
      assert(databaseHandle->locked.lineNb != 0); \
      databaseHandle->locked.t1     = Misc_getTimestamp(); \
      databaseHandle->locked.lineNb = 0; \
fprintf(stderr,"%s, %d: %llu %llu\n",__FILE__,__LINE__,databaseHandle->locked.t0,databaseHandle->locked.t1); \
assert(databaseHandle->locked.t1 >= databaseHandle->locked.t0); \
fprintf(stderr,"%s, %d: t=%llums\n",__FILE__,__LINE__,(databaseHandle->locked.t1-databaseHandle->locked.t0)/1000LL); \
      assert(((databaseHandle->locked.t1-databaseHandle->locked.t0)/1000LL) < 1000LL); \
      sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)); \
    } \
    while (0)
#else /* NDEBUG */
  #define DATABASE_LOCK(databaseHandle,text) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)); \
    } \
    while (0)

  #define DATABASE_UNLOCK(databaseHandle) \
    do \
    { \
      assert(databaseHandle != NULL); \
      \
      sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)); \
    } \
    while (0)
#endif /* not NDEBUG */

#define DATABASE_QUERY_LOCK(databaseQueryHandle) \
  do \
  { \
    assert(databaseQueryHandle != NULL); \
    \
    DATABASE_LOCK(databaseQueryHandle->databaseHandle,String_cString(databaseQueryHandle->sqlString)); \
  } \
  while (0)
#define DATABASE_QUERY_UNLOCK(databaseQueryHandle) \
  do \
  { \
    assert(databaseQueryHandle != NULL); \
    \
    DATABASE_UNLOCK(databaseQueryHandle->databaseHandle); \
  } \
  while (0)

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
        fprintf(stderr,"DEBUG database: execution time=%llums\n",databaseQueryHandle->dt/1000LL); \
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
* Name   : executeCallback
* Purpose: SQLite3 callback wrapper
* Input  : userData - user data
*          count    - number of columns
*          values   - value array
*          names    - column names array
* Output : -
* Return : 0 if OK, 1 for abort
* Notes  : -
\***********************************************************************/

LOCAL int executeCallback(void *userData,
                          int  count,
                          char *values[],
                          char *names[]
                         )
{
  DatabaseRowCallback *databaseRowCallback = (DatabaseRowCallback*)userData;

  return databaseRowCallback->function(databaseRowCallback->userData,
                                       (int)count,
                                       (const char**)names,
                                       (const char**)values
                                      )
          ? 0
          : 1;
}

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
  #define SLEEP_TIME 500LL

  #ifdef HAVE_NANOSLEEP
    struct timespec ts;
  #endif /* HAVE_NANOSLEEP */

  UNUSED_VARIABLE(userData);

  #ifndef NDEBUG
    fprintf(stderr,"Warning: database busy handler called (%d)\n",n);
  #endif /* not NDEBUG */

  #if defined(PLATFORM_LINUX)
    #if   defined(HAVE_NANOSLEEP)
      ts.tv_sec  = (ulong)(SLEEP_TIME/1000LL);
      ts.tv_nsec = (ulong)((SLEEP_TIME%1000LL)*1000000);
      while (   (nanosleep(&ts,&ts) == -1)
             && (errno == EINTR)
            )
     {
        // nothing to do
      }
    #else
      sleep(1);
    #endif
  #elif defined(PLATFORM_WINDOWS)
    Sleep(SLEEP_TIME);
  #endif

  return (n < 50);

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
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
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
        columnNode->value.d = String_new();
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

    List_append(columnList,columnNode);
  }
  Database_finalize(&databaseQueryHandle1);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
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
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
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
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL const char *getDatabaseTypeString(DatabaseTypes type)
{
  const char *string;

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
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }

  return string;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode
                      )
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         uint              __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
                         DatabaseOpenModes databaseOpenMode
                        )
#endif /* NDEBUG */
{
  String directory;
  Errors error;
  int    sqliteMode;
  int    sqliteResult;

  assert(databaseHandle != NULL);

  // init variables
  databaseHandle->handle = NULL;
  #ifndef NDEBUG
    stringClear(databaseHandle->fileName);
    databaseHandle->locked.lineNb = 0;
    databaseHandle->locked.t0     = 0ULL;
    databaseHandle->locked.t1     = 0ULL;
  #endif /* not NDEBUG */

  if (fileName != NULL)
  {
    // create directory if needed
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
        return error;
      }
    }
    String_delete(directory);
  }

  // get mode
  sqliteMode = 0;
  switch (databaseOpenMode)
  {
    case DATABASE_OPENMODE_CREATE:    sqliteMode = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE; break;
    case DATABASE_OPENMODE_READ:      sqliteMode = SQLITE_OPEN_READONLY;                     break;
    case DATABASE_OPENMODE_READWRITE: sqliteMode = SQLITE_OPEN_READWRITE;                    break;
  }

  // open database
  if (fileName == NULL) fileName = ":memory:";
  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    return error;
  }
  #ifndef NDEBUG
    strncpy(databaseHandle->fileName,fileName,sizeof(databaseHandle->fileName)); databaseHandle->fileName[sizeof(databaseHandle->fileName)-1] = '\0';
  #endif /* not NDEBUG */

  // set busy timeout handler
  sqlite3_busy_handler(databaseHandle->handle,busyHandlerCallback,NULL);

  // register REGEXP functions
  sqlite3_create_function(databaseHandle->handle,
                          "regexp",
                          3,
                          SQLITE_ANY,
                          NULL,
                          regexpMatch,
                          NULL,
                          NULL
                         );

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
}

void Database_lock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//  DATABASE_LOCK(databaseHandle,"");
}
void Database_unlock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);
  assert(databaseHandle->handle != NULL);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//  DATABASE_UNLOCK(databaseHandle);
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
                           enabled ? "ON" : "OFF"
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
                          const char                *tableName,
                          DatabaseCopyTableFunction preCopyTableFunction,
                          void                      *preCopyTableUserData,
                          DatabaseCopyTableFunction postCopyTableFunction,
                          void                      *postCopyTableUserData,
                          const char                *fromAdditional,
                          ...
                         )
{
  Errors             error;
  DatabaseColumnList fromColumnList,toColumnList;
  DatabaseColumnNode *columnNode;
  String             sqlSelectString,sqlInsertString;
  va_list            arguments;
  sqlite3_stmt       *fromHandle,*toHandle;
  int                sqliteResult;
  uint               n;
  DatabaseColumnNode *toColumnNode;
  DatabaseId         lastRowId;

  assert(fromDatabaseHandle != NULL);
  assert(fromDatabaseHandle->handle != NULL);
  assert(toDatabaseHandle != NULL);
  assert(toDatabaseHandle->handle != NULL);
  assert(tableName != NULL);

  // get table columns
  error = getTableColumnList(&fromColumnList,fromDatabaseHandle,tableName);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = getTableColumnList(&toColumnList,toDatabaseHandle,tableName);
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
  formatSQLString(sqlSelectString," FROM %s",tableName);
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
//fprintf(stderr,"%s, %d: sqlSelectString=%s\n",__FILE__,__LINE__,String_cString(sqlSelectString));

  // create SQL insert statement string
  sqlInsertString = formatSQLString(String_new(),"INSERT INTO %s (",tableName);
  n = 0;
  LIST_ITERATE(&toColumnList,columnNode)
  {
    if (columnNode->type != DATABASE_TYPE_PRIMARY_KEY)
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
    if (columnNode->type != DATABASE_TYPE_PRIMARY_KEY)
    {
      if (n > 0) String_appendChar(sqlInsertString,',');
      String_appendChar(sqlInsertString,'?');
      n++;
    }
  }
  String_appendCString(sqlInsertString,");");
//fprintf(stderr,"%s, %d: sqlInsertString=%s\n",__FILE__,__LINE__,String_cString(sqlInsertString));

  // select rows in from-table
  BLOCK_DOX(error,
            { DATABASE_LOCK(fromDatabaseHandle,tableName);
              DATABASE_LOCK(toDatabaseHandle,tableName);
            },
            { DATABASE_UNLOCK(fromDatabaseHandle);
              DATABASE_UNLOCK(toDatabaseHandle);
            },
  {
    // create select statement
    DATABASE_DEBUG_SQL(fromDatabaseHandle,sqlSelectString);
    sqliteResult = sqlite3_prepare_v2(fromDatabaseHandle->handle,
                                      String_cString(sqlSelectString),
                                      -1,
                                      &fromHandle,
                                      NULL
                                     );
    if (sqliteResult != SQLITE_OK)
    {
      return ERRORX_(DATABASE,sqlite3_errcode(fromDatabaseHandle->handle),"%s",sqlite3_errmsg(fromDatabaseHandle->handle));
    }

    // create insert statement
    DATABASE_DEBUG_SQL(toDatabaseHandle,sqlInsertString);
    sqliteResult = sqlite3_prepare_v2(toDatabaseHandle->handle,
                                      String_cString(sqlInsertString),
                                      -1,
                                      &toHandle,
                                      NULL
                                     );
    if (sqliteResult != SQLITE_OK)
    {
      sqlite3_finalize(fromHandle);
      return ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s",sqlite3_errmsg(toDatabaseHandle->handle));
    }

    // copy rows
    while ((sqliteResult = sqlite3_step(fromHandle)) == SQLITE_ROW)
    {
      sqlite3_reset(toHandle);

      // get from values, set in toColumnList
      n = 0;
      LIST_ITERATE(&fromColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_TYPE_PRIMARY_KEY:
            columnNode->value.id = sqlite3_column_int64(fromHandle,n);
//fprintf(stderr,"%s, %d: DATABASE_TYPE_PRIMARY_KEY %d %s: %lld\n",__FILE__,__LINE__,n,columnNode->name,columnNode->value.id);
            break;
          case DATABASE_TYPE_INT64:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_INT64 %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
            }
            break;
          case DATABASE_TYPE_DOUBLE:
            String_setCString(columnNode->value.d,(const char*)sqlite3_column_text(fromHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DOUBLE %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.d,columnNode->value.d);
            }
            break;
          case DATABASE_TYPE_DATETIME:
            String_setCString(columnNode->value.i,(const char*)sqlite3_column_text(fromHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_DATETIME %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.i,columnNode->value.i);
            }
            break;
          case DATABASE_TYPE_TEXT:
            String_setCString(columnNode->value.text,(const char*)sqlite3_column_text(fromHandle,n));
//fprintf(stderr,"%s, %d: DATABASE_TYPE_TEXT %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            toColumnNode = findTableColumnNode(&toColumnList,columnNode->name);
            if (toColumnNode != NULL)
            {
              String_set(toColumnNode->value.text,columnNode->value.text);
            }
            break;
          case DATABASE_TYPE_BLOB:
            HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; // not reached
          #endif /* NDEBUG */
        }
        n++;
      }

      // call pre-copy callback (if defined)
      if (preCopyTableFunction != NULL)
      {
        error = preCopyTableFunction(&fromColumnList,&toColumnList,preCopyTableUserData);
        if (error != ERROR_NONE)
        {
          sqlite3_finalize(toHandle);
          sqlite3_finalize(fromHandle);
          return error;
        }
      }

      // set to value
      n = 0;
      LIST_ITERATE(&toColumnList,columnNode)
      {
        switch (columnNode->type)
        {
          case DATABASE_TYPE_PRIMARY_KEY:
            // can not be set
            break;
          case DATABASE_TYPE_INT64:
//fprintf(stderr,"%s, %d: DATABASE_TYPE_INT64 %d %s: %s %d\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.i),sqlite3_column_type(fromHandle,n));
            sqlite3_bind_text(toHandle,n,String_cString(columnNode->value.i),-1,NULL);
            break;
          case DATABASE_TYPE_DOUBLE:
            sqlite3_bind_text(toHandle,n,String_cString(columnNode->value.d),-1,NULL);
            break;
          case DATABASE_TYPE_DATETIME:
            sqlite3_bind_text(toHandle,n,String_cString(columnNode->value.d),-1,NULL);
            break;
          case DATABASE_TYPE_TEXT:
//fprintf(stderr,"%s, %d: DATABASE_TYPE_TEXT %d %s: %s\n",__FILE__,__LINE__,n,columnNode->name,String_cString(columnNode->value.text));
            sqlite3_bind_text(toHandle,n,String_cString(columnNode->value.text),-1,NULL);
            break;
          case DATABASE_TYPE_BLOB:
            HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; // not reached
          #endif /* NDEBUG */
        }
        n++;
      }

      // insert row
      if (sqlite3_step(toHandle) != SQLITE_DONE)
      {
        sqlite3_finalize(toHandle);
        sqlite3_finalize(fromHandle);
        return ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),"%s",sqlite3_errmsg(toDatabaseHandle->handle));
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
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; // not reached
          #endif /* NDEBUG */
        }
      }

      // call post-copy callback (if defined)
      if (postCopyTableFunction != NULL)
      {
        error = postCopyTableFunction(&fromColumnList,&toColumnList,postCopyTableUserData);
        if (error != ERROR_NONE)
        {
          sqlite3_finalize(toHandle);
          sqlite3_finalize(fromHandle);
          return error;
        }
      }
    }

    // free resources
    sqlite3_finalize(toHandle);
    sqlite3_finalize(fromHandle);

    return ERROR_NONE;
  });

  // free resources
  String_delete(sqlInsertString);
  String_delete(sqlSelectString);
  freeTableColumnList(&toColumnList);
  freeTableColumnList(&fromColumnList);

  return error;
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
      return String_toInteger64(columnNode->value.d,STRING_BEGIN,NULL,NULL,0);
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
    return String_toInteger64(columnNode->value.d,STRING_BEGIN,NULL,NULL,0);
  }
  else
  {
    return defaultValue;
  }
}

const char *Database_getTableColumnListText(const DatabaseColumnList *columnList, const char *columnName, const char *defaultValue)
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
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Database_setTableColumnListText(const DatabaseColumnList *columnList, const char *columnName, ConstString value)
{
  return Database_setTableColumnListTextCString(columnList,columnName,String_cString(value));
}

bool Database_setTableColumnListTextCString(const DatabaseColumnList *columnList, const char *columnName, const char *value)
{
  DatabaseColumnNode *columnNode;

  columnNode = findTableColumnNode(columnList,columnName);
  if (columnNode != NULL)
  {
    assert(columnNode->type == DATABASE_TYPE_TEXT);
    String_setCString(columnNode->value.text,value);
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
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
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
  sqlite3_stmt             *handle;
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
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                NULL,
                                NULL,
                                NULL
                               );
    if (sqliteResult != SQLITE_OK)
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }

    // copy old table -> new table
    formatSQLString(String_clear(sqlString),"SELECT * FROM %s;",tableName);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &handle,
                                      NULL
                                     );
    if (sqliteResult != SQLITE_OK)
    {
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }

    // copy table rows
    while (sqlite3_step(handle) == SQLITE_ROW)
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
              formatSQLString(sqlString,"%'s",sqlite3_column_text(handle,column));
              break;
            case DATABASE_TYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; // not reached
            #endif /* NDEBUG */
          }
          n++;
        }

        column++;
      }
      String_appendCString(sqlString,");");

      // execute SQL command
      DATABASE_DEBUG_SQL(databaseHandle,sqlString);
      sqliteResult = sqlite3_exec(databaseHandle->handle,
                                  String_cString(sqlString),
                                  NULL,
                                  NULL,
                                  NULL
                                 );
      if (sqliteResult != SQLITE_OK)
      {
        return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
      }
    }

    // done table
    sqlite3_finalize(handle);

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
  DatabaseRowCallback databaseRowCallback;
  int                 sqliteResult;

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
fprintf(stderr,"%s, %d: command=%s\n",__FILE__,__LINE__,command);
  BLOCK_DOX(error,
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    databaseRowCallback.function = databaseRowFunction;
    databaseRowCallback.userData = databaseRowUserData;
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                (databaseRowFunction != NULL) ? executeCallback : NULL,
                                (databaseRowFunction != NULL) ? &databaseRowCallback : NULL,
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
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
  error = ERROR_NONE;
  BLOCK_DO(DATABASE_QUERY_LOCK(databaseQueryHandle),
           DATABASE_QUERY_UNLOCK(databaseQueryHandle),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
//    DATABASE_DEBUG_QUERY_PLAN(databaseHandle,sqlString);

    DATABASE_DEBUG_TIME_START(databaseQueryHandle);
    {
      sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                        String_cString(sqlString),
                                        -1,
                                        &databaseQueryHandle->handle,
                                        NULL
                                       );
    }
    DATABASE_DEBUG_TIME_END(databaseQueryHandle);
    if (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }
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

  va_start(arguments,format);
  BLOCK_DOX(result,
            DATABASE_QUERY_LOCK(databaseQueryHandle),
            DATABASE_QUERY_UNLOCK(databaseQueryHandle),
  {
fprintf(stderr,"%s, %d: //////////\n",__FILE__,__LINE__);
asm("nop");
asm("nop");
asm("nop");
asm("nop");
uint64 t2;
uint64 t=Misc_getTimestamp2(&t2);
asm("nop");
asm("nop");
asm("nop");
asm("nop");
fprintf(stderr,"%s, %d: a t=%016llx %016llx %016llx %016llx\n",__FILE__,__LINE__,t,t,t2,t2);
assert((t & 0x8000000000000000ULL) == 0x0000000000000000ULL);
assert(t == t2);
    DATABASE_DEBUG_TIME_START(databaseQueryHandle);
    if (sqlite3_step(databaseQueryHandle->handle) == SQLITE_ROW)
    {
      // get data
      column = 0;
      while ((*format) != '\0')
      {
fprintf(stderr,"%s, %d: b %llums\n",__FILE__,__LINE__,Misc_getTimestamp()/1000ULL);
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
                (*value.b) = (sqlite3_column_int(databaseQueryHandle->handle,column) == 1);
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
                  (*value.ll) = (int64)sqlite3_column_int64(databaseQueryHandle->handle,column);
                }
              }
              else if (longFlag)
              {
                value.l = va_arg(arguments,long*);
                if (value.l != NULL)
                {
                  (*value.l) = (long)sqlite3_column_int64(databaseQueryHandle->handle,column);
                }
              }
              else
              {
                value.i = va_arg(arguments,int*);
                if (value.i != NULL)
                {
                  (*value.i) = sqlite3_column_int(databaseQueryHandle->handle,column);
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
                  (*value.ull) = (uint64)sqlite3_column_int64(databaseQueryHandle->handle,column);
                }
              }
              else if (longFlag)
              {
                value.ul = va_arg(arguments,ulong*);
                if (value.ul != NULL)
                {
                  (*value.ul) = (ulong)sqlite3_column_int64(databaseQueryHandle->handle,column);
                }
              }
              else
              {
                value.ui = va_arg(arguments,uint*);
                if (value.ui != NULL)
                {
                  (*value.ui) = (uint)sqlite3_column_int(databaseQueryHandle->handle,column);
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
                  (*value.d) = atof((const char*)sqlite3_column_text(databaseQueryHandle->handle,column));
                }
              }
              else
              {
                value.f = va_arg(arguments,float*);
                if (value.f != NULL)
                {
                  (*value.f) = (float)atof((const char*)sqlite3_column_text(databaseQueryHandle->handle,column));
                }
              }
              break;
            case 'c':
              // char
              format++;

              value.ch = va_arg(arguments,char*);
              if (value.ch != NULL)
              {
                (*value.ch) = ((char*)sqlite3_column_text(databaseQueryHandle->handle,column))[0];
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
                  strncpy(value.s,(const char*)sqlite3_column_text(databaseQueryHandle->handle,column),maxLength-1);
                  value.s[maxLength-1] = '\0';
                }
                else
                {
                  strcpy(value.s,(const char*)sqlite3_column_text(databaseQueryHandle->handle,column));
                }
              }
              break;
            case 'S':
              // string
              format++;

              value.string = va_arg(arguments,String);
              if (value.string != NULL)
              {
                String_setCString(value.string,(const char*)sqlite3_column_text(databaseQueryHandle->handle,column));
              }
              break;
            case 'p':
              // text via pointer
              format++;

              value.p = va_arg(arguments,void*);
              if (value.p != NULL)
              {
                (*value.p) = (void*)sqlite3_column_text(databaseQueryHandle->handle,column);
              }
              break;
            default:
              return FALSE;
              break; /* not reached */
          }

          column++;
        }
      }
fprintf(stderr,"%s, %d: c %llu\n",__FILE__,__LINE__,Misc_getTimestamp()/1000ULL);

      return TRUE;
    }
    else
    {
      return FALSE;
    }
    DATABASE_DEBUG_TIME_END(databaseQueryHandle);
  });
  va_end(arguments);
fprintf(stderr,"%s, %d: format done=%s\n",__FILE__,__LINE__,format);

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

  BLOCK_DO(DATABASE_QUERY_LOCK(databaseQueryHandle),
           DATABASE_QUERY_UNLOCK(databaseQueryHandle),
  {
    DATABASE_DEBUG_TIME_START(databaseQueryHandle);
    {
      sqlite3_finalize(databaseQueryHandle->handle);
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
  sqlite3_stmt *handle;
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
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    bool existsFlag = FALSE;

    DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &handle,
                                      NULL
                                     );
    if (sqliteResult == SQLITE_OK)
    {
      if (sqlite3_step(handle) == SQLITE_ROW)
      {
        existsFlag = TRUE;
      }
    }

    sqlite3_finalize(handle);

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
  sqlite3_stmt *handle;
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
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"get int64",sqlString);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &handle,
                                      NULL
                                     );
    if (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }

    if (sqlite3_step(handle) == SQLITE_ROW)
    {
      (*value) = (int64)sqlite3_column_int64(handle,0);
    }

    sqlite3_finalize(handle);

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
  int     sqliteResult;

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
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                NULL,
                                NULL,
                                NULL
                               );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse");
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }

    return error;
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
              DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
              DATABASE_UNLOCK(databaseHandle),
    {
      DATABASE_DEBUG_SQLX(databaseHandle,"set int64",sqlString);
      sqliteResult = sqlite3_exec(databaseHandle->handle,
                                  String_cString(sqlString),
                                  NULL,
                                  NULL,
                                  NULL
                                 );
      if      (sqliteResult == SQLITE_OK)
      {
        error = ERROR_NONE;
      }
      else if (sqliteResult == SQLITE_MISUSE)
      {
        HALT_INTERNAL_ERROR("SQLite library reported misuse");
      }
      else
      {
        error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
      }

      return error;
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
  sqlite3_stmt *handle;
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
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"get string",sqlString);
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &handle,
                                      NULL
                                     );
    if (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
    }

    if (sqlite3_step(handle) == SQLITE_ROW)
    {
      String_setCString(value,(const char*)sqlite3_column_text(handle,0));
    }

    sqlite3_finalize(handle);

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
            DATABASE_LOCK(databaseHandle,String_cString(sqlString)),
            DATABASE_UNLOCK(databaseHandle),
  {
    DATABASE_DEBUG_SQLX(databaseHandle,"set string",sqlString);
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                NULL,
                                NULL,
                                NULL
                               );
    if      (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else if (sqliteResult == SQLITE_MISUSE)
    {
      HALT_INTERNAL_ERROR("SQLite library reported misuse");
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),"%s",sqlite3_errmsg(databaseHandle->handle));
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

//#ifndef NDEBUG

void Database_debugEnable(bool enabled)
{
#if 0
  if (enabled)
  {
    databaseDebugCounter++;
  }
  else
  {
    assert(databaseDebugCounter>0);

    databaseDebugCounter--;
  }
#endif
}

void Database_debugPrintQueryInfo(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

//  DATABASE_DEBUG_SQLX(databaseQueryHandle->databaseHandle,"SQL query",databaseQueryHandle->sqlString);
}

//#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
