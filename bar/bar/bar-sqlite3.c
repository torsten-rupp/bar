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
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_PCRE
  #include <pcreposix.h>
#endif
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/database.h"

#include "sqlite3.h"

#include "index_definition.h"
#include "archive_format_const.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

// program exit codes
typedef enum
{
  EXITCODE_OK                     =   0,
  EXITCODE_FAIL                   =   1,

  EXITCODE_INVALID_ARGUMENT       =   5,

  EXITCODE_FATAL_ERROR            = 126,
  EXITCODE_FUNCTION_NOT_SUPPORTED = 127,

  EXITCODE_UNKNOWN                = 128
} ExitCodes;

// archive types
const char *ARCHIVE_TYPES[] =
{
  [CHUNK_CONST_ARCHIVE_TYPE_NONE        ] = "none",
  [CHUNK_CONST_ARCHIVE_TYPE_NORMAL      ] = "normal",
  [CHUNK_CONST_ARCHIVE_TYPE_FULL        ] = "full",
  [CHUNK_CONST_ARCHIVE_TYPE_INCREMENTAL ] = "incremental",
  [CHUNK_CONST_ARCHIVE_TYPE_DIFFERENTIAL] = "differential",
  [CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS  ] = "continuous"
};


/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL bool       infoFlag              = FALSE;  // output index database info
LOCAL bool       checkFlag             = FALSE;  // check database
LOCAL bool       createFlag            = FALSE;  // create new index database
LOCAL bool       createIndizesFlag     = FALSE;  // re-create indizes
LOCAL bool       createTriggersFlag    = FALSE;  // re-create triggers
LOCAL bool       createNewestFlag      = FALSE;  // re-create newest data
LOCAL bool       createAggregatesFlag  = FALSE;  // re-create aggregate data
LOCAL bool       cleanFlag             = FALSE;  // execute clean
LOCAL bool       purgeDeletedFlag      = FALSE;  // execute purge deleted storages
LOCAL bool       vacuumFlag            = FALSE;  // execute vacuum
LOCAL bool       showStoragesFlag      = FALSE;  // show storage of job
LOCAL bool       showEntriesFlag       = FALSE;  // show entries of job
LOCAL bool       showEntriesNewestFlag = FALSE;  // show newest entries of job
LOCAL bool       showNamesFlag         = FALSE;
LOCAL bool       showHeaderFlag        = FALSE;
LOCAL bool       headerPrintedFlag     = FALSE;
LOCAL bool       foreignKeysFlag       = TRUE;
LOCAL bool       forceFlag             = TRUE;
LOCAL bool       pipeFlag              = FALSE;
LOCAL const char *tmpDirectory         = NULL;
LOCAL bool       verboseFlag           = FALSE;
LOCAL const char *jobUUID              = NULL;

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
  printf("Options:  --info                    - output index database infos\n");
  printf("          --create                  - create new index database\n");
  printf("          --create-triggers         - re-create triggers\n");
  printf("          --create-newest           - re-create newest data\n");
  printf("          --create-indizes          - re-create indizes\n");
  printf("          --create-aggregates       - re-create aggregated data\n");
  printf("          --check                   - check index database integrity\n");
  printf("          --clean                   - clean index database\n");
  printf("          --vacuum                  - collect and free unused file space\n");
  printf("          -s|--storages [<uuid>]    - print storages\n");
  printf("          -e|--entries [<uuid>]     - print entries\n");
  printf("          --entries-newest [<uuid>] - print newest entries\n");
  printf("          -n|--names                - print named values\n");
  printf("          -H|--header               - print headers\n");
  printf("          -f|--no-foreign-keys      - disable foreign key constraints\n");
  printf("          --force                   - force operation\n");
  printf("          --pipe                    - read data from stdin and pipe into database (use ? as variable)\n");
  printf("          --tmp-directory           - temporary files directory\n");
  printf("          -v|--verbose              - verbose output\n");
  printf("          -h|--help                 - print this help\n");
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
* Name   : createDatabase
* Purpose: create database
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createDatabase(DatabaseHandle *databaseHandle, const char *databaseFileName)
{
  Errors error;

  if (verboseFlag) { fprintf(stderr,"Create..."); fflush(stderr); }

  // check if exists
  if (!forceFlag && File_existsCString(databaseFileName))
  {
    return ERROR_DATABASE_EXISTS;
  }

  // delete existing file
  (void)File_deleteCString(databaseFileName,FALSE);

  // create new database
  error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,WAIT_FOREVER);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create database fail: %s!\n",Error_getText(error));
    return error;
  }

  // create tables, triggers
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           INDEX_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create database fail: %s!\n",Error_getText(error));
    return error;
  }

  if (verboseFlag) fprintf(stderr,"OK  \n");

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : openDatabase
* Purpose: open database
* Input  : -
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors openDatabase(DatabaseHandle *databaseHandle, const char *databaseFileName)
{
  Errors error;

  if (verboseFlag) { fprintf(stderr,"Open database '%s'...",databaseFileName); fflush(stderr); }

  error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,WAIT_FOREVER);
  if (error != ERROR_NONE)
  {
    if (verboseFlag) fprintf(stderr,"FAIL\n");
    fprintf(stderr,"ERROR: cannot open database '%s' (Error: %s)!\n",databaseFileName,Error_getText(error));
    return error;
  }

  if (verboseFlag) fprintf(stderr,"OK  \n");

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

LOCAL void closeDatabase(DatabaseHandle *databaseHandle)
{
  if (verboseFlag) { fprintf(stderr,"Close database..."); fflush(stderr); }
  Database_close(databaseHandle);
  if (verboseFlag) fprintf(stderr,"OK  \n");
}

/***********************************************************************\
* Name   : checkDatabase
* Purpose: check database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void checkDatabase(DatabaseHandle *databaseHandle)
{
  Errors error;

  printf("Check:\n");

  fprintf(stderr,"  Quick integrity...");
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "PRAGMA quick_check;"
                          );
  if (error == ERROR_NONE)
  {
    fprintf(stderr,"ok\n");
  }
  else
  {
    fprintf(stderr,"FAIL: %s!\n",Error_getText(error));
  }

  fprintf(stderr,"  Foreign key...");
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "PRAGMA foreign_key_check;"
                          );
  if (error == ERROR_NONE)
  {
    fprintf(stderr,"ok\n");
  }
  else
  {
    fprintf(stderr,"FAIL: %s!\n",Error_getText(error));
  }

  if (verboseFlag) { fprintf(stderr,"Create triggers..."); fflush(stderr); }
  fprintf(stderr,"  Full integrity...");
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "PRAGMA integrit_check;"
                          );
  if (error == ERROR_NONE)
  {
    fprintf(stderr,"ok\n");
  }
  else
  {
    fprintf(stderr,"FAIL: %s!\n",Error_getText(error));
  }
}

/***********************************************************************\
* Name   : createTriggers
* Purpose: create triggers
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createTriggers(DatabaseHandle *databaseHandle)
{
  Errors error;
  char   name[1024];

  if (verboseFlag) { fprintf(stderr,"Create triggers..."); fflush(stderr); }

  // delete all existing triggers
  do
  {
    stringClear(name);
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                               stringSet(name,sizeof(name),values[0]);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             "SELECT name FROM sqlite_master WHERE type='trigger' AND name LIKE 'trigger%%'"
                            );

    if ((error == ERROR_NONE) && !stringIsEmpty(name))
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DROP TRIGGER %s",
                               name
                              );
    }
  }
  while ((error == ERROR_NONE) && !stringIsEmpty(name));
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create triggers fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // create new triggeres
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           INDEX_TRIGGERS_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create triggers fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
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

LOCAL void createIndizes(DatabaseHandle *databaseHandle)
{
  Errors error;
  char   name[1024];

  if (verboseFlag) { fprintf(stderr,"Create indizes:\n"); }

  // start transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "BEGIN TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // delete all existing indizes
  if (verboseFlag) { fprintf(stderr,"  Discard indizes..."); }
  do
  {
    stringClear(name);
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                               stringSet(name,sizeof(name),values[0]);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'index%%' LIMIT 0,1"
                            );
    if ((error == ERROR_NONE) && !stringIsEmpty(name))
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DROP INDEX %s",
                               name
                              );
    }
  }
  while ((error == ERROR_NONE) && !stringIsEmpty(name));
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  do
  {
    stringClear(name);
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                               stringSet(name,sizeof(name),values[0]);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'FTS_%%' LIMIT 0,1"
                            );
    if ((error == ERROR_NONE) && !stringIsEmpty(name))
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DROP TABLE %s",
                               name
                              );
    }

    Database_flush(databaseHandle);
    sqlProgressHandler(NULL);
  }
  while ((error == ERROR_NONE) && !stringIsEmpty(name));
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // end transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "END TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // start transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "BEGIN TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // create new indizes
  if (verboseFlag) { fprintf(stderr,"  Create new indizes..."); }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           INDEX_INDIZES_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  if (verboseFlag) { fprintf(stderr,"  Create new FTS..."); }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           INDEX_FTS_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create FTS fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK\n"); }

  // clear FTS names
  if (verboseFlag) { fprintf(stderr,"  Discard FTS indizes..."); }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DELETE FROM FTS_storage"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "DELETE FROM FTS_entries"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // create FTS names
  if (verboseFlag) { fprintf(stderr,"  Create new storage FTS index..."); }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "INSERT INTO FTS_storage SELECT id,name FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }
  if (verboseFlag) { fprintf(stderr,"  Create new entries FTS index..."); }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "INSERT INTO FTS_entries SELECT id,name FROM entries"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) { fprintf(stderr,"OK  \n"); }

  // end transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "END TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: recreate indizes fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
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

LOCAL void createNewest(DatabaseHandle *databaseHandle)
{
  Errors error;
  ulong  totalEntriesCount,totalEntriesNewestCount;
  ulong  n,m;

  // start transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "BEGIN TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // set entries offset/size
  if (verboseFlag) { fprintf(stderr,"Create newest entries..."); fflush(stderr); }

  // get total counts
  totalEntriesCount       = 0L;
  totalEntriesNewestCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalEntriesCount = (ulong)atol(values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM entries \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalEntriesNewestCount = (ulong)atol(values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM entriesNewest \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // delete newest
  do
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             &m,
                             "DELETE FROM entriesNewest LIMIT 0,1000"
                            );
    n += m;
//fprintf(stderr,"%s, %d: m=%llu\n",__FILE__,__LINE__,m);
    if (verboseFlag) printPercentage(n,totalEntriesCount+totalEntriesNewestCount);
  }
  while ((error == ERROR_NONE) && (m > 0));
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // insert newest
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             bool       existsFlag;
                             uint64     entryId;
                             uint64     storageId;
                             const char *name;
                             uint       type;
                             uint64     timeLastChanged;
                             uint       userId;
                             uint       groupId;
                             uint       permission;
                             uint64     size;

                             assert(count == 9);
                             assert(values != NULL);
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

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             entryId         = (uint64)atoll(values[0]);
                             storageId       = (uint64)atoll(values[1]);
                             name            = values[2];
                             type            = (uint)atol(values[3]);
                             timeLastChanged = (uint64)atoll(values[4]);
                             userId          = (uint)atol(values[5]);
                             groupId         = (uint)atol(values[6]);
                             permission      = (uint)atol(values[7]);
                             size            = (uint64)atoll(values[8]);
//fprintf(stderr,"%s, %d: %llu name=%s offset=%llu size=%llu timeLastChanged=%llu\n",__FILE__,__LINE__,entryId,name,offset,size,timeLastChanged);

                             // check if exists
                             existsFlag = FALSE;
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                                      {
                                                        assert(count == 1);
                                                        assert(values != NULL);
                                                        assert(values[0] != NULL);

                                                        UNUSED_VARIABLE(columns);
                                                        UNUSED_VARIABLE(count);
                                                        UNUSED_VARIABLE(userData);

                                                        existsFlag = (values[0] != NULL);

                                                        return ERROR_NONE;
                                                      },NULL),
                                                      NULL,  // changedRowCount
                                                      "SELECT id \
                                                       FROM entriesNewest \
                                                       WHERE name=%'s \
                                                      ",
                                                      name
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest fail for entries (error: %s)!\n",Error_getText(error));
                               return error;
                             }
#if 0
String s = String_new();
error = Database_getString(databaseHandle,s,"entries","entities.jobUUID","left join storage on storage.id=entries.storageId left join entities on entities.id=storage.entityId where entries.id=%d",entryId);
fprintf(stderr,"%s, %d: %lu name=%s size=%lu timeLastChanged=%lu job=%s existsFlag=%d error=%s\n",__FILE__,__LINE__,entryId,name,size,timeLastChanged,String_cString(s),existsFlag,Error_getText(error));
//exit(1);
String_delete(s);
#endif

                             if (!existsFlag)
                             {
                               // insert
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "INSERT INTO entriesNewest \
                                                           (entryId,\
                                                            storageId, \
                                                            name, \
                                                            type, \
                                                            timeLastChanged, \
                                                            userId, \
                                                            groupId, \
                                                            permission, \
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
                                                        size
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create newest fail for entries (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalEntriesCount+totalEntriesNewestCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id, \
                                   storageId, \
                                   name, \
                                   type, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission, \
                                   size \
                             FROM entries \
                             ORDER BY name,timeLastChanged DESC \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // end transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "END TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create newest fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
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

LOCAL void createAggregates(DatabaseHandle *databaseHandle)
{
  Errors error;
  ulong  totalCount;
  ulong  n;

  // start transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "BEGIN TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

#if 0
//TODO: remove?
  // set entries offset/size
  if (verboseFlag) { fprintf(stderr,"Create aggregates for entries..."); fflush(stderr); }

  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(userData);

                             totalCount = (ulong)atol(values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT  (SELECT COUNT(id) FROM fileEntries    ) \
                                   +(SELECT COUNT(id) FROM imageEntries   ) \
                                   +(SELECT COUNT(id) FROM hardlinkEntries) \
                                   +(SELECT COUNT(id) FROM entries        ) \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(EXITCODE_FAIL);
  }

  n = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 entryId;
                             uint64 fragmentOffset;
                             uint64 fragmentSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(userData);

                             entryId        = (uint64)atoll(values[0]);
                             fragmentOffset = (uint64)atoll(values[1]);
                             fragmentSize   = (uint64)atoll(values[2]);
      //fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                             // set offset/size
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entries \
                                                       SET offset=%llu, \
                                                           size  =%llu \
                                                       WHERE id=%llu \
                                                      ",
                                                      fragmentOffset,
                                                      fragmentSize,
                                                      entryId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for entries: %s (Error: %s)!\n",Error_getText(error));
                               return error;
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entryId,fragmentOffset,fragmentSize FROM fileEntries"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(EXITCODE_FAIL);
  }
  error = sqlite3_exec(databaseHandle,
                       "SELECT entryId,blockSize,blockOffset,blockCount FROM imageEntries",
                       CALLBACK_INLINE(Errors,(void *userData, int count, char *values[], char *columns[]),
                       {
                         uint64 entryId;
                         ulong  blockSize;
                         uint64 blockOffset;
                         uint64 blockCount;

                         assert(count == 4);
                         assert(values != NULL);
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
                         error = Database_execute(databaseHandle,
                                                  CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                  NULL,  // changedRowCount
                                                  "UPDATE entries \
                                                   SET offset=%llu, \
                                                       size  =%llu \
                                                   WHERE id=%llu \
                                                  ",
                                                  (uint64)blockSize*blockOffset,
                                                  (uint64)blockSize*blockCount,
                                                  entryId
                                                 );
                         if (error != ERROR_NONE)
                         {
                           if (verboseFlag) fprintf(stderr,"FAIL!\n");
                           fprintf(stderr,"ERROR: create aggregates fail for entries: %s (Error: %s)!\n",Error_getText(error));
                           return sqliteResult;
                         }

                         n++;
                         if (verboseFlag) printPercentage(n,totalCount);

                         return ERROR_NONE;
                       },NULL),
                       (char**)&errorMessage
                      );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(EXITCODE_FAIL);
  }
  error = sqlite3_exec(databaseHandle,
                       "SELECT entryId,fragmentOffset,fragmentSize FROM hardlinkEntries",
                     CALLBACK_INLINE(int,(void *userData, int count, char *values[], char *columns[]),
                       {
                         uint64 entryId;
                         uint64 fragmentOffset;
                         uint64 fragmentSize;

                         assert(count == 3);
                         assert(values != NULL);
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
                         error = Database_execute(databaseHandle,
                                                  CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                  NULL,  // changedRowCount
                                                  "UPDATE entries \
                                                   SET offset=%llu, \
                                                       size  =%llu \
                                                   WHERE id=%llu \
                                                  ",
                                                  fragmentOffset,
                                                  fragmentSize,
                                                  entryId
                                                 );
                         if (error != ERROR_NONE)
                         {
                           if (verboseFlag) fprintf(stderr,"FAIL!\n");
                           fprintf(stderr,"ERROR: create aggregates fail for entries: %s (Error: %s)!\n",Error_getText(error));
                           return error;
                         }

                         n++;
                         if (verboseFlag) printPercentage(n,totalCount);

                         return ERROR_NONE;
                       },NULL),
                       (char**)&errorMessage
                      );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 entryId;
                             uint64 offset;
                             uint64 size;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(userData);

                             entryId = (uint64)atoll(values[0]);
                             offset  = (uint64)atoll(values[1]);
                             size    = (uint64)atoll(values[2]);
      //fprintf(stderr,"%s, %d: %llu %llu %llu\n",__FILE__,__LINE__,entryId,fragmentOffset,fragmentSize);

                             // set offset/size
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entriesNewest \
                                                       SET offset=%llu, \
                                                           size  =%llu \
                                                       WHERE entryId=%llu \
                                                      ",
                                                      offset,
                                                      size,
                                                      entryId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for entries: %s (SQLite error: %d)!\n",errorMessage,sqliteResult);
                               return sqliteResult;
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id,offset,size FROM entries"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",errorMessage);
    exit(EXITCODE_FAIL);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");
#endif

  // calculate directory content size/count aggregated data
  if (verboseFlag) { fprintf(stderr,"Create aggregates for directory content..."); fflush(stderr); }

  // get total count
  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalCount = (ulong)atol(values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT  (SELECT COUNT(entries.id) FROM fileEntries      LEFT JOIN entries ON entries.id=fileEntries.entryId      WHERE entries.id IS NOT NULL) \
                                   +(SELECT COUNT(entries.id) FROM directoryEntries LEFT JOIN entries ON entries.id=directoryEntries.entryId WHERE entries.id IS NOT NULL) \
                                   +(SELECT COUNT(entries.id) FROM linkEntries      LEFT JOIN entries ON entries.id=linkEntries.entryId      WHERE entries.id IS NOT NULL) \
                                   +(SELECT COUNT(entries.id) FROM hardlinkEntries  LEFT JOIN entries ON entries.id=hardlinkEntries.entryId  WHERE entries.id IS NOT NULL) \
                                   +(SELECT COUNT(entries.id) FROM specialEntries   LEFT JOIN entries ON entries.id=specialEntries.entryId   WHERE entries.id IS NOT NULL) \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // clear directory content size/count aggregated data
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE directoryEntries \
                            SET totalEntryCount      =0, \
                                totalEntrySize       =0, \
                                totalEntryCountNewest=0, \
                                totalEntrySizeNewest =0 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // update directory content size/count aggegated data: files
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;
                             uint64 fragmentSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId    = (uint64)atoll(values[0]);
                             name         = String_newCString(values[1]);
                             fragmentSize = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);
//if (String_equalsCString(name,"/home/torsten/tmp/blender/yofrankie/trunk/textures/level_nut/X.png"))
//fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);


                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
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
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.storageId, \
                                   entries.name, \
                                   fileEntries.fragmentSize \
                            FROM fileEntries \
                              LEFT JOIN entries ON entries.id=fileEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;
                             uint64 fragmentSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId    = (uint64)atoll(values[0]);
                             name         = String_newCString(values[1]);
                             fragmentSize = (uint64)atoll(values[2]);
//fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);

                             // update directory content count/size aggregates in all newest directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                                             totalEntrySizeNewest =totalEntrySizeNewest +%llu \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        fragmentSize,
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entriesNewest.storageId, \
                                   entriesNewest.name, \
                                   fileEntries.fragmentSize \
                            FROM fileEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=fileEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggegated data: directories
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCount=totalEntryCount+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.storageId, \
                                   entries.name \
                            FROM directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entriesNewest.storageId, \
                                   entriesNewest.name \
                            FROM directoryEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggegated data: links
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
      //fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCount=totalEntryCount+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.storageId, \
                                   entries.name \
                            FROM linkEntries \
                              LEFT JOIN entries ON entries.id=linkEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
      //fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entriesNewest.storageId, \
                                   entriesNewest.name \
                            FROM linkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggegated data: hardlinks
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;
                             uint64 fragmentSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId    = (uint64)atoll(values[0]);
                             name         = String_newCString(values[1]);
                             fragmentSize = (uint64)atoll(values[2]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
      //fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
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
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.storageId, \
                                   entries.name, \
                                   hardlinkEntries.fragmentSize \
                            FROM hardlinkEntries \
                              LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;
                             uint64 fragmentSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId    = (uint64)atoll(values[0]);
                             name         = String_newCString(values[1]);
                             fragmentSize = (uint64)atoll(values[2]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s fragmentSize=%llu\n",__FILE__,__LINE__,storageId,String_cString(name),fragmentSize);

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
      //fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                                             totalEntrySizeNewest =totalEntrySizeNewest +%llu \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        fragmentSize,
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entriesNewest.storageId, \
                                   entriesNewest.name, \
                                   hardlinkEntries.fragmentSize \
                            FROM hardlinkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=hardlinkEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // update directory content size/count aggegated data: special
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCount=totalEntryCount+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.storageId, \
                                   entries.name \
                            FROM specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;
                             String name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);
                             name      = String_newCString(values[1]);
      //fprintf(stderr,"%s, %d: storageId=%llu name=%s\n",__FILE__,__LINE__,storageId,String_cString(name));

                             // update directory content count/size aggregates in all directories
                             while (!String_isEmpty(File_getDirectoryName(name,name)))
                             {
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        "UPDATE directoryEntries \
                                                         SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                                         WHERE     storageId=%llu \
                                                               AND name=%'S \
                                                        ",
                                                        storageId,
                                                        name
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 if (verboseFlag) fprintf(stderr,"FAIL!\n");
                                 fprintf(stderr,"ERROR: create aggregates fail for entries: (error: %s)!\n",Error_getText(error));
                                 return error;
                               }
                             }

                             String_delete(name);

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT entriesNewest.storageId, \
                                   entriesNewest.name \
                            FROM specialEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  if (verboseFlag) printf("OK  \n");

  // calculate storage total count/size aggregates
  if (verboseFlag) { fprintf(stderr,"Create aggregates for storage..."); fflush(stderr); }

  // get total count
  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalCount = (ulong)atol(values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT (SELECT COUNT(id) FROM storage) \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  n = 0L;

  // update storage total count/size aggregates
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint64 storageId;

                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (uint64)atoll(values[0]);

                             // total count/size
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
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
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIu64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storage \
                                                       SET totalEntryCount=totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount, \
                                                           totalEntrySize =totalFileSize +totalImageSize +                                   totalHardlinkSize \
                                                       WHERE id=%llu \
                                                      ",
                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIu64": (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             // total count/size newest
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
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
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIu64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storage \
                                                       SET totalEntryCountNewest=totalFileCountNewest+totalImageCountNewest+totalDirectoryCountNewest+totalLinkCountNewest+totalHardlinkCountNewest+totalSpecialCountNewest, \
                                                           totalEntrySizeNewest =totalFileSizeNewest +totalImageSizeNewest +                                               totalHardlinkSizeNewest \
                                                       WHERE id=%"PRIu64" \
                                                      ",
                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIu64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
  if (verboseFlag) fprintf(stderr,"OK  \n");

  // end transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount,
                           "END TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
}

/***********************************************************************\
* Name   : cleanUpOrphanedEntries
* Purpose: purge orphaned entries (entries without storage)
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanUpOrphanedEntries(DatabaseHandle *databaseHandle)
{
  String storageName;
  ulong  n;

  if (verboseFlag) { fprintf(stderr,"Clean-up orphaned:\n"); }

  // initialize variables
  storageName = String_new();

  n = 0L;

  // clean-up entries without storage name
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount
                         "DELETE FROM fileEntries \
                            LEFT JOIN storage ON storage.id=fileEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount
                         "DELETE FROM imageEntries \
                            LEFT JOIN storage ON storage.id=imageEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount,
                         "DELETE FROM directoryEntries \
                            LEFT JOIN storage ON storage.id=directoryEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount,
                         "DELETE FROM linkEntries \
                            LEFT JOIN storage ON storage.id=linkEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount,
                         "DELETE FROM hardlinkEntries \
                            LEFT JOIN storage ON storage.id=hardlinkEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount,
                         "DELETE FROM specialEntries \
                            LEFT JOIN storage ON storage.id=specialEntries.storageId \
                          WHERE storage.name IS NULL OR storage.name=''; \
                         "
                        );

  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount,
                         "DELETE FROM storage \
                          WHERE name IS NULL OR name=''; \
                         "
                        );

  // clean-up *Entries without entry
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           databaseId = (int64)atoll(values[0]);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM fileEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT fileEntries.id \
                          FROM fileEntries \
                            LEFT JOIN entries ON entries.id=fileEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           databaseId = (int64)atoll(values[0]);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM imageEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT imageEntries.id \
                          FROM imageEntries \
                            LEFT JOIN entries ON entries.id=imageEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId = (int64)atoll(values[0]);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM directoryEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT directoryEntries.id \
                          FROM directoryEntries \
                            LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId = (int64)atoll(values[0]);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM linkEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT linkEntries.id \
                          FROM linkEntries \
                            LEFT JOIN entries ON entries.id=linkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId = (int64)atoll(values[0]);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM hardlinkEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT hardlinkEntries.id \
                          FROM hardlinkEntries \
                            LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
        (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId = (int64)atoll(values[0]);

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM specialEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT specialEntries.id \
                          FROM specialEntries \
                            LEFT JOIN entries ON entries.id=specialEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );

  fprintf(stdout,"Clean-up %lu orphaned entries\n",n);

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : cleanUpDuplicateIndizes
* Purpose: purge duplicate storage entries
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanUpDuplicateIndizes(DatabaseHandle *databaseHandle)
{
  ulong n;

  if (verboseFlag) { fprintf(stderr,"Clean-up duplicates:\n"); }

  // init variables

  // get storage entry
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64      databaseId;
                           const char *storageName;

                           assert(count == 2);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId  = (int64)atoll(values[0]);
                           storageName = values[1];

                           (void)Database_execute(databaseHandle,
                                                  CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                                  {
                                                    int64 duplicateDatabaseId;

                                                    assert(count == 1);
                                                    assert(values != NULL);
                                                    assert(values[0] != NULL);

                                                    UNUSED_VARIABLE(columns);
                                                    UNUSED_VARIABLE(count);
                                                    UNUSED_VARIABLE(userData);

                                                    duplicateDatabaseId = (int64)atoll(values[0]);

                                                    (void)Database_execute(databaseHandle,
                                                                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                           NULL,  // changedRowCount,
                                                                           "DELETE FROM storage \
                                                                            WHERE id=%lld \
                                                                           ",
                                                                           duplicateDatabaseId
                                                                          );

                                                    n++;

                                                    return ERROR_NONE;
                                                  },NULL),
                                                  NULL,  // changedRowCount
                                                  "SELECT id \
                                                   FROM storage \
                                                   WHERE id!=%lld AND name='%s' \
                                                  ",
                                                  databaseId,
                                                  storageName
                                                 );

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT id,name \
                          FROM storage \
                         "
                        );

#if 0
//              && Storage_equalNames(storageName,duplicateStorageName)
//          error = Index_deleteStorage(indexHandle,deleteStorageIndexId);
#endif
  fprintf(stdout,"Clean-up %lu duplicate entries\n",n);

  // free resources
}

/***********************************************************************\
* Name   : purgeDeletedStorages
* Purpose: purge deleted storages
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeDeletedStorages(DatabaseHandle *databaseHandle)
{
  ulong n;

  if (verboseFlag) { fprintf(stderr,"Purge deleted storages:\n"); }

  // init variables

  // get storage entry
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                         {
                           int64 databaseId;

                           assert(count == 1);
                           assert(values != NULL);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(columns);
                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(userData);

                           databaseId  = (int64)atoll(values[0]);

                           if (verboseFlag) { fprintf(stderr,"  %ld...",databaseId); }
                           (void)Database_execute(databaseHandle,
                                                  CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                                  {
                                                    int64 purgeDatabaseId;

                                                    assert(count == 1);
                                                    assert(values != NULL);
                                                    assert(values[0] != NULL);

                                                    UNUSED_VARIABLE(columns);
                                                    UNUSED_VARIABLE(count);
                                                    UNUSED_VARIABLE(userData);

                                                    purgeDatabaseId = (int64)atoll(values[0]);

                                                    (void)Database_execute(databaseHandle,
                                                                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                           NULL,  // changedRowCount,
                                                                           "DELETE FROM storage \
                                                                            WHERE id=%lld \
                                                                           ",
                                                                           purgeDatabaseId
                                                                          );

                                                    n++;

                                                    return ERROR_NONE;
                                                  },NULL),
                                                  NULL,  // changedRowCount
                                                  "SELECT id \
                                                   FROM storage \
                                                   WHERE id!=%lld \
                                                  ",
                                                  databaseId
                                                 );

                           if (verboseFlag) { fprintf(stderr,"  OK\n"); }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT id \
                          FROM storage \
                          WHERE deletedFlag=1 \
                         "
                        );

  // free resources
}

/***********************************************************************\
* Name   : vacuum
* Purpose: vacuum database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void vacuum(DatabaseHandle *databaseHandle)
{
  Errors error;

  if (verboseFlag) { fprintf(stderr,"Vacuum..."); fflush(stderr); }

  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "VACUUM"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: vacuum fail: %s!\n",Error_getText(error));
    exit(1);
  }

  if (verboseFlag) fprintf(stderr,"OK  \n");
}

/***********************************************************************\
* Name   : getColumnsWidth
* Purpose: get column width
* Input  : columns - column names
*          values  - values
*          count   - number of values
* Output : width - widths
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getColumnsWidth(size_t widths[], const char *columns[], const char *values[], uint count)
{
  uint i;

  for (i = 0; i < count; i++)
  {
    widths[i] = 0;
    if ((columns[i] != NULL) && (strlen(columns[i]) > widths[i])) widths[i] = strlen(columns[i]);
    if ((values [i] != NULL) && (strlen(values [i]) > widths[i])) widths[i] = strlen(values [i]);
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
* Input  : columns  - column names
*          values   - values
*          count    - number of values
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors printRow(const char *columns[], const char *values[], uint count, void *userData)
{
  uint   i;
  size_t *widths;

  assert(columns != NULL);
  assert(values != NULL);

  UNUSED_VARIABLE(userData);

  widths = (size_t*)malloc(count*sizeof(size_t));
  assert(widths != NULL);
  getColumnsWidth(widths,columns,values,count);

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

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : printInfo
* Purpose: print index info
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printInfo(DatabaseHandle *databaseHandle)
{
  Errors error;

  // show meta data
  printf("Meta:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[],uint count,void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  %-16s: %s\n",values[0],values[1]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT name,value FROM meta"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get meta data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // show number of storages
  printf("Storages:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  OK              : %s\n",values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM storage WHERE state=%d",
                           INDEX_CONST_STATE_OK
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Update requested: %s\n",values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM storage WHERE state=%d",INDEX_CONST_STATE_UPDATE_REQUESTED
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Error           : %s\n",values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM storage WHERE state=%d",INDEX_CONST_STATE_ERROR
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Deleted         : %s\n",values[0]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) FROM storage WHERE deletedFlag=1"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // show number of entries, newest entries
  printf("Entries:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Total           : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalEntryCount),TOTAL(totalEntrySize) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Files           : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalFileCount),TOTAL(totalFileSize) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Images          : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalImageCount),TOTAL(totalImageSize) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Directories     : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalDirectoryCount) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Links           : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalLinkCount) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Hardlinks       : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalHardlinkCount),TOTAL(totalHardlinkSize) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Special         : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalSpecialCount) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // show number of newest entries
  printf("Newest entries:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Total           : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalEntryCountNewest),TOTAL(totalEntrySizeNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Files           : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalFileCountNewest),TOTAL(totalFileSizeNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Images          : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalImageCountNewest),TOTAL(totalImageSizeNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Directories     : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalDirectoryCountNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Links           : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalLinkCountNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Hardlinks       : %lu, %llubytes\n",atol(values[0]),atoll(values[1]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalHardlinkCountNewest),TOTAL(totalHardlinkSizeNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Special         : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalSpecialCountNewest) FROM storage"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
}

/***********************************************************************\
* Name   : inputAvailable
* Purpose: check if input available on stdin
* Input  : -
* Output : -
* Return : TRUE iff input available
* Notes  : -
\***********************************************************************/

LOCAL bool inputAvailable(void)
{
  struct timeval tv;
  fd_set         fdSet;

  tv.tv_sec  = 0;
  tv.tv_usec = 0;

  FD_ZERO(&fdSet);
  FD_SET(STDIN_FILENO,&fdSet);
  select(STDIN_FILENO+1,&fdSet,NULL,NULL,&tv);

  return FD_ISSET(STDIN_FILENO,&fdSet);
}
/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : initAll
* Purpose: initialize
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initAll(void)
{
  Errors error;

  // initialize modules
  error = Thread_initAll();
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: %s\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: %s\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
}

/***********************************************************************\
* Name   : doneAll
* Purpose: deinitialize
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneAll(void)
{
  Database_doneAll();
  Thread_doneAll();
}

int main(int argc, const char *argv[])
{
  const uint MAX_LINE_LENGTH = 8192;

  uint            i,n;
  const char      *databaseFileName;
  String          path;
  String          commands;
  char            line[MAX_LINE_LENGTH];
  Errors          error;
  DatabaseHandle  databaseHandle;
  const char      *l;

  initAll();

  // init variables
  databaseFileName = NULL;
  commands         = String_new();

  i = 1;
  n = 0;
  while (i < (uint)argc)
  {
    if      (stringEquals(argv[i],"--info"))
    {
      infoFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--check"))
    {
      checkFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create"))
    {
      createFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-triggers"))
    {
      createTriggersFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-newest"))
    {
      createNewestFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-indizes"))
    {
      createIndizesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-aggregates"))
    {
      createAggregatesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--clean"))
    {
      cleanFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--purge"))
    {
      purgeDeletedFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--vacuum"))
    {
      vacuumFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-s") || stringEquals(argv[i],"--storages"))
    {
      showStoragesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-e") || stringEquals(argv[i],"--entries"))
    {
      showEntriesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--entries-newest"))
    {
      showEntriesNewestFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-n") || stringEquals(argv[i],"--names"))
    {
      showNamesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-H") || stringEquals(argv[i],"--header"))
    {
      showHeaderFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-f") || stringEquals(argv[i],"--no-foreign-keys"))
    {
      foreignKeysFlag = FALSE;
      i++;
    }
    else if (stringEquals(argv[i],"--force"))
    {
      forceFlag = FALSE;
      i++;
    }
    else if (stringEquals(argv[i],"--pipe"))
    {
      pipeFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--tmp-directory"))
    {
      if ((i+1) >= (uint)argc)
      {
        fprintf(stderr,"ERROR: expected path name for option --tmp-directory!\n");
        String_delete(commands);
        exit(EXITCODE_INVALID_ARGUMENT);
      }
      tmpDirectory = argv[i+1];
      i += 2;
    }
    else if (stringEquals(argv[i],"-v") || stringEquals(argv[i],"--verbose"))
    {
      verboseFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-h") || stringEquals(argv[i],"--help"))
    {
      printUsage(argv[0]);
      exit(EXITCODE_OK);
    }
    else if (stringEquals(argv[i],"--"))
    {
      i++;
      break;
    }
    else if (stringStartsWith(argv[i],"-"))
    {
      fprintf(stderr,"ERROR: unknown option '%s'!\n",argv[i]);
      String_delete(commands);
      exit(EXITCODE_INVALID_ARGUMENT);
    }
    else
    {
      switch (n)
      {
        case 0:
          databaseFileName = argv[i];
          n++;
          i++;
          break;
        default:
          String_appendCString(commands,argv[i]);
          jobUUID = argv[i];
          i++;
          break;
      }
    }
  }
  while (i < (uint)argc)
  {
    switch (n)
    {
      case 0:
        databaseFileName = argv[i];
        n++;
        i++;
        break;
      default:
        String_appendCString(commands,argv[i]);
        jobUUID = argv[i];
        i++;
        break;
    }
  }

  // check arguments
  if (databaseFileName == NULL)
  {
    fprintf(stderr,"ERROR: no database file name given!\n");
    String_delete(commands);
    exit(EXITCODE_INVALID_ARGUMENT);
  }

  if (String_equalsCString(commands,"-"))
  {
    // get commands from stdin
    String_clear(commands);
    while (fgets(line,sizeof(line),stdin) != NULL)
    {
      String_appendCString(commands,line);
    }
  }

  if (createFlag)
  {
    // create database
    error = createDatabase(&databaseHandle,databaseFileName);
  }
  else
  {
    // open database
    error = openDatabase(&databaseHandle,databaseFileName);
  }
  if (error != ERROR_NONE)
  {
    String_delete(commands);
    exit(EXITCODE_FAIL);
  }

  // set temporary directory
  if (tmpDirectory != NULL)
  {
    error = Database_setTmpDirectory(&databaseHandle,tmpDirectory);
  }
  else
  {
    path = File_getDirectoryNameCString(String_new(),databaseFileName);
    if (String_isEmpty(path)) File_getCurrentDirectory(path);
    error = Database_setTmpDirectory(&databaseHandle,String_cString(path));
    String_delete(path);
  }
  if (error != ERROR_NONE)
  {
    closeDatabase(&databaseHandle);
    String_delete(commands);
    exit(EXITCODE_FAIL);
  }

  // output info
  if (   infoFlag
      || (   !checkFlag
          && !createTriggersFlag
          && !createNewestFlag
          && !createIndizesFlag
          && !createAggregatesFlag
          && !cleanFlag
          && !purgeDeletedFlag
          && !vacuumFlag
          && !showStoragesFlag
          && !showEntriesFlag
          && !showEntriesNewestFlag
          && String_isEmpty(commands)
          && !pipeFlag
          && !inputAvailable())
     )
  {
    printInfo(&databaseHandle);
  }

  if (checkFlag)
  {
    checkDatabase(&databaseHandle);
  }

  // recreate triggeres
  if (createTriggersFlag)
  {
    createTriggers(&databaseHandle);
  }

  // recreate newest data
  if (createNewestFlag)
  {
    createNewest(&databaseHandle);
  }

  // recreate indizes
  if (createIndizesFlag)
  {
    createIndizes(&databaseHandle);
  }

  // calculate aggregate data
  if (createAggregatesFlag)
  {
    createAggregates(&databaseHandle);
  }

  // clean
  if (cleanFlag)
  {
    cleanUpOrphanedEntries(&databaseHandle);
    cleanUpDuplicateIndizes(&databaseHandle);
  }

  // purge deleted storages
  if (purgeDeletedFlag)
  {
    purgeDeletedStorages(&databaseHandle);
  }

  // vacuum
  if (vacuumFlag)
  {
    vacuum(&databaseHandle);
  }

  if (showStoragesFlag)
  {
    uint64 maxIdLength,maxStorageNameLength;
    char   format[256];

    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       assert(count == 2);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = (uint)1+log10(atof(values[0]));
                       maxStorageNameLength = (uint)atoi(values[1]);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT MAX(storage.id),MAX(LENGTH(storage.name)) FROM storage \
                        LEFT join entities on storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );

    stringFormat(format,sizeof(format),"%%-%ds %%-%ds %%64s %%-10s\n",maxIdLength,maxStorageNameLength);
    UNUSED_VARIABLE(maxStorageNameLength);
    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       uint       archiveType;
                       const char *s;

                       assert(count == 4);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       archiveType = atoi(values[3]);
                       s = "unknown";
                       for (uint i = CHUNK_CONST_ARCHIVE_TYPE_NONE; i <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS; i++)
                       {
                         if (i == archiveType) s = ARCHIVE_TYPES[i];
                       }

                       printf(format,values[0],values[1],values[2],s);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT storage.id,storage.name,entities.jobUUID,entities.type FROM storage \
                        LEFT join entities on storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                      ORDER BY storage.name ASC \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );
  }

  if (showEntriesFlag)
  {
    uint64 maxIdLength,maxEntryNameLength,maxStorageNameLength;
    char   format[256];

    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       assert(count == 3);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = (uint)1+log10(atof(values[0]));
                       maxEntryNameLength   = (uint)atoi(values[1]);
                       maxStorageNameLength = (uint)atoi(values[2]);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT MAX(entries.id),MAX(LENGTH(entries.name)),MAX(LENGTH(storage.name)) FROM entries \
                        LEFT join storage ON entries.storageId=storage.id \
                        LEFT join entities ON storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );

    stringFormat(format,sizeof(format),"%%%ds %%-%ds %%-s\n",maxIdLength,maxEntryNameLength);
    UNUSED_VARIABLE(maxStorageNameLength);
    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       assert(count == 3);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       printf(format,values[0],values[1],values[2]);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT entries.id,entries.name,storage.name FROM entries \
                        LEFT join storage ON entries.storageId=storage.id \
                        LEFT join entities ON storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                      ORDER BY entries.name ASC \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );
  }

  if (showEntriesNewestFlag)
  {
    uint64 maxIdLength,maxEntryNameLength,maxStorageNameLength;
    char   format[256];

    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       assert(count == 3);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = (uint)1+log10(atof(values[0]));
                       maxEntryNameLength   = (uint)atoi(values[1]);
                       maxStorageNameLength = (uint)atoi(values[2]);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT MAX(entriesNewest.id),MAX(LENGTH(entriesNewest.name)),MAX(LENGTH(storage.name)) FROM entriesNewest \
                        LEFT join storage ON entriesNewest.storageId=storage.id \
                        LEFT join entities ON storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );

    stringFormat(format,sizeof(format),"%%%ds %%-%ds %%-s\n",maxIdLength,maxEntryNameLength);
    UNUSED_VARIABLE(maxStorageNameLength);
    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                     {
                       assert(count == 3);
                       assert(values != NULL);
                       assert(values[0] != NULL);
                       assert(values[1] != NULL);

                       UNUSED_VARIABLE(columns);
                       UNUSED_VARIABLE(count);
                       UNUSED_VARIABLE(userData);

                       printf(format,values[0],values[1],values[2]);

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     "SELECT entriesNewest.id,entriesNewest.name,storage.name FROM entriesNewest \
                        LEFT join storage ON entriesNewest.storageId=storage.id \
                        LEFT join entities ON storage.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                      ORDER BY entriesNewest.name ASC \
                     ",
                     jobUUID == NULL,
                     jobUUID
                    );
  }

  if (   !showStoragesFlag
      && !showEntriesFlag
      && !showEntriesNewestFlag
     )
  {
    // execute command
    if (!String_isEmpty(commands))
    {
      if (pipeFlag)
      {
        // pipe from stdin

        // start transaction
        error = Database_execute(&databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "BEGIN TRANSACTION"
                                );
        if (error != ERROR_NONE)
        {
          printf("FAIL\n");
          fprintf(stderr,"ERROR: start transaction fail: %s!\n",Error_getText(error));
          String_delete(commands);
          exit(EXITCODE_FAIL);
        }

  #if 0
        // prepare SQL statements
        statementHandleCount = 0;
        command = String_cString(commands);
        while (!stringIsEmpty(command))
        {
          if (statementHandleCount > SIZE_OF_ARRAY(statementHandles))
          {
            fprintf(stderr,"ERROR: too many SQL commands (limit %lu)!\n",SIZE_OF_ARRAY(statementHandles));
            String_delete(commands);
            exit(EXITCODE_FAIL);
          }

          sqliteResult = sqlite3_prepare_v2(databaseHandle,
                                            command,
                                            -1,
                                            &statementHandles[statementHandleCount],
                                            &nextCommand
                                           );
          if (verboseFlag) fprintf(stderr,"Result: %d\n",sqliteResult);
          if (sqliteResult != SQLITE_OK)
          {
            (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
  //TODO          fprintf(stderr,"ERROR: SQL command #%u: '%s' fail: %s!\n",i+1,command,sqlite3_errmsg(databaseHandle));
            String_delete(commands);
            exit(EXITCODE_FAIL);
          }
          statementHandleCount++;
          command = stringTrimBegin(nextCommand);
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
                (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
                fprintf(stderr,"ERROR: Invalid data '%s'!\n",String_cString(string));
                String_delete(commands);
                exit(EXITCODE_FAIL);
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
            (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
  //TODO          fprintf(stderr,"ERROR: SQL command #%u: '%s' fail: %s!\n",i+1,String_cString(commands),sqlite3_errmsg(databaseHandle));
            String_delete(commands);
            exit(EXITCODE_FAIL);
          }
        }
        String_delete(s);

        // free resources
        for (i = 0; i < statementHandleCount; i++)
        {
          sqlite3_finalize(statementHandles[i]);
        }
  #endif

        // end transaction
        error = Database_execute(&databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "END TRANSACTION"
                                );
        if (error != ERROR_NONE)
        {
          fprintf(stderr,"ERROR: end transaction fail: %s!\n",Error_getText(error));
          String_delete(commands);
          exit(EXITCODE_FAIL);
        }
      }
      else
      {
        // single command execution
        error = Database_execute(&databaseHandle,
                                 CALLBACK_(printRow,NULL),
                                 NULL,  // changedRowCount
                                 String_cString(commands)
                                );
        if (error != ERROR_NONE)
        {
          fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",String_cString(commands),Error_getText(error));
          String_delete(commands);
          exit(EXITCODE_FAIL);
        }
      }
    }
  }

  while (inputAvailable() && (fgets(line,sizeof(line),stdin) != NULL))
  {
    l = stringTrim(line);
    error = Database_execute(&databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             l
                            );
    if (error == ERROR_NONE)
    {
      if (verboseFlag) fprintf(stderr,"Result: %s\n",Error_getText(error));
    }
    else
    {
      fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",l,Error_getText(error));
      String_delete(commands);
      exit(EXITCODE_FAIL);
    }
  }

  // close database
  closeDatabase(&databaseHandle);

  // free resources
  String_delete(commands);

  doneAll();

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
