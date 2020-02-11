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
#include "common/misc.h"

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
LOCAL bool       infoFlag                             = FALSE;  // output index database info
LOCAL bool       infoUUIDsFlag                        = FALSE;  // output index database UUIDs info
LOCAL bool       infoEntitiesFlag                     = FALSE;  // output index database entities info
LOCAL bool       infoStoragesFlag                     = FALSE;  // output index database storages info
LOCAL bool       infoLostStoragesFlag                 = FALSE;  // output index database lost storages info
LOCAL bool       infoEntriesFlag                      = FALSE;  // output index database entries info
LOCAL bool       infoLostEntriesFlag                  = FALSE;  // output index database lost entries info
LOCAL bool       checkFlag                            = FALSE;  // check database
LOCAL bool       createFlag                           = FALSE;  // create new index database
LOCAL bool       createIndizesFlag                    = FALSE;  // re-create indizes
LOCAL bool       createTriggersFlag                   = FALSE;  // re-create triggers
LOCAL bool       createNewestFlag                     = FALSE;  // re-create newest data
LOCAL bool       createAggregatesDirectoryContentFlag = FALSE;  // re-create aggregates entities data
LOCAL bool       createAggregatesEntitiesFlag         = FALSE;  // re-create aggregates entities data
LOCAL bool       createAggregatesStoragesFlag         = FALSE;  // re-create aggregates storages data
LOCAL bool       cleanFlag                            = FALSE;  // execute clean
LOCAL bool       purgeDeletedFlag                     = FALSE;  // execute purge deleted storages
LOCAL bool       vacuumFlag                           = FALSE;  // execute vacuum
LOCAL bool       showStoragesFlag                     = FALSE;  // show storage of job
LOCAL bool       showEntriesFlag                      = FALSE;  // show entries of job
LOCAL bool       showEntriesNewestFlag                = FALSE;  // show newest entries of job
LOCAL bool       showNamesFlag                        = FALSE;
LOCAL bool       showHeaderFlag                       = FALSE;
LOCAL bool       headerPrintedFlag                    = FALSE;
LOCAL bool       foreignKeysFlag                      = TRUE;
LOCAL bool       forceFlag                            = FALSE;
LOCAL bool       pipeFlag                             = FALSE;
LOCAL const char *tmpDirectory                        = NULL;
LOCAL bool       verboseFlag                          = FALSE;
LOCAL bool       timeFlag                             = FALSE;
LOCAL bool       explainQueryPlanFlag                 = FALSE;
LOCAL const char *jobUUID                             = NULL;

bool xxxFlag = FALSE;

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
  printf("Options:  --info                                - output index database infos\n");
  printf("          --info-entities[=id,...]              - output index database entities infos\n");
  printf("          --info-storages[=id,...]              - output index database storages infos\n");
  printf("          --info-lost-storages[=id,...]         - output index database storages infos without an entity\n");
  printf("          --info-entries[=id,...|name]          - output index database entries infos\n");
  printf("          --info-lost-entries[=id,...]          - output index database entries infos without an entity\n");
  printf("          --create                              - create new index database\n");
  printf("          --create-triggers                     - re-create triggers\n");
  printf("          --create-newest                       - re-create newest data\n");
  printf("          --create-indizes                      - re-create indizes\n");
  printf("          --create-aggregates                   - re-create aggregated data\n");
  printf("          --create-aggregates-directory-content - re-create aggregated data\n");
  printf("          --create-aggregates-entities          - re-create aggregated data\n");
  printf("          --create-aggregates-storages          - re-create aggregated data\n");
  printf("          --check                               - check index database integrity\n");
  printf("          --clean                               - clean index database\n");
  printf("          --vacuum                              - collect and free unused file space\n");
  printf("          -s|--storages [<uuid>]                - print storages\n");
  printf("          -e|--entries [<uuid>]                 - print entries\n");
  printf("          --entries-newest [<uuid>]             - print newest entries\n");
  printf("          -n|--names                            - print named values\n");
  printf("          -H|--header                           - print headers\n");
  printf("          -f|--no-foreign-keys                  - disable foreign key constraints\n");
  printf("          --force                               - force operation\n");
  printf("          --pipe                                - read data from stdin and pipe into database\n");
  printf("          --tmp-directory                       - temporary files directory\n");
  printf("          -v|--verbose                          - verbose output\n");
  printf("          -t|--time                             - print execution time\n");
  printf("          -x|--explain-query                    - explain SQL queries\n");
  printf("          -h|--help                             - print this help\n");
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

/***********************************************************************\
* Name   : getByteSize
* Purpose: get bytes size [byte, K, M, G, T, P]
* Input  : n - value
* Output : -
* Return : byte size
* Notes  : -
\***********************************************************************/

LOCAL double getByteSize(uint64 n)
{
  double result;

  if      (n >= (1024LL*1024LL*1024LL*1024LL*1024LL)) result = (double)n/(1024LL*1024LL*1024LL*1024LL*1024LL);
  else if (n >= (       1024LL*1024LL*1024LL*1024LL)) result = (double)n/(1024LL*1024LL*1024LL*1024LL);
  else if (n >= (              1024LL*1024LL*1024LL)) result = (double)n/(1024LL*1024LL*1024LL);
  else if (n >= (                     1024LL*1024LL)) result = (double)n/(1024LL*1024LL);
  else if (n >= (                            1024LL)) result = (double)n/(1024LL);
  else                                                result = (double)n;

  return result;
}

/***********************************************************************\
* Name   : getByteUnitShort
* Purpose: get byte short unit [byte, K, M, G, T, P]
* Input  : n - value
* Output : -
* Return : byte short unit
* Notes  : -
\***********************************************************************/

LOCAL const char *getByteUnitShort(uint64 n)
{
  const char *shortUnit;

  if      (n >= 1024LL*1024LL*1024LL*1024LL*1024LL) shortUnit = "PB";
  else if (n >=        1024LL*1024LL*1024LL*1024LL) shortUnit = "TB";
  else if (n >=               1024LL*1024LL*1024LL) shortUnit = "GB";
  else if (n >=                      1024LL*1024LL) shortUnit = "MB";
  else if (n >=                             1024LL) shortUnit = "KB";
  else                                              shortUnit = "bytes";

  return shortUnit;
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
    fprintf(stderr,"ERROR: database file already exists! Use --force if needed.\n");
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
* Name   : getFTSString
* Purpose: get full-text-search filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedTextFlag,addedPatternFlag;
  size_t          iteratorVariable;
  Codepoint       codepoint;

  String_clear(string);

  if (!String_isEmpty(patternText))
  {
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      addedTextFlag    = FALSE;
      addedPatternFlag = FALSE;
      STRING_CHAR_ITERATE_UTF8(token,iteratorVariable,codepoint)
      {
        if (isalnum(codepoint) || (codepoint >= 128))
        {
          if (addedPatternFlag)
          {
            String_appendChar(string,' ');
            addedPatternFlag = FALSE;
          }
          String_appendCharUTF8(string,codepoint);
          addedTextFlag = TRUE;
        }
        else
        {
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(string,'*');
            addedTextFlag    = FALSE;
            addedPatternFlag = TRUE;
          }
        }
      }
      if (addedTextFlag && !addedPatternFlag)
      {
        String_appendChar(string,'*');
      }
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
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
                           "DELETE FROM FTS_storages"
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
                           "INSERT INTO FTS_storages SELECT id,name FROM storages"
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

  // delete all newest entries
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
                             DatabaseId entryId;
                             DatabaseId entityId;
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

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             entryId         = (DatabaseId)atoll(values[0]);
                             entityId        = (DatabaseId)atoll(values[1]);
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
error = Database_getString(databaseHandle,s,"entries","entities.jobUUID","left join storages on storages.id=entries.storageId left join entities on entities.id=storages.entityId where entries.id=%d",entryId);
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
                                                            entityId, \
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
                                                        entityId,
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
                                   entityId, \
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
* Name   : createAggregatesDirectoryContent
* Purpose: create aggregates diretory content data
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesDirectoryContent(DatabaseHandle *databaseHandle, const Array databaseIdArray)
{
  Errors error;
  ulong  totalCount;
  ulong  n;

  UNUSED_VARIABLE(databaseIdArray);

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
                           "SELECT  2*(SELECT COUNT(entries.id) FROM fileEntries      LEFT JOIN entries ON entries.id=fileEntries.entryId      WHERE entries.id  IS NOT NULL) \
                                   +2*(SELECT COUNT(entries.id) FROM directoryEntries LEFT JOIN entries ON entries.id=directoryEntries.entryId WHERE entries.id  IS NOT NULL) \
                                   +2*(SELECT COUNT(entries.id) FROM linkEntries      LEFT JOIN entries ON entries.id=linkEntries.entryId      WHERE entries.id  IS NOT NULL) \
                                   +2*(SELECT COUNT(entries.id) FROM hardlinkEntries  LEFT JOIN entries ON entries.id=hardlinkEntries.entryId  WHERE entries.id  IS NOT NULL) \
                                   +2*(SELECT COUNT(entries.id) FROM specialEntries   LEFT JOIN entries ON entries.id=specialEntries.entryId   WHERE entries.id  IS NOT NULL) \
                                   +(SELECT COUNT(id)           FROM entities                                                                  WHERE entities.id IS NOT NULL) \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  n = 0L;

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

  // update directory content size/count aggegrated data: files
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;
                             String     name;
                             uint64     totalSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
                             name      = String_newCString(values[1]);
                             totalSize = (uint64)atoll(values[2]);
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
                                                        totalSize,
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
                           "SELECT entryFragments.storageId, \
                                   entries.name, \
                                   TOTAL(entryFragments.size) \
                            FROM fileEntries \
                              LEFT JOIN entries        ON entries.id=fileEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.id IS NOT NULL \
                                  AND entryFragments.storageId IS NOT NULL \
                            GROUP BY entries.name \
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
                             DatabaseId storageId;
                             String     name;
                             uint64     totalSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
                             name      = String_newCString(values[1]);
                             totalSize = (uint64)atoll(values[2]);
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
                                                        totalSize,
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
                           "SELECT entryFragments.storageId, \
                                   entriesNewest.name, \
                                   TOTAL(entryFragments.size) \
                            FROM fileEntries \
                              LEFT JOIN entriesNewest  ON entriesNewest.entryId=fileEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.id IS NOT NULL \
                                  AND entryFragments.storageId IS NOT NULL \
                            GROUP BY entriesNewest.name \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggregated data: directories
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                           "SELECT directoryEntries.storageId, \
                                   COUNT(entries.id) \
                            FROM directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                            GROUP BY entries.name \
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
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                           "SELECT directoryEntries.storageId, \
                                   COUNT(entriesNewest.id) \
                            FROM directoryEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                            GROUP BY entriesNewest.name \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggregated data: links
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                           "SELECT linkEntries.storageId, \
                                   COUNT(entries.id) \
                            FROM linkEntries \
                              LEFT JOIN entries ON entries.id=linkEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                            GROUP BY entries.name \
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
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                           "SELECT linkEntries.storageId, \
                                   COUNT(entriesNewest.id) \
                            FROM linkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                            GROUP BY entriesNewest.name \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // update directory content size/count aggregated data: hardlinks
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;
                             String     name;
                             uint64     totalSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
                             name      = String_newCString(values[1]);
                             totalSize = (uint64)atoll(values[2]);
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
                                                        totalSize,
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
                           "SELECT entryFragments.storageId, \
                                   COUNT(entries.id), \
                                   TOTAL(entryFragments.size) \
                            FROM hardlinkEntries \
                              LEFT JOIN entries        ON entries.id=hardlinkEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            WHERE     entries.id IS NOT NULL \
                                  AND entryFragments.storageId IS NOT NULL \
                            GROUP BY entries.name \
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
                             DatabaseId storageId;
                             String     name;
                             uint64     totalSize;

                             assert(count == 3);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);
                             assert(values[2] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
                             name      = String_newCString(values[1]);
                             totalSize = (uint64)atoll(values[2]);
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
                                                        totalSize,
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
                           "SELECT entryFragments.storageId, \
                                   COUNT(entriesNewest.id), \
                                   TOTAL(entryFragments.size) \
                            FROM hardlinkEntries \
                              LEFT JOIN entriesNewest  ON entriesNewest.entryId=hardlinkEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                            WHERE     entriesNewest.id IS NOT NULL \
                                  AND entryFragments.storageId IS NOT NULL \
                            GROUP BY entriesNewest.name \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // update directory content size/count aggregated data: special
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                           "SELECT specialEntries.storageId, \
                                   COUNT(entries.id) \
                            FROM specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                            WHERE entries.id IS NOT NULL \
                            GROUP BY entries.name \
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
                             DatabaseId storageId;
                             String     name;

                             assert(count == 2);
                             assert(values != NULL);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);
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
                                                         WHERE     storageId=%lld \
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
                           "SELECT specialEntries.storageId, \
                                   COUNT(entriesNewest.id) \
                            FROM specialEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                            WHERE entriesNewest.id IS NOT NULL \
                            GROUP BY entriesNewest.name \
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
* Name   : createAggregatesEntities
* Purpose: create aggregates entities data
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesEntities(DatabaseHandle *databaseHandle, const Array entityIdArray)
{
  String     entityIdsString;
  ulong      i;
  DatabaseId entityId;
  Errors     error;
  ulong      totalCount;
  ulong      n;

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIdArray,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

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

  // calculate storage total count/size aggregates
  if (verboseFlag) { fprintf(stderr,"Create aggregates for entities..."); fflush(stderr); }

  // get entities total count
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
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ",
                           String_isEmpty(entityIdsString) ? 1 : 0,
                           entityIdsString
                          );
  if (error != ERROR_NONE)
  {
    String_delete(entityIdsString);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
  n = 0L;

  // update entities total count/size aggregates
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId entityId;

                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             entityId = (DatabaseId)atoll(values[0]);

                             // total count/size
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entities \
                                                       SET totalFileCount     =(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalImageCount    =(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalDirectoryCount=(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalLinkCount     =(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalHardlinkCount =(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalSpecialCount  =(SELECT COUNT(entries.id) \
                                                                                FROM entries \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           \
                                                           totalFileSize      =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalImageSize     =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ), \
                                                           totalHardlinkSize  =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entries.entityId=%lld \
                                                                               ) \
                                                       WHERE id=%lld \
                                                      ",
                                                      INDEX_CONST_TYPE_FILE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_IMAGE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_DIRECTORY,
                                                      entityId,
                                                      INDEX_CONST_TYPE_LINK,
                                                      entityId,
                                                      INDEX_CONST_TYPE_HARDLINK,
                                                      entityId,
                                                      INDEX_CONST_TYPE_SPECIAL,
                                                      entityId,

                                                      INDEX_CONST_TYPE_FILE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_IMAGE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_HARDLINK,
                                                      entityId,

                                                      entityId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for entity #%"PRIi64" (error: %s)!\n",entityId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entities \
                                                       SET totalEntryCount= totalFileCount \
                                                                           +totalImageCount \
                                                                           +totalDirectoryCount \
                                                                           +totalLinkCount\
                                                                           +totalHardlinkCount \
                                                                           +totalSpecialCount, \
                                                           totalEntrySize = totalFileSize \
                                                                           +totalImageSize \
                                                                           +totalHardlinkSize \
                                                       WHERE id=%llu \
                                                      ",
                                                      entityId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for entity #%"PRIi64": (error: %s)!\n",entityId,Error_getText(error));
                               return error;
                             }

                             // total count/size newest
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entities \
                                                       SET totalFileCountNewest     =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalImageCountNewest    =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalDirectoryCountNewest=(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalLinkCountNewest     =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalHardlinkCountNewest =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalSpecialCountNewest  =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           \
                                                           totalFileSizeNewest      =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalImageSizeNewest     =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ), \
                                                           totalHardlinkSizeNewest  =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                     ) \
                                                       WHERE id=%lld \
                                                      ",
                                                      INDEX_CONST_TYPE_FILE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_IMAGE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_DIRECTORY,
                                                      entityId,
                                                      INDEX_CONST_TYPE_LINK,
                                                      entityId,
                                                      INDEX_CONST_TYPE_HARDLINK,
                                                      entityId,
                                                      INDEX_CONST_TYPE_SPECIAL,
                                                      entityId,

                                                      INDEX_CONST_TYPE_FILE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_IMAGE,
                                                      entityId,
                                                      INDEX_CONST_TYPE_HARDLINK,
                                                      entityId,

                                                      entityId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for entity #%"PRIi64" (error: %s)!\n",entityId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE entities \
                                                       SET totalEntryCountNewest= totalFileCountNewest \
                                                                                 +totalImageCountNewest \
                                                                                 +totalDirectoryCountNewest \
                                                                                 +totalLinkCountNewest \
                                                                                 +totalHardlinkCountNewest \
                                                                                 +totalSpecialCountNewest, \
                                                           totalEntrySizeNewest = totalFileSizeNewest \
                                                                                 +totalImageSizeNewest \
                                                                                 +totalHardlinkSizeNewest \
                                                       WHERE id=%lld \
                                                      ",
                                                      entityId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for entity #%"PRIi64" (error: %s)!\n",entityId,Error_getText(error));
                               return error;
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id \
                            FROM entities \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ", \
                           String_isEmpty(entityIdsString) ? 1 : 0,
                           entityIdsString
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    String_delete(entityIdsString);
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
    String_delete(entityIdsString);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(entityIdsString);
}

/***********************************************************************\
* Name   : createAggregatesStorages
* Purpose: create aggregates storages data
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesStorages(DatabaseHandle *databaseHandle, const Array storageIdArray)
{
  String     storageIdsString;
  ulong      i;
  DatabaseId storageId;
  Errors     error;
  ulong      totalCount;
  ulong      n;

  storageIdsString = String_new();
  ARRAY_ITERATE(&storageIdArray,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%lld",storageId);
  }

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

  // calculate storage total count/size aggregates
  if (verboseFlag) { fprintf(stderr,"Create aggregates for storages..."); fflush(stderr); }

  // get storage total count
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
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ",
                           String_isEmpty(storageIdsString) ? 1 : 0,
                           storageIdsString
                          );
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }
  n = 0L;

  // update storage total count/size aggregates
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId storageId;

                             assert(count == 1);
                             assert(values != NULL);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             storageId = (DatabaseId)atoll(values[0]);

                             // total count/size
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storages \
                                                       SET totalFileCount     =(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ), \
                                                           totalImageCount    =(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ), \
                                                           totalDirectoryCount=(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                                                                WHERE entries.type=%d AND directoryEntries.storageId=%lld \
                                                                               ), \
                                                           totalLinkCount     =(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN linkEntries      ON linkEntries.entryId     =entries.id \
                                                                                WHERE entries.type=%d AND linkEntries.storageId=%lld \
                                                                               ), \
                                                           totalHardlinkCount =(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ), \
                                                           totalSpecialCount  =(SELECT COUNT(DISTINCT entries.id) \
                                                                                FROM entries \
                                                                                  LEFT JOIN specialEntries   ON specialEntries.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND specialEntries.storageId=%lld \
                                                                               ), \
                                                           \
                                                           totalFileSize      =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ), \
                                                           totalImageSize     =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ), \
                                                           totalHardlinkSize  =(SELECT TOTAL(entryFragments.size) \
                                                                                FROM entries \
                                                                                  LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                               ) \
                                                       WHERE id=%lld \
                                                      ",
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
                                                      storageId,

                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIi64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storages \
                                                       SET totalEntryCount= totalFileCount \
                                                                           +totalImageCount \
                                                                           +totalDirectoryCount \
                                                                           +totalLinkCount\
                                                                           +totalHardlinkCount \
                                                                           +totalSpecialCount, \
                                                           totalEntrySize = totalFileSize \
                                                                           +totalImageSize \
                                                                           +totalHardlinkSize \
                                                       WHERE id=%llu \
                                                      ",
                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create aggregates fail for storage #%"PRIi64": (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             // total count/size newest
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storages \
                                                       SET totalFileCountNewest     =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ), \
                                                           totalImageCountNewest    =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ), \
                                                           totalDirectoryCountNewest=(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND directoryEntries.storageId=%lld \
                                                                                     ), \
                                                           totalLinkCountNewest     =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN linkEntries      ON linkEntries.entryId     =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND linkEntries.storageId=%lld \
                                                                                     ), \
                                                           totalHardlinkCountNewest =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ), \
                                                           totalSpecialCountNewest  =(SELECT COUNT(entriesNewest.id) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN specialEntries   ON specialEntries.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND specialEntries.storageId=%lld \
                                                                                     ), \
                                                           \
                                                           totalFileSizeNewest      =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ), \
                                                           totalImageSizeNewest     =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ), \
                                                           totalHardlinkSizeNewest  =(SELECT TOTAL(entryFragments.size) \
                                                                                      FROM entriesNewest \
                                                                                        LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                      WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                     ) \
                                                       WHERE id=%lld \
                                                      ",
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
                                                      storageId,

                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIi64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             error = Database_execute(databaseHandle,
                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                      NULL,  // changedRowCount
                                                      "UPDATE storages \
                                                       SET totalEntryCountNewest= totalFileCountNewest \
                                                                                 +totalImageCountNewest \
                                                                                 +totalDirectoryCountNewest \
                                                                                 +totalLinkCountNewest \
                                                                                 +totalHardlinkCountNewest \
                                                                                 +totalSpecialCountNewest, \
                                                           totalEntrySizeNewest = totalFileSizeNewest \
                                                                                 +totalImageSizeNewest \
                                                                                 +totalHardlinkSizeNewest \
                                                       WHERE id=%lld \
                                                      ",
                                                      storageId
                                                     );
                             if (error != ERROR_NONE)
                             {
                               if (verboseFlag) fprintf(stderr,"FAIL!\n");
                               fprintf(stderr,"ERROR: create newest aggregates fail for storage #%"PRIi64" (error: %s)!\n",storageId,Error_getText(error));
                               return error;
                             }

                             n++;
                             if (verboseFlag) printPercentage(n,totalCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id \
                            FROM storages \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ",
                           String_isEmpty(storageIdsString) ? 1 : 0,
                           storageIdsString
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    String_delete(storageIdsString);
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
    String_delete(storageIdsString);
    fprintf(stderr,"ERROR: create aggregates fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(storageIdsString);
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
  ulong  total;

  if (verboseFlag) { fprintf(stderr,"Clean-up orphaned entries:\n"); }

  // initialize variables
  storageName = String_new();
  total       = 0;

  // clean-up fragments/directory entries/link entries/special entries without storage name
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entryFragments \
                            LEFT JOIN storages ON storages.id=entryFragments.storageId \
                          WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM directoryEntries \
                            LEFT JOIN storages ON storages.id=entryFragments.storageId \
                          WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM linkEntries \
                            LEFT JOIN storages ON storages.id=entryFragments.storageId \
                          WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM specialEntries \
                            LEFT JOIN storages ON storages.id=entryFragments.storageId \
                          WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                         "
                        );
  if (verboseFlag) { fprintf(stdout,"  %lu without storage name\n",n); }
  total += n;

  // clean-up entries without storage
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM fileEntries \
                          WHERE (SELECT COUNT(id) FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId)=0 \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM imageEntries \
                          WHERE (SELECT COUNT(id) FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId)=0 \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM hardlinkEntries \
                          WHERE (SELECT COUNT(id) FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId)=0 \
                         "
                        );

  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM fileEntries WHERE fileEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_FILE
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM imageEntries WHERE imageEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_IMAGE
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM directoryEntries WHERE directoryEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_DIRECTORY
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM linkEntries WHERE linkEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_LINK
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM hardlinkEntries WHERE hardlinkEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_HARDLINK
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM entries \
                          WHERE entries.type=%u AND (SELECT COUNT(id) FROM specialEntries WHERE specialEntries.entryId=entries.id)=0 \
                         ",
                         INDEX_CONST_TYPE_SPECIAL
                        );
  if (verboseFlag) { fprintf(stdout,"  %lu without storage\n",n); }
  total += n;

  // clean-up storages
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_(NULL,NULL),  // databaseRowFunction
                         &n,
                         "DELETE FROM storages \
                          WHERE name IS NULL OR name=''; \
                         "
                        );
  if (verboseFlag) { fprintf(stdout,"  %lu storages\n",n); }
  total += n;

  // clean-up *Entries without entry
  n = 0L;
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
  if (verboseFlag) { fprintf(stdout,"  %lu entries\n",n); }
  total += n;

  fprintf(stdout,"Clean-up %lu orphaned entries\n",total);

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
                                                                           "DELETE FROM storages \
                                                                            WHERE id=%lld \
                                                                           ",
                                                                           duplicateDatabaseId
                                                                          );

                                                    n++;

                                                    return ERROR_NONE;
                                                  },NULL),
                                                  NULL,  // changedRowCount
                                                  "SELECT id \
                                                   FROM storages \
                                                   WHERE id!=%lld AND name='%s' \
                                                  ",
                                                  databaseId,
                                                  storageName
                                                 );

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT id,name \
                          FROM storages \
                          WHERE deletedFlag!=1 \
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
                                                                           "DELETE FROM storages \
                                                                            WHERE id=%lld \
                                                                           ",
                                                                           purgeDatabaseId
                                                                          );

                                                    n++;

                                                    return ERROR_NONE;
                                                  },NULL),
                                                  NULL,  // changedRowCount
                                                  "SELECT id \
                                                   FROM storages \
                                                   WHERE id!=%lld \
                                                  ",
                                                  databaseId
                                                 );

                           if (verboseFlag) { fprintf(stderr,"  OK\n"); }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT id \
                          FROM storages \
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
    if ((columns[i] != NULL) && (stringLength(columns[i]) > widths[i])) widths[i] = stringLength(columns[i]);
    if ((values [i] != NULL) && (stringLength(values [i]) > widths[i])) widths[i] = stringLength(values [i]);
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
      printf("%s ",columns[i]); printSpaces(widths[i]-stringLength(columns[i]));
    }
    printf("\n");

    headerPrintedFlag = TRUE;
  }
  for (i = 0; i < count; i++)
  {
    if (values[i] != NULL)
    {
      if (showNamesFlag) printf("%s=",columns[i]);
      printf("%s ",!stringIsEmpty(values[i]) ? values[i] : "''"); if (showHeaderFlag) { printSpaces(widths[i]-(!stringIsEmpty(values[i]) ? stringLength(values[i]) : 2)); }
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
  int64  n;

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
                           "SELECT name,value \
                            FROM meta \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get meta data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  printf("Storages:");
  if (verboseFlag)
  {
    // show number of storages
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "storages",
                                  "COUNT(id)",
                                  "WHERE deletedFlag!=1"
                                 );
    if (error != ERROR_NONE)
    {
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
      exit(1);
    }
    printf(": %"PRIi64,n);
  }
  printf("\n");
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
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
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
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
                           INDEX_CONST_STATE_UPDATE_REQUESTED
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
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
                           INDEX_CONST_STATE_ERROR
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
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE deletedFlag=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  printf("Entries:");
  if (verboseFlag)
  {
    // show number of entries
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "entries",
                                  "COUNT(id)",
                                  ""
                                 );
    if (error != ERROR_NONE)
    {
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
      exit(1);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalEntryCount;
                             uint64 totalEntrySize;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalEntryCount = (ulong)atol(values[0]);
                             totalEntrySize  = (uint64)atoll(values[1]);

                             totalEntryCount = (ulong)atol(values[0]);
                             totalEntrySize  = (uint64)atoll(values[1]);

                             printf("  Total           : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalEntryCount),TOTAL(totalEntrySize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalFileCount;
                             uint64 totalFileSize;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalFileCount = (ulong)atol(values[0]);
                             totalFileSize  = (uint64)atoll(values[1]);

                             printf("  Files           : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalFileCount),TOTAL(totalFileSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalImageCount;
                             uint64 totalImageSize;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalImageCount = (ulong)atol(values[0]);
                             totalImageSize  = (uint64)atoll(values[1]);

                             printf("  Images          : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalImageCount),TOTAL(totalImageSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalDirectoryCount;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalDirectoryCount = (ulong)atol(values[0]);

                             printf("  Directories     : %lu\n",totalDirectoryCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalDirectoryCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalLinkCount;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalLinkCount = (ulong)atol(values[0]);

                             printf("  Links           : %lu\n",totalLinkCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalLinkCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalHardlinkCount;
                             uint64 totalHardlinkSize;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalHardlinkCount = (ulong)atol(values[0]);
                             totalHardlinkSize  = (uint64)atoll(values[1]);

                             printf("  Hardlinks       : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalHardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalHardlinkCount),TOTAL(totalHardlinkSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalSpecialCount;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalSpecialCount = (ulong)atol(values[0]);

                             printf("  Special         : %lu\n",totalSpecialCount);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalSpecialCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  printf("Newest entries:");
  if (verboseFlag)
  {
    // show number of newest entries
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "entriesNewest",
                                  "COUNT(id)",
                                  ""
                                 );
    if (error != ERROR_NONE)
    {
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
      exit(1);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalEntryCountNewest;
                             uint64 totalEntrySizeNewest;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalEntryCountNewest = (ulong)atol(values[0]);
                             totalEntrySizeNewest  = (uint64)atoll(values[1]);

                             printf("  Total           : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalEntryCountNewest,getByteSize(totalEntrySizeNewest),getByteUnitShort(totalEntrySizeNewest),totalEntrySizeNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalEntryCountNewest),TOTAL(totalEntrySizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalFileCountNewest;
                             uint64 totalFileSizeNewest;
\
                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalFileCountNewest = (ulong)atol(values[0]);
                             totalFileSizeNewest  = (uint64)atoll(values[1]);

                             printf("  Files           : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalFileCountNewest,getByteSize(totalFileSizeNewest),getByteUnitShort(totalFileSizeNewest),totalFileSizeNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalFileCountNewest),TOTAL(totalFileSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalImageCountNewest;
                             uint64 totalImageSizeNewest;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalImageCountNewest = (ulong)atol(values[0]);
                             totalImageSizeNewest  = (uint64)atoll(values[1]);

                             printf("  Images          : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalImageCountNewest,getByteSize(totalImageSizeNewest),getByteUnitShort(totalImageSizeNewest),totalImageSizeNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalImageCountNewest),TOTAL(totalImageSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalDirectoryCountNewest;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalDirectoryCountNewest = (ulong)atol(values[0]);

                             printf("  Directories     : %lu\n",totalDirectoryCountNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalDirectoryCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalLinkCountNewest;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalLinkCountNewest = (ulong)atol(values[0]);

                             printf("  Links           : %lu\n",totalLinkCountNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalLinkCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong  totalHardlinkCountNewest;
                             uint64 totalHardlinkSizeNewest;

                             assert(count == 2);
                             assert(values[0] != NULL);
                             assert(values[1] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalHardlinkCountNewest = (ulong)atol(values[0]);
                             totalHardlinkSizeNewest  = (uint64)atoll(values[1]);

                             printf("  Hardlinks       : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalHardlinkCountNewest,getByteSize(totalHardlinkSizeNewest),getByteUnitShort(totalHardlinkSizeNewest),totalHardlinkSizeNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalHardlinkCountNewest),TOTAL(totalHardlinkSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             ulong totalSpecialCountNewest;

                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             totalSpecialCountNewest = (ulong)atol(values[0]);

                             printf("  Special         : %lu\n",totalSpecialCountNewest);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT TOTAL(totalSpecialCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entries data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  printf("Entities:");
  if (verboseFlag)
  {
    // show number of entities
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "entities",
                                  "COUNT(id)",
                                  "WHERE deletedFlag!=1"
                                 );
    if (error != ERROR_NONE)
    {
      fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
      exit(1);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             assert(count == 1);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             printf("  Normal          : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_NORMAL
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entities data fail: %s!\n",Error_getText(error));
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

                             printf("  Full            : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_FULL
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entities data fail: %s!\n",Error_getText(error));
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

                             printf("  Differential    : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_DIFFERENTIAL
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entities data fail: %s!\n",Error_getText(error));
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

                             printf("  Incremental     : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_INCREMENTAL
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entities data fail: %s!\n",Error_getText(error));
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

                             printf("  Continuous      : %lu\n",atol(values[0]));

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entities data fail: %s!\n",Error_getText(error));
    exit(1);
  }
}

/***********************************************************************\
* Name   : printUUIDsInfo
* Purpose: print UUIDs index info
* Input  : databaseHandle - database handle
*          uuidIdArray    - array with UUID ids or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printUUIDsInfo(DatabaseHandle *databaseHandle, const Array uuidIdArray)
{
  String     uuidIdsString;
  ulong      i;
  DatabaseId uuidId;
  Errors     error;

  uuidIdsString = String_new();
  ARRAY_ITERATE(&uuidIdArray,i,uuidId)
  {
    if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
    String_formatAppend(uuidIdsString,"%lld",uuidId);
  }

  printf("UUIDs:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId uuidId;
                             ulong      totalEntryCount;
                             uint64     totalEntrySize;
                             ulong      totalFileCount;
                             uint64     totalFileSize;
                             ulong      totalImageCount;
                             uint64     totalImageSize;
                             ulong      totalDirectoryCount;
                             ulong      totalLinkCount;
                             ulong      totalHardlinkCount;
                             uint64     totalHardlinkSize;
                             ulong      totalSpecialCount;
                             String     idsString;

                             assert(count == 14);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             uuidId              = (DatabaseId)atoll(values[0]);
                             totalEntryCount     = (ulong)atoll(values[2]);
                             totalEntrySize      = (uint64)atoll(values[3]);
                             totalFileCount      = (ulong)atoll(values[4]);
                             totalFileSize       = (uint64)atoll(values[5]);
                             totalImageCount     = (ulong)atoll(values[6]);
                             totalImageSize      = (uint64)atoll(values[7]);
                             totalDirectoryCount = (ulong)atoll(values[8]);
                             totalLinkCount      = (ulong)atoll(values[9]);
                             totalHardlinkCount  = (ulong)atoll(values[10]);
                             totalHardlinkSize   = (uint64)atoll(values[11]);
                             totalSpecialCount   = (ulong)atoll(values[12]);

                             printf("  Id              : %"PRIi64"\n",uuidId);
                             printf("    UUID          : %s\n",values[ 1]);
                             printf("\n");
                             printf("    Total entries : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                             printf("\n");
                             printf("    Files         : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalFileCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalFileSize);
                             printf("    Images        : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalImageCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalImageSize);
                             printf("    Directories   : %lu\n",totalDirectoryCount);
                             printf("    Links         : %lu\n",totalLinkCount);
                             printf("    Hardlinks     : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalHardlinkCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalHardlinkSize);
                             printf("    Special       : %lu\n",totalSpecialCount);
                             printf("\n");

                             idsString = String_new();
                             String_clear(idsString);
                             Database_execute(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                              {
                                                assert(count == 1);
                                                assert(values[0] != NULL);

                                                UNUSED_VARIABLE(columns);
                                                UNUSED_VARIABLE(count);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_appendCString(idsString,values[0]);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              "SELECT id \
                                               FROM entities \
                                               WHERE     uuidId=%lld \
                                                     AND deletedFlag!=1 \
                                              ",
                                              uuidId
                                             );
                             printf("    Entity ids    : %s\n",String_cString(idsString));
                             String_clear(idsString);
                             Database_execute(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                              {
                                                assert(count == 1);
                                                assert(values[0] != NULL);

                                                UNUSED_VARIABLE(columns);
                                                UNUSED_VARIABLE(count);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_appendCString(idsString,values[0]);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              "SELECT id \
                                               FROM storages \
                                               WHERE     uuidId=%lld \
                                                     AND deletedFlag!=1 \
                                              ",
                                              uuidId
                                             );
                             printf("    Storage ids   : %s\n",String_cString(idsString));
                             String_delete(idsString);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id,\
                                   jobUUID, \
                                   \
                                   (SELECT TOTAL(totalEntryCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalEntrySize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT TOTAL(totalFileCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalFileSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalImageCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalImageSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalDirectoryCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalLinkCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalHardlinkCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalHardlinkSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalSpecialCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT TOTAL(totalEntryCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalEntrySizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT TOTAL(totalFileCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalFileSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalImageCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalImageSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalDirectoryCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalLinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalHardlinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalHardlinkSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT TOTAL(totalSpecialCountNewest) FROM entities WHERE entities.uuidId=uuids.id) \
                            FROM uuids \
                            WHERE     (%d OR id IN (%S)) \
                           ",
                           String_isEmpty(uuidIdsString) ? 1 : 0,
                           uuidIdsString
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get UUID data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(uuidIdsString);
}

/***********************************************************************\
* Name   : printEntitiesInfo
* Purpose: print entities index info
* Input  : databaseHandle - database handle
*          entityIdArray  - array with entity ids or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printEntitiesInfo(DatabaseHandle *databaseHandle, const Array entityIdArray)
{
  const char *TYPE_NAMES[] = {"none","normal","full","incremental","differential","continuous"};

  String     entityIdsString;
  ulong      i;
  DatabaseId entityId;
  Errors     error;

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIdArray,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

  printf("Entities:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId entityId;
                             uint       type;
                             ulong      totalEntryCount;
                             uint64     totalEntrySize;
                             ulong      totalFileCount;
                             uint64     totalFileSize;
                             ulong      totalImageCount;
                             uint64     totalImageSize;
                             ulong      totalDirectoryCount;
                             ulong      totalLinkCount;
                             ulong      totalhardlinkCount;
                             uint64     totalHardlinkSize;
                             ulong      totalSpecialCount;
                             DatabaseId uuidId;
                             String     idsString;

                             assert(count == 14);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             entityId            = (DatabaseId)atoll(values[0]);
                             type                = (uint)atoi(values[1]);
                             totalEntryCount     = (ulong)atoll(values[3]);
                             totalEntrySize      = (uint64)atoll(values[4]);
                             totalFileCount      = (ulong)atoll(values[5]);
                             totalFileSize       = (uint64)atoll(values[6]);
                             totalImageCount     = (ulong)atoll(values[7]);
                             totalImageSize      = (uint64)atoll(values[8]);
                             totalDirectoryCount = (ulong)atoll(values[9]);
                             totalLinkCount      = (ulong)atoll(values[10]);
                             totalhardlinkCount  = (ulong)atoll(values[11]);
                             totalHardlinkSize   = (uint64)atoll(values[12]);
                             totalSpecialCount   = (ulong)atoll(values[13]);

                             uuidId              = (DatabaseId)atoll(values[25]);

                             printf("  Id              : %"PRIi64"\n",entityId);
                             printf("    Type          : %s\n",(type <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS) ? TYPE_NAMES[type] : values[ 1]);
                             printf("    UUID          : %s\n",values[ 2]);
                             printf("\n");
                             printf("    Total entries : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                             printf("\n");
                             printf("    Files         : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                             printf("    Images        : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                             printf("    Directories   : %lu\n",totalDirectoryCount);
                             printf("    Links         : %lu\n",totalLinkCount);
                             printf("    Hardlinks     : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalhardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                             printf("    Special       : %lu\n",totalSpecialCount);
                             printf("\n");
                             printf("    UUID id       : %"PRIi64"\n",uuidId);

                             idsString = String_new();
                             String_clear(idsString);
                             Database_execute(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                              {
                                                assert(count == 1);
                                                assert(values[0] != NULL);

                                                UNUSED_VARIABLE(columns);
                                                UNUSED_VARIABLE(count);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_appendCString(idsString,values[0]);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              "SELECT id \
                                               FROM storages \
                                               WHERE     entityId=%lld \
                                                     AND deletedFlag!=1 \
                                              ",
                                              entityId
                                             );
                             printf("    Storage ids   : %s\n",String_cString(idsString));
                             String_delete(idsString);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id,\
                                   type, \
                                   jobUUID, \
                                   \
                                   totalEntryCount, \
                                   totalEntrySize, \
                                   \
                                   totalFileCount, \
                                   totalFileSize, \
                                   totalImageCount, \
                                   totalImageSize, \
                                   totalDirectoryCount, \
                                   totalLinkCount, \
                                   totalHardlinkCount, \
                                   totalHardlinkSize, \
                                   totalSpecialCount, \
                                   \
                                   totalEntryCountNewest, \
                                   totalEntrySizeNewest, \
                                   \
                                   totalFileCountNewest, \
                                   totalFileSizeNewest, \
                                   totalImageCountNewest, \
                                   totalImageSizeNewest, \
                                   totalDirectoryCountNewest, \
                                   totalLinkCountNewest, \
                                   totalHardlinkCountNewest, \
                                   totalHardlinkSizeNewest, \
                                   totalSpecialCountNewest, \
                                   \
                                   uuidId \
                            FROM entities \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ",
                           String_isEmpty(entityIdsString) ? 1 : 0,
                           entityIdsString
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entity data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(entityIdsString);
}

/***********************************************************************\
* Name   : printStoragesInfo
* Purpose: print storages index info
* Input  : databaseHandle - database handle
*          storageIdArray - array with storage ids or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printStoragesInfo(DatabaseHandle *databaseHandle, const Array storageIdArray, bool lostFlag)
{
  const char *STATE_TEXT[] = {"","OK","create","update requested","update","error"};
  const char *MODE_TEXT [] = {"manual","auto"};

#define INDEX_CONST_MODE_MANUAL 0
#define INDEX_CONST_MODE_AUTO 1

  String     storageIdsString;
  ulong      i;
  DatabaseId storageId;
  Errors     error;

  storageIdsString = String_new();
  ARRAY_ITERATE(&storageIdArray,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%lld",storageId);
  }

  printf("Storages:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             uint   state;
                             uint   mode;
                             ulong  totalEntryCount;
                             uint64 totalEntrySize;
                             ulong  totalFileCount;
                             uint64 totalFileSize;
                             ulong  totalImageCount;
                             uint64 totalImageSize;
                             ulong  totalDirectoryCount;
                             ulong  totalLinkCount;
                             ulong  totalHardlinkCount;
                             uint64 totalHardlinkSize;
                             ulong  totalSpecialCount;

//                             assert(count == 14);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             state               = (uint)atoi(values[7]);
                             mode                = (uint)atoi(values[8]);
                             totalEntryCount     = (values[11] != NULL) ? (ulong)atoll(values[11]) : 0L;
                             totalEntrySize      = (values[12] != NULL) ? (uint64)atoll(values[12]) : 0LL;
                             totalFileCount      = (values[13] != NULL) ? (ulong)atoll(values[13]) : 0L;
                             totalFileSize       = (values[14] != NULL) ? (uint64)atoll(values[14]) : 0LL;
                             totalImageCount     = (values[15] != NULL) ? (ulong)atoll(values[15]) : 0L;
                             totalImageSize      = (values[16] != NULL) ? (uint64)atoll(values[16]) : 0LL;
                             totalDirectoryCount = (values[17] != NULL) ? (ulong)atoll(values[17]) : 0L;
                             totalLinkCount      = (values[18] != NULL) ? (ulong)atoll(values[18]) : 0L;
                             totalHardlinkCount  = (values[19] != NULL) ? (ulong)atoll(values[19]) : 0L;
                             totalHardlinkSize   = (values[20] != NULL) ? (uint64)atoll(values[20]) : 0LL;
                             totalSpecialCount   = (values[21] != NULL) ? (ulong)atoll(values[21]) : 0L;

                             printf("  Id              : %s\n",values[ 0]);
                             printf("    Name          : %s\n",values[ 3]);
                             printf("    Created       : %s\n",(values[ 4] != NULL) ? values[ 4] : "");
                             printf("    User name     : %s\n",(values[ 5] != NULL) ? values[ 5] : "");
                             printf("    Comment       : %s\n",(values[ 6] != NULL) ? values[ 6] : "");
                             printf("    State         : %s\n",(state <= INDEX_CONST_STATE_ERROR) ? STATE_TEXT[state] : values[ 7]);
                             printf("    Mode          : %s\n",(mode <= INDEX_CONST_MODE_AUTO) ? MODE_TEXT[mode] : values[ 8]);
                             printf("    Last checked  : %s\n",values[ 9]);
                             printf("    Error message : %s\n",(values[10] != NULL) ? values[10] : "");
                             printf("\n");
                             printf("    Total entries : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                             printf("\n");
                             printf("    Files         : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                             printf("    Images        : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                             printf("    Directories   : %lu\n",totalDirectoryCount);
                             printf("    Links         : %lu\n",totalLinkCount);
                             printf("    Hardlinks     : %lu, %.1lf%s (%"PRIu64"bytes)\n",totalHardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                             printf("    Special       : %lu\n",totalSpecialCount);
                             printf("\n");
                             printf("    UUID id       : %s\n",values[1]);
                             printf("    Entity id     : %s\n",values[2]);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT storages.id,\
                                   storages.uuidId, \
                                   storages.entityId, \
                                   storages.name, \
                                   storages.created, \
                                   storages.userName, \
                                   storages.comment, \
                                   storages.state, \
                                   storages.mode, \
                                   storages.lastChecked, \
                                   storages.errorMessage, \
                                   \
                                   storages.totalEntryCount, \
                                   storages.totalEntrySize, \
                                   \
                                   storages.totalFileCount, \
                                   storages.totalFileSize, \
                                   storages.totalImageCount, \
                                   storages.totalImageSize, \
                                   storages.totalDirectoryCount, \
                                   storages.totalLinkCount, \
                                   storages.totalHardlinkCount, \
                                   storages.totalHardlinkSize, \
                                   storages.totalSpecialCount, \
                                   \
                                   storages.totalEntryCountNewest, \
                                   storages.totalEntrySizeNewest, \
                                   \
                                   storages.totalFileCountNewest, \
                                   storages.totalFileSizeNewest, \
                                   storages.totalImageCountNewest, \
                                   storages.totalImageSizeNewest, \
                                   storages.totalDirectoryCountNewest, \
                                   storages.totalLinkCountNewest, \
                                   storages.totalHardlinkCountNewest, \
                                   storages.totalHardlinkSizeNewest, \
                                   storages.totalSpecialCountNewest \
                            FROM storages \
                            LEFT JOIN entities ON entities.id=storages.entityId \
                            WHERE     (%d OR storages.id IN (%S)) \
                                  AND (%d OR entities.id IS NULL) \
                                  AND storages.deletedFlag!=1 \
                           ",
                           String_isEmpty(storageIdsString) ? 1 : 0,
                           storageIdsString,
                           !lostFlag
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get storage data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(storageIdsString);
}

/***********************************************************************\
* Name   : printEntriesInfo
* Purpose: print entries index info
* Input  : databaseHandle - database handle
*          entityIdArray  - array with entity ids or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printEntriesInfo(DatabaseHandle *databaseHandle, const Array entityIdArray, ConstString name)
{
  const char *TYPE_TEXT[] = {"","uuid","entity","storage","entry","file","image","directory","link","hardlink","special","history"};

  String     entityIdsString;
  String     ftsName;
  ulong      i;
  DatabaseId entityId;
  Errors     error;

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIdArray,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

  ftsName = String_new();
  getFTSString(ftsName,name);

  printf("Entries:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                           {
                             DatabaseId entityId;
                             DatabaseId uuidId;
                             bool       entityOutputFlag;

                             assert(count == 2);
                             assert(values[0] != NULL);

                             UNUSED_VARIABLE(columns);
                             UNUSED_VARIABLE(count);
                             UNUSED_VARIABLE(userData);

                             entityId = (DatabaseId)atoll(values[0]);
                             uuidId   = (DatabaseId)atoll(values[1]);

                             entityOutputFlag = FALSE;
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                                                      {
                                                        uint type;

                                                        assert(count == 8);
                                                        assert(values[0] != NULL);

                                                        UNUSED_VARIABLE(columns);
                                                        UNUSED_VARIABLE(count);
                                                        UNUSED_VARIABLE(userData);

                                                        type = (uint)atoll(values[2]);

                                                        if (!entityOutputFlag)
                                                        {
                                                          printf("  Entity id: %"PRIi64"\n",entityId);
                                                          printf("  UUID id  : %"PRIi64"\n",uuidId);
                                                          entityOutputFlag = TRUE;
                                                        }
                                                        printf("    Id               : %s\n",values[0]);
                                                        printf("      Name           : %s\n",values[1]);
                                                        printf("      Type           : %s\n",(type <= INDEX_CONST_TYPE_HISTORY) ? TYPE_TEXT[type] : values[2]);
                                                        switch (type)
                                                        {
                                                          case INDEX_CONST_TYPE_FILE:
                                                            printf("      Size           : %s\n",values[ 3]);
                                                            printf("      Fragment id    : %s\n",values[ 9]);
                                                            printf("      Fragment offset: %s\n",values[11]);
                                                            printf("      Fragment size  : %s\n",values[12]);
                                                            printf("      Storage id:    : %s\n",values[10]);
                                                            break;
                                                          case INDEX_CONST_TYPE_IMAGE:
                                                            printf("      Size           : %s\n",values[ 4]);
                                                            printf("      Fragment id    : %s\n",values[ 9]);
                                                            printf("      Fragment offset: %s\n",values[11]);
                                                            printf("      Fragment size  : %s\n",values[12]);
                                                            printf("      Storage id:    : %s\n",values[10]);
                                                            break;
                                                          case INDEX_CONST_TYPE_DIRECTORY:
                                                            printf("      Storage id:    : %s\n",values[ 5]);
                                                            break;
                                                          case INDEX_CONST_TYPE_LINK:
                                                            printf("      Storage id:    : %s\n",values[ 6]);
                                                            break;
                                                          case INDEX_CONST_TYPE_HARDLINK:
                                                            printf("      Size           : %s\n",values[ 7]);
                                                            printf("      Fragment id    : %s\n",values[ 9]);
                                                            printf("      Fragment offset: %s\n",values[11]);
                                                            printf("      Fragment size  : %s\n",values[12]);
                                                            printf("      Storage id:    : %s\n",values[10]);
                                                            break;
                                                            break;
                                                          case INDEX_CONST_TYPE_SPECIAL:
                                                            printf("      Storage id:    : %s\n",values[ 8]);
                                                            break;
                                                          default:
                                                            break;
                                                        }


                                                        return ERROR_NONE;
                                                      },NULL),
                                                      NULL,  // changedRowCount
                                                      "SELECT entries.id,\
                                                              entries.name, \
                                                              entries.type, \
                                                              \
                                                              fileEntries.size, \
                                                              imageEntries.size, \
                                                              directoryEntries.storageId, \
                                                              linkEntries.storageId, \
                                                              hardlinkEntries.size, \
                                                              specialEntries.storageId, \
                                                              entryFragments.id, \
                                                              entryFragments.storageId, \
                                                              entryFragments.offset, \
                                                              entryFragments.size \
                                                              \
                                                       FROM entries \
                                                       LEFT JOIN fileEntries      ON fileEntries.entryId     =entries.id \
                                                       LEFT JOIN imageEntries     ON imageEntries.entryId    =entries.id \
                                                       LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                                       LEFT JOIN linkEntries      ON linkEntries.entryId     =entries.id \
                                                       LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId =entries.id \
                                                       LEFT JOIN specialEntries   ON specialEntries.entryId  =entries.id \
                                                       LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                       WHERE     entries.entityId=%lld \
                                                             AND (%d OR entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')) \
                                                      ",
                                                      entityId,
                                                      String_isEmpty(ftsName) ? 1 : 0,
                                                      ftsName
                                                     );
                             if (error != ERROR_NONE)
                             {
                               return error;
                             }

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           "SELECT id,uuidId \
                            FROM entities \
                            WHERE     (%d OR id IN (%S)) \
                                  AND deletedFlag!=1 \
                           ",
                           String_isEmpty(entityIdsString) ? 1 : 0,
                           entityIdsString
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: get entity data fail: %s!\n",Error_getText(error));
    exit(1);
  }

  // free resources
  String_delete(ftsName);
  String_delete(entityIdsString);
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
  return Misc_waitHandle(STDIN_FILENO,NULL,HANDLE_EVENT_INPUT,0) != 0;
}
/*---------------------------------------------------------------------*/

LOCAL void xxx(DatabaseHandle *databaseHandle)
{
  Errors error;
//  ulong  totalCount;
//  ulong  n;

  // start transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "BEGIN TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

#if 0
  entityId        INTEGER REFERENCES entities(id) ON DELETE CASCADE,
  name            TEXT NOT NULL,
  type            INTEGER,
  timeLastAccess  INTEGER,
  timeModified    INTEGER,
  timeLastChanged INTEGER,
  userId          INTEGER,
  groupId         INTEGER,
  permission      INTEGER,
#endif

  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
//                               DatabaseId storageId;
//                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

//fprintf(stderr,"%s, %d: xadff %s %s %s %s\n",__FILE__,__LINE__,values[0 ],values[1 ],values[2 ],values[3 ]);

                               return Database_execute(databaseHandle,
                                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                       NULL,  // changedRowCount
                                                       "INSERT INTO ss \
                                                        (entityId       ,\
                                                         storageId, \
                                                         name           ,\
                                                         type           ,\
                                                         timeLastAccess ,\
                                                         timeModified   ,\
                                                         timeLastChanged,\
                                                         userId         ,\
                                                         groupId        ,\
                                                         permission     \
                                                        ) \
                                                        VALUES \
                                                        (%llu,\
                                                         %llu, \
                                                         %'s, \
                                                         %llu, \
                                                         %llu, \
                                                         %llu, \
                                                         %llu, \
                                                         %llu, \
                                                         %llu, \
                                                         %llu \
                                                        ) \
                                                       ",
                                                       atoll(values[1]),
                                                       atoll(values[2]),
                                                       values[3],
                                                       atoll(values[4]),
                                                       atoll(values[5]),
                                                       atoll(values[6]),
                                                       atoll(values[7]),
                                                       atoll(values[8]),
                                                       atoll(values[9])
                                                      );
                               },NULL),
                             NULL,  // changedRowCount
                             "SELECT entries.id,\\
                              entries.entityId,\
                              entryFragments.storageId, \
                              entries.name, \
                              entries.type, \
                              entries.timeLastAccess, \
                              entries.timeModified, \
                              entries.timeLastChanged, \
                              entries.userId, \
                              entries.groupId, \
                              entries.permission \
                             FROM entries LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                             WHERE entries.type in (5,6,9) \
                             and entryFragments.id is not null \
                             "
                            );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
//                               DatabaseId storageId;
//                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

//fprintf(stderr,"%s, %d: xadff %s %s %s %s\n",__FILE__,__LINE__,values[0 ],values[1 ],values[2 ],values[3 ]);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss \
                                                      (entityId       ,\
                                                       storageId, \
                                                       name           ,\
                                                       type           ,\
                                                       timeLastAccess ,\
                                                       timeModified   ,\
                                                       timeLastChanged,\
                                                       userId         ,\
                                                       groupId        ,\
                                                       permission     \
                                                      ) \
                                                      VALUES \
                                                      (%llu,\
                                                       %llu, \
                                                       %'s, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu \
                                                      ) \
                                                     ",
                                                     atoll(values[1]),
                                                     atoll(values[2]),
                                                     values[3],
                                                     atoll(values[4]),
                                                     atoll(values[5]),
                                                     atoll(values[6]),
                                                     atoll(values[7]),
                                                     atoll(values[8]),
                                                     atoll(values[9])
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.id,\\
                            entries.entityId,\
                            directoryEntries.storageId, \
                            entries.name, \
                            entries.type, \
                            entries.timeLastAccess, \
                            entries.timeModified, \
                            entries.timeLastChanged, \
                            entries.userId, \
                            entries.groupId, \
                            entries.permission \
                           FROM entries LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                           WHERE entries.type=7 \
                           and directoryEntries.id is not null \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
//                               DatabaseId storageId;
//                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

//fprintf(stderr,"%s, %d: xadff %s %s %s %s\n",__FILE__,__LINE__,values[0 ],values[1 ],values[2 ],values[3 ]);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss \
                                                      (entityId       ,\
                                                       storageId, \
                                                       name           ,\
                                                       type           ,\
                                                       timeLastAccess ,\
                                                       timeModified   ,\
                                                       timeLastChanged,\
                                                       userId         ,\
                                                       groupId        ,\
                                                       permission     \
                                                      ) \
                                                      VALUES \
                                                      (%llu,\
                                                       %llu, \
                                                       %'s, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu \
                                                      ) \
                                                     ",
                                                     atoll(values[1]),
                                                     atoll(values[2]),
                                                     values[3],
                                                     atoll(values[4]),
                                                     atoll(values[5]),
                                                     atoll(values[6]),
                                                     atoll(values[7]),
                                                     atoll(values[8]),
                                                     atoll(values[9])
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.id,\\
                            entries.entityId,\
                            linkEntries.storageId, \
                            entries.name, \
                            entries.type, \
                            entries.timeLastAccess, \
                            entries.timeModified, \
                            entries.timeLastChanged, \
                            entries.userId, \
                            entries.groupId, \
                            entries.permission \
                           FROM entries LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                           WHERE entries.type=8 \
                           and linkEntries.id is not null \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
//                               DatabaseId storageId;
//                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

//fprintf(stderr,"%s, %d: xadff %s %s %s %s\n",__FILE__,__LINE__,values[0 ],values[1 ],values[2 ],values[3 ]);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss \
                                                      (entityId       ,\
                                                       storageId, \
                                                       name           ,\
                                                       type           ,\
                                                       timeLastAccess ,\
                                                       timeModified   ,\
                                                       timeLastChanged,\
                                                       userId         ,\
                                                       groupId        ,\
                                                       permission     \
                                                      ) \
                                                      VALUES \
                                                      (%llu,\
                                                       %llu, \
                                                       %'s, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu, \
                                                       %llu \
                                                      ) \
                                                     ",
                                                     atoll(values[1]),
                                                     atoll(values[2]),
                                                     values[3],
                                                     atoll(values[4]),
                                                     atoll(values[5]),
                                                     atoll(values[6]),
                                                     atoll(values[7]),
                                                     atoll(values[8]),
                                                     atoll(values[9])
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT entries.id,\\
                            entries.entityId,\
                            specialEntries.storageId, \
                            entries.name, \
                            entries.type, \
                            entries.timeLastAccess, \
                            entries.timeModified, \
                            entries.timeLastChanged, \
                            entries.userId, \
                            entries.groupId, \
                            entries.permission \
                           FROM entries LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                           WHERE entries.type=10 \
                           and specialEntries.id is not null \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
#if 0
  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               DatabaseId storageId;
                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                             storageId         = (DatabaseId)atoll(values[0]);
                             entryId         = (DatabaseId)atoll(values[0]);
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss (storageId,entryId) VALUES (%llu,%llu)",
                                                     storageId,
                                                     entryId
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT storageId,entryId FROM linkEntries LEFT JOIN storages ON storages.id=linkEntries.storageId"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               DatabaseId storageId;
                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                             storageId         = (DatabaseId)atoll(values[0]);
                             entryId         = (DatabaseId)atoll(values[0]);
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss (storageId,entryId) VALUES (%llu,%llu)",
                                                     storageId,
                                                     entryId
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT storageId,entryId FROM specialEntries LEFT JOIN storages ON storages.id=specialEntries.storageId"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const char *columns[], const char *values[], uint count, void *userData),
                             {
                               DatabaseId storageId;
                               DatabaseId entryId;

//                               assert(count == 1);
                               assert(values != NULL);
                               assert(values[0] != NULL);

                               UNUSED_VARIABLE(columns);
                               UNUSED_VARIABLE(count);
                               UNUSED_VARIABLE(userData);

                             storageId         = (DatabaseId)atoll(values[0]);
                             entryId         = (DatabaseId)atoll(values[0]);
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

                             return Database_execute(databaseHandle,
                                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                     NULL,  // changedRowCount
                                                     "INSERT INTO ss (storageId,entryId) VALUES (%llu,%llu)",
                                                     storageId,
                                                     entryId
                                                    );
                             },NULL),
                           NULL,  // changedRowCount
                           "SELECT storageId,entryId FROM entryFragments LEFT JOIN storages ON storages.id=entryFragments.storageId"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    (void)Database_execute(databaseHandle,CALLBACK_(NULL,NULL),NULL,"ROLLBACK TRANSACTION");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
#endif

  // end transaction
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "END TRANSACTION"
                          );
  if (error != ERROR_NONE)
  {
    printf("FAIL\n");
    fprintf(stderr,"ERROR: xxx fail: %s!\n",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
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

  Array            uuidIdArray,entityIdArray,storageIdArray;
  String           entryName;
  uint             i,n;
  CStringTokenizer stringTokenizer;
  const char       *token;
  DatabaseId       databaseId;
  const char       *databaseFileName;
  String           path;
  String           commands;
  char             line[MAX_LINE_LENGTH];
  Errors           error;
  DatabaseHandle   databaseHandle;
  uint64           t0,t1;
  String           s;
  const char       *l;

  initAll();

  // init variables
  Array_init(&uuidIdArray,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&entityIdArray,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&storageIdArray,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  entryName        = String_new();
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
    else if (stringStartsWith(argv[i],"--info-uuids"))
    {
      infoUUIDsFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][16],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&uuidIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-entities"))
    {
      infoEntitiesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][16],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-storages"))
    {
      infoStoragesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][16],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-lost-storages"))
    {
      infoLostStoragesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-entries"))
    {
      infoEntriesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][14],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIdArray,&databaseId);
        }
        String_setCString(entryName,token);
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-lost-entries"))
    {
      infoLostEntriesFlag = TRUE;
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
    else if (stringStartsWith(argv[i],"--create-aggregates-directory-content"))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][38],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates-entities"))
    {
      createAggregatesEntitiesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][29],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates-storages"))
    {
      createAggregatesStoragesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][29],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates"))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      createAggregatesEntitiesFlag         = TRUE;
      createAggregatesStoragesFlag         = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][20],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIdArray,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
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
      forceFlag = TRUE;
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
    else if (stringEquals(argv[i],"-t") || stringEquals(argv[i],"--time"))
    {
      timeFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"-x") || stringEquals(argv[i],"--explain-query"))
    {
      explainQueryPlanFlag = TRUE;
      i++;
    }
else if (stringEquals(argv[i],"--xxx"))
{
  xxxFlag = TRUE;
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
  error = Database_setEnabledForeignKeys(&databaseHandle,foreignKeysFlag);
  if (error != ERROR_NONE)
  {
    closeDatabase(&databaseHandle);
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
      || (   !infoUUIDsFlag
          && !infoEntitiesFlag
          && !infoStoragesFlag
          && !infoLostStoragesFlag
          && !infoEntriesFlag
          && !infoLostEntriesFlag
          && !checkFlag
          && !createTriggersFlag
          && !createNewestFlag
          && !createIndizesFlag
          && !createAggregatesEntitiesFlag
          && !createAggregatesStoragesFlag
          && !cleanFlag
          && !purgeDeletedFlag
          && !vacuumFlag
&& !xxxFlag
          && !showStoragesFlag
          && !showEntriesFlag
          && !showEntriesNewestFlag
          && String_isEmpty(commands)
          && !pipeFlag
          && !inputAvailable())
     )
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    printInfo(&databaseHandle);
  }

  if (infoUUIDsFlag)
  {
    printUUIDsInfo(&databaseHandle,uuidIdArray);
  }

  if (infoEntitiesFlag)
  {
    printEntitiesInfo(&databaseHandle,entityIdArray);
  }

  if      (infoLostStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIdArray,TRUE);
  }
  else if (infoStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIdArray,FALSE);
  }

  if (infoEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entityIdArray,entryName);
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

  // calculate aggregates data
  if (createAggregatesDirectoryContentFlag)
  {
    createAggregatesDirectoryContent(&databaseHandle,entityIdArray);
  }
  if (createAggregatesStoragesFlag)
  {
    createAggregatesStorages(&databaseHandle,storageIdArray);
  }
  if (createAggregatesEntitiesFlag)
  {
    createAggregatesEntities(&databaseHandle,entityIdArray);
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

if (xxxFlag)
{
  xxx(&databaseHandle);
}

#warning remove? use storages-info?
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
                     "SELECT MAX(storages.id),MAX(LENGTH(storages.name)) FROM storages \
                        LEFT JOIN entities on storages.entityId=entities.id \
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
                     "SELECT storages.id,storages.name,entities.jobUUID,entities.type FROM storages \
                        LEFT JOIN entities on storages.entityId=entities.id \
                      WHERE %d OR entities.jobUUID=%'s \
                      ORDER BY storages.name ASC \
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
                     "SELECT MAX(entries.id),MAX(LENGTH(entries.name)),MAX(LENGTH(storages.name)) FROM entries \
                        LEFT JOIN storages ON entries.storageId=storages.id \
                        LEFT JOIN entities ON storages.entityId=entities.id \
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
                     "SELECT entries.id,entries.name,storages.name FROM entries \
                        LEFT JOIN storages ON entries.storageId=storages.id \
                        LEFT JOIN entities ON storages.entityId=entities.id \
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
                     "SELECT MAX(entriesNewest.id),MAX(LENGTH(entriesNewest.name)),MAX(LENGTH(storages.name)) FROM entriesNewest \
                        LEFT JOIN storages ON entriesNewest.storageId=storages.id \
                        LEFT JOIN entities ON storages.entityId=entities.id \
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
                     "SELECT entriesNewest.id,entriesNewest.name,storages.name FROM entriesNewest \
                        LEFT JOIN storages ON entriesNewest.storageId=storages.id \
                        LEFT JOIN entities ON storages.entityId=entities.id \
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
        s = String_new();
        s = explainQueryPlanFlag
              ? String_append(String_setCString(s,"EXPLAIN QUERY PLAN "),commands)
              : String_set(s,commands);
        t0 = Misc_getTimestamp();
        error = Database_execute(&databaseHandle,
                                 CALLBACK_(printRow,NULL),
                                 NULL,  // changedRowCount
                                 String_cString(s)
                                );
        t1 = Misc_getTimestamp();
        String_delete(s);
        if (error != ERROR_NONE)
        {
          fprintf(stderr,"ERROR: SQL command '%s' fail: %s!\n",String_cString(commands),Error_getText(error));
          String_delete(commands);
          exit(EXITCODE_FAIL);
        }
        if (timeFlag && !explainQueryPlanFlag) printf("Execution time: %"PRIu64"us\n",t1-t0);
      }
    }
  }

  while (inputAvailable() && (fgets(line,sizeof(line),stdin) != NULL))
  {
    l = stringTrim(line);
    t0 = Misc_getTimestamp();
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
    t1 = Misc_getTimestamp();
    if (timeFlag && !explainQueryPlanFlag) printf("Execution time: %"PRIu64"us\n",t1-t0);
  }

  // close database
  closeDatabase(&databaseHandle);

  // free resources
  String_delete(entryName);
  Array_done(&storageIdArray);
  Array_done(&entityIdArray);
  Array_done(&uuidIdArray);
  String_delete(commands);

  doneAll();

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
