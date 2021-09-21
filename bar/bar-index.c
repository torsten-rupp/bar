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

#if defined(HAVE_MYSQL_MYSQL_H) && defined(HAVE_MYSQL_ERRMSG_H)
  #include "mysql/mysql.h"
  #include "mysql/errmsg.h"
#endif /* HAVE_MYSQL_H */

#include "common/global.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/cmdoptions.h"
#include "common/database.h"
#include "common/misc.h"

#include "sqlite3.h"

#include "index.h"
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

#define INDEX_CONST_TYPE_ANY 0

/***************************** Datatypes *******************************/
#if 0
// index types
typedef enum
{
  INDEX_TYPE_NONE      = 0,

  INDEX_TYPE_UUID      = INDEX_CONST_TYPE_UUID,
  INDEX_TYPE_ENTITY    = INDEX_CONST_TYPE_ENTITY,
  INDEX_TYPE_STORAGE   = INDEX_CONST_TYPE_STORAGE,
  INDEX_TYPE_ENTRY     = INDEX_CONST_TYPE_ENTRY,
  INDEX_TYPE_FILE      = INDEX_CONST_TYPE_FILE,
  INDEX_TYPE_IMAGE     = INDEX_CONST_TYPE_IMAGE,
  INDEX_TYPE_DIRECTORY = INDEX_CONST_TYPE_DIRECTORY,
  INDEX_TYPE_LINK      = INDEX_CONST_TYPE_LINK,
  INDEX_TYPE_HARDLINK  = INDEX_CONST_TYPE_HARDLINK,
  INDEX_TYPE_SPECIAL   = INDEX_CONST_TYPE_SPECIAL,
  INDEX_TYPE_HISTORY   = INDEX_CONST_TYPE_HISTORY,

  INDEX_TYPE_ANY       = 0xF
} IndexTypes;
#endif

typedef struct
{
  bool   showHeaderFlag;
  bool   headerPrintedFlag;
  size_t *widths;
} PrintTableData;

#if 0
// TODO: still not used
LOCAL const CommandLineOption COMMAND_LINE_OPTIONS[] = CMD_VALUE_ARRAY
(
  CMD_OPTION_BOOLEAN      ("table-names",                       0  , 1, 0, showTableNames,   "output version"                  ),
  CMD_OPTION_BOOLEAN      ("index-names",                       0  , 1, 0, showIndexNames,   "output version"                  ),
  CMD_OPTION_BOOLEAN      ("trigger-names",                     0  , 1, 0, showTriggerNames, "output version"                  ),
  CMD_OPTION_BOOLEAN      ("drop-tables",                       0  , 1, 0, dropTriggersFlag, "output version"                  ),
  CMD_OPTION_BOOLEAN      ("drop-triggers",                     0  , 1, 0, dropTriggersFlag, "output version"                  ),
  CMD_OPTION_BOOLEAN      ("drop-indizes",                      0  , 1, 0, dropIndizesFlag,  "output version"                  ),
  CMD_OPTION_BOOLEAN      ("help",                              'h', 0, 0, helpFlag,         "output this help"                ),
  CMD_OPTION_BOOLEAN      ("xhelp",                             0,   0, 0, xhelpFlag,        "output help to extended options" ),
);
#endif

// progress info data
typedef struct
{
  const char *text;
  uint64     startTimestamp;
  uint64     steps,maxSteps;
  ulong      lastProgressSum;  // last progress sum [1/1000]
  uint       lastProgressCount;
  uint64     lastProgressTimestamp;
} ProgressInfo;


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
//LOCAL bool       helpFlag                             = FALSE;
//LOCAL bool       xhelpFlag                            = FALSE;

LOCAL ProgressInfo importProgressInfo;

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

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define DIMPORT(format,...) \
    do \
    { \
    } \
    while (0)
#else /* not INDEX_DEBUG_IMPORT_OLD_DATABASE */
  #define DIMPORT(format,...) \
    do \
    { \
    } \
    while (0)
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

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
  printf("     mysql:<server>:<user>:<password>\n");
  printf("\n");
  printf("Options:  -C|--directory=<name>                   - change to directory\n");
  printf("          --info                                  - output index database infos\n");
  printf("          --info-jobs[=<uuid id>|UUID,...]        - output index database job infos\n");
  printf("          --info-entities[=<entity id>,...]       - output index database entities infos\n");
  printf("          --info-storages[=<storage id>,...]      - output index database storages infos\n");
  printf("          --info-lost-storages[=<storage id>,...] - output index database storages infos without an entity\n");
  printf("          --info-entries[=<entity id>,...|name]   - output index database entries infos\n");
  printf("          --info-lost-entries[=<entity id>,...]   - output index database entries infos without an entity\n");
  printf("          --entry-type=<type>                     - entries type:\n");
  printf("                                                      file\n");
  printf("                                                      image\n");
  printf("                                                      directory\n");
  printf("                                                      link\n");
  printf("                                                      harlink\n");
  printf("                                                      special\n");
  printf("          --create                                - create new index database\n");
  printf("          --create-triggers                       - re-create triggers\n");
  printf("          --create-indizes                        - re-create indizes\n");
  printf("          --create-fts-indizes                  - re-create FTS indizes (full text search)\n");
  printf("          --create-newest[=id,...]                - re-create newest data\n");
  printf("          --create-aggregates                     - re-create aggregated data\n");
  printf("          --create-aggregates-directory-content   - re-create aggregated data directory content\n");
  printf("          --create-aggregates-entities            - re-create aggregated data entities\n");
  printf("          --create-aggregates-storages            - re-create aggregated data storages\n");
  printf("          --import <URI>                          - import database\n");
  printf("          --optimize                              - optimize database (analyze and collect statistics data)\n");
  printf("          --reindex                               - re-create all existing indizes\n");
  printf("          --check                                 - check index database\n");
  printf("          --check-integrity                       - check index database integrity\n");
  printf("          --check-orphaned                        - check index database for orphaned entries\n");
  printf("          --check-duplicates                      - check index database for duplicate entries\n");
  printf("          --clean                                 - clean index database\n");
  printf("          --clean-orphaned                        - clean orphaned in index database\n");
  printf("          --clean-duplicates                      - clean duplicates in index database\n");
  printf("          --purge                                 - purge deleted storages\n");
  printf("          --vacuum [<new file name>]              - collect and free unused file space\n");
  printf("          -s|--storages [<uuid>]                  - print storages\n");
  printf("          -e|--entries [<uuid>]                   - print entries\n");
  printf("          --entries-newest [<uuid>]               - print newest entries\n");
  printf("          -n|--names                              - print values with names\n");
  printf("          -H|--header                             - print headers\n");
  printf("          --transaction                           - enable transcations\n");
  printf("          -f|--no-foreign-keys                    - disable foreign key constraints\n");
  printf("          --force                                 - force operation\n");
  printf("          --pipe|-                                - read data from stdin and pipe into database\n");
  printf("          --tmp-directory                         - temporary files directory\n");
  printf("          -v|--verbose                            - verbose output (default: ON; deprecated)\n");
  printf("          -q|--quiet                              - no output\n");
  printf("          -t|--time                               - print execution time\n");
  printf("          -x|--explain-query                      - explain SQL queries\n");
  if (extendedFlag)
  {
    printf("          --table-names                           - show table names\n");
    printf("          --index-names                           - show index names\n");
    printf("          --trigger-names                         - show trigger names\n");
    printf("          --drop-tables                           - drop all tables\n");
    printf("          --drop-triggers                         - drop all triggers\n");
    printf("          --drop-indizes                          - drop all indixes\n");
  }
  printf("          -h|--help                               - print this help\n");
  printf("          --xhelp                                 - print extended help\n");
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
* Name   : printInfo
* Purpose: print info (if verbose)
* Input  : format - printf-format string
*          ...    - optional arguments
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
    vfprintf(stdout,format,arguments); fflush(stdout);
    va_end(arguments);
  }
}

/***********************************************************************\
* Name   : printError
* Purpose: print error message
* Input  : format - printf-format string
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printError(const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  fprintf(stderr,"ERROR: ");
  vfprintf(stderr,format,arguments);
  fprintf(stderr,"\n");
  va_end(arguments);
}

/***********************************************************************\
* Name   : printWarning
* Purpose: print warning message
* Input  : format - printf-format string
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printWarning(const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  fprintf(stderr,"Warning: ");
  vfprintf(stderr,format,arguments);
  fprintf(stderr,"\n");
  va_end(arguments);
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
* Input  : databaseFileName - database file name
*          createFlag       - TRUE to create database if it does not
*                             exists
* Output : databaseHandle   - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors openDatabase(DatabaseHandle *databaseHandle, const char *databaseFileName, bool createFlag)
{
  DatabaseOpenModes openMode;
  Errors            error;

  // open database
  printInfo("Open database '%s'...",databaseFileName);
  openMode = (forceFlag)
               ? DATABASE_OPENMODE_FORCE_CREATE
               : DATABASE_OPENMODE_READWRITE;
  openMode |= DATABASE_OPENMODE_AUX;
  error = Database_open(databaseHandle,
                        databaseFileName,
                        openMode,
                        WAIT_FOREVER
                       );
  if (error != ERROR_NONE)
  {
    if (createFlag)
    {
      error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE|DATABASE_OPENMODE_AUX,WAIT_FOREVER);
    }
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("cannot open database '%s' (error: %s)!",databaseFileName,Error_getText(error));
    return error;
  }
  printInfo("OK  \n");

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
* Name   : createTablesIndicesTriggers
* Purpose: create database with tables/indices/triggers
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createTablesIndicesTriggers(DatabaseHandle *databaseHandle)
{
  Errors     error;
  const char *indexDefinition;

  // create tables, triggers
  printInfo("Create tables/indices/triggers...");
  error = ERROR_NONE;
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITIONS[Database_getType(databaseHandle)],
                             indexDefinition,
                             error == ERROR_NONE
                            )
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             indexDefinition
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
* Name   : fixBrokenIds
* Purpose: fix broken ids
* Input  : indexHandle - index handle
*          tableName   - table name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fixBrokenIds(IndexHandle *indexHandle, const char *tableName)
{
}

/***********************************************************************\
* Name   : initProgress
* Purpose: init progress
* Input  : text - text
* Output : progressInfo - progress info
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initProgress(ProgressInfo *progressInfo, const char *text)
{
}

/***********************************************************************\
* Name   : resetProgress
* Purpose: RESET progress
* Input  : progressInfo - progress info
*          maxSteps     - max. number of steps
* Output :
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void resetProgress(ProgressInfo *progressInfo, uint64 maxSteps)
{
}

/***********************************************************************\
* Name   : doneProgress
* Purpose: done progress
* Input  : progressInfo - progress info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneProgress(ProgressInfo *progressInfo)
{
}

/***********************************************************************\
* Name   : progressStep
* Purpose: step progress and log
* Input  : userData - user data (progress info)
* Output : -
* Return : -
* Notes  : increment step counter for each call!
\***********************************************************************/

LOCAL void progressStep(void *userData)
{
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
  vprintf(format,arguments);
  va_end(arguments);
  printf("\n");
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

Errors initEntity(DatabaseHandle *oldDatabaseHandle,
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
                             "id=%lld",
                             storageId
                            ) != ERROR_NONE
         )
      || (Database_getId(oldDatabaseHandle,
                         entityId,
                         "entities",
                         "id",
                         "jobUUID=%'S",
                         jobUUID
                        ) != ERROR_NONE
         )
     )
  {
    Misc_getUUID(jobUUID);
    error = Database_execute(newDatabaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "INSERT OR IGNORE INTO uuids \
                                ( \
                                 jobUUID \
                                ) \
                              VALUES \
                                ( \
                                 %'S \
                                ) \
                             ",
                             jobUUID
                            );

    // get uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(newDatabaseHandle,
                             &uuidId,
                             "uuids",
                             "id",
                             "WHERE jobUUID=%'S \
                             ",
                             jobUUID
                            );
    }

    // create entity
    if (error == ERROR_NONE)
    {
      error = Database_execute(newDatabaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
                                   uuidId, \
                                   jobUUID, \
                                   type, \
                                   lockedCount \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %'S, \
                                   %u, \
                                   1 \
                                  ) \
                               ",
                               uuidId,
                               jobUUID,
                               ARCHIVE_TYPE_FULL
                              );
      return error;
    }

    if (error == ERROR_NONE)
    {
      (*entityId) = Database_getLastRowId(newDatabaseHandle);
    }
  }

  return error;
}

Errors unlockEntity(DatabaseHandle *databaseHandle,
                    DatabaseId     entityId
                   )
{
  Errors error;

  assert(databaseHandle != NULL);

  return Database_execute(databaseHandle,
                          CALLBACK_(NULL,NULL),  // databaseRowFunction
                          NULL,  // changedRowCount
                          DATABASE_COLUMN_TYPES(),
                          "UPDATE entities SET lockedCount=lockedCount-1 WHERE id=%lld AND lockedCount>0;",
                          entityId
                         );
}

#include "index_current.c"

/***********************************************************************\
* Name   : importIntoDatabase
* Purpose: import database
* Input  : databaseHandle - database handle
*          uir            - database URI
* Output :
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors importIntoDatabase(DatabaseHandle *databaseHandle, const char *uri)
{
  Errors         error;
  DatabaseHandle oldDatabaseHandle;

  error = Database_open(&oldDatabaseHandle,uri,DATABASE_OPENMODE_READ,WAIT_FOREVER);
  if (error == ERROR_NONE)
  {
    error = importIndexVersion7XXX(&oldDatabaseHandle, databaseHandle);

    Database_close(&oldDatabaseHandle);
  }

  return error;
}

/***********************************************************************\
* Name   : checkDatabaseIntegrity
* Purpose: check database integrity
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void checkDatabaseIntegrity(DatabaseHandle *databaseHandle)
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
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "fileEntries",
                                "COUNT(id)",
                                "WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  image entries without fragments...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "imageEntries",
                                "COUNT(id)",
                                "WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (uint64)n;
  printInfo("  hardlink entries without fragments...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "hardlinkEntries",
                                "COUNT(id)",
                                "WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM fileEntries WHERE fileEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_FILE
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without image entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM imageEntries WHERE imageEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_IMAGE
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without directory entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM directoryEntries WHERE directoryEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_DIRECTORY
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without link entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM linkEntries WHERE linkEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_LINK
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without hardlink entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM hardlinkEntries WHERE hardlinkEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_HARDLINK
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  entries without special entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entries",
                                "COUNT(id)",
                                "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM specialEntries WHERE specialEntries.entryId=entries.id LIMIT 0,1)",
                                INDEX_CONST_TYPE_SPECIAL
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check storages without name
  printInfo("  storages without name...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "storages",
                                "COUNT(id)",
                                "WHERE name IS NULL OR name=''"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check FTS entries without entry
  printInfo("  FTS entries without entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "FTS_entries",
                                "COUNT(entryId)",
                                "WHERE NOT EXISTS(SELECT id FROM entries WHERE entries.id=FTS_entries.entryId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check FTS storages without storage
  printInfo("  FTS storages without storage...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "FTS_storages",
                                "COUNT(storageId)",
                                "WHERE NOT EXISTS(SELECT id FROM storages WHERE storages.id=FTS_storages.storageId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

  // check newest entries without entry
  printInfo("  newest entries without entry...");
  error = Database_getInteger64(databaseHandle,
                                &n,
                                "entriesNewest",
                                "COUNT(id)",
                                "WHERE NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 0,1)"
                               );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError("orphaned check fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += (ulong)n;

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
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             if (String_equalsCString(name,values[0].text.data))
                             {
                               n++;
                             }
                             String_setCString(name,values[0].text.data);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(TEXT),
                           "SELECT name FROM storages \
                            WHERE deletedFlag!=1 \
                            ORDER BY name \
                           "
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%lld\n",n);
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
  StringList       nameList;
  Errors           error;
  ulong            n;
  const StringNode *stringNode;
  ConstString      name;

  // init variables
  StringList_init(&nameList);

  printInfo("Optimize:\n");

  printInfo("  Tables...");
  StringList_clear(&nameList);
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             StringList_appendCString(&nameList,values[0].text.data);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(TEXT),
                           "SELECT name FROM sqlite_master WHERE type='table'"
                          );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("get tables fail (error: %s)!",Error_getText(error));
    return;
  }
  n = 0;
  STRINGLIST_ITERATE(&nameList,stringNode,name)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "ANALYZE %s",
                             String_cString(name)
                            );
    if (error != ERROR_NONE)
    {
      break;
    }

    n++;
    printPercentage(n,StringList_count(&nameList));
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("optimize table '%s' fail (error: %s)!",String_cString(name),Error_getText(error));
    StringList_done(&nameList);
    return;
  }
  printInfo("OK  \n");

  printInfo("  Indizes...");
  StringList_clear(&nameList);
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             StringList_appendCString(&nameList,values[0].text.data);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(TEXT),
                           "SELECT name FROM sqlite_master WHERE type='index'"
                          );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("get indizes fail (error: %s)!",Error_getText(error));
    return;
  }
  n = 0;
  STRINGLIST_ITERATE(&nameList,stringNode,name)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "ANALYZE %s",
                             String_cString(name)
                            );
    if (error != ERROR_NONE)
    {
      break;
    }

    n++;
    printPercentage(n,StringList_count(&nameList));
  }
  clearPercentage();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("optimize index '%s' fail (error: %s)!",String_cString(name),Error_getText(error));
    StringList_done(&nameList);
    return;
  }
  printInfo("OK  \n");

  // free resources
  StringList_done(&nameList);
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
// TODO:
  char   name[1024];
  const char *indexDefinition;

  printInfo("Create triggers...");

  // delete all existing triggers
// TODO:
error = ERROR_NONE;
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_TRIGGER_NAMES_SQLITE, name, error == ERROR_NONE)
  {
  }
  do
  {
    stringClear(name);
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               stringSet(name,sizeof(name),values[0].text.data);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(TEXT),
                             "SELECT name FROM sqlite_master WHERE type='trigger' AND name LIKE 'trigger%%'"
                            );

    if ((error == ERROR_NONE) && !stringIsEmpty(name))
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DROP TRIGGER %s",
                               name
                              );
    }
  }
  while ((error == ERROR_NONE) && !stringIsEmpty(name));
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create triggers fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // create new triggeres
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_TRIGGERS_SQLITE, indexDefinition)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             indexDefinition
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
  const char *name;
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_TABLE_NAMES[Database_getType(databaseHandle)],name)
  {
    printf("%s\n",name);
  }
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_FTS_TABLE_NAMES[Database_getType(databaseHandle)],name)
  {
    printf("%s\n",name);
  }
// TODO: remove/move to database.c
#if 0
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           printf("%s\n",values[0].text.data);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_COLUMN_TYPES(TEXT),
                         "SELECT name FROM sqlite_master WHERE type='table'"
                        );
#endif
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
  const char *name;
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_INDEX_NAMES[Database_getType(databaseHandle)],name)
  {
    printf("%s\n",name);
  }
// TODO: remove/move to database.c
#if 0
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           printf("%s\n",values[0]);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_COLUMN_TYPES(),
                         "SELECT name FROM sqlite_master WHERE type='index'"
                        );
#endif
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
  const char *name;
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_TRIGGER_NAMES[Database_getType(databaseHandle)],name)
  {
    printf("%s\n",name);
  }
// TODO: remove/move to database.c
#if 0
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           printf("%s\n",values[0]);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_COLUMN_TYPES(),
                         "SELECT name FROM sqlite_master WHERE type='trigger'"
                        );
#endif
}
/***********************************************************************\
* Name   : dropTables
* Purpose: drop all tables
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropTables(DatabaseHandle *databaseHandle)
{
  Errors     error;
  const char *tableName;
  uint       failCount;

  // delete all existing triggers
  printInfo("Drop tables...");
  failCount = 0;
  INDEX_DEFINITIONS_ITERATE(INDEX_DEFINITION_TABLE_NAMES[Database_getType(databaseHandle)], tableName)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DROP TABLE %s",
                             tableName
                            );
    if (error != ERROR_NONE)
    {
      if (failCount == 0) printInfo("FAIL\n");
      printWarning("drop table '%s' fail (error: %s)!", tableName, Error_getText(error));
      failCount++;
    }
  }
  if (failCount == 0)
  {
    printInfo("OK\n");
  }
}

/***********************************************************************\
* Name   : dropTriggers
* Purpose: drop all triggers
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropTriggers(DatabaseHandle *databaseHandle)
{
  Errors error;
  char   name[1024];
  uint   failCount;

  // delete all existing triggers
  printInfo("Drop triggers...");
  failCount = 0;
// TODO:
  do
  {
    stringClear(name);
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               stringSet(name,sizeof(name),values[0].text.data);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(TEXT),
                             "SELECT name FROM sqlite_master WHERE type='trigger' AND name LIKE 'trigger%%'"
                            );

    if ((error == ERROR_NONE) && !stringIsEmpty(name))
    {
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DROP TRIGGER %s",
                               name
                              );
      if (error != ERROR_NONE)
      {
        if (failCount == 0) printInfo("FAIL\n");
        printWarning("drop triggers fail (error: %s)!",Error_getText(error));
        failCount++;
      }
    }
  }
  while (!stringIsEmpty(name));
  if (failCount == 0)
  {
    printInfo("OK\n");
  }
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
  char   name[1024];

  /* manual forced delete
  PRAGMA writable_schema = ON;
  DELETE FROM sqlite_master WHERE type = 'table' AND name = 'tablename';
  */

  printInfo("Create indizes:\n");

  error = ERROR_NONE;
// TODO:
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop all existing indizes
    if (error == ERROR_NONE)
    {
      printInfo("  Discard indizes...");
      do
      {
        stringClear(name);
        error = Database_execute(databaseHandle,
                                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                 {
                                   assert(values != NULL);
                                   assert(valueCount == 1);

                                   UNUSED_VARIABLE(valueCount);
                                   UNUSED_VARIABLE(userData);

                                   stringSet(name,sizeof(name),values[0].text.data);

                                   return ERROR_NONE;
                                 },NULL),
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(TEXT),
                                 "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'index%%' LIMIT 0,1"
                                );
        if ((error == ERROR_NONE) && !stringIsEmpty(name))
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   DATABASE_COLUMN_TYPES(),
                                   "DROP INDEX IF EXISTS %s",
                                   name
                                  );
        }
      }
      while ((error == ERROR_NONE) && !stringIsEmpty(name));
      if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }

    // create new indizes (if not exists)
    if (error == ERROR_NONE)
    {
      const char *indexDefinition;

      printInfo("  Collect indizes...");
      INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_INDICES[Database_getType(databaseHandle)], indexDefinition, error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 indexDefinition
                                );
      }
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }

    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printError("recreate indizes fail (error: %s)!",Error_getText(error));
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
    if (error == ERROR_NONE)
    {
      const char *name;

      printInfo("  Discard FTS indizes...");
      INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_FTS_TABLE_NAMES[Database_getType(databaseHandle)], name, error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DROP TABLE IF EXISTS %s",
                                 name
                                );
      }
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }

    // create new FTS tables (if not exists)
    if (error == ERROR_NONE)
    {
      const char *indexDefinition;

      printInfo("  Create FTS indizes...");
      INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_FTS_TABLES_MYSQL, indexDefinition, error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 indexDefinition
                                );
      }
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }

    // create FTS index
    if (error == ERROR_NONE)
    {
      printInfo("  Collect storages FTS index...");
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "INSERT INTO FTS_storages SELECT id,name FROM storages"
                              );
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }
    if (error == ERROR_NONE)
    {
      printInfo("  Collect entries FTS index...");
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "INSERT INTO FTS_entries SELECT id,name FROM entries"
                              );
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }

    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printError("recreate FTS indizes fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);
}

/***********************************************************************\
* Name   : dropIndizes
* Purpose: drop all indizes
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void dropIndizes(DatabaseHandle *databaseHandle)
{
  Errors error;
  char   name[1024];

  printInfo("Drop indizes...");

  error = ERROR_NONE;

  // delete all existing indizes
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop all indizes
    if (error == ERROR_NONE)
    {
      do
      {
        stringClear(name);
// TODO: replace by Database_getCString()
        error = Database_execute(databaseHandle,
                                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                 {
                                   assert(values != NULL);
                                   assert(valueCount == 1);

                                   UNUSED_VARIABLE(valueCount);
                                   UNUSED_VARIABLE(userData);

                                   stringSet(name,sizeof(name),values[0].text.data);

                                   return ERROR_NONE;
                                 },NULL),
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(TEXT),
                                 "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'index%%' LIMIT 0,1"
                                );
        if ((error == ERROR_NONE) && !stringIsEmpty(name))
        {
          printInfo("  drop %s...",name);
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   DATABASE_COLUMN_TYPES(),
                                   "DROP INDEX IF EXISTS %s",
                                   name
                                  );
          printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
        }
      }
      while ((error == ERROR_NONE) && !stringIsEmpty(name));
      if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);
    }

    // drop FTS indizes
    if (error == ERROR_NONE)
    {
      printInfo("  drop FTS_storages...");
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DROP TABLE IF EXISTS FTS_storages"
                              );
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }
    if (error == ERROR_NONE)
    {
      printInfo("  drop FTS_entries...");
      error = Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "DROP TABLE IF EXISTS FTS_entries"
                              );
      printInfo("%s\n",(error == ERROR_NONE) ? "OK" : "FAIL");
    }
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("drop indizes fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  printInfo("OK\n");
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
* Name   : addStorageToNewest
* Purpose: add storage entries to newest entries (if newest)
* Input  : databaseHandle - database handle
*          storageId      - storage database id
*          progressInfo   - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addStorageToNewest(DatabaseHandle *databaseHandle, DatabaseId storageId)
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

  EntryList entryList;
  Errors    error;
  EntryNode *entryNode;

  assert(databaseHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  error = ERROR_NONE;

  // get entries info to add
  if (error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               EntryNode *entryNode;

                               assert(values != NULL);
                               assert(valueCount == 10);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               entryNode = LIST_NEW_NODE(EntryNode);
                               if (entryNode == NULL)
                               {
                                 HALT_INSUFFICIENT_MEMORY();
                               }

                               entryNode->entryId                = values[0].id;
                               entryNode->uuidId                 = values[1].id;
                               entryNode->entityId               = values[2].id;
                               entryNode->indexType              = values[3].i;
                               entryNode->name                   = String_newBuffer(values[4].text.data,values[4].text.length);
                               entryNode->timeLastChanged        = values[5].dateTime;
                               entryNode->userId                 = values[6].i;
                               entryNode->groupId                = values[7].i;
                               entryNode->permission             = values[8].i;
                               entryNode->size                   = values[9].i;
                               entryNode->newest.entryId         = DATABASE_ID_NONE;
                               entryNode->newest.timeLastChanged = 0LL;

                               List_append(&entryList,entryNode);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   KEY,
                                                   KEY,
                                                   INT,
                                                   TEXT,
                                                   DATETIME,
                                                   INT,
                                                   INT,
                                                   INT,
                                                   INT64
                                                  ),
                             "      SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           entries.name, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM entryFragments \
                                      LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                    WHERE entryFragments.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           entries.name, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM directoryEntries \
                                      LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                    WHERE directoryEntries.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           entries.name, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM linkEntries \
                                      LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                    WHERE linkEntries.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           entries.name, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM specialEntries \
                                      LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                    WHERE specialEntries.storageId=%lld \
                              GROUP BY name \
                             ",
                             storageId,
                             storageId,
                             storageId,
                             storageId
                            );
  }
  if (error != ERROR_NONE)
  {
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }
//fprintf(stderr,"%s, %d: add %d\n",__FILE__,__LINE__,List_count(&entryList));

  // find newest entries for entries to add
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               entryNode->newest.entryId         = values[0].id;
                               entryNode->newest.timeLastChanged = values[1].dateTime;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,DATETIME),
                             "      SELECT entriesNewest.id, \
                                           UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                    FROM entryFragments \
                                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                      LEFT JOIN entriesNewest ON entriesNewest.id=entryFragments.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entriesNewest.name=%'S \
                              UNION SELECT entriesNewest.id, \
                                           UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                    FROM directoryEntries \
                                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                      LEFT JOIN entriesNewest ON entriesNewest.id=directoryEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entriesNewest.name=%'S \
                              UNION SELECT entriesNewest.id, \
                                           UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                    FROM linkEntries \
                                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                      LEFT JOIN entriesNewest ON entriesNewest.id=linkEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entriesNewest.name=%'S \
                              UNION SELECT entriesNewest.id, \
                                           UNIXTIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                    FROM specialEntries \
                                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                                      LEFT JOIN entriesNewest ON entriesNewest.id=specialEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entriesNewest.name=%'S \
                              ORDER BY timeLastChanged DESC \
                              LIMIT 0,1 \
                             ",
                             entryNode->name,
                             entryNode->name,
                             entryNode->name,
                             entryNode->name
                            );
  }
  if (error != ERROR_NONE)
  {
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // add entries to newest entries
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
//fprintf(stderr,"%s, %d: %s %llu %llu %d\n",__FILE__,__LINE__,String_cString(entryNode->name),entryNode->timeLastChanged,entryNode->newest.timeLastChanged,entryNode->timeLastChanged > entryNode->newest.timeLastChanged);
      if (entryNode->timeLastChanged > entryNode->newest.timeLastChanged)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "INSERT OR REPLACE INTO entriesNewest \
                                    ( \
                                     entryId, \
                                     uuidId, \
                                     entityId, \
                                     type, \
                                     name, \
                                     timeLastChanged, \
                                     userId, \
                                     groupId, \
                                     permission, \
                                     size \
                                    ) \
                                  VALUES \
                                    ( \
                                      %lld, \
                                      %lld, \
                                      %lld, \
                                      %u, \
                                      %'S, \
                                      %llu, \
                                      %lu, \
                                      %lu, \
                                      %lu, \
                                      %llu \
                                    ) \
                                 ",
                                 NULL,
                                 entryNode->entryId,
                                 entryNode->uuidId,
                                 entryNode->entityId,
                                 entryNode->indexType,
                                 entryNode->name,
                                 entryNode->timeLastChanged,
                                 entryNode->userId,
                                 entryNode->groupId,
                                 entryNode->permission,
                                 entryNode->size
                                );
      }
    }
  }
  if (error != ERROR_NONE)
  {
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // free resources
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : removeStorageFromNewest
* Purpose: remove storage entries from newest entries
* Input  : databaseHandle - database handle
*          storageId      - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeStorageFromNewest(DatabaseHandle *databaseHandle, DatabaseId storageId)
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
//fprintf(stderr,"%s, %d: name=%s t=%s newid=%s newt=%s\n",__FILE__,__LINE__,values[2],values[3],values[4],values[5]);
                               EntryNode *entryNode;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               entryNode = LIST_NEW_NODE(EntryNode);
                               if (entryNode == NULL)
                               {
                                 HALT_INSUFFICIENT_MEMORY();
                               }

                               entryNode->entryId        = values[0].id;
                               entryNode->name           = String_newCString(values[1].text.data);
                               entryNode->newest.entryId = DATABASE_ID_NONE;

                               List_append(&entryList,entryNode);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,TEXT),
                             "      SELECT entries.id, \
                                           entries.name \
                                    FROM entryFragments \
                                      LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                    WHERE entryFragments.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.name \
                                    FROM directoryEntries \
                                      LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                    WHERE directoryEntries.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.name \
                                    FROM linkEntries \
                                      LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                    WHERE linkEntries.storageId=%lld \
                              UNION SELECT entries.id, \
                                           entries.name \
                                    FROM specialEntries \
                                      LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                    WHERE specialEntries.storageId=%lld \
                             ",
                             storageId,
                             storageId,
                             storageId,
                             storageId
                            );
  }
  if (error != ERROR_NONE)
  {
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }
//fprintf(stderr,"%s, %d: remove %d\n",__FILE__,__LINE__,List_count(&entryList));

  // find new newest entries for entries to remove
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 9);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               entryNode->newest.entryId         = values[0].id;
                               entryNode->newest.uuidId          = values[1].id;
                               entryNode->newest.entityId        = values[2].id;
                               entryNode->newest.indexType       = values[3].i;
                               entryNode->newest.timeLastChanged = values[4].dateTime;
                               entryNode->newest.userId          = values[5].i;
                               entryNode->newest.groupId         = values[6].i;
                               entryNode->newest.permission      = values[7].i;
                               entryNode->newest.size            = values[8].i;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   KEY,
                                                   KEY,
                                                   INT,
                                                   DATETIME,
                                                   INT,
                                                   INT,
                                                   INT,
                                                   INT64
                                                  ),
                             "      SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM entryFragments \
                                      LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                      LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entries.name=%'S \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM directoryEntries \
                                      LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                      LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entries.name=%'S \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM linkEntries \
                                      LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                      LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entries.name=%'S \
                              UNION SELECT entries.id, \
                                           entries.uuidId, \
                                           entries.entityId, \
                                           entries.type, \
                                           UNIXTIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                           entries.userId, \
                                           entries.groupId, \
                                           entries.permission, \
                                           entries.size \
                                    FROM specialEntries \
                                      LEFT JOIN storages ON storages.id=specialEntries.storageId \
                                      LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                    WHERE     storages.deletedFlag!=1 \
                                          AND entries.name=%'S \
                              ORDER BY timeLastChanged DESC \
                              LIMIT 0,1 \
                             ",
                             entryNode->name,
                             entryNode->name,
                             entryNode->name,
                             entryNode->name
                            );
  }
  if (error != ERROR_NONE)
  {
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // remove/update entries from newest entries
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
      if (error == ERROR_NONE)
      {
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM entriesNewest \
                                  WHERE entryId=%lld \
                                 ",
                                 entryNode->entryId
                                );
      }

      if (error == ERROR_NONE)
      {
        if (entryNode->newest.entryId != DATABASE_ID_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   DATABASE_COLUMN_TYPES(),
                                   "INSERT OR REPLACE INTO entriesNewest \
                                      ( \
                                       entryId, \
                                       uuidId, \
                                       entityId, \
                                       type, \
                                       name, \
                                       timeLastChanged, \
                                       userId, \
                                       groupId, \
                                       permission, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                        %lld, \
                                        %lld, \
                                        %lld, \
                                        %u, \
                                        %'S, \
                                        %llu, \
                                        %lu, \
                                        %lu, \
                                        %lu, \
                                        %llu \
                                      ) \
                                   ",
                                   entryNode->newest.entryId,
                                   entryNode->newest.uuidId,
                                   entryNode->newest.entityId,
                                   entryNode->newest.indexType,
                                   entryNode->name,
                                   entryNode->newest.timeLastChanged,
                                   entryNode->newest.userId,
                                   entryNode->newest.groupId,
                                   entryNode->newest.permission,
                                   entryNode->newest.size
                                  );
        }
      }
    }
  }

  // free resources
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
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
  ulong         totalEntriesNewestCount;
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
                            "WHERE deletedFlag!=1 \
                             ORDER BY created DESC \
                            "
                           );
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("create newest fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printPercentage(1,2);

    // get total counts
    totalEntriesNewestCount = 0L;
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               totalEntriesNewestCount = values[0].i;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT COUNT(id) FROM entriesNewest \
                             "
                            );
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError("create newest fail (error: %s)!",Error_getText(error));
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
        error = Database_execute(databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 &m,
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM entriesNewest LIMIT 0,1000"
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
      error = addStorageToNewest(databaseHandle,storageId);
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
      printInfo("  %lld...",storageId);
      if (error == ERROR_NONE)
      {
        error = removeStorageFromNewest(databaseHandle,storageId);
      }
      if (error == ERROR_NONE)
      {
        error = addStorageToNewest(databaseHandle,storageId);
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
  ulong  totalCount;
  ulong  n;

  UNUSED_VARIABLE(entityIds);

  // calculate directory content size/count aggregated data
  printInfo("Create aggregates for directory content...");

  // get total count
  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             totalCount = values[0].i;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
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
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  n = 0L;

  // clear directory content size/count aggregated data
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "UPDATE directoryEntries \
                              SET totalEntryCount      =0, \
                                  totalEntrySize       =0, \
                                  totalEntryCountNewest=0, \
                                  totalEntrySizeNewest =0 \
                             "
                            );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  (void)Database_flush(databaseHandle);

  // update directory content size/count aggegrated data: files
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
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
                               name      = String_newCString(values[1].text.data);
                               totalSize = values[2].i;
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
                                                          NULL,
                                                          totalSize,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,TEXT,INT64),
                             "SELECT entryFragments.storageId, \
                                     entries.name, \
                                     SUM(entryFragments.size) \
                              FROM fileEntries \
                                LEFT JOIN entries        ON entries.id=fileEntries.entryId \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              WHERE     entries.id IS NOT NULL \
                                    AND entryFragments.storageId IS NOT NULL \
                              GROUP BY entries.name \
                             "
                            );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_execute(databaseHandle,
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
                               name      = String_newCString(values[1].text.data);
                               totalSize = values[2].i;
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
                             DATABASE_COLUMN_TYPES(KEY,TEXT,INT64),
                             "SELECT entryFragments.storageId, \
                                     entriesNewest.name, \
                                     SUM(entryFragments.size) \
                              FROM fileEntries \
                                LEFT JOIN entriesNewest  ON entriesNewest.entryId=fileEntries.entryId \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                              WHERE     entriesNewest.id IS NOT NULL \
                                    AND entryFragments.storageId IS NOT NULL \
                              GROUP BY entriesNewest.name \
                             "
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

  // update directory content size/count aggregated data: directories
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                                                          NULL,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT directoryEntries.storageId, \
                                     COUNT(entries.id) \
                              FROM directoryEntries \
                                LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                              WHERE entries.id IS NOT NULL \
                              GROUP BY entries.name \
                             "
                            );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT directoryEntries.storageId, \
                                     COUNT(entriesNewest.id) \
                              FROM directoryEntries \
                                LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                              WHERE entriesNewest.id IS NOT NULL \
                              GROUP BY entriesNewest.name \
                             "
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

  // update directory content size/count aggregated data: links
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                                                          NULL,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT linkEntries.storageId, \
                                     COUNT(entries.id) \
                              FROM linkEntries \
                                LEFT JOIN entries ON entries.id=linkEntries.entryId \
                              WHERE entries.id IS NOT NULL \
                              GROUP BY entries.name \
                             "
                            );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT linkEntries.storageId, \
                                     COUNT(entriesNewest.id) \
                              FROM linkEntries \
                                LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                              WHERE entriesNewest.id IS NOT NULL \
                              GROUP BY entriesNewest.name \
                             "
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

  // update directory content size/count aggregated data: hardlinks
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
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
                               name      = String_newCString(values[1].text.data);
                               totalSize = values[2].i;
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT,
                                                   INT64
                                                  ),
                             "SELECT entryFragments.storageId, \
                                     COUNT(entries.id), \
                                     SUM(entryFragments.size) \
                              FROM hardlinkEntries \
                                LEFT JOIN entries        ON entries.id=hardlinkEntries.entryId \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              WHERE     entries.id IS NOT NULL \
                                    AND entryFragments.storageId IS NOT NULL \
                              GROUP BY entries.name \
                             "
                            );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_execute(databaseHandle,
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
                               name      = String_newCString(values[1].text.data);
                               totalSize = values[2].i;
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
                                                          NULL,
                                                          totalSize,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT,
                                                   INT64
                                                  ),
                             "SELECT entryFragments.storageId, \
                                     COUNT(entriesNewest.id), \
                                     SUM(entryFragments.size) \
                              FROM hardlinkEntries \
                                LEFT JOIN entriesNewest  ON entriesNewest.entryId=hardlinkEntries.entryId \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.entryId \
                              WHERE     entriesNewest.id IS NOT NULL \
                                    AND entryFragments.storageId IS NOT NULL \
                              GROUP BY entriesNewest.name \
                             "
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

  // update directory content size/count aggregated data: special
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                                                          NULL,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT specialEntries.storageId, \
                                     COUNT(entries.id) \
                              FROM specialEntries \
                                LEFT JOIN entries ON entries.id=specialEntries.entryId \
                              WHERE entries.id IS NOT NULL \
                              GROUP BY entries.name \
                             "
                            );
    if (error != ERROR_NONE) DATABASE_TRANSACTION_ABORT(databaseHandle);

    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;
                               String     name;

                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;
                               name      = String_newCString(values[1].text.data);
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
                                                          NULL,
                                                          storageId,
                                                          name
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
                             DATABASE_COLUMN_TYPES(KEY,
                                                   INT
                                                  ),
                             "SELECT specialEntries.storageId, \
                                     COUNT(entriesNewest.id) \
                              FROM specialEntries \
                                LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                              WHERE entriesNewest.id IS NOT NULL \
                              GROUP BY entriesNewest.name \
                             "
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
  ulong      totalCount;
  ulong      n;

  // init variables
  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

  printInfo("Create aggregates for entities...");

  // get entities total count
  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             totalCount = values[0].i;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
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
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // update entities total count/size aggregates
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId entityId;

                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               entityId = values[0].id;

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
                                                             totalFileSize      =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entries.entityId=%lld \
                                                                                 ), \
                                                             totalImageSize     =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entries.entityId=%lld \
                                                                                 ), \
                                                             totalHardlinkSize  =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entries.entityId=%lld \
                                                                                 ) \
                                                         WHERE id=%lld \
                                                        ",
                                                        NULL,
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
                                 printInfo("FAIL!\n");
                                 printError("create aggregates fail for entity #%"PRIi64" (error: %s)!",entityId,Error_getText(error));
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
                                                        NULL,
                                                        entityId
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 printInfo("FAIL!\n");
                                 printError("create aggregates fail for entity #%"PRIi64": (error: %s)!",entityId,Error_getText(error));
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
                                                             totalFileSizeNewest      =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                       ), \
                                                             totalImageSizeNewest     =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                       ), \
                                                             totalHardlinkSizeNewest  =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entriesNewest.entityId=%lld \
                                                                                       ) \
                                                         WHERE id=%lld \
                                                        ",
                                                        NULL,
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
                                 printInfo("FAIL!\n");
                                 printError("create newest aggregates fail for entity #%"PRIi64" (error: %s)!",entityId,Error_getText(error));
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
                                                        NULL,
                                                        entityId
                                                       );
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
                             DATABASE_COLUMN_TYPES(KEY),
                             "SELECT id \
                              FROM entities \
                              WHERE     (%d OR id IN (%S)) \
                                    AND deletedFlag!=1 \
                             ", \
                             String_isEmpty(entityIdsString) ? 1 : 0,
                             entityIdsString
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
  ulong      i;
  DatabaseId storageId;
  Errors     error;
  ulong      totalCount;
  ulong      n;

  storageIdsString = String_new();
  ARRAY_ITERATE(&storageIds,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%lld",storageId);
  }

  printInfo("Create aggregates for storages...");

  // get storage total count
  totalCount = 0L;
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             totalCount = values[0].i;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
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
    printError("create aggregates fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  n = 0L;

  // update storage total count/size aggregates
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               DatabaseId storageId;

                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

                               storageId = values[0].id;

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
                                                             totalFileSize      =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                                 ), \
                                                             totalImageSize     =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                                 ), \
                                                             totalHardlinkSize  =(SELECT SUM(entryFragments.size) \
                                                                                  FROM entries \
                                                                                    LEFT JOIN entryFragments   ON entryFragments.entryId  =entries.id \
                                                                                  WHERE entries.type=%d AND entryFragments.storageId=%lld \
                                                                                 ) \
                                                         WHERE id=%lld \
                                                        ",
                                                        NULL,
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
                                 printInfo("FAIL!\n");
                                 printError("create aggregates fail for storage #%"PRIi64" (error: %s)!",storageId,Error_getText(error));
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
                                                        NULL,
                                                        storageId
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 printInfo("FAIL!\n");
                                 printError("create aggregates fail for storage #%"PRIi64": (error: %s)!",storageId,Error_getText(error));
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
                                                             totalFileSizeNewest      =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                       ), \
                                                             totalImageSizeNewest     =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                       ), \
                                                             totalHardlinkSizeNewest  =(SELECT SUM(entryFragments.size) \
                                                                                        FROM entriesNewest \
                                                                                          LEFT JOIN entryFragments   ON entryFragments.entryId  =entriesNewest.entryId \
                                                                                        WHERE entriesNewest.type=%d AND entryFragments.storageId=%lld \
                                                                                       ) \
                                                         WHERE id=%lld \
                                                        ",
                                                        NULL,
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
                                 printInfo("FAIL!\n");
                                 printError("create newest aggregates fail for storage #%"PRIi64" (error: %s)!",storageId,Error_getText(error));
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
                                                        NULL,
                                                        storageId
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 printInfo("FAIL!\n");
                                 printError("create newest aggregates fail for storage #%"PRIi64" (error: %s)!",storageId,Error_getText(error));
                                 return error;
                               }

                               n++;
                               printPercentage(n,totalCount);

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY),
                             "SELECT id \
                              FROM storages \
                              WHERE     (%d OR id IN (%S)) \
                                    AND deletedFlag!=1 \
                             ",
                             String_isEmpty(storageIdsString) ? 1 : 0,
                             storageIdsString
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
  Array         entryIds;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  // initialize variables
  storageName = String_new();
  Array_init(&entryIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  total       = 0;

  printInfo("Clean-up orphaned:\n");

  // clean fragments/directory entries/link entries/special entries without or an empty storage name
  printInfo("  entries without storage name...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM entryFragments \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                            WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM directoryEntries \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                            WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM linkEntries \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                            WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM specialEntries \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                            WHERE storages.id IS NULL OR storages.name IS NULL OR storages.name=''; \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean entries without fragments
  printInfo("  file entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM fileEntries \
                            WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 0,1) \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;
  printInfo("  image entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM imageEntries \
                            WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 0,1) \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;
  printInfo("  hardlink entries without fragments...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM hardlinkEntries \
                            WHERE NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 0,1) \
                           "
                          );

  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM fileEntries WHERE fileEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_FILE
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without image entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM imageEntries WHERE imageEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_IMAGE
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
      }
      clearPercentage();
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without directory entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM directoryEntries WHERE directoryEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_DIRECTORY
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,Array_length(&entryIds));
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without link entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM linkEntries WHERE linkEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_LINK
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without hardlink entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM hardlinkEntries WHERE hardlinkEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_HARDLINK
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
      }
    }
    (void)Database_flush(databaseHandle);
  }
  clearPercentage();
  printInfo("%lu\n",n);
  total += n;

  printInfo("  entries without special entry...");
  Array_clear(&entryIds);
  error = Database_getIds(databaseHandle,
                          &entryIds,
                          "entries",
                          "id",
                          "WHERE entries.type=%u AND NOT EXISTS(SELECT id FROM specialEntries WHERE specialEntries.entryId=entries.id LIMIT 0,1) \
                          ",
                          INDEX_CONST_TYPE_SPECIAL
                         );
  if (error == ERROR_NONE)
  {
    n = 0L;
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      printPercentage(0,Array_length(&entryIds));
      ARRAY_ITERATE(&entryIds,arrayIterator,databaseId)
      {
        (void)Database_execute(databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               &n,
                               DATABASE_COLUMN_TYPES(),
                               "DELETE FROM entries \
                                WHERE id=%lld \
                               ",
                               databaseId
                              );
        printPercentage(n,Array_length(&entryIds));
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
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM storages \
                            WHERE name IS NULL OR name='' \
                           "
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
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM storages \
                            WHERE (state<%u) OR (state>%u) \
                           ",
                           INDEX_CONST_STATE_OK,
                           INDEX_CONST_STATE_ERROR
                          );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

  // clean FTS entries without entry
  printInfo("  FTS entries without entry...");
  n = 0L;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM FTS_entries \
                            WHERE NOT EXISTS(SELECT id FROM entries WHERE entries.id=FTS_entries.entryId LIMIT 0,1) \
                           "
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
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM FTS_storages \
                            WHERE NOT EXISTS(SELECT id FROM storages WHERE storages.id=FTS_storages.storageId LIMIT 0,1) \
                           "
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
    (void)Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           &n,
                           DATABASE_COLUMN_TYPES(),
                           "DELETE FROM entriesNewest \
                            WHERE NOT EXISTS(SELECT id FROM entries WHERE entries.id=entriesNewest.entryId LIMIT 0,1) \
                           "
                          );
  }
  (void)Database_flush(databaseHandle);
  printInfo("%lu\n",n);
  total += n;

//TODO: obsolete, remove
#if 0
  // clean *Entries without entry
  printInfo("  orphaned entries...");
  n = 0L;
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

                           error = Database_execute(databaseHandle,
                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM fileEntries WHERE id=%lld",
                                                    NULL,
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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT fileEntries.id \
                          FROM fileEntries \
                            LEFT JOIN entries ON entries.id=fileEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT imageEntries.id \
                          FROM imageEntries \
                            LEFT JOIN entries ON entries.id=imageEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT directoryEntries.id \
                          FROM directoryEntries \
                            LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT linkEntries.id \
                          FROM linkEntries \
                            LEFT JOIN entries ON entries.id=linkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT hardlinkEntries.id \
                          FROM hardlinkEntries \
                            LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           int64  databaseId;
                           Errors error;

                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(valueCount);
                           UNUSED_VARIABLE(userData);

                           databaseId = values[0].key;

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
                         DATABASE_COLUMN_TYPES(KEY),
                         "SELECT specialEntries.id \
                          FROM specialEntries \
                            LEFT JOIN entries ON entries.id=specialEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  printInfo("%lu\n",n);
  total += n;
#endif

  fprintf(stdout,"Total %lu orphaned entries removed\n",total);

  // free resources
  Array_done(&entryIds);
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
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DatabaseId storageId;

                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             storageId = values[0].id;

                             if (String_equalsCString(name,values[1].text.data))
                             {
                               error = Database_execute(databaseHandle,
                                                        CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                        NULL,  // changedRowCount
                                                        DATABASE_COLUMN_TYPES(),
                                                        "UPDATE storages \
                                                         SET deletedFlag=1 \
                                                         WHERE id=%lld \
                                                        ",
                                                        storageId
                                                       );
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }
                               n++;
                               printInfo("    %s\n",values[1]);
                             }
                             String_setCString(name,values[1].text.data);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(KEY,
                                                 TEXT
                                                ),
                           "SELECT id,name FROM storages \
                            WHERE deletedFlag!=1 \
                            ORDER BY name \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError("clean duplicates fail (error: %s)!\n",Error_getText(error));
  }
  totalCount += n;

  fprintf(stdout,"Total %lu duplicate entries removed\n",n);

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
                             "WHERE deletedFlag=1"
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
                                   "WHERE storageId=%lld \
                                   ",
                                   storageId
                                  );
        }

        // collect directory/link/special entries to purge
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "directoryEntries",
                                   "entryId",
                                   "WHERE storageId=%lld \
                                   ",
                                   storageId
                                  );
        }
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "linkEntries",
                                   "entryId",
                                   "WHERE storageId=%lld \
                                   ",
                                   storageId
                                  );
        }
        if (error == ERROR_NONE)
        {
           error = Database_getIds(databaseHandle,
                                   &entryIds,
                                   "specialEntries",
                                   "entryId",
                                   "WHERE storageId=%lld \
                                   ",
                                   storageId
                                  );
        }

        printPercentage(0,2*Array_length(&entryIds));

        // purge fragments
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount,
                                   "DELETE FROM entryFragments \
                                    WHERE storageId=%lld \
                                   ",
                                   NULL,
                                   storageId
                                  );
        }

        // purge FTS entries
        if (error == ERROR_NONE)
        {
          printPercentage(0*Array_length(&entryIds),2*Array_length(&entryIds));
          ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
          {
            if (!Database_exists(databaseHandle,"entryFragments","id","entryId=%lld",entryId))
            {
              error = Database_execute(databaseHandle,
                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                       &n,
                                       "DELETE FROM FTS_entries \
                                        WHERE entryId MATCH %lld \
                                       ",
                                       NULL,
                                       entryId
                                      );
            }
            printPercentage(0*Array_length(&entryIds)+n,2*Array_length(&entryIds));
          }
          printPercentage(1*Array_length(&entryIds),2*Array_length(&entryIds));
        }

        // purge directory/link/special entries
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount,
                                   "DELETE FROM directoryEntries \
                                    WHERE storageId=%lld \
                                   ",
                                   NULL,
                                   storageId
                                  );
        }
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount,
                                   DATABASE_COLUMN_TYPES(),
                                   "DELETE FROM linkEntries \
                                    WHERE storageId=%lld \
                                   ",
                                   storageId
                                  );
        }
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount,
                                   DATABASE_COLUMN_TYPES(),
                                   "DELETE FROM specialEntries \
                                    WHERE storageId=%lld \
                                   ",
                                   NULL,
                                   storageId
                                  );
        }

        // purge FTS storages
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   &n,
                                   "DELETE FROM FTS_storages \
                                    WHERE storageId MATCH %lld \
                                   ",
                                   NULL,
                                   storageId
                                  );
        }

        // purge storage
        if (error == ERROR_NONE)
        {
          error = Database_execute(databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount,
                                   "DELETE FROM storages \
                                    WHERE id=%lld \
                                   ",
                                   NULL,
                                   storageId
                                  );
        }

        // purge entries
        if (error == ERROR_NONE)
        {
          printPercentage(1*Array_length(&entryIds),2*Array_length(&entryIds));
          ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
          {
            if (!Database_exists(databaseHandle,"entryFragments","id","entryId=%lld",entryId))
            {
              error = Database_execute(databaseHandle,
                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                       &n,
                                       "DELETE FROM entries \
                                        WHERE id=%lld \
                                       ",
                                       NULL,
                                       entryId
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
          printInfo("FAIL\n");
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
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "VACUUM INTO '%s'",
                             toFileName
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
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "VACUUM"
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

/***********************************************************************\
* Name   : getColumnsWidth
* Purpose: get columns width
* Input  : columns - database columns
* Output : -
* Return : widths
* Notes  : -
\***********************************************************************/

LOCAL size_t* getColumnsWidth(const DatabaseColumns *columns)
{
  size_t *widths;
  uint   i;
  char   buffer[1024];

  assert(columns != NULL);

  widths = (size_t*)malloc(columns->count*sizeof(size_t));
  assert(widths != NULL);

  for (i = 0; i < columns->count; i++)
  {
    widths[i] = 0;
    Database_valueToCString(buffer,sizeof(buffer),&columns->values[i]);
    if (stringLength(buffer) > widths[i])
    {
      widths[i] = stringLength(buffer);
    }
  }

  return widths;
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
* Name   : calculateColumnWidths
* Purpose: calculate column width call back
* Input  : columns  - column names
*          values   - values
*          count    - number of values
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors calculateColumnWidths(const DatabaseColumns *columns, void *userData)
{
  PrintTableData *printTableData = (PrintTableData*)userData;
  uint           i;
  char           buffer[1024];

  assert(columns != NULL);
  assert(printTableData != NULL);

  UNUSED_VARIABLE(userData);

  if (printTableData->widths == NULL) printTableData->widths = getColumnsWidth(columns);
  assert(printTableData->widths != NULL);

  for (i = 0; i < columns->count; i++)
  {
    Database_valueToCString(buffer,sizeof(buffer),&columns->values[i]);
    printTableData->widths[i] = MAX(stringLength(buffer),printTableData->widths[i]);
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

LOCAL Errors printRow(const DatabaseColumns *columns, void *userData)
{
  PrintTableData *printTableData = (PrintTableData*)userData;
  uint           i;
  char           buffer[1024];

  assert(columns != NULL);
  assert(printTableData != NULL);
  assert(printTableData->widths != NULL);

  UNUSED_VARIABLE(userData);

  if (printTableData->showHeaderFlag && !printTableData->headerPrintedFlag)
  {
    for (i = 0; i < columns->count; i++)
    {
      printf("%s ",columns->names[i]); printSpaces(printTableData->widths[i]-stringLength(columns->names[i]));
    }
    printf("\n");

    printTableData->headerPrintedFlag = TRUE;
  }
  for (i = 0; i < columns->count; i++)
  {
    Database_valueToCString(buffer,sizeof(buffer),&columns->values[i]);
    printf("%s ",buffer);
    if (printTableData->showHeaderFlag)
    {
      printSpaces(printTableData->widths[i]-stringLength(buffer));
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
  int64  n;

  // show meta data
  printf("Meta:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  %-16s: %s\n",
                                    values[0].text.data,
                                    values[1].text.data
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(TEXT,TEXT),
                           "SELECT name,value \
                            FROM meta \
                           "
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
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "entities",
                                  "COUNT(id)",
                                  "WHERE id!=0 AND deletedFlag!=1"
                                 );
    if (error != ERROR_NONE)
    {
      printError("get entities data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Normal          : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_NORMAL
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Full            : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_FULL
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Differential    : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_DIFFERENTIAL
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Incremental     : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_INCREMENTAL
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Continuous      : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM entities \
                            WHERE type=%u \
                           ",
                           CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS
                          );
  if (error != ERROR_NONE)
  {
    printError("get entities data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
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
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  OK              : %lld\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
                           INDEX_CONST_STATE_OK
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Update requested: %lld\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
                           INDEX_CONST_STATE_UPDATE_REQUESTED
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Error           : %lld\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE state=%d AND deletedFlag!=1 \
                           ",
                           INDEX_CONST_STATE_ERROR
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Deleted         : %lld\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(id) \
                            FROM storages \
                            WHERE deletedFlag=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
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
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Total           : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalEntryCount),SUM(totalEntrySize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Files           : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalFileCount),SUM(totalFileSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Images          : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalImageCount),SUM(totalImageSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Directories     : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalDirectoryCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Links           : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalLinkCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Hardlinks       : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalHardlinkCount),SUM(totalHardlinkSize) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Special         : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalSpecialCount) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
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
    error = Database_getInteger64(databaseHandle,
                                  &n,
                                  "entriesNewest",
                                  "COUNT(id)",
                                  ""
                                 );
    if (error != ERROR_NONE)
    {
      printError("get storage data fail (error: %s)!",Error_getText(error));
      exit(EXITCODE_FAIL);
    }
    printf(" %"PRIi64,n);
  }
  printf("\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Total           : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalEntryCountNewest),SUM(totalEntrySizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Files           : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalFileCountNewest),SUM(totalFileSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Images          : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalImageCountNewest),SUM(totalImageSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Directories     : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalDirectoryCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Links           : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalLinkCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Hardlinks       : %lu, %.1lf %s (%"PRIu64" bytes)\n",
                                    values[0].i,
                                    getByteSize(values[1].i),
                                    getByteUnitShort(values[1].i),
                                    values[1].i
                                   );

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT,
                                                 INT64
                                                ),
                           "SELECT SUM(totalHardlinkCountNewest),SUM(totalHardlinkSizeNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
                          );
  if (error != ERROR_NONE)
  {
    printError("get entries data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             printf("  Special         : %lu\n",values[0].i);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT SUM(totalSpecialCountNewest) \
                            FROM storages \
                            WHERE deletedFlag!=1 \
                           "
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

LOCAL void printUUIDsInfo(DatabaseHandle *databaseHandle, const Array uuidIds, const Array uuIds)
{
  String       uuidIdsString,uuidsString;
  char         s[MISC_UUID_STRING_LENGTH];
  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  ulong        i;
  DatabaseId   uuidId;
  Errors       error;

  uuidIdsString = String_new();
  uuidsString = String_new();
  ARRAY_ITERATE(&uuidIds,i,uuidId)
  {
    if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
    String_formatAppend(uuidIdsString,"%lld",uuidId);
  }
  ARRAY_ITERATE(&uuIds,i,s)
  {
    String_setBuffer(uuid,s,MISC_UUID_STRING_LENGTH);
    if (!String_isEmpty(uuidsString)) String_appendChar(uuidsString,',');
    String_formatAppend(uuidsString,"'%S'",uuid);
  }

  printf("UUIDs:\n");
  error = Database_execute(databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
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

                             assert(values != NULL);
                             assert(valueCount == 14);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             uuidId              = values[0].id;
                             totalEntryCount     = values[2].i;
                             totalEntrySize      = values[3].i;
                             totalFileCount      = values[4].i;
                             totalFileSize       = values[5].i;
                             totalImageCount     = values[6].i;
                             totalImageSize      = values[7].i;
                             totalDirectoryCount = values[8].i;
                             totalLinkCount      = values[9].i;
                             totalHardlinkCount  = values[10].i;
                             totalHardlinkSize   = values[11].i;
                             totalSpecialCount   = values[12].i;

                             printf("  Id              : %"PRIi64"\n",uuidId);
                             printf("    UUID          : %s\n",values[ 1].text.data);
                             printf("\n");
                             printf("    Total entries : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                             printf("\n");
                             printf("    Files         : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalFileSize);
                             printf("    Images        : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalImageSize);
                             printf("    Directories   : %lu\n",totalDirectoryCount);
                             printf("    Links         : %lu\n",totalLinkCount);
                             printf("    Hardlinks     : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalHardlinkCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalHardlinkSize);
                             printf("    Special       : %lu\n",totalSpecialCount);
                             printf("\n");

                             idsString = String_new();
                             String_clear(idsString);
                             Database_execute(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                              {
                                                assert(values != NULL);
                                                assert(valueCount == 1);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_formatAppend(idsString,"%"PRIi64,values[0].id);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              DATABASE_COLUMN_TYPES(KEY),
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
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                              {
                                                assert(values != NULL);
                                                assert(valueCount == 1);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_formatAppend(idsString,"%"PRIi64,values[0].id);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              DATABASE_COLUMN_TYPES(KEY),
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
                           DATABASE_COLUMN_TYPES(KEY,TEXT,
                                                 INT,INT64,
                                                 INT,INT64,INT,INT64,INT,INT,INT,INT64,INT,
                                                 INT,INT64,
                                                 INT,INT64,INT,INT64,INT,INT,INT,INT64,INT
                                                ),
                           "SELECT id,\
                                   jobUUID, \
                                   \
                                   (SELECT SUM(totalEntryCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalEntrySize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT SUM(totalFileCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalFileSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalImageCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalImageSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalDirectoryCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalLinkCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalHardlinkCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalHardlinkSize) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalSpecialCount) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT SUM(totalEntryCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalEntrySizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   \
                                   (SELECT SUM(totalFileCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalFileSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalImageCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalImageSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalDirectoryCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalLinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalHardlinkCountNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalHardlinkSizeNewest) FROM entities WHERE entities.uuidId=uuids.id), \
                                   (SELECT SUM(totalSpecialCountNewest) FROM entities WHERE entities.uuidId=uuids.id) \
                            FROM uuids \
                            WHERE     (%d OR id IN (%S)) \
                                  AND (%d OR jobUUID IN (%S)) \
                           ",
                           String_isEmpty(uuidIdsString) ? 1 : 0,
                           uuidIdsString,
                           String_isEmpty(uuidsString) ? 1 : 0,
                           uuidsString
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

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

  printf("Entities:\n");
  error = Database_execute(databaseHandle,
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
                             String     idsString;

                             assert(values != NULL);
                             assert(valueCount == 14);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             entityId            = values[0].id;
                             type                = values[1].i;
                             totalEntryCount     = values[4].i;
                             totalEntrySize      = values[5].i;
                             totalFileCount      = values[6].i;
                             totalFileSize       = values[7].i;
                             totalImageCount     = values[8].i;
                             totalImageSize      = values[9].i;
                             totalDirectoryCount = values[10].i;
                             totalLinkCount      = values[11].i;
                             totalhardlinkCount  = values[12].i;
                             totalHardlinkSize   = values[13].i;
                             totalSpecialCount   = values[14].i;

                             uuidId              = values[26].id;

                             printf("  Id              : %"PRIi64"\n",entityId);
                             printf("    Type          : %s\n",(type <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS) ? TYPE_NAMES[type] : "xxx");//TODO values[ 1]);
                             printf("    Job UUID      : %s\n",values[ 2]);
                             printf("    Schedule UUID : %s\n",values[ 3]);
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

                             idsString = String_new();
                             String_clear(idsString);
                             Database_execute(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                              {
                                                assert(values != NULL);
                                                assert(valueCount == 1);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                if (!String_isEmpty(idsString)) String_appendChar(idsString,',');
                                                String_formatAppend(idsString,"%"PRIi64,values[0].id);

                                                return ERROR_NONE;
                                              },NULL),
                                              NULL,  // changedRowCount
                                              DATABASE_COLUMN_TYPES(KEY),
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
                           DATABASE_COLUMN_TYPES(KEY,
                                                 INT,
                                                 TEXT,
                                                 TEXT,

                                                 INT,
                                                 INT64,

                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT,
                                                 INT,
                                                 INT64,
                                                 INT,

                                                 INT,
                                                 INT64,

                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT,
                                                 INT,
                                                 INT64,
                                                 INT
                                                ),
                           "SELECT id,\
                                   type, \
                                   jobUUID, \
                                   scheduleUUID, \
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

LOCAL void printStoragesInfo(DatabaseHandle *databaseHandle, const Array storageIds, bool lostFlag)
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
  ARRAY_ITERATE(&storageIds,i,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_formatAppend(storageIdsString,"%lld",storageId);
  }

  printf("%s:\n",lostFlag ? "Lost storages" : "Storages");
  error = Database_execute(databaseHandle,
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
//                             assert(valueCount == 14);

                             UNUSED_VARIABLE(valueCount);
                             UNUSED_VARIABLE(userData);

                             state               = values[10].i;
                             mode                = values[11].i;
                             totalEntryCount     = values[14].i;
                             totalEntrySize      = values[15].i;
                             totalFileCount      = values[16].i;
                             totalFileSize       = values[17].i;
                             totalImageCount     = values[18].i;
                             totalImageSize      = values[19].i;
                             totalDirectoryCount = values[20].i;
                             totalLinkCount      = values[21].i;
                             totalHardlinkCount  = values[22].i;
                             totalHardlinkSize   = values[23].i;
                             totalSpecialCount   = values[24].i;

                             printf("  Id              : %"PRIi64"\n",values[ 0].id);
                             printf("    Name          : %s\n",values[ 5].text.data);
                             printf("    Created       : %d\n",values[ 6].dateTime);
                             printf("    Host name     : %s\n",(values[ 7].text.data != NULL) ? values[ 7].text.data : "");
                             printf("    User name     : %s\n",(values[ 8].text.data != NULL) ? values[ 8].text.data : "");
                             printf("    Comment       : %s\n",(values[ 9].text.data != NULL) ? values[ 9].text.data : "");
                             printf("    State         : %s\n",(state <= INDEX_CONST_STATE_ERROR) ? STATE_TEXT[state] : "xxx");//TODO values[10].i);
                             printf("    Mode          : %s\n",(mode <= INDEX_CONST_MODE_AUTO) ? MODE_TEXT[mode] : "xxx");//TODO values[11].i);
                             printf("    Last checked  : %d\n",values[12].dateTime);
                             printf("    Error message : %s\n",(values[13].text.data != NULL) ? values[13].text.data : "");
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
                             printf("    Job UUID      : %s\n",values[3].text.data);
                             printf("    Schedule UUID : %s\n",values[4].text.data);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(KEY,
                                                 KEY,
                                                 KEY,
                                                 TEXT,
                                                 TEXT,
                                                 TEXT,
                                                 DATETIME,
                                                 TEXT,
                                                 TEXT,
                                                 TEXT,
                                                 INT,
                                                 INT,
                                                 DATETIME,
                                                 TEXT,

                                                 INT,
                                                 INT64,

                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT,
                                                 INT,
                                                 INT64,
                                                 INT,

                                                 INT,
                                                 INT64,

                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT64,
                                                 INT,
                                                 INT,
                                                 INT,
                                                 INT64,
                                                 INT
                                                ),
                           "SELECT storages.id,\
                                   storages.uuidId, \
                                   storages.entityId, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storages.name, \
                                   storages.created, \
                                   storages.hostName, \
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
                            LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
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
    printError("get storage data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
  }

  // free resources
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
  String     ftsName;
  ulong      i;
  DatabaseId entityId;
  Errors     error;

//TODO: lostFlag
UNUSED_VARIABLE(lostFlag);

  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,i,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_formatAppend(entityIdsString,"%lld",entityId);
  }

  ftsName = String_new();
  getFTSString(ftsName,name);

  printf("Entries:\n");
  error = Database_execute(databaseHandle,
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
                             error = Database_execute(databaseHandle,
                                                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                                      {
                                                        uint type;

                                                        assert(values != NULL);
                                                        assert(valueCount == 8);

                                                        UNUSED_VARIABLE(valueCount);
                                                        UNUSED_VARIABLE(userData);

                                                        type = (uint)values[2].i;
                                                        if (!entityOutputFlag)
                                                        {
                                                          printf("  Entity id: %"PRIi64"\n",entityId);
                                                          printf("  UUID id  : %"PRIi64"\n",uuidId);
                                                          entityOutputFlag = TRUE;
                                                        }
                                                        printf("    Id               : %"PRIi64"\n",values[0].id);
                                                        printf("      Name           : %s\n",values[1].text.data);
                                                        printf("      Type           : %s\n",(type <= INDEX_CONST_TYPE_HISTORY) ? TYPE_TEXT[type] : "xxx");//TODO values[2].i);
                                                        switch (type)
                                                        {
                                                          case INDEX_CONST_TYPE_FILE:
                                                            printf("      Size           : %"PRIi64"\n",values[ 3].i);
                                                            printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                            printf("      Fragment offset: %"PRIi64"\n",values[11].i);
                                                            printf("      Fragment size  : %"PRIi64"\n",values[12].i);
                                                            printf("      Storage id:    : %"PRIi64"\n",values[10].id);
                                                            break;
                                                          case INDEX_CONST_TYPE_IMAGE:
                                                            printf("      Size           : %"PRIi64"\n",values[ 4].i);
                                                            printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                            printf("      Fragment offset: %"PRIi64"\n",values[11].i);
                                                            printf("      Fragment size  : %"PRIi64"\n",values[12].i);
                                                            printf("      Storage id:    : %"PRIi64"\n",values[10].i);
                                                            break;
                                                          case INDEX_CONST_TYPE_DIRECTORY:
                                                            printf("      Storage id:    : %"PRIi64"\n",values[ 5].id);
                                                            break;
                                                          case INDEX_CONST_TYPE_LINK:
                                                            printf("      Storage id:    : %"PRIi64"\n",values[ 6].id);
                                                            break;
                                                          case INDEX_CONST_TYPE_HARDLINK:
                                                            printf("      Size           : %"PRIi64"\n",values[ 7].i);
                                                            printf("      Fragment id    : %"PRIi64"\n",values[ 9].id);
                                                            printf("      Fragment offset: %"PRIi64"\n",values[11].i);
                                                            printf("      Fragment size  : %"PRIi64"\n",values[12].i);
                                                            printf("      Storage id:    : %"PRIi64"\n",values[10].i);
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
                                                      DATABASE_COLUMN_TYPES(KEY,
                                                                            TEXT,
                                                                            INT,

                                                                            INT64,
                                                                            INT64,
                                                                            KEY,
                                                                            KEY,
                                                                            INT64,
                                                                            KEY,
                                                                            KEY,
                                                                            KEY,
                                                                            INT64,
                                                                            INT64
                                                                           ),
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
                                                             AND ((%u=0) OR (type=%u)) \
                                                             AND (%d OR entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')) \
                                                      ",
                                                      entityId,
                                                      entryType,entryType,
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
                           DATABASE_COLUMN_TYPES(KEY,
                                                 KEY
                                                ),
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
    printError("get entity data fail (error: %s)!",Error_getText(error));
    exit(EXITCODE_FAIL);
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
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    printInfo("FAIL!\n");
    return;
  }

  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(),
                           "INSERT INTO %1 \
                              ( \
                                storageId, \
                                entryId, \
                                name, \
                                timeLastChanged \
                              ) \
                                  SELECT entryFragments.storageId,entries.id AS entryId,entries.name,entries.timeLastChanged \
                                  FROM entryFragments \
                                    LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                  WHERE storageId=%lld \
                            UNION SELECT directoryEntries.storageId,entries.id AS entryId,entries.name,entries.timeLastChanged \
                                  FROM directoryEntries \
                                    LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                  WHERE storageId=%lld \
                            UNION SELECT linkEntries.storageId,entries.id AS entryId,entries.name,entries.timeLastChanged \
                                  FROM linkEntries \
                                    LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                  WHERE storageId=%lld \
                            UNION SELECT specialEntries.storageId,entries.id AS entryId,entries.name,entries.timeLastChanged \
                                  FROM specialEntries \
                                    LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                  WHERE storageId=%lld \
                           ",
                           storageId,
                           storageId,
                           storageId,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    printInfo("FAIL!\n");
    return;
  }
  error = Database_execute(databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(),
                           "UPDATE %1 \
                            SET entriesNewestId=IFNULL((SELECT entriesNewest.id \
                                                        FROM entriesNewest \
                                                        WHERE entriesNewest.entryId=%1.entryId \
                                                        LIMIT 0,1 \
                                                       ), \
                                                       0 \
                                                      ) \
                           "
                          );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    printInfo("FAIL!\n");
    return;
  }

  if (show == 1)
  {
    printInfo("storages:\n");
    n = 0;
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%s: %s\n",values[0],values[1]);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   TEXT
                                                  ),
                             "SELECT entryFragments.storageId,storages.name FROM entryFragments \
                                LEFT JOIN storages ON storages.id=entryFragments.storageId \
                              WHERE storages.deletedFlag!=1 \
                              GROUP BY storageId \
                             "
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%s entryId=%s name=%s timeLastChanged=%s\n",values[0],values[1],values[2],values[3]);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   KEY,
                                                   TEXT,
                                                   DATETIME
                                                  ),
                             "      SELECT entryFragments.storageId,entriesNewest.entryId,entriesNewest.name,entriesNewest.timeLastChanged \
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%s entryId=%s name=%s timeLastChanged=%s entriesNewestId=%s\n",values[0],values[1],values[2],values[3],values[4]);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   KEY,
                                                   TEXT,
                                                   DATETIME,
                                                   KEY
                                                  ),
                             "SELECT storageId,entryId,name,timeLastChanged,entriesNewestId FROM %1 \
                             "
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"storageId=%d entryId=%d name=%s timeLastChanged=%d\n",values[0].i,values[1].i,values[2].i,values[3].dateTime);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   KEY,
                                                   TEXT,
                                                   DATETIME
                                                  ),
                             "SELECT storageId,entryId,name,timeLastChanged FROM %1 \
                              WHERE %1.entriesNewestId!=0 \
                             "
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"entryId=%s name=%s timeLastChanged=%s\n",values[0],values[1],values[2]);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   TEXT,
                                                   DATETIME
                                                  ),
                             "SELECT entryId,name,timeLastChanged FROM %1 \
                              WHERE %1.entriesNewestId!=0 \
                             "
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
    error = Database_execute(databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               UNUSED_VARIABLE(valueCount);
                               UNUSED_VARIABLE(userData);

fprintf(stdout,"new entryId=%s name=%s timeLastChanged=%s\n",values[0],values[1],values[2]);
n++;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(KEY,
                                                   TEXT,
                                                   DATETIME
                                                  ),
                             "SELECT entries.id AS entryId,%1.name,entries.timeLastChanged FROM %1 \
                                LEFT JOIN entries ON entries.id=(SELECT id \
                                                                 FROM entries \
                                                                 WHERE name=%1.name \
                                                                 ORDER BY timeLastChanged DESC \
                                                                 LIMIT 0,1 \
                                                                ) \
                              WHERE entries.id=%1.entryId \
                             "
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
    error = addStorageToNewest(databaseHandle,storageId);
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
    error = removeStorageFromNewest(databaseHandle,storageId);
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
  String           entryName;
  uint             i,n;
  CStringTokenizer stringTokenizer;
  const char       *token;
  DatabaseId       databaseId;
  const char       *databaseFileName;
  String           path;
  String           command;
  char             line[MAX_LINE_LENGTH];
  Errors           error;
  DatabaseHandle   databaseHandle;
  uint64           t0,t1,dt;
  PrintTableData   printTableData;
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
  entryName        = String_new();
  databaseFileName = NULL;
  command          = String_new();

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
      }
      stringTokenizerDone(&stringTokenizer);
      i++;
    }
    else if (stringEquals(argv[i],"--info-lost-storages"))
    {
      infoLostStoragesFlag = TRUE;
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
    else if (stringEquals(argv[i],"-h") || stringEquals(argv[i],"--help"))
    {
      printUsage(argv[0],FALSE);
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuIds);
      Array_done(&uuidIds);
      String_delete(command);
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
      String_delete(entryName);
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
        databaseFileName = argv[i];
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
  if (databaseFileName == NULL)
  {
    printError("no database file name given!");
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
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
  error = openDatabase(&databaseHandle,databaseFileName,!String_isEmpty(command) || pipeFlag);
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(entryName);
    exit(EXITCODE_FAIL);
  }
  error = Database_setEnabledForeignKeys(&databaseHandle,foreignKeysFlag);
  if (error != ERROR_NONE)
  {
    closeDatabase(&databaseHandle);
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
    String_delete(entryName);
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
    printError("%s",Error_getText(error));
    closeDatabase(&databaseHandle);
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuIds);
    Array_done(&uuidIds);
    String_delete(command);
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

  if      (infoLostStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIds,TRUE);
  }
  else if (infoStoragesFlag)
  {
    printStoragesInfo(&databaseHandle,storageIds,FALSE);
  }

  if      (infoLostEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entityIds,entryType,entryName,TRUE);
  }
  else if (infoEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entityIds,entryType,entryName,FALSE);
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

  // drop tables
  if (dropTablesFlag)
  {
    dropTables(&databaseHandle);
  }

  // drop triggeres
  if (dropTriggersFlag)
  {
    dropTriggers(&databaseHandle);
  }

  // drop indizes
  if (dropIndizesFlag)
  {
    dropIndizes(&databaseHandle);
  }

  // create tables/indices/triggers
  if (createFlag)
  {
    error = createTablesIndicesTriggers(&databaseHandle);
  }

  // import
  if (importFileName != NULL)
  {
    importIntoDatabase(&databaseHandle,importFileName);
  }

  // check
  if (checkIntegrityFlag)
  {
    checkDatabaseIntegrity(&databaseHandle);
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

    Database_execute(&databaseHandle,
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 2);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = 1+(uint)log10(values[0].i);
                       maxStorageNameLength = (uint)values[1].i;

                       return ERROR_NONE;
                     },NULL),
                     NULL,  // changedRowCount
                     DATABASE_COLUMN_TYPES(INT,
                                           INT
                                          ),
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
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       uint       archiveType;
                       const char *s;

                       assert(values != NULL);
                       assert(valueCount == 4);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       archiveType = values[3].i;
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
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 3);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = 1+(uint)log10(values[0].i);
                       maxEntryNameLength   = values[1].i;
                       maxStorageNameLength = values[2].i;

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
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 3);

                       UNUSED_VARIABLE(valueCount);
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
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 3);

                       UNUSED_VARIABLE(valueCount);
                       UNUSED_VARIABLE(userData);

                       maxIdLength          = 1+(uint)log10(values[0].i);
                       maxEntryNameLength   = values[1].i;
                       maxStorageNameLength = values[2].i;

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
                     CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                     {
                       assert(values != NULL);
                       assert(valueCount == 3);

                       UNUSED_VARIABLE(valueCount);
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
    if (!String_isEmpty(command))
    {
      error = ERROR_NONE;

      s = String_duplicate(command);
      String_replaceAllCString(s,STRING_BEGIN,"%","%%");
      if (explainQueryPlanFlag) String_insertCString(s,STRING_BEGIN,"EXPLAIN QUERY PLAN ");

      printTableData.showHeaderFlag    = showHeaderFlag;
      printTableData.headerPrintedFlag = FALSE;
      printTableData.widths            = NULL;
      if (showHeaderFlag)
      {
        if (error == ERROR_NONE)
        {
          error = Database_execute(&databaseHandle,
                                   CALLBACK_(calculateColumnWidths,&printTableData),
                                   NULL,  // changedRowCount
                                   DATABASE_COLUMN_TYPES(),
                                   String_cString(s)
                                  );
        }
      }
      t0 = 0L;
      t1 = 0L;
      if (error == ERROR_NONE)
      {
        t0 = Misc_getTimestamp();
        error = Database_execute(&databaseHandle,
                                 CALLBACK_(printRow,&printTableData),
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 String_cString(s)
                                );
        t1 = Misc_getTimestamp();
      }
      freeColumnsWidth(printTableData.widths);

      String_delete(s);
      if (error != ERROR_NONE)
      {
        printError("SQL command '%s' fail: %s!",String_cString(command),Error_getText(error));
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuIds);
        Array_done(&uuidIds);
        String_delete(command);
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
        String_delete(entryName);
        exit(EXITCODE_FAIL);
      }
    }
    while (inputAvailable() && (fgets(line,sizeof(line),stdin) != NULL))
    {
      l = stringTrim(line);
      t0 = Misc_getTimestamp();
      error = Database_execute(&databaseHandle,
                               CALLBACK_(printRow,NULL),
                               NULL,  // changedRowCount,
                               DATABASE_COLUMN_TYPES(),
                               "%s",
                               l
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
  String_delete(entryName);
  String_delete(command);

  doneAll();

  return 0;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
