/***********************************************************************\
*
* $Revision: 1458 $
* $Date: 2012-01-28 09:50:13 +0100 (Sat, 28 Jan 2012) $
* $Author: trupp $
* Contents: BAR sqlite3 shell
* Systems: Unix
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_PCRE
  #include <pcreposix.h>
#endif
#include <assert.h>

#include "common/global.h"
#include "strings.h"
#include "common/files.h"

#include "sqlite3.h"

#include "index_definition.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL bool infoFlag             = FALSE;  // output index database info
LOCAL bool checkFlag            = FALSE;  // check database
LOCAL bool createFlag           = FALSE;  // create new index database
LOCAL bool createIndizesFlag    = FALSE;  // re-create indizes
LOCAL bool createTriggersFlag   = FALSE;  // re-create triggers
LOCAL bool createNewestFlag     = FALSE;  // re-create newest data
LOCAL bool createAggregatesFlag = FALSE;  // re-create aggregate data
LOCAL bool vacuumFlag           = FALSE;  // execute vacuum
LOCAL bool showNamesFlag        = FALSE;
LOCAL bool showHeaderFlag       = FALSE;
LOCAL bool headerPrintedFlag    = FALSE;
LOCAL bool foreignKeysFlag      = TRUE;
LOCAL bool pipeFlag             = FALSE;
LOCAL bool verboseFlag          = FALSE;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : printUsage
* Purpose: print usage
* Input  : programName - program name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printUsage(const char *programName)
{
  printf("Usage %s: [<options>] <database file> [<command>...|-]\n",programName);
  printf("\n");
  printf("Options:  --info               - output index database infos\n");
  printf("          --create             - create new index database\n");
  printf("          --create-triggers    - re-create triggers\n");
  printf("          --create-newest      - re-create newest data\n");
  printf("          --create-indizes     - re-create indizes\n");
  printf("          --create-aggregates  - re-create aggregated data\n");
  printf("          --check              - check index database integrity\n");
  printf("          --vacuum             - collect and remove unused file space\n");
  printf("          -n|--names           - print named values\n");
  printf("          -H|--header          - print headers\n");
  printf("          -f|--no-foreign-keys - disable foreign key constraints\n");
  printf("          --pipe               - read data from stdin and pipe into database (use ? as variable)\n");
  printf("          -v|--verbose         - verbose output\n");
  printf("          -h|--help            - print this help\n");
}

/***********************************************************************\
* Name   : unixTimestamp
* Purpose: callback for UNIXTIMESTAMP function
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
  format = (argc >= 2) ? (const char*)argv[1] : NULL;

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
        timestamp = (uint64)mktime(tm);
      }
      else
      {
        s = strptime(text,(format != NULL) ? format : "%Y-%m-%d %H:%M:%S",&tmBuffer);
        if ((s != NULL) && ((*s) == '\0'))
        {
          timestamp = (uint64)mktime(&tmBuffer);
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

#ifdef HAVE_PCRE

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
#endif /* HAVE_PCRE */

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
              String_format(sqlString,"%"PRIi64,value.ll);
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
              String_format(sqlString,"%"PRIu64,value.ull);
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
* Name   : formatSQLString
* Purpose: format SQL string from command
* Input  : sqlString - SQL string variable
*          command   - command string with %[l]d, %S, %s
*          ..        - optional argument list
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

  assert(sqlString != NULL);

  va_start(arguments,command);
  vformatSQLString(sqlString,
                   command,
                   arguments
                  );
  va_end(arguments);

  return sqlString;
}

/***********************************************************************\
* Name   : sqlProgressHandler
* Purpose: SQLite progress handler
* Input  : userData - user data
* Output : -
* Return : 0
* Notes  : -
\***********************************************************************/

LOCAL int sqlProgressHandler(void *userData)
{
  const char *WHEEL = "|/-\\";

  static uint64 lastTimestamp = 0LL;
  static uint   count         = 0;

  struct timeval tv;
  uint64         timestamp;

  UNUSED_VARIABLE(userData);

  // get timestamp
  gettimeofday(&tv,NULL);
  timestamp = (uint64)tv.tv_usec/1000L+((uint64)tv.tv_sec)*1000ULL;

  // info output
  if (timestamp > (lastTimestamp+250))
  {
    if (verboseFlag)
    {
      fprintf(stderr,"%c\b",WHEEL[count%4]); fflush(stderr); count++;
    }
    lastTimestamp = timestamp;
  }

  return 0;
}

#if 0
still not used
/***********************************************************************\
* Name   : sqlPrepare
* Purpose: prepare SQL command
* Input  : statementHandle - statement handle variable
*          databaseHandle  - database handle
*          command         - command string with %[l]d, %S, %s
*          ...             - optional argument list
* Output : statementHandle - statement handle
* Return : SQLITE_OK or sqlite error code
* Notes  : -
\***********************************************************************/

LOCAL int sqlPrepare(sqlite3_stmt **statementHandle,
                     sqlite3      *databaseHandle,
                     const char   *command,
                     ...
                    )
{
  String  sqlString;
  va_list arguments;
  int     sqliteResult;

  assert(statementHandle != NULL);
  assert(databaseHandle != NULL);
  assert(command != NULL);

  // format SQL command string
  va_start(arguments,command);
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );
  va_end(arguments);

  // prepare SQL command execution
  sqliteResult = g(databaseHandle,
                                    String_cString(sqlString),
                                    -1,
                                    &statementHandle,
                                    NULL
                                   );
  if (sqliteResult != SQLITE_OK)
  {
    String_delete(sqlString);
    return sqliteResult;
  }

  // free resources
  String_delete(sqlString);

  return SQLITE_OK;
}

/***********************************************************************\
* Name   : sqlFinalize
* Purpose: finalize SQL command
* Input  : statementHandle - statement handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sqlFinalize(sqlite3_stmt *statementHandle)
{
  assert(statementHandle != NULL);

  sqlite3_finalize(statementHandle);
}

/***********************************************************************\
* Name   : sqlStep
* Purpose: step SQL command
* Input  : statementHandle - statement handle
* Output : -
* Return : SQLITE_OK or sqlite error code
* Notes  : -
\***********************************************************************/

LOCAL int sqlStep(sqlite3_stmt *statementHandle)
{
  assert(statementHandle != NULL);

  return sqlite3_step(statementHandle);
}

/***********************************************************************\
* Name   : finalize
* Purpose: finalize SQL command
* Input  : statementHandle - statement handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void finalize(sqlite3_stmt *statementHandle)
{
  assert(statementHandle != NULL);

  sqlite3_finalize(statementHandle);
}
#endif /* 0 */

/***********************************************************************\
* Name   : sqlExecute
* Purpose: execute SQL command
* Input  : databaseHandle - database handle
*          errorMessage   - error message variable (can be NULL)
*          command        - SQL command
*          ...            - optional arguments for SQL command
* Output : -
* Return : SQLITE_OK or error code
* Notes  : -
\***********************************************************************/

LOCAL int sqlExecute(sqlite3    *databaseHandle,
                     const char **errorMessage,
                     const char *command,
                     ...
                    )
{
  String       sqlString;
  va_list      arguments;
  int          sqliteResult;
  sqlite3_stmt *statementHandle;

  assert(databaseHandle != NULL);

  // format SQL command string
  va_start(arguments,command);
  sqlString = vformatSQLString(String_new(),
                               command,
                               arguments
                              );
  va_end(arguments);

  // prepare SQL command execution
  sqliteResult = sqlite3_prepare_v2(databaseHandle,
                                    String_cString(sqlString),
                                    -1,
                                    &statementHandle,
                                    NULL
                                   );
  if (sqliteResult != SQLITE_OK)
  {
    (*errorMessage) = sqlite3_errmsg(databaseHandle);
    String_delete(sqlString);
    return sqliteResult;
  }
  sqliteResult = sqlite3_step(statementHandle);
  if (sqliteResult != SQLITE_DONE)
  {
    (*errorMessage) = sqlite3_errmsg(databaseHandle);
    String_delete(sqlString);
    return sqliteResult;
  }
  sqlite3_finalize(statementHandle);

  // free resources
  String_delete(sqlString);

  return SQLITE_OK;
}

/***********************************************************************\
* Name   : printPercentage
* Purpose: print percentage value
* Input  : n     - value
*          count - max. value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printPercentage(ulong n, ulong count)
{
  uint percentage;

  percentage = (count > 0) ? (uint)(((double)n*100.0)/(double)count) : 0;
  if (percentage > 100) percentage = 100;

  fprintf(stderr,"%3u%%\b\b\b\b",percentage); fflush(stderr);
}

/***********************************************************************\
* Name   : createTriggers
* Purpose: create triggers
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createTriggers(sqlite3 *databaseHandle)
{
  char       command[1024];
  char       name[1024];
  int        sqliteResult;
  const char *errorMessage;

  if (verboseFlag) { fprintf(stderr,"Create triggers..."); fflush(stderr); }

  // delete all existing triggers
  do
  {
    stringClear(name);
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT name FROM sqlite_master WHERE type='trigger' AND name LIKE 'trigger%'",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  stringSet(name,sizeof(name),values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );

    if ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name))
    {
      stringClear(command);
      stringFormat(command,sizeof(command),"DROP TRIGGER %s",name);
      sqliteResult = sqlite3_exec(databaseHandle,
                                  command,
                                  CALLBACK(NULL,NULL),
                                  (char**)&errorMessage
                                 );
    }
  }
  while ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name));
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create triggers fail: %s!\n",errorMessage);
    exit(1);
  }

  // create new triggeres
  sqliteResult = sqlite3_exec(databaseHandle,
                              INDEX_TRIGGERS_DEFINITION,
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create triggers fail: %s!\n",errorMessage);
    exit(1);
  }

  if (verboseFlag) fprintf(stderr,"OK  \n");
}

/***********************************************************************\
* Name   : createIndizes
* Purpose: create indizes
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createIndizes(sqlite3 *databaseHandle)
{
  char       command[1024];
  char       name[1024];
  int        sqliteResult;
  const char *errorMessage;

  if (verboseFlag) { fprintf(stderr,"Create indizes:\n"); }

  // start transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "BEGIN TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }

  // delete all existing indizes
  if (verboseFlag) { fprintf(stderr,"  Discard indizes..."); }
  do
  {
    stringClear(name);
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'index%' LIMIT 0,1",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  stringSet(name,sizeof(name),values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name))
    {
      stringClear(command);
      stringFormat(command,sizeof(command),"DROP INDEX %s",name);
      sqliteResult = sqlite3_exec(databaseHandle,
                                  command,
                                  CALLBACK(NULL,NULL),
                                  (char**)&errorMessage
                                 );
    }
  }
  while ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name));
  do
  {
    stringClear(name);
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'FTS_%' LIMIT 0,1",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  stringSet(name,sizeof(name),values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name))
    {
      stringClear(command);
      stringFormat(command,sizeof(command),"DROP TABLE %s",name);
      sqliteResult = sqlite3_exec(databaseHandle,
                                  command,
                                  CALLBACK(NULL,NULL),
                                  (char**)&errorMessage
                                 );
    }

    (void)sqlite3_wal_checkpoint_v2(databaseHandle,NULL,CHECKPOINT_MODE,NULL,NULL);
    sqlProgressHandler(NULL);
  }
  while ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name));
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // end transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "END TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }

  // start transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "BEGIN TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }

  // create new indizes
  if (verboseFlag) { fprintf(stderr,"  Create new indizes..."); }
  sqliteResult = sqlite3_exec(databaseHandle,
                              INDEX_INDIZES_DEFINITION,
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  if (verboseFlag) { fprintf(stderr,"  Create new FTS..."); }
  sqliteResult = sqlite3_exec(databaseHandle,
                              INDEX_FTS_DEFINITION,
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create FTS fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK\n"); }

  // clear FTS names
  if (verboseFlag) { fprintf(stderr,"  Discard FTS indizes..."); }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "DELETE FROM FTS_storage",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "DELETE FROM FTS_entries",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // create FTS names
  if (verboseFlag) { fprintf(stderr,"  Create new storage FTS index..."); }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "INSERT INTO FTS_storage SELECT id,name FROM storage",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }
  if (verboseFlag) { fprintf(stderr,"  Create new entries FTS index..."); }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "INSERT INTO FTS_entries SELECT id,name FROM entries",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // end transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "END TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",errorMessage);
    exit(1);
  }
}

/***********************************************************************\
* Name   : createNewest
* Purpose: create newest data
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createNewest(sqlite3 *databaseHandle)
{
  int        sqliteResult;
  const char *errorMessage;
  ulong      n,totalCount;

  // start transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "BEGIN TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",errorMessage);
    exit(1);
  }

  // set entries offset/size
  if (verboseFlag) { fprintf(stderr,"Create newest entries..."); fflush(stderr); }

  totalCount = 0L;
  n          = 0L;
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT COUNT(id) FROM entries \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                assert(count == 1);
                                assert(values[0] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                totalCount = (ulong)atol(values[0]);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",errorMessage);
    exit(1);
  }

  // delete newest
  sqliteResult = sqlite3_exec(databaseHandle,
                              "DELETE FROM entriesNewest",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create newest fail: %s!\n",errorMessage);
    exit(1);
  }

  // insert newest
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT id, \
                                      storageId, \
                                      name, \
                                      type, \
                                      timeLastChanged, \
                                      userId, \
                                      groupId, \
                                      permission, \
                                      offset, \
                                      size \
                                FROM entries \
                                ORDER BY name,offset,size,timeLastChanged DESC \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                String     sqlString;
                                bool       existsFlag;
                                uint64     entryId;
                                uint64     storageId;
                                const char *name;
                                uint       type;
                                uint64     timeLastChanged;
                                uint       userId;
                                uint       groupId;
                                uint       permission;
                                uint64     offset;
                                uint64     size;

                                assert(count == 10);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);
                                assert(values[3] != NULL);
                                assert(values[4] != NULL);
                                assert(values[5] != NULL);
                                assert(values[6] != NULL);
                                assert(values[7] != NULL);
                                assert(values[8] != NULL);
                                assert(values[9] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                entryId         = (uint64)atoll(values[0]);
                                storageId       = (uint64)atoll(values[1]);
                                name            = values[2];
                                type            = (uint)atol(values[3]);
                                timeLastChanged = (uint64)atoll(values[4]);
                                userId          = (uint)atol(values[5]);
                                groupId         = (uint)atol(values[6]);
                                permission      = (uint)atol(values[7]);
                                offset          = (uint64)atoll(values[8]);
                                size            = (uint64)atoll(values[9]);
//fprintf(stderr,"%s, %d: %llu name=%s offset=%llu size=%llu timeLastChanged=%llu\n",__FILE__,__LINE__,entryId,name,offset,size,timeLastChanged);

                                // check if exists
                                sqlString = formatSQLString(String_new(),
                                                            "SELECT id \
                                                             FROM entriesNewest \
                                                             WHERE     name=%'s \
                                                                   AND offset=%llu \
                                                                   AND size=%llu \
                                                            ",
                                                            name,
                                                            offset,
                                                            size
                                                           );
                                existsFlag = FALSE;
                                sqliteResult = sqlite3_exec(databaseHandle,
                                                            String_cString(sqlString),
                                                            CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                                            {
                                                              assert(count == 1);

                                                              UNUSED_VARIABLE(userData);
                                                              UNUSED_VARIABLE(columns);

                                                              existsFlag = (values[0] != NULL);

                                                              return SQLITE_OK;
                                                            },NULL),
                                                            (char**)&errorMessage
                                                           );
                                String_delete(sqlString);
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create newest fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                if (!existsFlag)
                                {
//fprintf(stderr,"%s, %d: %llu name=%s offset=%llu size=%llu timeLastChanged=%llu\n",__FILE__,__LINE__,entryId,name,offset,size,timeLastChanged);
                                  // insert
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "INSERT INTO entriesNewest \
                                                               (entryId,\
                                                                storageId, \
                                                                name, \
                                                                type, \
                                                                timeLastChanged, \
                                                                userId, \
                                                                groupId, \
                                                                permission, \
                                                                offset,\
                                                                size \
                                                               ) \
                                                             VALUES \
                                                               (%llu, \
                                                                %llu, \
                                                                %'s, \
                                                                %u, \
                                                                %llu, \
                                                                %u, \
                                                                %u, \
                                                                %u, \
                                                                %llu, \
                                                                %llu \
                                                               ) \
                                                            ",
                                                            entryId,
                                                            storageId,
                                                            name,
                                                            type,
                                                            timeLastChanged,
                                                            userId,
                                                            groupId,
                                                            permission,
                                                            offset,
                                                            size
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create newest fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create newest fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // end transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "END TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",errorMessage);
    exit(1);
  }
}

/***********************************************************************\
* Name   : createAggregates
* Purpose: create aggregates data
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregates(sqlite3 *databaseHandle)
{
  int        sqliteResult;
  const char *errorMessage;
  ulong      n,totalCount;

  // start transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "BEGIN TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }

  // set entries offset/size
  if (verboseFlag) { fprintf(stderr,"Create aggregates for entries..."); fflush(stderr); }

  totalCount = 0L;
  n          = 0L;
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT  (SELECT COUNT(id) FROM fileEntries    ) \
                                      +(SELECT COUNT(id) FROM imageEntries   ) \
                                      +(SELECT COUNT(id) FROM hardlinkEntries) \
                                      +(SELECT COUNT(id) FROM entries        ) \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                assert(count == 1);
                                assert(values[0] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                totalCount = (ulong)atol(values[0]);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }

  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entryId,fragmentOffset,fragmentSize FROM fileEntries",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 entryId;
                                uint64 fragmentOffset;
                                uint64 fragmentSize;

                                assert(count == 3);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                entryId        = (uint64)atoll(values[0]);
                                fragmentOffset = (uint64)atoll(values[1]);
                                fragmentSize   = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                                // set offset/size
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE entries \
                                                           SET offset=%llu, \
                                                               size  =%llu \
                                                           WHERE id=%llu \
                                                          ",
                                                          fragmentOffset,
                                                          fragmentSize,
                                                          entryId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entryId,blockSize,blockOffset,blockCount FROM imageEntries",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 entryId;
                                ulong  blockSize;
                                uint64 blockOffset;
                                uint64 blockCount;

                                assert(count == 4);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);
                                assert(values[3] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                entryId     = (uint64)atoll(values[0]);
                                blockSize   = (ulong)atol(values[1]);
                                blockOffset = (uint64)atoll(values[2]);
                                blockCount  = (uint64)atoll(values[3]);
//fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                                // set offset/size
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE entries \
                                                           SET offset=%llu, \
                                                               size  =%llu \
                                                           WHERE id=%llu \
                                                          ",
                                                          (uint64)blockSize*blockOffset,
                                                          (uint64)blockSize*blockCount,
                                                          entryId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entryId,fragmentOffset,fragmentSize FROM hardlinkEntries",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 entryId;
                                uint64 fragmentOffset;
                                uint64 fragmentSize;

                                assert(count == 3);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                entryId        = (uint64)atoll(values[0]);
                                fragmentOffset = (uint64)atoll(values[1]);
                                fragmentSize   = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                                // set offset/size
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE entries \
                                                           SET offset=%llu, \
                                                               size  =%llu \
                                                           WHERE id=%llu \
                                                          ",
                                                          fragmentOffset,
                                                          fragmentSize,
                                                          entryId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT id,offset,size FROM entries",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 entryId;
                                uint64 offset;
                                uint64 size;

                                assert(count == 3);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                entryId = (uint64)atoll(values[0]);
                                offset  = (uint64)atoll(values[1]);
                                size    = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                                // set offset/size
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE entriesNewest \
                                                           SET offset=%llu, \
                                                               size  =%llu \
                                                           WHERE entryId=%llu \
                                                          ",
                                                          offset,
                                                          size,
                                                          entryId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // calculate directory content size/count
  if (verboseFlag) { fprintf(stderr,"Create aggregates for directory content..."); fflush(stderr); }

  totalCount = 0L;
  n          = 0L;
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT  (SELECT COUNT(entries.id) FROM fileEntries      LEFT JOIN entries ON entries.id=fileEntries.entryId      WHERE entries.id IS NOT NULL) \
                                      +(SELECT COUNT(entries.id) FROM directoryEntries LEFT JOIN entries ON entries.id=directoryEntries.entryId WHERE entries.id IS NOT NULL) \
                                      +(SELECT COUNT(entries.id) FROM linkEntries      LEFT JOIN entries ON entries.id=linkEntries.entryId      WHERE entries.id IS NOT NULL) \
                                      +(SELECT COUNT(entries.id) FROM hardlinkEntries  LEFT JOIN entries ON entries.id=hardlinkEntries.entryId  WHERE entries.id IS NOT NULL) \
                                      +(SELECT COUNT(entries.id) FROM specialEntries   LEFT JOIN entries ON entries.id=specialEntries.entryId   WHERE entries.id IS NOT NULL) \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                assert(count == 1);
                                assert(values[0] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                totalCount = (ulong)atol(values[0]);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }

  sqliteResult = sqlite3_exec(databaseHandle,
                              "UPDATE directoryEntries \
                               SET totalEntryCount      =0, \
                                   totalEntrySize       =0, \
                                   totalEntryCountNewest=0, \
                                   totalEntrySizeNewest =0 \
                              ",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entries.storageId, \
                                      entries.name, \
                                      fileEntries.fragmentSize \
                               FROM fileEntries \
                                 LEFT JOIN entries ON entries.id=fileEntries.entryId \
                               WHERE entries.id IS NOT NULL \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;
                                String name;
                                uint64 fragmentSize;

                                assert(count == 3);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId    = (uint64)atoll(values[0]);
                                name         = String_newCString(values[1]);
                                fragmentSize = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);

                                // update directory content count/size aggregates in all directories
                                while (!String_isEmpty(File_getDirectoryName(name,name)))
                                {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "UPDATE directoryEntries \
                                                             SET totalEntryCount=totalEntryCount+1, \
                                                                 totalEntrySize =totalEntrySize +%llu \
                                                             WHERE     storageId=%llu \
                                                                   AND name=%'S \
                                                            ",
                                                            fragmentSize,
                                                            storageId,
                                                            name
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                String_delete(name);

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }

  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entries.storageId, \
                                      entries.name \
                               FROM directoryEntries \
                                 LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                               WHERE entries.id IS NOT NULL \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;
                                String name;

                                assert(count == 2);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId = (uint64)atoll(values[0]);
                                name      = String_newCString(values[1]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                                // update directory content count/size aggregates in all directories
                                while (!String_isEmpty(File_getDirectoryName(name,name)))
                                {
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "UPDATE directoryEntries \
                                                             SET totalEntryCount=totalEntryCount+1 \
                                                             WHERE     storageId=%llu \
                                                                   AND name=%'S \
                                                            ",
                                                            storageId,
                                                            name
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                String_delete(name);

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entries.storageId, \
                                      entries.name \
                               FROM linkEntries \
                                 LEFT JOIN entries ON entries.id=linkEntries.entryId \
                               WHERE entries.id IS NOT NULL \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;
                                String name;

                                assert(count == 2);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId = (uint64)atoll(values[0]);
                                name      = String_newCString(values[1]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                                // update directory content count/size aggregates in all directories
                                while (!String_isEmpty(File_getDirectoryName(name,name)))
                                {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "UPDATE directoryEntries \
                                                             SET totalEntryCount=totalEntryCount+1 \
                                                             WHERE     storageId=%llu \
                                                                   AND name=%'S \
                                                            ",
                                                            storageId,
                                                            name
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                String_delete(name);

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entries.storageId, \
                                      entries.name, \
                                      hardlinkEntries.fragmentSize \
                               FROM hardlinkEntries \
                                 LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                               WHERE entries.id IS NOT NULL \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;
                                String name;
                                uint64 fragmentSize;

                                assert(count == 3);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);
                                assert(values[2] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId    = (uint64)atoll(values[0]);
                                name         = String_newCString(values[1]);
                                fragmentSize = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);

                                // update directory content count/size aggregates in all directories
                                while (!String_isEmpty(File_getDirectoryName(name,name)))
                                {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "UPDATE directoryEntries \
                                                             SET totalEntryCount=totalEntryCount+1, \
                                                                 totalEntrySize =totalEntrySize +%llu \
                                                             WHERE     storageId=%llu \
                                                                   AND name=%'S \
                                                            ",
                                                            fragmentSize,
                                                            storageId,
                                                            name
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLiteerror: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                String_delete(name);

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT entries.storageId, \
                                      entries.name \
                               FROM specialEntries \
                                 LEFT JOIN entries ON entries.id=specialEntries.entryId \
                               WHERE entries.id IS NOT NULL \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;
                                String name;

                                assert(count == 2);
                                assert(values[0] != NULL);
                                assert(values[1] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId = (uint64)atoll(values[0]);
                                name      = String_newCString(values[1]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                                // update directory content count/size aggregates in all directories
                                while (!String_isEmpty(File_getDirectoryName(name,name)))
                                {
                                  sqliteResult = sqlExecute(databaseHandle,
                                                            &errorMessage,
                                                            "UPDATE directoryEntries \
                                                             SET totalEntryCount=totalEntryCount+1 \
                                                             WHERE     storageId=%llu \
                                                                   AND name=%'S \
                                                            ",
                                                            storageId,
                                                            name
                                                           );
                                  if (sqliteResult != SQLITE_OK)
                                  {
                                    if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                    fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                                    return sqliteResult;
                                  }
                                }

                                String_delete(name);

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) printf("OK  \n");

  // calculate total count/size aggregates
  if (verboseFlag) { fprintf(stderr,"Create aggregates for storage..."); fflush(stderr); }

  totalCount = 0L;
  n          = 0L;
  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT (SELECT COUNT(id) FROM storage) \
                              ",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                assert(count == 1);
                                assert(values[0] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                totalCount = (ulong)atol(values[0]);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }

  sqliteResult = sqlite3_exec(databaseHandle,
                              "SELECT id FROM storage",
                              CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                              {
                                uint64 storageId;

                                assert(count == 1);
                                assert(values[0] != NULL);

                                UNUSED_VARIABLE(userData);
                                UNUSED_VARIABLE(columns);

                                storageId = (uint64)atoll(values[0]);

                                // total count/size
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE storage \
                                                           SET totalFileCount     =(SELECT COUNT(entries.id) FROM entries LEFT JOIN fileEntries      ON fileEntries.entryId     =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalImageCount    =(SELECT COUNT(entries.id) FROM entries LEFT JOIN imageEntries     ON imageEntries.entryId    =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalDirectoryCount=(SELECT COUNT(entries.id) FROM entries LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalLinkCount     =(SELECT COUNT(entries.id) FROM entries LEFT JOIN linkEntries      ON linkEntries.entryId     =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalHardlinkCount =(SELECT COUNT(entries.id) FROM entries LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalSpecialCount  =(SELECT COUNT(entries.id) FROM entries LEFT JOIN specialEntries   ON specialEntries.entryId  =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalFileSize      =(SELECT TOTAL(fileEntries.fragmentSize)                       FROM entries LEFT JOIN fileEntries     ON fileEntries.entryId    =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalImageSize     =(SELECT TOTAL(imageEntries.blockSize*imageEntries.blockCount) FROM entries LEFT JOIN imageEntries    ON imageEntries.entryId   =entries.id WHERE entries.storageId=%llu AND entries.type=%d), \
                                                               totalHardlinkSize  =(SELECT TOTAL(hardlinkEntries.fragmentSize)                   FROM entries LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id WHERE entries.storageId=%llu AND entries.type=%d) \
                                                           WHERE id=%"PRIu64" \
                                                          ",
                                                          storageId,
                                                          INDEX_CONST_TYPE_FILE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_IMAGE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_DIRECTORY,
                                                          storageId,
                                                          INDEX_CONST_TYPE_LINK,
                                                          storageId,
                                                          INDEX_CONST_TYPE_HARDLINK,
                                                          storageId,
                                                          INDEX_CONST_TYPE_SPECIAL,

                                                          storageId,
                                                          INDEX_CONST_TYPE_FILE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_IMAGE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_HARDLINK,

                                                          storageId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIu64": %s (SQLite error: %d)!\n",storageId,errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE storage \
                                                           SET totalEntryCount=totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount, \
                                                               totalEntrySize =totalFileSize +totalImageSize +                                   totalHardlinkSize \
                                                           WHERE id=%llu \
                                                          ",
                                                          storageId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIu64": %s (SQLite error: %d)!\n",storageId,errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                // total count/size newest
                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE storage \
                                                           SET totalFileCountNewest     =(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN fileEntries      ON fileEntries.entryId     =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalImageCountNewest    =(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN imageEntries     ON imageEntries.entryId    =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalDirectoryCountNewest=(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalLinkCountNewest     =(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN linkEntries      ON linkEntries.entryId     =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalHardlinkCountNewest =(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalSpecialCountNewest  =(SELECT COUNT(entriesNewest.id) FROM entriesNewest LEFT JOIN specialEntries   ON specialEntries.entryId  =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalFileSizeNewest      =(SELECT TOTAL(fileEntries.fragmentSize)                       FROM entriesNewest LEFT JOIN fileEntries     ON fileEntries.entryId    =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalImageSizeNewest     =(SELECT TOTAL(imageEntries.blockSize*imageEntries.blockCount) FROM entriesNewest LEFT JOIN imageEntries    ON imageEntries.entryId   =entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d), \
                                                               totalHardlinkSizeNewest  =(SELECT TOTAL(hardlinkEntries.fragmentSize)                   FROM entriesNewest LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.id WHERE entriesNewest.storageId=%llu AND entriesNewest.type=%d) \
                                                           WHERE id=%"PRIu64" \
                                                          ",
                                                          storageId,
                                                          INDEX_CONST_TYPE_FILE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_IMAGE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_DIRECTORY,
                                                          storageId,
                                                          INDEX_CONST_TYPE_LINK,
                                                          storageId,
                                                          INDEX_CONST_TYPE_HARDLINK,
                                                          storageId,
                                                          INDEX_CONST_TYPE_SPECIAL,

                                                          storageId,
                                                          INDEX_CONST_TYPE_FILE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_IMAGE,
                                                          storageId,
                                                          INDEX_CONST_TYPE_HARDLINK,

                                                          storageId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIu64": %s (error: %d)!\n",storageId,errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                sqliteResult = sqlExecute(databaseHandle,
                                                          &errorMessage,
                                                          "UPDATE storage \
                                                           SET totalEntryCountNewest=totalFileCountNewest+totalImageCountNewest+totalDirectoryCountNewest+totalLinkCountNewest+totalHardlinkCountNewest+totalSpecialCountNewest, \
                                                               totalEntrySizeNewest =totalFileSizeNewest +totalImageSizeNewest +                                               totalHardlinkSizeNewest \
                                                           WHERE id=%"PRIu64" \
                                                          ",
                                                          storageId
                                                         );
                                if (sqliteResult != SQLITE_OK)
                                {
                                  if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                  fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIu64": %s (SQLite error: %d)!\n",storageId,errorMessage,sqliteResult);
                                  return sqliteResult;
                                }

                                n++;
                                if (verboseFlag) printPercentage(n,totalCount);

                                return SQLITE_OK;
                              },NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    printf("FAIL\n");
    sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // end transaction
  sqliteResult = sqlite3_exec(databaseHandle,
                              "END TRANSACTION",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(1);
  }
}

/***********************************************************************\
* Name   : vacuum
* Purpose: vacuum database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void vacuum(sqlite3 *databaseHandle)
{
  int        sqliteResult;
  const char *errorMessage;

  if (verboseFlag) { fprintf(stderr,"Vacuum..."); fflush(stderr); }

  sqliteResult = sqlite3_exec(databaseHandle,
                              "VACUUM",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: vacuum fail: %s!\n",errorMessage);
    exit(1);
  }

  if (verboseFlag) fprintf(stderr,"OK  \n");
}

/***********************************************************************\
* Name   : getColumnsWidth
* Purpose: get column width
* Input  : argc    - number of columns
*          argv    - values
*          columns - column names
* Output : width - widths
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getColumnsWidth(size_t widths[], int argc, char *argv[], char *columns[])
{
  int i;

  for (i = 0; i < argc; i++)
  {
    widths[i] = 0;
    if ((argv[i]    != NULL) && (strlen(argv[i]   ) > widths[i])) widths[i] = strlen(argv[i]   );
    if ((columns[i] != NULL) && (strlen(columns[i]) > widths[i])) widths[i] = strlen(columns[i]);
  }
}

/***********************************************************************\
* Name   : printSpaces
* Purpose: print spaces
* Input  : n - number of spaces
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpaces(int n)
{
  int i;

  for (i = 0; i < n; i++)
  {
    printf(" ");
  }
}

/***********************************************************************\
* Name   : printRow
* Purpose: print row call back
* Input  : userData - user data
*          count    - number of values
*          values   - values
*          columns  - column names
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL int printRow(void *userData, int count, char *values[], char *columns[])
{
  int    i;
  size_t *widths;

  assert(count >= 0);
  assert(values != NULL);
  assert(columns != NULL);

  UNUSED_VARIABLE(userData);

  widths = (size_t*)malloc(count*sizeof(size_t));
  assert(widths != NULL);
  getColumnsWidth(widths,count,values,columns);

  if (showHeaderFlag && !headerPrintedFlag)
  {
    for (i = 0; i < count; i++)
    {
      printf("%s ",columns[i]); printSpaces(widths[i]-strlen(columns[i]));
    }
    printf("\n");

    headerPrintedFlag = TRUE;
  }
  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      if (showNamesFlag) printf("%s=",columns[i]);
      printf("%s ",!stringIsEmpty(values[i]) ? values[i] : "''"); if (showHeaderFlag) { printSpaces(widths[i]-strlen(values[i])); }
    }
    else
    {
      printf("- "); if (showHeaderFlag) { printSpaces(widths[i]-1); }
    }
  }
  printf("\n");

  free(widths);

  return SQLITE_OK;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  const uint MAX_LINE_LENGTH = 8192;

  uint            i,j,n;
  const char      *databaseFileName;
  String          s;
  String          sqlCommands;
  char            line[MAX_LINE_LENGTH];
  int             sqliteMode;
  int             sqliteResult;
  sqlite3         *databaseHandle;
  sqlite3_stmt    *statementHandles[64];
  uint            statementHandleCount;
  const char      *sqlCommand,*nextSqlCommand;
  StringTokenizer stringTokenizer;
  ConstString     string;
  union
  {
    int64  l;
    double d;
  }               value;
  long            nextIndex;
  const char      *errorMessage;
  char            buffer[4096];

  // init variables
  databaseFileName = NULL;
  sqlCommands      = String_new();

  i = 1;
  n = 0;
  while (i < (uint)argc)
  {
    if      (stringEquals(argv[i],"--info"))
    {
      infoFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--check"))
    {
      checkFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--create"))
    {
      createFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--create-triggers"))
    {
      createTriggersFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--create-newest"))
    {
      createNewestFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--create-indizes"))
    {
      createIndizesFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--create-aggregates"))
    {
      createAggregatesFlag = TRUE;
    }
    else if (stringEquals(argv[i],"--vacuum"))
    {
      vacuumFlag = TRUE;
    }
    else if (stringEquals(argv[i],"-n") || stringEquals(argv[i],"--names"))
    {
      showNamesFlag = TRUE;
    }
    else if (stringEquals(argv[i],"-H") || stringEquals(argv[i],"--header"))
    {
      showHeaderFlag = TRUE;
    }
    else if (stringEquals(argv[i],"-f") || stringEquals(argv[i],"--no-foreign-keys"))
    {
      foreignKeysFlag = FALSE;
    }
    else if (stringEquals(argv[i],"--pipe"))
    {
      pipeFlag = TRUE;
    }
    else if (stringEquals(argv[i],"-v") || stringEquals(argv[i],"--verbose"))
    {
      verboseFlag = TRUE;
    }
    else if (stringEquals(argv[i],"-h") || stringEquals(argv[i],"--help"))
    {
      printUsage(argv[0]);
      exit(0);
    }
    else if (stringEquals(argv[i],"--"))
    {
      break;
    }
    else if (stringStartsWith(argv[i],"-"))
    {
      fprintf(stderr,"ERROR: unknown option '%s'!\n",argv[i]);
      String_delete(sqlCommands);
      exit(1);
    }
    else
    {
      switch (n)
      {
        case 0: databaseFileName = argv[i]; n++; break;
        default:
          String_appendCString(sqlCommands,argv[i]);
          break;
      }
    }
    i++;
  }
  while (i < (uint)argc)
  {
    switch (n)
    {
      case 0: databaseFileName = argv[i]; n++; break;
      default:
        String_appendCString(sqlCommands,argv[i]);
        break;
    }
    i++;
  }

  // check arguments
  if (databaseFileName == NULL)
  {
    fprintf(stderr,"ERROR: no database file name given!\n");
    String_delete(sqlCommands);
    exit(1);
  }

  if (String_equalsCString(sqlCommands,"-"))
  {
    // get commands from stdin
    String_clear(sqlCommands);
    while (fgets(line,sizeof(line),stdin) != NULL)
    {
      String_appendCString(sqlCommands,line);
    }
  }

  // open database
  if (verboseFlag) { fprintf(stderr,"Open database '%s'...",databaseFileName); fflush(stderr); }
  if (createFlag)
  {
    sqliteMode = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
    File_deleteCString(databaseFileName,FALSE);
  }
  else
  {
    sqliteMode = SQLITE_OPEN_READWRITE;
  }
  sqliteResult = sqlite3_open_v2(databaseFileName,&databaseHandle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    if (verboseFlag) fprintf(stderr,"FAIL\n");
    fprintf(stderr,"ERROR: cannot open database '%s' (SQLite error: %d)!\n",databaseFileName,sqliteResult);
    String_delete(sqlCommands);
    exit(1);
  }
  sqlite3_progress_handler(databaseHandle,10000,sqlProgressHandler,NULL);

  // disable synchronous mode, enable WAL
  sqliteResult = sqlite3_exec(databaseHandle,
                              "PRAGMA synchronous=OFF",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    if (verboseFlag) fprintf(stderr,"FAIL\n");
    fprintf(stderr,"ERROR: cannot open database '%s' (SQLite error: %d)!\n",databaseFileName,sqliteResult);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "PRAGMA journal_mode=WAL",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  if (sqliteResult != SQLITE_OK)
  {
    if (verboseFlag) fprintf(stderr,"FAIL\n");
    fprintf(stderr,"ERROR: cannot open database '%s' (SQLite error: %d)!\n",databaseFileName,sqliteResult);
    exit(1);
  }
  sqliteResult = sqlite3_exec(databaseHandle,
                              "PRAGMA recursive_triggers=ON",
                              CALLBACK(NULL,NULL),
                              (char**)&errorMessage
                             );
  assert(sqliteResult == SQLITE_OK);
  if (foreignKeysFlag)
  {
    sqliteResult = sqlite3_exec(databaseHandle,
                                "PRAGMA foreign_keys=ON;",
                                CALLBACK(NULL,NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      if (verboseFlag) fprintf(stderr,"FAIL\n");
      fprintf(stderr,"ERROR: cannot open database '%s' (SQLite error: %d)!\n",databaseFileName,sqliteResult);
      String_delete(sqlCommands);
      exit(1);
    }
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // register special functions
  sqliteResult = sqlite3_create_function(databaseHandle,
                                         "unixtimestamp",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         unixTimestamp,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
#ifdef HAVE_PCRE
  sqliteResult = sqlite3_create_function(databaseHandle,
                                         "regexp",
                                         3,
                                         SQLITE_ANY,
                                         NULL,
                                         regexpMatch,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
#endif /* HAVE_PCRE */
  sqliteResult = sqlite3_create_function(databaseHandle,
                                         "dirname",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         dirname,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);

  // output info
  if (infoFlag)
  {
    // show meta data
    printf("Meta:\n");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT name,value FROM meta",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  %-11s: %s\n",values[0],values[1]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get meta data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }

    // show number of storages
    printf("Storages:\n");
    s = String_new();
    String_format(String_clear(s),"SELECT COUNT(id) FROM storage WHERE state=%d",INDEX_CONST_STATE_OK);
    sqliteResult = sqlite3_exec(databaseHandle,
                                String_cString(s),
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  OK         : %s\n",values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      String_delete(s);
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    String_format(String_clear(s),"SELECT COUNT(id) FROM storage WHERE state=%d",INDEX_CONST_STATE_ERROR);
    sqliteResult = sqlite3_exec(databaseHandle,
                                String_cString(s),
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Error      : %s\n",values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      String_delete(s);
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    String_format(String_clear(s),"SELECT COUNT(id) FROM storage WHERE state=%d",INDEX_CONST_STATE_DELETED);
    sqliteResult = sqlite3_exec(databaseHandle,
                                String_cString(s),
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Deleted    : %s\n",values[0]);

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      String_delete(s);
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    String_delete(s);

    // show number of entries, newest entries
    printf("Entries:\n");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalEntryCount),TOTAL(totalEntrySize) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Total      : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalFileCount),TOTAL(totalFileSize) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Files      : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalImageCount),TOTAL(totalImageSize) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Images     : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalDirectoryCount) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Directories: %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalLinkCount) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Links      : %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalHardlinkCount),TOTAL(totalHardlinkSize) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Hardlinks  : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalSpecialCount) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Special    : %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }

    // show number of newest entries
    printf("Newest entries:\n");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalEntryCountNewest),TOTAL(totalEntrySizeNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Total      : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalFileCountNewest),TOTAL(totalFileSizeNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Files      : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalImageCountNewest),TOTAL(totalImageSizeNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Images     : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalDirectoryCountNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Directories: %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalLinkCountNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Links      : %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalHardlinkCountNewest),TOTAL(totalHardlinkSizeNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 2);
                                  assert(values[0] != NULL);
                                  assert(values[1] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Hardlinks  : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
    sqliteResult = sqlite3_exec(databaseHandle,
                                "SELECT TOTAL(totalSpecialCountNewest) FROM storage",
                                CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                {
                                  assert(count == 1);
                                  assert(values[0] != NULL);

                                  UNUSED_VARIABLE(userData);
                                  UNUSED_VARIABLE(columns);

                                  printf("  Special    : %lu\n",atol(values[0]));

                                  return SQLITE_OK;
                                },NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: get entries data fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
  }

  if (checkFlag)
  {
    // check database
    printf("Check:\n");
    fprintf(stderr,"  Quick integrity check...");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "PRAGMA quick_check;",
                                CALLBACK(NULL,NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult == SQLITE_OK)
    {
      fprintf(stderr,"ok\n");
    }
    else
    {
      fprintf(stderr,"FAIL: %s!\n",errorMessage);
    }

    fprintf(stderr,"  Foreign key check...");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "PRAGMA foreign_key_check;",
                                CALLBACK(NULL,NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult == SQLITE_OK)
    {
      fprintf(stderr,"ok\n");
    }
    else
    {
      fprintf(stderr,"FAIL: %s!\n",errorMessage);
    }

    fprintf(stderr,"  Full integrity check...");
    sqliteResult = sqlite3_exec(databaseHandle,
                                "PRAGMA integrit_check;",
                                CALLBACK(NULL,NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult == SQLITE_OK)
    {
      fprintf(stderr,"ok\n");
    }
    else
    {
      fprintf(stderr,"FAIL: %s!\n",errorMessage);
    }
  }

  // create database
  if (createFlag)
  {
    if (verboseFlag) { fprintf(stderr,"Create..."); fflush(stderr); }

    sqliteResult = sqlite3_exec(databaseHandle,
                                INDEX_DEFINITION,
                                CALLBACK(NULL,NULL),
                                (char**)&errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create database fail: %s!\n",errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }

    if (verboseFlag) fprintf(stderr,"OK  \n");
  }

  // recreate triggeres
  if (createTriggersFlag)
  {
    createTriggers(databaseHandle);
  }

  // recreate newest data
  if (createNewestFlag)
  {
    createNewest(databaseHandle);
  }

  // recreate indizes
  if (createIndizesFlag)
  {
    createIndizes(databaseHandle);
  }

  // calculate aggregate data
  if (createAggregatesFlag)
  {
    createAggregates(databaseHandle);
  }

  // vacuum
  if (vacuumFlag)
  {
    vacuum(databaseHandle);
  }

  // execute command
  if (!String_isEmpty(sqlCommands))
  {
    if (pipeFlag)
    {
      // pipe from stdin

      // start transaction
      sqliteResult = sqlite3_exec(databaseHandle,
                                  "BEGIN TRANSACTION",
                                  CALLBACK(NULL,NULL),
                                  (char**)&errorMessage
                                 );
      if (sqliteResult != SQLITE_OK)
      {
        printf("FAIL\n");
        fprintf(stderr,"ERROR: start transaction fail: %s!\n",errorMessage);
        String_delete(sqlCommands);
        exit(1);
      }

      // prepare SQL statements
      statementHandleCount = 0;
      sqlCommand = String_cString(sqlCommands);
      while (!stringIsEmpty(sqlCommand))
      {
        if (statementHandleCount > SIZE_OF_ARRAY(statementHandles))
        {
          fprintf(stderr,"ERROR: too many SQL commands (limit %lu)!\n",SIZE_OF_ARRAY(statementHandles));
          String_delete(sqlCommands);
          exit(1);
        }

        sqliteResult = sqlite3_prepare_v2(databaseHandle,
                                          sqlCommand,
                                          -1,
                                          &statementHandles[statementHandleCount],
                                          &nextSqlCommand
                                         );
        if (verboseFlag) fprintf(stderr,"Result: %d\n",sqliteResult);
        if (sqliteResult != SQLITE_OK)
        {
          sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
          fprintf(stderr,"ERROR: SQL command #%u: '%s' fail: %s!\n",i+1,sqlCommand,sqlite3_errmsg(databaseHandle));
          String_delete(sqlCommands);
          exit(1);
        }
        statementHandleCount++;
        sqlCommand = stringTrimBegin(nextSqlCommand);
      }

      s = String_new();
      while (fgets(line,sizeof(line),stdin) != NULL)
      {
//fprintf(stderr,"%s, %d: line=%s\n",__FILE__,__LINE__,line);
        String_trim(String_setCString(s,line),STRING_WHITE_SPACES);
        if (verboseFlag) fprintf(stderr,"%s...",String_cString(s));

        // reset SQL statements
        for (i = 0; i < statementHandleCount; i++)
        {
          sqlite3_reset(statementHandles[i]);
        }

        // parse input
        i = 0;
        String_initTokenizer(&stringTokenizer,s,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,TRUE);
        i = 0;
        j = 1;
        while ((i < statementHandleCount) && String_getNextToken(&stringTokenizer,&string,NULL))
        {
//fprintf(stderr,"%s, %d: %d %d -> %s\n",__FILE__,__LINE__,i,j,String_cString(string));
          do
          {
            nextIndex = STRING_BEGIN;
            if      (nextIndex != STRING_END)
            {
              value.l = String_toInteger64(string,STRING_BEGIN,&nextIndex,NULL,0);
              if (nextIndex == STRING_END)
              {
                sqliteResult = sqlite3_bind_int64(statementHandles[i],j,value.l);
              }
            }
            if (nextIndex != STRING_END)
            {
              value.d = String_toDouble(string,STRING_BEGIN,&nextIndex,NULL,0);
              if (nextIndex == STRING_END)
              {
                sqliteResult = sqlite3_bind_double(statementHandles[i],j,value.d);
              }
            }
            if (nextIndex != STRING_END)
            {
              nextIndex = STRING_END;
              sqliteResult = sqlite3_bind_text(statementHandles[i],j,String_cString(string),String_length(string),SQLITE_TRANSIENT);
            }
            if (nextIndex != STRING_END)
            {
              String_delete(s);
              sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
              fprintf(stderr,"ERROR: Invalid data '%s'!\n",String_cString(string));
              String_delete(sqlCommands);
              exit(1);
            }

            if (sqliteResult == SQLITE_OK)
            {
              // next argument
              j++;
            }
            else
            {
              // next statement
              i++;
              j = 1;
            }
          }
          while ((sqliteResult != SQLITE_OK) && (i < statementHandleCount));
        }
        String_doneTokenizer(&stringTokenizer);

        // execute SQL commands
        for (i = 0; i < statementHandleCount; i++)
        {
          sqliteResult = sqlite3_step(statementHandles[i]);
          if (sqliteResult != SQLITE_DONE) break;
        }
        if (verboseFlag) fprintf(stderr,"Result: %d\n",sqliteResult);
        if (sqliteResult != SQLITE_DONE)
        {
          String_delete(s);
          sqlite3_exec(databaseHandle,"ROLLBACK TRANSACTION",CALLBACK(NULL,NULL),NULL);
          fprintf(stderr,"ERROR: SQL command #%u: '%s' fail: %s!\n",i+1,String_cString(sqlCommands),sqlite3_errmsg(databaseHandle));
          String_delete(sqlCommands);
          exit(1);
        }
      }
      String_delete(s);

      // free resources
      for (i = 0; i < statementHandleCount; i++)
      {
        sqlite3_finalize(statementHandles[i]);
      }

      // end transaction
      sqliteResult = sqlite3_exec(databaseHandle,
                                  "END TRANSACTION",
                                  CALLBACK(NULL,NULL),
                                  (char**)&errorMessage
                                 );
      if (sqliteResult != SQLITE_OK)
      {
        fprintf(stderr,"ERROR: end transaction fail: %s!\n",errorMessage);
        String_delete(sqlCommands);
        exit(1);
      }
    }
    else
    {
      // single command execution
      sqliteResult = sqlite3_exec(databaseHandle,
                                  String_cString(sqlCommands),
                                  CALLBACK(printRow,NULL),
                                  (char**)&errorMessage
                                 );
      if (verboseFlag) fprintf(stderr,"Result: %d\n",sqliteResult);
      if (sqliteResult != SQLITE_OK)
      {
        fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",String_cString(sqlCommands),errorMessage);
        String_delete(sqlCommands);
        exit(1);
      }
    }
  }

  while (fgets(buffer,sizeof(buffer),stdin) != NULL)
  {
    stringTrim(buffer);
    sqliteResult = sqlite3_exec(databaseHandle,
                                buffer,
                                CALLBACK(printRow,NULL),
                                (char**)&errorMessage
                               );
    if (verboseFlag) fprintf(stderr,"Result: %d\n",sqliteResult);
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",buffer,errorMessage);
      String_delete(sqlCommands);
      exit(1);
    }
  }

  // close database
  if (verboseFlag) { fprintf(stderr,"Close database..."); fflush(stderr); }
  sqlite3_close(databaseHandle);
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // free resources
  String_delete(sqlCommands);

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
