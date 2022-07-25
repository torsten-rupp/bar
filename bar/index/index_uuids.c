/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index UUID functions
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

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

#include "index/index.h"
#include "index/index_common.h"

#include "index/index_uuids.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : cleanUpNoUUID
* Purpose: purge UUID entries without jobUUID
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpNoUUID(IndexHandle *indexHandle)
{
  ulong  n;
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes without UUID"
             );

  // init variables

  // clean-up
  n = 0L;
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_delete(&indexHandle->databaseHandle,
                            &n,
                            "uuids",
                            DATABASE_FLAG_NONE,
                            "uuids.jobUUID=''",
                            DATABASE_FILTERS
                            (
                            ),
                            DATABASE_UNLIMITED
                           );
  });

  // free resource

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes without UUID",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without UUID failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

#if 0
//TODO: still not used
/***********************************************************************\
* Name   : updateUUIDAggregates
* Purpose: update UUID aggregates
* Input  : indexHandle - index handle
*          uuidId      - UUID database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : currently nothing to do
\***********************************************************************/

LOCAL Errors updateUUIDAggregates(IndexHandle *indexHandle,
                                  DatabaseId  uuidId
                                 )
{
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(uuidId);

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : isEmptyUUID
* Purpose: check if UUID if empty
* Input  : indexHandle - index handle
*          uuidId      - UUID database id
* Output : -
* Return : TRUE iff UUID is empty
* Notes  : -
\***********************************************************************/

LOCAL bool isEmptyUUID(IndexHandle *indexHandle,
                       DatabaseId  uuidId
                      )
{
  assert(indexHandle != NULL);

  return    (uuidId != DATABASE_ID_NONE)
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "entities \
                                     LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  ",
                                  DATABASE_FLAG_NONE,
                                  "entities.id",
                                  "uuids.id=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(uuidId)
                                  )
                                 );
}

// ----------------------------------------------------------------------

Errors IndexUUID_cleanUp(IndexHandle *indexHandle)
{
  Errors error;

  assert(indexHandle != NULL);

  error = ERROR_NONE;

  if (error == ERROR_NONE) error = cleanUpNoUUID(indexHandle);

  return error;
}

Errors IndexUUID_purge(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       DatabaseId  uuidId
                      )
{
  Errors        error;

  assert(indexHandle != NULL);
  assert(uuidId != DATABASE_ID_NONE);

  UNUSED_VARIABLE(doneFlag);
  UNUSED_VARIABLE(deletedCounter);

  if (indexHandle->masterIO == NULL)
  {
    // delete UUID index
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "uuids",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(uuidId)
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_PURGE uuidId=%"PRIi64,
                                    INDEX_ID_(INDEX_TYPE_UUID,uuidId)
                                   );
  }

  return ERROR_NONE;
}

Errors IndexUUID_prune(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       DatabaseId  uuidId
                      )
{
  Errors        error;

  assert(indexHandle != NULL);
  assert(uuidId != DATABASE_ID_NONE);

  UNUSED_VARIABLE(doneFlag);
  UNUSED_VARIABLE(deletedCounter);

  // delete uuid if empty
  if (isEmptyUUID(indexHandle,uuidId))
  {
    error = IndexUUID_purge(indexHandle,doneFlag,deletedCounter,uuidId);
    if (error != ERROR_NONE)
    {
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Purged UUID #%"PRIu64": no entities",
                uuidId
               );
  }

  return ERROR_NONE;
}

#if 0
//TODO: not used, remove
/***********************************************************************\
* Name   : refreshUUIDsInfos
* Purpose: refresh UUIDs infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshUUIDsInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          indexId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListUUIDs(&indexQueryHandle,
                              indexHandle,
                              INDEX_STATE_SET_ALL,
                              INDEX_MODE_SET_ALL,
                              NULL,  // name
                              0LL,  // offset
                              INDEX_UNLIMITED
                             );
  if (error == ERROR_NONE)
  {
    while (Index_getNextUUID(&indexQueryHandle,
                             &indexId,
                             NULL,  // jobUUID
                             NULL,  // lastCheckedDateTime
                             NULL,  // lastErrorMessage
                             NULL,  // size
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
          )
    {
      error = Index_updateUUIDInfos(indexHandle,indexId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}
#endif

// ----------------------------------------------------------------------

Errors Index_findUUID(IndexHandle  *indexHandle,
                      const char   *findJobUUID,
                      const char   *findScheduleUUID,
                      IndexId      *uuidId,
                      uint         *executionCountNormal,
                      uint         *executionCountFull,
                      uint         *executionCountIncremental,
                      uint         *executionCountDifferential,
                      uint         *executionCountContinuous,
                      uint64       *averageDurationNormal,
                      uint64       *averageDurationFull,
                      uint64       *averageDurationIncremental,
                      uint64       *averageDurationDifferential,
                      uint64       *averageDurationContinuous,
                      uint         *totalEntityCount,
                      uint         *totalStorageCount,
                      uint64       *totalStorageSize,
                      uint         *totalEntryCount,
                      uint64       *totalEntrySize
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return ERROR_DATABASE_NOT_FOUND;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_get(&indexHandle->databaseHandle,
                          CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                          {
                            assert(values != NULL);
                            assert(valueCount == 16);

                            UNUSED_VARIABLE(userData);
                            UNUSED_VARIABLE(valueCount);

                            if (uuidId                      != NULL) (*uuidId                     ) = INDEX_ID_UUID(values[0].id);
                            if (executionCountNormal        != NULL) (*executionCountNormal       ) = values[ 1].u;
                            if (executionCountFull          != NULL) (*executionCountFull         ) = values[ 2].u;
                            if (executionCountIncremental   != NULL) (*executionCountIncremental  ) = values[ 3].u;
                            if (executionCountDifferential  != NULL) (*executionCountDifferential ) = values[ 4].u;
                            if (executionCountContinuous    != NULL) (*executionCountContinuous   ) = values[ 5].u;
                            if (averageDurationNormal       != NULL) (*averageDurationNormal      ) = values[ 6].u;
                            if (averageDurationFull         != NULL) (*averageDurationFull        ) = values[ 7].u;
                            if (averageDurationIncremental  != NULL) (*averageDurationIncremental ) = values[ 8].u;
                            if (averageDurationDifferential != NULL) (*averageDurationDifferential) = values[ 9].u;
                            if (averageDurationContinuous   != NULL) (*averageDurationContinuous  ) = values[10].u;
                            if (totalEntityCount            != NULL) (*totalEntityCount           ) = values[11].u;
                            if (totalStorageCount           != NULL) (*totalStorageCount          ) = values[12].u;
                            if (totalStorageSize            != NULL) (*totalStorageSize           ) = values[13].u64;
                            if (totalEntryCount             != NULL) (*totalEntryCount            ) = values[14].u;
                            if (totalEntrySize              != NULL) (*totalEntrySize             ) = values[15].u64;

                            return ERROR_NONE;
                          },NULL),
                          NULL,  // changedRowCount
                          DATABASE_TABLES
                          (
                            "uuids \
                               LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                               LEFT JOIN storages ON storages.entityId=entities.id AND (storages.deletedFlag!=TRUE) \
                            "
                          ),
                          DATABASE_FLAG_NONE,
                          DATABASE_COLUMNS
                          (
                            DATABASE_COLUMN_KEY   ("uuids.id"),
                            DATABASE_COLUMN_UINT  ("(SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND COALESCE(history.errorMessage,'')='' AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND COALESCE(history.errorMessage,'')='' AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND COALESCE(history.errorMessage,'')='' AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND COALESCE(history.errorMessage,'')='' AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("(SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND COALESCE(history.errorMessage,'')='' AND history.type=?)"),
                            DATABASE_COLUMN_UINT  ("COUNT(entities.id)"),
                            DATABASE_COLUMN_UINT  ("COUNT(storages.id)"),
                            DATABASE_COLUMN_UINT64("SUM(storages.size)"),
                            DATABASE_COLUMN_UINT  ("SUM(storages.totalEntryCount)"),
                            DATABASE_COLUMN_UINT64("SUM(storages.totalEntrySize)")
                          ),
                          "    ? OR uuids.jobUUID=? \
                           AND ? OR entities.scheduleUUID=? \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_NORMAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_FULL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_INCREMENTAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_DIFFERENTIAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_CONTINUOUS),

                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_NORMAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_FULL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_INCREMENTAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_DIFFERENTIAL),
                            DATABASE_FILTER_UINT(ARCHIVE_TYPE_CONTINUOUS),

                            DATABASE_FILTER_BOOL   (stringIsEmpty(findJobUUID)),
                            DATABASE_FILTER_CSTRING(findJobUUID),
                            DATABASE_FILTER_BOOL   (stringIsEmpty(findScheduleUUID)),
                            DATABASE_FILTER_CSTRING(findScheduleUUID)
                          ),
                          "uuids.id",
                          NULL,  // orderBy
                          0LL,
                          1LL
                         );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      StringMap_getInt64 (resultMap,"uuidId",uuidId,INDEX_ID_NONE);
                                      if (!INDEX_ID_IS_NONE(*uuidId))
                                      {
                                        if (executionCountNormal        != NULL) StringMap_getUInt  (resultMap,"executionCountNormal",       executionCountNormal,       0   );
                                        if (executionCountFull          != NULL) StringMap_getUInt  (resultMap,"executionCountFull",         executionCountFull,         0   );
                                        if (executionCountIncremental   != NULL) StringMap_getUInt  (resultMap,"executionCountIncremental",  executionCountIncremental,  0   );
                                        if (executionCountDifferential  != NULL) StringMap_getUInt  (resultMap,"executionCountDifferential", executionCountDifferential, 0   );
                                        if (executionCountContinuous    != NULL) StringMap_getUInt  (resultMap,"executionCountContinuous",   executionCountContinuous,   0   );
                                        if (averageDurationNormal       != NULL) StringMap_getUInt64(resultMap,"averageDurationNormal",      averageDurationNormal,      0LL );
                                        if (averageDurationFull         != NULL) StringMap_getUInt64(resultMap,"averageDurationFull",        averageDurationFull,        0LL );
                                        if (averageDurationIncremental  != NULL) StringMap_getUInt64(resultMap,"averageDurationIncremental", averageDurationIncremental, 0LL );
                                        if (averageDurationDifferential != NULL) StringMap_getUInt64(resultMap,"averageDurationDifferential",averageDurationDifferential,0LL );
                                        if (averageDurationContinuous   != NULL) StringMap_getUInt64(resultMap,"averageDurationContinuous",  averageDurationContinuous,  0LL );
                                        if (totalEntityCount            != NULL) StringMap_getUInt  (resultMap,"totalEntityCount",           totalEntityCount,           0L  );
                                        if (totalStorageCount           != NULL) StringMap_getUInt  (resultMap,"totalStorageCount",          totalStorageCount,          0   );
                                        if (totalStorageSize            != NULL) StringMap_getUInt64(resultMap,"totalStorageSize",           totalStorageSize,           0LL );
                                        if (totalEntryCount             != NULL) StringMap_getUInt  (resultMap,"totalEntryCount",            totalEntryCount,            0   );
                                        if (totalEntrySize              != NULL) StringMap_getUInt64(resultMap,"totalEntrySize",             totalEntrySize,             0LL );

                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_DATABASE_INDEX_NOT_FOUND;
                                      }
                                    },NULL),
                                    "INDEX_FIND_UUID jobUUID=%'s scheduleUUID=%'s",
                                    findJobUUID,
                                    (findScheduleUUID != NULL) ? findScheduleUUID : ""
                                   );
  }

  return error;
}

Errors Index_getUUIDsInfos(IndexHandle *indexHandle,
                           IndexId     uuidId,
//TODO: remove?
                           ConstString jobUUID,
                           ConstString scheduleUUID,
                           ConstString name,
                           uint64      *lastExecutedDateTime,
                           uint        *totalEntityCount,
                           uint        *totalEntryCount,
                           uint64      *totalEntrySize
                          )
{
  String ftsMatchString;
  String filterString;
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // init variables

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
// TODO: FTS_uuids do not exists
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_uuids.name",name);

  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  Database_filterAppend(filterString,!String_isEmpty(jobUUID),"AND","uuids.jobUUID='%S'",jobUUID);
  Database_filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID='%S'",scheduleUUID);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","uuids.id IN (SELECT uuidId FROM FTS_uuids WHERE %S)",ftsMatchString);

  INDEX_DO(indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    // get last executed, total entities count, total entry count, total entry size
    if (lastExecutedDateTime != NULL) (*lastExecutedDateTime) = 0LL;
    if (totalEntityCount     != NULL) (*totalEntityCount)     = 0;
    if (totalEntryCount      != NULL) (*totalEntryCount)      = 0;
    if (totalEntrySize       != NULL) (*totalEntrySize)       = 0LL;
    error = Database_get(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 4);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           if (lastExecutedDateTime != NULL) (*lastExecutedDateTime) = values[0].u64;
                           if (totalEntityCount     != NULL) (*totalEntityCount)     = values[1].u;
                           if (totalEntryCount      != NULL) (*totalEntryCount)      = values[2].u;
                           if (totalEntrySize       != NULL) (*totalEntrySize)       = values[3].u64;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
  //TODO newest
                         DATABASE_TABLES
                         (
                           "uuids \
                             LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                             LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=1 \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_DATETIME("MAX(entities.created)"),
                           DATABASE_COLUMN_UINT    ("COUNT(entities.id)"),
                           DATABASE_COLUMN_UINT    ("SUM(storages.totalEntryCount)"),
                           DATABASE_COLUMN_UINT64  ("SUM(storages.totalEntrySize)"),
                         ),
                         stringFormat(sqlString,sizeof(sqlString),
                                      "%s",
                                      String_cString(filterString)
                                     ),
                         DATABASE_FILTERS
                         (
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         1LL
                        );
    assert((error == ERROR_NONE) || (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND));
  });

  // free resources
  Database_deleteFilter(filterString);
  String_delete(ftsMatchString);

  return ERROR_NONE;
}

Errors Index_updateUUIDInfos(IndexHandle *indexHandle,
                             IndexId     uuidId
                            )
{
// not required? calculate on-the-fly
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
#if 0
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalFileCount;
  double              totalFileSize_;
  uint64              totalFileSize;
  ulong               totalImageCount;
  double              totalImageSize_;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  double              totalHardlinkSize_;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // get file aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalFileCount = values[0].u;
                             totalFileSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get image aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalImageCount = values[0].u;
                             totalImageSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           )
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
7                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get directory aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)"),
                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get link aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)")
                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get hardlink aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalHardlinkCount = values[0].u;
                             totalHardlinkSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get special aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                                LEFT JOIN entities ON entities.id=entries.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entries.id)"),
                           ),
                           "    entries.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update aggregate data
//fprintf(stderr,"%s, %d: aggregate %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "uuids",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT("totalEntryCount",      totalFileCount
                                                                           +totalImageCount
                                                                           +totalDirectoryCount
                                                                           +totalLinkCount
                                                                           +totalHardlinkCount
                                                                           +totalSpecialCount
                                                   ),
                                DATABASE_VALUE_UINT("totalEntrySize",       totalFileSize
                                                                           +totalImageSize
                                                                           +totalHardlinkSize
                                                   ),
                                DATABASE_VALUE_UINT("totalFileCount",      totalFileCount),
                                DATABASE_VALUE_UINT("totalFileSize",       totalFileSize),
                                DATABASE_VALUE_UINT("totalImageCount",     totalImageCount),
                                DATABASE_VALUE_UINT("totalImageSize",      totalImageSize),
                                DATABASE_VALUE_UINT("totalDirectoryCount", totalDirectoryCount),
                                DATABASE_VALUE_UINT("totalLinkCount",      totalLinkCount),
                                DATABASE_VALUE_UINT("totalHardlinkCount",  totalHardlinkCount),
                                DATABASE_VALUE_UINT("totalHardlinkSize",   totalHardlinkSize),
                                DATABASE_VALUE_UINT("totalSpecialCount",   totalSpecialCount),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(Index_getDatabaseId(uuidId))
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // -----------------------------------------------------------------

      // get newest file aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalFileCount = values[0].u;
                             totalFileSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get newest image aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalImageCount = values[0].u;
                             totalImageSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get newest directory aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get newest link aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get newest hardlink aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalHardlinkCount = values[0].u;
                             totalHardlinkSize  = values[0].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                             DATABASE_COLUMN_STRING("SUM(entryFragments.size)")
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get newest special aggregate data
      error = Database_get(&indexHandle->databaseHandle,
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
                             "entriesNewest \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                                LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("COUNT(DISTINCT entriesNewest.id)"),
                           ),
                           "    entriesNewest.type=? \
                            AND uuids.id=?  \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),
                             DATABASE_FILTER_KEY   (Index_getDatabaseId(uuidId))
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update newest aggregate data
//fprintf(stderr,"%s, %d: newest aggregate %"PRIu64" %"PRIu64"\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "uuids",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT("totalEntryCountNewest",      totalFileCount
                                                                                 +totalImageCount
                                                                                 +totalDirectoryCount
                                                                                 +totalLinkCount
                                                                                 +totalHardlinkCount
                                                                                 +totalSpecialCount
                                                   ),
                                DATABASE_VALUE_UINT("totalEntrySizeNewest",       totalFileSize
                                                                                 +totalImageSize
                                                                                 +totalHardlinkSize
                                                   ),
                                DATABASE_VALUE_UINT("totalFileCountNewest",      totalFileCount),
                                DATABASE_VALUE_UINT("totalFileSizeNewest",       totalFileSize),
                                DATABASE_VALUE_UINT("totalImageCountNewest",     totalImageCount),
                                DATABASE_VALUE_UINT("totalImageSizeNewest",      totalImageSize),
                                DATABASE_VALUE_UINT("totalDirectoryCountNewest", totalDirectoryCount),
                                DATABASE_VALUE_UINT("totalLinkCountNewest",      totalLinkCount),
                                DATABASE_VALUE_UINT("totalHardlinkCountNewest",  totalHardlinkCount),
                                DATABASE_VALUE_UINT("totalHardlinkSizeNewest",   totalHardlinkSize),
                                DATABASE_VALUE_UINT("totalSpecialCountNewest",   totalSpecialCount),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(Index_getDatabaseId(uuidId))
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_UPDATE_INFOS uuidId=%lld",
                                    uuidId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  return ERROR_NONE;
}

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           IndexStateSet    indexStateSet,
                           IndexModeSet     indexModeSet,
                           ConstString      name,
                           uint64           offset,
                           uint64           limit
                          )
{
  String ftsMatchString;
  String filterString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_storages.name",name);

  // get filters
  string = String_new();
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  Database_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
//TODO newest
                           "uuids \
                              LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                              LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=TRUE \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("uuids.id"),
                             DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                             DATABASE_COLUMN_DATETIME("(SELECT MAX(entities.created) FROM entities WHERE entities.jobUUID=uuids.jobUUID)"),
                             DATABASE_COLUMN_STRING  ("(SELECT storages.errorMessage FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 1)"),
                             DATABASE_COLUMN_UINT64  ("SUM(storages.size)"),
                             DATABASE_COLUMN_UINT    ("SUM(storages.totalEntryCount)"),
                             DATABASE_COLUMN_UINT64  ("SUM(storages.totalEntrySize)")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "     %s \
                                          AND uuids.jobUUID!='' \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           "uuids.id",
                           NULL,  // orderBy
                           offset,
                           limit
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(ftsMatchString);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);

  // free resources
  Database_deleteFilter(filterString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           jobUUID,
                       uint64           *lastExecutedDateTime,
                       uint             *lastErrorCode,
                       String           lastErrorData,
                       uint64           *totalSize,
                       uint             *totalEntryCount,
                       uint64           *totalEntrySize
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

// TODO:
if (lastErrorCode != NULL) (*lastErrorCode) = 0;
  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           jobUUID,
                           lastExecutedDateTime,
// TODO:                           lastErrorCode
                           lastErrorData,
                           totalSize,
                           totalEntryCount,
                           totalEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_UUID(databaseId);

  return TRUE;
}

Errors Index_newUUID(IndexHandle *indexHandle,
                     const char  *jobUUID,
                     IndexId     *uuidId
                    )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(uuidId != NULL);

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
      DatabaseId databaseId;

      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // databaseId,
                              "uuids",
                              DATABASE_FLAG_IGNORE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_CSTRING("jobUUID", jobUUID),
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_getId(&indexHandle->databaseHandle,
                             &databaseId,
                             "uuids",
                             "id",
                             "jobUUID=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_CSTRING(jobUUID)
                             )
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*uuidId) = INDEX_ID_UUID(databaseId);

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      if (StringMap_getInt64(resultMap,"uuidId",uuidId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_UUID jobUUID=%s",
                                    jobUUID
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        IndexId     uuidId
                       )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    // delete entities
    error = Database_get(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           return Index_deleteEntity(indexHandle,INDEX_ID_ENTITY(values[0].id));
                         },NULL),
                         NULL,  // changedRowCount
                         DATABASE_TABLES
                         (
                           "entities \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                           "
                         ),
                         DATABASE_FLAG_NONE,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY  ("entities.id")
                         ),
                         "uuids.id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (Index_getDatabaseId(uuidId))
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0,
                         DATABASE_UNLIMITED
                        );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    // delete UUID index
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "uuids",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(Index_getDatabaseId(uuidId))
                            ),
                            DATABASE_UNLIMITED
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

bool Index_isEmptyUUID(IndexHandle *indexHandle,
                       IndexId     uuidId
                      )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return isEmptyUUID(indexHandle,
                       Index_getDatabaseId(uuidId)
                      );
  });

  return emptyFlag;
}

Errors Index_purgeUUID(IndexHandle *indexHandle,
                       IndexId     indexId
                      )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_UUID);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexUUID_purge(indexHandle,
                             NULL,  // doneFlag
                             NULL,  // deletedCounter
                             Index_getDatabaseId(indexId)
                            );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_PURGE uuidId=%lld",
                                    indexId
                                   );
  }

  return error;
}

Errors Index_pruneUUID(IndexHandle *indexHandle,
                       IndexId     indexId
                      )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_UUID);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexUUID_prune(indexHandle,
                             NULL,  // doneFlag
                             NULL,  // deletedCounter
                             Index_getDatabaseId(indexId)
                            );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_PRUNE uuidId=%lld",
                                    indexId
                                   );
  }

  return error;
}

Errors IndexUUID_pruneAll(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter
                         )
{
  Array         uuidIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    uuidId;

  assert(indexHandle != NULL);

  // init variables
  Array_init(&uuidIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all UUIDs
  error = Database_getIds(&indexHandle->databaseHandle,
                          &uuidIds,
                          "uuids",
                          "id",
                          DATABASE_FILTERS_NONE
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&uuidIds);
    return error;
  }

  // prune UUIDs
  ARRAY_ITERATEX(&uuidIds,arrayIterator,uuidId,error == ERROR_NONE)
  {
    error = IndexUUID_prune(indexHandle,
                            doneFlag,
                            deletedCounter,
                            uuidId
                           );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&uuidIds);
    return error;
  }

  // free resources
  Array_done(&uuidIds);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
