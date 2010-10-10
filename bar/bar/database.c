/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/database.c,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Database functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "files.h"
#include "errors.h"

#include "sqlite3.h"

#include "database.h"

/****************** Conditional compilation switches *******************/
#define _DATABASE_DEBUG

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  DatabaseFunction function;
  void             *userData;
} DatabaseCallback;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : formatSQLString
* Purpose: format SQL string from command
* Input  : sqlString - SQL string variable
*          command   - command string with %[l]d, %S, %s
*          arguments - optional argument list
* Output : -
* Return : SQL string
* Notes  : -
\***********************************************************************/

LOCAL String formatSQLString(String     sqlString,
                             const char *command,
                             va_list    arguments
                            )
{
  const char *s;
  char       ch;
  bool       longFlag;
  char       quoteFlag;
  union
  {
    int        i;
    uint       ui;
    int64      l;
    uint64     ul;
    const char *s;
    String     string;
  }          value;
  const char *t;
  long       i;

  assert(sqlString != NULL);
  assert(command != NULL);

  s = command;
  while ((ch = (*s)) != '\0')
  {
    switch (ch)
    {
      case '\\':
        /* escaped character */
        String_appendChar(sqlString,'\\');
        s++;
        if ((*s) != '\0')
        {
          String_appendChar(sqlString,*s);
          s++;
        }
        break;
      case '%':
        /* format character */
        s++;

        /* check for long flag */
        if (    ((*s) != '\0')
             && ((*s) == 'l')
           )
        {
          longFlag = TRUE;
          s++;
        }
        else
        {
          longFlag = FALSE;
        }

        /* quoting flag (ignore quote char) */
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

        /* get format char */
        switch (*s)
        {
          case 'd':
            /* integer */
            s++;

            if (longFlag)
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
            /* unsigned integer */
            s++;

            if (longFlag)
            {
              value.ul = va_arg(arguments,uint64);
              String_format(sqlString,"%lu",value.ul);
            }
            else
            {
              value.ui = va_arg(arguments,uint);
              String_format(sqlString,"%u",value.ui);
            }
            break;
          case 's':
            /* C string */
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
            /* string */
            s++;

            if (quoteFlag) String_appendChar(sqlString,'\'');
            value.string = va_arg(arguments,String);
            if (value.string != NULL)
            {
              i = 0;
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
            /* %% */
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
//fprintf(stderr,"%s,%d: text=%s caseSensitive=%d patternText=%s\n",__FILE__,__LINE__,text,caseSensitive,patternText);
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

/*---------------------------------------------------------------------*/

Errors Database_open(DatabaseHandle    *databaseHandle,
                     const char        *fileName,
                     DatabaseOpenModes databaseOpenMode
                    )
{
  String directory;
  Errors error;
  int    sqliteMode;
  int    sqliteResult;

  assert(databaseHandle != NULL);
  assert(fileName != NULL);

  /* create lock */
  if (!Semaphore_init(&databaseHandle->lock))
  {
    return ERRORX(DATABASE,errno,"create database lock fail");
  }

  /* create directory if needed */
  directory = File_getFilePathNameCString(String_new(),fileName);
  if (   !String_empty(directory)
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
      Semaphore_done(&databaseHandle->lock);
      return error;
    }
  }
  File_deleteFileName(directory);

  /* get mode */
  sqliteMode = 0;
  switch (databaseOpenMode)
  {
    case DATABASE_OPENMODE_CREATE:    sqliteMode = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE; break;
    case DATABASE_OPENMODE_READ:      sqliteMode = SQLITE_OPEN_READONLY;                     break;
    case DATABASE_OPENMODE_READWRITE: sqliteMode = SQLITE_OPEN_READWRITE;                    break;
  }

  /* open database */
  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    Semaphore_done(&databaseHandle->lock);
    return ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
  }

  /* register REGEXP functions */
  sqlite3_create_function(databaseHandle->handle,
                          "regexp",
                          3,
                          SQLITE_ANY,
                          NULL,
                          regexpMatch,
                          NULL,
                          NULL
                         );

  return ERROR_NONE;
}

void Database_close(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  sqlite3_close(databaseHandle->handle);
  Semaphore_done(&databaseHandle->lock);
}

void Database_lock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  Semaphore_lock(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
}

void Database_unlock(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  Semaphore_unlock(&databaseHandle->lock);
}

bool Database_isLocked(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  return Semaphore_isLocked(&databaseHandle->lock);
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
  assert(command != NULL);

  /* format SQL command string */
  va_start(arguments,command);
  sqlString = formatSQLString(String_new(),
                              command,
                              arguments
                             );
  va_end(arguments);

  /* execute SQL command */
  error = ERROR_NONE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    #ifdef DATABASE_DEBUG
     fprintf(stderr,"Database debug: execute command: %s\n",String_cString(sqlString));
    #endif
    databaseCallback.function = databaseFunction;
    databaseCallback.userData = databaseUserData;
    sqliteResult = sqlite3_exec(databaseHandle->handle,
                                String_cString(sqlString),
                                (databaseFunction != NULL) ? executeCallback : NULL,
                                (databaseFunction != NULL) ? &databaseCallback : NULL,
                                NULL
                               );
    if (sqliteResult == SQLITE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
    }
  }
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  /* free resources */
  String_delete(sqlString);

  return ERROR_NONE;
}

Errors Database_prepare(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseHandle      *databaseHandle,
                        const char          *command,
                        ...
                       )
{
  String  sqlString;
  va_list arguments;
  Errors  error;
  int     sqliteResult;

  assert(databaseHandle != NULL);
  assert(databaseQueryHandle != NULL);
  assert(command != NULL);

  /* initialize variables */
  databaseQueryHandle->databaseHandle = databaseHandle;

  /* format SQL command string */
  va_start(arguments,command);
  sqlString = formatSQLString(String_new(),
                              command,
                              arguments
                             );
  va_end(arguments);

  /* prepare SQL command execution */
  error = ERROR_NONE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    #ifdef DATABASE_DEBUG
     fprintf(stderr,"Database debug: prepare command: %s\n",String_cString(sqlString));
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
      error = ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
    }
  }
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  /* free resources */
  String_delete(sqlString);

  return ERROR_NONE;
}

bool Database_getNextRow(DatabaseQueryHandle *databaseQueryHandle,
                         const char          *format,
                         ...
                        )
{
  uint    column;
  va_list arguments;
  bool    longFlag;
  uint    maxLength;
  union
  {
    int    *i;
    int64  *l;
    float  *f;
    double *d;
    char   *ch;
    char   *s;
    String *string;
  }       value;
  bool    result;

  assert(databaseQueryHandle != NULL);
  assert(format != NULL);

  result = FALSE;
  SEMAPHORE_LOCKED_DO(&databaseQueryHandle->databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (sqlite3_step(databaseQueryHandle->handle) == SQLITE_ROW)
    {
      /* get data */
      column = 0;
      va_start(arguments,format);
      while ((*format) != '\0')
      {
        /* find next format specifier */
        while (((*format) != '\0') && ((*format) != '%'))
        {
          format++;
        }

        if ((*format) == '%')
        {
          format++;

          /* skip align specifier */
          if (    ((*format) != '\0')
               && (   ((*format) == '-')
                   || ((*format) == '-')
                  )
             )
          {
            format++;
          }

          /* get length specifier */
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

          /* check for long flag */
          if (    ((*format) != '\0')
               && ((*format) == 'l')
             )
          {
            longFlag = TRUE;
            format++;
          }
          else
          {
            longFlag = FALSE;
          }

          /* handle format type */
          switch (*format)
          {
            case 'd':
              /* integer */
              format++;

              if (longFlag)
              {
                value.l = va_arg(arguments,int64*);
                if (value.l != NULL)
                {
                  (*value.l) = (int64)sqlite3_column_int64(databaseQueryHandle->handle,column);
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
            case 'f':
              /* float */
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
              /* char */
              format++;

              value.ch = va_arg(arguments,char*);
              if (value.ch != NULL)
              {
                (*value.ch) = ((char*)sqlite3_column_text(databaseQueryHandle->handle,column))[0];
              }
              break;
            case 's':
              /* C string */
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
              /* string */
              format++;

              value.string = va_arg(arguments,String*);
              if (value.string != NULL)
              {
                String_setCString(*value.string,(const char*)sqlite3_column_text(databaseQueryHandle->handle,column));
              }
              break;
            default:
              return FALSE;
              break;
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
  }

  return result;
}

void Database_finalize(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

  SEMAPHORE_LOCKED_DO(&databaseQueryHandle->databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    sqlite3_finalize(databaseQueryHandle->handle);
  }
}

Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *l,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            )
{
  String       sqlString;
  va_list      arguments;
  sqlite3_stmt *handle;
  Errors       error;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  assert(l != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  /* init variables */
  (*l) = DATABASE_ID_NONE;

  /* format SQL command string */
  sqlString = String_format(String_new(),
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
    formatSQLString(sqlString,
                    additional,
                    arguments
                   );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  /* execute SQL command */
  error = ERROR_NONE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    #ifdef DATABASE_DEBUG
     fprintf(stderr,"Database debug: get integer 64: %s\n",__FILE__,__LINE__,String_cString(sqlString));
    #endif
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
      error = ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
    }

    if (sqlite3_step(handle) == SQLITE_ROW)
    {
      (*l) = (int64)sqlite3_column_int64(handle,0);
    }

    sqlite3_finalize(handle);
  }
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  /* free resources */
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
  String       sqlString;
  va_list      arguments;
  sqlite3_stmt *handle;
  Errors       error;
  int          sqliteResult;

  assert(databaseHandle != NULL);
  assert(string != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  /* init variables */
  String_clear(string);

  /* format SQL command string */
  sqlString = String_format(String_new(),
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
    formatSQLString(sqlString,
                    additional,
                    arguments
                   );
    va_end(arguments);
  }
  String_appendCString(sqlString," LIMIT 0,1");

  /* execute SQL command */
  error = ERROR_NONE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    #ifdef DATABASE_DEBUG
     fprintf(stderr,"Database debug: get integer 64: %s\n",__FILE__,__LINE__,String_cString(sqlString));
    #endif
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
      error = ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
    }

    if (sqlite3_step(handle) == SQLITE_ROW)
    {
      String_setCString(string,(const char*)sqlite3_column_text(handle,0));
    }

    sqlite3_finalize(handle);
  }
  if (error != ERROR_NONE)
  {
    String_delete(sqlString);
    return error;
  }

  /* free resources */
  String_delete(sqlString);

  return ERROR_NONE;
}

int64 Database_getLastRowId(DatabaseHandle *databaseHandle)
{
  int64 databaseId;

  assert(databaseHandle != NULL);

  databaseId = DATABASE_ID_NONE;
  SEMAPHORE_LOCKED_DO(&databaseHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    databaseId = (uint64)sqlite3_last_insert_rowid(databaseHandle->handle);
  }

  return databaseId;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
