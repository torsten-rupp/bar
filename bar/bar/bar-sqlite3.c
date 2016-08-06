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
#include <time.h>
#include <pcreposix.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "files.h"

#include "sqlite3.h"

#include "index_definition.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL bool create         = FALSE;
LOCAL bool createIndizes  = FALSE;
LOCAL bool createTriggers = FALSE;
LOCAL bool showNames      = FALSE;
LOCAL bool showHeader     = FALSE;
LOCAL bool headerPrinted  = FALSE;
LOCAL bool foreignKeys    = TRUE;
LOCAL bool verbose        = FALSE;

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
  printf("Usage %s: [<options>] <database file> [<command>...]\n",programName);
  printf("\n");
  printf("Options:  -c|--create       - create index file\n");
  printf("          --create-indizes  - re-create indizes\n");
  printf("          --create-triggers - re-create triggers\n");
  printf("          -n|--names        - print named values\n");
  printf("          -H|--header       - print headers\n");
  printf("          -v|--verbose      - verbose output\n");
  printf("          -h|--help         - print this help\n");
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

  if (showHeader && !headerPrinted)
  {
    for (i = 0; i < count; i++)
    {
      printf("%s ",columns[i]); printSpaces(widths[i]-strlen(columns[i]));
    }
    printf("\n");

    headerPrinted = TRUE;
  }
  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      if (showNames) printf("%s=",columns[i]);
      printf("%s ",values[i]); if (showHeader) { printSpaces(widths[i]-strlen(values[i])); }
    }
    else
    {
      printf("- "); if (showHeader) { printSpaces(widths[i]-1); }
    }
  }
  printf("\n");

  free(widths);

  return SQLITE_OK;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  int        i,n;
  const char *databaseFileName;
  String     sqlCommands;
  char       line[2048];
  int        sqliteMode;
  char       command[1024];
  char       name[1024];
  int        sqliteResult;
  sqlite3    *handle;
  char       *errorMessage;

  databaseFileName = NULL;
  sqlCommands      = String_new();
  i = 1;
  n = 0;
  while (i < argc)
  {
    if      (stringEquals(argv[i],"-c") || stringEquals(argv[i],"--create"))
    {
      create = TRUE;
    }
    else if (stringEquals(argv[i],"--create-indizes"))
    {
      createIndizes = TRUE;
    }
    else if (stringEquals(argv[i],"--create-triggers"))
    {
      createTriggers = TRUE;
    }
    else if (stringEquals(argv[i],"-n") || stringEquals(argv[i],"--names"))
    {
      showNames = TRUE;
    }
    else if (stringEquals(argv[i],"-H") || stringEquals(argv[i],"--header"))
    {
      showHeader = TRUE;
    }
    else if (stringEquals(argv[i],"-n") || stringEquals(argv[i],"--no-foreign-keys"))
    {
      foreignKeys = FALSE;
    }
    else if (stringEquals(argv[i],"-v") || stringEquals(argv[i],"--verbose"))
    {
      verbose = TRUE;
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
  while (i < argc)
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
    exit(1);
  }

  if (String_isEmpty(sqlCommands))
  {
    // get commands from stdin
    while (fgets(line,sizeof(line),stdin) != NULL)
    {
      String_appendCString(sqlCommands,line);
    }
  }

  // open database
  if (create)
  {
    sqliteMode = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
    File_deleteCString(databaseFileName,FALSE);
  }
  else
  {
    sqliteMode = SQLITE_OPEN_READWRITE;
  }
  sqliteResult = sqlite3_open_v2(databaseFileName,&handle,sqliteMode,NULL);
  if (sqliteResult != SQLITE_OK)
  {
    fprintf(stderr,"ERROR: cannot open database '%s' (error: %d)!\n",databaseFileName,sqliteResult);
    exit(1);
  }

  sqliteResult = sqlite3_exec(handle,
                              "PRAGMA synchronous=OFF",
                              CALLBACK(NULL,NULL),
                              &errorMessage
                             );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_exec(handle,
                              "PRAGMA journal_mode=WAL",
                              CALLBACK(NULL,NULL),
                              &errorMessage
                             );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_exec(handle,
                              "PRAGMA recursive_triggers=ON",
                              CALLBACK(NULL,NULL),
                              &errorMessage
                             );
  assert(sqliteResult == SQLITE_OK);
  if (foreignKeys)
  {
    sqliteResult = sqlite3_exec(handle,
                                "PRAGMA foreign_keys=ON;",
                                CALLBACK(NULL,NULL),
                                &errorMessage
                               );
    assert(sqliteResult == SQLITE_OK);
  }

  // register special functions
  sqliteResult = sqlite3_create_function(handle,
                                         "unixtimestamp",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         unixTimestamp,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_create_function(handle,
                                         "regexp",
                                         3,
                                         SQLITE_ANY,
                                         NULL,
                                         regexpMatch,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);
  sqliteResult = sqlite3_create_function(handle,
                                         "dirname",
                                         1,
                                         SQLITE_ANY,
                                         NULL,
                                         dirname,
                                         NULL,
                                         NULL
                                        );
  assert(sqliteResult == SQLITE_OK);

  // create database
  if (create)
  {
    if (verbose) printf("Create...");

    sqliteResult = sqlite3_exec(handle,
                                INDEX_DEFINITION,
                                CALLBACK(NULL,NULL),
                                &errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create database fail: %s!\n",errorMessage);
      exit(1);
    }

    if (verbose) printf("OK\n");
  }

  // recreate indizes
  if (createIndizes)
  {
    if (verbose) printf("Create indizes...");

    // delete all existing indizes
    do
    {
      stringClear(name);
      sqliteResult = sqlite3_exec(handle,
                                  "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'index%'",
                                  CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                  {
                                    assert(count == 1);
                                    assert(values[0] != NULL);

                                    stringCopy(name,values[0],sizeof(name));

                                    return SQLITE_OK;
                                  },NULL),
                                  &errorMessage
                                 );

      if ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name))
      {
        stringFormat(command,sizeof(command),"DROP INDEX %s",name);
        sqliteResult = sqlite3_exec(handle,
                                    command,
                                    CALLBACK(NULL,NULL),
                                    &errorMessage
                                   );
      }
    }
    while ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name));
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create indizes fail: %s!\n",errorMessage);
      exit(1);
    }

    // create new triggeres
    sqliteResult = sqlite3_exec(handle,
                                INDEX_INDIZES_DEFINITION,
                                CALLBACK(NULL,NULL),
                                &errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create indizes fail: %s!\n",errorMessage);
      exit(1);
    }

    if (verbose) printf("OK\n");
  }

  // recreate triggeres
  if (createTriggers)
  {
    if (verbose) printf("Create triggers...");

    // delete all existing triggers
    do
    {
      stringClear(name);
      sqliteResult = sqlite3_exec(handle,
                                  "SELECT name FROM sqlite_master WHERE type='trigger' AND name LIKE 'trigger%'",
                                  CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                                  {
                                    assert(count == 1);
                                    assert(values[0] != NULL);

                                    stringCopy(name,values[0],sizeof(name));

                                    return SQLITE_OK;
                                  },NULL),
                                  &errorMessage
                                 );

      if ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name))
      {
        stringFormat(command,sizeof(command),"DROP TRIGGER %s",name);
        sqliteResult = sqlite3_exec(handle,
                                    command,
                                    CALLBACK(NULL,NULL),
                                    &errorMessage
                                   );
      }
    }
    while ((sqliteResult == SQLITE_OK) && !stringIsEmpty(name));
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create triggers fail: %s!\n",errorMessage);
      exit(1);
    }

    // create new triggeres
    sqliteResult = sqlite3_exec(handle,
                                INDEX_TRIGGERS_DEFINITION,
                                CALLBACK(NULL,NULL),
                                &errorMessage
                               );
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: create triggers fail: %s!\n",errorMessage);
      exit(1);
    }

    if (verbose) printf("OK\n");
  }

  // execute command
  if (!String_isEmpty(sqlCommands))
  {
    sqliteResult = sqlite3_exec(handle,
                                String_cString(sqlCommands),
                                CALLBACK(printRow,NULL),
                                &errorMessage
                               );
    if (verbose) printf("Result: %d\n",sqliteResult);
    if (sqliteResult != SQLITE_OK)
    {
      fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",String_cString(sqlCommands),errorMessage);

      exit(1);
    }
  }

  // close database
  sqlite3_close(handle);

  // free resources
  String_delete(sqlCommands);

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
