/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index history functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "errors.h"

#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"
#include "bar.h"
#include "bar_global.h"

#include "index/index.h"
#include "index/index_common.h"

#include "index/index_history.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ---------------------------------------------------------------------

Errors Index_initListHistory(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             ConstString      jobUUID,
                             DatabaseOrdering ordering,
                             uint64           offset,
                             uint64           limit
                            )
{
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  filterString = String_newCString("1");
  orderString  = String_new();

  // get filters
  IndexCommon_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  IndexCommon_filterAppend(filterString,!String_isEmpty(jobUUID),"AND","history.jobUUID=%'S",jobUUID);

  // get ordering
  IndexCommon_appendOrdering(orderString,TRUE,"history.created",ordering);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,KEY,STRING,STRING,STRING,STRING,INT,UINT64,STRING,INT,INT,INT64,INT,INT64,INT,INT64),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT history.id, \
                                                 IFNULL(uuids.id,0), \
                                                 history.jobUUID, \
                                                 history.scheduleUUID, \
                                                 history.hostName, \
                                                 history.userName, \
                                                 history.type, \
                                                 UNIX_TIMESTAMP(history.created), \
                                                 history.errorMessage, \
                                                 history.duration, \
                                                 history.totalEntryCount, \
                                                 history.totalEntrySize, \
                                                 history.skippedEntryCount, \
                                                 history.skippedEntrySize, \
                                                 history.errorEntryCount, \
                                                 history.errorEntrySize \
                                          FROM history \
                                            LEFT JOIN uuids ON uuids.jobUUID=history.jobUUID \
                                          WHERE %s \
                                          %s \
                                          LIMIT ?,? \
                                         ",
                                         String_cString(filterString),
                                         String_cString(orderString)
                                        ),
                            DATABASE_VALUES2
                            (
                            ),
                            DATABASE_FILTERS
                            (
                              UINT64(offset),
                              UINT64(limit)
                            )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextHistory(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          IndexId          *uuidId,
                          String           jobUUID,
                          String           scheduleUUID,
                          String           hostName,
                          String           userName,
                          ArchiveTypes     *archiveType,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          uint64           *duration,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          ulong            *skippedEntryCount,
                          uint64           *skippedEntrySize,
                          ulong            *errorEntryCount,
                          uint64           *errorEntrySize
                         )
{
  DatabaseId historyDatabaseId;
  DatabaseId uuidDatabaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return indexQueryHandle->indexHandle->upgradeError;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %lld %S %S %S %S %u %llu %S %llu %lu %llu %lu %llu %lu %llu",
                           &historyDatabaseId,
                           &uuidDatabaseId,
                           jobUUID,
                           scheduleUUID,
                           hostName,
                           userName,
                           archiveType,
                           createdDateTime,
                           errorMessage,
                           duration,
                           totalEntryCount,
                           totalEntrySize,
                           skippedEntryCount,
                           skippedEntrySize,
                           errorEntryCount,
                           errorEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(historyDatabaseId);
  if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );

  return TRUE;
}

Errors Index_newHistory(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  scheduleUUID,
                        ConstString  hostName,
                        ConstString  userName,
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        uint64       duration,
                        ulong        totalEntryCount,
                        uint64       totalEntrySize,
                        ulong        skippedEntryCount,
                        uint64       skippedEntrySize,
                        ulong        errorEntryCount,
                        uint64       errorEntrySize,
                        IndexId      *historyId
                       )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      Errors error;

      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "history",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                STRING ("jobUUID",           jobUUID),
                                STRING ("scheduleUUID",      scheduleUUID),
                                STRING ("hostName",          hostName),
                                STRING ("userName",          userName),
                                UINT   ("type",              archiveType),
                                UINT   ("type",              archiveType),
                                UINT64 ("created",           createdDateTime),
                                CSTRING("errorMessage",      errorMessage),
                                UINT64 ("duration",          duration),
                                UINT   ("totalEntryCount",   totalEntryCount),
                                UINT64 ("totalEntrySize",    totalEntrySize),
                                UINT   ("skippedEntryCount", skippedEntryCount),
                                UINT64 ("skippedEntrySize",  skippedEntrySize),
                                UINT   ("errorEntryCount",   errorEntryCount),
                                UINT64 ("errorEntrySize",    errorEntrySize)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      Errors error;

                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      error = ERROR_NONE;

                                      if (historyId != NULL)
                                      {
                                        if (!StringMap_getInt64(resultMap,"historyId",historyId,INDEX_ID_NONE))
                                        {
                                          error = ERROR_EXPECTED_PARAMETER;
                                        }
                                      }

                                      return error;
                                    },NULL),
                                    "INDEX_NEW_HISTORY jobUUID=%S scheduleUUID=%s hostName=%'S userName=%'S archiveType=%s createdDateTime=%llu errorMessage=%'s duration=%llu totalEntryCount=%lu totalEntrySize=%llu skippedEntryCount=%lu skippedEntrySize=%llu errorEntryCount=%lu errorEntrySize=%llu",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    userName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime,
                                    errorMessage,
                                    duration,
                                    totalEntryCount,
                                    totalEntrySize,
                                    skippedEntryCount,
                                    skippedEntrySize,
                                    errorEntryCount,
                                    errorEntrySize
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteHistory(IndexHandle *indexHandle,
                           IndexId     historyId
                          )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete history entry
  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM history WHERE id=%lld",
                             Index_getDatabaseId(historyId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
