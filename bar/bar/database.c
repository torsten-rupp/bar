/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/database.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Database functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "errors.h"

#include "sqlite3.h"

#include "database.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  DatabaseFunction function;
  void             *userData;
} DatabaseCallbackData;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Database_formatSQLString
* Purpose: format SQL string from command
* Input  : sqlString - SQL string variable
*          command   - command string with %[l]d, %S, %s
*          arguments - optional argument list
* Output : -
* Return : SQL string
* Notes  : -
\***********************************************************************/

LOCAL String Database_formatSQLString(String     sqlString,
                                      const char *command,
                                      va_list    arguments
                                     )
{
  const char *s;
  char       ch;
  bool       longFlag;
  union
  {
    int        i;
    int64      l;
    const char *s;
    String     string;
  }          value;
  const char *t;
  long       i;

  assert(sqlString != NULL);
  assert(command != NULL);

  String_clear(sqlString);
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
          case 's':
            /* C string */
            s++;

            value.s = va_arg(arguments,const char*);

            String_appendChar(sqlString,'\'');
            t = value.s;
            while ((ch = (*t)) != '\0')
            {
              switch (ch)
              {
                case '\\':
                  String_appendCString(sqlString,"\\\\");
                  break;
                case '\'':
                  String_appendCString(sqlString,"\\'");
                  break;
                default:
                  String_appendChar(sqlString,ch);
                  break;
              }
              t++;
            }
            String_appendChar(sqlString,'\'');
            break;
          case 'S':
            /* string */
            s++;

            String_appendChar(sqlString,'\'');
            value.string = va_arg(arguments,String);
            i = 0;
            while (i < String_length(value.string))
            {
              ch = String_index(value.string,i);
              switch (ch)
              {
                case '\\':
                  String_appendCString(sqlString,"\\\\");
                  break;
                case '\'':
                  String_appendCString(sqlString,"\\'");
                  break;
                default:
                  String_appendChar(sqlString,ch);
                  break;
              }
              i++;
            }
            String_appendChar(sqlString,'\'');
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

LOCAL int Database_callback(void *userData,
                            int  count,
                            char *values[],
                            char *names[]
                           )
{
  DatabaseCallbackData *databaseCallbackData = (DatabaseCallbackData*)userData;

  return databaseCallbackData->function(databaseCallbackData->userData,
                                        count,
                                        (const char**)names,
                                        (const char**)values
                                       )
          ? 0
          : 1;
}

/*---------------------------------------------------------------------*/

Errors Database_open(DatabaseHandle *databaseHandle,
                     const char     *fileName
                    )
{
  int sqliteResult;

  assert(databaseHandle != NULL);
  assert(fileName != NULL);

  sqliteResult = sqlite3_open_v2(fileName,&databaseHandle->handle,SQLITE_OPEN_READWRITE,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    return ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
  }

  return ERROR_NONE;
}

void Database_close(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  sqlite3_close(databaseHandle->handle);
}

Errors Database_insert(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  return ERROR_NONE;
}

Errors Database_delete(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  return ERROR_NONE;
}

Errors Database_select(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  return ERROR_NONE;
}

Errors Database_execute(DatabaseHandle   *databaseHandle,
                        DatabaseFunction databaseFunction,
                        void             *databaseUserData,
                        const char       *command,
                        ...
                       )
{
  String               sqlString;
  va_list              arguments;
  DatabaseCallbackData databaseCallbackData;
  int                  sqliteResult;

  assert(databaseHandle != NULL);
  assert(command != NULL);

  /* format SQL command string */
  va_start(arguments,command);
  sqlString = Database_formatSQLString(String_new(),
                                       command,
                                       arguments
                                      );
  va_end(arguments);

  /* execute SQL command */
  databaseCallbackData.function = databaseFunction;
  databaseCallbackData.userData = databaseUserData;
fprintf(stderr,"%s,%d: %s\n",__FILE__,__LINE__,String_cString(sqlString));
  sqliteResult = sqlite3_exec(databaseHandle->handle,
                              String_cString(sqlString),
                              (databaseFunction != NULL) ? Database_callback : NULL,
                              (databaseFunction != NULL) ? &databaseCallbackData : NULL,
                              NULL
                             );
  if (sqliteResult != SQLITE_OK)
  {
    String_delete(sqlString);
    return ERRORX(DATABASE,sqlite3_errcode(databaseHandle->handle),sqlite3_errmsg(databaseHandle->handle));
  }

  /* free resources */
  String_delete(sqlString);

  return ERROR_NONE;
}

uint64 Database_getLastRowId(DatabaseHandle *databaseHandle)
{
  assert(databaseHandle != NULL);

  return (uint64)sqlite3_last_insert_rowid(databaseHandle->handle);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
