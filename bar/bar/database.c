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
  DatabaseFunction function;
  void             *userData;
} DatabaseCallback;

// table column definition list
typedef struct ColumnNode
{
  LIST_NODE_HEADER(struct ColumnNode);

  char          *name;
  DatabaseTypes type;
} ColumnNode;

typedef struct
{
  LIST_HEADER(ColumnNode);
} ColumnList;

/***************************** Variables *******************************/

#ifndef NDEBUG
  LOCAL uint databaseDebugCounter = 0;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define DATABASE_DEBUG_SQL(databaseHandle,sqlString) \
    do \
    { \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database: execute command: %s: %s\n",(databaseHandle)->fileName,String_cString(sqlString)); \
      } \
    } \
    while (0)
  #define DATABASE_DEBUG_SQLX(databaseHandle,text,sqlString) \
    do \
    { \
      if (databaseDebugCounter > 0) \
      { \
        fprintf(stderr,"DEBUG database: " text ": %s: %s\n",(databaseHandle)->fileName,String_cString(sqlString)); \
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
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
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
  union
  {
    int        i;
    uint       ui;
    long       l;
    ulong      ul;
    int64      ll;
    uint64     ull;
    const char *s;
    String     string;
  }          value;
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
  DatabaseCallback *databaseCallback = (DatabaseCallback*)userData;

  return databaseCallback->function(databaseCallback->userData,
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

LOCAL void freeColumnNode(ColumnNode *columnNode, void *userData)
{
  assert(columnNode != NULL);
  assert(columnNode->name != NULL);

  UNUSED_VARIABLE(userData);

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

LOCAL Errors getTableColumnList(ColumnList     *columnList,
                                DatabaseHandle *databaseHandle,
                                const char     *tableName
                               )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  const char          *name,*type1,*type2;
  bool                primaryKey;
  ColumnNode          *columnNode;

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
                             "%d %p %p %d %b",
                             NULL,
                             &name,
                             &type1,
                             NULL,
                             &primaryKey
                            )
        )
  {
    error = Database_prepare(&databaseQueryHandle2,
                             databaseHandle,
                             "SELECT TYPEOF(%s) FROM %s \
                              LIMIT 0,1 \
                             ",
                             name,
                             tableName
                            );
    if (error != ERROR_NONE)
    {
      Database_finalize(&databaseQueryHandle1);
      List_done(columnList,CALLBACK((ListNodeFreeFunction)freeColumnNode,NULL));
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle2,"%p",&type2))
    {
      if (stringEqualsIgnoreCase(type2,"NULL"))
      {
        type2 = NULL;
      }
    }
    else
    {
      type2 = NULL;
    }

    columnNode = LIST_NEW_NODE(ColumnNode);
    if (columnNode == NULL)
    {
      List_done(columnList,CALLBACK((ListNodeFreeFunction)freeColumnNode,NULL));
      return ERROR_INSUFFICIENT_MEMORY;
    }

    columnNode->name = strdup(name);
    if      (type2 != NULL)
    {
      if (stringEqualsIgnoreCase(type2,"INTEGER"))
      {
        if (primaryKey)
        {
          columnNode->type = DATABASE_TYPE_PRIMARY_KEY;
        }
        else
        {
          columnNode->type = DATABASE_TYPE_INT64;
        }
      }
      else if (stringEqualsIgnoreCase(type2,"REAL"))
      {
        columnNode->type = DATABASE_TYPE_DOUBLE;
      }
      else if (stringEqualsIgnoreCase(type2,"TEXT"))
      {
        columnNode->type = DATABASE_TYPE_TEXT;
      }
      else if (stringEqualsIgnoreCase(type2,"BLOB"))
      {
        columnNode->type = DATABASE_TYPE_BLOB;
      }
      else
      {
        HALT_INTERNAL_ERROR("Unknown database data type '%s' for '%s'",type2,name);
      }
    }
    else
    {
      if (stringEqualsIgnoreCase(type1,"INTEGER"))
      {
        if (primaryKey)
        {
          columnNode->type = DATABASE_TYPE_PRIMARY_KEY;
        }
        else
        {
          columnNode->type = DATABASE_TYPE_INT64;
        }
      }
      else if (stringEqualsIgnoreCase(type1,"REAL"))
      {
        columnNode->type = DATABASE_TYPE_DOUBLE;
      }
      else if (stringEqualsIgnoreCase(type1,"TEXT"))
      {
        columnNode->type = DATABASE_TYPE_TEXT;
      }
      else if (stringEqualsIgnoreCase(type1,"BLOB"))
      {
        columnNode->type = DATABASE_TYPE_BLOB;
      }
      else
      {
        HALT_INTERNAL_ERROR("Unknown database data type '%s' for '%s'",type1,name);
      }
    }

    List_append(columnList,columnNode);

    Database_finalize(&databaseQueryHandle2);
  }
  Database_finalize(&databaseQueryHandle1);

  return ERROR_NONE;
}

LOCAL void freeTableColumnList(ColumnList *columnList)
{
  assert(columnList != NULL);

  List_done(columnList,CALLBACK((ListNodeFreeFunction)freeColumnNode,NULL));
}

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
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break; // not reached
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
  assert(fileName != NULL);

  // init variables
  databaseHandle->handle = NULL;
  #ifndef NDEBUG
    stringClear(databaseHandle->fileName);
  #endif /* not NDEBUG */

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

  // get mode
  sqliteMode = 0;
  switch (databaseOpenMode)
  {
    case DATABASE_OPENMODE_CREATE:    sqliteMode = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE; break;
    case DATABASE_OPENMODE_READ:      sqliteMode = SQLITE_OPEN_READONLY;                     break;
    case DATABASE_OPENMODE_READWRITE: sqliteMode = SQLITE_OPEN_READWRITE;                    break;
  }

  // open database
  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
    return error;
  }
  #ifndef NDEBUG
    strncpy(databaseHandle->fileName,fileName,sizeof(databaseHandle->fileName)); databaseHandle->fileName[sizeof(databaseHandle->fileName)-1] = '\0';
  #endif /* not NDEBUG */

  // set busy timeout
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
    DEBUG_ADD_RESOURCE_TRACE("database",databaseHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"database",databaseHandle);
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
    DEBUG_REMOVE_RESOURCE_TRACE(databaseHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseHandle);
  #endif /* NDEBUG */

  #ifdef DATABASE_DEBUG
    fprintf(stderr,"Database debug: close\n");
  #endif

  // clear busy timeout
  sqlite3_busy_handler(databaseHandle->handle,NULL,NULL);

  // close database
  sqlite3_close(databaseHandle->handle);
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

Errors Database_copyTable(DatabaseHandle *fromDatabaseHandle,
                          DatabaseHandle *toDatabaseHandle,
                          const char     *tableName
                         )
{
  Errors           error;
  ColumnList       fromColumnList,toColumnList;
  ColumnList       columnList;
  ColumnNode       *fromColumnNode,*toColumnNode,*columnNode;
  String           sqlString;
  sqlite3_stmt     *handle;
  int              sqliteResult;
  uint             n;
  uint             column;

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

  // get mutual columns to copy
  List_init(&columnList);
  fromColumnNode = fromColumnList.head;
  while (fromColumnNode != NULL)
  {
    toColumnNode = toColumnList.head;
    while (   (toColumnNode != NULL)
           && (strcmp(fromColumnNode->name,toColumnNode->name) != 0)
          )
    {
      toColumnNode = toColumnNode->next;
    }
    if (toColumnNode != NULL)
    {
      columnNode = List_remove(&fromColumnList,fromColumnNode);
      if (fromColumnNode->type == DATABASE_TYPE_UNKNOWN) fromColumnNode->type = toColumnNode->type;
      if (fromColumnNode->type == DATABASE_TYPE_UNKNOWN) fromColumnNode->type = DATABASE_TYPE_INT64;
      List_append(&columnList,fromColumnNode);
      fromColumnNode = columnNode;
    }
    else
    {
      fromColumnNode = fromColumnNode->next;
    }
  }
  freeTableColumnList(&toColumnList);
  freeTableColumnList(&fromColumnList);

  // select rows in from-table
  sqlString = String_new();
  BLOCK_DOX(error,
            { sqlite3_mutex_enter(sqlite3_db_mutex(fromDatabaseHandle->handle));
              sqlite3_mutex_enter(sqlite3_db_mutex(toDatabaseHandle->handle));
            },
            { sqlite3_mutex_leave(sqlite3_db_mutex(fromDatabaseHandle->handle));
              sqlite3_mutex_enter(sqlite3_db_mutex(toDatabaseHandle->handle));
            },
  {
    formatSQLString(String_clear(sqlString),"SELECT ");
    n = 0;
    LIST_ITERATE(&columnList,columnNode)
    {
      {
        if (n > 0) String_appendChar(sqlString,',');

        String_appendCString(sqlString,columnNode->name);
        n++;
      }
    }
    formatSQLString(sqlString," FROM %s;",tableName);

    DATABASE_DEBUG_SQL(fromDatabaseHandle,sqlString);
    sqliteResult = sqlite3_prepare_v2(fromDatabaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &handle,
                                      NULL
                                     );
    if (sqliteResult != SQLITE_OK)
    {
      return ERRORX_(DATABASE,sqlite3_errcode(fromDatabaseHandle->handle),sqlite3_errmsg(fromDatabaseHandle->handle));
    }

    // insert rows in to-table
    while (sqlite3_step(handle) == SQLITE_ROW)
    {
      formatSQLString(String_clear(sqlString),"INSERT INTO %s (",tableName);
      n = 0;
      LIST_ITERATE(&columnList,columnNode)
      {
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
//        if (!stringEquals(columnNode->name,columnName))
        {
          if (n > 0) String_appendChar(sqlString,',');

          switch (columnNode->type)
          {
            case DATABASE_TYPE_PRIMARY_KEY:
            case DATABASE_TYPE_INT64:
              formatSQLString(sqlString,"%lld",(int64)sqlite3_column_int64(handle,column));
              break;
            case DATABASE_TYPE_DOUBLE:
              formatSQLString(sqlString,"%lf",sqlite3_column_double(handle,column));
              break;
            case DATABASE_TYPE_DATETIME:
              formatSQLString(sqlString,"%llu",(uint64)sqlite3_column_int64(handle,column));
              break;
            case DATABASE_TYPE_TEXT:
              formatSQLString(sqlString,"%'s",sqlite3_column_text(handle,column));
              break;
            case DATABASE_TYPE_BLOB:
              HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              break;
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; // not reached
          }
          n++;
        }

        column++;
      }
      String_appendCString(sqlString,");");

      // execute SQL command
      DATABASE_DEBUG_SQL(toDatabaseHandle,sqlString);
      sqliteResult = sqlite3_exec(toDatabaseHandle->handle,
                                  String_cString(sqlString),
                                  NULL,
                                  NULL,
                                  NULL
                                 );
      if (sqliteResult != SQLITE_OK)
      {
        return ERRORX_(DATABASE,sqlite3_errcode(toDatabaseHandle->handle),sqlite3_errmsg(toDatabaseHandle->handle));
      }
    }

    // done table
    sqlite3_finalize(handle);

    return ERROR_NONE;
  });
  String_delete(sqlString);

  // free resources
  freeTableColumnList(&columnList);

  return error;
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
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
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
  Errors           error;
  ColumnList       columnList;
  const ColumnNode *columnNode;
  String           sqlString,value;
  sqlite3_stmt     *handle;
  int              sqliteResult;
  uint             n;
  uint             column;

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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
      return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; // not reached
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
        return ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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

Errors Database_execute(DatabaseHandle   *databaseHandle,
                        DatabaseFunction databaseFunction,
                        void             *databaseUserData,
                        const char       *command,
                        ...
                       )
{
  String           sqlString;
  va_list          arguments;
  Errors           error;
  DatabaseCallback databaseCallback;
  int              sqliteResult;

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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
    databaseCallback.function = databaseFunction;
    databaseCallback.userData = databaseUserData;
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                (databaseFunction != NULL) ? executeCallback : NULL,
                                (databaseFunction != NULL) ? &databaseCallback : NULL,
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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

#if 0
int XXX(void*userdata,int argc,const char *argv[], const char *columns[])
{
int i;
//fprintf(stderr,"%s, %d:\n",__FILE__,__LINE__);
for (i = 0; i < argc; i++)
{
fprintf(stderr,"  %s=%s",columns[i],argv[i]);
}
fprintf(stderr," \n");
return 0;
}
#endif

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
  #endif /* not NDEBUG */

  // prepare SQL command execution
  error = ERROR_NONE;
  BLOCK_DO(sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
           sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
  {
    DATABASE_DEBUG_SQL(databaseHandle,sqlString);
#if 0
{
String s = String_new();
String_format(s,"EXPLAIN QUERY PLAN %s",String_cString(sqlString));
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(s));
sqlite3_exec(databaseHandle->handle,
                                String_cString(s),
                                XXX,
                                NULL,
                                NULL
                               );
String_delete(s);
//exit(1);
}
#endif
    sqliteResult = sqlite3_prepare_v2(databaseHandle->handle,
                                      String_cString(sqlString),
                                      -1,
                                      &databaseQueryHandle->handle,
                                      NULL
                                     );
    if (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
    DEBUG_ADD_RESOURCE_TRACE("prepare",databaseQueryHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"prepare",databaseQueryHandle);
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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseQueryHandle->databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseQueryHandle->databaseHandle->handle)),
  {
    if (sqlite3_step(databaseQueryHandle->handle) == SQLITE_ROW)
    {
      // get data
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

      return TRUE;
    }
    else
    {
      return FALSE;
    }
  });
  va_end(arguments);

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
    DEBUG_REMOVE_RESOURCE_TRACE(databaseQueryHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,databaseQueryHandle);
  #endif /* NDEBUG */

  BLOCK_DO(sqlite3_mutex_enter(sqlite3_db_mutex(databaseQueryHandle->databaseHandle->handle)),
           sqlite3_mutex_leave(sqlite3_db_mutex(databaseQueryHandle->databaseHandle->handle)),
  {
    sqlite3_finalize(databaseQueryHandle->handle);
  });
  #ifndef NDEBUG
    String_delete(databaseQueryHandle->sqlString);
  #endif /* not NDEBUG */
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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
              sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
              sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
        error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
            sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
            sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
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
      error = ERRORX_(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
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
  BLOCK_DO(sqlite3_mutex_enter(sqlite3_db_mutex(databaseHandle->handle)),
           sqlite3_mutex_leave(sqlite3_db_mutex(databaseHandle->handle)),
  {
    databaseId = (uint64)sqlite3_last_insert_rowid(databaseHandle->handle);
  });

  return databaseId;
}

#ifndef NDEBUG

void Database_debugEnable(bool enabled)
{
  if (!enabled)
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

  DATABASE_DEBUG_SQLX(databaseQueryHandle->databaseHandle,"SQL query",databaseQueryHandle->sqlString);
}

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
