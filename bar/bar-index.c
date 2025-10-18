/***********************************************************************\
*
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

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/cmdoptions.h"
#include "common/database.h"
#include "common/misc.h"
#include "common/progressinfo.h"

#include "sqlite3.h"

#include "bar_common.h"
#include "index/index.h"
#include "index/index_common.h"
#include "index_definition.h"
#include "archive_format_const.h"

/****************** Conditional compilation switches *******************/
#define _INDEX_DEBUG_IMPORT_DATABASE

//#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_RESTART
#define CHECKPOINT_MODE           SQLITE_CHECKPOINT_TRUNCATE

/***************************** Constants *******************************/

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

/***************************** Variables *******************************/
LOCAL const char *changeToDirectory                   = NULL;
LOCAL const char *importFileName                      = NULL;
LOCAL Array      uuidIds;
LOCAL Array      uuids;
LOCAL Array      entityIds;
LOCAL Array      entryIds;
LOCAL Array      storageIds;
LOCAL String     entryName,storageName;
LOCAL String     command;
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
LOCAL bool       checkFlag                            = FALSE;  // check database integrity+orphaned+duplicate entries
LOCAL bool       optimizeFlag                         = FALSE;  // optimize database
LOCAL bool       reindexFlag                          = FALSE;  // re-create existing indices
LOCAL bool       createFlag                           = FALSE;  // create new index database
LOCAL bool       createTriggersFlag                   = FALSE;  // re-create triggers
LOCAL bool       dropTablesFlag                       = FALSE;  // drop tables
LOCAL bool       dropTriggersFlag                     = FALSE;  // drop triggers
LOCAL bool       createIndicesFlag                    = FALSE;  // re-create indices
LOCAL bool       createFTSIndicesFlag                 = FALSE;  // re-create FTS indices
LOCAL bool       showTableNames                       = FALSE;  // output index database table names
LOCAL bool       showIndexNames                       = FALSE;  // output index database index names
LOCAL bool       showTriggerNames                     = FALSE;  // output index database trigger names
LOCAL bool       dropIndicesFlag                      = FALSE;  // drop indices
LOCAL bool       createNewestFlag                     = FALSE;  // re-create newest data
LOCAL bool       createAggregatesEntitiesFlag         = FALSE;  // re-create aggregates entities data
LOCAL bool       createAggregatesStoragesFlag         = FALSE;  // re-create aggregates storages data
LOCAL bool       createAggregatesFlag                 = FALSE;  // re-create aggregates entities+storages data
LOCAL bool       cleanOrphanedFlag                    = FALSE;  // execute clean orphaned entries
LOCAL bool       cleanDuplicateEntriesFlag            = FALSE;  // execute clean duplicate entries
LOCAL bool       cleanFlag                            = FALSE;  // execute clean orphaned+duplicate entries
LOCAL bool       purgeFlag                            = FALSE;  // execute purge deleted storages
LOCAL bool       purgeWithErrorFlag                   = FALSE;  // execute purge storages with error
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
LOCAL bool       verboseFlag                          = TRUE;
LOCAL bool       quietFlag                            = FALSE;
LOCAL bool       timeFlag                             = FALSE;
LOCAL bool       explainQueryPlanFlag                 = FALSE;
LOCAL const char *systemEncoding                      = NULL;
LOCAL const char *consoleEncoding                     = NULL;
LOCAL const char *jobUUID                             = NULL;
LOCAL const char *toFileName                          = NULL;

LOCAL bool       versionFlag                          = FALSE;
LOCAL bool       helpFlag                             = FALSE;
LOCAL bool       xhelpFlag                            = FALSE;

/***********************************************************************\
* Name   : cmdOptionParseJobsUUIDIds
* Purpose: command line option call back to parse jobs UUID ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseJobsUUIDIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  infoUUIDsFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&uuidIds,&databaseId);
      }
      else
      {
        Array_append(&uuids,token);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseEntityIds
* Purpose: command line option call back to parse entity ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseEntityIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  infoEntitiesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&entityIds,&databaseId);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseEntryIdsName
* Purpose: command line option call back to parse entry ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseEntryIdsName(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  infoEntriesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&entryIds,&databaseId);
      }
      else
      {
        String_setCString(entryName,token);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseLostEntryIdsName
* Purpose: command line option call back to parse lost entry ids or name
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseLostEntryIdsName(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  infoLostEntriesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&entryIds,&databaseId);
      }
      else
      {
        String_setCString(entryName,token);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseStorageIdsName
* Purpose: command line option call back to parse storage ids or name
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseStorageIdsName(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  infoStoragesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&storageIds,&databaseId);
      }
      else
      {
        String_setCString(storageName,token);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseEntityIds
* Purpose: command line option call back to parse create newest ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCreateNewestIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  createNewestFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&storageIds,&databaseId);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCreateAggregateEntityIds
* Purpose: command line option call back to parse entity ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCreateAggregateEntityIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  createAggregatesEntitiesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&entityIds,&databaseId);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseIds
* Purpose: command line option call back to parse ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCreateAggregateStorageIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  createAggregatesStoragesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&storageIds,&databaseId);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseCreateAggregateIds
* Purpose: command line option call back to parse aggregate ids
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseCreateAggregateIds(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  createAggregatesFlag = TRUE;

  if (value != NULL)
  {
    CStringTokenizer stringTokenizer;
    stringTokenizerInit(&stringTokenizer,value,",");
    const char       *token;
    while (stringGetNextToken(&stringTokenizer,&token))
    {
      DatabaseId databaseId;
      if (stringToInt64(token,&databaseId,NULL))
      {
        Array_append(&uuidIds,&databaseId);
      }
    }
    stringTokenizerDone(&stringTokenizer);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : cmdOptionParseVerbose
* Purpose: command line option call back to parse verbose option
* Input  : -
* Output : -
* Return : TRUE iff parsed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool cmdOptionParseVerbose(void *variable, const char *name, const char *value, const void *defaultValue, char errorMessage[], uint errorMessageSize, void *userData)
{
  UNUSED_VARIABLE(variable);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(defaultValue);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);
  UNUSED_VARIABLE(userData);

  quietFlag = FALSE;

  return TRUE;
}

LOCAL const CommandLineOptionSelect COMMAND_LINE_OPTIONS_ENTRY_TYPE[] = CMD_VALUE_SELECT_ARRAY
(
  {"file",     INDEX_CONST_TYPE_FILE,     "files"},
  {"image",    INDEX_CONST_TYPE_IMAGE,    "images"},
  {"directory",INDEX_CONST_TYPE_DIRECTORY,"directories"},
  {"link",     INDEX_CONST_TYPE_LINK,     "symbolic links"},
  {"harlink",  INDEX_CONST_TYPE_HARDLINK, "hard links"},
  {"special",  INDEX_CONST_TYPE_SPECIAL,  "special files"},
);

LOCAL CommandLineOption COMMAND_LINE_OPTIONS[] = CMD_VALUE_ARRAY
(
  CMD_OPTION_CSTRING      ("directory",                         'C',0,0,changeToDirectory,                                                                       "change to directory","directory"                                 ),
  CMD_OPTION_CSTRING      ("import",                            'I',0,0,importFileName,                                                                          "import database","URI"                                           ),

  CMD_OPTION_BOOLEAN      ("info",                              0,  0,0,infoFlag,                                                                                "output index database infos"                                     ),
  CMD_OPTION_SPECIAL      ("info-jobs",                         0,  0,1,&infoUUIDsFlag,                  cmdOptionParseJobsUUIDIds,NULL,0,                       "output index database job infos","uuid id|UUID"                  ),
  CMD_OPTION_SPECIAL      ("info-entities",                     0,  0,1,&infoEntitiesFlag            ,   cmdOptionParseEntityIds,NULL,0,                         "output index database entities infos","entity id"                ),
  CMD_OPTION_SPECIAL      ("info-entries",                      0,  0,1,&infoEntriesFlag,                cmdOptionParseEntryIdsName,NULL,0,                      "output index database entries infos","entry id|name"             ),
  CMD_OPTION_SELECT       ("entry-type",                        0,  0,1,entryType,                       COMMAND_LINE_OPTIONS_ENTRY_TYPE,                        "entries type","type","(default)"                                 ),
  CMD_OPTION_SPECIAL      ("info-lost-entries",                 0,  0,1,&infoLostEntriesFlag,            cmdOptionParseLostEntryIdsName,NULL,0,                  "output index database entries infos without an entity","entry id"),
  CMD_OPTION_SPECIAL      ("info-storages",                     0,  0,1,&infoStoragesFlag,               cmdOptionParseStorageIdsName,NULL,0,                    "output index database storages infos","storage id"               ),
  CMD_OPTION_BOOLEAN      ("info-lost-storages",                0,  0,0,infoLostStoragesFlag,                                                                    "output index database storages infos without an entity"          ),
  CMD_OPTION_BOOLEAN      ("check-integrity",                   0,  0,0,checkIntegrityFlag,                                                                      "check index database integrity"                                  ),
  CMD_OPTION_BOOLEAN      ("check-orphaned",                    0,  0,0,checkOrphanedFlag,                                                                       "check index database for orphaned entries"                       ),
  CMD_OPTION_BOOLEAN      ("check-duplicates",                  0,  0,0,checkDuplicatesFlag,                                                                     "check index database for duplicate entries"                      ),
  CMD_OPTION_BOOLEAN      ("check",                             0,  0,0,checkFlag,                                                                               "check index database"                                            ),
  CMD_OPTION_BOOLEAN      ("optimize",                          0,  0,0,optimizeFlag,                                                                            "optimize database (analyze and collect statistics data)"         ),
  CMD_OPTION_BOOLEAN      ("reindex",                           0,  0,0,reindexFlag,                                                                             "re-create all existing indices"                                  ),
  CMD_OPTION_BOOLEAN      ("create",                            0,  0,0,createFlag,                                                                              "create new index database"                                       ),
  CMD_OPTION_BOOLEAN      ("create-triggers",                   0,  0,0,createTriggersFlag,                                                                      "re-create triggers"                                              ),
  CMD_OPTION_BOOLEAN      ("create-indices",                    0,  0,0,createIndicesFlag,                                                                       "re-create indices"                                               ),
  CMD_OPTION_BOOLEAN      ("create-fts-indices",                0,  0,0,createFTSIndicesFlag,                                                                    "re-create FTS indices (full text search)"                        ),
  CMD_OPTION_SPECIAL      ("create-newest",                     0,  0,1,&uuidIds,                        cmdOptionParseCreateNewestIds,NULL,0,                   "re-create newest data","id"                                      ),
  CMD_OPTION_SPECIAL      ("create-aggregates-entities",        0,  0,1,&createAggregatesEntitiesFlag,   cmdOptionParseCreateAggregateEntityIds,NULL,0,          "re-create aggregated data entities","entity id"                  ),
  CMD_OPTION_SPECIAL      ("create-aggregates-storages",        0,  0,1,&createAggregatesStoragesFlag,   cmdOptionParseCreateAggregateStorageIds,NULL,0,         "re-create aggregated data storages","storage id"                 ),
  CMD_OPTION_SPECIAL      ("create-aggregates",                 0,  0,1,&createAggregatesFlag,           cmdOptionParseCreateAggregateIds,NULL,0,                "re-create aggregated data","id"                                  ),
  CMD_OPTION_BOOLEAN      ("clean-orphaned",                    0,  0,0,cleanOrphanedFlag,                                                                       "clean orphaned in index database"                                ),
  CMD_OPTION_BOOLEAN      ("clean-duplicates",                  0,  0,0,cleanDuplicateEntriesFlag,                                                               "clean duplicates in index database"                              ),
  CMD_OPTION_BOOLEAN      ("clean",                             0,  0,0,cleanFlag,                                                                               "clean index database"                                            ),
  CMD_OPTION_BOOLEAN      ("purge",                             0,  0,0,purgeFlag,                                                                               "purge deleted storages"                                          ),
  CMD_OPTION_BOOLEAN      ("purge-with-error",                  0,  0,0,purgeWithErrorFlag,                                                                      "purge storages with error"                                       ),
  CMD_OPTION_BOOLEAN      ("vacuum",                            0,  0,0,vacuumFlag,                                                                              "collect and free unused file space"                              ),
  CMD_OPTION_BOOLEAN      ("storages",                          's',0,0,showStoragesFlag,                                                                        "print storages"                                                  ),
  CMD_OPTION_BOOLEAN      ("entries",                           'e',0,0,showEntriesFlag,                                                                         "print entries"                                                   ),
  CMD_OPTION_BOOLEAN      ("entries-newest",                    0  ,0,0,showEntriesNewestFlag,                                                                   "print newest entries"                                            ),
  CMD_OPTION_BOOLEAN      ("names",                             'n',0,0,showNamesFlag,                                                                           "print values with names"                                         ),
  CMD_OPTION_BOOLEAN      ("header",                            'H',0,0,showHeaderFlag,                                                                          "print headers"                                                   ),
  CMD_OPTION_BOOLEAN      ("transaction",                       0  ,0,0,transactionFlag,                                                                         "enable transcations"                                             ),
  CMD_OPTION_BOOLEAN      ("no-foreign-keys",                   'f',0,0,foreignKeysFlag,                                                                         "disable foreign key constraints"                                 ),
  CMD_OPTION_BOOLEAN      ("force",                             0  ,0,0,forceFlag,                                                                               "force operation"                                                 ),
  CMD_OPTION_BOOLEAN      ("pipe",                              0  ,0,0,pipeFlag,                                                                                "read data from stdin and pipe into database"                     ),
  CMD_OPTION_STRING       ("tmp-directory",                     0,  0,0,tmpDirectory,                                                                            "temporary files directory","directory"                           ),
  CMD_OPTION_BOOLEAN      ("quiet",                             'q',0,0,quietFlag,                                                                               "suppress any output"                                             ),
  CMD_OPTION_BOOLEAN      ("time",                              't',0,0,timeFlag,                                                                                "print execution time"                                            ),
  CMD_OPTION_BOOLEAN      ("explain-query",                     0,  0,0,explainQueryPlanFlag,                                                                    "explain SQL queries"                                             ),
  CMD_OPTION_CSTRING      ("system-encoding",                   0,  0,0,systemEncoding,                                                                          "system character encoding to use","encoding"                     ),
  CMD_OPTION_CSTRING      ("console-encoding",                  0,  0,0,consoleEncoding,                                                                         "console character encoding to use","encoding"                    ),

  CMD_OPTION_BOOLEAN      ("table-names",                       0,  1,0,showTableNames,                                                                          "show table names"                                                ),
  CMD_OPTION_BOOLEAN      ("index-names",                       0,  1,0,showIndexNames,                                                                          "show index names"                                                ),
  CMD_OPTION_BOOLEAN      ("trigger-names",                     0,  1,0,showTriggerNames,                                                                        "show trigger names"                                              ),
  CMD_OPTION_BOOLEAN      ("drop-tables",                       0,  1,0,dropTablesFlag,                                                                          "drop all tables"                                                 ),
  CMD_OPTION_BOOLEAN      ("drop-triggers",                     0,  1,0,dropTriggersFlag,                                                                        "drop all triggers"                                               ),
  CMD_OPTION_BOOLEAN      ("drop-indices",                      0,  1,0,dropIndicesFlag,                                                                         "drop all indices"                                                ),

  CMD_OPTION_BOOLEAN      ("version",                           0  ,0,0,versionFlag,                                                                             "print version"                                                   ),
  CMD_OPTION_BOOLEAN      ("help",                              'h',0,0,helpFlag,                                                                                "print this help"                                                 ),
  CMD_OPTION_BOOLEAN      ("xhelp",                             0,  0,0,xhelpFlag,                                                                               "print help to extended options"                                  ),

  CMD_OPTION_DEPRECATED   ("verbose",                           0,  0,0,&verboseFlag,                        cmdOptionParseVerbose,NULL,1,                       "quiet"                                                           ),
  CMD_OPTION_DEPRECATED   ("create-indizes",                    0,  0,0,&createIndicesFlag,                  CmdOption_parseDeprecatedBooleanOption,NULL,1,      "create-indices"                                                  ),
  CMD_OPTION_DEPRECATED   ("create-fts-indizes",                0,  0,0,&createFTSIndicesFlag,               CmdOption_parseDeprecatedBooleanOption,NULL,1,      "create-fts-indices"                                              ),
  CMD_OPTION_DEPRECATED   ("drop-indizes",                      0,  1,0,&dropIndicesFlag,                    CmdOption_parseDeprecatedBooleanOption,NULL,1,      "drop-indices"                                                    ),
);

LOCAL ulong outputProgressCount;
LOCAL uint  outputProgressDigits;
LOCAL char  outputProgressBuffer[128];
LOCAL uint  outputProgressBufferLength;
LOCAL uint  outputProgressLength;

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
  assert(format != NULL);

  String line = String_new();

  // format line
  if (prefix != NULL) String_appendCString(line,prefix);
  String_appendVFormat(line,format,arguments);

  String consoleEncodedLine = convertSystemToConsoleEncodingAppend(String_new(),line);

  // output
  UNUSED_RESULT(fwrite(String_cString(consoleEncodedLine),1,String_length(consoleEncodedLine),stdout)); fflush(stdout);

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
  assert(format != NULL);

  if (verboseFlag && !quietFlag)
  {
    va_list arguments;
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
  assert(text != NULL);

  if (verboseFlag && !quietFlag)
  {
    String line = String_new();

    va_list arguments;
    va_start(arguments,text);
    String_appendCString(line,"Warning: ");
    String_appendVFormat(line,text,arguments);
    String_appendChar(line,'\n');
    va_end(arguments);
    UNUSED_RESULT(fwrite(String_cString(line),1,String_length(line),stderr));

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
  assert(text != NULL);

  String line = String_new();

  va_list arguments;
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);
  UNUSED_RESULT(fwrite(String_cString(line),1,String_length(line),stderr));

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

  UNUSED_VARIABLE(userData);

  // get timestamp
  struct timeval tv;
  gettimeofday(&tv,NULL);
  uint64 timestamp = (uint64)tv.tv_usec/1000L+((uint64)tv.tv_sec)*1000ULL;

  // info output
  if (timestamp > (lastTimestamp+250))
  {
    if (verboseFlag && !quietFlag)
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

  if      (n >= 1024LL*1024LL*1024LL*1024LL*1024LL) shortUnit = "PiB";
  else if (n >=        1024LL*1024LL*1024LL*1024LL) shortUnit = "TiB";
  else if (n >=               1024LL*1024LL*1024LL) shortUnit = "GiB";
  else if (n >=                      1024LL*1024LL) shortUnit = "MiB";
  else if (n >=                             1024LL) shortUnit = "KiB";
  else                                              shortUnit = "bytes";

  return shortUnit;
}

/***********************************************************************\
* Name   : initProgress
* Purpose: init print progress (if verbose)
* Input  : count - max. value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initProgress(ulong count)
{

  if (verboseFlag && !quietFlag)
  {
    outputProgressCount  = count;
    outputProgressDigits = (count > 0L) ? (uint)lrint(ceil(log10((double)count))) : 1;

    char buffer[64];
    stringFormat(buffer,sizeof(buffer),"  0.0%% (%*"PRIu64"/%*"PRIu64")",(int)outputProgressDigits,0,(int)outputProgressDigits,count);
    outputProgressLength = stringLength(buffer);

    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    stringFill(buffer,sizeof(buffer),outputProgressLength,'\b');
    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    fflush(stdout);
  }
}

/***********************************************************************\
* Name   : printProgress
* Purpose: print progress (if verbose)
* Input  : n - value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printProgress(ulong n)
{
  if (verboseFlag && !quietFlag)
  {
    double percentage = (outputProgressCount > 0L) ? ((double)n*1000.0)/((double)outputProgressCount*10.0) : 0.0;
    if (percentage > 100.0) percentage = 100.0;

    char buffer[64];
    stringFormat(buffer,sizeof(buffer),"%5.1lf%% (%*"PRIu64"/%*"PRIu64")",percentage,(int)outputProgressDigits,n,(int)outputProgressDigits,outputProgressCount);
    outputProgressLength = stringLength(buffer);

    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    stringFill(buffer,sizeof(buffer),outputProgressLength,'\b');
    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    fflush(stdout);
  }
}

/***********************************************************************\
* Name   : doneProgress
* Purpose: done print progress (if verbose)
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneProgress(void)
{
  char buffer[64];

  if (verboseFlag && !quietFlag)
  {
    stringFill(buffer,sizeof(buffer),outputProgressLength,' ');
    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    stringFill(buffer,sizeof(buffer),outputProgressLength,'\b');
    UNUSED_RESULT(fwrite(buffer,outputProgressLength,1,stdout));
    fflush(stdout);
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
  Errors error;

  // parse URI and fill default values
  DatabaseSpecifier databaseSpecifier;
  error = Database_parseSpecifier(&databaseSpecifier,databaseURI,INDEX_DEFAULT_DATABASE_NAME);
  if (error != ERROR_NONE)
  {
    printError(_("no valid database URI '%s'"),databaseURI);
    return error;
  }
  switch (databaseSpecifier.type)
  {
    case DATABASE_TYPE_SQLITE3:
      break;
    case DATABASE_TYPE_MARIADB:
      #if defined(HAVE_MARIADB)
        if (String_isEmpty(databaseSpecifier.mariadb.databaseName))
        {
          String_setCString(databaseSpecifier.mariadb.databaseName,DEFAULT_DATABASE_NAME);
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
  String printableDataseURI = Database_getPrintableName(String_new(),&databaseSpecifier,NULL);

  // open database
  DatabaseOpenModes openMode = (createFlag)
                                 ? DATABASE_OPEN_MODE_FORCE_CREATE
                                 : DATABASE_OPEN_MODE_READWRITE;
  openMode |= DATABASE_OPEN_MODE_AUX;
  error = Database_open(databaseHandle,
                        &databaseSpecifier,
                        NULL,  // databaseName
                        openMode,
                        WAIT_FOREVER
                       );
  if (Error_getCode(error) == ERROR_CODE_DATABASE_AUTHORIZATION)
  {
    switch (databaseSpecifier.type)
    {
      case DATABASE_TYPE_SQLITE3:
        break;
      case DATABASE_TYPE_MARIADB:
        #if defined(HAVE_MARIADB)
          if (Password_input(&databaseSpecifier.mariadb.password,"MariaDB database password",PASSWORD_INPUT_MODE_ANY))
          {
            error = Database_open(databaseHandle,
                                  &databaseSpecifier,
                                  NULL,  // databaseName
                                  openMode,
                                  WAIT_FOREVER
                                 );
          }
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      case DATABASE_TYPE_POSTGRESQL:
        #if defined(HAVE_POSTGRESQL)
          if (Password_input(&databaseSpecifier.postgresql.password,"PostgreSQL database password",PASSWORD_INPUT_MODE_ANY))
          {
            error = Database_open(databaseHandle,
                                  &databaseSpecifier,
                                  NULL,  // databaseName
                                  openMode,
                                  WAIT_FOREVER
                                 );
          }
        #else /* HAVE_POSTGRESQL */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_POSTGRESQL */
        break;
    }
  }
  if (error != ERROR_NONE)
  {
    printError(_("cannot open database '%s' (error: %s)!"),String_cString(printableDataseURI),Error_getText(error));
    String_delete(printableDataseURI);
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  printInfo("Opened database '%s'\n",String_cString(printableDataseURI));

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
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors dropTables(DatabaseHandle *databaseHandle, bool quietFlag)
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

  return error;
}

/***********************************************************************\
* Name   : dropViews
* Purpose: drop all views
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors dropViews(DatabaseHandle *databaseHandle, bool quietFlag)
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

  return error;
}

/***********************************************************************\
* Name   : dropIndices
* Purpose: drop all indices
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors dropIndices(DatabaseHandle *databaseHandle, bool quietFlag)
{
  Errors error;

  if (!quietFlag) printInfo("Drop indices...");
  error = ERROR_UNKNOWN;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop indices
    error = Database_dropIndices(databaseHandle);
    if (Database_getType(databaseHandle) == DATABASE_TYPE_SQLITE3)
    {
      // drop FTS indices
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

  return error;
}

/***********************************************************************\
* Name   : dropTriggers
* Purpose: drop all triggers
* Input  : databaseHandle - database handle
*          quietFlag      - TRUE to suppress output
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors dropTriggers(DatabaseHandle *databaseHandle, bool quietFlag)
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

  return error;
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
                             indexDefinition
                            );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("create database fail (error: %s)!"),Error_getText(error));
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

// TODO: use
#if 0
LOCAL void logImportProgress(const char *format, ...)
{
  va_list arguments;

  va_start(arguments,format);
  vfprintf(stdout,format,arguments); fflush(stdout);
  va_end(arguments);
}
#endif

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

  if (verboseFlag && !quietFlag)
  {
    UNUSED_RESULT(fwrite(text,1,stringLength(text),stdout));
    fflush(stdout);
  }
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

  if (verboseFlag && !quietFlag)
  {
    stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                 "%2dh:%02dmin:%02ds",
                 totalTime/(60*60),
                 totalTime%(60*60)/60,
                 totalTime%60
                );
    stringFillAppend(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,' ');

    UNUSED_RESULT(fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout));

    stringFill(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,'\b');
    UNUSED_RESULT(fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout));

    UNUSED_RESULT(fwrite("\n",1,1,stdout));

    fflush(stdout);

    stringClear(outputProgressBuffer);
    outputProgressBufferLength = 0;
  }
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

  if (verboseFlag && !quietFlag)
  {
    if (estimatedRestTime < (99999*60*60))
    {
      stringAppendFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                         " / %5.1f%% %5uh:%02umin:%02us %c",
                         (float)progress/10.0,
                         estimatedRestTime/S_PER_HOUR,
                         (estimatedRestTime%S_PER_HOUR)/S_PER_MINUTE,
                         estimatedRestTime%S_PER_MINUTE,
                         WHEEL[wheelIndex]
                        );
    }
    else
    {
      stringAppendFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                         " / %5.1f%% -----h:--min:--s %c",
                         (float)progress/10.0,
                         WHEEL[wheelIndex]
                        );
    }
    outputProgressBufferLength = stringLength(outputProgressBuffer);

    UNUSED_RESULT(fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout));

    stringFill(outputProgressBuffer,sizeof(outputProgressBuffer),outputProgressBufferLength,'\b');
    UNUSED_RESULT(fwrite(outputProgressBuffer,1,outputProgressBufferLength,stdout));

    fflush(stdout);

    stringClear(outputProgressBuffer);
    outputProgressBufferLength = 0;

    wheelIndex = (wheelIndex+1) % 4;
  }
}

/***********************************************************************\
* Name   : formatProgressInfo
* Purpose: format progress info
* Input  : progress           - progres [%%]
*          estimatedTotalTime - estimated total time [s]
*          estimatedRestTime  - estimated rest time [s]
*          userData           - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void formatProgressInfo(uint  progress,
                              ulong estimatedTotalTime,
                              ulong estimatedRestTime,
                              void  *userData
                             )
{
  UNUSED_VARIABLE(estimatedTotalTime);
  UNUSED_VARIABLE(userData);

  if (verboseFlag && !quietFlag)
  {
    if (estimatedRestTime < (99999*60*60))
    {
      stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                   "%5.1f%% %5uh:%02umin:%02us",
                   (float)progress/10.0,
                   estimatedRestTime/S_PER_HOUR,
                   (estimatedRestTime%S_PER_HOUR)/S_PER_MINUTE,
                   estimatedRestTime%S_PER_MINUTE
                  );
    }
    else
    {
      stringFormat(outputProgressBuffer,sizeof(outputProgressBuffer),
                   "%5.1f%% -----h:--min:--s",
                   (float)progress/10.0
                  );
    }
    outputProgressBufferLength = stringLength(outputProgressBuffer);
  }
}

/***********************************************************************\
* Name   : getPostgreSQLFTSTokens
* Purpose: get PostgreSQL full-text-seach tokens from text
* Input  : string - token string variable
*          text   - text (can be NULL)
* Output : -
* Return : token string
* Notes  : -
\***********************************************************************/

LOCAL String getPostgreSQLFTSTokens(String tokens, ConstString text)
{
  String_clear(tokens);

  if (text != NULL)
  {
    Codepoint codepoint;
    bool      spaceFlag = FALSE;
    STRING_CHAR_ITERATE_UTF8(text,codepoint)
    {
      if (!isCharUTF8(codepoint))
      {
        if      (isalnum((int)codepoint))
        {
          String_appendCharUTF8(tokens,codepoint);
          spaceFlag = FALSE;
        }
        else if (!spaceFlag)
        {
          String_appendChar(tokens,' ');
          spaceFlag = TRUE;
        }
      }
      else
      {
        String_appendCharUTF8(tokens,codepoint);
        spaceFlag = FALSE;
      }
    }
  }

  return tokens;
}

#include "index/index_version6.c"
#include "index/index_version7.c"
#include "index/index_version8.c"

/***********************************************************************\
* Name   : importIntoDatabase
* Purpose: import database
* Input  : databaseHandle - database handle
*          databaseURI    - database URI
* Output :
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors importIntoDatabase(DatabaseHandle *databaseHandle, const char *databaseURI)
{
  Errors error;

  // parse URI and fill in default values
  DatabaseSpecifier databaseSpecifier;
  error = Database_parseSpecifier(&databaseSpecifier,databaseURI,INDEX_DEFAULT_DATABASE_NAME);
  if (error != ERROR_NONE)
  {
    return error;
  }
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
  String printableDatabaseURI = Database_getPrintableName(String_new(),&databaseSpecifier,NULL);

  printInfo("Import database '%s':\n",String_cString(printableDatabaseURI));

  DatabaseHandle oldDatabaseHandle;
  error = Database_open(&oldDatabaseHandle,
                        &databaseSpecifier,
                        NULL,  // databaseName
                        DATABASE_OPEN_MODE_READ,
                        WAIT_FOREVER
                       );
  if (error != ERROR_NONE)
  {
    printError(_("open database fail (error: %s)!"),Error_getText(error));
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  uint indexVersion;
  error = Database_getUInt(&oldDatabaseHandle,
                           &indexVersion,
                           "meta",
                           "value",
                           "name='version'",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // orderGroup
                          );
  if (error != ERROR_NONE)
  {
    printError(_("get database version fail (error: %s)!"),Error_getText(error));    String_delete(printableDatabaseURI);
    Database_doneSpecifier(&databaseSpecifier);
    return error;
  }

  size_t maxSteps = 0LL;
  switch (indexVersion)
  {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      break;
    case 6:
      maxSteps = getImportStepsVersion6(&oldDatabaseHandle);
      break;
    case 7:
      maxSteps = getImportStepsVersion7(&oldDatabaseHandle);
      break;
    case INDEX_CONST_VERSION:
      maxSteps = getImportStepsVersion8(&oldDatabaseHandle);
      break;
    default:
      break;
  }
  ProgressInfo progressInfo;
  ProgressInfo_init(&progressInfo,
                    NULL,  // parentProgressInfo
                    128,  // filterWindowSize
                    500,  // reportTime
                    maxSteps,
                    CALLBACK_(NULL,NULL),  // progresInitFunction
                    CALLBACK_(NULL,NULL),  // progressDoneFunction
                    CALLBACK_(outputProgressInfo,NULL),
                    NULL  // text
                   );

  switch (indexVersion)
  {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case 6:
      error = importIndexVersion6(&oldDatabaseHandle,
                                  databaseHandle,
                                  &progressInfo
                                 );
      break;
    case 7:
      error = importIndexVersion7(&oldDatabaseHandle,
                                  databaseHandle,
                                  &progressInfo
                                 );
      break;
    case INDEX_CONST_VERSION:
      error = importIndexVersion8(&oldDatabaseHandle,
                                  databaseHandle,
                                  &progressInfo
                                 );
      break;
    default:
      // unknown version if index
      error = ERRORX_(DATABASE_VERSION_UNKNOWN,0,"%d",indexVersion);
      break;
  }
  if (error != ERROR_NONE)
  {
    printError(_("import database fail: %s!"),Error_getText(error));
    return error;
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
* Return : TRUE iff integrity OK
* Notes  : -
\***********************************************************************/

LOCAL bool checkIntegrity(DatabaseHandle *databaseHandle)
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
    printError(_("quick integrity check fail (error: %s)!"),Error_getText(error));
    return FALSE;
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
    printError(_("foreign key check fail (error: %s)!"),Error_getText(error));
    return FALSE;
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
    printError(_("integrity check fail (error: %s)!"),Error_getText(error));
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : checkOrphanedEntries
* Purpose: check database orphaned entries
* Input  : databaseHandle - database handle
* Output : -
* Return : number of orphaned entries
* Notes  : -
\***********************************************************************/

LOCAL ulong checkOrphanedEntries(DatabaseHandle *databaseHandle)
{
  Errors error;

  size_t totalCount = 0;

  printInfo("Check orphaned:\n");

  uint n;
  // check entries without fragments
  printInfo("  entries without storage name...      ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entryFragments \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                           ",
                           "COUNT(entryFragments.id)",
                           "storages.id IS NULL OR storages.name IS NULL OR storages.name=''",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned entry fragments check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check entries without fragments
  printInfo("  file entries without fragments...    ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "fileEntries \
                              LEFT JOIN entries ON entries.id=fileEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.entryId \
                           ",
                           "COUNT(fileEntries.id)",
                           "    entries.deletedFlag!=TRUE \
                            AND entryFragments.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned file entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (ulong)n;
  printInfo("  image entries without fragments...   ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "imageEntries \
                              LEFT JOIN entries ON entries.id=imageEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.entryId \
                           ",
                           "COUNT(imageEntries.id)",
                           "    entries.deletedFlag!=TRUE \
                            AND entryFragments.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned image entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (uint64)n;
  printInfo("  hardlink entries without fragments...");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "hardlinkEntries \
                              LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                              LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.entryId \
                           ",
                           "COUNT(hardlinkEntries.id)",
                           "    entries.deletedFlag!=TRUE \
                            AND entryFragments.id IS NULL \
                           ",
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
    printError(_("orphaned hardlink entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...        ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND fileEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned file entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;
  printInfo("  entries without image entry...       ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND imageEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned image entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;
  printInfo("  entries without directory entry...   ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND directoryEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_DIRECTORY)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned directory entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;
  printInfo("  entries without link entry...        ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND linkEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_LINK)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned link entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;
  printInfo("  entries without hardlink entry...    ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND hardlinkEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned hardlink entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;
  printInfo("  entries without special entry...     ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                           ",
                           "COUNT(entries.id)",
                           "    entries.type=? \
                            AND entries.deletedFlag!=TRUE \
                            AND specialEntries.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_SPECIAL)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned special entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check entries with wrong entity
  printInfo("  entries with wrong entity...         ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entries \
                              LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                           ",
                           "COUNT(entries.id)",
                           "    entries.deletedFlag!=TRUE \
                            AND (entries.type=? OR entries.type=? OR entries.type=?)\
                            AND storages.entityId!=entries.entityId \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE),
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE),
                             DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned file entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check storages without name
  printInfo("  storages without name...             ");
  error = Database_getUInt(databaseHandle,
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
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("storages name check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check storages with invalid state
  printInfo("  storages with invalid state...       ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "(state<?) OR (state>?)",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT(INDEX_CONST_STATE_OK),
                             DATABASE_FILTER_UINT(INDEX_CONST_STATE_ERROR)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("invalid storages check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  // check entities without storages
  printInfo("  entities without entries... ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities \
                              LEFT JOIN entries ON entries.entityId=entities.id \
                              LEFT JOIN entriesNewest ON entriesNewest.entityId=entities.id \
                           ",
                           "COUNT(entities.id)",
                           "    entities.deletedFlag!=TRUE \
                            AND entities.id!=? \
                            AND entries.id IS NULL \
                            AND entriesNewest.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY(INDEX_DEFAULT_ENTITY_DATABASE_ID)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned entities check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  printInfo("  entities without storages... ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities \
                              LEFT JOIN storages ON storages.entityId=entities.id \
                           ",
                           "COUNT(entities.id)",
                           "    entities.deletedFlag!=TRUE \
                            AND entities.id!=? \
                            AND storages.id IS NULL \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY(INDEX_DEFAULT_ENTITY_DATABASE_ID)
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned entities check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      // check FTS entries without entry
      printInfo("  FTS entries without entry...         ");
      error = Database_getUInt(databaseHandle,
                               &n,
                               "FTS_entries \
                                  LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                               ",
                               "COUNT(entryId)",
                               "entries.id IS NULL",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // group
                              );
      if (error == ERROR_NONE)
      {
        printInfo("%u\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError(_("orphaned FTS entries check fail (error: %s)!"),Error_getText(error));
      }
      totalCount += (size_t)n;

      // check FTS storages without storage
      printInfo("  FTS storages without storage...      ");
      error = Database_getUInt(databaseHandle,
                               &n,
                               "FTS_storages \
                                  LEFT JOIN storages ON storages.id=FTS_storages.storageId \
                               ",
                               "COUNT(FTS_storages.rowid)",
                               "storages.id IS NULL",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // group
                              );
      if (error == ERROR_NONE)
      {
        printInfo("%u\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError(_("orphaned FTS storages check fail (error: %s)!"),Error_getText(error));
      }
      totalCount += (size_t)n;
      break;
    case DATABASE_TYPE_MARIADB:
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // check FTS entries without entry
      printInfo("  FTS entries without entry...         ");
      error = Database_getUInt(databaseHandle,
                               &n,
                               "FTS_entries \
                                  LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                               ",
                               "COUNT(entryId)",
                               "entries.id IS NULL",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // group
                              );
      if (error == ERROR_NONE)
      {
        printInfo("%u\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError(_("orphaned FTS entries check fail (error: %s)!"),Error_getText(error));
      }
      totalCount += (size_t)n;

      // check FTS storages without storage
      printInfo("  FTS storages without storage...      ");
      error = Database_getUInt(databaseHandle,
                               &n,
                               "FTS_storages \
                                  LEFT JOIN storages ON storages.id=FTS_storages.storageId \
                               ",
                               "COUNT(FTS_storages.rowid)",
                               "storages.id IS NULL",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // group
                              );
      if (error == ERROR_NONE)
      {
        printInfo("%u\n",n);
      }
      else
      {
        printInfo("FAIL!\n");
        printError(_("orphaned FTS storages fail (error: %s)!"),Error_getText(error));
      }
      totalCount += (size_t)n;
      break;
  }

  // check newest entries without entry
  printInfo("  newest entries without entry...      ");
  error = Database_getUInt(databaseHandle,
                           &n,
                           "entriesNewest \
                              LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                           ",
                           "COUNT(entriesNewest.id)",
                           "entries.id IS NULL",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error == ERROR_NONE)
  {
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("orphaned newest entries check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += (size_t)n;

  if (totalCount > 0)
  {
    printWarning(_("found %zu orphaned entries. Clean is recommented"),totalCount);
  }

  // free resources

  return totalCount;
}

/***********************************************************************\
* Name   : checkDuplicates
* Purpose: check database duplicates
* Input  : databaseHandle - database handle
* Output : -
* Return : number of duplicates
* Notes  : -
\***********************************************************************/

LOCAL ulong checkDuplicates(DatabaseHandle *databaseHandle)
{
  String name;
  Errors error;

  name       = String_new();
  size_t totalCount = 0;

  printInfo("Check duplicates:\n");

  // check duplicate storages
  printInfo("  storages...                          ");
  size_t n = 0;
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
    printInfo("%u\n",n);
  }
  else
  {
    printInfo("FAIL!\n");
    printError(_("duplicates check fail (error: %s)!"),Error_getText(error));
  }
  totalCount += n;

  if (totalCount > 0)
  {
    printWarning(_("found %zu duplicate entries. Clean is recommented"),totalCount);
  }

  // free resources
  String_delete(name);

  return totalCount;
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
  Errors error;

  // init variables

  printInfo("Optimize:\n");

  ulong       n;
  ConstString name;

  printInfo("  Tables...");
  StringList tableNameList;
  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("get tables fail (error: %s)!"),Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  initProgress(StringList_count(&tableNameList));
  n = 0;
  STRINGLIST_ITERATE(&tableNameList,name)
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          char sqlString[256];

          error = Database_execute(databaseHandle,
                                   NULL,  // changedRowCount
                                   DATABASE_FLAG_NONE,
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "ANALYZE %s",
                                                String_cString(name)
                                               )
                                  );
        }
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
    printProgress(n);
  }
  doneProgress();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("optimize table '%s' fail (error: %s)!"),String_cString(name),Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  printInfo("OK  \n");
  StringList_done(&tableNameList);

  printInfo("  Indices...");
  StringList_clear(&tableNameList);
  error = Database_getIndexList(&tableNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("get indices fail (error: %s)!"),Error_getText(error));
    return;
  }
  initProgress(StringList_count(&tableNameList));
  n = 0;
  STRINGLIST_ITERATE(&tableNameList,name)
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          char sqlString[256];

          error = Database_execute(databaseHandle,
                                   NULL,  // changedRowCount
                                   DATABASE_FLAG_NONE,
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "ANALYZE %s",
                                                String_cString(name)
                                               )
                                  );
        }
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
    printProgress(n);
  }
  doneProgress();
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("optimize index '%s' fail (error: %s)!"),String_cString(name),Error_getText(error));
    StringList_done(&tableNameList);
    return;
  }
  printInfo("OK  \n");
  StringList_done(&tableNameList);

  // free resources
}

/***********************************************************************\
* Name   : getFTSMatchString
* Purpose: get full-text-search filter match string
* Input  : string         - string variable
*          databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          patternText    - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSMatchString(String         string,
                               DatabaseHandle *databaseHandle,
                               const char     *tableName,
                               const char     *columnName,
                               ConstString    patternText
                              )
{
  assert(string != NULL);
  assert(databaseHandle != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  String_clear(string);

  if (!String_isEmpty(patternText))
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          String_appendFormat(string,"%s MATCH '",tableName);

          StringTokenizer stringTokenizer;
          String_initTokenizer(&stringTokenizer,
                               patternText,
                               STRING_BEGIN,
                               STRING_WHITE_SPACES,
                               STRING_QUOTES,
                               TRUE
                              );
          ConstString token;
          while (String_getNextToken(&stringTokenizer,&token,NULL))
          {
            bool      addedTextFlag    = FALSE;
            bool      addedPatternFlag = FALSE;
            Codepoint codepoint;
            STRING_CHAR_ITERATE_UTF8(token,codepoint)
            {
              if (isalnum(codepoint) || isCharUTF8(codepoint))
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

          String_appendFormat(string,"'");
        }
        break;
      case DATABASE_TYPE_MARIADB:
        {
          String_appendFormat(string,"MATCH(%s.%s) AGAINST('",tableName,columnName);

          StringTokenizer stringTokenizer;
          String_initTokenizer(&stringTokenizer,
                               patternText,
                               STRING_BEGIN,
                               STRING_WHITE_SPACES,
                               STRING_QUOTES,
                               TRUE
                              );
          ConstString token;
          while (String_getNextToken(&stringTokenizer,&token,NULL))
          {
            bool      addedTextFlag    = FALSE;
            bool      addedPatternFlag = FALSE;
            Codepoint codepoint;
            STRING_CHAR_ITERATE_UTF8(token,codepoint)
            {
              if (isalnum(codepoint) || isCharUTF8(codepoint))
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

          String_appendFormat(string,"' IN BOOLEAN MODE)");
        }
        break;
      case DATABASE_TYPE_POSTGRESQL:
        {
          String_appendFormat(string,"%s.%s @@ to_tsquery('",tableName,columnName);

          StringTokenizer stringTokenizer;
          String_initTokenizer(&stringTokenizer,
                               patternText,
                               STRING_BEGIN,
                               STRING_WHITE_SPACES,
                               STRING_QUOTES,
                               TRUE
                              );
          bool        firstTokenFlag = TRUE;
          ConstString token;
          while (String_getNextToken(&stringTokenizer,&token,NULL))
          {
            if (!firstTokenFlag)
            {
              String_appendCString(string," & ");
            }

            bool      addedTextFlag    = FALSE;
            bool      addedPatternFlag = FALSE;
            Codepoint codepoint;
            STRING_CHAR_ITERATE_UTF8(token,codepoint)
            {
              if (isalnum(codepoint) || isCharUTF8(codepoint))
              {
                if (addedPatternFlag)
                {
                  String_appendCString(string," & ");
                  addedPatternFlag = FALSE;
                }
                String_appendCharUTF8(string,codepoint);
                addedTextFlag = TRUE;
              }
              else
              {
                if (addedTextFlag && !addedPatternFlag)
                {
                  String_appendCString(string,":*");
                  addedTextFlag    = FALSE;
                  addedPatternFlag = TRUE;
                }
              }
            }
            if (addedTextFlag && !addedPatternFlag)
            {
              String_appendCString(string,":*");
            }

            firstTokenFlag = FALSE;
          }
          String_doneTokenizer(&stringTokenizer);

          String_appendFormat(string,"')");
        }
        break;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : createTriggers
* Purpose: create triggers
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createTriggers(DatabaseHandle *databaseHandle)
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
    printError(_("create triggers fail (error: %s)!"),Error_getText(error));
    return error;
  }

  // create new triggeres
  INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_TRIGGERS[Database_getType(databaseHandle)], triggerDefinition, error == ERROR_NONE)
  {
    error = Database_execute(databaseHandle,
                             NULL,  // changedRowCount
                             DATABASE_FLAG_NONE,
                             triggerDefinition
                            );
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("create triggers fail (error: %s)!"),Error_getText(error));
    return error;
  }

  printInfo("OK\n");

  return ERROR_NONE;
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
  Errors error;

  StringList tableNameList;
  StringList_init(&tableNameList);
  error = Database_getTableList(&tableNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    StringList_done(&tableNameList);
    printError(_("cannot get table names (error: %s)!"),Error_getText(error));
    return;
  }
  ConstString tableName;
  STRINGLIST_ITERATE(&tableNameList,tableName)
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
  Errors error;

  StringList indexNameList;
  StringList_init(&indexNameList);
  error = Database_getIndexList(&indexNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    StringList_done(&indexNameList);
    printError(_("cannot get index names (error: %s)!"),Error_getText(error));
    return;
  }
  ConstString indexName;
  STRINGLIST_ITERATE(&indexNameList,indexName)
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
  Errors error;

  StringList triggerNameList;
  StringList_init(&triggerNameList);
  error = Database_getTriggerList(&triggerNameList,databaseHandle,NULL);
  if (error != ERROR_NONE)
  {
    StringList_done(&triggerNameList);
    printError(_("cannot get trigger names (error: %s)!"),Error_getText(error));
    return;
  }
  ConstString triggerName;
  STRINGLIST_ITERATE(&triggerNameList,triggerName)
  {
    printf("%s\n",String_cString(triggerName));
  }
  StringList_done(&triggerNameList);
}

/***********************************************************************\
* Name   : createIndices
* Purpose: create all indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createIndices(DatabaseHandle *databaseHandle)
{
  Errors error;

  /* manual forced delete
  PRAGMA writable_schema = ON;
  DELETE FROM sqlite_master WHERE type = 'table' AND name = 'tablename';
  */

  printInfo("Create indices:\n");

  error = ERROR_UNKNOWN;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop all existing indices
    printInfo("  Discard indices...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          StringList indexNameList;
          StringList_init(&indexNameList);
          error = Database_getIndexList(&indexNameList,databaseHandle,NULL);
          ConstString indexName;
          STRINGLIST_ITERATEX(&indexNameList,indexName,error == ERROR_NONE)
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
        error = ERROR_NONE;
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        error = ERROR_NONE;
        break;
    }
    if (error == ERROR_NONE)
    {
      printInfo("OK");
    }
    else
    {
      printInfo("FAIL (ignored)");
      error = ERROR_NONE;
    }

    // create new indices (if not exists)
    printInfo("  Create indices...");
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        {
          const char *indexDefinition;
          INDEX_DEFINITIONS_ITERATEX(INDEX_DEFINITION_INDICES[Database_getType(databaseHandle)],indexDefinition,error == ERROR_NONE)
          {
            error = Database_execute(databaseHandle,
                                     NULL,  // changedRowCount
                                     DATABASE_FLAG_NONE,
                                     indexDefinition
                                    );
          }
        }
        break;
      case DATABASE_TYPE_MARIADB:
        #if defined(HAVE_MARIADB)
          error = ERROR_NONE;
        #else /* HAVE_MARIADB */
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_MARIADB */
        break;
      case DATABASE_TYPE_POSTGRESQL:
        #if defined(HAVE_POSTGRESQL)
          error = ERROR_NONE;
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
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    printError(_("create indices fail (error: %s)!"),Error_getText(error));
    return error;
  }
  (void)Database_flush(databaseHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createFTSIndices
* Purpose: create FTS indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createFTSIndices(DatabaseHandle *databaseHandle)
{
  Errors       error;
  uint         maxSteps;
  uint64       n;
  ProgressInfo progressInfo;

  printInfo("Create FTS indices:\n");

  error = ERROR_UNKNOWN;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    // drop FTS indices
    printInfo("  Discard FTS indices...");
    error = ERROR_NONE;
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
    if (error == ERROR_NONE)
    {
      printInfo("OK\n");
    }
    else
    {
      printInfo("FAIL (ignored)\n");
      error = ERROR_NONE;
    }

    // create new FTS tables (if not exists)
    printInfo("  Create FTS indices...");
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
                                     indexDefinition
                                    );
          }
        }
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        error = ERROR_NONE;
        break;
      case DATABASE_TYPE_POSTGRESQL:
        // nothing to do
        error = ERROR_NONE;
        break;
    }
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }

    // get max. steps
    maxSteps = 0LL;
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        maxSteps += 2;
        error = ERROR_NONE;
        break;
      case DATABASE_TYPE_MARIADB:
        maxSteps += 0;
        error = ERROR_NONE;
        break;
      case DATABASE_TYPE_POSTGRESQL:
        if (error == ERROR_NONE)
        {
          error = Database_getUInt64(databaseHandle,
                                     &n,
                                     "entries",
                                     "COUNT(id)",
                                     "deletedFlag!=TRUE",
                                     DATABASE_FILTERS
                                     (
                                     ),
                                     NULL  // group
                                    );
          maxSteps += n;
        }
        if (error == ERROR_NONE)
        {
          error = Database_getUInt64(databaseHandle,
                                     &n,
                                     "storages",
                                     "COUNT(id)",
                                     "deletedFlag!=TRUE",
                                     DATABASE_FILTERS
                                     (
                                     ),
                                     NULL  // group
                                    );
          maxSteps += n;
        }
        break;
    }
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }

    // fill FTS tables
    ProgressInfo_init(&progressInfo,
                      NULL,  // parentProgressInfo
                      128,  // filterWindowSize
                      500,  // reportTime
                      maxSteps,
                      CALLBACK_(outputProgressInit,NULL),  // progresInitFunction
                      CALLBACK_(outputProgressDone,NULL),  // progressDoneFunction
                      CALLBACK_(outputProgressInfo,NULL),
                      NULL  // userData
                     );
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        if (error == ERROR_NONE)
        {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
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
                                        "storages.deletedFlag!=TRUE \
                                        ",
                                        DATABASE_FILTERS
                                        (
                                        ),
                                        NULL,  // groupBy
                                        NULL,  // orderBy
                                        0LL,
                                        DATABASE_UNLIMITED
                                       );
          ProgressInfo_step(&progressInfo);
        }
        if (error == ERROR_NONE)
        {
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
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
                                        "entries.deletedFlag!=TRUE \
                                        ",
                                        DATABASE_FILTERS
                                        (
                                        ),
                                        NULL,  // groupBy
                                        NULL,  // orderBy
                                        0LL,
                                        DATABASE_UNLIMITED
                                       );
          ProgressInfo_step(&progressInfo);
        }
        break;
      case DATABASE_TYPE_MARIADB:
        // nothing to do
        error = ERROR_NONE;
        break;
      case DATABASE_TYPE_POSTGRESQL:
        {
          String  tokens;

          tokens = String_new();
          error  = ERROR_NONE;

          if (error == ERROR_NONE)
          {
            error = Database_get(databaseHandle,
                                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                 {
                                   DatabaseId storageId;
                                   String     name;

                                   assert(values != NULL);
                                   assert(valueCount == 2);

                                   UNUSED_VARIABLE(userData);
                                   UNUSED_VARIABLE(valueCount);

                                   storageId = values[0].id;
                                   name      = values[1].string;

                                   getPostgreSQLFTSTokens(tokens,name);
                                   error = Database_insert(databaseHandle,
                                                           NULL,  // insertRowId
                                                           "FTS_storages",
                                                           DATABASE_FLAG_NONE,
                                                           DATABASE_VALUES
                                                           (
                                                             DATABASE_VALUE_KEY   ("storageId", storageId),
                                                             DATABASE_VALUE_STRING("name",      "to_tsvector(?)", tokens)
                                                           ),
                                                           DATABASE_COLUMNS_NONE,
                                                           DATABASE_FILTERS_NONE
                                                          );

                                   ProgressInfo_step(&progressInfo);

                                   return error;
                                 },NULL),
                                 NULL,  // changedRowCount
                                 DATABASE_TABLES
                                 (
                                   "storages"
                                 ),
                                 DATABASE_FLAG_FETCH_ALL,
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
                                   DatabaseId entryId;
                                   String     name;

                                   assert(values != NULL);
                                   assert(valueCount == 2);

                                   UNUSED_VARIABLE(userData);
                                   UNUSED_VARIABLE(valueCount);

                                   entryId = values[0].id;
                                   name    = values[1].string;

                                   getPostgreSQLFTSTokens(tokens,name);
                                   error = Database_insert(databaseHandle,
                                                           NULL,  // insertRowId
                                                           "FTS_entries",
                                                           DATABASE_FLAG_NONE,
                                                           DATABASE_VALUES
                                                           (
                                                             DATABASE_VALUE_KEY   ("entryId", entryId),
                                                             DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                                           ),
                                                           DATABASE_COLUMNS_NONE,
                                                           DATABASE_FILTERS_NONE
                                                          );

                                   ProgressInfo_step(&progressInfo);

                                   return error;
                                 },NULL),
                                 NULL,  // changedRowCount
                                 DATABASE_TABLES
                                 (
                                   "entries"
                                 ),
                                 DATABASE_FLAG_FETCH_ALL,
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
                                 NULL,  // orderBy
                                 0LL,
                                 DATABASE_UNLIMITED
                                );
          }

          String_delete(tokens);
        }
        break;
    }
    ProgressInfo_done(&progressInfo);
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
    printInfo("OK\n");
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    printError(_("create FTS indices fail (error: %s)!"),Error_getText(error));
    return error;
  }
  (void)Database_flush(databaseHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : reindex
* Purpose: re-create existing indices
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_CODE or error none
* Notes  : -
\***********************************************************************/

LOCAL Errors reindex(DatabaseHandle *databaseHandle)
{
  Errors error;

  printInfo("Reindex...");

  error = Database_reindex(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("reindex fail (error: %s)!"),Error_getText(error));
    return error;
  }

  printInfo("OK\n");

  return ERROR_NONE;
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
  *          userData  - user data (not used)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode, void *userData);
  void freeEntryNode(EntryNode *entryNode, void *userData)
  {
    assert(entryNode != NULL);

    UNUSED_VARIABLE(userData);

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
  List_init(&entryList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));

  // get entries info to add
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
  if (error != ERROR_NONE)
  {
    List_done(&entryList);
    return error;
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
  if (error != ERROR_NONE)
  {
    List_done(&entryList);
    return error;
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
  if (error != ERROR_NONE)
  {
    List_done(&entryList);
    return error;
  }

  // free resources
  List_done(&entryList);

  return ERROR_NONE;
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
  *          userData  - user data (not used)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeEntryNode(EntryNode *entryNode, void *userData);
  void freeEntryNode(EntryNode *entryNode, void *userData)
  {
    assert(entryNode != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(entryNode->name);
  }

  EntryList entryList;
  Errors    error;
  EntryNode *entryNode;

  assert(databaseHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
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
                            DATABASE_UNLIMITED
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
  List_done(&entryList);

  return error;
}

/***********************************************************************\
* Name   : createNewest
* Purpose: create newest data
* Input  : databaseHandle - database handle
*          storageIds     - database storage id array
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createNewest(DatabaseHandle *databaseHandle, Array storageIds)
{
  Errors     error;
  uint       totalEntriesNewestCount;
  ulong      n,m;
  DatabaseId storageId;

  // initialize variables
  error = ERROR_NONE;

  if (Array_isEmpty(&storageIds))
  {
    printInfo("Collect data for newest entries...");
    initProgress(2);

    // get storage ids
    error = Database_getIds(databaseHandle,
                            &storageIds,
                            "storages",
                            "id",
                            "deletedFlag!=TRUE",
                            DATABASE_FILTERS
                            (
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      doneProgress();
      printInfo("FAIL\n");
      printError(_("collect newest fail (error: %s)!"),Error_getText(error));
      return error;
    }
    printProgress(1);

    // get total counts
    error = Database_getUInt(databaseHandle,
                             &totalEntriesNewestCount,
                             "entriesNewest",
                             "COUNT(id)",
                             DATABASE_FILTERS_NONE,
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      doneProgress();
      printInfo("FAIL\n");
      printError(_("collect newest fail (error: %s)!"),Error_getText(error));
      return error;
    }
    printProgress(2);

    doneProgress();
    printInfo("OK  \n");

    // delete all newest entries
    printInfo("Purge newest entries...");
    initProgress(totalEntriesNewestCount);
    n = 0;
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
        printProgress(n);
      }
      while ((error == ERROR_NONE) && (m > 0));
    }
    doneProgress();
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError(_("create newest fail (error: %s)!"),Error_getText(error));
      return error;
    }
    (void)Database_flush(databaseHandle);
    printInfo("OK\n");

    // insert newest entries
    printInfo("Create newest entries...");
    initProgress(Array_length(&storageIds));
    n = 0;
    ARRAY_ITERATEX(&storageIds,storageId,error == ERROR_NONE)
    {
      error = addToNewest(databaseHandle,storageId);
      n++;
      printProgress(n);
    }
    doneProgress();
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError(_("create newest fail (error: %s)!"),Error_getText(error));
      return error;
    }
    (void)Database_flush(databaseHandle);
    printInfo("OK\n");
  }
  else
  {
    // Refresh newest entries
    printInfo("Create newest entries:\n");
    ARRAY_ITERATE(&storageIds,storageId)
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
    if (error != ERROR_NONE)
    {
      printInfo("FAIL\n");
      printError(_("create newest fail (error: %s)!"),Error_getText(error));
      return error;
    }
    (void)Database_flush(databaseHandle);
  }

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createAggregatesEntities
* Purpose: create aggregates entities data
* Input  : databaseHandle - database handle
*          entityIds      - database entity id array
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createAggregatesEntities(DatabaseHandle *databaseHandle, const Array entityIds)
{
  String     entityIdsString;
  DatabaseId entityId;
  Errors     error;
  uint       totalCount;
  char       filterString[1024];
  ulong      n;

  // init variables
  entityIdsString = String_new();
  ARRAY_ITERATE(&entityIds,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,entityId);
  }

  printInfo("Create aggregates for entities...");

  // get entities total count
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
    printError(_("create aggregates fail (error: %s)!"),Error_getText(error));
    return error;
  }

  initProgress(totalCount);
  n = 0;

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
                             printError(_("create aggregates fail for entity #%"PRIi64" (error: %s)!"),entityId,Error_getText(error));
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
                             printError(_("create newest aggregates fail for entity #%"PRIi64" (error: %s)!"),entityId,Error_getText(error));
                             return error;
                           }

                           n++;
                           printProgress(n);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entities"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
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
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
  }
  if (error != ERROR_NONE)
  {
    doneProgress();
    printInfo("FAIL\n");
    printError(_("create aggregates fail (error: %s)!"),Error_getText(error));
    String_delete(entityIdsString);
    return error;
  }
  (void)Database_flush(databaseHandle);

  doneProgress();

  printInfo("OK  \n");

  // free resources
  String_delete(entityIdsString);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createAggregatesStorages
* Purpose: create aggregates storages data
* Input  : databaseHandle - database handle
*          storageIds     - database storage id array
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createAggregatesStorages(DatabaseHandle *databaseHandle, const Array storageIds)
{
  Errors error;

  String     storageIdsString = String_new();
  DatabaseId storageId;
  ARRAY_ITERATE(&storageIds,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_appendFormat(storageIdsString,"%"PRIi64,storageId);
  }

  printInfo("Create aggregates for storages...");

  // get storage total count
  uint totalCount;
  char filterString[1024];
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
    printError(_("create aggregates fail (error: %s)!"),Error_getText(error));
    return error;
  }

  initProgress(totalCount);
  ulong n = 0;

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
                             printError(_("create aggregates fail for storage #%"PRIi64" (error: %s)!"),storageId,Error_getText(error));
                             return error;
                           }

                           n++;
                           printProgress(n);

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "storages"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
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
    if (error != ERROR_NONE)
    {
      DATABASE_TRANSACTION_ABORT(databaseHandle);
      break;
    }
  }
  if (error != ERROR_NONE)
  {
    doneProgress();
    printInfo("FAIL\n");
    printError(_("create aggregates fail (error: %s)!"),Error_getText(error));
    String_delete(storageIdsString);
    return error;
  }
  (void)Database_flush(databaseHandle);

  doneProgress();

  printInfo("OK  \n");

  // free resources
  String_delete(storageIdsString);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanOrphanedEntries
* Purpose: purge orphaned entries
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanOrphanedEntries(DatabaseHandle *databaseHandle)
{
  const size_t INCREMENT_SIZE = 4096;

  Errors error;

  // initialize variables
  String storageName = String_new();
  Array  ids;
  Array_init(&ids,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  size_t totalCount  = 0;

  printInfo("Clean-up orphaned:\n");

  size_t n = 0;

  // clean fragments/directory entries/link entries/special entries without or an empty storage name
  printInfo("  entries without storage name...      ");
  error = ERROR_UNKNOWN;
  do
  {
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
                            ),
                            INCREMENT_SIZE
                           );
      error = Database_deleteByIds(databaseHandle,
                                   &n,
                                   "entryFragments",
                                   "id",
                                   DATABASE_FLAG_NONE,
                                   Array_cArray(&ids),
                                   Array_length(&ids)
                                  );
    }
    (void)Database_flush(databaseHandle);
  }
  while (!Array_isEmpty(&ids) && (error == ERROR_NONE));
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }

  do
  {
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
                            ),
                            INCREMENT_SIZE
                           );
      error = Database_deleteByIds(databaseHandle,
                                   &n,
                                   "directoryEntries",
                                   "id",
                                   DATABASE_FLAG_NONE,
                                   Array_cArray(&ids),
                                   Array_length(&ids)
                                  );
    }
    (void)Database_flush(databaseHandle);
  }
  while (!Array_isEmpty(&ids) && (error == ERROR_NONE));
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }

  do
  {
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
                            ),
                            INCREMENT_SIZE
                           );
      error = Database_deleteByIds(databaseHandle,
                                   &n,
                                   "linkEntries",
                                   "id",
                                   DATABASE_FLAG_NONE,
                                   Array_cArray(&ids),
                                   Array_length(&ids)
                                  );
    }
    (void)Database_flush(databaseHandle);
  }
  while (!Array_isEmpty(&ids) && (error == ERROR_NONE));
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }

  do
  {
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
                            ),
                            INCREMENT_SIZE
                           );
      error = Database_deleteByIds(databaseHandle,
                                   &n,
                                   "specialEntries",
                                   "id",
                                   DATABASE_FLAG_NONE,
                                   Array_cArray(&ids),
                                   Array_length(&ids)
                                  );
    }
    (void)Database_flush(databaseHandle);
  }
  while (!Array_isEmpty(&ids) && (error == ERROR_NONE));
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean entries without fragments
  printInfo("  file entries without fragments...    ");
  n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "fileEntries \
                             LEFT JOIN entries ON entries.id=fileEntries.entryId \
                             LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.entryId \
                          ",
                          "fileEntries.id",
                          "    entries.deletedFlag!=TRUE \
                           AND entryFragments.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
    error = Database_deleteByIds(databaseHandle,
                                 &n,
                                 "fileEntries",
                                 "id",
                                 DATABASE_FLAG_NONE,
                                 Array_cArray(&ids),
                                 Array_length(&ids)
                                );
  }
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  image entries without fragments...   ");
  n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "imageEntries \
                             LEFT JOIN entries ON entries.id=imageEntries.entryId \
                             LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.entryId \
                          ",
                          "imageEntries.id",
                          "    entries.deletedFlag!=TRUE \
                           AND entryFragments.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
    error = Database_deleteByIds(databaseHandle,
                                 &n,
                                 "imageEntries",
                                 "id",
                                 DATABASE_FLAG_NONE,
                                 Array_cArray(&ids),
                                 Array_length(&ids)
                                );
  }
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  hardlink entries without fragments...");
  n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "hardlinkEntries \
                             LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                             LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.entryId \
                          ",
                          "hardlinkEntries.id",
                          "    entries.deletedFlag!=TRUE \
                           AND entryFragments.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
    error = Database_deleteByIds(databaseHandle,
                                 &n,
                                 "hardlinkEntries",
                                 "id",
                                 DATABASE_FLAG_NONE,
                                 Array_cArray(&ids),
                                 Array_length(&ids)
                                );
  }
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean entries without associated file/image/directory/link/hardlink/special entry
  printInfo("  entries without file entry...        ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                              LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND fileEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entries without image entry...       ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                             LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND imageEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entries without directory entry...   ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                             LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND directoryEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_DIRECTORY)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entries without link entry...        ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                             LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND linkEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_LINK)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entries without hardlink entry...    ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                             LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND hardlinkEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entries without special entry...     ");
  Array_clear(&ids);
  error = Database_getIds(databaseHandle,
                          &ids,
                          "entries \
                             LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                          ",
                          "entries.id",
                          "    entries.type=? \
                           AND entries.deletedFlag!=TRUE \
                           AND specialEntries.id IS NULL \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_CONST_TYPE_SPECIAL)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error == ERROR_NONE)
  {
    initProgress(Array_length(&ids));
    n = 0;
    ArraySegmentIterator arraySegmentIterator;
    ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
    {
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                     Array_segmentLength(&ids,&arraySegmentIterator)
                                    );
      }
      printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
    }
    doneProgress();

    (void)Database_flush(databaseHandle);
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean entries with wrong entity
  printInfo("  entries with wrong entity...         ");

  typedef struct EntryNode
  {
    LIST_NODE_HEADER(struct EntryNode);

    DatabaseId entryId;
    DatabaseId entityId;
  } EntryNode;

  typedef struct
  {
    LIST_HEADER(EntryNode);
  } EntryList;

  EntryList entryList;
  List_init(&entryList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get entries info to add
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(userData);
                         UNUSED_VARIABLE(valueCount);

                         DatabaseId entryId  = values[0].id;
                         DatabaseId entityId = values[1].id;

                         EntryNode *entryNode = LIST_NEW_NODE(EntryNode);
                         if (entryNode == NULL)
                         {
                           HALT_INSUFFICIENT_MEMORY();
                         }

                         entryNode->entryId  = entryId;
                         entryNode->entityId = entityId;

                         List_append(&entryList,entryNode);

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entries \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                            LEFT JOIN storages ON storages.id=entryFragments.storageId \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY     ("entries.id"),
                         DATABASE_COLUMN_KEY     ("storages.entityId"),
                       ),
                       "    entries.deletedFlag!=TRUE \
                        AND (entries.type=? OR entries.type=? OR entries.type=?)\
                        AND entryFragments.entryId=entries.id \
                        AND storages.entityId!=entries.entityId \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_CONST_TYPE_FILE),
                         DATABASE_FILTER_UINT(INDEX_CONST_TYPE_IMAGE),
                         DATABASE_FILTER_UINT(INDEX_CONST_TYPE_HARDLINK)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    List_done(&entryList);
    return error;
  }
  const EntryNode *entryNode;
  n = 0;
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    error = Database_update(databaseHandle,
                            &n,  // changedRowCount
                            "entries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY     ("entityId",entryNode->entityId),
                            ),
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryNode->entryId)
                            )
                           );

                           n++;
  }
  List_done(&entryList);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean storages without name
  printInfo("  storages without name...             ");
  n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_delete(databaseHandle,
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
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean storages with invalid state
  printInfo("  storages with invalid state...       ");
  n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    error = Database_delete(databaseHandle,
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
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  // clean entities without entries/storages
  DatabaseId entityId;

  printInfo("  entities without entries...          ");
  n = 0;
  Array_clear(&ids);
  (void)Database_getIds(databaseHandle,
                        &ids,
                        "entities \
                           LEFT JOIN entries ON entries.entityId=entities.id \
                           LEFT JOIN entriesNewest ON entriesNewest.entityId=entities.id \
                        ",
                        "entities.id",
                        "    entities.deletedFlag!=TRUE \
                         AND entities.id!=? \
                         AND entries.id IS NULL \
                         AND entriesNewest.id IS NULL \
                        ",
                        DATABASE_FILTERS
                        (
                          DATABASE_FILTER_KEY(INDEX_DEFAULT_ENTITY_DATABASE_ID)
                        ),
                        DATABASE_UNLIMITED
                       );
  initProgress(Array_length(&ids));
  n = 0;
  ARRAY_ITERATEX(&ids,entityId,error == ERROR_NONE)
  {
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      error = Database_delete(databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(entityId)
                              ),
                              DATABASE_UNLIMITED
                             );
    }
    printProgress(n);

    n++;
  }
  doneProgress();
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  printInfo("  entities without storages...         ");
  n = 0;
  Array_clear(&ids);
  (void)Database_getIds(databaseHandle,
                        &ids,
                        "entities \
                           LEFT JOIN storages ON storages.entityId=entities.id \
                        ",
                        "entities.id",
                        "    entities.deletedFlag!=TRUE \
                         AND entities.id!=? \
                         AND storages.id IS NULL \
                        ",
                        DATABASE_FILTERS
                        (
                          DATABASE_FILTER_KEY(INDEX_DEFAULT_ENTITY_DATABASE_ID)
                        ),
                        DATABASE_UNLIMITED
                       );
  initProgress(Array_length(&ids));
  n = 0;
  ARRAY_ITERATEX(&ids,entityId,error == ERROR_NONE)
  {
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      error = Database_delete(databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(entityId)
                              ),
                              DATABASE_UNLIMITED
                             );
    }
    printProgress(n);

    n++;
  }
  doneProgress();
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      // clean FTS entries without entry
      printInfo("  FTS entries without entry...         ");
      n = 0;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        Array_clear(&ids);
        (void)Database_getIds(databaseHandle,
                              &ids,
                              "FTS_entries \
                                 LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                              ",
                              "FTS_entries.rowid",
                              "entries.id IS NULL",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "FTS_entries",
                                     "rowid",
                                     DATABASE_FLAG_NONE,
                                     Array_cArray(&ids),
                                     Array_length(&ids)
                                    );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      totalCount += n;

      // clean FTS storages without storage
      printInfo("  FTS storages without storage...      ");
      n = 0;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        Array_clear(&ids);
        (void)Database_getIds(databaseHandle,
                              &ids,
                              "FTS_storages \
                                LEFT JOIN storages ON storages.id=FTS_storages.storageId \
                              ",
                              "FTS_storages.rowid",
                              "storages.id IS NULL",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "FTS_storages",
                                     "rowid",
                                     DATABASE_FLAG_NONE,
                                     Array_cArray(&ids),
                                     Array_length(&ids)
                                    );
      }
      (void)Database_flush(databaseHandle);
      if (error == ERROR_NONE)
      {
        printInfo("%lu\n",n);
      }
      else
      {
        printInfo("FAIL (error: %s",Error_getText(error));
      }
      totalCount += n;
      break;
    case DATABASE_TYPE_MARIADB:
      // nothing to do (use views)
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // clean FTS entries without entry
      printInfo("  FTS entries without entry...         ");
      n = 0;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        Array_clear(&ids);
        (void)Database_getIds(databaseHandle,
                              &ids,
                              "FTS_entries \
                                 LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                              ",
                              "FTS_entries.id",
                              "entries.id IS NULL",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "FTS_entries",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     Array_cArray(&ids),
                                     Array_length(&ids)
                                    );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      totalCount += n;

      // clean FTS storages without entry
      printInfo("  FTS storages without storage...      ");
      n = 0;
      DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
      {
        Array_clear(&ids);
        (void)Database_getIds(databaseHandle,
                              &ids,
                              "FTS_storages \
                                 LEFT JOIN storages ON storages.id=FTS_storages.storageId \
                              ",
                              "FTS_storages.id",
                              "storages.id IS NULL",
                              DATABASE_FILTERS
                              (
                              ),
                              DATABASE_UNLIMITED
                             );
        error = Database_deleteByIds(databaseHandle,
                                     &n,
                                     "FTS_storages",
                                     "id",
                                     DATABASE_FLAG_NONE,
                                     Array_cArray(&ids),
                                     Array_length(&ids)
                                    );
      }
      (void)Database_flush(databaseHandle);
      printInfo("%lu\n",n);
      totalCount += n;
      break;
  }

  // clean newest entries without entry
  printInfo("  newest entries without entry...      ");
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    Array_clear(&ids);
    (void)Database_getIds(databaseHandle,
                          &ids,
                          "entriesNewest \
                             LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                          ",
                          "entriesNewest.id",
                          "entries.id IS NULL",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
    error = Database_deleteByIds(databaseHandle,
                                 &n,
                                 "entriesNewest",
                                 "id",
                                 DATABASE_FLAG_NONE,
                                 Array_cArray(&ids),
                                 Array_length(&ids)
                                );
  }
  (void)Database_flush(databaseHandle);
  if (error != ERROR_NONE)
  {
    printInfo("FAIL\n");
    printError(_("clean orphaned fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(storageName);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;

//TODO: obsolete, remove
#if 0
  // clean *Entries without entry
  printInfo("  orphaned entries...                  ");
  n = 0;
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
                     DATABASE_FLAG_FETCH_ALL,
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
                     DATABASE_FLAG_FETCH_ALL,
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
                     DATABASE_FLAG_FETCH_ALL,
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
                     DATABASE_FLAG_FETCH_ALL,
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
                     DATABASE_FLAG_FETCH_ALL,
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
                     DATABASE_FLAG_FETCH_ALL,
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
  totalCount += n;
#endif

  printInfo("Total %zu orphaned entries cleaned\n",totalCount);

  // free resources
  Array_done(&ids);
  String_delete(storageName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanDuplicateEntries
* Purpose: purge duplicate entries
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanDuplicateEntries(DatabaseHandle *databaseHandle)
{
  Errors error;

  // init variables
  size_t totalCount = 0;

  printInfo("Clean duplicates:\n");

  printInfo("  storages...                          ");
  Array ids;
  Array_init(&ids,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  String name = String_new();
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
                           Array_append(&ids,&storageId);
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
    printError(_("clean duplicates fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(name);
    return error;
  }
  size_t n = 0;
  DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
  {
    DatabaseId id;
    ARRAY_ITERATEX(&ids,id,error == ERROR_NONE)
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
                                DATABASE_FILTER_KEY  (id)
                              )
                             );
      if (error == ERROR_NONE)
      {
        n++;
      }
    }
  }
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("clean duplicates fail (error: %s)!"),Error_getText(error));
    Array_done(&ids);
    String_delete(name);
    return error;
  }
  printInfo("%lu\n",n);
  totalCount += n;
  Array_done(&ids);

  printInfo("Total %zu duplicate entries removed\n",n);

  // free resources
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeStorages
* Purpose: purge storages
* Input  : databaseHandle - database handle
*          storageIds     - storages to purge
*          isPrintInfo    - TRUE to print info/progress
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeStorages(DatabaseHandle *databaseHandle, Array *storageIds, bool isPrintInfo)
{
  Errors error;

  Array entryIds;
  Array_init(&entryIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error = ERROR_NONE;
  DatabaseId storageId;
  ARRAY_ITERATEX(storageIds,storageId,error == ERROR_NONE)
  {
    DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
    {
      if (isPrintInfo) printInfo("  %10"PRIi64"...",storageId);

      // get entity id, job UUID
      DatabaseId databaseId;
      if (error == ERROR_NONE)
      {
        error = Database_getId(databaseHandle,
                               &databaseId,
                               "storages",
                               "entityId",
                               "id=?",
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_KEY(storageId)
                               )
                              );
      }
      IndexId entityId = INDEX_ID_ENTITY(databaseId);
      StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
      if (error == ERROR_NONE)
      {
        error = Database_getString(databaseHandle,
                                   jobUUID,
                                   "entities",
                                   "jobUUID",
                                   "id=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(databaseId)
                                   )
                                  );
      }

      // collect file/image/hardlink entries to delete
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
                                 ),
                                 DATABASE_UNLIMITED
                                );
      }

      // collect directory/link/special entries to delete
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
                                 ),
                                 DATABASE_UNLIMITED
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
                                 ),
                                 DATABASE_UNLIMITED
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
                                 ),
                                DATABASE_UNLIMITED
                                );
      }

      // delete fragments
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
                                DATABASE_UNLIMITED
                               );
      }

      // purge entries FTS
      initProgress(2*Array_length(&entryIds));
      size_t n = 0;
      switch (Database_getType(databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          if (error == ERROR_NONE)
          {
            DatabaseId entryId;
            ARRAY_ITERATEX(&entryIds,entryId,error == ERROR_NONE)
            {
              if (!Database_existsValue(databaseHandle,
                                        "entryFragments",
                                        DATABASE_FLAG_NONE,
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
                                        DATABASE_UNLIMITED
                                       );
              }
              if (isPrintInfo) printProgress(0*Array_length(&entryIds)+n);
            }
          }
          break;
        case DATABASE_TYPE_MARIADB:
          // nothing to do (use views)
          break;
        case DATABASE_TYPE_POSTGRESQL:
          if (error == ERROR_NONE)
          {
            DatabaseId entryId;
            ARRAY_ITERATEX(&entryIds,entryId,error == ERROR_NONE)
            {
              if (!Database_existsValue(databaseHandle,
                                        "entryFragments",
                                        DATABASE_FLAG_NONE,
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
                                        "entryId=?",
                                        DATABASE_FILTERS
                                        (
                                          DATABASE_FILTER_KEY(entryId)
                                        ),
                                        DATABASE_UNLIMITED
                                       );
              }
              if (isPrintInfo) printProgress(0*Array_length(&entryIds)+n);
            }
          }
          break;
      }
      doneProgress();

      // delete directory/link/special entries
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
                                DATABASE_UNLIMITED
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
                                DATABASE_UNLIMITED
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
                                DATABASE_UNLIMITED
                               );
      }

      // delete storage FTS
      switch (Database_getType(databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          // purge FTS storages
          if (error == ERROR_NONE)
          {
            error = Database_delete(databaseHandle,
                                    NULL,  // changedRowCount,
                                    "FTS_storages",
                                    DATABASE_FLAG_NONE,
                                    "storageId MATCH ?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(storageId)
                                    ),
                                    DATABASE_UNLIMITED
                                   );
          }
          break;
        case DATABASE_TYPE_MARIADB:
          // nothing to do (use views)
          break;
        case DATABASE_TYPE_POSTGRESQL:
          // purge FTS storages
          if (error == ERROR_NONE)
          {
            error = Database_delete(databaseHandle,
                                    NULL,  // changedRowCount,
                                    "FTS_storages",
                                    DATABASE_FLAG_NONE,
                                    "storageId=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(storageId)
                                    ),
                                    DATABASE_UNLIMITED
                                   );
          }
          break;
      }

      // delete storage
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
                                DATABASE_UNLIMITED
                               );
      }

      // purge entries
      if (error == ERROR_NONE)
      {
        initProgress(2*Array_length(&entryIds));
        size_t     n = 0;
        DatabaseId entryId;
        ARRAY_ITERATEX(&entryIds,entryId,error == ERROR_NONE)
        {
          if (!Database_existsValue(databaseHandle,
                                    "entryFragments",
                                    DATABASE_FLAG_NONE,
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
                                    DATABASE_UNLIMITED
                                   );
          }
          if (isPrintInfo) printProgress(1*Array_length(&entryIds)+n);
        }
        doneProgress();
      }

      // prune entity
      if (error == ERROR_NONE)
      {
        if (    !INDEX_ID_IS_NONE(entityId)
             && (INDEX_DATABASE_ID(entityId) != INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
             && !Database_existsValue(databaseHandle,
                                      "entries",
                                      DATABASE_FLAG_NONE,
                                      "id",
                                      "entityId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entityId))
                                      )
                                     )
             && !Database_existsValue(databaseHandle,
                                      "entriesNewest",
                                      DATABASE_FLAG_NONE,
                                      "id",
                                      "entityId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entityId))
                                      )
                                     )
          )
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "entities",
                                  DATABASE_FLAG_NONE,
                                  "id=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entityId))
                                  ),
                                  DATABASE_UNLIMITED
                                 );
        }
      }

      // prune UUID
      if (error == ERROR_NONE)
      {
        if (   !String_isEmpty(jobUUID)
            && !Database_existsValue(databaseHandle,
                                     "entities",
                                     DATABASE_FLAG_NONE,
                                     "id",
                                     "jobUUID=?",
                                     DATABASE_FILTERS
                                     (
                                       DATABASE_FILTER_STRING(jobUUID)
                                     )
                                    )
           )
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "uuids",
                                  DATABASE_FLAG_NONE,
                                  "jobUUID=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(jobUUID)
                                  ),
                                  DATABASE_UNLIMITED
                                 );
        }
      }

      if (error == ERROR_NONE)
      {
        if (isPrintInfo) printInfo("OK\n");
      }
      else
      {
        if (isPrintInfo) printInfo("FAIL!\n");
        DATABASE_TRANSACTION_ABORT(databaseHandle);
        break;
      }

      n++;
    }
  }
  if (error != ERROR_NONE)
  {
    Array_done(&entryIds);
    return error;
  }

  // free resources
  Array_done(&entryIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeDeletedStorages
* Purpose: purge deleted storages
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeDeletedStorages(DatabaseHandle *databaseHandle)
{
  Errors error;

  // init variables

  printInfo("Purge deleted storages:\n");

  Array storageIds;
  Array_init(&storageIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error = Database_getIds(databaseHandle,
                          &storageIds,
                          "storages",
                          "id",
                          "deletedFlag=TRUE",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    printError(_("purge deleted storages fail (error: %s)!"),Error_getText(error));
    Array_done(&storageIds);
    return error;
  }

  error = purgeStorages(databaseHandle,&storageIds,TRUE);
  if (error != ERROR_NONE)
  {
    printError(_("purge deleted storages fail (error: %s)!"),Error_getText(error));
    Array_done(&storageIds);
    return error;
  }
  (void)Database_flush(databaseHandle);

  // free resources
  Array_done(&storageIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeStoragesWithError
* Purpose: purge storages with error
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeStoragesWithError(DatabaseHandle *databaseHandle)
{
  Errors error;

  // init variables

  printInfo("Purge storages with error:\n");

  Array storageIds;
  Array_init(&storageIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error = Database_getIds(databaseHandle,
                          &storageIds,
                          "storages",
                          "id",
                          "state=? AND deletedFlag!=TRUE",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT  (INDEX_CONST_STATE_ERROR),
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    printError(_("purge storages with error fail (error: %s)!"),Error_getText(error));
    Array_done(&storageIds);
    return error;
  }

  error = purgeStorages(databaseHandle,&storageIds,TRUE);
  if (error != ERROR_NONE)
  {
    printError(_("purge storages with error fail (error: %s)!"),Error_getText(error));
    Array_done(&storageIds);
    return error;
  }
  (void)Database_flush(databaseHandle);

  // free resources
  Array_done(&storageIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeEntities
* Purpose: purge entities
* Input  : databaseHandle - database handle
*          entityIds      - entities to purge
*          isPrintInfo    - TRUE to print info/progress
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeEntities(DatabaseHandle *databaseHandle, Array *entityIds, bool isPrintInfo)
{
  const size_t INCREMENT_SIZE = 4096;

  Errors error;

  Array storageIds;
  Array_init(&storageIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error = ERROR_NONE;
  DatabaseId entityId;
  ARRAY_ITERATEX(entityIds,entityId,error == ERROR_NONE)
  {
    if (   (entityId != DATABASE_ID_NONE)
        && (entityId != INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
       )
    {
      if (isPrintInfo) printInfo("  %10"PRIi64"...",entityId);

      // get job UUID
      StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
      if (error == ERROR_NONE)
      {
        error = Database_getString(databaseHandle,
                                   jobUUID,
                                   "entities",
                                   "jobUUID",
                                   "id=?",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(entityId)
                                   )
                                  );
      }

      // collect storages to delete
      Array_clear(&storageIds);
      if (error == ERROR_NONE)
      {
         error = Database_getIds(databaseHandle,
                                 &storageIds,
                                 "storages",
                                 "id",
                                 "entityId=?",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_KEY(entityId)
                                 ),
                                 DATABASE_UNLIMITED
                                );
      }

      // delete storages of entity
      if (error == ERROR_NONE)
      {
        error = purgeStorages(databaseHandle,&storageIds,FALSE);
      }

      // delete entries of entity
      if (error == ERROR_NONE)
      {
        Array ids;
        Array_init(&ids,sizeof(DatabaseId),INCREMENT_SIZE,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
        (void)Database_getIds(databaseHandle,
                              &ids,
                              "entries",
                              "id",
                              "entityId=? \
                              ",
                              DATABASE_FILTERS
                              (
                                 DATABASE_FILTER_KEY(entityId)
                              ),
                              DATABASE_UNLIMITED
                             );

        initProgress(Array_length(&ids));
        DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
        {
          ArraySegmentIterator arraySegmentIterator;
          ARRAY_SEGMENT(&ids,arraySegmentIterator,INCREMENT_SIZE)
          {
            error = Database_deleteByIds(databaseHandle,
                                         NULL,  // changedRowCount
                                         "entries",
                                         "id",
                                         DATABASE_FLAG_NONE,
                                         (DatabaseId*)Array_cArraySegment(&ids,&arraySegmentIterator),
                                         Array_segmentLength(&ids,&arraySegmentIterator)
                                        );
            printProgress(Array_segmentOffset(&ids,&arraySegmentIterator)+Array_segmentLength(&ids,&arraySegmentIterator));
          }
        }
        doneProgress();

        (void)Database_flush(databaseHandle);

        Array_done(&ids);
      }

      // delete skipped entries of entity
      if (error == ERROR_NONE)
      {
        DATABASE_TRANSACTION_DO(databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount
                                  "skippedEntries",
                                  DATABASE_FLAG_NONE,
                                  "entityId=? \
                                  ",
                                  DATABASE_FILTERS
                                  (
                                     DATABASE_FILTER_KEY(entityId)
                                  ),
                                  DATABASE_UNLIMITED
                                 );
        }
      }

      // delete entity
      if (error == ERROR_NONE)
      {
        if (    (entityId != DATABASE_ID_NONE)
             && (entityId != INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
             && !Database_existsValue(databaseHandle,
                                      "entities",
                                      DATABASE_FLAG_NONE,
                                      "id",
                                      "entityId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(entityId)
                                      )
                                     )
          )
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "entities",
                                  DATABASE_FLAG_NONE,
                                  "id=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(entityId)
                                  ),
                                  DATABASE_UNLIMITED
                                 );
        }
      }

      // prune UUID
      if (error == ERROR_NONE)
      {
        if (   !String_isEmpty(jobUUID)
            && !Database_existsValue(databaseHandle,
                                     "entities",
                                     DATABASE_FLAG_NONE,
                                     "id",
                                     "jobUUID=?",
                                     DATABASE_FILTERS
                                     (
                                       DATABASE_FILTER_STRING(jobUUID)
                                     )
                                    )
           )
        {
          error = Database_delete(databaseHandle,
                                  NULL,  // changedRowCount,
                                  "uuids",
                                  DATABASE_FLAG_NONE,
                                  "jobUUID=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(jobUUID)
                                  ),
                                  DATABASE_UNLIMITED
                                 );
        }
      }

      if (error == ERROR_NONE)
      {
        if (isPrintInfo) printInfo("OK\n");
      }
      else
      {
        if (isPrintInfo) printInfo("FAIL!\n");
      }
    }
  }
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    return error;
  }

  // free resources
  Array_done(&storageIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeDeletedEntities
* Purpose: purge deleted entities
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeDeletedEntities(DatabaseHandle *databaseHandle)
{
  Errors error;

  // init variables

  printInfo("Purge deleted entities:\n");

  Array entityIds;
  Array_init(&entityIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error = Database_getIds(databaseHandle,
                          &entityIds,
                          "entities",
                          "id",
                          "deletedFlag=TRUE",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    printError(_("purge deleted entities fail (error: %s)!"),Error_getText(error));
    Array_done(&entityIds);
    return error;
  }

  error = purgeEntities(databaseHandle,&entityIds,TRUE);
  if (error != ERROR_NONE)
  {
    printError(_("purge deleted fail (error: %s)!"),Error_getText(error));
    Array_done(&entityIds);
    return error;
  }
  (void)Database_flush(databaseHandle);

  // free resources
  Array_done(&entityIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : vacuum
* Purpose: vacuum database
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors vacuum(DatabaseHandle *databaseHandle, const char *toFileName)
{
  printInfo("Vacuum...");
  const char * const *tableNames    = NULL;
  uint               tableNameCount = 0;
  switch (Database_getType(databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      tableNames     = NULL;
      tableNameCount = 0;
      break;
    case DATABASE_TYPE_MARIADB:
      tableNames     = INDEX_DEFINITION_TABLE_NAMES_MARIADB;
      tableNameCount = INDEX_DEFINITION_TABLE_NAME_COUNT_MARIADB;
      break;
    case DATABASE_TYPE_POSTGRESQL:
      tableNames     = INDEX_DEFINITION_TABLE_NAMES_POSTGRESQL;
      tableNameCount = INDEX_DEFINITION_TABLE_NAME_COUNT_POSTGRESQL;
      break;
  }
  Errors error = Database_vacuum(databaseHandle,
                                 tableNames,
                                 tableNameCount,
                                 toFileName,
                                 forceFlag
                                );
  if (error != ERROR_NONE)
  {
    printInfo("FAIL!\n");
    printError(_("vacuum fail (error: %s)!"),Error_getText(error));
    return error;
  }
  printInfo("OK\n");

  return ERROR_NONE;
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
    UNUSED_RESULT(fwrite(&ch,sizeof(char),1,stdout));
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
    if (printRowData->widths == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
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

      case DATABASE_DATATYPE_BOOL:        n = stringFormatLengthCodepointsUTF8("%s",values[i].b ? "TRUE" : "FALSE"); break;
      case DATABASE_DATATYPE_INT:         n = stringFormatLengthCodepointsUTF8("%d",values[i].i); break;
      case DATABASE_DATATYPE_INT64:       n = stringFormatLengthCodepointsUTF8("%"PRIi64,values[i].i64); break;
      case DATABASE_DATATYPE_UINT:        n = stringFormatLengthCodepointsUTF8("%u",values[i].u); break;
      case DATABASE_DATATYPE_UINT64:      n = stringFormatLengthCodepointsUTF8("%"PRIu64,values[i].u64); break;
      case DATABASE_DATATYPE_DOUBLE:      n = stringFormatLengthCodepointsUTF8("%lf",values[i].d); break;
      case DATABASE_DATATYPE_ENUM:        n = stringFormatLengthCodepointsUTF8("%u",values[i].u); break;
      case DATABASE_DATATYPE_DATETIME:    n = stringLengthCodepointsUTF8(Misc_formatDateTimeCString(buffer,sizeof(buffer),values[i].dateTime,TIME_TYPE_LOCAL,NULL)); break;
      case DATABASE_DATATYPE_STRING:      n = String_lengthCodepointsUTF8(values[i].string); break;
      case DATABASE_DATATYPE_CSTRING:     n = stringLengthCodepointsUTF8(values[i].s); break;
      case DATABASE_DATATYPE_BLOB:        break;
      default:                            break;
    }
    printRowData->widths[i] = MAX(printRowData->widths[i],n);
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

  UNUSED_VARIABLE(userData);

  if ((printRowData != NULL) && printRowData->showHeaderFlag && !printRowData->printedHeaderFlag)
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
      case DATABASE_DATATYPE_ENUM:        s = stringFormat(buffer,sizeof(buffer),"%u",values[i].u); break;
      case DATABASE_DATATYPE_DATETIME:    s = Misc_formatDateTimeCString(buffer,sizeof(buffer),values[i].dateTime,TIME_TYPE_LOCAL,NULL); break;
      case DATABASE_DATATYPE_STRING:      s = String_cString(values[i].string); break;
      case DATABASE_DATATYPE_CSTRING:     s = values[i].s; break;
      case DATABASE_DATATYPE_BLOB:        break;
      default:                            break;
    }
    if (s != NULL)
    {
      n = stringLengthCodepointsUTF8(s);
      UNUSED_RESULT(fwrite(s,1,n,stdout));
      if ((printRowData != NULL) && printRowData->showHeaderFlag)
      {
        assert(printRowData->widths[i] >= n);
        printChars(' ',printRowData->widths[i]-n);
      }
      putc(' ',stdout);
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
    printError(_("get meta data fail (error: %s)!"),Error_getText(error));
    return;
  }

  printf("Entities:");
  if (verboseFlag && !quietFlag)
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
      printError(_("get entities data fail (error: %s)!"),Error_getText(error));
      return;
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
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Full            : %u\n",n);

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
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Incremental     : %u\n",n);

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
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Differential    : %u\n",n);

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
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Continuous      : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "lockedCount>0",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                           );
  if (error != ERROR_NONE)
  {
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Locked          : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "entities",
                           "COUNT(id)",
                           "deletedFlag=TRUE",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                           );
  if (error != ERROR_NONE)
  {
    printError(_("get entities data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Deleted         : %u\n",n);

  printf("Storages:");
  if (verboseFlag && !quietFlag)
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
      printError(_("get storage data fail (error: %s)!"),Error_getText(error));
      return;
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
    printError(_("get storage data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get storage data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get storage data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Error           : %u\n",n);

  error = Database_getUInt(databaseHandle,
                           &n,
                           "storages",
                           "COUNT(id)",
                           "deletedFlag=TRUE",
                           DATABASE_FILTERS
                           (
                           ),
                           NULL  // group
                          );
  if (error != ERROR_NONE)
  {
    printError(_("get storage data fail (error: %s)!"),Error_getText(error));
    return;
  }
  printf("  Deleted         : %u\n",n);

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("Entries: %u, %.1lf %s (%"PRIu64" bytes)\n",
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
  }

  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         printf("Newest entries: %u, %.1lf %s (%"PRIu64" bytes)\n",
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
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
    printError(_("get entries data fail (error: %s)!"),Error_getText(error));
    return;
  }
}

/***********************************************************************\
* Name   : printUUIDsInfo
* Purpose: print UUIDs index info
* Input  : databaseHandle - database handle
*          uuidIds        - array with UUID ids or empty array
*          uuids          - array with UUIDs or empty array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printUUIDsInfo(DatabaseHandle *databaseHandle, const Array uuidIds, const Array uuids)
{
  String     uuidIdsString = String_new();
  String     uuidsString   = String_new();
  DatabaseId uuidId;
  ARRAY_ITERATE(&uuidIds,uuidId)
  {
    if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
    String_appendFormat(uuidIdsString,"%"PRIi64,uuidId);
  }
  char s[MISC_UUID_STRING_LENGTH];
  ARRAY_ITERATE(&uuids,s)
  {
    StaticString (uuid,MISC_UUID_STRING_LENGTH);
    String_setBuffer(uuid,s,MISC_UUID_STRING_LENGTH);
    if (!String_isEmpty(uuidsString)) String_appendChar(uuidsString,',');
    String_appendFormat(uuidsString,"'%S'",uuid);
  }

  Errors error;

  printf("UUIDs:\n");
  char filterString[1024];
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
                       DATABASE_FLAG_FETCH_ALL,
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
    printError(_("get UUID data fail (error: %s)!"),Error_getText(error));
    return;
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

  String     entityIdsString = String_new();
  DatabaseId entityId;
  ARRAY_ITERATE(&entityIds,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,entityId);
  }

  Errors error;

  printf("Entities:\n");
  char filterString[1024];
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 28);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         DatabaseId entityId            = values[0].id;
                         uint       type                = values[1].u;
                         uint64     createdDateTime     = values[4].dateTime;
                         ulong      totalEntryCount     = values[5].u;
                         uint64     totalEntrySize      = values[6].u64;
                         ulong      totalFileCount      = values[7].u;
                         uint64     totalFileSize       = values[8].u64;
                         ulong      totalImageCount     = values[9].u;
                         uint64     totalImageSize      = values[10].u64;
                         ulong      totalDirectoryCount = values[11].u;
                         ulong      totalLinkCount      = values[12].u;
                         ulong      totalhardlinkCount  = values[13].u;
                         uint64     totalHardlinkSize   = values[14].u64;
                         ulong      totalSpecialCount   = values[15].u;

                         DatabaseId uuidId              = values[27].id;

                         char buffer[64];
                         printf("  Id             : %"PRIi64"\n",entityId);
                         printf("    Type         : %s\n",
                                (type <= CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS)
                                  ? TYPE_NAMES[type]
                                  : stringFormat(buffer,sizeof(buffer),"unknown (%d)",type)
                               );
                         printf("    Job UUID     : %s\n",String_cString(values[ 2].string));
                         printf("    Entity UUID  : %s\n",String_cString(values[ 3].string));
                         printf("    Created      : %s\n",(createdDateTime > 0LL) ? Misc_formatDateTimeCString(buffer,sizeof(buffer),createdDateTime,TIME_TYPE_LOCAL,NULL) : "-");
                         printf("\n");
                         printf("    Total entries: %lu, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                         printf("\n");
                         printf("    Files        : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                         printf("    Images       : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                         printf("    Directories  : %lu\n",totalDirectoryCount);
                         printf("    Links        : %lu\n",totalLinkCount);
                         printf("    Hardlinks    : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalhardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                         printf("    Special      : %lu\n",totalSpecialCount);
                         printf("\n");
                         printf("    UUID id      : %"PRIi64"\n",uuidId);

                         uint       i       = 0;
                         const char *prefix = "    Storage ids  : ";
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
                                      "id",
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
                       DATABASE_FLAG_FETCH_ALL,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_KEY     ("id"),
                         DATABASE_COLUMN_UINT    ("type"),
                         DATABASE_COLUMN_STRING  ("jobUUID"),
                         DATABASE_COLUMN_STRING  ("scheduleUUID"),
                         DATABASE_COLUMN_DATETIME("created"),

                         DATABASE_COLUMN_UINT    ("totalEntryCount"),
                         DATABASE_COLUMN_UINT64  ("totalEntrySize"),

                         DATABASE_COLUMN_UINT    ("totalFileCount"),
                         DATABASE_COLUMN_UINT64  ("totalFileSize"),
                         DATABASE_COLUMN_UINT    ("totalImageCount"),
                         DATABASE_COLUMN_UINT64  ("totalImageSize"),
                         DATABASE_COLUMN_UINT    ("totalDirectoryCount"),
                         DATABASE_COLUMN_UINT    ("totalLinkCount"),
                         DATABASE_COLUMN_UINT    ("totalHardlinkCount"),
                         DATABASE_COLUMN_UINT64  ("totalHardlinkSize"),
                         DATABASE_COLUMN_UINT    ("totalSpecialCount"),

                         DATABASE_COLUMN_UINT    ("totalEntryCountNewest"),
                         DATABASE_COLUMN_UINT64  ("totalEntrySizeNewest"),

                         DATABASE_COLUMN_UINT    ("totalFileCountNewest"),
                         DATABASE_COLUMN_UINT64  ("totalFileSizeNewest"),
                         DATABASE_COLUMN_UINT    ("totalImageCountNewest"),
                         DATABASE_COLUMN_UINT64  ("totalImageSizeNewest"),
                         DATABASE_COLUMN_UINT    ("totalDirectoryCountNewest"),
                         DATABASE_COLUMN_UINT    ("totalLinkCountNewest"),
                         DATABASE_COLUMN_UINT    ("totalHardlinkCountNewest"),
                         DATABASE_COLUMN_UINT64  ("totalHardlinkSizeNewest"),
                         DATABASE_COLUMN_UINT    ("totalSpecialCountNewest"),

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
                       "id ASC",
                       0LL,
                       DATABASE_UNLIMITED
                      );
  if (error != ERROR_NONE)
  {
    printError(_("get entity data fail (error: %s)!"),Error_getText(error));
    return;
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

  Errors error;

  String     storageIdsString = String_new();
  DatabaseId storageId;
  ARRAY_ITERATE(&storageIds,storageId)
  {
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_appendFormat(storageIdsString,"%"PRIi64,storageId);
  }

  String ftsMatchString = getFTSMatchString(String_new(),databaseHandle,"FTS_storages","name",name);

  printf("%s:\n",lostFlag ? "Lost storages" : "Storages");
  char filterString[1024];
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 1);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         DatabaseId storageId = values[0].id;
                         char       filterString[1024];
                         error = Database_get(databaseHandle,
                                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                              {
                                                assert(values != NULL);
                                                assert(valueCount == 36);

                                                UNUSED_VARIABLE(valueCount);
                                                UNUSED_VARIABLE(userData);

                                                DatabaseId id                  = values[ 0].id;
                                                DatabaseId uuidId              = values[ 1].id;
                                                DatabaseId entityId            = values[ 2].id;
                                                String     jobUUID             = values[ 3].string;
                                                String     entityUUID          = values[ 4].string;
                                                String     name                = values[ 5].string;
                                                uint64     createdDateTime     = values[ 6].dateTime;
                                                String     hostname            = values[ 7].string;
                                                String     userName            = values[ 8].string;
                                                String     comment             = values[ 9].string;
                                                uint       state               = values[10].u;
                                                uint       mode                = values[11].u;
                                                uint64     lastCheckedDateTime = values[12].dateTime;
                                                String     message             = values[13].string;
                                                ulong      totalEntryCount     = values[14].u;
                                                uint64     totalEntrySize      = values[15].u64;
                                                ulong      totalFileCount      = values[16].u;
                                                uint64     totalFileSize       = values[17].u64;
                                                ulong      totalImageCount     = values[18].u;
                                                uint64     totalImageSize      = values[19].u64;
                                                ulong      totalDirectoryCount = values[20].u;
                                                ulong      totalLinkCount      = values[21].u64;
                                                ulong      totalHardlinkCount  = values[22].u;
                                                uint64     totalHardlinkSize   = values[23].u64;
                                                ulong       totalSpecialCount   = values[24].u;

                                                char buffer[64];
                                                printf("  Id             : %"PRIi64"\n",id);
                                                printf("    Name         : %s\n",String_cString(name));
                                                printf("    Created      : %s\n",(createdDateTime > 0LL) ? Misc_formatDateTimeCString(buffer,sizeof(buffer),createdDateTime,TIME_TYPE_LOCAL,NULL) : "-");
                                                printf("    Host name    : %s\n",String_cString(hostname));
                                                printf("    User name    : %s\n",String_cString(userName));
                                                printf("    Comment      : %s\n",String_cString(comment));
                                                printf("    State        : %s\n",
                                                       (state <= INDEX_CONST_STATE_ERROR)
                                                         ? STATE_TEXT[state]
                                                         : stringFormat(buffer,sizeof(buffer),"unknown (%d)",state)
                                                      );
                                                printf("    Mode         : %s\n",
                                                       (mode <= INDEX_CONST_MODE_AUTO)
                                                         ? MODE_TEXT[mode]
                                                         : stringFormat(buffer,sizeof(buffer),"unknown (%d)",mode)
                                                      );
                                                printf("    Last checked : %s\n",(lastCheckedDateTime > 0LL) ? Misc_formatDateTimeCString(buffer,sizeof(buffer),lastCheckedDateTime,TIME_TYPE_LOCAL,NULL) : "-");
                                                printf("    Error message: %s\n",String_cString(message));
                                                printf("\n");
                                                printf("    Total entries: %lu, %.1lf %s (%"PRIu64" bytes)\n",totalEntryCount,getByteSize(totalEntrySize),getByteUnitShort(totalEntrySize),totalEntrySize);
                                                printf("\n");
                                                printf("    Files        : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalFileCount,getByteSize(totalFileSize),getByteUnitShort(totalFileSize),totalFileSize);
                                                printf("    Images       : %lu, %.1lf %s (%"PRIu64" bytes)\n",totalImageCount,getByteSize(totalImageSize),getByteUnitShort(totalImageSize),totalImageSize);
                                                printf("    Directories  : %lu\n",totalDirectoryCount);
                                                printf("    Links        : %lu\n",totalLinkCount);
                                                printf("    Hardlinks    : %lu, %.1lf%s (%"PRIu64" bytes)\n",totalHardlinkCount,getByteSize(totalHardlinkSize),getByteUnitShort(totalHardlinkSize),totalHardlinkSize);
                                                printf("    Special      : %lu\n",totalSpecialCount);
                                                printf("\n");
                                                printf("    UUID id      : %"PRIi64"\n",uuidId);
                                                printf("    Entity id    : %"PRIi64"\n",entityId);
                                                printf("    Job UUID     : %s\n",String_cString(jobUUID));
                                                printf("    Entity UUID  : %s\n",String_cString(entityUUID));

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
                                                            AND (? OR storages.id IN (SELECT storageId FROM FTS_storages WHERE %s)) \
                                                           ",
                                                           !String_isEmpty(ftsMatchString) ? String_cString(ftsMatchString) : "TRUE"
                                                          ),
                                              DATABASE_FILTERS
                                              (
                                                DATABASE_FILTER_KEY  (!lostFlag ? storageId : DATABASE_ID_NONE),
                                                DATABASE_FILTER_BOOL (String_isEmpty(name))
                                              ),
                                              NULL,  // groupBy
                                              "storages.id ASC",
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
                       DATABASE_FLAG_FETCH_ALL,
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
    printError(_("get storage data fail (error: %s)!"),Error_getText(error));
    return;
  }

  // free resources
  String_delete(ftsMatchString);
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

  Errors error;

  String     entityIdsString = String_new();
  DatabaseId entityId;
  ARRAY_ITERATE(&entityIds,entityId)
  {
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,entityId);
  }

  String ftsMatchString = getFTSMatchString(String_new(),databaseHandle,"FTS_entries","name",name);

  printf("%s:\n",lostFlag ? "Lost entries" : "Entries");
  char filterString[1024];
  error = Database_get(databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(valueCount);
                         UNUSED_VARIABLE(userData);

                         DatabaseId entityId = values[0].id;
                         DatabaseId uuidId   = values[1].id;

                         bool entityOutputFlag = FALSE;
                         char buffer[64];
                         char filterString[1024];
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
                                                printf("      Type           : %s\n",
                                                       (type <= INDEX_CONST_TYPE_HISTORY)
                                                         ? TYPE_TEXT[type]
                                                         : stringFormat(buffer,sizeof(buffer),"unknown (%d)",type)
                                                      );
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
                                                            AND (? OR entries.id IN (SELECT entryId FROM FTS_entries WHERE %s)) \
                                                           ",
                                                           !String_isEmpty(ftsMatchString) ? String_cString(ftsMatchString) : "TRUE"
                                                          ),
                                              DATABASE_FILTERS
                                              (
                                                DATABASE_FILTER_KEY  (!lostFlag ? entityId : DATABASE_ID_NONE),
                                                DATABASE_FILTER_BOOL (entryType == INDEX_CONST_TYPE_ANY),
                                                DATABASE_FILTER_UINT (entryType),
                                                DATABASE_FILTER_BOOL (String_isEmpty(name))
                                              ),
                                              NULL,  // groupBy
                                              "entries.id ASC",
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
                       DATABASE_FLAG_FETCH_ALL,
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
    printError(_("get entity data fail (error: %s)!"),Error_getText(error));
    return;
  }

  // free resources
  String_delete(ftsMatchString);
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
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initAll(void)
{
  Errors error;

  // initialize modules
  error = Common_initAll();
  if (error != ERROR_NONE)
  {
    printError("%s",Error_getText(error));
    return error;
  }

  error = Thread_initAll();
  if (error != ERROR_NONE)
  {
    Common_doneAll();
    printError("%s",Error_getText(error));
    return error;
  }

  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    Thread_doneAll();
    Common_doneAll();
    printError("%s",Error_getText(error));
    return error;
  }

  return ERROR_NONE;
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
  Common_doneAll();
}

int main(int argc, const char *argv[])
{
  const uint MAX_LINE_LENGTH = 8192;

  const char       *databaseURI;
  char             line[MAX_LINE_LENGTH];
  Errors           error;
  DatabaseHandle   databaseHandle;
  bool             integrityFlag;
  ulong            orphanedEntriesCount,duplicatesCount;
  uint64           t0,t1,dt;
  PrintRowData     printRowData;
  String           s;
  const char       *l;

  initAll();

  // init variables
  AutoFreeList autoFreeList;
  AutoFree_init(&autoFreeList);
  Array_init(&uuidIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&uuids,MISC_UUID_STRING_LENGTH,64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&entityIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&entryIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&storageIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  entryName   = String_new();
  storageName = String_new();
  databaseURI = NULL;
  command     = String_new();
  AUTOFREE_ADD(&autoFreeList,&uuidIds,{ Array_done(&uuidIds); });
  AUTOFREE_ADD(&autoFreeList,&uuids,{ Array_done(&uuids); });
  AUTOFREE_ADD(&autoFreeList,&entityIds,{ Array_done(&entityIds); });
  AUTOFREE_ADD(&autoFreeList,&entryIds,{ Array_done(&entryIds); });
  AUTOFREE_ADD(&autoFreeList,&storageIds,{ Array_done(&storageIds); });
  AUTOFREE_ADD(&autoFreeList,entryName,{ String_delete(entryName); });
  AUTOFREE_ADD(&autoFreeList,storageName,{ String_delete(storageName); });
  AUTOFREE_ADD(&autoFreeList,command,{ String_delete(command); });

  // parse arguments
  CmdOption_init(COMMAND_LINE_OPTIONS);
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       CMD_PRIORITY_ANY,CMD_PRIORITY_ANY,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_INVALID_ARGUMENT);
  }
  uint i = 1;
  uint n = 0;
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

  if (helpFlag || xhelpFlag)
  {
    printf("Usage %s: [<options>] <URI> [<SQL command>...|-]\n",argv[0]);
    printf("\n");
    printf("URI: [sqlite:]<file name>\n");
    printf("     mariadb:<server>:<user>[:<password>]\n");
    printf("     postgresql:<server>:<user>[:<password>]\n");
    printf("\n");
    if      (xhelpFlag) CmdOption_printHelp(stdout,COMMAND_LINE_OPTIONS,1);
    else if (helpFlag ) CmdOption_printHelp(stdout,COMMAND_LINE_OPTIONS,0);

    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_OK);
  }
  if (versionFlag)
  {
    printf("BAR index version %s\n",VERSION_REVISION_STRING);

    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_OK);
  }

  // init encoding converter
  error = initEncodingConverter(systemEncoding,consoleEncoding);
  if (error != ERROR_NONE)
  {
    printError(_("cannot initialize encoding (error: %s)!"),
               Error_getText(error)
              );

    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_FATAL_ERROR);
  }

  // check arguments
  if (databaseURI == NULL)
  {
    printError(_("no database URI given!"));
    doneEncodingConverter();

    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_INVALID_ARGUMENT);
  }

  if (!stringIsEmpty(changeToDirectory))
  {
    error = File_changeDirectoryCString(changeToDirectory);
    if (error != ERROR_NONE)
    {
      printError(_("cannot change to directory '%s' (error: %s)!"),
                 changeToDirectory,
                 Error_getText(error)
                );
      doneEncodingConverter();
      CmdOption_done(COMMAND_LINE_OPTIONS);
      AutoFree_cleanup(&autoFreeList);
      exit(EXITCODE_FAIL);
    }
  }

  // open database
  error = openDatabase(&databaseHandle,databaseURI,createFlag);
  if (error != ERROR_NONE)
  {
    doneEncodingConverter();
    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_FAIL);
  }
  error = Database_setEnabledForeignKeys(&databaseHandle,foreignKeysFlag);
  if (error != ERROR_NONE)
  {
    printError(_("cannot set foreign key support (error: %s)"),Error_getText(error));

    closeDatabase(&databaseHandle);
    doneEncodingConverter();
    CmdOption_done(COMMAND_LINE_OPTIONS);
    AutoFree_cleanup(&autoFreeList);
    exit(EXITCODE_FAIL);
  }

  // set temporary directory
  if (!String_isEmpty(tmpDirectory))
  {
    error = Database_setTmpDirectory(&databaseHandle,String_cString(tmpDirectory));
    if (error != ERROR_NONE)
    {
      printError("%s",Error_getText(error));

      closeDatabase(&databaseHandle);
      doneEncodingConverter();
      CmdOption_done(COMMAND_LINE_OPTIONS);
      AutoFree_cleanup(&autoFreeList);
      exit(EXITCODE_FAIL);
    }
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
          && !checkFlag
          && !optimizeFlag
          && !reindexFlag
          && !createTriggersFlag
          && !dropTablesFlag
          && !dropTriggersFlag
          && !createIndicesFlag
          && !createFTSIndicesFlag
          && !dropIndicesFlag
          && !createNewestFlag
          && !createAggregatesEntitiesFlag
          && !createAggregatesStoragesFlag
          && !createAggregatesFlag
          && !cleanOrphanedFlag
          && !cleanDuplicateEntriesFlag
          && !cleanFlag
          && !purgeFlag
          && !purgeWithErrorFlag
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
    printUUIDsInfo(&databaseHandle,uuidIds,uuids);
  }

  if (infoEntitiesFlag)
  {
    printEntitiesInfo(&databaseHandle,entityIds);
  }

  if      (infoLostEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entryIds,entryType,entryName,TRUE);
  }
  else if (infoEntriesFlag)
  {
    printEntriesInfo(&databaseHandle,entryIds,entryType,entryName,FALSE);
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
    if (error == ERROR_NONE) error = dropTables(&databaseHandle,FALSE);
    if (error == ERROR_NONE) error = dropViews(&databaseHandle,FALSE);
  }

  // drop triggeres
  if (dropTriggersFlag)
  {
    if (error == ERROR_NONE) error = dropTriggers(&databaseHandle,FALSE);
  }

  // drop indices
  if (dropIndicesFlag)
  {
    if (error == ERROR_NONE) error = dropIndices(&databaseHandle,FALSE);
  }

  // create tables/views/indices/triggers
  if (createFlag && !pipeFlag)
  {
    if (error == ERROR_NONE) error = createTablesViewsIndicesTriggers(&databaseHandle);
  }

  // import
  if (importFileName != NULL)
  {
    if (error == ERROR_NONE) error = importIntoDatabase(&databaseHandle,importFileName);
  }

  // check
  if (checkIntegrityFlag || checkFlag)
  {
    integrityFlag = checkIntegrity(&databaseHandle);
  }
  else
  {
    integrityFlag = TRUE;
  }
  if (checkOrphanedFlag || checkFlag)
  {
    orphanedEntriesCount = checkOrphanedEntries(&databaseHandle);
  }
  else
  {
    orphanedEntriesCount = 0L;
  }
  if (checkDuplicatesFlag || checkFlag)
  {
    duplicatesCount = checkDuplicates(&databaseHandle);
  }
  else
  {
    duplicatesCount = 0L;
  }

  // recreate triggeres
  if (createTriggersFlag)
  {
    if (error == ERROR_NONE) error = createTriggers(&databaseHandle);
  }

  // recreate indices
  if (createIndicesFlag)
  {
    if (error == ERROR_NONE) error = createIndices(&databaseHandle);
  }
  if (createFTSIndicesFlag)
  {
    if (error == ERROR_NONE) error = createFTSIndices(&databaseHandle);
  }

  // clean
  if (cleanOrphanedFlag || cleanFlag)
  {
    error = cleanOrphanedEntries(&databaseHandle);
  }
  if (cleanDuplicateEntriesFlag || cleanFlag)
  {
    if (error == ERROR_NONE) error = cleanDuplicateEntries(&databaseHandle);
  }

  // recreate newest data
  if (createNewestFlag)
  {
    if (error == ERROR_NONE) error = createNewest(&databaseHandle,storageIds);
  }

  // calculate aggregates data
  if (createAggregatesStoragesFlag || createAggregatesFlag)
  {
    if (error == ERROR_NONE) error = createAggregatesStorages(&databaseHandle,storageIds);
  }
  if (createAggregatesEntitiesFlag || createAggregatesFlag)
  {
    if (error == ERROR_NONE) error = createAggregatesEntities(&databaseHandle,entityIds);
  }

  // purge deleted storages
  if (purgeFlag)
  {
    if (error == ERROR_NONE) error = purgeDeletedStorages(&databaseHandle);
    if (error == ERROR_NONE) error = purgeDeletedEntities(&databaseHandle);
  }
  if (purgeWithErrorFlag)
  {
    if (error == ERROR_NONE) error = purgeStoragesWithError(&databaseHandle);
  }

  // vacuum
  if (vacuumFlag)
  {
    if (error == ERROR_NONE) error = vacuum(&databaseHandle,toFileName);
  }

  // re-create existing indices
  if (reindexFlag)
  {
    if (error == ERROR_NONE) error = reindex(&databaseHandle);
  }

  // optimize
  if (optimizeFlag)
  {
    optimizeDatabase(&databaseHandle);
  }

//TODO: remove? use storages-info?
  if (showStoragesFlag)
  {
    uint maxIdLength,maxStorageNameLength;

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 2);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = (values[0].id > 0LL) ? ceil(log10((double)values[0].id)) : 1.0;
                   maxIdLength          = (uint)d;
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

                   printf("%-*"PRIi64" %-*s %-*s %-s\n",maxIdLength,values[0].id,maxStorageNameLength,String_cString(values[1].string),MISC_UUID_STRING_LENGTH,String_cString(values[2].string),s);

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
    uint maxIdLength,maxEntryNameLength,maxStorageNameLength;

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = (values[0].id > 0LL) ? ceil(log10((double)values[0].id)) : 1.0;
                   maxIdLength          = (uint)d;
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

    UNUSED_VARIABLE(maxStorageNameLength);
    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   printf("%*"PRIi64" %-*s %-s\n",maxIdLength,values[0].id,maxEntryNameLength,String_cString(values[1].string),String_cString(values[2].string));

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
    uint maxIdLength,maxEntryNameLength,maxStorageNameLength;

    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   double d;

                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   d = (values[0].id > 0LL) ? ceil(log10((double)values[0].id)) : 1.0;
                   maxIdLength          = (uint)d;
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

    UNUSED_VARIABLE(maxStorageNameLength);
    Database_get(&databaseHandle,
                 CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                 {
                   assert(values != NULL);
                   assert(valueCount == 3);

                   UNUSED_VARIABLE(valueCount);
                   UNUSED_VARIABLE(userData);

                   printf("%*"PRIi64" %-*s %-s\n",maxIdLength,values[0].id,maxEntryNameLength,String_cString(values[1].string),String_cString(values[2].string));

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
        printError(_("SQL command '%s' fail: %s!"),String_cString(command),Error_getText(error));

        doneEncodingConverter();
        CmdOption_done(COMMAND_LINE_OPTIONS);
        AutoFree_cleanup(&autoFreeList);
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
        printError(_("init transaction fail: %s!"),Error_getText(error));

        doneEncodingConverter();
        CmdOption_done(COMMAND_LINE_OPTIONS);
        AutoFree_cleanup(&autoFreeList);
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
                           DATABASE_PLAIN(l),
                           DATABASE_COLUMNS_NONE,
                           DATABASE_FILTERS_NONE,
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (error != ERROR_NONE)
      {
        printError(_("SQL command '%s' fail: %s!"),l,Error_getText(error));

        doneEncodingConverter();
        CmdOption_done(COMMAND_LINE_OPTIONS);
        AutoFree_cleanup(&autoFreeList);
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
        printError(_("done transaction fail: %s!"),Error_getText(error));

        doneEncodingConverter();
        CmdOption_done(COMMAND_LINE_OPTIONS);
        AutoFree_cleanup(&autoFreeList);
        exit(EXITCODE_FAIL);
      }
    }
  }

  // close database
  closeDatabase(&databaseHandle);

  // free resources
  AUTOFREE_REMOVE(&autoFreeList,storageName);
  AUTOFREE_REMOVE(&autoFreeList,command);
  AUTOFREE_REMOVE(&autoFreeList,entryName);
  AUTOFREE_REMOVE(&autoFreeList,&storageIds);
  AUTOFREE_REMOVE(&autoFreeList,&entryIds);
  AUTOFREE_REMOVE(&autoFreeList,&entityIds);
  AUTOFREE_REMOVE(&autoFreeList,&uuids);
  AUTOFREE_REMOVE(&autoFreeList,&uuidIds);
  doneEncodingConverter();
  CmdOption_done(COMMAND_LINE_OPTIONS);
  String_delete(command);
  String_delete(storageName);
  String_delete(entryName);
  Array_done(&storageIds);
  Array_done(&entryIds);
  Array_done(&entityIds);
  Array_done(&uuids);
  Array_done(&uuidIds);

  doneAll();

  return    (error == ERROR_NONE)
         && (!checkIntegrityFlag  || integrityFlag)
         && (!checkOrphanedFlag   || (orphanedEntriesCount == 0L))
         && (!checkDuplicatesFlag || (duplicatesCount == 0L))
         && (!checkFlag || (integrityFlag && (orphanedEntriesCount == 0L) && (duplicatesCount == 0L)))
           ? EXITCODE_OK
           : EXITCODE_FAIL;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
