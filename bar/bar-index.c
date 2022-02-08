/***********************************************************************\
*
* $Revision: 1458 $
* $Date: 2012-01-28 09:50:13 +0100 (Sat, 28 Jan 2012) $
* $Author: trupp $
* Contents: BAR index tool
* Systems: Unix
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_PCRE
  #include <pcreposix.h>
#endif
#include <assert.h>

#if defined(HAVE_MARIADB_MYSQL_H) && defined(HAVE_MARIADB_ERRMSG_H)
  #include "mariadb/mysql.h"
  #include "mariadb/errmsg.h"
#endif /* HAVE_MARIADB_MYSQL_H && HAVE_MARIADB_ERRMSG_H */

#include "common/global.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/cmdoptions.h"
#include "common/database.h"
#include "common/misc.h"
#include "common/progressinfo.h"

#include "sqlite3.h"

#include "index/index.h"
#include "index/index_common.h"
#include "index_definition.h"
#include "archive_format_const.h"

/****************** Conditional compilation switches *******************/
#define _INDEX_DEBUG_IMPORT_DATABASE

//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

/***************************** Constants *******************************/

#define __VERSION_TO_STRING(z) __VERSION_TO_STRING_TMP(z)
#define __VERSION_TO_STRING_TMP(z) #z
#define VERSION_MAJOR_STRING __VERSION_TO_STRING(VERSION_MAJOR)
#define VERSION_MINOR_STRING __VERSION_TO_STRING(VERSION_MINOR)
#define VERSION_REPOSITORY_STRING __VERSION_TO_STRING(VERSION_REPOSITORY)
#define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING VERSION_PATCH " (rev. " VERSION_REPOSITORY_STRING ")"

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

#define INDEX_CONST_TYPE_ANY 0

/***************************** Datatypes *******************************/
typedef struct
{
  bool   showHeaderFlag;
  bool   printedHeaderFlag;
  size_t *widths;
} PrintRowData;

#if 0
// TODO: still not used
LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] = CMD_VALUE_ARRAY
(
  CMD_OPTION_BOOLEAN      ("table-names",                       0  , 1, 0, showTableNames,   "xxx"                  ),
  CMD_OPTION_BOOLEAN      ("index-names",                       0  , 1, 0, showIndexNames,   ""                  ),
  CMD_OPTION_BOOLEAN      ("trigger-names",                     0  , 1, 0, showTriggerNames, ""                  ),
  CMD_OPTION_BOOLEAN      ("drop-tables",                       0  , 1, 0, dropTablesFlag,   ""                  ),
  CMD_OPTION_BOOLEAN      ("drop-triggers",                     0  , 1, 0, dropTriggersFlag, ""                  ),
  CMD_OPTION_BOOLEAN      ("drop-indizes",                      0  , 1, 0, dropIndizesFlag,  ""                  ),
  CMD_OPTION_BOOLEAN      ("help",                              'h', 0, 0, helpFlag,         "output this help"                ),
  CMD_OPTION_BOOLEAN      ("xhelp",                             0,   0, 0, xhelpFlag,        "output help to extended options" ),
);
#endif

/***************************** Variables *******************************/
LOCAL const char *changeToDirectory                   = NULL;
LOCAL const char *importFileName                      = NULL;
LOCAL bool       infoFlag                             = FALSE;  // output index database info
LOCAL bool       infoUUIDsFlag                        = FALSE;  // output index database UUIDs info
LOCAL bool       infoEntitiesFlag                     = FALSE;  // output index database entities info
LOCAL bool       infoStoragesFlag                     = FALSE;  // output index database storages info
LOCAL bool       infoLostStoragesFlag                 = FALSE;  // output index database lost storages info
LOCAL bool       infoEntriesFlag                      = FALSE;  // output index database entries info
LOCAL bool       infoLostEntriesFlag                  = FALSE;  // output index database lost entries info
LOCAL uint       entryType                            = INDEX_CONST_TYPE_ANY;
LOCAL bool       checkIntegrityFlag                   = FALSE;  // check database integrity
LOCAL bool       checkOrphanedFlag                    = FALSE;  // check database orphaned entries
LOCAL bool       checkDuplicatesFlag                  = FALSE;  // check database duplicate entries
LOCAL bool       optimizeFlag                         = FALSE;  // optimize database
LOCAL bool       reindexFlag                          = FALSE;  // re-create existing indizes
LOCAL bool       createFlag                           = FALSE;  // create new index database
LOCAL bool       createTriggersFlag                   = FALSE;  // re-create triggers
LOCAL bool       dropTablesFlag                       = FALSE;  // drop tables
LOCAL bool       dropTriggersFlag                     = FALSE;  // drop triggers
LOCAL bool       createIndizesFlag                    = FALSE;  // re-create indizes
LOCAL bool       createFTSIndizesFlag                 = FALSE;  // re-create FTS indizes
LOCAL bool       showTableNames                       = FALSE;  // output index database table names
LOCAL bool       showIndexNames                       = FALSE;  // output index database index names
LOCAL bool       showTriggerNames                     = FALSE;  // output index database trigger names
LOCAL bool       dropIndizesFlag                      = FALSE;  // drop indizes
LOCAL bool       createNewestFlag                     = FALSE;  // re-create newest data
LOCAL bool       createAggregatesDirectoryContentFlag = FALSE;  // re-create aggregates entities data
LOCAL bool       createAggregatesEntitiesFlag         = FALSE;  // re-create aggregates entities data
LOCAL bool       createAggregatesStoragesFlag         = FALSE;  // re-create aggregates storages data
LOCAL bool       cleanOrphanedFlag                    = FALSE;  // execute clean orphaned entries
LOCAL bool       cleanDuplicatesFlag                  = FALSE;  // execute clean duplicate entries
LOCAL bool       purgeDeletedFlag                     = FALSE;  // execute purge deleted storages
LOCAL bool       vacuumFlag                           = FALSE;  // execute vacuum
LOCAL bool       showStoragesFlag                     = FALSE;  // show storages of job
LOCAL bool       showEntriesFlag                      = FALSE;  // show entries of job
LOCAL bool       showEntriesNewestFlag                = FALSE;  // show newest entries of job
LOCAL bool       showNamesFlag                        = FALSE;
LOCAL bool       showHeaderFlag                       = FALSE;
LOCAL bool       transactionFlag                      = FALSE;
LOCAL bool       foreignKeysFlag                      = TRUE;
LOCAL bool       forceFlag                            = FALSE;
LOCAL bool       pipeFlag                             = FALSE;
LOCAL const char *tmpDirectory                        = NULL;
LOCAL bool       verboseFlag                          = TRUE;
LOCAL bool       timeFlag                             = FALSE;
LOCAL bool       explainQueryPlanFlag                 = FALSE;
LOCAL const char *jobUUID                             = NULL;
LOCAL const char *toFileName                          = NULL;
// TODO:
//LOCAL bool       helpFlag                             = FALSE;
//LOCAL bool       xhelpFlag                            = FALSE;

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : DIMPORT
* Purpose: debug import index
* Input  : format - format string
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef INDEX_DEBUG_IMPORT_DATABASE
  #define DIMPORT(format,...) \
    do \
    { \
      uint64 timestamp = Misc_getTimestamp() / 1000LL; \
      \
      fprintf(stderr,"DEBUG IMPORT [%12"PRIu64".%03"PRIu64"]: ",timestamp/1000,timestamp%1000); \
      fprintf(stderr,format, ## __VA_ARGS__); \
      fprintf(stderr,"\n"); \
    } \
    while (0)
#else /* not INDEX_DEBUG_IMPORT_DATABASE */
  #define DIMPORT(format,...) \
    do \
    { \
    } \
    while (0)
#endif /* INDEX_DEBUG_IMPORT_DATABASE */

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

LOCAL void printUsage(const char *programName, bool extendedFlag)
{
  printf("Usage %s: [<options>] <URI> [<SQL command>...|-]\n",programName);
  printf("\n");
  printf("URI: [sqlite:]<file name>\n");
  printf("     mariadb:<server>:<user>:<password>\n");
  printf("     postgresql:<server>:<user>:<password>\n");
  printf("\n");
  printf("Options:  -C|--directory=<name>                        - change to directory\n");
  printf("          --info                                       - output index database infos\n");
  printf("          --info-jobs[=<uuid id>|UUID,...]             - output index database job infos\n");
  printf("          --info-entities[=<entity id>,...]            - output index database entities infos\n");
  printf("          --info-entries[=<entity id>,...|name]        - output index database entries infos\n");
  printf("          --info-lost-entries[=<entity id>,...|name]   - output index database entries infos without an entity\n");
  printf("          --entry-type=<type>                          - entries type:\n");
  printf("                                                           file\n");
  printf("                                                           image\n");
  printf("                                                           directory\n");
  printf("                                                           link\n");
  printf("                                                           harlink\n");
  printf("                                                           special\n");
  printf("          --info-storages[=<storage id>,...|name]      - output index database storages infos\n");
  printf("          --info-lost-storages[=<storage id>,...|name] - output index database storages infos without an entity\n");
  printf("          --create                                     - create new index database\n");
  printf("          --create-triggers                            - re-create triggers\n");
  printf("          --create-indizes                             - re-create indizes\n");
  printf("          --create-fts-indizes                         - re-create FTS indizes (full text search)\n");
  printf("          --create-newest[=id,...]                     - re-create newest data\n");
  printf("          --create-aggregates                          - re-create aggregated data\n");
  printf("          --create-aggregates-directory-content        - re-create aggregated data directory content\n");
  printf("          --create-aggregates-entities                 - re-create aggregated data entities\n");
  printf("          --create-aggregates-storages                 - re-create aggregated data storages\n");
  printf("          --import <URI>                               - import database\n");
  printf("          --optimize                                   - optimize database (analyze and collect statistics data)\n");
  printf("          --reindex                                    - re-create all existing indizes\n");
  printf("          --check                                      - check index database\n");
  printf("          --check-integrity                            - check index database integrity\n");
  printf("          --check-orphaned                             - check index database for orphaned entries\n");
  printf("          --check-duplicates                           - check index database for duplicate entries\n");
  printf("          --clean                                      - clean index database\n");
  printf("          --clean-orphaned                             - clean orphaned in index database\n");
  printf("          --clean-duplicates                           - clean duplicates in index database\n");
  printf("          --purge                                      - purge deleted storages\n");
  printf("          --vacuum [<new file name>]                   - collect and free unused file space\n");
  printf("          -e|--entries [<uuid>]                        - print entries\n");
  printf("          --entries-newest [<uuid>]                    - print newest entries\n");
  printf("          -s|--storages [<uuid>]                       - print storages\n");
  printf("          -n|--names                                   - print values with names\n");
  printf("          -H|--header                                  - print headers\n");
  printf("          --transaction                                - enable transcations\n");
  printf("          -f|--no-foreign-keys                         - disable foreign key constraints\n");
  printf("          --force                                      - force operation\n");
  printf("          --pipe|-                                     - read data from stdin and pipe into database\n");
  printf("          --tmp-directory                              - temporary files directory\n");
  printf("          -v|--verbose                                 - verbose output (default: ON; deprecated)\n");
  printf("          -q|--quiet                                   - no output\n");
  printf("          -t|--time                                    - print execution time\n");
  printf("          -x|--explain-query                           - explain SQL queries\n");
  if (extendedFlag)
  {
    printf("          --table-names                                - show table names\n");
    printf("          --index-names                                - show index names\n");
    printf("          --trigger-names                              - show trigger names\n");
    printf("          --drop-tables                                - drop all tables\n");
    printf("          --drop-triggers                              - drop all triggers\n");
    printf("          --drop-indizes                               - drop all indixes\n");
  }
  printf("          --version                                    - print version\n");
  printf("          -h|--help                                    - print this help\n");
  printf("          --xhelp                                      - print extended help\n");
}

/***********************************************************************\
* Name   : vprintInfo, pprintInfo, printInfo
* Purpose: output info to console
* Input  : prefix    - prefix text
*          format    - format string (like printf)
*          arguments - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void vprintInfo(const char *prefix, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  line = String_new();

  // format line
  if (prefix != NULL) String_appendCString(line,prefix);
  String_appendVFormat(line,format,arguments);

  // output
  fwrite(String_cString(line),1,String_length(line),stdout); fflush(stdout);

  String_delete(line);
}

/***********************************************************************\
* Name   : printInfo
* Purpose: output info to console
* Input  : format - format string (like printf)
*          ...    - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printInfo(const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  if (verboseFlag)
  {
    va_start(arguments,format);
    vprintInfo(NULL,format,arguments);
    va_end(arguments);
  }
}

/***********************************************************************\
* Name   : printWarning
* Purpose: output warning on console and write to log file
* Input  : text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printWarning(const char *text, ...)
{
  String  line;
  va_list arguments;

  assert(text != NULL);

  if (verboseFlag)
  {
    line = String_new();
    va_start(arguments,text);
    String_appendCString(line,"Warning: ");
    String_appendVFormat(line,text,arguments);
    String_appendChar(line,'\n');
    va_end(arguments);
    (void)fwrite(String_cString(line),1,String_length(line),stderr);

    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printError
* Purpose: print error message on stderr and write to log file
*          text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printError(const char *text, ...)
{
  String  line;
  va_list arguments;

  assert(text != NULL);

  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);
  (void)fwrite(String_cString(line),1,String_length(line),stderr);

  String_delete(line);
}

//TODO: remove, not used
#if 0
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
      fprintf(stdout,"%c\b",WHEEL[count%4]); count++; fflush(stdout);
    }
    lastTimestamp = timestamp;
  }

  return 0;
}
#endif

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
* Purpose: print percentage value (if verbose)
* Input  : n     - value
*          count - max. value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printPercentage(ulong n, ulong count)
{
  double percentage;

  if (verboseFlag)
  {
    percentage = (count > 0) ? ((double)n*1000.0)/((double)count*10.0) : 0.0;
    if (percentage > 100.0) percentage = 100.0;

    fprintf(stdout,"%5.1lf%%\b\b\b\b\b\b",percentage); fflush(stdout);
  }
}

/***********************************************************************\
* Name   : clearPercentage
* Purpose: clear percentage value (if verbose)
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearPercentage(void)
{
  if (verboseFlag)
  {
    fprintf(stdout,"      \b\b\b\b\b\b"); fflush(stdout);
  }
}

/***********************************************************************\
* Name   : openDatabase
* Purpose: open database
* Input  : databaseURI - database URI string
*          createFlag  - TRUE to create database if it does not exists
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors openDatabase(DatabaseHandle *databaseHandle, const char *databaseURI, bool createFlag)
{
  DatabaseSpecifier databaseSpecifier;
  bool              validURIPrefix;
  String            printableDataseURI;
  DatabaseOpenModes openMode;
  Errors            error;

  // parse URI and fill int default values
  Database_parseSpecifier(&databaseSpecifier,databaseURI,INDEX_DEFAULT_DATABASE_NAME,&validURIPrefix);
  if (!validURIPrefix)
  {
    printWarning("No valid prefix in database URI");
  }
  switch (databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      break;
    case DATABASE_TYPE_MARIADB:
      #if defined(HAVE_MARIADB)
        if (String_isEmpty(databaseSpecifier.mysql.databaseName))
        {
          String_setCString(databaseSpecifier.mysql.databaseName,DEFAULT_DATABASE_NAME);
        }
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
    case DATABASE_TYPE_POSTGRESQL:
      #if defined(HAVE_POSTGRESQL)
        if (String_isEmpty(databaseSpecifier.postgresql.databaseName))
        {
          String_setCString(databaseSpecifier.postgresql.databaseName,DEFAULT_DATABASE_NAME);
        }
      #else /* HAVE_POSTGRESQL */
      #endif /* HAVE_POSTGRESQL */
      break;
  }
  printableDataseURI = Database_getPrintableName(String_new(),&databaseSpecifier);

  // open database
  printInfo("Open database '%s'...",String_cString(printableDataseURI));
  openMode = (createFlag)
               ? DATABASE_OPEN_MODE_FORCE_CREATE
               : DATABASE_OPEN_MODE_READWRITE;
  openMode |= DATABASE_OPEN_MODE_AUX;
  error = Database_open(databaseHandle,
                        &databaseSpecifier,
                        openMode,
                        WAIT_FOREVER
                       );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("cannot open database '%s' (error: %s)!",String_cString(printableDataseURI),Error_getText(error));
    String_delete(printableDataseURI);
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }
  printInfo("OK  \n");

  // free resources
  String_delete(printableDataseURI);
  Database_doneSpecifier(&databaseSpecifier);

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
  printInfo("Close database...");
  Database_close(databaseHandle);
  printInfo("OK\n");
}

/***********************************************************************\
* Name   : dropTables
* Purpose: drop all tables
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropTables(DatabaseHandle *databaseHandle, bool quietFlag)
{
  Errors error;

  if (!quietFlag) printInfo("Drop tables...");
  error = Database_dropTables(databaseHandle);
  if (error == ERROR_NONE)
  {
    if (!quietFlag) printInfo("OK\n");
  }
  else
  {
    if (!quietFlag) printInfo("FAIL (error: %s)\n",Error_getText(error));
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : dropViews
* Purpose: drop all views
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropViews(DatabaseHandle *databaseHandle, bool quietFlag)
{
  Errors error;

  if (!quietFlag) printInfo("Drop views...");
  error = Database_dropViews(databaseHandle);
  if (error == ERROR_NONE)
  {
    if (!quietFlag) printInfo("OK\n");
  }
  else
  {
    if (!quietFlag) printInfo("FAIL (error: %s)\n",Error_getText(error));
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : dropIndices
* Purpose: drop all indices
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropIndices(DatabaseHandle *databaseHandle, bool quietFlag)
{
  Errors error;

  if (!quietFlag) printInfo("Drop indizes...");
  error = ERROR_UNKNOWN;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop indices
    error = Database_dropIndices(databaseHandle);
    if (Database_getType(databaseHandle) == DATABASE_TYPE_SQLITE3)
    {
      // drop FTS indizes
      if (error == ERROR_NONE)
      {
        error = Database_dropTable(databaseHandle,
                                   "FTS_storages"
                                  );
      }
      if (error == ERROR_NONE)
      {
        error = Database_dropTable(databaseHandle,
                                   "FTS_entries"
                                  );
      }
    }

    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
  }
  if (error == ERROR_NONE)
  {
    if (!quietFlag) printInfo("OK\n");
  }
  else
  {
    if (!quietFlag) printInfo("FAIL (error: %s)!\n",Error_getText(error));
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : dropTriggers
* Purpose: drop all triggers
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropTriggers(DatabaseHandle *databaseHandle, bool quietFlag)
{
  Errors error;

  if (!quietFlag) printInfo("Drop triggers...");
  error = Database_dropTriggers(databaseHandle);
  if (error == ERROR_NONE)
  {
    if (!quietFlag) printInfo("OK\n");
  }
  else
  {
    if (!quietFlag) printInfo("FAIL (error: %s)\n",Error_getText(error));
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : createTablesViewsIndicesTriggers
* Purpose: create database with tables/views/indices/triggers
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createTablesViewsIndicesTriggers(DatabaseHandle *databaseHandle)
{
  Errors     error;
  const char *indexDefinition;

  // create tables, triggers
  if (forceFlag)
  {
    dropTriggers(databaseHandle,FALSE);
    dropIndices(databaseHandle,FALSE);
    dropViews(databaseHandle,FALSE);
    dropTables(databaseHandle,FALSE);
  }

  printInfo("Create tables/views/indices/triggers...");
  error = ERROR_NONE;
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITIONS[Database_getType(databaseHandle)],
                             indexDefinition,
                             error == ERROR_NONE
                            )
  {
    error = Database_execute(databaseHandle,
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             indexDefinition,
                             DATABASE_PARAMETERS
                             (
                             )
                            );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create database fail (error: %s)!",Error_getText(error));
    return error;
  }
  printInfo("OK  \n");

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : logImportProgress
* Purpose: log import progress
* Input  : format - log format string (can be NULL)
*          ...    - optional arguments for log format string
* Output : -
* Return : -
* Notes  : increment step counter for each call!
\***********************************************************************/

LOCAL void logImportProgress(const char *format, ...)
{
  va_list arguments;

  va_start(arguments,format);
  vfprintf(stdout,format,arguments); fflush(stdout);
  va_end(arguments);
}

/***********************************************************************\
* Name   : getCopyPauseCallback
* Purpose: get pause callback
* Input  : -
* Output : -
* Return : pause callback function or NULL
* Notes  : -
\***********************************************************************/

LOCAL_INLINE DatabaseCopyPauseCallbackFunction getCopyPauseCallback(void)
{
  return NULL;
}

LOCAL Errors initEntity(DatabaseHandle *oldDatabaseHandle,
                        DatabaseHandle *newDatabaseHandle,
                        DatabaseId     storageId,
                        DatabaseId     *entityId
                       )
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors     error;
  DatabaseId uuidId;

  assert(oldDatabaseHandle != NULL);
  assert(newDatabaseHandle != NULL);
  assert(entityId != NULL);

  if (   (Database_getString(oldDatabaseHandle,
                             jobUUID,
                             "storages",
                             "jobUUID",
                             "id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(storageId)
                             )
                            ) != ERROR_NONE
         )
      || (Database_getId(oldDatabaseHandle,
                         entityId,
                         "entities",
                         "id",
                         "jobUUID=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_STRING(jobUUID)
                         )
                        ) != ERROR_NONE
         )
     )
  {
    Misc_getUUID(jobUUID);
    error = Database_insert(newDatabaseHandle,
                            NULL,  // insertRowId
                            "uuids",
                            DATABASE_FLAG_IGNORE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_STRING("jobUUID", jobUUID)
                            ),
                            DATABASE_COLUMNS_NONE,
                            DATABASE_FILTERS_NONE
                           );

    // get uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(newDatabaseHandle,
                             &uuidId,
                             "uuids",
                             "id",
                             "jobUUID=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_STRING(jobUUID)
                             )
                            );
    }

    // create entity
    if (error == ERROR_NONE)
    {
      error = Database_insert(newDatabaseHandle,
                              entityId,
                              "entities",
                              DATABASE_FLAG_IGNORE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("uuidId",      uuidId),
                                DATABASE_VALUE_STRING("jobUUID",     jobUUID),
                                DATABASE_VALUE_UINT  ("type",        ARCHIVE_TYPE_FULL),
                                DATABASE_VALUE_UINT  ("lockedCount", 1)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
    }
  }
  else
  {
// TODO: error message UUID not found
    error = ERROR_DATABASE_INVALID_INDEX;
  }

  return error;
}

LOCAL Errors unlockEntity(DatabaseHandle *databaseHandle,
                          DatabaseId     entityId
                         )
{
  assert(databaseHandle != NULL);

  return Database_update(databaseHandle,
                         NULL,  // changedRowCount
                         "entities",
                         DATABASE_FLAG_NONE,
                         DATABASE_VALUES
                         (
                           DATABASE_VALUE("lockedCount","lockedCount-1")
                         ),
                         "id=? AND lockedCount>0",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(entityId)
                         )
                        );
}

LOCAL char outputProgressBuffer[64];
LOCAL uint outputProgressBufferLength;

LOCAL void formatSubProgressInfo(uint  progress,
                                 ulong estimatedTotalTime,
                                 ulong estimatedRestTime,
                                 void  *userData
                                )
{
  UNUSED_VARIABLE(estimatedTotalTime);
  UNUSED_VARIABLE(userData);

  if (estimatedRestTime < 99*60*60)
  {
    stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                 "%5.1f%% %2dh:%02dmin:%02ds",
                 (float)progress/10.0,
                 estimatedRestTime/(60*60),
                 estimatedRestTime%(60*60)/60,
                 estimatedRestTime%60
                );
  }
  else
  {
    stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                 "%5.1f%% --h:--min:--s",
                 (float)progress/10.0
                );
  }
}

/***********************************************************************\
* Name   : outputProgressInit
* Purpose: output progress text
* Input  : text     - text
*          maxSteps - nax. number of steps (not used)
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressInit(const char *text,
                              uint64     maxSteps,
                              void       *userData
                             )
{
  UNUSED_VARIABLE(maxSteps);
  UNUSED_VARIABLE(userData);

  fwrite(text,1,stringLength(text),stdout);
  fflush(stdout);
}

/***********************************************************************\
* Name   : outputProgressInfo
* Purpose: output progress info on console
* Input  : progress           - progres [%%]
*          estimatedTotalTime - estimated total time [s]
*          estimatedRestTime  - estimated rest time [s]
*          userData           - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressInfo(uint  progress,
                              ulong estimatedTotalTime,
                              ulong estimatedRestTime,
                              void  *userData
                             )
{
  const char *WHEEL = "|/-\\";
  static uint wheelIndex = 0;

  UNUSED_VARIABLE(estimatedTotalTime);
  UNUSED_VARIABLE(userData);

  if (estimatedRestTime < 999*60*60)
  {
    stringFormatAppend(outputProgressBuffer,sizeof(outputProgressBuffer),
                       " / %5.1f%% %3dh:%02dmin:%02ds %c",
                       (float)progress/10.0,
                       estimatedRestTime/(60*60),
                       estimatedRestTime%(60*60)/60,
                       estimatedRestTime%60,
                       WHEEL[wheelIndex]
                      );
  }
  else
  {
    stringFormatAppend(outputProgressBuffer,sizeof(outputProgressBuffer),
                       " / %5.1f%% ---h:--min:--s %c",
                       (float)progress/10.0,
                       WHEEL[wheelIndex]
                      );
  }
  outputProgressBufferLength = stringLength(outputProgressBuffer);

  fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout);

  stringFill(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,'\b');
  fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout);

  fflush(stdout);

  wheelIndex = (wheelIndex+1) % 4;
}

/***********************************************************************\
* Name   : outputProgressDone
* Purpose: done progress output
* Input  : totalTime - total time [s]
*          userData  - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputProgressDone(ulong totalTime,
                              void  *userData
                             )
{
  UNUSED_VARIABLE(userData);

  stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
               "%2dh:%02dmin:%02ds",
               totalTime/(60*60),
               totalTime%(60*60)/60,
               totalTime%60
              );
  stringFillAppend(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,' ');

  fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout);

  stringFill(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,'\b');
  fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout);

  fwrite("\n",1,1,stdout);

  fflush(stdout);
}

#include "index/index_current.c"

/***********************************************************************\
* Name   : importIntoDatabase
* Purpose: import database
* Input  : databaseHandle - database handle
*          uir            - database URI
* Output :
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

ProgressInfo *ppp;

LOCAL Errors importIntoDatabase(DatabaseHandle *databaseHandle, const char *databaseURI)
{
  DatabaseSpecifier databaseSpecifier;
  String            printableDatabaseURI;
  ulong             maxSteps;
  ProgressInfo      progressInfo;
  Errors            error;
  DatabaseHandle    oldDatabaseHandle;

  // parse URI and fill in default values
  Database_parseSpecifier(&databaseSpecifier,databaseURI,INDEX_DEFAULT_DATABASE_NAME,NULL);
  switch (databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      break;
    case DATABASE_TYPE_MARIADB:
      #if defined(HAVE_MARIADB)
      #else /* HAVE_MARIADB */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_MARIADB */
      break;
    case DATABASE_TYPE_POSTGRESQL:
      #if defined(HAVE_POSTGRESQL)
      #else /* HAVE_POSTGRESQL */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_POSTGRESQL */
      break;
  }
  printableDatabaseURI = Database_getPrintableName(String_new(),&databaseSpecifier);

  printInfo("Import database '%s':\n",String_cString(printableDatabaseURI));

  error = Database_open(&oldDatabaseHandle,
                        &databaseSpecifier,
                        DATABASE_OPEN_MODE_READ,
                        WAIT_FOREVER
                       );
  if (error != ERROR_NONE)
  {
    String_delete(printableDatabaseURI);
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  maxSteps = 0LL;
// TODO:
#if 0
  switch (indexVersion)
  {
    case 1:
      maxSteps = getImportStepsVersion1(&oldIndexHandle,2,2,2);
      break;
    case 2:
      maxSteps = getImportStepsVersion2(&oldIndexHandle,2,2,2);
      break;
    case 3:
      maxSteps = getImportStepsVersion3(&oldIndexHandle,2,2,2);
      break;
    case 4:
      maxSteps = getImportStepsVersion4(&oldIndexHandle,2,2,2);
      break;
    case 5:
      maxSteps = getImportStepsVersion5(&oldIndexHandle,2,2,2);
      break;
    case 6:
      maxSteps = getImportStepsVersion6(&oldIndexHandle,2,2,2);
      break;
    case INDEX_CONST_VERSION:
      maxSteps = getImportStepsVersion7(&oldDatabaseHandle);
      break;
    default:
      // unknown version if index
      error = ERROR_DATABASE_VERSION_UNKNOWN;
      break;
  }
#else
maxSteps = getImportStepsVersion7(&oldDatabaseHandle);
#endif
ppp=&progressInfo;
  ProgressInfo_init(&progressInfo,
                    NULL,  // parentProgressInfo
                    0,  // filterWindowSize
                    500,
                    maxSteps,
                    CALLBACK_(NULL,NULL),  // progresInitFunction
                    CALLBACK_(outputProgressDone,NULL),
                    CALLBACK_(outputProgressInfo,NULL),
                    NULL
                   );

  error = importIndexVersion7XXX(&oldDatabaseHandle,databaseHandle,&progressInfo);
  if (error != ERROR_NONE)
  {
    printError("Import database fail: %s!\n",Error_getText(error));
  }

  // free resources
  ProgressInfo_done(&progressInfo);
  Database_close(&oldDatabaseHandle);
  String_delete(printableDatabaseURI);
  Database_doneSpecifier(&databaseSpecifier);

  return error;
}

/***********************************************************************\
* Name   : checkIntegrity
* Purpose: check database integrity
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void checkIntegrity(DatabaseHandle *databaseHandle)
{
  Errors error;

  printInfo("Check integrity:\n");

  printInfo("  quick...");
  error = Database_check(databaseHandle, DATABASE_CHECK_QUICK);
  if (error == ERROR_NONE)
  {
    printInfo("OK\n");
  }
  else
  {
    printInfo("FAIL!\n");
    printError("quick integrity check fail (error: %s)!\n",Error_getText(error));
  }

  printInfo("  foreign key...");
  error = Database_check(databaseHandle, DATABASE_CHECK_KEYS);
  if (error == ERROR_NONE)
  {
    printInfo("OK\n");
  }
  else
  {
    printInfo("FAIL!\n");
    printError("foreign key check fail (error: %s)!\n",Error_getText(error));
  }

  printInfo("  full...");
  error = Database_check(databaseHandle, DATABASE_CHECK_FULL);
  if (error == ERROR_NONE)
  {
    printInfo("OK\n");
  }
  else
  {
    printInfo("FAIL!\n");
    printError("integrity check fail (error: %s)!\n",Error_getText(error));
  }
}

/***********************************************************************\
* Name   : checkOrphanedEntries
* Purpose: check database orphaned entries
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void checkOrphanedEntries(DatabaseHandle *databaseHandle)
{
  Errors error;
  ulong  totalCount;
  int64  n;

  totalCount = 0LL;

  printInfo("Check orphaned:\n");

  // check entries without fragments
  printInfo("  file entries without fragments...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "fileEntries",
                            "COUNT(id)",
                            "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 1)",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  image entries without fragments...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "imageEntries",
                            "COUNT(id)",
                            "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 1)",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (uint64)n;
  printInfo("  hardlink entries without fragments...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "hardlinkEntries",
                            "COUNT(id)",
                            "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 1)",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM fileEntries WHERE fileEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without image entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM imageEntries WHERE imageEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without directory entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM directoryEntries WHERE directoryEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_DIRECTORY)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without link entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM linkEntries WHERE linkEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_LINK)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without hardlink entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM hardlinkEntries WHERE hardlinkEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without special entry...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "entries",
                            "COUNT(id)",
                            "    entries.type=? \
                             AND NOT EXISTS(SELECT id FROM specialEntries WHERE specialEntries.entryId=entries.id LIMIT 1) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_UINT(INDEX_CONST_TYPE_SPECIAL)
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check storages without name
  printInfo("  storages without name...");
  error = Database_getInt64(databaseHandle,
                            &n,
                            "storages",
                            "COUNT(id)",
                            "name IS NULL OR name=''",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      // check FTS entries without entry
      printInfo("  FTS entries without entry...");
      error = Database_getInt64(databaseHandle,
                                &n,
                                "FTS_entries",
                                "COUNT(entryId)",
                                "NOT EXISTS(SELECT id FROM entries WHERE entries.id=FTS_entries.entryId LIMIT 1)",
                                DATABASE_FILTERS
                                (
                                ),
                                NULL  // group
                               );
      if (error == ERROR_NONE)
      {
        printInfo("%"PRIi64"\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError("orphaned check fail (error: %s)!\n",Error_getText(error));
      }
      totalCount += (ulong)n;

      // check FTS storages without storage
      printInfo("  FTS storages without storage...");
      error = Database_getInt64(databaseHandle,
                                &n,
                                "FTS_storages",
                                "COUNT(storageId)",
                                "NOT EXISTS(SELECT id FROM storages WHERE storages.id=FTS_storages.storageId LIMIT 1)",
                                DATABASE_FILTERS
                                (
                                ),
                                NULL  // group
                               );
      if (error == ERROR_NONE)
      {
        printInfo("%"PRIi64"\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError("orphaned check fail (error: %s)!\n",Error_getText(error));
      }
      totalCount += (ulong)n;

      // check newest entries without entry
      printInfo("  newest entries without entry...");
      error = Database_getInt64(databaseHandle,
                                &n,
                                "entriesNewest",
                                "COUNT(id)",
                                "NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 1)",
                                DATABASE_FILTERS
                                (
                                ),
                                NULL  // group
                               );
      if (error == ERROR_NONE)
      {
        printInfo("%"PRIi64"\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError("orphaned check fail (error: %s)!\n",Error_getText(error));
      }
      totalCount += (ulong)n;
      break;
    case DATABASE_TYPE_MARIADB:
      // check newest entries without entry
      printInfo("  newest entries without entry...");
      error = Database_getInt64(databaseHandle,
                                &n,
                                "entriesNewest",
                                "COUNT(id)",
                                "NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 1)",
                                DATABASE_FILTERS
                                (
                                ),
                                NULL  // group
                               );
      if (error == ERROR_NONE)
      {
        printInfo("%"PRIi64"\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError("orphaned check fail (error: %s)!\n",Error_getText(error));
      }
      totalCount += (ulong)n;
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // check newest entries without entry
      printInfo("  newest entries without entry...");
      error = Database_getInt64(databaseHandle,
                                &n,
                                "entriesNewest",
                                "COUNT(id)",
                                "NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 1)",
                                DATABASE_FILTERS
                                (
                                ),
                                NULL  // group
                               );
      if (error == ERROR_NONE)
      {
        printInfo("%"PRIi64"\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError("orphaned check fail (error: %s)!\n",Error_getText(error));
      }
      totalCount += (ulong)n;
      break;
  }

  if (totalCount > 0LL)
  {
    printWarning("Found %lu orphaned entries. Clean is recommented",totalCount);
  }

  // free resources
}

/***********************************************************************\
* Name   : checkDuplicates
* Purpose: check database duplicates
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void checkDuplicates(DatabaseHandle *databaseHandle)
{
  String name;
  Errors error;
  ulong  totalCount;
  ulong  n;

  name       = String_new();
  totalCount = 0LL;

  printInfo("Check duplicates:\n");

  // check duplicate storages
  printInfo("  storages...");
  n = 0L;
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         if (String_equals(name,values[0].string))
                         {
                           n++;
                         }
                         String_set(name,values[0].string);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_STRING("name")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       "name",
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error == ERROR_NONE)
  {
    printInfo("%"PRIi64"\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("duplicates check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += n;

  if (totalCount > 0LL)
  {
    printWarning("Found %lu duplicate entries. Clean is recommented",totalCount);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : optimizeDatabase
* Purpose: optimize database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void optimizeDatabase(DatabaseHandle *databaseHandle)
{
  StringList         tableNameList;
  Errors             error;
  ulong              n;
  StringListIterator stringListIterator;
  ConstString        name;

  // init variables

  printInfo("Optimize:\n");

  printInfo("  Tables...");
  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("get tables fail (error: %s)!",Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  n = 0;
  STRINGLIST_ITERATE(&tableNameList,stringListIterator,name)
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_execute(databaseHandle,
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 "ANALYZE ?",
                                 DATABASE_PARAMETERS
                                 (
                                   DATABASE_PARAMETER_STRING(name)
                                 )
                                );
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        break;
    }
    if (error != ERROR_NONE)
    {
      break;
    }

    n++;
    printPercentage(n,StringList_count(&tableNameList));
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("optimize table '%s' fail (error: %s)!",String_cString(name),Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  printInfo("OK  \n");
  StringList_done(&tableNameList);

  printInfo("  Indizes...");
  StringList_clear(&tableNameList);
  error = Database_getIndexList(&tableNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("get indizes fail (error: %s)!",Error_getText(error));
    return;
  }
  n = 0;
  STRINGLIST_ITERATE(&tableNameList,stringListIterator,name)
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        error = Database_execute(databaseHandle,
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 "ANALYZE ?",
                                 DATABASE_PARAMETERS
                                 (
                                   DATABASE_PARAMETER_STRING(name)
                                 )
                                );
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        break;
    }
    if (error != ERROR_NONE)
    {
      break;
    }

    n++;
    printPercentage(n,StringList_count(&tableNameList));
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("optimize index '%s' fail (error: %s)!",String_cString(name),Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  printInfo("OK  \n");
  StringList_done(&tableNameList);

  // free resources
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
  Errors     error;
  const char *triggerName;
  const char *triggerDefinition;

  printInfo("Create triggers...");

  // delete all existing triggers
  error = ERROR_NONE;
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_TRIGGER_NAMES[Database_getType(databaseHandle)], triggerName)
  {
    error = Database_dropTrigger(databaseHandle,
                                 triggerName
                                );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create triggers fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // create new triggeres
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_TRIGGERS[Database_getType(databaseHandle)], triggerDefinition, error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             triggerDefinition,
                             DATABASE_PARAMETERS
                             (
                             )
                            );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create triggers fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  printInfo("OK\n");
}

/***********************************************************************\
* Name   : printTableNames
* Purpose: print table names
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printTableNames(DatabaseHandle *databaseHandle)
{
  Errors             error;
  StringList         tableNameList;
  StringListIterator stringListIterator;
  ConstString        tableName;

  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle);
  if (error != ERROR_NONE)
  {
    printError("cannot get table names (error: %s)!",Error_getText(error));
    StringList_done(&tableNameList);
    exit(EXITCODE_FAIL);
  }
  STRINGLIST_ITERATE(&tableNameList,stringListIterator,tableName)
  {
    printf("%s\n",String_cString(tableName));
  }
  StringList_done(&tableNameList);
}

/***********************************************************************\
* Name   : printIndexNames
* Purpose: print index names
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printIndexNames(DatabaseHandle *databaseHandle)
{
  Errors             error;
  StringList         indexNameList;
  StringListIterator stringListIterator;
  ConstString        indexName;

  error = Database_getIndexList(&indexNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    printError("cannot get index names (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  STRINGLIST_ITERATE(&indexNameList,stringListIterator,indexName)
  {
    printf("%s\n",String_cString(indexName));
  }
  StringList_done(&indexNameList);
}

/***********************************************************************\
* Name   : printTriggerNames
* Purpose: print trigger names
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printTriggerNames(DatabaseHandle *databaseHandle)
{
  Errors             error;
  StringList         triggerNameList;
  StringListIterator stringListIterator;
  ConstString        triggerName;

  StringList_init(&triggerNameList);
  error = Database_getTriggerList(&triggerNameList,databaseHandle);
  if (error != ERROR_NONE)
  {
    StringList_done(&triggerNameList);
    printError("cannot get trigger names (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  STRINGLIST_ITERATE(&triggerNameList,stringListIterator,triggerName)
  {
    printf("%s\n",String_cString(triggerName));
  }
  StringList_done(&triggerNameList);
}

/***********************************************************************\
* Name   : createIndizes
* Purpose: create all indizes
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createIndizes(DatabaseHandle *databaseHandle)
{
  Errors error;

  /* manual forced delete
  PRAGMA writable_schema = ON;
  DELETE FROM sqlite_master WHERE type = 'table' AND name = 'tablename';
  */

  printInfo("Create indizes:\n");

  error = ERROR_NONE;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop all existing indizes
    printInfo("  Discard indizes...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          StringList         indexNameList;
          StringListIterator iteratorIndexName;
          String             indexName;

          StringList_init(&indexNameList);
          error = Database_getIndexList(&indexNameList,databaseHandle,NULL);
          STRINGLIST_ITERATEX(&indexNameList,iteratorIndexName,indexName,error == ERROR_NONE)
          {
            error = Database_dropIndex(databaseHandle,
                                       String_cString(indexName)
                                      );
          }
          StringList_done(&indexNameList);
        }
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        break;
    }
    printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }

    // create new indizes (if not exists)
    printInfo("  Create indizes...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          const char *indexDefinition;

          INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_INDICES[Database_getType(databaseHandle)], indexDefinition, error == ERROR_NONE)
          {
            error = Database_execute(databaseHandle,
                                     NULL,  // changedRowCount
                                     DATABASE_FLAG_NONE,
                                     indexDefinition,
                                     DATABASE_PARAMETERS
                                     (
                                     )
                                    );
          }
        }
        break;
      case DATABASE_TYPE_MARIADB:
        #if defined(HAVE_MARIADB)
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      case DATABASE_TYPE_POSTGRESQL:
        #if defined(HAVE_POSTGRESQL)
        #else /* HAVE_POSTGRESQL */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_POSTGRESQL */
        break;
    }
    printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
  }
  if (error != ERROR_NONE)
  {
    printError("create indizes fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : createFTSIndizes
* Purpose: create FTS indizes
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createFTSIndizes(DatabaseHandle *databaseHandle)
{
  Errors error;

  printInfo("Create FTS indizes:\n");

  error = ERROR_NONE;

  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop FTS indizes
    printInfo("  Discard FTS indizes...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          const char *name;

          INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_FTS_TABLE_NAMES[Database_getType(databaseHandle)], name, error == ERROR_NONE)
          {
            error = Database_dropTable(databaseHandle,
                                       name
                                      );
          }
        }
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        break;
    }
    printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }

    // create new FTS tables (if not exists)
    printInfo("  Create FTS indizes...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          const char *indexDefinition;

          INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_FTS_TABLES_SQLITE, indexDefinition, error == ERROR_NONE)
          {
            error = Database_execute(databaseHandle,
                                     NULL,  // changedRowCount
                                     DATABASE_FLAG_NONE,
                                     indexDefinition,
                                     DATABASE_PARAMETERS
                                     (
                                     )
                                    );
          }
          if (error == ERROR_NONE)
          {
            error = Database_insertSelect(databaseHandle,
                                          NULL,  // changedRowCount
                                          "FTS_storages",
                                          DATABASE_FLAG_IGNORE,
                                          DATABASE_COLUMNS
                                          (
                                            DATABASE_COLUMN_KEY   ("storageId"),
                                            DATABASE_COLUMN_STRING("name")
                                          ),
                                          DATABASE_TABLES
                                          (
                                            "storages"
                                          ),
                                          DATABASE_COLUMNS
                                          (
                                            DATABASE_COLUMN_KEY   ("id"),
                                            DATABASE_COLUMN_STRING("name")
                                          ),
                                          DATABASE_FILTERS_NONE,
                                          NULL,  // groupBy
                                          NULL,  // orderBy
                                          0LL,
                                          DATABASE_UNLIMITED
                                         );
          }
          if (error == ERROR_NONE)
          {
            error = Database_insertSelect(databaseHandle,
                                          NULL,  // changedRowCount
                                          "FTS_entries",
                                          DATABASE_FLAG_IGNORE,
                                          DATABASE_COLUMNS
                                          (
                                            DATABASE_COLUMN_KEY   ("entryId"),
                                            DATABASE_COLUMN_STRING("name")
                                          ),
                                          DATABASE_TABLES
                                          (
                                            "entries"
                                          ),
                                          DATABASE_COLUMNS
                                          (
                                            DATABASE_COLUMN_KEY   ("id"),
                                            DATABASE_COLUMN_STRING("name")
                                          ),
                                          DATABASE_FILTERS_NONE,
                                          NULL,  // groupBy
                                          NULL,  // orderBy
                                          0LL,
                                          DATABASE_UNLIMITED
                                         );
          }
        }
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        break;
    }
    printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
  }
  if (error != ERROR_NONE)
  {
    printError("create FTS indizes fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : reindex
* Purpose: re-create existing indizes
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void reindex(DatabaseHandle *databaseHandle)
{
  Errors error;

  printInfo("Reindex...");

  error = Database_reindex(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("reindex fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  printInfo("OK\n");
}

/***********************************************************************\
* Name   : addToNewest
* Purpose: add storage entries to newest entries (if newest)
* Input  : databaseHandle - database handle
*          storageId      - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addToNewest(DatabaseHandle *databaseHandle,
                         DatabaseId     storageId
                        )
{
  typedef struct EntryNode
  {
    LIST_NODE_HEADER(struct EntryNode);

    DatabaseId entryId;
    DatabaseId uuidId;
    DatabaseId entityId;
    IndexTypes indexType;
    String     name;
    uint64     timeLastChanged;
    uint32     userId;
    uint32     groupId;
    uint32     permission;
    uint64     size;

    struct
    {
      DatabaseId entryId;
      uint64     timeLastChanged;
    } newest;
  } EntryNode;

  typedef struct
  {
    LIST_HEADER(EntryNode);
  } EntryList;

  /***********************************************************************\
  * Name   : freeEntryNode
  * Purpose: free entry node
  * Input  : entryNode - entry node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode);
  void freeEntryNode(EntryNode *entryNode)
  {
    assert(entryNode != NULL);

    String_delete(entryNode->name);
  }

  EntryList   entryList;
  DatabaseId  entryId;
  DatabaseId  uuidId;
  DatabaseId  entityId;
  uint        indexType;
  ConstString entryName;
  uint64      timeLastChanged;
  uint32      userId;
  uint32      groupId;
  uint32      permission;
  uint64      size;
  Errors      error;
  EntryNode   *entryNode;

  assert(databaseHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  error     = ERROR_NONE;

  // get entries info to add
  if (error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 10);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryId         = values[0].id;
                           uuidId          = values[1].id;
                           entityId        = values[2].id;
                           indexType       = values[3].id;
                           entryName       = values[4].string;
                           timeLastChanged = values[5].dateTime;
                           userId          = values[6].u;
                           groupId         = values[7].u;
                           permission      = values[8].u;
                           size            = values[9].u64;
                           assert(entryId != DATABASE_ID_NONE);

                           entryNode = LIST_NEW_NODE(EntryNode);
                           if (entryNode == NULL)
                           {
                             HALT_INSUFFICIENT_MEMORY();
                           }

                           entryNode->entryId                = entryId;
                           entryNode->uuidId                 = uuidId;
                           entryNode->entityId               = entityId;
                           entryNode->indexType              = (IndexTypes)indexType;
                           entryNode->name                   = String_duplicate(entryName);
                           entryNode->timeLastChanged        = timeLastChanged;
                           entryNode->userId                 = (uint32)userId;
                           entryNode->groupId                = (uint32)groupId;
                           entryNode->permission             = (uint32)permission;
                           entryNode->size                   = (uint64)size;
                           entryNode->newest.entryId         = DATABASE_ID_NONE;
                           entryNode->newest.timeLastChanged = 0LL;

                           List_append(&entryList,entryNode);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entryFragments \
                             LEFT JOIN storages ON storages.id=entryFragments.storageId \
                             LEFT JOIN entries ON entries.id=entryFragments.entryId \
                           ",
                           "directoryEntries \
                             LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                             LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                           ",
                           "linkEntries \
                             LEFT JOIN storages ON storages.id=linkEntries.storageId \
                             LEFT JOIN entries ON entries.id=linkEntries.entryId \
                           ",
                           "specialEntries \
                             LEFT JOIN storages ON storages.id=specialEntries.storageId \
                             LEFT JOIN entries ON entries.id=specialEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("entries.id"),
                           DATABASE_COLUMN_KEY     ("entries.uuidId"),
                           DATABASE_COLUMN_KEY     ("entries.entityId"),
                           DATABASE_COLUMN_UINT    ("entries.type"),
                           DATABASE_COLUMN_STRING  ("entries.name"),
                           DATABASE_COLUMN_DATETIME("entries.timeLastChanged","timeLastChanged"),
                           DATABASE_COLUMN_UINT    ("entries.userId"),
                           DATABASE_COLUMN_UINT    ("entries.groupId"),
                           DATABASE_COLUMN_UINT    ("entries.permission"),
                           DATABASE_COLUMN_UINT64  ("entries.size")
                         ),
                         "storageId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(storageId)
                         ),
                         NULL,  // groupBy
                         "timeLastChanged DESC",
                         0LL,
                         DATABASE_UNLIMITED
                        );
  }

  // find newest entries for entries to add
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryNode->newest.entryId         = values[0].id;
                           entryNode->newest.timeLastChanged = values[1].dateTime;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entryFragments \
                             LEFT JOIN storages ON storages.id=entryFragments.storageId \
                             LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                           ",
                           "directoryEntries \
                             LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                             LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                           ",
                           "linkEntries \
                             LEFT JOIN storages ON storages.id=linkEntries.storageId \
                             LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                           ",
                           "specialEntries \
                             LEFT JOIN storages ON storages.id=specialEntries.storageId \
                             LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("entriesNewest.id"),
                           DATABASE_COLUMN_DATETIME("entriesNewest.timeLastChanged","timeLastChanged")
                         ),
                         "    storages.deletedFlag!=TRUE \
                          AND entriesNewest.name=? \
                          AND entriesNewest.id IS NOT NULL \
                         ",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_STRING(entryNode->name)
                         ),
                         NULL,  // groupBy
                         "timeLastChanged DESC",
                         0LL,
                         1LL
                        );
  }

  // update/add entries to newest entries
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    if (entryNode->timeLastChanged > entryNode->newest.timeLastChanged)
    {
      if (entryNode->newest.entryId != DATABASE_ID_NONE)
      {
        error = Database_update(databaseHandle,
                                NULL,  // changedRowCount
                                "entriesNewest",
                                DATABASE_FLAG_REPLACE,
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_KEY     ("entryId",         entryNode->entryId),
                                  DATABASE_VALUE_KEY     ("uuidId",          entryNode->uuidId),
                                  DATABASE_VALUE_KEY     ("entityId",        entryNode->entityId),
                                  DATABASE_VALUE_UINT    ("type",            entryNode->indexType),
                                  DATABASE_VALUE_STRING  ("name",            entryNode->name),
                                  DATABASE_VALUE_DATETIME("timeLastChanged", entryNode->timeLastChanged),
                                  DATABASE_VALUE_UINT    ("userId",          entryNode->userId),
                                  DATABASE_VALUE_UINT    ("groupId",         entryNode->groupId),
                                  DATABASE_VALUE_UINT    ("permission",      entryNode->permission),
                                  DATABASE_VALUE_UINT64  ("size",            entryNode->size)
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(entryNode->newest.entryId)
                                )
                               );
      }
      else
      {
        error = Database_insert(databaseHandle,
                                NULL,  // insertRowId
                                "entriesNewest",
                                DATABASE_FLAG_REPLACE,
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_KEY     ("entryId",         entryNode->entryId),
                                  DATABASE_VALUE_KEY     ("uuidId",          entryNode->uuidId),
                                  DATABASE_VALUE_KEY     ("entityId",        entryNode->entityId),
                                  DATABASE_VALUE_UINT    ("type",            entryNode->indexType),
                                  DATABASE_VALUE_STRING  ("name",            entryNode->name),
                                  DATABASE_VALUE_DATETIME("timeLastChanged", entryNode->timeLastChanged),
                                  DATABASE_VALUE_UINT    ("userId",          entryNode->userId),
                                  DATABASE_VALUE_UINT    ("groupId",         entryNode->groupId),
                                  DATABASE_VALUE_UINT    ("permission",      entryNode->permission),
                                  DATABASE_VALUE_UINT64  ("size",            entryNode->size)
                                ),
                                DATABASE_COLUMNS
                                (
                                  DATABASE_COLUMN_STRING("name")
                                ),
                                "entriesNewest.name=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_STRING(entryNode->name)
                                )
                               );
      }
    }
  }

  // free resources
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return error;
}

/***********************************************************************\
* Name   : removeFromNewest
* Purpose: remove storage entries from newest entries
* Input  : databaseHandle - database handle
*          storageId      - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeFromNewest(DatabaseHandle *databaseHandle,
                              DatabaseId     storageId
                             )
{
  typedef struct EntryNode
  {
    LIST_NODE_HEADER(struct EntryNode);

    DatabaseId entryId;
    String     name;

    struct
    {
      DatabaseId entryId;
      DatabaseId uuidId;
      DatabaseId entityId;
      IndexTypes indexType;
      uint64     timeLastChanged;
      uint32     userId;
      uint32     groupId;
      uint32     permission;
      uint64     size;
    } newest;
  } EntryNode;

  typedef struct
  {
    LIST_HEADER(EntryNode);
  } EntryList;

  /***********************************************************************\
  * Name   : freeEntryNode
  * Purpose: free entry node
  * Input  : entryNode - entry node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode);
  void freeEntryNode(EntryNode *entryNode)
  {
    assert(entryNode != NULL);

    String_delete(entryNode->name);
  }

  EntryList entryList;
  Errors    error;
  EntryNode *entryNode;

  assert(databaseHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  error = ERROR_NONE;

  // get entries info to remove
  if (error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryNode = LIST_NEW_NODE(EntryNode);
                           if (entryNode == NULL)
                           {
                             HALT_INSUFFICIENT_MEMORY();
                           }

                           entryNode->entryId        = values[0].id;
                           String_set(entryNode->name,values[1].string);
                           entryNode->newest.entryId = DATABASE_ID_NONE;

                           List_append(&entryList,entryNode);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entries.id"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "entryFragments.storageId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(storageId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryNode = LIST_NEW_NODE(EntryNode);
                           if (entryNode == NULL)
                           {
                             HALT_INSUFFICIENT_MEMORY();
                           }

                           entryNode->entryId        = values[0].id;
                           String_set(entryNode->name,values[1].string);
                           entryNode->newest.entryId = DATABASE_ID_NONE;

                           List_append(&entryList,entryNode);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entries.id"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "directoryEntries.storageId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(storageId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryNode = LIST_NEW_NODE(EntryNode);
                           if (entryNode == NULL)
                           {
                             HALT_INSUFFICIENT_MEMORY();
                           }

                           entryNode->entryId        = values[0].id;
                           String_set(entryNode->name,values[1].string);
                           entryNode->newest.entryId = DATABASE_ID_NONE;

                           List_append(&entryList,entryNode);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "linkEntries \
                               LEFT JOIN entries ON entries.id=linkEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entries.id"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "linkEntries.storageId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(storageId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           entryNode = LIST_NEW_NODE(EntryNode);
                           if (entryNode == NULL)
                           {
                             HALT_INSUFFICIENT_MEMORY();
                           }

                           entryNode->entryId        = values[0].id;
                           String_set(entryNode->name,values[1].string);
                           entryNode->newest.entryId = DATABASE_ID_NONE;

                           List_append(&entryList,entryNode);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entries.id"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "specialEntries.storageId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(storageId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
  }

  // find new newest entries for entries to remove
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    if ((entryNode->prev == NULL) || !String_equals(entryNode->prev->name,entryNode->name))
    {
      error = Database_get(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 9);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryNode->newest.entryId         = values[0].id;
                             entryNode->newest.uuidId          = values[1].id;
                             entryNode->newest.entityId        = values[2].id;
                             entryNode->newest.indexType       = (IndexTypes)values[3].u;
                             entryNode->newest.timeLastChanged = values[4].dateTime;
                             entryNode->newest.userId          = values[5].u;
                             entryNode->newest.groupId         = values[6].u;
                             entryNode->newest.permission      = values[7].u;
                             entryNode->newest.size            = values[8].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entryFragments \
                                LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                LEFT JOIN entries ON entries.id=entryFragments.entryId \
                             ",
                             "directoryEntries \
                                LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                             ",
                             "linkEntries \
                                LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                LEFT JOIN entries ON entries.id=linkEntries.entryId \
                             ",
                             "specialEntries \
                                LEFT JOIN storages ON storages.id=specialEntries.storageId \
                                LEFT JOIN entries ON entries.id=specialEntries.entryId \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_KEY     ("entries.uuidId"),
                             DATABASE_COLUMN_UINT    ("entries.type"),
                             DATABASE_COLUMN_DATETIME("entries.timeLastChanged"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission"),
                             DATABASE_COLUMN_UINT64  ("entries.size")
                           ),
                           "    storages.deletedFlag!=TRUE \
                            AND entries.name=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_STRING(entryNode->name)
                           ),
                           NULL,  // groupBy
                           "entries.timeLastChanged DESC",
                           0LL,
                           1LL
                          );
    }
  }

  // remove/update entries from newest entries
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    error = Database_delete(databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            "entryId=? \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryNode->entryId)
                            ),
                            0
                           );
    if (error != ERROR_NONE)
    {
      break;
    }

    if (entryNode->newest.entryId != DATABASE_ID_NONE)
    {
      error = Database_insert(databaseHandle,
                              NULL,  // insertRowId
                              "entriesNewest",
                              DATABASE_FLAG_REPLACE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_STRING("entryId",        entryNode->newest.entryId),
                                DATABASE_VALUE_KEY   ("uuidId",         entryNode->newest.uuidId),
                                DATABASE_VALUE_KEY   ("entityId",       entryNode->newest.entityId),
                                DATABASE_VALUE_UINT  ("type",           entryNode->newest.indexType),
                                DATABASE_VALUE_STRING("name",           entryNode->name),
                                DATABASE_VALUE_UINT64("timeLastChanged",entryNode->newest.timeLastChanged),
                                DATABASE_VALUE_UINT  ("userId",         entryNode->newest.userId),
                                DATABASE_VALUE_UINT  ("groupId",        entryNode->newest.groupId),
                                DATABASE_VALUE_UINT  ("permission",     entryNode->newest.permission),
                                DATABASE_VALUE_UINT64("size",           entryNode->newest.size)
                              ),
                              DATABASE_COLUMNS
                              (
                                DATABASE_COLUMN_STRING("name")
                              ),
                              "name=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_STRING(entryNode->name)
                              )
                             );
      if (error != ERROR_NONE)
      {
        break;
      }
    }
  }

  // free resources
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return error;
}

/***********************************************************************\
* Name   : createNewest
* Purpose: create newest data
* Input  : databaseHandle - database handle
*          storageIds     - database storage id array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createNewest(DatabaseHandle *databaseHandle, Array storageIds)
{
  Errors        error;
  uint          totalEntriesNewestCount;
  ulong         n,m;
  ArrayIterator arrayIterator;
  DatabaseId    storageId;

  // initialize variables
  error = ERROR_NONE;

  if (Array_isEmpty(&storageIds))
  {
    printInfo("Collect data for newest entries...");

    // get storage ids
    error = Database_getIds(databaseHandle,
                            &storageIds,
                            "storages",
                            "id",
                            "deletedFlag!=TRUE",
                            DATABASE_FILTERS
                            (
                            )
                           );
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("collect newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printPercentage(1,2);

    // get total counts
    totalEntriesNewestCount = 0L;
    error = Database_getUInt(databaseHandle,
                             &totalEntriesNewestCount,
                             "entriesNewest",
                             "COUNT(id)",
                             DATABASE_FILTERS_NONE,
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("collect newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printPercentage(2,2);
    clearPercentage();
    printInfo("OK  \n");


    // delete all newest entries
    printInfo("Purge newest entries...");
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      do
      {
        m = 0L;
        error = Database_delete(databaseHandle,
                                &m,
                                "entriesNewest",
                                DATABASE_FLAG_NONE,
                                DATABASE_FILTERS_NONE,
                                1000
                               );
        n += m;
        printPercentage(n,totalEntriesNewestCount);
      }
      while ((error == ERROR_NONE) && (m > 0));
    }
    clearPercentage();
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("create newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    (void)Database_flush(databaseHandle);
    printInfo("OK  \n");

    // insert newest entries
    printInfo("Create newest entries...");
    n = 0L;
    ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
    {
      error = addToNewest(databaseHandle,storageId);
      n++;
      printPercentage(n,Array_length(&storageIds));
    }
    clearPercentage();
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("create newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    (void)Database_flush(databaseHandle);
    printInfo("OK  \n");
  }
  else
  {
    // Refresh newest entries
    printInfo("Create newest entries:\n");
    ARRAY_ITERATE(&storageIds,arrayIterator,storageId)
    {
      printInfo("  %"PRIi64"...",storageId);
      if (error == ERROR_NONE)
      {
        error = removeFromNewest(databaseHandle,storageId);
      }
      if (error == ERROR_NONE)
      {
        error = addToNewest(databaseHandle,storageId);
      }
      if (error != ERROR_NONE)
      {
        break;
      }
      printInfo("OK\n");
    }
    clearPercentage();
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("create newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    (void)Database_flush(databaseHandle);
  }

  // free resources
}

/***********************************************************************\
* Name   : createAggregatesDirectoryContent
* Purpose: create aggregates diretory content data
* Input  : databaseHandle - database handle
*          entityIds      - database entity id array (still not used!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesDirectoryContent(DatabaseHandle *databaseHandle, const Array entityIds)
{
  Errors error;
  uint   fileEntryCount,directoryEntryCount,linkEntryCount,hardlinkEntryCount,specialEntryCount;
  uint   entityCount;
  uint   totalCount;
  ulong  n;

  UNUSED_VARIABLE(entityIds);

  // calculate directory content size/count aggregated data
  printInfo("Create aggregates for directory content...");

  error = ERROR_NONE;

  // get total count
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &fileEntryCount,
                             "fileEntries \
                                LEFT JOIN entries ON entries.id=fileEntries.entryId \
                             ",
                             "COUNT(entries.id)",
                             "entries.id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &directoryEntryCount,
                             "directoryEntries \
                                LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                             ",
                             "COUNT(entries.id)",
                             "entries.id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &linkEntryCount,
                             "linkEntries \
                                LEFT JOIN entries ON entries.id=linkEntries.entryId \
                             ",
                             "COUNT(entries.id)",
                             "entries.id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &hardlinkEntryCount,
                             "hardlinkEntries \
                                LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                             ",
                             "COUNT(entries.id)",
                             "entries.id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &specialEntryCount,
                             "specialEntries \
                                LEFT JOIN entries ON entries.id=specialEntries.entryId \
                             ",
                             "COUNT(entries.id)",
                             "entries.id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error == ERROR_NONE)
  {
    error = Database_getUInt(databaseHandle,
                             &entityCount,
                             "entities",
                             "COUNT(id)",
                             "id IS NOT NULL",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  }
  if (error != ERROR_NONE)
  {
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  totalCount =  2*fileEntryCount
               +2*directoryEntryCount
               +2*linkEntryCount
               +2*hardlinkEntryCount
               +2*specialEntryCount
               +2*entityCount;
  n = 0L;

  // clear directory content size/count aggregated data
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_update(databaseHandle,
                            NULL,  // changedRowCount
                            "directoryEntries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_UINT  ("totalEntryCount",      0),
                              DATABASE_VALUE_UINT64("totalEntrySize",       0LL),
                              DATABASE_VALUE_UINT  ("totalEntryCountNewest",0),
                              DATABASE_VALUE_UINT64("totalEntrySizeNewest", 0LL)
                            ),
                            DATABASE_FILTERS_NONE
                           );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggegrated data: files
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;
                           uint64     totalSize;

                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);
                           totalSize = values[2].u64;

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE       ("totalEntryCount","totalEntryCount+1"),
                                                       DATABASE_VALUE_UINT64("totalEntrySize", "totalEntrySize+?",totalSize)
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "fileEntries \
                              LEFT JOIN entries        ON entries.id=fileEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entryFragments.storageId"),
                           DATABASE_COLUMN_STRING("entries.name"),
                           DATABASE_COLUMN_UINT64("entryFragments.size"),
                         ),
                         "    entries.id IS NOT NULL \
                          AND entryFragments.storageId IS NOT NULL \
                         ",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;
                           uint64     totalSize;

                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);
                           totalSize = values[2].u64;

                           // update directory content count/size aggregates in all newest directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE       ("totalEntryCountNewest","totalEntryCountNewest+1"),
                                                       DATABASE_VALUE_UINT64("totalEntrySizeNewest", "totalEntrySizeNewest+?",totalSize),
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "fileEntries \
                              LEFT JOIN entriesNewest  ON entriesNewest.entryId=fileEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entryFragments.storageId"),
                           DATABASE_COLUMN_STRING("entriesNewest.name"),
                           DATABASE_COLUMN_UINT64("entryFragments.size")
                         ),
                         "    entriesNewest.id IS NOT NULL \
                          AND entryFragments.storageId IS NOT NULL \
                         ",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggregated data: directories
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCount","totalEntryCount+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s'(error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("directoryEntries.storageId"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "entries.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCountNewest","totalEntryCountNewest+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "directoryEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("directoryEntries.storageId"),
                           DATABASE_COLUMN_STRING("entriesNewest.name")
                         ),
                         "entriesNewest.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggregated data: links
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
//fprintf(stderr,"%s, %d: name=%s\n",__FILE__,__LINE__,String_cString(name));
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCount","totalEntryCount+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "linkEntries \
                              LEFT JOIN entries ON entries.id=linkEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("linkEntries.storageId"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "entries.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCountNewest","totalEntryCountNewest+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "linkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("linkEntries.storageId"),
                           DATABASE_COLUMN_STRING("entriesNewest.name")
                         ),
                         "entriesNewest.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggregated data: hardlinks
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;
                           uint64     totalSize;

                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);
                           totalSize = values[2].u64;

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE       ("totalEntryCount","totalEntryCount+1"),
                                                       DATABASE_VALUE_UINT64("totalEntrySize", "totalEntrySize+?",totalSize)
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "hardlinkEntries \
                              LEFT JOIN entries        ON entries.id=hardlinkEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entryFragments.storageId"),
                           DATABASE_COLUMN_STRING("entries.name"),
                           DATABASE_COLUMN_UINT64("entryFragments.size")
                         ),
                         "    entries.id IS NOT NULL \
                          AND entryFragments.storageId IS NOT NULL \
                         ",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;
                           uint64     totalSize;

                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);
                           totalSize = values[2].u64;

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE       ("totalEntryCountNewest","totalEntryCountNewest+1"),
                                                       DATABASE_VALUE_UINT64("totalEntrySizeNewest", "totalEntrySizeNewest+?",totalSize)
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "hardlinkEntries \
                              LEFT JOIN entriesNewest  ON entriesNewest.entryId=hardlinkEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("entryFragments.storageId"),
                           DATABASE_COLUMN_STRING("entriesNewest.name"),
                           DATABASE_COLUMN_UINT64("entryFragments.size")
                         ),
                         "    entriesNewest.id IS NOT NULL \
                          AND entryFragments.storageId IS NOT NULL \
                         ",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggregated data: special
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCount","totalEntryCount+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("specialEntries.storageId"),
                           DATABASE_COLUMN_STRING("entries.name")
                         ),
                         "entries.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           String     name;

                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;
                           name      = String_duplicate(values[1].string);

                           // update directory content count/size aggregates in all directories
                           while (!String_isEmpty(File_getDirectoryName(name,name)))
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "directoryEntries",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE("totalEntryCountNewest","totalEntryCountNewest+1")
                                                     ),
                                                     "    storageId=? \
                                                      AND name=? \
                                                     ",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY   (storageId),
                                                       DATABASE_FILTER_STRING(name)
                                                     )
                                                    );
                             if (error != ERROR_NONE)
                             {
                               printInfo("FAIL!\n");
                               printError("create aggregates fail for entries '%s' (error: %s)!",String_cString(name),Error_getText(error));
                               return error;
                             }
                           }

                           String_delete(name);

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "specialEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("specialEntries.storageId"),
                           DATABASE_COLUMN_STRING("entriesNewest.name")
                         ),
                         "entriesNewest.id IS NOT NULL",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  printInfo("OK  \n");
}

/***********************************************************************\
* Name   : createAggregatesEntities
* Purpose: create aggregates entities data
* Input  : databaseHandle - database handle
*          entityIds      - database entity id array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesEntities(DatabaseHandle *databaseHandle, const Array entityIds)
{
  String     entityIdsString;
  ulong      i;
  DatabaseId entityId;
  Errors     error;
  uint       totalCount;
  char       filterString[1024];
  ulong      n;

  // init variables
  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%"PRIi64,entityId);
  }

  printInfo("Create aggregates for entities...");

  // get entities total count
  totalCount = 0;
  error = Database_getUInt(databaseHandle,
                           &totalCount,
                           "entities",
                           "COUNT(id)",
                           stringFormat(filterString,sizeof(filterString),
                                        "    (? OR id IN (%s)) \
                                         AND deletedFlag!=TRUE \
                                        ",
                                        !String_isEmpty(entityIdsString) ? String_cString(entityIdsString) : "0"
                                       ),
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_BOOL(String_isEmpty(entityIdsString))
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    String_delete(entityIdsString);
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // update entities total count/size aggregates
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId entityId;

                           uint   totalFileCount;
                           uint   totalImageCount;
                           uint   totalDirectoryCount;
                           uint   totalLinkCount;
                           uint   totalHardlinkCount;
                           uint   totalSpecialCount;
                           uint64 totalFileSize;
                           uint64 totalImageSize;
                           uint64 totalHardlinkSize;

                           uint   totalFileCountNewest;
                           uint   totalImageCountNewest;
                           uint   totalDirectoryCountNewest;
                           uint   totalLinkCountNewest;
                           uint   totalHardlinkCountNewest;
                           uint   totalSpecialCountNewest;
                           uint64 totalFileSizeNewest;
                           uint64 totalImageSizeNewest;
                           uint64 totalHardlinkSizeNewest;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           entityId = values[0].id;

                           // total count/size
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalFileCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_FILE     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalImageCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_IMAGE    ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalDirectoryCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_DIRECTORY),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalLinkCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_LINK     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalHardlinkCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_HARDLINK ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalSpecialCount,
                                                      "entries",
                                                      "COUNT(entries.id)",
                                                      "entries.type=? AND entries.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_SPECIAL  ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }

                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalFileSize,
                                                        "entries \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entries.type=? AND entries.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_FILE     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                       );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalImageSize,
                                                        "entries \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entries.type=? AND entries.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                          DATABASE_FILTER_KEY(INDEX_CONST_TYPE_IMAGE    ),
                                                          DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                      );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalHardlinkSize,
                                                        "entries \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entries.type=? AND entries.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                          DATABASE_FILTER_KEY(INDEX_CONST_TYPE_HARDLINK ),
                                                          DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                       );
                           }

                           if (error == ERROR_NONE)
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "entities",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE_UINT64("totalEntryCount",      totalFileCount
                                                                                                    +totalImageCount
                                                                                                    +totalDirectoryCount
                                                                                                    +totalLinkCount
                                                                                                    +totalHardlinkCount
                                                                                                    +totalSpecialCount
                                                                            ),
                                                       DATABASE_VALUE_UINT64("totalEntrySize",       totalFileSize
                                                                                                    +totalImageSize
                                                                                                    +totalHardlinkSize
                                                                            ),

                                                       DATABASE_VALUE_UINT  ("totalFileCount",      totalFileCount),
                                                       DATABASE_VALUE_UINT  ("totalImageCount",     totalImageCount),
                                                       DATABASE_VALUE_UINT  ("totalDirectoryCount", totalDirectoryCount),
                                                       DATABASE_VALUE_UINT  ("totalLinkCount",      totalLinkCount),
                                                       DATABASE_VALUE_UINT  ("totalHardlinkCount",  totalHardlinkCount),
                                                       DATABASE_VALUE_UINT  ("totalSpecialCount",   totalSpecialCount),

                                                       DATABASE_VALUE_UINT64("totalFileSize",       totalFileSize),
                                                       DATABASE_VALUE_UINT64("totalImageSize",      totalImageSize),
                                                       DATABASE_VALUE_UINT64("totalHardlinkSize",   totalHardlinkSize)
                                                     ),
                                                     "id=?",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY(entityId)
                                                     )
                                                    );
                           }
                           if (error != ERROR_NONE)
                           {
                             printInfo("FAIL!\n");
                             printError("create aggregates fail for entity #%"PRIi64" (error: %s)!",entityId,Error_getText(error));
                             return error;
                           }

                           // total count/size newest
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalFileCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_FILE     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalImageCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_IMAGE    ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalDirectoryCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_DIRECTORY),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalLinkCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_LINK     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalHardlinkCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_HARDLINK ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt(databaseHandle,
                                                      &totalSpecialCountNewest,
                                                      "entriesNewest",
                                                      "COUNT(entriesNewest.id)",
                                                      "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                      DATABASE_FILTERS
                                                      (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_SPECIAL  ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                      ),
                                                      NULL
                                                     );
                           }

                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalFileSizeNewest,
                                                        "entriesNewest \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                        DATABASE_FILTER_KEY(INDEX_CONST_TYPE_FILE     ),
                                                        DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                       );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalImageSizeNewest,
                                                        "entriesNewest \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                          DATABASE_FILTER_KEY(INDEX_CONST_TYPE_IMAGE    ),
                                                          DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                      );
                           }
                           if (error == ERROR_NONE)
                           {
                             error = Database_getUInt64(databaseHandle,
                                                        &totalHardlinkSizeNewest,
                                                        "entriesNewest \
                                                           LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                                        ",
                                                        "SUM(entryFragments.size)",
                                                        "entriesNewest.type=? AND entriesNewest.entityId=?",
                                                        DATABASE_FILTERS
                                                        (
                                                          DATABASE_FILTER_KEY(INDEX_CONST_TYPE_HARDLINK ),
                                                          DATABASE_FILTER_KEY(entityId)
                                                        ),
                                                        NULL
                                                       );
                           }

                           if (error == ERROR_NONE)
                           {
                             error = Database_update(databaseHandle,
                                                     NULL,  // changedRowCount
                                                     "entities",
                                                     DATABASE_FLAG_NONE,
                                                     DATABASE_VALUES
                                                     (
                                                       DATABASE_VALUE_UINT64("totalEntryCountNewest",      totalFileCountNewest
                                                                                                          +totalImageCountNewest
                                                                                                          +totalDirectoryCountNewest
                                                                                                          +totalLinkCountNewest
                                                                                                          +totalHardlinkCountNewest
                                                                                                          +totalSpecialCountNewest
                                                                            ),
                                                       DATABASE_VALUE_UINT64("totalEntrySizeNewest",       totalFileSizeNewest
                                                                                                          +totalImageSizeNewest
                                                                                                          +totalHardlinkSizeNewest
                                                                            ),

                                                       DATABASE_VALUE_UINT  ("totalFileCountNewest",      totalFileCountNewest),
                                                       DATABASE_VALUE_UINT  ("totalImageCountNewest",     totalImageCountNewest),
                                                       DATABASE_VALUE_UINT  ("totalDirectoryCountNewest", totalDirectoryCountNewest),
                                                       DATABASE_VALUE_UINT  ("totalLinkCountNewest",      totalLinkCountNewest),
                                                       DATABASE_VALUE_UINT  ("totalHardlinkCountNewest",  totalHardlinkCountNewest),
                                                       DATABASE_VALUE_UINT  ("totalSpecialCountNewest",   totalSpecialCountNewest),

                                                       DATABASE_VALUE_UINT64("totalFileSizeNewest",       totalFileSizeNewest),
                                                       DATABASE_VALUE_UINT64("totalImageSizeNewest",      totalImageSizeNewest),
                                                       DATABASE_VALUE_UINT64("totalHardlinkSizeNewest",   totalHardlinkSizeNewest)
                                                     ),
                                                     "id=?",
                                                     DATABASE_FILTERS
                                                     (
                                                       DATABASE_FILTER_KEY(entityId)
                                                     )
                                                    );
                           }
                           if (error != ERROR_NONE)
                           {
                             printInfo("FAIL!\n");
                             printError("create newest aggregates fail for entity #%"PRIi64" (error: %s)!",entityId,Error_getText(error));
                             return error;
                           }

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entities"
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY("id")
                         ),
                         stringFormat(filterString,sizeof(filterString),
                                      "     (? OR id IN (%s)) \
                                       AND deletedFlag!=TRUE \
                                      ",
                                      !Array_isEmpty(&entityIds) ? String_cString(entityIdsString) : "0"
                                     ),
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_BOOL   (Array_isEmpty(&entityIds))
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    String_delete(entityIdsString);
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  printInfo("OK  \n");

  // free resources
  String_delete(entityIdsString);
}

/***********************************************************************\
* Name   : createAggregatesStorages
* Purpose: create aggregates storages data
* Input  : databaseHandle - database handle
*          storageIds     - database storage id array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createAggregatesStorages(DatabaseHandle *databaseHandle, const Array storageIds)
{
  String     storageIdsString;
  uint       i;
  DatabaseId storageId;
  Errors     error;
  uint       totalCount;
  char       filterString[1024];
  ulong      n;

  storageIdsString = String_new();
  ARRAY_ITERATE(&storageIds,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%"PRIi64,storageId);
  }

  printInfo("Create aggregates for storages...");

  // get storage total count
  totalCount = 0;
  error = Database_getUInt(databaseHandle,
                           &totalCount,
                           "storages",
                           "COUNT(id)",
                           stringFormat(filterString,sizeof(filterString),
                                        "    (? OR id IN (%s)) \
                                         AND deletedFlag!=TRUE \
                                        ",
                                        !String_isEmpty(storageIdsString) ? String_cString(storageIdsString) : "0"
                                       ),
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_BOOL(String_isEmpty(storageIdsString))
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // update storage total count/size aggregates
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           DatabaseId storageId;
                           uint       totalFileCount;
                           uint       totalImageCount;
                           uint       totalDirectoryCount;
                           uint       totalLinkCount;
                           uint       totalHardlinkCount;
                           uint       totalSpecialCount;
                           uint64     totalFileSize;
                           uint64     totalImageSize;
                           uint64     totalHardlinkSize;

                           uint       totalFileCountNewest;
                           uint       totalImageCountNewest;
                           uint       totalDirectoryCountNewest;
                           uint       totalLinkCountNewest;
                           uint       totalHardlinkCountNewest;
                           uint       totalSpecialCountNewest;
                           uint64     totalFileSizeNewest;
                           uint64     totalImageSizeNewest;
                           uint64     totalHardlinkSizeNewest;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           storageId = values[0].id;

                           // get file aggregate data
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalFileCount = values[0].u;
                                                  totalFileSize  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                 AND entries.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_FILE)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get image aggregate data
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalImageCount = values[0].u;
                                                  totalImageSize  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                  AND entries.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalDirectoryCount = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "directoryEntries \
                                                     LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT("COUNT(DISTINCT entries.id)")
                                                ),
                                                "directoryEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalLinkCount = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "linkEntries \
                                                     LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT("COUNT(DISTINCT entries.id)"),
                                                ),
                                                "linkEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get hardlink aggregate data
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalHardlinkCount = values[0].u;
                                                  totalHardlinkSize  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                 AND entries.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalSpecialCount = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "specialEntries \
                                                     LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT("COUNT(DISTINCT entries.id)"),
                                                ),
                                                "specialEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           // get newest file aggregate data
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalFileCountNewest = values[0].u;
                                                  totalFileSizeNewest  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                 AND entriesNewest.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_FILE)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalFileCountNewest = 0L;
                             totalFileSizeNewest  = 0LL;
                           }

                           // get newest image aggregate data
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalImageCountNewest = values[0].u;
                                                  totalImageSizeNewest  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                 AND entriesNewest.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalImageCountNewest = 0L;
                             totalImageSizeNewest  = 0LL;
                           }

                           // get newest directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalDirectoryCountNewest = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "directoryEntries \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                                                ),
                                                "directoryEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalDirectoryCountNewest = 0L;
                           }

                           // get newest link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalLinkCountNewest = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "linkEntries \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                                                ),
                                                "linkEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalLinkCountNewest = 0L;
                           }

                           // get newest hardlink aggregate data
                         //vs bar-index: getUInt
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 2);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalHardlinkCountNewest = values[0].u;
                                                  totalHardlinkSizeNewest  = values[1].u64;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "entryFragments \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                                                  DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                                                ),
                                                "    entryFragments.storageId=? \
                                                 AND entriesNewest.type=? \
                                                ",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId),
                                                  DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalHardlinkCountNewest = 0L;
                             totalHardlinkSizeNewest  = 0LL;
                           }

                           // get newest special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
                           error = Database_get(databaseHandle,
                                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                {
                                                  assert(values != NULL);
                                                  assert(valueCount == 1);

                                                  UNUSED_VARIABLE(userData);
                                                  UNUSED_VARIABLE(valueCount);

                                                  totalSpecialCountNewest = values[0].u;

                                                  return ERROR_NONE;
                                                },NULL),
                                                NULL,  // changedRowCount
                                                DATABASE_TABLES
                                                (
                                                  "specialEntries \
                                                     LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                                                  "
                                                ),
                                                DATABASE_FLAG_NONE,
                                                DATABASE_COLUMNS
                                                (
                                                  DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                                                ),
                                                "specialEntries.storageId=?",
                                                DATABASE_FILTERS
                                                (
                                                  DATABASE_FILTER_KEY (storageId)
                                                ),
                                                NULL,  // groupBy
                                                NULL,  // orderBy
                                                0LL,
                                                1LL
                                               );
                           if (error != ERROR_NONE)
                           {
                             totalSpecialCountNewest = 0L;
                           }
                           // update total count/size
                           error = Database_update(databaseHandle,
                                                   NULL,  // changedRowCount
                                                   "storages",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_UINT  ("totalEntryCount",           totalFileCount
                                                                                                       +totalImageCount
                                                                                                       +totalDirectoryCount
                                                                                                       +totalLinkCount
                                                                                                       +totalHardlinkCount
                                                                                                       +totalSpecialCount
                                                                          ),
                                                     DATABASE_VALUE_UINT64("totalEntrySize",            totalFileSize
                                                                                                       +totalImageSize
                                                                                                       +totalHardlinkSize
                                                                          ),
                                                     DATABASE_VALUE_UINT  ("totalFileCount",           totalFileCount),
                                                     DATABASE_VALUE_UINT  ("totalImageCount",          totalImageCount),
                                                     DATABASE_VALUE_UINT  ("totalDirectoryCount",      totalDirectoryCount),
                                                     DATABASE_VALUE_UINT  ("totalLinkCount",           totalLinkCount),
                                                     DATABASE_VALUE_UINT  ("totalHardlinkCount",       totalHardlinkCount),
                                                     DATABASE_VALUE_UINT  ("totalSpecialCount",        totalSpecialCount),

                                                     DATABASE_VALUE_UINT64("totalFileSize",            totalFileSize),
                                                     DATABASE_VALUE_UINT64("totalImageSize",           totalImageSize),
                                                     DATABASE_VALUE_UINT64("totalHardlinkSize",        totalHardlinkSize),

                                                     DATABASE_VALUE_UINT  ("totalEntryCountNewest",     totalFileCountNewest
                                                                                                       +totalImageCountNewest
                                                                                                       +totalDirectoryCountNewest
                                                                                                       +totalLinkCountNewest
                                                                                                       +totalHardlinkCountNewest
                                                                                                       +totalSpecialCountNewest
                                                                          ),
                                                     DATABASE_VALUE_UINT64("totalEntrySizeNewest",      totalFileSizeNewest
                                                                                                       +totalImageSizeNewest
                                                                                                       +totalHardlinkSizeNewest
                                                                          ),
                                                     DATABASE_VALUE_UINT  ("totalFileCountNewest",     totalFileCountNewest),
                                                     DATABASE_VALUE_UINT  ("totalImageCountNewest",    totalImageCountNewest),
                                                     DATABASE_VALUE_UINT  ("totalDirectoryCountNewest",totalDirectoryCountNewest),
                                                     DATABASE_VALUE_UINT  ("totalLinkCountNewest",     totalLinkCountNewest),
                                                     DATABASE_VALUE_UINT  ("totalHardlinkCountNewest", totalHardlinkCountNewest),
                                                     DATABASE_VALUE_UINT  ("totalSpecialCountNewest",  totalSpecialCountNewest),

                                                     DATABASE_VALUE_UINT64("totalFileSizeNewest",      totalFileSizeNewest),
                                                     DATABASE_VALUE_UINT64("totalImageSizeNewest",     totalImageSizeNewest),
                                                     DATABASE_VALUE_UINT64("totalHardlinkSizeNewest",  totalHardlinkSizeNewest)
                                                   ),
                                                   "id=?",
                                                   DATABASE_FILTERS
                                                   (
                                                     DATABASE_FILTER_KEY(storageId)
                                                   )
                                                  );
                           if (error != ERROR_NONE)
                           {
                             printInfo("FAIL!\n");
                             printError("create aggregates fail for storage #%"PRIi64" (error: %s)!",storageId,Error_getText(error));
                             return error;
                           }

                           n++;
                           printPercentage(n,totalCount);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "storages"
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY("id")
                         ),
                         stringFormat(filterString,sizeof(filterString),
                                      "    (? OR id IN (%s)) \
                                       AND deletedFlag!=TRUE \
                                      ",
                                      !String_isEmpty(storageIdsString) ? String_cString(storageIdsString) : "0"
                                     ),
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_BOOL(String_isEmpty(storageIdsString))
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    String_delete(storageIdsString);
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  printInfo("OK  \n");

  // free resources
  String_delete(storageIdsString);
}

/***********************************************************************\
* Name   : cleanOrphanedEntries
* Purpose: purge orphaned entries
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanOrphanedEntries(DatabaseHandle *databaseHandle)
{
  String        storageName;
  Errors        error;
  ulong         total;
  ulong         n;
  Array         ids;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  // initialize variables
  storageName = String_new();
  Array_init(&ids,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  total       = 0;

  printInfo("Clean-up orphaned:\n");

  // clean fragments/directory entries/link entries/special entries without or an empty storage name
  printInfo("  entries without storage name...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "entryFragments \
                             LEFT JOIN storages ON storages.id=entryFragments.storageId \
                          ",
                          "entryFragments.id",
                          "storages.id IS NULL OR storages.name IS NULL OR storages.name=''",
                          DATABASE_FILTERS
                          (
                          )
                         );
    ARRAY_ITERATE(&ids,arrayIterator,databaseId)
    {
      (void)Database_delete(databaseHandle,
                            &n,
                            "entryFragments",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(databaseId)
                            ),
                            DATABASE_UNLIMITED
                           );
    }
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "directoryEntries \
                             LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                          ",
                          "directoryEntries.id",
                          "storages.id IS NULL OR storages.name IS NULL OR storages.name=''",
                          DATABASE_FILTERS
                          (
                          )
                         );
    ARRAY_ITERATE(&ids,arrayIterator,databaseId)
    {
      (void)Database_delete(databaseHandle,
                            &n,
                            "directoryEntries",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(databaseId)
                            ),
                            DATABASE_UNLIMITED
                           );
    }
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "linkEntries \
                             LEFT JOIN storages ON storages.id=linkEntries.storageId \
                          ",
                          "linkEntries.id",
                          "storages.id IS NULL OR storages.name IS NULL OR storages.name=''",
                          DATABASE_FILTERS
                          (
                          )
                         );
    ARRAY_ITERATE(&ids,arrayIterator,databaseId)
    {
      (void)Database_delete(databaseHandle,
                            &n,
                            "linkEntries",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(databaseId)
                            ),
                            DATABASE_UNLIMITED
                           );
    }
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "specialEntries \
                             LEFT JOIN storages ON storages.id=specialEntries.storageId \
                          ",
                          "specialEntries.id",
                          "storages.id IS NULL OR storages.name IS NULL OR storages.name=''",
                          DATABASE_FILTERS
                          (
                          )
                         );
    ARRAY_ITERATE(&ids,arrayIterator,databaseId)
    {
      (void)Database_delete(databaseHandle,
                            &n,
                            "specialEntries",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(databaseId)
                            ),
                            DATABASE_UNLIMITED
                           );
    }
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean entries without fragments
  printInfo("  file entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_delete(databaseHandle,
                          &n,
                          "fileEntries",
                          DATABASE_FLAG_NONE,
                          "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 1)",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;
  printInfo("  image entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_delete(databaseHandle,
                          &n,
                          "imageEntries",
                          DATABASE_FLAG_NONE,
                          "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 1)",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;
  printInfo("  hardlink entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_delete(databaseHandle,
                          &n,
                          "hardlinkEntries",
                          DATABASE_FLAG_NONE,
                          "NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 1)",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM fileEntries WHERE fileEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without image entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM imageEntries WHERE imageEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
      clearPercentage();
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without directory entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM directoryEntries WHERE directoryEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_DIRECTORY)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,Array_length(&ids));
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without link entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM linkEntries WHERE linkEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_LINK)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without hardlink entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM hardlinkEntries WHERE hardlinkEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without special entry...");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries",
                          "id",
                          "    entries.type=? \
                           AND NOT EXISTS(SELECT id FROM specialEntries WHERE specialEntries.entryId=entries.id LIMIT 1) \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_SPECIAL)
                          )
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&ids));
      ARRAY_ITERATE(&ids,arrayIterator,databaseId)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entries",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        printPercentage(n,Array_length(&ids));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  // clean storages without name
  printInfo("  storages without name...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_delete(databaseHandle,
                          &n,
                          "storages",
                          DATABASE_FLAG_NONE,
                          "name IS NULL OR name=''",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean storages with invalid state
  printInfo("  storages with invalid state...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_delete(databaseHandle,
                          &n,
                          "storages",
                          DATABASE_FLAG_NONE,
                          "(state<?) OR (state>?)",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_STATE_OK),
                            DATABASE_FILTER_UINT(INDEX_CONST_STATE_ERROR)
                          ),
                          DATABASE_UNLIMITED
                         );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      // clean FTS entries without entry
      printInfo("  FTS entries without entry...");
      n = 0L;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "FTS_entries",
                              DATABASE_FLAG_NONE,
                              "NOT EXISTS(SELECT id FROM entries WHERE entries.id=FTS_entries.entryId LIMIT 1)",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      total += n;

      // clean FTS storages without entry
      printInfo("  FTS storages without storage...");
      n = 0L;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "FTS_storages",
                              DATABASE_FLAG_NONE,
                              "NOT EXISTS(SELECT id FROM storages WHERE storages.id=FTS_storages.storageId LIMIT 1)",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      total += n;

      // clean newest entries without entry
      printInfo("  FTS entries without entry...");
      n = 0L;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        (void)Database_delete(databaseHandle,
                              &n,
                              "entriesNewest",
                              DATABASE_FLAG_NONE,
                              "NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 1)",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      total += n;
      break;
    case DATABASE_TYPE_MARIADB:
      // nothing to do (use views)
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // nothing to do (use views)
      break;
  }

//TODO: obsolete, remove
#if 0
  // clean *Entries without entry
  printInfo("  orphaned entries...");
  n = 0L;
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "fileEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "fileEntries \
                          LEFT JOIN entries ON entries.id=fileEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("fileEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "imageEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "imageEntries \
                          LEFT JOIN entries ON entries.id=imageEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("imageEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "directoryEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "directoryEntries \
                          LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("directoryEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "linkEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "linkEntries \
                          LEFT JOIN entries ON entries.id=linkEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("linkEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "hardlinkEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "linkEntries \
                          LEFT JOIN entries ON entries.id=linkEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("linkEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  (void)Database_get(databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       int64  databaseId;
                       Errors error;

                       assert(values != NULL);
                       assert(valueCount == 1);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       databaseId = values[0].id;

                       error = Database_delete(databaseHandle,
                                               NULL,  // changedRowCount
                                               "specialEntries",
                                               DATABASE_FLAG_NONE,
                                               "id=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_KEY(databaseId)
                                               ),
                                               DATABASE_UNLIMITED
                                              );
                       if (error != ERROR_NONE)
                       {
                         return error;
                       }

                       n++;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_TABLES
                     (
                       "specialEntries \
                          LEFT JOIN entries ON entries.id=specialEntries.entryId \
                       "
                     ),
                     DATABASE_FLAG_NONE,
                     DATABASE_COLUMNS
                     (
                       DATABASE_COLUMN_KEY  ("specialEntries.id")
                     ),
                     "entries.id IS NULL ",
                     DATABASE_FILTERS
                     (
                     ),
                     NULL,  // orderGroup
                     0LL,
                     DATABASE_UNLIMITED
                    );
  printInfo("%lu\n",n);
  total += n;
#endif

  printInfo("Total %lu orphaned entries removed\n",total);

  // free resources
  Array_done(&ids);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : cleanDuplicates
* Purpose: purge duplicate entries
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanDuplicates(DatabaseHandle *databaseHandle)
{
  String name;
  Errors error;
  ulong  totalCount;
  ulong  n;

  // init variables
  name       = String_new();
  totalCount = 0LL;

  printInfo("Clean duplicates:\n");

  // check duplicate storages
  printInfo("  storages:\n");
  n = 0L;
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         DatabaseId  storageId;
                         ConstString otherName;

                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         storageId = values[0].id;
                         otherName = values[1].string;

                         if (String_equals(name,otherName))
                         {
                           error = Database_update(databaseHandle,
                                                   NULL,  // changedRowCount
                                                   "storages",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_BOOL("deletedFlag",TRUE)
                                                   ),
                                                   "id=?",
                                                   DATABASE_FILTERS
                                                   (
                                                     DATABASE_FILTER_KEY  (storageId)
                                                   )
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }
                           n++;
                           printInfo("    %s\n",String_cString(otherName));
                         }
                         String_set(name,otherName);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY   ("id"),
                         DATABASE_COLUMN_STRING("name")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       "name",
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("clean duplicates fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += n;

  printInfo("Total %lu duplicate entries removed\n",n);

  // free resources
  String_delete(name);
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
  ulong         n;
  Errors        error;
  DatabaseId    storageId;
  Array         entryIds;
  ArrayIterator arrayIterator;
  DatabaseId    entryId;

  // init variables

  printInfo("Purge deleted storages:\n");

  Array_init(&entryIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  n     = 0L;
  error = ERROR_UNKNOWN;
  do
  {
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      // get storage to purge
      error = Database_getId(databaseHandle,
                             &storageId,
                             "storages",
                             "id",
                             "deletedFlag=1",
                             DATABASE_FILTERS
                             (
                             )
                            );
      if ((error == ERROR_NONE) && (storageId != DATABASE_ID_NONE))
      {
        printInfo("  %10"PRIi64"...",storageId);

        // collect file/image/hardlink entries to purge
        Array_clear(&entryIds);
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "entryFragments",
                                   "entryId",
                                   "storageId=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(storageId)
                                   )
                                  );
        }

        // collect directory/link/special entries to purge
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "directoryEntries",
                                   "entryId",
                                   "storageId=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(storageId)
                                   )
                                  );
        }
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "linkEntries",
                                   "entryId",
                                   "storageId=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(storageId)
                                   )
                                  );
        }
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "specialEntries",
                                   "entryId",
                                   "storageId=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(storageId)
                                   )
                                  );
        }

        printPercentage(0,2*Array_length(&entryIds));

        // purge fragments
        if (error == ERROR_NONE)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "entryFragments",
                                  DATABASE_FLAG_NONE,
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  ),
                                  0
                                 );
        }

        switch (Database_getType(databaseHandle))
        {
          case DATABASE_TYPE_SQLITE3:
            // purge FTS entries
            if (error == ERROR_NONE)
            {
              printPercentage(0*Array_length(&entryIds),2*Array_length(&entryIds));
              ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
              {
                if (!Database_existsValue(databaseHandle,
                                          "entryFragments",
                                          "id",
                                          "entryId=?",
                                          DATABASE_FILTERS
                                          (
                                            DATABASE_FILTER_KEY(entryId)
                                          )
                                         )
                   )
                {
                  error = Database_delete(databaseHandle,
                                          &n,
                                          "FTS_entries",
                                          DATABASE_FLAG_NONE,
                                          "entryId MATCH ?",
                                          DATABASE_FILTERS
                                          (
                                            DATABASE_FILTER_KEY(entryId)
                                          ),
                                          0
                                         );
                }
                printPercentage(0*Array_length(&entryIds)+n,2*Array_length(&entryIds));
              }
              printPercentage(1*Array_length(&entryIds),2*Array_length(&entryIds));
            }
            break;
          case DATABASE_TYPE_MARIADB:
            // nothing to do (use views)
            break;
          case DATABASE_TYPE_POSTGRESQL:
            // nothing to do (use views)
            break;
        }

        // purge directory/link/special entries
        if (error == ERROR_NONE)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "directoryEntries",
                                  DATABASE_FLAG_NONE,
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  ),
                                  0
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "linkEntries",
                                  DATABASE_FLAG_NONE,
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  ),
                                  0
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "specialEntries",
                                  DATABASE_FLAG_NONE,
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  ),
                                  0
                                 );
        }

        switch (Database_getType(databaseHandle))
        {
          case DATABASE_TYPE_SQLITE3:
            // purge FTS storages
            if (error == ERROR_NONE)
            {
              error = Database_delete(databaseHandle,
                                      &n,
                                      "FTS_storages",
                                      DATABASE_FLAG_NONE,
                                      "storageId MATCH ?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(storageId)
                                      ),
                                      0
                                     );
            }
            break;
          case DATABASE_TYPE_MARIADB:
            // nothing to do (use views)
            break;
          case DATABASE_TYPE_POSTGRESQL:
            // nothing to do (use views)
            break;
        }

        // purge storage
        if (error == ERROR_NONE)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  "id=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  ),
                                  0
                                 );
        }

        // purge entries
        if (error == ERROR_NONE)
        {
          printPercentage(1*Array_length(&entryIds),2*Array_length(&entryIds));
          ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
          {
            if (!Database_existsValue(databaseHandle,
                                      "entryFragments",
                                      "id",
                                      "entryId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(entryId)
                                      )
                                     )
               )
            {
              error = Database_delete(databaseHandle,
                                      &n,
                                      "entries",
                                      DATABASE_FLAG_NONE,
                                      "id=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(entryId)
                                      ),
                                      0
                                     );
            }
            printPercentage(1*Array_length(&entryIds)+n,2*Array_length(&entryIds));
          }
          printPercentage(2*Array_length(&entryIds),2*Array_length(&entryIds));
        }

        clearPercentage();

        if (error == ERROR_NONE)
        {
          printInfo("OK\n");
          n++;
        }
        else
        {
          printInfo("FAIL!\n");
          DATABASE_TRANSACTION_ABORT(databaseHandle);
        }
      }
    }
  }
  while ((storageId != DATABASE_ID_NONE) && (error == ERROR_NONE));
  if (error != ERROR_NONE)
  {
    printError("purge deleted fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // free resources
  Array_done(&entryIds);
}

/***********************************************************************\
* Name   : vacuum
* Purpose: vacuum database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void vacuum(DatabaseHandle *databaseHandle, const char *toFileName)
{
  Errors     error;
  FileHandle handle;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      printInfo("Vacuum...");

      if (toFileName != NULL)
      {
        // check if file exists
        if (!forceFlag && File_existsCString(toFileName))
        {
          printInfo("FAIL!\n");
          printError("vacuum fail: file '%s' already exists!",toFileName);
          exit(EXITCODE_FAIL);
        }

        // create empty file
        error = File_openCString(&handle,toFileName,FILE_OPEN_CREATE);
        if (error == ERROR_NONE)
        {
          (void)File_close(&handle);
        }
        else
        {
          printInfo("FAIL!\n");
          printError("vacuum fail (error: %s)!",Error_getText(error));
          exit(EXITCODE_FAIL);
        }

        // vacuum into file
    // TODO: move to databaes.c
        error = Database_execute(databaseHandle,
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 "VACUUM INTO ?",
                                 DATABASE_PARAMETERS
                                 (
                                   DATABASE_PARAMETER_STRING(toFileName)
                                 )
                                );
        if (error != ERROR_NONE)
        {
          printInfo("FAIL!\n");
          printError("vacuum fail (error: %s)!",Error_getText(error));
          exit(EXITCODE_FAIL);
        }
      }
      else
      {
        // vacuum
        error = Database_execute(databaseHandle,
                                 NULL,  // changedRowCount
                                 DATABASE_FLAG_NONE,
                                 "VACUUM",
                                 DATABASE_PARAMETERS
                                 (
                                 )
                                );
        if (error != ERROR_NONE)
        {
          printInfo("FAIL!\n");
          printError("vacuum fail (error: %s)!",Error_getText(error));
          exit(EXITCODE_FAIL);
        }
      }

      printInfo("OK\n");
      break;
    case DATABASE_TYPE_MARIADB:
      {
        char sqlCommand[256];

        printInfo("Vacuum...");

        error = ERROR_NONE;
        for (uint i = 0; i < INDEX_DEFINITION_TABLE_NAME_COUNT_MARIADB; i++)
        {
          error = Database_execute(databaseHandle,
                                   NULL,  // changedRowCount
                                   DATABASE_FLAG_NONE,
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "OPTIMIZE TABLE %s",
                                                INDEX_DEFINITION_TABLE_NAMES_MARIADB[i]
                                               ),
                                   DATABASE_PARAMETERS
                                   (
                                   )
                                  );
          if (error != ERROR_NONE)
          {
            printInfo("FAIL!\n");
            printError("vacuum fail (error: %s)!",Error_getText(error));
            exit(EXITCODE_FAIL);
          }
        }

        printInfo("OK\n");
      }
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // nothing to do
      break;
  }
}

/***********************************************************************\
* Name   : freeColumnsWidth
* Purpose: get columns width
* Input  : widths - column widths
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeColumnsWidth(size_t widths[])
{
  if (widths != NULL) free(widths);
}

/***********************************************************************\
* Name   : printChars
* Purpose: print characters
* Input  : ch - character to print
*          n  - number of spaces
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printChars(char ch, uint n)
{
  uint i;

  for (i = 0; i < n; i++)
  {
    fwrite(&ch,sizeof(char),1,stdout);
  }
}

/***********************************************************************\
* Name   : getColumnWidths
* Purpose: get columns width call back
* Input  : values   - values
*          count    - number of values
*          userData - user data (PrintRowData)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getColumnWidths(const DatabaseValue values[], uint valueCount, void *userData)
{
  PrintRowData *printRowData = (PrintRowData*)userData;
  uint         i;

  assert(values != NULL);
  assert(printRowData != NULL);

  UNUSED_VARIABLE(userData);

  if (printRowData->widths == NULL)
  {
    printRowData->widths = (size_t*)calloc(valueCount,sizeof(size_t));
    assert(printRowData->widths != NULL);
  }
  assert(printRowData->widths != NULL);

  for (i = 0; i < valueCount; i++)
  {
    size_t n;
    char   buffer[64];

    printRowData->widths[i] = MAX(stringFormatLengthCodepointsUTF8(values[i].name),printRowData->widths[i]);
    n = 0;
    switch (values[i].type)
    {
      case DATABASE_DATATYPE_NONE:        break;
      case DATABASE_DATATYPE:             break;

      case DATABASE_DATATYPE_PRIMARY_KEY:
      case DATABASE_DATATYPE_KEY:         n = stringFormatLengthCodepointsUTF8("%"PRIi64,values[i].id); break;

      case DATABASE_DATATYPE_BOOL:        n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%s",values[i].b ? "TRUE" : "FALSE"); break;
      case DATABASE_DATATYPE_INT:         n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%d",values[i].i); break;
      case DATABASE_DATATYPE_INT64:       n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%"PRIi64,values[i].i64); break;
      case DATABASE_DATATYPE_UINT:        n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%u",values[i].u); break;
      case DATABASE_DATATYPE_UINT64:      n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%"PRIu64,values[i].u64); break;
      case DATABASE_DATATYPE_DOUBLE:      n = stringFormatLengthCodepointsUTF8(buffer,sizeof(buffer),"%lf",values[i].d); break;
//      case DATABASE_DATATYPE_ENUM = DATABASE_DATATYPE_UINT,
      case DATABASE_DATATYPE_DATETIME:    n = stringFormatLengthCodepointsUTF8(Misc_formatDateTimeCString(buffer,sizeof(buffer),values[i].dateTime,NULL)); break;
      case DATABASE_DATATYPE_STRING:      n = String_lengthCodepointsUTF8(values[i].string); break;
      case DATABASE_DATATYPE_CSTRING:     n = stringLengthCodepointsUTF8(values[i].s); break;
      case DATABASE_DATATYPE_BLOB:        break;
      default:                            break;
    }
    printRowData->widths[i] = MAX(n,printRowData->widths[i]);
  }

  return ERROR_NONE;
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

LOCAL Errors printRow(const DatabaseValue values[], uint valueCount, void *userData)
{
  PrintRowData *printRowData = (PrintRowData*)userData;
  uint         i;

  assert(values != NULL);
  assert(printRowData != NULL);

  UNUSED_VARIABLE(userData);

  if (printRowData->showHeaderFlag && !printRowData->printedHeaderFlag)
  {
    uint n;

    assert(printRowData->widths != NULL);

    n = 0;
    for (i = 0; i < valueCount; i++)
    {
      if (i > 0)
      {
         printf(" ");
         n += 1;
      }
      printf("%s",values[i].name); printChars(' ',printRowData->widths[i]-stringLength(values[i].name));
      n += printRowData->widths[i];
    }
    printf("\n");
    printChars('-',n); printf("\n");

    printRowData->printedHeaderFlag = TRUE;
  }
  for (i = 0; i < valueCount; i++)
  {
    char       buffer[64];
    const char *s;
    size_t     n;

    s = NULL;
    switch (values[i].type)
    {
      case DATABASE_DATATYPE_NONE:        break;
      case DATABASE_DATATYPE:             break;

      case DATABASE_DATATYPE_PRIMARY_KEY:
      case DATABASE_DATATYPE_KEY:         s = stringFormat(buffer,sizeof(buffer),"%"PRIi64,values[i].id); break;

      case DATABASE_DATATYPE_BOOL:        s = stringFormat(buffer,sizeof(buffer),"%s",values[i].b ? "TRUE" : "FALSE"); break;
      case DATABASE_DATATYPE_INT:         s = stringFormat(buffer,sizeof(buffer),"%d",values[i].i); break;
      case DATABASE_DATATYPE_INT64:       s = stringFormat(buffer,sizeof(buffer),"%"PRIi64,values[i].i64); break;
      case DATABASE_DATATYPE_UINT:        s = stringFormat(buffer,sizeof(buffer),"%u",values[i].u); break;
      case DATABASE_DATATYPE_UINT64:      s = stringFormat(buffer,sizeof(buffer),"%"PRIu64,values[i].u64); break;
      case DATABASE_DATATYPE_DOUBLE:      s = stringFormat(buffer,sizeof(buffer),"%lf",values[i].d); break;
//      case DATABASE_DATATYPE_ENUM = DATABASE_DATATYPE_UINT,
      case DATABASE_DATATYPE_DATETIME:    s = Misc_formatDateTimeCString(buffer,sizeof(buffer),values[i].dateTime,NULL); break;
      case DATABASE_DATATYPE_STRING:      s = String_cString(values[i].string); break;
      case DATABASE_DATATYPE_CSTRING:     s = values[i].s; break;
      case DATABASE_DATATYPE_BLOB:        break;
      default:                            break;
    }
    if (s != NULL)
    {
      n = stringLength(s);
      (void)fwrite(s,n,1,stdout);
      if (printRowData->showHeaderFlag)
      {
        assert(printRowData->widths[i] >= n);
        printChars(' ',printRowData->widths[i]-n);
      }
      else
      {
        putc(' ',stdout);
      }
    }
  }
  printf("\n");

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : printIndexInfo
* Purpose: print index info
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printIndexInfo(DatabaseHandle *databaseHandle)
{
  Errors error;
  uint   n;

  // show meta data
  printf("Meta:\n");
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  %-16s: %s\n",
                                String_cString(values[0].string),
                                String_cString(values[1].string)
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "meta"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_STRING("name"),
                         DATABASE_COLUMN_STRING("value")
                       ),
                       DATABASE_FILTERS_NONE,
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError("get meta data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  printf("Entities:");
  if (verboseFlag)
  {
    // show number of entities
    error = Database_getUInt(databaseHandle,
                             &n,
                             "entities",
                             "COUNT(id)",
                             "id!=0 AND deletedFlag!=TRUE",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      printError("get entities data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %u",n);
  }
  printf("\n");

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "type=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(CHUNK_CONST_ARCHIVE_TYPE_NORMAL)
                           ),
                           NULL  // group
                           );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Normal          : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "type=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(CHUNK_CONST_ARCHIVE_TYPE_FULL)
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Full            : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "type=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(CHUNK_CONST_ARCHIVE_TYPE_DIFFERENTIAL)
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Differential    : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "type=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(CHUNK_CONST_ARCHIVE_TYPE_INCREMENTAL)
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Incremental     : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "type=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS)
                           ),
                           NULL  // group
                           );
  if (error!= ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Continuous      : %u\n",n);

  printf("Storages:");
  if (verboseFlag)
  {
    // show number of storages
    error = Database_getUInt(databaseHandle,
                             &n,
                             "storages",
                             "COUNT(id)",
                             "deletedFlag!=TRUE",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %u",n);
  }
  printf("\n");

  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "state=? AND deletedFlag!=TRUE",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_STATE_OK),
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  OK              : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "state=? AND deletedFlag!=TRUE",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_STATE_UPDATE_REQUESTED)
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Update requested: %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "state=? AND deletedFlag!=TRUE",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_STATE_ERROR)
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Error           : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "deletedFlag!=TRUE",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  printf("  Deleted         : %u\n",n);

  printf("Entries:");
  if (verboseFlag)
  {
    // show number of entries
    error = Database_getUInt(databaseHandle,
                             &n,
                             "entries",
                             "COUNT(id)",
                             DATABASE_FILTERS_NONE,
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %u",n);
  }
  printf("\n");

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Total           : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalEntryCount)"),
                         DATABASE_COLUMN_UINT64("SUM(totalEntrySize)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Files           : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalFileCount)"),
                         DATABASE_COLUMN_UINT64("SUM(totalFileSize)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Images          : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalImageCount)"),
                         DATABASE_COLUMN_UINT64("SUM(totalImageSize)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Directories     : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalDirectoryCount)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Links           : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalLinkCount)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Hardlinks       : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalHardlinkCount)"),
                         DATABASE_COLUMN_UINT64("SUM(totalHardlinkSize)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Special         : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalSpecialCount)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  printf("Newest entries:");
  if (verboseFlag)
  {
    // show number of newest entries
    error = Database_getUInt(databaseHandle,
                             &n,
                             "entriesNewest",
                             "COUNT(id)",
                             DATABASE_FILTERS_NONE,
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %u",n);
  }
  printf("\n");

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Total           : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalEntryCountNewest)"),
                         DATABASE_COLUMN_UINT64("SUM(totalEntrySizeNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Files           : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalFileCountNewest)"),
                         DATABASE_COLUMN_UINT64("SUM(totalFileSizeNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Images          : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalImageCountNewest)"),
                         DATABASE_COLUMN_UINT64("SUM(totalImageSizeNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Directories     : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalDirectoryCountNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Links           : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalLinkCountNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Hardlinks       : %u, %.1lf %s (%"PRIu64" bytes)\n",
                                values[0].u,
                                getByteSize(values[1].u64),
                                getByteUnitShort(values[1].u64),
                                values[1].u64
                               );

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalHardlinkCountNewest)"),
                         DATABASE_COLUMN_UINT64("SUM(totalHardlinkSizeNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("  Special         : %u\n",values[0].u);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                        "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("SUM(totalSpecialCountNewest)")
                       ),
                       "deletedFlag!=TRUE",
                       DATABASE_FILTERS
                       (
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
}

/***********************************************************************\
* Name   : printUUIDsInfo
* Purpose: print UUIDs index info
* Input  : databaseHandle - database handle
*          uuidIds        - array with UUID ids or empty array
*          uuIds          - array with UUIDs or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printUUIDsInfo(DatabaseHandle *databaseHandle, const Array uuidIds, const Array uuids)
{
  String       uuidIdsString,uuidsString;
  char         s[MISC_UUID_STRING_LENGTH];
  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  ulong        i;
  DatabaseId   uuidId;
  Errors       error;
  char         filterString[1024];

  uuidIdsString = String_new();
  uuidsString   = String_new();
  ARRAY_ITERATE(&uuidIds,i,uuidId)
  {
    if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
    String_formatAppend(uuidIdsString,"%"PRIi64,uuidId);
  }
  ARRAY_ITERATE(&uuids,i,s)
  {
    String_setBuffer(uuid,s,MISC_UUID_STRING_LENGTH);
    if (!String_isEmpty(uuidsString)) String_appendChar(uuidsString,',');
    String_formatAppend(uuidsString,"'%S'",uuid);
  }

  printf("UUIDs:\n");
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         DatabaseId uuidId;
                         uint       totalEntryCount;
                         uint64     totalEntrySize;
                         uint       totalFileCount;
                         uint64     totalFileSize;
                         uint       totalImageCount;
                         uint64     totalImageSize;
                         uint       totalDirectoryCount;
                         uint       totalLinkCount;
                         uint       totalHardlinkCount;
                         uint64     totalHardlinkSize;
                         uint       totalSpecialCount;
                         const char *prefix;
                         uint       i;

                         assert(values != NULL);
                         assert(valueCount == 24);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         uuidId              = values[0].id;
                         totalEntryCount     = values[2].u;
                         totalEntrySize      = values[3].u64;
                         totalFileCount      = values[4].u;
                         totalFileSize       = values[5].u64;
                         totalImageCount     = values[6].u;
                         totalImageSize      = values[7].u64;
                         totalDirectoryCount = values[8].u;
                         totalLinkCount      = values[9].u;
                         totalHardlinkCount  = values[10].u;
                         totalHardlinkSize   = values[11].u64;
                         totalSpecialCount   = values[12].u;

                         printf("  Id              : %"PRIi64"\n",uuidId);
                         printf("    UUID          : %s\n",String_cString(values[ 1].string));
                         printf("\n");
                         printf("    Total entries : %u, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                         printf("\n");
                         printf("    Files         : %u, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                         printf("    Images        : %u, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                         printf("    Directories   : %u\n",totalDirectoryCount);
                         printf("    Links         : %u\n",totalLinkCount);
                         printf("    Hardlinks     : %u, %.1lf %s (%"PRIu64" bytes)\n",totalHardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                         printf("    Special       : %u\n",totalSpecialCount);
                         printf("\n");

                         i      = 0;
                         prefix = "    Entity ids    : ";
                         Database_get(databaseHandle,
                                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                      {
                                        assert(values != NULL);
                                        assert(valueCount == 1);

                                        UNUSED_VARIABLE(valueCount);
                                        UNUSED_VARIABLE(userData);

                                        if      (i > 10)
                                        {
                                          putchar('\n');
                                          i      = 0;
                                          prefix = "                    ";
                                        }

                                        if      (i == 0) printf("%s",prefix);
                                        else if (i >  0) printf("%s",", ");
                                        printf("%"PRIi64,values[0].id);
                                        i++;

                                        return ERROR_NONE;
                                      },NULL),
                                      NULL,  // changedRowCount
                                      DATABASE_TABLES
                                      (
                                        "entities"
                                      ),
                                      DATABASE_FLAG_NONE,
                                      DATABASE_COLUMNS
                                      (
                                        DATABASE_COLUMN_KEY("id")
                                      ),
                                      "    uuidId=? \
                                       AND deletedFlag!=TRUE \
                                      ",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY  (uuidId)
                                      ),
                                      NULL,  // groupBy
                                      NULL,  // orderBy
                                      0LL,
                                      DATABASE_UNLIMITED
                                     );
                         if (i > 0) putchar('\n');

                         i      = 0;
                         prefix = "    Storage ids   : ";
                         Database_get(databaseHandle,
                                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                      {
                                        assert(values != NULL);
                                        assert(valueCount == 1);

                                        UNUSED_VARIABLE(valueCount);
                                        UNUSED_VARIABLE(userData);

                                        if      (i > 10)
                                        {
                                          putchar('\n');
                                          i      = 0;
                                          prefix = "                    ";
                                        }

                                        if      (i == 0) printf("%s",prefix);
                                        else if (i >  0) printf("%s",", ");
                                        printf("%"PRIi64,values[0].id);
                                        i++;

                                        return ERROR_NONE;
                                      },NULL),
                                      NULL,  // changedRowCount
                                      DATABASE_TABLES
                                      (
                                        "storages"
                                      ),
                                      DATABASE_FLAG_NONE,
                                      DATABASE_COLUMNS
                                      (
                                        DATABASE_COLUMN_KEY("id")
                                      ),
                                      "    uuidId=? \
                                       AND deletedFlag!=TRUE \
                                      ",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY  (uuidId)
                                      ),
                                      NULL,  // groupBy
                                      NULL,  // orderBy
                                      0LL,
                                      DATABASE_UNLIMITED
                                     );
                         if (i > 0) putchar('\n');

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "uuids"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY   ("id"),
                         DATABASE_COLUMN_STRING("jobUUID"),

                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalEntryCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalEntrySize) FROM entities WHERE entities.uuidId=uuids.id)"),

                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalFileCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalFileSize) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalImageCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalImageSize) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalDirectoryCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalLinkCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalHardlinkCount) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalHardlinkSize) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalSpecialCount) FROM entities WHERE entities.uuidId=uuids.id)"),

                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalEntryCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalEntrySizeNewest) FROM entities WHERE entities.uuidId=uuids.id)"),

                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalFileCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalFileSizeNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalImageCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalImageSizeNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalDirectoryCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalLinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalHardlinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT64("(SELECT SUM(totalHardlinkSizeNewest) FROM entities WHERE entities.uuidId=uuids.id)"),
                         DATABASE_COLUMN_UINT  ("(SELECT SUM(totalSpecialCountNewest) FROM entities WHERE entities.uuidId=uuids.id)")
                       ),
                       stringFormat(filterString,sizeof(filterString),
                                    "    (? OR id IN (%s)) \
                                     AND (? OR jobUUID IN (%s)) \
                                    ",
                                    !Array_isEmpty(&uuidIds) ? String_cString(uuidIdsString) : "0",
                                    !Array_isEmpty(&uuids  ) ? String_cString(uuidsString  ) : "''"
                                   ),
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_BOOL  (Array_isEmpty(&uuidIds)),
                         DATABASE_FILTER_BOOL  (Array_isEmpty(&uuids))
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError("get UUID data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // free resources
  String_delete(uuidsString);
  String_delete(uuidIdsString);
}

/***********************************************************************\
* Name   : printEntitiesInfo
* Purpose: print entities index info
* Input  : databaseHandle - database handle
*          entityIds      - database entity ids array or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printEntitiesInfo(DatabaseHandle *databaseHandle, const Array entityIds)
{
  const char *TYPE_NAMES[] = {"none","normal","full","incremental","differential","continuous"};

  String     entityIdsString;
  ulong      i;
  DatabaseId entityId;
  Errors     error;
  char       filterString[1024];

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%"PRIi64,entityId);
  }

  printf("Entities:\n");
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
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
                         const char *prefix;
                         uint       i;

                         assert(values != NULL);
                         assert(valueCount == 27);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         entityId            = values[0].id;
                         type                = values[1].u;
                         totalEntryCount     = values[4].u;
                         totalEntrySize      = values[5].u64;
                         totalFileCount      = values[6].u;
                         totalFileSize       = values[7].u64;
                         totalImageCount     = values[8].u;
                         totalImageSize      = values[9].u64;
                         totalDirectoryCount = values[10].u;
                         totalLinkCount      = values[11].u;
                         totalhardlinkCount  = values[12].u;
                         totalHardlinkSize   = values[13].u64;
                         totalSpecialCount   = values[14].u;

                         uuidId              = values[26].id;

                         printf("  Id              : %"PRIi64"\n",entityId);
                         printf("    Type          : %s\n",(type <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS) ? TYPE_NAMES[type] : "xxx");//TODO values[ 1]);
                         printf("    Job UUID      : %s\n",String_cString(values[ 2].string));
                         printf("    Schedule UUID : %s\n",String_cString(values[ 3].string));
                         printf("\n");
                         printf("    Total entries : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                         printf("\n");
                         printf("    Files         : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                         printf("    Images        : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                         printf("    Directories   : %lu\n",totalDirectoryCount);
                         printf("    Links         : %lu\n",totalLinkCount);
                         printf("    Hardlinks     : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalhardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                         printf("    Special       : %lu\n",totalSpecialCount);
                         printf("\n");
                         printf("    UUID id       : %"PRIi64"\n",uuidId);

                         i      = 0;
                         prefix = "    Storage ids   : ";
                         Database_get(databaseHandle,
                                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                      {
                                        assert(values != NULL);
                                        assert(valueCount == 1);

                                        UNUSED_VARIABLE(valueCount);
                                        UNUSED_VARIABLE(userData);

                                        if      (i > 10)
                                        {
                                          putchar('\n');
                                          i      = 0;
                                          prefix = "                    ";
                                        }

                                        if      (i == 0) printf("%s",prefix);
                                        else if (i >  0) printf("%s",", ");
                                        printf("%"PRIi64,values[0].id);
                                        i++;

                                        return ERROR_NONE;
                                      },NULL),
                                      NULL,  // changedRowCount
                                      DATABASE_TABLES
                                      (
                                        "storages"
                                      ),
                                      DATABASE_FLAG_NONE,
                                      DATABASE_COLUMNS
                                      (
                                        DATABASE_COLUMN_KEY  ("id")
                                      ),
                                      "    entityId=? \
                                       AND deletedFlag!=TRUE \
                                      ",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY  (entityId)
                                      ),
                                      NULL,  // groupBy
                                      NULL,  // orderBy
                                      0LL,
                                      DATABASE_UNLIMITED
                                     );
                         if (i > 0) putchar('\n');

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entities"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY   ("id"),
                         DATABASE_COLUMN_UINT  ("type"),
                         DATABASE_COLUMN_STRING("jobUUID"),
                         DATABASE_COLUMN_STRING("scheduleUUID"),

                         DATABASE_COLUMN_UINT  ("totalEntryCount"),
                         DATABASE_COLUMN_UINT64("totalEntrySize"),

                         DATABASE_COLUMN_UINT  ("totalFileCount"),
                         DATABASE_COLUMN_UINT64("totalFileSize"),
                         DATABASE_COLUMN_UINT  ("totalImageCount"),
                         DATABASE_COLUMN_UINT64("totalImageSize"),
                         DATABASE_COLUMN_UINT  ("totalDirectoryCount"),
                         DATABASE_COLUMN_UINT  ("totalLinkCount"),
                         DATABASE_COLUMN_UINT  ("totalHardlinkCount"),
                         DATABASE_COLUMN_UINT64("totalHardlinkSize"),
                         DATABASE_COLUMN_UINT  ("totalSpecialCount"),

                         DATABASE_COLUMN_UINT  ("totalEntryCountNewest"),
                         DATABASE_COLUMN_UINT64("totalEntrySizeNewest"),

                         DATABASE_COLUMN_UINT  ("totalFileCountNewest"),
                         DATABASE_COLUMN_UINT64("totalFileSizeNewest"),
                         DATABASE_COLUMN_UINT  ("totalImageCountNewest"),
                         DATABASE_COLUMN_UINT64("totalImageSizeNewest"),
                         DATABASE_COLUMN_UINT  ("totalDirectoryCountNewest"),
                         DATABASE_COLUMN_UINT  ("totalLinkCountNewest"),
                         DATABASE_COLUMN_UINT  ("totalHardlinkCountNewest"),
                         DATABASE_COLUMN_UINT64("totalHardlinkSizeNewest"),
                         DATABASE_COLUMN_UINT  ("totalSpecialCountNewest"),

                         DATABASE_COLUMN_KEY   ("uuidId")
                       ),
                       stringFormat(filterString,sizeof(filterString),
                                    "    (? OR id IN (%s)) \
                                     AND deletedFlag!=TRUE \
                                    ",
                                    !Array_isEmpty(&entityIds) ? String_cString(entityIdsString) : "0"
                                   ),
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_BOOL  (Array_isEmpty(&entityIds))
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError("get entity data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // free resources
  String_delete(entityIdsString);
}

/***********************************************************************\
* Name   : printStoragesInfo
* Purpose: print storages index info
* Input  : databaseHandle - database handle
*          storageIds     - datbase storage ids array or empty array
*          lostFlag       - TRUE to print lost storages
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printStoragesInfo(DatabaseHandle *databaseHandle, const Array storageIds, ConstString name, bool lostFlag)
{
  const char *STATE_TEXT[] = {"","OK","create","update requested","update","error"};
  const char *MODE_TEXT [] = {"manual","auto"};

// TODO:
#define INDEX_CONST_MODE_MANUAL 0
#define INDEX_CONST_MODE_AUTO 1

  String     storageIdsString;
  String     ftsName,ftsSubSelect;
  ulong      i;
  DatabaseId storageId;
  Errors     error;
  char       filterString[1024];
  char       buffer[64];

  storageIdsString = String_new();
  ARRAY_ITERATE(&storageIds,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%"PRIi64,storageId);
  }

  ftsName = getFTSString(String_new(),name);

  ftsSubSelect = String_new();
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      String_format(ftsSubSelect,"SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S'",ftsName);
      break;
    case DATABASE_TYPE_MARIADB:
      #if defined(HAVE_MARIADB)
        String_format(ftsSubSelect,"SELECT id FROM storages WHERE MATCH(name) AGAINST ('%S')",ftsName);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
    case DATABASE_TYPE_POSTGRESQL:
      #if defined(HAVE_POSTGRESQL)
// TODO:
//        String_format(ftsSubSelect,"xxxx",ftsName);
//        textsearchable_index_col @@ to_tsquery('create & table')
      #else /* HAVE_POSTGRESQL */
      #endif /* HAVE_POSTGRESQL */
      break;
  }

  printf("%s:\n",lostFlag ? "Lost storages" : "Storages");
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         DatabaseId storageId;

                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         storageId = values[0].id;

                         error = Database_get(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
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

                                                assert(values != NULL);
                                                assert(valueCount == 36);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                state               = values[10].u;
                                                mode                = values[11].u;
                                                totalEntryCount     = values[14].u;
                                                totalEntrySize      = values[15].u64;
                                                totalFileCount      = values[16].u;
                                                totalFileSize       = values[17].u64;
                                                totalImageCount     = values[18].u;
                                                totalImageSize      = values[19].u64;
                                                totalDirectoryCount = values[20].u;
                                                totalLinkCount      = values[21].u64;
                                                totalHardlinkCount  = values[22].u;
                                                totalHardlinkSize   = values[23].u64;
                                                totalSpecialCount   = values[24].u;

                                                printf("  Id              : %"PRIi64"\n",values[ 0].id);
                                                printf("    Name          : %s\n",String_cString(values[ 5].string));
                                                printf("    Created       : %s\n",Misc_formatDateTimeCString(buffer,sizeof(buffer),values[ 6].dateTime,NULL));
                                                printf("    Host name     : %s\n",String_cString(values[ 7].string));
                                                printf("    User name     : %s\n",String_cString(values[ 8].string));
                                                printf("    Comment       : %s\n",String_cString(values[ 9].string));
                                                printf("    State         : %s\n",(state <= INDEX_CONST_STATE_ERROR) ? STATE_TEXT[state] : "xxx");//TODO values[10].i);
                                                printf("    Mode          : %s\n",(mode <= INDEX_CONST_MODE_AUTO) ? MODE_TEXT[mode] : "xxx");//TODO values[11].i);
                                                printf("    Last checked  : %s\n",Misc_formatDateTimeCString(buffer,sizeof(buffer),values[12].dateTime,NULL));
                                                printf("    Error message : %s\n",(values[13].s != NULL) ? values[13].s : "");
                                                printf("\n");
                                                printf("    Total entries : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                                                printf("\n");
                                                printf("    Files         : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                                                printf("    Images        : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                                                printf("    Directories   : %lu\n",totalDirectoryCount);
                                                printf("    Links         : %lu\n",totalLinkCount);
                                                printf("    Hardlinks     : %lu, %.1lf%s (%"PRIu64" bytes)\n",totalHardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                                                printf("    Special       : %lu\n",totalSpecialCount);
                                                printf("\n");
                                                printf("    UUID id       : %"PRIi64"\n",values[1].id);
                                                printf("    Entity id     : %"PRIi64"\n",values[2].id);
                                                printf("    Job UUID      : %s\n",String_cString(values[3].string));
                                                printf("    Schedule UUID : %s\n",String_cString(values[4].string));

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              DATABASE_TABLES
                                              (
                                                "storages \
                                                   LEFT JOIN entities ON entities.id=storages.entityId \
                                                   LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                                "
                                              ),
                                              DATABASE_FLAG_NONE,
                                              DATABASE_COLUMNS
                                              (
                                                DATABASE_COLUMN_KEY     ("storages.id"),
                                                DATABASE_COLUMN_KEY     ("storages.uuidId"),
                                                DATABASE_COLUMN_KEY     ("storages.entityId"),
                                                DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                                                DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                                                DATABASE_COLUMN_STRING  ("storages.name"),
                                                DATABASE_COLUMN_DATETIME("storages.created"),
                                                DATABASE_COLUMN_STRING  ("storages.hostName"),
                                                DATABASE_COLUMN_STRING  ("storages.userName"),
                                                DATABASE_COLUMN_STRING  ("storages.comment"),
                                                DATABASE_COLUMN_UINT    ("storages.state"),
                                                DATABASE_COLUMN_UINT    ("storages.mode"),
                                                DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                                                DATABASE_COLUMN_STRING  ("storages.errorMessage"),

                                                DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalEntrySize"),

                                                DATABASE_COLUMN_UINT    ("storages.totalFileCount"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalFileSize"),
                                                DATABASE_COLUMN_UINT    ("storages.totalImageCount"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalImageSize"),
                                                DATABASE_COLUMN_UINT    ("storages.totalDirectoryCount"),
                                                DATABASE_COLUMN_UINT    ("storages.totalLinkCount"),
                                                DATABASE_COLUMN_UINT    ("storages.totalHardlinkCount"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalHardlinkSize"),
                                                DATABASE_COLUMN_UINT    ("storages.totalSpecialCount"),

                                                DATABASE_COLUMN_UINT    ("storages.totalEntryCountNewest"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalEntrySizeNewest"),

                                                DATABASE_COLUMN_UINT    ("storages.totalFileCountNewest"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalFileSizeNewest"),
                                                DATABASE_COLUMN_UINT    ("storages.totalImageCountNewest"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalImageSizeNewest"),
                                                DATABASE_COLUMN_UINT    ("storages.totalDirectoryCountNewest"),
                                                DATABASE_COLUMN_UINT    ("storages.totalLinkCountNewest"),
                                                DATABASE_COLUMN_UINT    ("storages.totalHardlinkCountNewest"),
                                                DATABASE_COLUMN_UINT64  ("storages.totalHardlinkSizeNewest"),
                                                DATABASE_COLUMN_UINT    ("storages.totalSpecialCountNewest")
                                              ),
                                              stringFormat(filterString,sizeof(filterString),
                                                           "    storages.id=? \
                                                            AND (? OR storages.id IN (%s)) \
                                                           ",
                                                           !String_isEmpty(name) ? String_cString(ftsSubSelect) : "0"
                                                          ),
                                              DATABASE_FILTERS
                                              (
                                                DATABASE_FILTER_KEY  (!lostFlag ? storageId : DATABASE_ID_NONE),
                                                DATABASE_FILTER_BOOL (String_isEmpty(name))
                                              ),
                                              NULL,  // groupBy
                                              NULL,  // orderBy
                                              0LL,
                                              DATABASE_UNLIMITED
                                             );
                         if (error != ERROR_NONE)
                         {
                           return error;
                         }

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "storages"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY("id")
                       ),
                       stringFormat(filterString,sizeof(filterString),
                                    "    (   (NOT ? AND (? OR id IN (%s))) \
                                          OR (    ? AND (id=?)) \
                                         ) \
                                     AND deletedFlag!=TRUE \
                                    ",
                                    !Array_isEmpty(&storageIds) ? String_cString(storageIdsString) : "0"
                                   ),
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_BOOL  (lostFlag),
                         DATABASE_FILTER_BOOL  (Array_isEmpty(&storageIds)),
                         DATABASE_FILTER_BOOL  (lostFlag),
                         DATABASE_FILTER_KEY   (DATABASE_ID_NONE)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // free resources
  String_delete(ftsSubSelect);
  String_delete(ftsName);
  String_delete(storageIdsString);
}

/***********************************************************************\
* Name   : printEntriesInfo
* Purpose: print entries index info
* Input  : databaseHandle - database handle
*          entityIds      - database entity ids array or empty array
*          entryType      - entry type; see INDEX_CONST_TYPE_...
*          name           - name or NULL
*          lostFlag       - TRUE to print lost entries
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printEntriesInfo(DatabaseHandle *databaseHandle, const Array entityIds, uint entryType, ConstString name, bool lostFlag)
{
  const char *TYPE_TEXT[] = {"","uuid","entity","storage","entry","file","image","directory","link","hardlink","special","history"};

  String     entityIdsString;
  String     ftsName,ftsSubSelect;
  ulong      i;
  DatabaseId entityId;
  Errors     error;
  char       filterString[1024];

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%"PRIi64,entityId);
  }

  ftsName = getFTSString(String_new(),name);

  ftsSubSelect = String_new();
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      String_format(ftsSubSelect,"SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S'",ftsName);
      break;
    case DATABASE_TYPE_MARIADB:
      #if defined(HAVE_MARIADB)
        String_format(ftsSubSelect,"SELECT id FROM entries WHERE MATCH(name) AGAINST ('%S')",ftsName);
      #else /* HAVE_MARIADB */
      #endif /* HAVE_MARIADB */
      break;
    case DATABASE_TYPE_POSTGRESQL:
      #if defined(HAVE_POSTGRESQL)
// TODO:
String_format(ftsSubSelect,"SELECT id FROM entries");
//        textsearchable_index_col @@ to_tsquery('create & table')
      #else /* HAVE_POSTGRESQL */
      #endif /* HAVE_POSTGRESQL */
      break;
  }

  printf("%s:\n",lostFlag ? "Lost entries" : "Entries");
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         DatabaseId entityId;
                         DatabaseId uuidId;
                         bool       entityOutputFlag;

                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         entityId = values[0].id;
                         uuidId   = values[1].id;

                         entityOutputFlag = FALSE;
                         error = Database_get(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                              {
                                                uint type;

                                                assert(values != NULL);
                                                assert(valueCount == 13);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                type = (uint)values[2].u;
                                                if (!entityOutputFlag)
                                                {
                                                  printf("  Entity id: %"PRIi64"\n",entityId);
                                                  printf("  UUID id  : %"PRIi64"\n",uuidId);
                                                  entityOutputFlag = TRUE;
                                                }
                                                printf("    Id               : %"PRIi64"\n",values[0].id);
                                                printf("      Name           : %s\n",String_cString(values[1].string));
                                                printf("      Type           : %s\n",(type <= INDEX_CONST_TYPE_HISTORY) ? TYPE_TEXT[type] : "xxx");//TODO values[2].i);
                                                switch (type)
                                                {
                                                  case INDEX_CONST_TYPE_FILE:
                                                    printf("      Size           : %"PRIi64"\n",values[ 3].u64);
                                                    printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                    printf("      Fragment offset: %"PRIi64"\n",values[11].u64);
                                                    printf("      Fragment size  : %"PRIi64"\n",values[12].u64);
                                                    printf("      Storage id:    : %"PRIi64"\n",values[10].id);
                                                    break;
                                                  case INDEX_CONST_TYPE_IMAGE:
                                                    printf("      Size           : %"PRIi64"\n",values[ 4].u64);
                                                    printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                    printf("      Fragment offset: %"PRIi64"\n",values[11].u64);
                                                    printf("      Fragment size  : %"PRIi64"\n",values[12].u64);
                                                    printf("      Storage id:    : %"PRIi64"\n",values[10].id);
                                                    break;
                                                  case INDEX_CONST_TYPE_DIRECTORY:
                                                    printf("      Storage id:    : %"PRIi64"\n",values[ 5].id);
                                                    break;
                                                  case INDEX_CONST_TYPE_LINK:
                                                    printf("      Storage id:    : %"PRIi64"\n",values[ 6].id);
                                                    break;
                                                  case INDEX_CONST_TYPE_HARDLINK:
                                                    printf("      Size           : %"PRIi64"\n",values[ 7].u64);
                                                    printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                    printf("      Fragment offset: %"PRIi64"\n",values[11].u64);
                                                    printf("      Fragment size  : %"PRIi64"\n",values[12].u64);
                                                    printf("      Storage id:    : %"PRIi64"\n",values[10].id);
                                                    break;
                                                    break;
                                                  case INDEX_CONST_TYPE_SPECIAL:
                                                    printf("      Storage id:    : %"PRIi64"\n",values[ 8].id);
                                                    break;
                                                  default:
                                                    break;
                                                }

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              DATABASE_TABLES
                                              (
                                                "entries \
                                                   LEFT JOIN fileEntries      ON fileEntries.entryId     =entries.id \
                                                   LEFT JOIN imageEntries     ON imageEntries.entryId    =entries.id \
                                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                                   LEFT JOIN linkEntries      ON linkEntries.entryId     =entries.id \
                                                   LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId =entries.id \
                                                   LEFT JOIN specialEntries   ON specialEntries.entryId  =entries.id \
                                                   LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                "
                                              ),
                                              DATABASE_FLAG_NONE,
                                              DATABASE_COLUMNS
                                              (
                                                DATABASE_COLUMN_KEY   ("entries.id"),
                                                DATABASE_COLUMN_STRING("entries.name"),
                                                DATABASE_COLUMN_UINT  ("entries.type"),

                                                DATABASE_COLUMN_UINT64("fileEntries.size"),
                                                DATABASE_COLUMN_UINT64("imageEntries.size"),
                                                DATABASE_COLUMN_KEY   ("directoryEntries.storageId"),
                                                DATABASE_COLUMN_KEY   ("linkEntries.storageId"),
                                                DATABASE_COLUMN_UINT64("hardlinkEntries.size"),
                                                DATABASE_COLUMN_KEY   ("specialEntries.storageId"),
                                                DATABASE_COLUMN_KEY   ("entryFragments.id"),
                                                DATABASE_COLUMN_KEY   ("entryFragments.storageId"),
                                                DATABASE_COLUMN_UINT64("entryFragments.offset"),
                                                DATABASE_COLUMN_UINT64("entryFragments.size")
                                              ),
                                              stringFormat(filterString,sizeof(filterString),
                                                           "    entries.entityId=? \
                                                            AND (? OR (type=?)) \
                                                            AND (? OR entries.id IN (%s)) \
                                                           ",
                                                           !String_isEmpty(name) ? String_cString(ftsSubSelect) : "0"
                                                          ),
                                              DATABASE_FILTERS
                                              (
                                                DATABASE_FILTER_KEY  (!lostFlag ? entityId : DATABASE_ID_NONE),
                                                DATABASE_FILTER_BOOL (entryType == INDEX_CONST_TYPE_ANY),
                                                DATABASE_FILTER_UINT (entryType),
                                                DATABASE_FILTER_BOOL (String_isEmpty(name))
                                              ),
                                              NULL,  // groupBy
                                              NULL,  // orderBy
                                              0LL,
                                              DATABASE_UNLIMITED
                                             );
                         if (error != ERROR_NONE)
                         {
                           return error;
                         }

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entities"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY("id"),
                         DATABASE_COLUMN_KEY("uuidId")
                       ),
                       stringFormat(filterString,sizeof(filterString),
                                    "    (   (NOT ? AND (? OR id IN (%s))) \
                                          OR (    ? AND (id=?)) \
                                         ) \
                                     AND deletedFlag!=TRUE \
                                    ",
                                    !Array_isEmpty(&entityIds) ? String_cString(entityIdsString) : "0"
                                   ),
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_BOOL  (lostFlag),
                         DATABASE_FILTER_BOOL  (Array_isEmpty(&entityIds)),
                         DATABASE_FILTER_BOOL  (lostFlag),
                         DATABASE_FILTER_KEY   (DATABASE_ID_NONE)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError("get entity data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // free resources
  String_delete(ftsSubSelect);
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
    printError("%s",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    printError("%s",Error_getText(error));
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

// TODO: remove
LOCAL void xxx(DatabaseHandle *databaseHandle, DatabaseId storageId, uint show, uint action)
{
  Errors error;
  ulong  n;

  // init variables

  error = Database_createTemporaryTable(databaseHandle,
                                        DATABASE_TEMPORARY_TABLE1,
                                        "storageId       INT, \
                                         entryId         INT, \
                                         name            TEXT, \
                                         timeLastChanged INT, \
                                         entriesNewestId INT DEFAULT 0 \
                                        "
                                       );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    return;
  }

  error = Database_insertSelect(databaseHandle,
                                NULL,  // changedRowCount
                                "%1",
                                DATABASE_FLAG_IGNORE,
                                DATABASE_COLUMNS
                                (
                                  DATABASE_COLUMN_KEY   ("storageId"),
                                  DATABASE_COLUMN_KEY   ("entryId"),
                                  DATABASE_COLUMN_STRING("name"),
                                  DATABASE_COLUMN_UINT64("timeLastChanged")
                                ),
                                DATABASE_TABLES
                                (
                                  "entryFragments \
                                    LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                  ",
                                  "directoryEntries \
                                    LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                  ",
                                  "linkEntries \
                                     LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                  ",
                                  "specialEntries \
                                     LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                  "
                                ),
                                DATABASE_COLUMNS
                                (
                                  DATABASE_COLUMN_KEY   ("storageId"),
                                  DATABASE_COLUMN_KEY   ("entryId"),
                                  DATABASE_COLUMN_STRING("name"),
                                  DATABASE_COLUMN_UINT64("timeLastChanged")
                                ),
                                "storageId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY   (storageId)
                                ),
                                NULL,  // groupBy
                                NULL,  // orderBy
                                0LL,
                                DATABASE_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    return;
  }
  error = Database_update(databaseHandle,
                          NULL,  // changedRowCount
                          "%1",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE("id", "COALESCE((SELECT entriesNewest.id \
                                                            FROM entriesNewest \
                                                            WHERE entriesNewest.entryId=%1.entryId \
                                                              LIMIT 1 \
                                                           ), \
                                                           0 \
                                                          ) \
                                                 "
                                          )
                          ),
                          DATABASE_FILTERS_NONE
                         );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    return;
  }

  if (show == 1)
  {
    printInfo("storages:\n");
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%"PRIi64": %s\n",values[0].id,String_cString(values[1].string));
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_PLAIN("SELECT entryFragments.storageId,storages.name FROM entryFragments \
                                           LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                         WHERE storages.deletedFlag!=TRUE \
                                        "
                                       ),
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_STRING("storageId"),
                           DATABASE_COLUMN_STRING("name")
                         ),
                         DATABASE_FILTERS_NONE,
                         "storageId",  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu storages\n",n);
  }

  if (show == 2)
  {
    printInfo("newest entries:\n");
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%"PRIi64" entryId=%"PRIi64" name=%s timeLastChanged=%"PRIu64"\n",values[0].id,values[1].id,String_cString(values[2].string),values[3].dateTime);
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_PLAIN("      SELECT entryFragments.storageId,entriesNewest.entryId,entriesNewest.name,entriesNewest.timeLastChanged \
                                               FROM entriesNewest \
                                                 LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                                               WHERE entriesNewest.type IN (5,6,9) \
                                               GROUP BY entriesNewest.entryId \
                                         UNION SELECT directoryEntries.storageId,entriesNewest.entryId,entriesNewest.name,entriesNewest.timeLastChanged \
                                               FROM entriesNewest \
                                                 LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                               WHERE entriesNewest.type=7 \
                                         UNION SELECT linkEntries.storageId,entriesNewest.entryId,entriesNewest.name,entriesNewest.timeLastChanged \
                                               FROM entriesNewest \
                                                 LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                               WHERE entriesNewest.type=8 \
                                         UNION SELECT specialEntries.storageId,entriesNewest.entryId,entriesNewest.name,entriesNewest.timeLastChanged \
                                               FROM entriesNewest \
                                                 LEFT JOIN specialEntries ON specialEntries.entryId=entriesNewest.entryId \
                                               WHERE entriesNewest.type=10 \
                                        "
                                       ),
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("storageId"),
                           DATABASE_COLUMN_KEY     ("entryId"),
                           DATABASE_COLUMN_STRING  ("name"),
                           DATABASE_COLUMN_DATETIME("timeLastChanged"),
                         ),
                         DATABASE_FILTERS_NONE,
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu newest entries\n",n);
  }

  if (show == 3)
  {
    printInfo("entries of storage:\n");
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%"PRIi64" entryId=%"PRIi64" name=%s timeLastChanged=%"PRIu64" entriesNewestId=%"PRIi64"\n",values[0].id,values[1].id,values[2].s,values[3].dateTime,values[4].id);
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "%1"
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("storageId"),
                           DATABASE_COLUMN_KEY     ("entryId"),
                           DATABASE_COLUMN_STRING  ("name"),
                           DATABASE_COLUMN_DATETIME("timeLastChanged"),
                           DATABASE_COLUMN_KEY     ("entriesNewestId")
                         ),
                         DATABASE_FILTERS_NONE,
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu entries\n",n);
  }

  if (show == 4)
  {
    printInfo("newest entries of storage:\n");
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%"PRIi64" entryId=%"PRIi64" name=%s timeLastChanged=%"PRIu64"\n",values[0].id,values[1].id,values[2].s,values[3].dateTime);
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "%1",
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("storageId"),
                           DATABASE_COLUMN_KEY     ("entryId"),
                           DATABASE_COLUMN_STRING  ("name"),
                           DATABASE_COLUMN_DATETIME("timeLastChanged")
                         ),
                         "%1.entriesNewestId!=0",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu newest entries\n",n);
  }

  if (show == 5)
  {
fprintf(stderr,"%s, %d: newest entry to remove\n",__FILE__,__LINE__);
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"entryId=%"PRIi64"d name=%s timeLastChanged=%"PRIu64"\n",values[0].id,String_cString(values[1].string),values[2].dateTime);
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "%1"
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("entryId"),
                           DATABASE_COLUMN_STRING  ("name"),
                           DATABASE_COLUMN_DATETIME("timeLastChanged")
                         ),
                         "%1.entriesNewestId!=0",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu newest entries to remove for storage %ld\n",n,storageId);
  }

  if (show == 6)
  {
fprintf(stderr,"%s, %d: newest entry to add from entries\n",__FILE__,__LINE__);
    n = 0;
    error = Database_get(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

fprintf(stdout,"new entryId=%"PRIi64" name=%s timeLastChanged=%"PRIu64"\n",values[0].id,String_cString(values[1].string),values[2].dateTime);
n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entries.id AS entryId,%1.name,entries.timeLastChanged FROM %1 \
                              LEFT JOIN entries ON entries.id=(SELECT id \
                                                               FROM entries \
                                                               WHERE name=%1.name \
                                                               ORDER BY timeLastChanged DESC \
                                                               LIMIT 1 \
                                                              ) \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY     ("entryId"),
                           DATABASE_COLUMN_STRING  ("name"),
                           DATABASE_COLUMN_DATETIME("timeLastChanged")
                         ),
                         "entries.id=%1.entryId",
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
fprintf(stdout,"%lu newest entries to add for storage %ld\n",n,storageId);
  }

  error = Database_dropTemporaryTable(databaseHandle,DATABASE_TEMPORARY_TABLE1);
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    printInfo("FAIL!\n");
    return;
  }

  if      (action == 1)
  {
fprintf(stderr,"%s, %d: add newest entries\n",__FILE__,__LINE__);
    error = addToNewest(databaseHandle,storageId);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
  }
  else if (action == 2)
  {
fprintf(stderr,"%s, %d: remove newest entries\n",__FILE__,__LINE__);
    error = removeFromNewest(databaseHandle,storageId);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
      printInfo("FAIL!\n");
      return;
    }
  }

  // free resources
}

int main(int argc, const char *argv[])
{
  const uint MAX_LINE_LENGTH = 8192;

  Array            uuidIds,uuIds,entityIds,storageIds;
  StaticString     (uuid,MISC_UUID_STRING_LENGTH);
  String           entryName,storageName;
  uint             i,n;
  CStringTokenizer stringTokenizer;
  const char       *token;
  DatabaseId       databaseId;
  const char       *databaseURI;
  String           command;
  char             line[MAX_LINE_LENGTH];
  Errors           error;
  DatabaseHandle   databaseHandle;
  uint64           t0,t1,dt;
  PrintRowData     printRowData;
  String           s;
  const char       *l;
DatabaseId xxxId=DATABASE_ID_NONE;
uint xxxAction=0;
uint xxxShow=0;

  initAll();

  // init variables
  Array_init(&uuidIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&uuIds,MISC_UUID_STRING_LENGTH,64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&entityIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&storageIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  entryName   = String_new();
  storageName = String_new();
  databaseURI = NULL;
  command     = String_new();

// TODO: use CmdOption?parse
#if 0
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       0,0,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(storageName);
    String_delete(entryName);
    exit(EXITCODE_INVALID_ARGUMENT);
  }
#endif

  i = 1;
  n = 0;
  while (i < (uint)argc)
  {
    if      (stringStartsWith(argv[i],"-C="))
    {
      changeToDirectory = &argv[i][3];
      i++;
    }
    else if (stringStartsWith(argv[i],"--directory="))
    {
      changeToDirectory = &argv[i][12];
      i++;
    }
    else if (stringEquals(argv[i],"-C") || stringEquals(argv[i],"--directory"))
    {
      if ((i+1) >= (uint)argc)
      {
        printError("no value for option '%s'!",argv[i]);
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_INVALID_ARGUMENT);
      }
      changeToDirectory = argv[i+1];
      i += 2;
    }
    if      (stringStartsWith(argv[i],"-I="))
    {
      importFileName = &argv[i][3];
      i++;
    }
    else if (stringStartsWith(argv[i],"--import="))
    {
      importFileName = &argv[i][12];
      i++;
    }
    else if (stringEquals(argv[i],"-I") || stringEquals(argv[i],"--import"))
    {
      if ((i+1) >= (uint)argc)
      {
        printError("no value for option '%s'!",argv[i]);
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_INVALID_ARGUMENT);
      }
      importFileName = argv[i+1];
      i += 2;
    }
    else if (stringEquals(argv[i],"--info"))
    {
      infoFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--info-jobs"))
    {
      infoUUIDsFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-jobs="))
    {
      infoUUIDsFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][12],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&uuidIds,&databaseId);
        }
        else
        {
          String_setCString(uuid,token);
          Array_append(&uuIds,String_cString(uuid));
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--info-entities"))
    {
      infoEntitiesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-entities="))
    {
      infoEntitiesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][16],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--info-entries"))
    {
      infoEntriesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-entries="))
    {
      infoEntriesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][15],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIds,&databaseId);
        }
        String_setCString(entryName,token);
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringStartsWith(argv[i],"--entry-type="))
    {
      if      (stringEquals(&argv[i][13],"file"     )) entryType = INDEX_CONST_TYPE_FILE;
      else if (stringEquals(&argv[i][13],"image"    )) entryType = INDEX_CONST_TYPE_IMAGE;
      else if (stringEquals(&argv[i][13],"directory")) entryType = INDEX_CONST_TYPE_DIRECTORY;
      else if (stringEquals(&argv[i][13],"link"     )) entryType = INDEX_CONST_TYPE_LINK;
      else if (stringEquals(&argv[i][13],"harlink"  )) entryType = INDEX_CONST_TYPE_HARDLINK;
      else if (stringEquals(&argv[i][13],"special"  )) entryType = INDEX_CONST_TYPE_SPECIAL;
      else
      {
        printError("unknown value '%s' for option '%s'!",&argv[i][13],argv[i]);
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_INVALID_ARGUMENT);
      }
      i++;
    }
    else if (stringEquals(argv[i],"--info-lost-entries"))
    {
      infoLostEntriesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-lost-entries="))
    {
      infoLostEntriesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][20],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIds,&databaseId);
        }
        String_setCString(entryName,token);
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--info-storages"))
    {
      infoStoragesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--info-storages="))
    {
      infoStoragesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][16],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIds,&databaseId);
        }
        String_setCString(storageName,token);
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--info-lost-storages"))
    {
      infoLostStoragesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--check-integrity"))
    {
      checkIntegrityFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--check-orphaned"))
    {
      checkOrphanedFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--check-duplicates"))
    {
      checkDuplicatesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--check"))
    {
      checkIntegrityFlag  = TRUE;
      checkOrphanedFlag   = TRUE;
      checkDuplicatesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--optimize"))
    {
      optimizeFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--reindex"))
    {
      reindexFlag = TRUE;
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
    else if (stringEquals(argv[i],"--table-names"))
    {
      showTableNames = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--index-names"))
    {
      showIndexNames = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--trigger-names"))
    {
      showTriggerNames = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--drop-tables"))
    {
      dropTablesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--drop-triggers"))
    {
      dropTriggersFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-indizes"))
    {
      createIndizesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--create-fts-indizes"))
    {
      createFTSIndizesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--drop-indizes"))
    {
      dropIndizesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-newest="))
    {
      createNewestFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][stringLength("--create-newest=")],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--create-newest"))
    {
      createNewestFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates-directory-content="))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][stringLength("--create-aggregates-directory-content=")],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--create-aggregates-directory-content"))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates-entities="))
    {
      createAggregatesEntitiesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][stringLength("--create-aggregates-entities=")],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&entityIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--create-aggregates-entities"))
    {
      createAggregatesEntitiesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates-storages="))
    {
      createAggregatesStoragesFlag = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][stringLength("--create-aggregates-storages=")],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--create-aggregates-storages"))
    {
      createAggregatesStoragesFlag = TRUE;
      i++;
    }
    else if (stringStartsWith(argv[i],"--create-aggregates="))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      createAggregatesEntitiesFlag         = TRUE;
      createAggregatesStoragesFlag         = TRUE;
      stringTokenizerInit(&stringTokenizer,&argv[i][stringLength("--create-aggregates=")],",");
      while (stringGetNextToken(&stringTokenizer,&token))
      {
        if (stringToInt64(token,&databaseId))
        {
          Array_append(&storageIds,&databaseId);
        }
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--create-aggregates"))
    {
      createAggregatesDirectoryContentFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--clean-orphaned"))
    {
      cleanOrphanedFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--clean-duplicates"))
    {
      cleanDuplicatesFlag = TRUE;
      i++;
    }
    else if (stringEquals(argv[i],"--clean"))
    {
      cleanOrphanedFlag   = TRUE;
      cleanDuplicatesFlag = TRUE;
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
    else if (stringEquals(argv[i],"--transaction"))
    {
      transactionFlag = FALSE;
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
        printError("expected path name for option --tmp-directory!");
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
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
    else if (stringEquals(argv[i],"-q") || stringEquals(argv[i],"--quiet"))
    {
      verboseFlag = FALSE;
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
    else if (stringEquals(argv[i],"--version"))
    {
      printf("BAR index version %s\n",VERSION_STRING);
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuIds);
      Array_done(&uuidIds);
      String_delete(command);
      String_delete(storageName);
      String_delete(entryName);
      exit(EXITCODE_OK);
    }
    else if (stringEquals(argv[i],"-h") || stringEquals(argv[i],"--help"))
    {
      printUsage(argv[0],FALSE);
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuIds);
      Array_done(&uuidIds);
      String_delete(command);
      String_delete(storageName);
      String_delete(entryName);
      exit(EXITCODE_OK);
    }
    else if (stringEquals(argv[i],"--xhelp"))
    {
      printUsage(argv[0],TRUE);
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuIds);
      Array_done(&uuidIds);
      String_delete(command);
      String_delete(storageName);
      String_delete(entryName);
      exit(EXITCODE_OK);
    }
    else if (stringEquals(argv[i],"-"))
    {
      pipeFlag = TRUE;
      i++;
    }
else if (stringEquals(argv[i],"--xxx"))
{
  xxxId = atoi(argv[i+1]);
  xxxShow = (argc >= (int)i+3) ? atoi(argv[i+2]) : 0;
  xxxAction = (argc >= (int)i+4) ? atoi(argv[i+3]) : 0;
  i++;
  i++;
  i++;
  i++;
}
    else if (stringEquals(argv[i],"--"))
    {
      i++;
      break;
    }
    else if (stringStartsWith(argv[i],"-"))
    {
      printError("unknown option '%s'!",argv[i]);
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuIds);
      Array_done(&uuidIds);
      String_delete(command);
      String_delete(storageName);
      String_delete(entryName);
      exit(EXITCODE_INVALID_ARGUMENT);
    }
    else
    {
      switch (n)
      {
        case 0:
          databaseURI = argv[i];
          n++;
          i++;
          break;
        default:
          if (vacuumFlag)
          {
            toFileName = argv[i];
          }
          else
          {
            if (!String_isEmpty(command)) String_appendChar(command,' ');
            String_appendCString(command,argv[i]);
            jobUUID = argv[i];
          }
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
        databaseURI = argv[i];
        n++;
        i++;
        break;
      default:
        if (vacuumFlag)
        {
          toFileName = argv[i];
        }
        else
        {
          if (!String_isEmpty(command)) String_appendChar(command,' ');
          String_appendCString(command,argv[i]);
          jobUUID = argv[i];
        }
        i++;
        break;
    }
  }

  // check arguments
  if (databaseURI == NULL)
  {
    printError("no database file name given!");
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(storageName);
    String_delete(entryName);
    exit(EXITCODE_INVALID_ARGUMENT);
  }

  if (!stringIsEmpty(changeToDirectory))
  {
    error = File_changeDirectoryCString(changeToDirectory);
    if (error != ERROR_NONE)
    {
      printError(_("Cannot change to directory '%s' (error: %s)!"),
                 changeToDirectory,
                 Error_getText(error)
                );
    }
  }

  // open database
  error = openDatabase(&databaseHandle,databaseURI,createFlag);
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(storageName);
    String_delete(entryName);
    exit(EXITCODE_FAIL);
  }
  error = Database_setEnabledForeignKeys(&databaseHandle,foreignKeysFlag);
  if (error != ERROR_NONE)
  {
    printError("Cannot set foreign key support (error: %s)",Error_getText(error));
    closeDatabase(&databaseHandle);
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(storageName);
    String_delete(entryName);
    exit(EXITCODE_FAIL);
  }

  // set temporary directory
  if (tmpDirectory != NULL)
  {
    error = Database_setTmpDirectory(&databaseHandle,tmpDirectory);
  }
  if (error != ERROR_NONE)
  {
    printError("%s",Error_getText(error));
    closeDatabase(&databaseHandle);
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(storageName);
    String_delete(entryName);
    exit(EXITCODE_FAIL);
  }

  // output info
  if (   infoFlag
      || (   (importFileName == NULL)
          && !infoUUIDsFlag
          && !infoEntitiesFlag
          && !infoStoragesFlag
          && !infoLostStoragesFlag
          && !infoEntriesFlag
          && !infoLostEntriesFlag
          && !createFlag
          && !showTableNames
          && !showIndexNames
          && !showTriggerNames
          && !checkIntegrityFlag
          && !checkOrphanedFlag
          && !checkDuplicatesFlag
          && !optimizeFlag
          && !reindexFlag
          && !createTriggersFlag
          && !dropTablesFlag
          && !dropTriggersFlag
          && !createIndizesFlag
          && !createFTSIndizesFlag
          && !dropIndizesFlag
          && !createNewestFlag
          && !createAggregatesDirectoryContentFlag
          && !createAggregatesEntitiesFlag
          && !createAggregatesStoragesFlag
          && !cleanOrphanedFlag
          && !cleanDuplicatesFlag
          && !purgeDeletedFlag
          && !vacuumFlag
          && !showStoragesFlag
          && !showEntriesFlag
          && !showEntriesNewestFlag
          && String_isEmpty(command)
          && !pipeFlag
          && !inputAvailable()
//&& (xxxId==DATABASE_ID_NONE)
         )
     )
  {
    printIndexInfo(&databaseHandle);
  }

  if (infoUUIDsFlag)
  {
    printUUIDsInfo(&databaseHandle,uuidIds,uuIds);
  }

  if (infoEntitiesFlag)
  {
    printEntitiesInfo(&databaseHandle,entityIds);
  }

  if      (infoLostEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entityIds,entryType,entryName,TRUE);
  }
  else if (infoEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entityIds,entryType,entryName,FALSE);
  }

  if      (infoLostStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIds,storageName,TRUE);
  }
  else if (infoStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIds,storageName,FALSE);
  }

  if (showTableNames)
  {
    printTableNames(&databaseHandle);
  }
  if (showIndexNames)
  {
    printIndexNames(&databaseHandle);
  }
  if (showTriggerNames)
  {
    printTriggerNames(&databaseHandle);
  }

  // drop tables/views
  if (dropTablesFlag)
  {
    dropTables(&databaseHandle,FALSE);
    dropViews(&databaseHandle,FALSE);
  }

  // drop triggeres
  if (dropTriggersFlag)
  {
    dropTriggers(&databaseHandle,FALSE);
  }

  // drop indizes
  if (dropIndizesFlag)
  {
    dropIndices(&databaseHandle,FALSE);
  }

  // create tables/views/indices/triggers
  if (createFlag)
  {
    error = createTablesViewsIndicesTriggers(&databaseHandle);
  }

  // import
  if (importFileName != NULL)
  {
    importIntoDatabase(&databaseHandle,importFileName);
  }

  // check
  if (checkIntegrityFlag)
  {
    checkIntegrity(&databaseHandle);
  }
  if (checkOrphanedFlag)
  {
    checkOrphanedEntries(&databaseHandle);
  }
  if (checkDuplicatesFlag)
  {
    checkDuplicates(&databaseHandle);
  }

  // recreate triggeres
  if (createTriggersFlag)
  {
    createTriggers(&databaseHandle);
  }

  // recreate indizes
  if (createIndizesFlag)
  {
    createIndizes(&databaseHandle);
  }
  if (createFTSIndizesFlag)
  {
    createFTSIndizes(&databaseHandle);
  }

  // clean
  if (cleanOrphanedFlag)
  {
    cleanOrphanedEntries(&databaseHandle);
  }
  if (cleanDuplicatesFlag)
  {
    cleanDuplicates(&databaseHandle);
  }

  // recreate newest data
  if (createNewestFlag)
  {
    createNewest(&databaseHandle,storageIds);
  }

  // calculate aggregates data
  if (createAggregatesDirectoryContentFlag)
  {
    createAggregatesDirectoryContent(&databaseHandle,entityIds);
  }
  if (createAggregatesStoragesFlag)
  {
    createAggregatesStorages(&databaseHandle,storageIds);
  }
  if (createAggregatesEntitiesFlag)
  {
    createAggregatesEntities(&databaseHandle,entityIds);
  }

  // purge deleted storages
  if (purgeDeletedFlag)
  {
    purgeDeletedStorages(&databaseHandle);
  }

  // vacuum
  if (vacuumFlag)
  {
    vacuum(&databaseHandle,toFileName);
  }

  // re-create existing indizes
  if (reindexFlag)
  {
    reindex(&databaseHandle);
  }

  // optimize
  if (optimizeFlag)
  {
    optimizeDatabase(&databaseHandle);
  }


if (xxxId != DATABASE_ID_NONE)
{
  xxx(&databaseHandle,xxxId,xxxShow,xxxAction);
}

//TODO: remove? use storages-info?
  if (showStoragesFlag)
  {
    uint64 maxIdLength,maxStorageNameLength;
    char   format[256];

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 2);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = log10((double)values[0].id);
                   maxIdLength          = 1+(uint)d;
                   maxStorageNameLength = (uint)values[1].u;

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "storages \
                      LEFT JOIN entities on entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("MAX(storages.id)"),
                   DATABASE_COLUMN_UINT  ("MAX(LENGTH(storages.name))")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 NULL,  // orderBy
                 0LL,
                 1LL
                );

    stringFormat(format,sizeof(format),"%%-%dlld %%-%ds %%-%ds %%-10s\n",maxIdLength,maxStorageNameLength,MISC_UUID_STRING_LENGTH);

    UNUSED_VARIABLE(maxStorageNameLength);
    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   uint       archiveType;
                   const char *s;

                   assert(values != NULL);
                   assert(valueCount == 4);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   archiveType = values[3].u;
                   s = "unknown";
                   for (uint i = CHUNK_CONST_ARCHIVE_TYPE_NONE; i <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS; i++)
                   {
                     if (i == archiveType) s = ARCHIVE_TYPES[i];
                   }

                   printf(format,values[0].id,String_cString(values[1].string),String_cString(values[2].string),s);

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "storages \
                      LEFT JOIN entities on entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("storages.id"),
                   DATABASE_COLUMN_STRING("storages.name"),
                   DATABASE_COLUMN_STRING("entities.jobUUID"),
                   DATABASE_COLUMN_UINT  ("entities.type")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 "storages.name ASC",
                 0LL,
                 DATABASE_UNLIMITED
                );
  }

  if (showEntriesFlag)
  {
    uint64 maxIdLength,maxEntryNameLength,maxStorageNameLength;
    char   format[256];

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = log10((double)values[0].id);
                   maxIdLength          = 1+(uint)d;
                   maxEntryNameLength   = values[1].u;
                   maxStorageNameLength = values[2].u;

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "entryFragments \
                      LEFT JOIN entries ON entries.id=entryFragments.entryId \
                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "directoryEntries \
                      LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "linkEntries \
                      LEFT JOIN entries ON entries.id=linkEntries.entryId \
                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "specialEntries \
                      LEFT JOIN entries ON entries.id=specialEntries.entryId \
                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("MAX(entries.id)"),
                   DATABASE_COLUMN_UINT  ("MAX(LENGTH(entries.name))"),
                   DATABASE_COLUMN_UINT  ("MAX(LENGTH(storages.name))")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 NULL,  // orderBy
                 0LL,
                 1LL
                );

    stringFormat(format,sizeof(format),"%%%dlld %%-%ds %%-s\n",maxIdLength,maxEntryNameLength);
    UNUSED_VARIABLE(maxStorageNameLength);
    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   printf(format,values[0].id,String_cString(values[1].string),String_cString(values[2].string));

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "entryFragments \
                      LEFT JOIN entries ON entries.id=entryFragments.entryId \
                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "directoryEntries \
                      LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "linkEntries \
                      LEFT JOIN entries ON entries.id=linkEntries.entryId \
                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "specialEntries \
                      LEFT JOIN entries ON entries.id=specialEntries.entryId \
                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("entries.id"),
                   DATABASE_COLUMN_STRING("entries.name","entryName"),
                   DATABASE_COLUMN_STRING("storages.name")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 "entryName ASC",
                 0LL,
                 DATABASE_UNLIMITED
                );
  }

  if (showEntriesNewestFlag)
  {
    uint64 maxIdLength,maxEntryNameLength,maxStorageNameLength;
    char   format[256];

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = log10((double)values[0].id);
                   maxIdLength          = 1+(uint)d;
                   maxEntryNameLength   = values[1].u;
                   maxStorageNameLength = values[2].u;

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "entryFragments \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "directoryEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "linkEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "specialEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("MAX(entriesNewest.id)"),
                   DATABASE_COLUMN_UINT  ("MAX(LENGTH(entriesNewest.name))"),
                   DATABASE_COLUMN_UINT  ("MAX(LENGTH(storages.name))")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 NULL,  // orderBy
                 0LL,
                 1LL
                );

    stringFormat(format,sizeof(format),"%%%ds %%-%ds %%-s\n",maxIdLength,maxEntryNameLength);
    UNUSED_VARIABLE(maxStorageNameLength);
    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   printf(format,values[0].id,String_cString(values[1].string),String_cString(values[2].string));

                   return ERROR_NONE;
                 },NULL),
                 NULL,  // changedRowCount
                 DATABASE_TABLES
                 (
                   "entryFragments \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "directoryEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "linkEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   ",
                   "specialEntries \
                      LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                      LEFT JOIN entities ON entities.id=storages.entityId \
                   "
                 ),
                 DATABASE_FLAG_NONE,
                 DATABASE_COLUMNS
                 (
                   DATABASE_COLUMN_KEY   ("entriesNewest.id"),
                   DATABASE_COLUMN_STRING("entriesNewest.name","entriesNewestName"),
                   DATABASE_COLUMN_STRING("storages.name")
                 ),
                 "? OR entities.jobUUID=?",
                 DATABASE_FILTERS
                 (
                   DATABASE_FILTER_BOOL   (jobUUID == NULL),
                   DATABASE_FILTER_CSTRING(jobUUID)
                 ),
                 NULL,  // groupBy
                 "entriesNewestName ASC",
                 0LL,
                 DATABASE_UNLIMITED
                );
  }

  if (   !showStoragesFlag
      && !showEntriesFlag
      && !showEntriesNewestFlag
     )
  {
    // execute command
    if (!String_isEmpty(command))
    {
      error = ERROR_NONE;

      s = String_duplicate(command);
      String_replaceAllCString(s,STRING_BEGIN,"%","%%");
      if (explainQueryPlanFlag)
      {
        switch (Database_getType(&databaseHandle))
        {
          case DATABASE_TYPE_SQLITE3:
            String_insertCString(s,STRING_BEGIN,"EXPLAIN QUERY PLAN ");
            break;
          case DATABASE_TYPE_MARIADB:
            #if defined(HAVE_MARIADB)
              String_insertCString(s,STRING_BEGIN,"EXPLAIN ");
            #else /* HAVE_MARIADB */
            #endif /* HAVE_MARIADB */
            break;
          case DATABASE_TYPE_POSTGRESQL:
              String_insertCString(s,STRING_BEGIN,"EXPLAIN ");
            #if defined(HAVE_POSTGRESQL)
            #else /* HAVE_POSTGRESQL */
            #endif /* HAVE_POSTGRESQL */
            break;
        }
      }

      printRowData.showHeaderFlag    = showHeaderFlag;
      printRowData.printedHeaderFlag = FALSE;
      printRowData.widths            = NULL;
      if (showHeaderFlag)
      {
        if (error == ERROR_NONE)
        {
          error = Database_get(&databaseHandle,
                               CALLBACK_(getColumnWidths,&printRowData),
                               NULL,  // changedRowCount
                               DATABASE_TABLES
                               (
                                 String_cString(s)
                               ),
                               DATABASE_FLAG_PLAIN|DATABASE_FLAG_COLUMN_NAMES,
                               DATABASE_COLUMNS_AUTO,
                               DATABASE_FILTERS_NONE,
                               NULL,  // groupBy
                               NULL,  // orderBy
                               0LL,
                               DATABASE_UNLIMITED
                              );
        }
      }
      t0 = 0L;
      t1 = 0L;
      if (error == ERROR_NONE)
      {
        t0 = Misc_getTimestamp();

        error = Database_get(&databaseHandle,
                             CALLBACK_(printRow,&printRowData),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               String_cString(s)
                             ),
                             DATABASE_FLAG_PLAIN|DATABASE_FLAG_COLUMN_NAMES,
                             DATABASE_COLUMNS_AUTO,
                             DATABASE_FILTERS_NONE,
                             NULL,  // groupBy
                             NULL,  // orderBy
                             0LL,
                             DATABASE_UNLIMITED
                            );
        t1 = Misc_getTimestamp();
      }
      freeColumnsWidth(printRowData.widths);

      String_delete(s);
      if (error != ERROR_NONE)
      {
        printError("SQL command '%s' fail: %s!",String_cString(command),Error_getText(error));
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_FAIL);
      }

      dt = t1-t0;
      if (timeFlag && !explainQueryPlanFlag) printf("Execution time: %lumin:%us:%uus\n",(ulong)(dt/US_PER_MINUTE),(uint)((dt%US_PER_MINUTE)/US_PER_S),(uint)(dt%US_PER_S));
    }
  }

  if (pipeFlag)
  {
    if (transactionFlag)
    {
      error = Database_beginTransaction(&databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER);
      if (error != ERROR_NONE)
      {
        printError("Init transaction fail: %s!",Error_getText(error));
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_FAIL);
      }
    }

    while (inputAvailable() && (fgets(line,sizeof(line),stdin) != NULL))
    {
      l = stringTrim(line);
      t0 = Misc_getTimestamp();
      error = Database_get(&databaseHandle,
                           CALLBACK_(printRow,NULL),
                           NULL,  // changedRowCount,
//                           DATABASE_FLAG_COLUMN_NAMES,
                           DATABASE_PLAIN(l),
                           DATABASE_COLUMNS
                           (
                           ),
                           DATABASE_FILTERS_NONE,
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (error == ERROR_NONE)
      {
        if (verboseFlag) fprintf(stderr,"Result: %s\n",Error_getText(error));
      }
      else
      {
        printError("SQL command '%s' fail: %s!",l,Error_getText(error));
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_FAIL);
      }
      t1 = Misc_getTimestamp();

      dt = t1-t0;
      if (timeFlag && !explainQueryPlanFlag) printf("Execution time: %lumin:%us:%uus\n",(ulong)(dt/US_PER_MINUTE),(uint)((dt%US_PER_MINUTE)/US_PER_S),(uint)(dt%US_PER_S));
    }

    if (transactionFlag)
    {
      error = Database_endTransaction(&databaseHandle);
      if (error != ERROR_NONE)
      {
        printError("Done transaction fail: %s!",Error_getText(error));
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
        String_delete(storageName);
        String_delete(entryName);
        exit(EXITCODE_FAIL);
      }
    }
  }

  // close database
  closeDatabase(&databaseHandle);

  // free resources
  Array_done(&storageIds);
  Array_done(&entityIds);
  Array_done(&uuIds);
  Array_done(&uuidIds);
  String_delete(storageName);
  String_delete(entryName);
  String_delete(command);

  doneAll();

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
