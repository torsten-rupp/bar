/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index storage functions
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

#include "index.h"
#include "index/index_common.h"
#include "index/index_entities.h"
#include "index/index_entries.h"

#include "index/index_storages.h"

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
* Name   : cleanUpIncompleteUpdate
* Purpose: reset incomplete updated database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteUpdate(IndexHandle *indexHandle)
{
  Errors           error;
  IndexId          indexId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up incomplete updates"
             );

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    // reset lock count
    (void)Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "entities",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES2
                          (
                            UINT("lockedCount", 0),
                          ),
                          NULL,
                          DATABASE_FILTERS
                          (
                          )
                         );

    // clear state of deleted storages
    (void)Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES2
                          (
                            UINT("state", INDEX_STATE_NONE),
                          ),
                          "deletedFlag=?",
                          DATABASE_FILTERS
                          (
                            BOOL(TRUE),
                          )
                         );

    // request update state of incomplete storages
    error = ERROR_NONE;
    while (Index_findStorageByState(indexHandle,
                                    INDEX_STATE_SET(INDEX_STATE_UPDATE),
                                    NULL,  // uuidId
                                    NULL,  // jobUUID
                                    NULL,  // entityId
                                    NULL,  // scheduleUUID
                                    &indexId,
                                    storageName,
                                    NULL,  // dateTime
                                    NULL,  // size
                                    NULL,  // indexMode
                                    NULL,  // lastCheckedDateTime
                                    NULL,  // errorMessage
                                    NULL,  // totalEntryCount
                                    NULL  // totalEntrySize
                                   )
           && (error == ERROR_NONE)
          )
    {
      // get printable name (if possible)
      error = Storage_parseName(&storageSpecifier,storageName);
      if (error == ERROR_NONE)
      {
        Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
      }
      else
      {
        String_set(printableStorageName,storageName);
      }

      error = Index_setStorageState(indexHandle,
                                    indexId,
                                    INDEX_STATE_UPDATE_REQUESTED,
                                    0LL,
                                    NULL
                                   );
      if (error == ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "Requested update index #%lld: %s",
                    indexId,
                    String_cString(printableStorageName)
                   );
      }
    }

    return error;
  });

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up incomplete updates"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up incomplete updates failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStoragenNoName
* Purpose: purge storage entries without name
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoName(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes without name"
             );

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount,
                                 INDEX_TYPE_SET_ALL,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // hostName
                                 NULL,  // userName
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // entityId
                                NULL,  // scheduleUUID
                                NULL,  // hostName
                                NULL,  // userName
                                NULL,  // comment
                                NULL,  // createdDateTime
                                NULL,  // archiveType
                                &storageId,
                                storageName,
                                NULL,  // dateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteStorage(indexHandle,storageId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  // free resource
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes without name",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without name failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStorageNoEntity
* Purpose: clean-up storage entries without any entity
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoEntity(IndexHandle *indexHandle)
{
//TODO
#if 0
  Errors                  error;
  String                  name1,name2;
  DatabaseStatementHandle databaseStatementHandle1,databaseStatementHandle2;
  DatabaseId              storageDatabaseId;
  StaticString            (uuid,MISC_UUID_STRING_LENGTH);
  uint64                  createdDateTime;
  DatabaseId              entityDatabaseId;
  bool                    equalsFlag;
  ulong                   i;
  String                  oldDatabaseFileName;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up no entity-entries"
             );

  // init variables
  name1 = String_new();
  name2 = String_new();

  // try to set entityId in storage entries
  INDEX_DOX(error,
            indexHandle,
  {
   error = Database_prepare(&databaseStatementHandle1,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(STRING,STRING,UINT64),
                             "SELECT uuid, \
                                     name, \
                                     UNIX_TIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY id,created ASC \
                             ",
                             DATABASE_VALUES
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,results
                                 "%S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseStatementHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(KEY,STRING),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=? \
                                 ",
                                 DATABASE_VALUES
                                 (
                                 ).
                                 DATABASE_FILTERS
                                 (
                                   STRING(uuid)
                                 )
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseStatementHandle2,
                                     "%lld %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
            // compare names (equals except digits)
            equalsFlag = String_length(name1) == String_length(name2);
            i = STRING_BEGIN;
            while (equalsFlag
                   && (i < String_length(name1))
                   && (   isdigit(String_index(name1,i))
                       || (String_index(name1,i) == String_index(name2,i))
                      )
                  )
            {
              i++;
            }
            if (equalsFlag)
            {
              // assign entity id
// TODO:
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     DATABASE_COLUMN_TYPES(),
                                     "UPDATE storages \
                                      SET entityId=%lld \
                                      WHERE id=%lld \
                                     ",
                                     entityDatabaseId,
                                     storageDatabaseId
                                    );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_insert(&newIndexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "entities",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("jobUUID", uuid),
                                  UINT64("created", createdDateTime),
                                  UINT  ("type",    ARCHIVE_TYPE_FULL),
                                )
                               );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

          // assign entity id for all storage entries with same uuid and matching name (equals except digits)
          error = Database_prepare(&databaseStatementHandle2,
                                   &oldIndexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(KEY,STRING),
                                   "SELECT id, \
                                           name \
                                    FROM storages \
                                    WHERE uuid=? \
                                   ",
                                   DATABASE_VALUES
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                     STRING(uuid)
                                   )
                                  );
          if (error == ERROR_NONE)
          {
            while (Database_getNextRow(&databaseStatementHandle2,
                                       "%lld %S",
                                       &storageId,
                                       name2
                                      )
                  )
            {
              // compare names (equals except digits)
              equalsFlag = String_length(name1) == String_length(name2);
              i = STRING_BEGIN;
              while (equalsFlag
                     && (i < String_length(name1))
                     && (   isdigit(String_index(name1,i))
                         || (String_index(name1,i) == String_index(name2,i))
                        )
                    )
              {
                i++;
              }
              if (equalsFlag)
              {
                // assign entity id
// TODO:
                (void)Database_execute(&newIndexHandle->databaseHandle,
                                       CALLBACK_(NULL,NULL),  // databaseRowFunction
                                       NULL,  // changedRowCount
                                       DATABASE_COLUMN_TYPES(),
                                       "UPDATE storages \
                                        SET entityId=%lld \
                                        WHERE id=%lld \
                                       ",
                                       entityId,
                                       storageId
                                      );
              }
            }
            Database_finalize(&databaseStatementHandle2);
          }
        }
      }
      Database_finalize(&databaseStatementHandle1);
    }

    return ERROR_NONE;
  });

  // free resources
  String_delete(name2);
  String_delete(name1);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up no entity-entries"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up no entity-entries failed (error: %s)",
                Error_getText(error)
               );
  }
#else
UNUSED_VARIABLE(indexHandle);
return ERROR_NONE;
#endif
}

/***********************************************************************\
* Name   : cleanUpStorageInvalidState
* Purpose: clean-up storage entries with invalid state
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageInvalidState(IndexHandle *indexHandle)
{
  ulong   n;
  Errors  error;
  IndexId storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up indizes with invalid state"
             );

  // init variables

  // clean-up
  n = 0;
  INDEX_DOX(error,
            indexHandle,
  {
    do
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &storageId,
                             "storages",
                             "id",
                             "WHERE     ((state<%u) OR (state>%u)) \
                                    AND deletedFlag!=1 \
                             ",
                             INDEX_STATE_MIN,
                             INDEX_STATE_MAX
                            );
      if ((error == ERROR_NONE) && (storageId != DATABASE_ID_NONE))
      {
        error = Index_deleteStorage(indexHandle,INDEX_ID_STORAGE(storageId));
        n++;
      }
    }
    while ((error == ERROR_NONE) && (storageId != DATABASE_ID_NONE));

    return error;
  });

  // free resource

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up %lu indizes with invalid state",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes with invalid state failed (error: %s)",
                Error_getText(error)
               );
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpDuplicateStorages
* Purpose: clean-up duplicate storages
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: use
#if 0
LOCAL Errors cleanUpDuplicateStorages(IndexHandle *indexHandle)
{
  Errors              error;
  String              name1,name2;
  DatabaseStatementHandle databaseStatementHandle1,databaseStatementHandle2;
  DatabaseId          storageDatabaseId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityDatabaseId;
  bool                equalsFlag;
  ulong               i;
  String              oldDatabaseFileName;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start clean-up duplicate storages"
             );

  // init variables
  name1 = String_new();
  name2 = String_new();

  // try to set entityId in storage entries
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle1,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,STRING,UINT64),
                             "SELECT id, \
                                     name, \
                                     UNIX_TIMESTAMP(created) \
                              FROM storages \
                              WHERE entityId=0 \
                              ORDER BY name ASC \
                             "
                             DATABASE_VALUES
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error == ERROR_NONE)
    {
      while (Database_getNextRow(&databaseStatementHandle1,
                                 "%llu %S %llu",
                                 uuid,
                                 name1,
                                 &createdDateTime
                                )
         )
      {
        // find matching entity/create default entity
        error = Database_prepare(&databaseStatementHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(KEY,STRING),
                                 "SELECT id, \
                                         name \
                                  FROM storages \
                                  WHERE uuid=? \
                                 ",
                                 DATABASE_VALUES
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                   STRING(uuid)
                                 )
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseStatementHandle2,
                                     "%lld %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
            // compare names (equals except digits)
            equalsFlag = String_length(name1) == String_length(name2);
            i = STRING_BEGIN;
            while (equalsFlag
                   && (i < String_length(name1))
                   && (   isdigit(String_index(name1,i))
                       || (String_index(name1,i) == String_index(name2,i))
                      )
                  )
            {
              i++;
            }
            if (equalsFlag)
            {
              // assign entity id
// TODO:
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     DATABASE_COLUMN_TYPES(),
                                     "UPDATE storages \
                                      SET entityId=%lld \
                                      WHERE id=%lld \
                                     ",
                                     entityDatabaseId,
                                     storageDatabaseId
                                    );
            }
          }
          Database_finalize(&databaseStatementHandle2);
        }

        error = Database_insert(&newIndexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "entities",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES
                                (
                                  STRING("jobUUID", uuid},
                                  UINT64("created", createdDateTime},
                                  UINT  ("type",    ARCHIVE_TYPE_FULL},
                                )
                               );
        if (error == ERROR_NONE)
        {
          // get entity id
          entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);
        }
      }
      Database_finalize(&databaseStatementHandle1);
    }

    return ERROR_NONE;
  });

  // free resources
  String_delete(name2);
  String_delete(name1);

  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done clean-up duplicate storages"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up duplicate storages failed (error: %s)",
                Error_getText(error)
               );
  }
}
#endif

/***********************************************************************\
* Name   : IndexStorage_getStorageState
* Purpose: get index storage state
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
*          errorMessage        - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

// TODO:
LOCAL Errors IndexStorage_getStorageState(IndexHandle *indexHandle,
                                    DatabaseId  storageId,
                                    IndexStates *indexState,
                                    uint64      *lastCheckedDateTime,
                                    String      errorMessage
                                   )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;

  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,UINT64,STRING),
                           "SELECT state, \
                                   UNIX_TIMESTAMP(lastChecked), \
                                   errorMessage \
                            FROM storages \
                            WHERE id=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (!Database_getNextRow(&databaseStatementHandle,
                           "%d %llu %S",
                           indexState,
                           lastCheckedDateTime,
                           errorMessage
                          )
     )
  {
    (*indexState) = INDEX_STATE_UNKNOWN;
    if (errorMessage != NULL) String_clear(errorMessage);
  }

  Database_finalize(&databaseStatementHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : clearStorageFragments
* Purpose: purge storage fragments
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageFragments(IndexHandle  *indexHandle,
                                   DatabaseId   storageId,
                                   ProgressInfo *progressInfo
                                  )
{
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;

  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  // purge fragments
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"entryFragments","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: fragments %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "entryFragments",
                                  "storageId=%lld",
                                  storageId
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
#ifndef NDEBUG
//fprintf(stderr,"%s, %d: 1 fragments done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));
#endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : clearStorageFTSEntries
* Purpose: purge storage FTS entries
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageFTSEntries(IndexHandle  *indexHandle,
                                    ProgressInfo *progressInfo,
                                    Array        *entryIds
                                   )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = IndexCommon_purge(indexHandle,
                                    &doneFlag,
                                    #ifndef NDEBUG
                                      &deletedCounter,
                                    #else
                                      NULL,  // deletedCounter
                                    #endif
                                    "FTS_entries",
                                    "entryId IN (%S)",
                                    entryIdsString
                                   );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged FTS entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageSubEntries
* Purpose: purge storage file/image/directory/link/hardlink/special sub
*          entries
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageSubEntries(IndexHandle  *indexHandle,
                                    DatabaseId   storageId,
                                    ProgressInfo *progressInfo,
                                    Array        *entryIds
                                   )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge file entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = IndexCommon_purge(indexHandle,
                                    &doneFlag,
                                    #ifndef NDEBUG
                                      &deletedCounter,
                                    #else
                                      NULL,  // deletedCounter
                                    #endif
                                    "fileEntries",
                                    "    entryId IN (%S) \
                                     AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=fileEntries.entryId LIMIT 0,1) \
                                    ",
                                    entryIdsString
                                   );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged file entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge image entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = IndexCommon_purge(indexHandle,
                                    &doneFlag,
                                    #ifndef NDEBUG
                                      &deletedCounter,
                                    #else
                                      NULL,  // deletedCounter
                                    #endif
                                    "imageEntries",
                                    "    entryId IN (%S) \
                                     AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=imageEntries.entryId LIMIT 0,1) \
                                    ",
                                    entryIdsString
                                   );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged image entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge directory entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"fileEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: directoryEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "directoryEntries",
                                  "storageId=%lld",
                                  storageId
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged directory entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge link entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"fileEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: linkEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "linkEntries",
                                  "storageId=%lld",
                                  storageId
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged link entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge hardlink entries without fragments
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = IndexCommon_purge(indexHandle,
                                    &doneFlag,
                                    #ifndef NDEBUG
                                      &deletedCounter,
                                    #else
                                      NULL,  // deletedCounter
                                    #endif
                                    "hardlinkEntries",
                                    "    entryId IN (%S) \
                                     AND NOT EXISTS(SELECT id FROM entryFragments WHERE entryFragments.entryId=hardlinkEntries.entryId LIMIT 0,1) \
                                    ",
                                    entryIdsString
                                   );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged hardlink entries without fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // purge special entries
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"specialEntries","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: specialEntries %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "specialEntries",
                                  "storageId=%lld",
                                  storageId
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged special entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageEntries
* Purpose: purge storage  entries
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageEntries(IndexHandle  *indexHandle,
                                 ProgressInfo *progressInfo,
                                 Array        *entryIds
                                )
{
  String               entryIdsString;
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  entryIdsString  = String_new();
  error           = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = IndexCommon_purge(indexHandle,
                                    &doneFlag,
                                    #ifndef NDEBUG
                                      &deletedCounter,
                                    #else
                                      NULL,  // deletedCounter
                                    #endif
                                    "entries",
                                    "id IN (%S)",
                                    entryIdsString
                                   );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return error;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  // free resources
  String_delete(entryIdsString);

  return error;
}

/***********************************************************************\
* Name   : clearStorageAggregates
* Purpose: clear storage aggregates
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
*          entryIds     - entry ids
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorageAggregates(IndexHandle  *indexHandle,
                                    DatabaseId   storageId,
                                    ProgressInfo *progressInfo
                                   )
{
  Errors               error;
  DatabaseId           entityId;
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    // clear storage aggregates
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_(NULL,NULL),  // databaseRowFunction
                              NULL,  // changedRowCount
                              DATABASE_COLUMN_TYPES(),
                              "UPDATE storages \
                               SET totalEntryCount          =0, \
                                   totalEntrySize           =0, \
                                   totalFileCount           =0, \
                                   totalFileSize            =0, \
                                   totalImageCount          =0, \
                                   totalImageSize           =0, \
                                   totalDirectoryCount      =0, \
                                   totalLinkCount           =0, \
                                   totalHardlinkCount       =0, \
                                   totalHardlinkSize        =0, \
                                   totalSpecialCount        =0, \
                                   \
                                   totalEntryCountNewest    =0, \
                                   totalEntrySizeNewest     =0, \
                                   totalFileCountNewest     =0, \
                                   totalFileSizeNewest      =0, \
                                   totalImageCountNewest    =0, \
                                   totalImageSizeNewest     =0, \
                                   totalDirectoryCountNewest=0, \
                                   totalLinkCountNewest     =0, \
                                   totalHardlinkCountNewest =0, \
                                   totalHardlinkSizeNewest  =0, \
                                   totalSpecialCountNewest  =0 \
                               WHERE id=%lld \
                              ",
                              storageId
                             );
    });
  }
  if (error == ERROR_NONE)
  {
    // update entity aggregates
    error = Database_getId(&indexHandle->databaseHandle,
                           &entityId,
                           "storages",
                           "entityId",
                           "WHERE id=%lld \
                           ",
                           storageId
                          );
    if (error == ERROR_NONE)
    {
      error = IndexEntity_updateAggregates(indexHandle,entityId);
    }
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, updated aggregates: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : clearStorage
* Purpose: clear index storage content
* Input  : indexHandle  - index handle
*          storageId    - database id of storage
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clearStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  Array                entryIds;
  //bool                 transactionFlag;
  Errors               error;
  //bool                 doneFlag;
  //ArraySegmentIterator arraySegmentIterator;
  //ArrayIterator        arrayIterator;
//  DatabaseId           entryId;
  //DatabaseId           entityId;
  #ifndef NDEBUG
    //ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // lock
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage #%"PRIi64"\n",__FILE__,__LINE__,storageId);
  #endif
  Semaphore_lock(&indexClearStorageLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage locked\n",__FILE__,__LINE__);
  #endif

  // init variables
  Array_init(&entryIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  error           = ERROR_NONE;

//TODO: do regulary in index thread?
#if 0
  /* get entries to purge without associated file/image/directory/link/hardlink/special entry
     Note: may be left from interrupted purge of previous run
  */
//l=0; Database_getInteger64(&indexHandle->databaseHandle,&l,"entries","count(id)",""); fprintf(stderr,"%s, %d: l=%lld\n",__FILE__,__LINE__,l);
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  Array_clear(&entryIds);
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                              WHERE entries.type=%u AND fileEntries.id IS NULL \
                             ",
                             INDEX_TYPE_FILE
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[0] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                              WHERE entries.type=%u AND imageEntries.id IS NULL \
                             ",
                             INDEX_TYPE_IMAGE
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[1] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                              WHERE entries.type=%u AND directoryEntries.id IS NULL \
                             ",
                             INDEX_TYPE_DIRECTORY
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[2] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                              WHERE entries.type=%u AND linkEntries.id IS NULL \
                             ",
                             INDEX_TYPE_LINK
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[3] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                              WHERE entries.type=%u AND hardlinkEntries.id IS NULL \
                             ",
                             INDEX_TYPE_HARDLINK
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[4] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             &entryIds,
                             "entries",
                             "entries.id",
                             "LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                              WHERE entries.type=%u AND specialEntries.id IS NULL \
                             ",
                             INDEX_TYPE_SPECIAL
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[5] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries without associated entry to purge: file %llums, image %llums, directory %llums, link %llums, hardlink %llums, special %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            Array_length(&entryIds),
            dt[0]/US_PER_MS,
            dt[1]/US_PER_MS,
            dt[2]/US_PER_MS,
            dt[3]/US_PER_MS,
            dt[4]/US_PER_MS,
            dt[5]/US_PER_MS
           );
  #endif

  /* purge entries without associated file/image/directory/link/hardlink/special entry
  */
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      ARRAY_SEGMENTX(&entryIds,arraySegmentIterator,SINGLE_STEP_PURGE_LIMIT,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(&entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_formatAppend(entryIdsString,"%lld",entryId);
        }

        do
        {
          doneFlag = TRUE;
          error = purge(indexHandle,
                        &doneFlag,
                        #ifndef NDEBUG
                          &deletedCounter,
                        #else
                          NULL,  // deletedCounter
                        #endif
                        "entries",
                        "id IN (%S)",
                        entryIdsString
                       );
          if (error == ERROR_NONE)
          {
            error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
          }
        }
        while ((error == ERROR_NONE) && !doneFlag);
      }

      return ERROR_NONE;
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged orphaned entries: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
#endif

  // collect file/image/diretory/link/hardlink/special entries to purge
  if (error == ERROR_NONE)
  {
    error = IndexEntry_collectIds(&entryIds,
                                       indexHandle,
                                       storageId,
                                       progressInfo
                                      );
  }

  // purge entry fragments
  if (error == ERROR_NONE)
  {
    error = clearStorageFragments(indexHandle,
                                  storageId,
                                  progressInfo
                                 );
  }

  // purge FTS entries
// TODO: do this wiht a trigger?
  switch (Database_getType(&indexHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      if (error == ERROR_NONE)
      {
        error = clearStorageFTSEntries(indexHandle,
                                       progressInfo,
                                       &entryIds
                                      );
      }
      break;
    case DATABASE_TYPE_MYSQL:
      break;
  }

  // purge file/image/directory/link/hardlink/special entries
  if (error == ERROR_NONE)
  {
    error = clearStorageSubEntries(indexHandle,
                                   storageId,
                                   progressInfo,
                                   &entryIds
                                  );
  }

  // purge entries
  if (error == ERROR_NONE)
  {
    error = clearStorageEntries(indexHandle,
                                progressInfo,
                                &entryIds
                               );
  }

  // remove from newest entries
  if (error == ERROR_NONE)
  {
    #ifdef INDEX_DEBUG_PURGE
      t0 = Misc_getTimestamp();
    #endif
    if (error == ERROR_NONE)
    {
      error = IndexStorage_removeFromNewest(indexHandle,
                                      storageId,
                                      progressInfo
                                     );
    }
    #ifdef INDEX_DEBUG_PURGE
      fprintf(stderr,"%s, %d: error: %s, removed from newest entries: %llums\n",__FILE__,__LINE__,
              Error_getText(error),
              (Misc_getTimestamp()-t0)/US_PER_MS
             );
    #endif
  }

  // clear aggregates
  if (error == ERROR_NONE)
  {
    error = clearStorageAggregates(indexHandle,
                                   storageId,
                                   progressInfo
                                  );
  }

  // unlock
  Semaphore_unlock(&indexClearStorageLock);
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: clear storage unlocked\n",__FILE__,__LINE__);
  #endif

  // free resources
  Array_done(&entryIds);

  return error;
}

/***********************************************************************\
* Name   : purgeStorage
* Purpose: purge storage (mark as "deleted")
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  String              name;
  String              string;
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  uint64              createdDateTime;
  bool                transactionFlag;
  bool                doneFlag;
  #ifndef NDEBUG
    ulong               deletedCounter;
  #endif

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  name   = String_new();
  string = String_new();

  // get storage name, created date/time
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(STRING,UINT64),
                           "SELECT storages.name, \
                                   UNIX_TIMESTAMP(entities.created) \
                            FROM storages \
                              LEFT JOIN entities ON entities.id=storages.entityId \
                            WHERE storages.id=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY(storageId)
                           )
                          );
  if (error == ERROR_NONE)
  {
    if (!Database_getNextRow(&databaseStatementHandle,
                             "%'S %llu",
                             name,
                             &createdDateTime
                            )
       )
    {
      error = ERRORX_(DATABASE,0,"prune storages");
    }
    Database_finalize(&databaseStatementHandle);
  }
  if (error != ERROR_NONE)
  {
    String_clear(name);
    createdDateTime = 0LL;
  }

  // clear storage
  if (error == ERROR_NONE)
  {
    error = clearStorage(indexHandle,storageId,progressInfo);
  }

  // purge FTS storages
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "FTS_storages",
                                  "storageId=%lld",
                                  storageId
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }

  // delete storage
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM storages \
                              WHERE id=%lld \
                             ",
                             storageId
                            );
  }

  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(name);
    return error;
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Removed deleted storage #%llu from index: %s, created at %s",
              storageId,
              String_cString(name),
              (createdDateTime != 0LL) ? String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,NULL)) : "unknown"
             );

  // free resources
  String_delete(string);
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneStorage
* Purpose: prune storage if empty, prune entity/UUID
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorage(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
                         )
{
  String              name;
  String              string;
  IndexStates         indexState;
  Errors              error;
  DatabaseId          entityId;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  name   = String_new();
  string = String_new();

  // get storage state
  error = IndexStorage_getStorageState(indexHandle,
                          storageId,
                          &indexState,
                          NULL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(name);
    return error;
  }

  // prune storage if not in error state/in use and empty
  if ((indexState == INDEX_STATE_OK) && IndexStorage_isEmpty(indexHandle,storageId))
  {
    // get entity id
    error = Database_getId(&indexHandle->databaseHandle,
                           &entityId,
                           "storages",
                           "entityId",
                           "WHERE id=%lld",
                           storageId
                          );
    if (error != ERROR_NONE)
    {
      entityId = DATABASE_ID_NONE;
    }

    // purge storage
    error = purgeStorage(indexHandle,storageId,progressInfo);
    if (error != ERROR_NONE)
    {
      String_delete(string);
      String_delete(name);
      return error;
    }

    // prune entity
    if (entityId != DATABASE_ID_NONE)
    {
      error = IndexEntity_prune(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        String_delete(name);
        return error;
      }
    }
  }

  // free resources
  String_delete(string);
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pruneStorages
* Purpose: prune all empty storages which have state OK or ERROR
* Input  : indexHandle  - index handle
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorages(IndexHandle  *indexHandle,
                           ProgressInfo *progressInfo
                          )
{
  Array         storageIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    storageId;

  assert(indexHandle != NULL);

  // init variables
  Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all storage ids
  error = Database_getIds(&indexHandle->databaseHandle,
                          &storageIds,
                          "storages",
                          "id",
                          "WHERE state IN (%u,%u) \
                          ",
                          INDEX_STATE_OK,
                          INDEX_STATE_ERROR
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    return error;
  }

  // prune storages
  ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
  {
    error = pruneStorage(indexHandle,storageId,progressInfo);
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

#if 0
//TODO: not used, remove
/***********************************************************************\
* Name   : refreshStoragesInfos
* Purpose: refresh storages info
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshStoragesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          indexId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount,
                                 INDEX_TYPE_SET_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // hostName
                                 NULL,  // userName
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // entityId
                                NULL,  // scheduleUUID
                                NULL,  // hostName
                                NULL,  // userName
                                NULL,  // comment
                                NULL,  // createdDateTime
                                NULL,  // archiveType
                                &storageId,
                                NULL,  // storageName,
                                NULL,  // dateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      error = Index_updateStorageInfos(indexHandle,indexId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}
#endif

// ----------------------------------------------------------------------

bool IndexStorage_isEmpty(IndexHandle *indexHandle,
                                 DatabaseId  storageId
                                )
{
  assert(indexHandle != NULL);

  return    (storageId != DATABASE_ID_NONE)
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "entryFragments",
                                   "id",
                                   "WHERE storageId=%lld",
                                   storageId
                                  )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "directoryEntries",
                                  "id",
                                  "WHERE storageId=%lld",
                                  storageId
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "linkEntries",
                                  "id",
                                  "WHERE storageId=%lld",
                                  storageId
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "specialEntries",
                                  "id",
                                  "WHERE storageId=%lld",
                                  storageId
                                 );
}

Errors IndexStorage_addToNewest(IndexHandle  *indexHandle,
                                       DatabaseId   storageId,
                                       ProgressInfo *progressInfo
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

  EntryList           entryList;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entryId;
  DatabaseId          uuidId;
  DatabaseId          entityId;
  uint                indexType;
  String              entryName;
  uint64              timeLastChanged;
  uint32              userId;
  uint32              groupId;
  uint32              permission;
  uint64              size;
  Errors              error;
  EntryNode           *entryNode;
  bool                transactionFlag;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  entryName = String_new();
  error     = ERROR_NONE;

  // get entries info to add
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(KEY,KEY,KEY,INT,STRING,UINT64,INT,INT,INT,INT64),
                               "      SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM entryFragments \
                                        LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                      WHERE entryFragments.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM directoryEntries \
                                        LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                      WHERE directoryEntries.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM linkEntries \
                                        LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                      WHERE linkEntries.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.uuidId, \
                                             entries.entityId, \
                                             entries.type, \
                                             entries.name, \
                                             UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
                                             entries.userId, \
                                             entries.groupId, \
                                             entries.permission, \
                                             entries.size \
                                      FROM specialEntries \
                                        LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                      WHERE specialEntries.storageId=? \
                                GROUP BY entries.name \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                                 KEY(storageId),
                                 KEY(storageId),
                                 KEY(storageId),
                                 KEY(storageId)
                               )
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      while (Database_getNextRow(&databaseStatementHandle,
                                 "%lld %lld %lld %u %S %llu %u %u %u %llu",
                                 &entryId,
                                 &uuidId,
                                 &entityId,
                                 &indexType,
                                 entryName,
                                 &timeLastChanged,
                                 &userId,
                                 &groupId,
                                 &permission,
                                 &size
                                )
            )
      {
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
      }

      Database_finalize(&databaseStatementHandle);

      return ERROR_NONE;
    });
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // find newest entries for entries to add
//fprintf(stderr,"%s, %d: find newest entries for entries to add %d\n",__FILE__,__LINE__,List_count(&entryList));
  IndexCommon_resetProgress(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
//fprintf(stderr,"a");
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_execute(&indexHandle->databaseHandle,
                              CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                              {
                                assert(values != NULL);
                                assert(valueCount == 2);

                                UNUSED_VARIABLE(userData);

                                entryNode->newest.entryId         = values[0].id;
                                entryNode->newest.timeLastChanged = values[1].dateTime;

                                return ERROR_NONE;
                              },NULL),
                              NULL,  // changedRowCount
                              DATABASE_COLUMN_TYPES(KEY,UINT64),
                              "      SELECT entriesNewest.id, \
                                            UNIX_TIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM entryFragments \
                                       LEFT JOIN storages ON storages.id=entryFragments.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=entryFragments.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIX_TIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM directoryEntries \
                                       LEFT JOIN storages ON storages.id=directoryEntries.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=directoryEntries.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIX_TIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
                                     FROM linkEntries \
                                       LEFT JOIN storages ON storages.id=linkEntries.storageId \
                                       LEFT JOIN entriesNewest ON entriesNewest.id=linkEntries.entryId \
                                     WHERE     storages.deletedFlag!=1 \
                                           AND entriesNewest.name=%'S \
                               UNION SELECT entriesNewest.id, \
                                            UNIX_TIMESTAMP(entriesNewest.timeLastChanged) AS timeLastChanged \
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
    });
    IndexCommon_progressStep(progressInfo);
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // add entries to newest entries
//fprintf(stderr,"%s, %d: add entries to newest entries %d\n",__FILE__,__LINE__,List_count(&entryList));
  INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
//fprintf(stderr,"b");
      if (entryNode->timeLastChanged > entryNode->newest.timeLastChanged)
      {
        INDEX_DOX(error,
                  indexHandle,
        {
          return Database_insert(&indexHandle->databaseHandle,
                                 NULL,  // changedRowCount
                                 "entriesNewest",
                                 DATABASE_FLAG_REPLACE,
                                 DATABASE_VALUES2
                                 (
                                   KEY   ("entryId",         entryNode->entryId),
                                   KEY   ("uuidId",          entryNode->uuidId),
                                   KEY   ("entityId",        entryNode->entityId),
                                   UINT  ("type",            entryNode->indexType),
                                   STRING("name",            entryNode->name),
                                   UINT64("timeLastChanged", entryNode->timeLastChanged),
                                   UINT  ("userId",          entryNode->userId),
                                   UINT  ("groupId",         entryNode->groupId),
                                   UINT  ("permission",      entryNode->permission),
                                   UINT64("size",            entryNode->size)
                                 )
                                );
        });
      }

#if 1
      if (error == ERROR_NONE)
      {
        error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,5LL*MS_PER_SECOND);
      }
#endif
    }

    return error;
  });
//fprintf(stderr,"%s, %d: add entries to newest entries %d done\n",__FILE__,__LINE__,List_count(&entryList));
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // free resources
  String_delete(entryName);
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
}

Errors IndexStorage_removeFromNewest(IndexHandle  *indexHandle,
                                            DatabaseId   storageId,
                                            ProgressInfo *progressInfo
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

  EntryList           entryList;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          entryId;
  String              entryName;
  Errors              error;
  EntryNode           *entryNode;
  bool                transactionFlag;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // init variables
  List_init(&entryList);
  entryName = String_new();
  error     = ERROR_NONE;

  // get entries info to remove
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(KEY,STRING),
                               "      SELECT entries.id, \
                                             entries.name \
                                      FROM entryFragments \
                                        LEFT JOIN entries ON entries.id=entryFragments.entryId \
                                      WHERE entryFragments.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM directoryEntries \
                                        LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                                      WHERE directoryEntries.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM linkEntries \
                                        LEFT JOIN entries ON entries.id=linkEntries.entryId \
                                      WHERE linkEntries.storageId=? \
                                UNION SELECT entries.id, \
                                             entries.name \
                                      FROM specialEntries \
                                        LEFT JOIN entries ON entries.id=specialEntries.entryId \
                                      WHERE specialEntries.storageId=? \
                                ORDER BY entries.name \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                                 KEY(storageId),
                                 KEY(storageId),
                                 KEY(storageId),
                                 KEY(storageId)
                               )
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      while (Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S",
                                 &entryId,
                                 entryName
                                )
            )
      {
        entryNode = LIST_NEW_NODE(EntryNode);
        if (entryNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }

        entryNode->entryId        = entryId;
        entryNode->name           = String_duplicate(entryName);
        entryNode->newest.entryId = DATABASE_ID_NONE;

        List_append(&entryList,entryNode);
      }

      Database_finalize(&databaseStatementHandle);

      return ERROR_NONE;
    });
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // find new newest entries for entries to remove
//fprintf(stderr,"%s, %d: find new newest entries for entries to remove %d\n",__FILE__,__LINE__,List_count(&entryList));
  IndexCommon_resetProgress(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
//fprintf(stderr,"c");
    // wait until index is unused
    WAIT_NOT_IN_USE(5LL*MS_PER_SECOND);

    if ((entryNode->prev == NULL) || !String_equals(entryNode->prev->name,entryNode->name))
    {
      INDEX_DOX(error,
                indexHandle,
      {
        return Database_execute(&indexHandle->databaseHandle,
                                CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                {
                                  assert(values != NULL);
                                  assert(valueCount == 9);

                                  UNUSED_VARIABLE(userData);

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
                                DATABASE_COLUMN_TYPES(KEY,KEY,KEY,INT,UINT64,INT,INT,INT,INT64),
                                "      SELECT entries.id, \
                                              entries.uuidId, \
                                              entries.entityId, \
                                              entries.type, \
                                              UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
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
                                              UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
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
                                              UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
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
                                              UNIX_TIMESTAMP(entries.timeLastChanged) AS timeLastChanged, \
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
        if (error != ERROR_NONE)
        {
          return error;
        }

        while (Database_getNextRow(&databaseStatementHandle,
                                   "%lld %S",
                                   &entryId,
                                   entryName
                                  )
              )
        {
          entryNode = LIST_NEW_NODE(EntryNode);
          if (entryNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          entryNode->entryId        = entryId;
          entryNode->name           = String_duplicate(entryName);
          entryNode->newest.entryId = DATABASE_ID_NONE;

          List_append(&entryList,entryNode);
        }

        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });
    }
    IndexCommon_progressStep(progressInfo);
  }
//fprintf(stderr,"%s, %d: find new newest entries for entries to remove %d done\n",__FILE__,__LINE__,List_count(&entryList));
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // remove/update entries from newest entries
//fprintf(stderr,"%s, %d: remove/update entries from newest entries %d\n",__FILE__,__LINE__,List_count(&entryList));
  INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
//fprintf(stderr,"d");
      INDEX_DOX(error,
                indexHandle,
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM entriesNewest \
                                  WHERE entryId=%lld \
                                 ",
                                 entryNode->entryId
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (entryNode->newest.entryId != DATABASE_ID_NONE)
        {
          error = Database_execute(&indexHandle->databaseHandle,
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
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        return ERROR_NONE;
      });

#if 1
      if (error == ERROR_NONE)
      {
        error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,5LL*MS_PER_SECOND);
      }
#endif
    }

    return error;
  });
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
    return error;
  }

  // free resources
  String_delete(entryName);
  List_done(&entryList,(ListNodeFreeFunction)freeEntryNode,NULL);

  return ERROR_NONE;
}

Errors IndexStorage_updateAggregates(IndexHandle *indexHandle,
                                            DatabaseId  storageId
                                           )
{
  Errors                  error;
  DatabaseStatementHandle databaseStatementHandle;
  ulong                   totalFileCount;
  double                  totalFileSize_;
  uint64                  totalFileSize;
  ulong                   totalImageCount;
  double                  totalImageSize_;
  uint64                  totalImageSize;
  ulong                   totalDirectoryCount;
  ulong                   totalLinkCount;
  ulong                   totalHardlinkCount;
  double                  totalHardlinkSize_;
  uint64                  totalHardlinkSize;
  ulong                   totalSpecialCount;
  DatabaseId              entityId;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  // get file aggregate data
#if 0
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entries.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                             UINT(INDEX_TYPE_FILE),
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalFileCount = 0L;
    totalFileSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);
#else
  error = Database_select(&indexHandle->databaseHandle,
                          CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                          {
                            assert(values != NULL);
                            assert(valueCount == 2);

                            UNUSED_VARIABLE(userData);

                            totalFileCount = values[0].u;
                            totalFileSize_ = values[1].u64;

                            return ERROR_NONE;
                          },NULL),
                          NULL,  // changedRowCount
                          "entryFragments \
                             LEFT JOIN entries ON entries.id=entryFragments.entryId \
                          ",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            { "COUNT(DISTINCT entries.id)", DATABASE_DATATYPE_INT, {(intptr_t)0} },
                            { "SUM(entryFragments.size)",   DATABASE_DATATYPE_INT, {(intptr_t)0} }
                          ),
                          "    entryFragments.storageId=? \
                           AND entries.type=? \
                          ",
                          DATABASE_VALUES
                          (
                            { "entryFragments.storageId", DATABASE_DATATYPE_KEY,  {storageId }},
                            { "entries.type",             DATABASE_DATATYPE_UINT, {(intptr_t)INDEX_TYPE_FILE }}
                          )
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  // get image aggregate data
#if 0
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entries.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId),
                             UINT(INDEX_TYPE_IMAGE)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);
#else
  error = Database_select(&indexHandle->databaseHandle,
                          CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                          {
                            assert(values != NULL);
                            assert(valueCount == 2);

                            UNUSED_VARIABLE(userData);

                            totalImageCount = values[0].u;
                            totalImageSize_ = values[1].u64;

                            return ERROR_NONE;
                          },NULL),
                          NULL,  // changedRowCount
                          "entryFragments \
                             LEFT JOIN entries ON entries.id=entryFragments.entryId \
                          ",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            { "COUNT(DISTINCT entries.id)", DATABASE_DATATYPE_INT, {(intptr_t)0} },
                            { "SUM(entryFragments.size)",   DATABASE_DATATYPE_INT, {(intptr_t)0} }
                          ),
                          "    entryFragments.storageId=? \
                           AND entries.type=? \
                          ",
                          DATABASE_VALUES
                          (
                            { "entryFragments.storageId", DATABASE_DATATYPE_KEY,  {storageId }},
                            { "entries.type",             DATABASE_DATATYPE_UINT, {(intptr_t)INDEX_TYPE_IMAGE }}
                          )
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  // get directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM directoryEntries \
                              LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                            WHERE directoryEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalDirectoryCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM linkEntries \
                              LEFT JOIN entries ON entries.id=linkEntries.entryId \
                            WHERE linkEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalLinkCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entries.size?
                           "SELECT COUNT(DISTINCT entries.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entries ON entries.id=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entries.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId),
                             UINT(INDEX_TYPE_HARDLINK)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalHardlinkCount = 0L;
    totalHardlinkSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entries.id) \
                            FROM specialEntries \
                              LEFT JOIN entries ON entries.id=specialEntries.entryId \
                            WHERE specialEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalSpecialCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // update aggregate data
fprintf(stderr,"%s:%d: totalFileCount=%lu\n",__FILE__,__LINE__,totalFileCount);
fprintf(stderr,"%s:%d: totalDirectoryCount=%lu\n",__FILE__,__LINE__,totalDirectoryCount);
fprintf(stderr,"%s:%d: totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount=%lu\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount);
  error = Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES2
                          (
                            UINT  ("totalEntryCount",     totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount),
                            UINT64("totalEntrySize",      totalFileSize+totalImageSize+totalHardlinkSize),
                            UINT  ("totalFileCount",      totalFileCount),
                            UINT64("totalFileSize",       totalFileSize),
                            UINT  ("totalImageCount",     totalImageCount),
                            UINT64("totalImageSize",      totalImageSize),
                            UINT  ("totalDirectoryCount", totalDirectoryCount),
                            UINT  ("totalLinkCount",      totalLinkCount),
                            UINT  ("totalHardlinkCount",  totalHardlinkCount),
                            UINT64("totalHardlinkSize",   totalHardlinkSize),
                            UINT  ("totalSpecialCount",   totalSpecialCount)
                          ),
                          "id=?",
                          DATABASE_FILTERS
                          (
                            KEY(storageId)
                          )
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // -----------------------------------------------------------------

  // get newest file aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entriesNewest.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId),
                             UINT(INDEX_TYPE_FILE)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalFileCount,
                           &totalFileSize_
                          )
     )
  {
    totalFileCount = 0L;
    totalFileSize_ = 0.0;
  }
  assert(totalFileSize_ >= 0.0);
  totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest image aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entriesNewest.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId),
                             UINT(INDEX_TYPE_IMAGE)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalImageCount,
                           &totalImageSize_
                          )
     )
  {
    totalImageCount = 0L;
    totalImageSize_ = 0.0;
  }
  assert(totalImageSize_ >= 0.0);
  totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest directory aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM directoryEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=directoryEntries.entryId \
                            WHERE directoryEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalDirectoryCount
                          )
     )
  {
    totalDirectoryCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest link aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM linkEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=linkEntries.entryId \
                            WHERE linkEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalLinkCount
                          )
     )
  {
    totalLinkCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // get newest hardlink aggregate data
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT,INT64),
//TODO: use entriesNewest.size?
                           "SELECT COUNT(DISTINCT entriesNewest.id), \
                                   SUM(entryFragments.size) \
                            FROM entryFragments \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=entryFragments.entryId \
                            WHERE     entryFragments.storageId=? \
                                  AND entriesNewest.type=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId),
                             UINT(INDEX_TYPE_HARDLINK)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu %lf",
                           &totalHardlinkCount,
                           &totalHardlinkSize_
                          )
     )
  {
    totalHardlinkCount = 0L;
    totalHardlinkSize_ = 0.0;
  }
  assert(totalHardlinkSize_ >= 0.0);
  totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
  Database_finalize(&databaseStatementHandle);

  // get newest special aggregate data (Note: do not filter by entries.type -> not required and it is slow!)
  error = Database_prepare(&databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           DATABASE_COLUMN_TYPES(INT),
                           "SELECT COUNT(DISTINCT entriesNewest.id) \
                            FROM specialEntries \
                              LEFT JOIN entriesNewest ON entriesNewest.entryId=specialEntries.entryId \
                            WHERE specialEntries.storageId=? \
                           ",
                           DATABASE_VALUES2
                           (
                           ),
                           DATABASE_FILTERS
                           (
                             KEY (storageId)
                           )
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseStatementHandle,
                           "%lu",
                           &totalSpecialCount
                          )
     )
  {
    totalSpecialCount = 0L;
  }
  Database_finalize(&databaseStatementHandle);

  // update newest aggregate data
// TODO:
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK_(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           DATABASE_COLUMN_TYPES(),
                           "UPDATE storages \
                            SET totalEntryCountNewest    =%llu, \
                                totalEntrySizeNewest     =%llu, \
                                totalFileCountNewest     =%llu, \
                                totalFileSizeNewest      =%llu, \
                                totalImageCountNewest    =%llu, \
                                totalImageSizeNewest     =%llu, \
                                totalDirectoryCountNewest=%llu, \
                                totalLinkCountNewest     =%llu, \
                                totalHardlinkCountNewest =%llu, \
                                totalHardlinkSizeNewest  =%llu, \
                                totalSpecialCountNewest  =%llu \
                            WHERE id=%lld \
                           ",
                           totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                           totalFileSize+totalImageSize+totalHardlinkSize,
                           totalFileCount,
                           totalFileSize,
                           totalImageCount,
                           totalImageSize,
                           totalDirectoryCount,
                           totalLinkCount,
                           totalHardlinkCount,
                           totalHardlinkSize,
                           totalSpecialCount,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // update entity aggregates
  error = Database_getId(&indexHandle->databaseHandle,
                         &entityId,
                         "storages",
                         "entityId",
                         "WHERE id=%lld \
                         ",
                         storageId
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = IndexEntity_updateAggregates(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

bool Index_findStorageById(IndexHandle *indexHandle,
                           IndexId     findStorageId,
                           String      jobUUID,
                           String      scheduleUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           uint64      *dateTime,
                           uint64      *size,
                           IndexStates *indexState,
                           IndexModes  *indexMode,
                           uint64      *lastCheckedDateTime,
                           String      errorMessage,
                           ulong       *totalEntryCount,
                           uint64      *totalEntrySize
                          )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  bool                result;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);
  assert(Index_getType(findStorageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,STRING,UINT64,INT64,INT,INT,UINT64,STRING,INT,INT64),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIX_TIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIX_TIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON entities.jobUUID=uuids.jobUUID \
                              WHERE     storages.deletedFlag!=1 \
                                    AND storages.id=? \
                              GROUP BY storages.id \
                              LIMIT 0,1 \
                             ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                               KEY(Index_getDatabaseId(findStorageId))
                             )
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseStatementHandle);

    result = Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 dateTime,
                                 size,
                                 indexState,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_UUID  (uuidDatabaseId  );
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityDatabaseId);

  return result;
}

bool Index_findStorageByName(IndexHandle            *indexHandle,
                             const StorageSpecifier *findStorageSpecifier,
                             ConstString            findArchiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 scheduleUUID,
                             IndexId                *storageId,
                             uint64                 *dateTime,
                             uint64                 *size,
                             IndexStates            *indexState,
                             IndexModes             *indexMode,
                             uint64                 *lastCheckedDateTime,
                             String                 errorMessage,
                             ulong                  *totalEntryCount,
                             uint64                 *totalEntrySize
                            )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;
  DatabaseId          uuidDatabaseId,entityDatabaseId,storageDatabaseId;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = INDEX_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
//TODO: optimize: search for part of name?
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,STRING,KEY,KEY,STRING,STRING,UINT64,INT64,INT,INT,UINT64,STRING,INT,INT64),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storages.id, \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIX_TIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIX_TIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE storages.deletedFlag!=1 \
                              GROUP BY storages.id \
                             ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseStatementHandle);

    storageName = String_new();
    Storage_initSpecifier(&storageSpecifier);
    foundFlag   = FALSE;
    while (   !foundFlag
           && Database_getNextRow(&databaseStatementHandle,
                                  "%lld %S %lld %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                  &uuidDatabaseId,
                                  jobUUID,
                                  &entityDatabaseId,
                                  &storageDatabaseId,
                                  scheduleUUID,
                                  storageName,
                                  dateTime,
                                  size,
                                  indexState,
                                  indexMode,
                                  lastCheckedDateTime,
                                  errorMessage,
                                  totalEntryCount,
                                  totalEntrySize
                                 )
          )
    {
      if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
      {
        foundFlag = Storage_equalSpecifiers(findStorageSpecifier,findArchiveName,
                                            &storageSpecifier,NULL
                                           );
        if (foundFlag)
        {
          if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );
          if (entityId  != NULL) (*entityId ) = INDEX_ID_ENTITY (entityDatabaseId );
          if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageDatabaseId);
        }
      }
    }
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  return foundFlag;
}

bool Index_findStorageByState(IndexHandle   *indexHandle,
                              IndexStateSet findIndexStateSet,
                              IndexId       *uuidId,
                              String        jobUUID,
                              IndexId       *entityId,
                              String        scheduleUUID,
                              IndexId       *storageId,
                              String        storageName,
                              uint64        *dateTime,
                              uint64        *size,
                              IndexModes    *indexMode,
                              uint64        *lastCheckedDateTime,
                              String        errorMessage,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
  Errors              error;
  String              indexStateSetString;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidDatabaseId,entityDatabaseId,storageDatabaseId;
  bool                result;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  indexStateSetString = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,STRING,KEY,KEY,STRING,STRING,UINT64,INT64,INT,UINT64,STRING,INT,INT64),
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storages.id, \
                                     entities.scheduleUUID, \
                                     storages.name, \
                                     UNIX_TIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.mode, \
                                     UNIX_TIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON storages.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE     storages.deletedFlag!=1 \
                                    AND (storages.state IN (?)) \
                              LIMIT 0,1 \
                             ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                               STRING(IndexCommon_getIndexStateSetString(indexStateSetString,findIndexStateSet))
                             )
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseStatementHandle,
                                 "%lld %S %lld %lld %S %S %llu %llu %d %llu %S %llu %llu",
                                 &uuidDatabaseId,
                                 jobUUID,
                                 &entityDatabaseId,
                                 &storageDatabaseId,
                                 scheduleUUID,
                                 storageName,
                                 dateTime,
                                 size,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }

  if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_UUID   (uuidDatabaseId   );
  if (entityId  != NULL) (*entityId ) = INDEX_ID_ENTITY (entityDatabaseId );
  if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageDatabaseId);

  // free resources
  String_delete(indexStateSetString);

  return result;
}

Errors Index_getStorageState(IndexHandle *indexHandle,
                             IndexId     storageId,
                             IndexStates *indexState,
                             uint64      *lastCheckedDateTime,
                             String      errorMessage
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    return IndexStorage_getStorageState(indexHandle,
                                        Index_getDatabaseId(storageId),
                                        indexState,
                                        lastCheckedDateTime,
                                        errorMessage
                                       );
  });

  return error;
}

Errors Index_setStorageState(IndexHandle *indexHandle,
                             IndexId     indexId,
                             IndexStates indexState,
                             uint64      lastCheckedDateTime,
                             const char  *errorMessage,
                             ...
                            )
{
  Errors  error;
  va_list arguments;
  String  errorText;

  assert(indexHandle != NULL);
  assert((Index_getType(indexId) == INDEX_TYPE_ENTITY) || (Index_getType(indexId) == INDEX_TYPE_STORAGE));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // format error message (if any)
  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    errorText = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);
  }
  else
  {
    errorText = NULL;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      switch (Index_getType(indexId))
      {
        case INDEX_TYPE_ENTITY:
          error = Database_update(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES2
                                  (
                                    UINT   ("state",        indexState),
                                    CSTRING("errorMessage", "")
                                  ),
                                  "entityId=?",
                                  DATABASE_FILTERS
                                  (
                                    KEY(Index_getDatabaseId(indexId))
                                  )
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      UINT64("lastChecked", lastCheckedDateTime)
                                    ),
                                    "entityId=?",
                                    DATABASE_FILTERS
                                    (
                                      KEY(Index_getDatabaseId(indexId))
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      STRING("errorMessage", errorText)
                                    ),
                                    "entityId=?",
                                    DATABASE_FILTERS
                                    (
                                      KEY(Index_getDatabaseId(indexId))
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        case INDEX_TYPE_STORAGE:
          error = Database_update(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES2
                                  (
                                    UINT   ("state",        indexState),
                                    CSTRING("errorMessage", "")
                                  ),
                                  "id=?",
                                  DATABASE_FILTERS
                                  (
                                    KEY(Index_getDatabaseId(indexId)),
                                  )
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      UINT64("lastChecked", lastCheckedDateTime)
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      KEY(Index_getDatabaseId(indexId)),
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      STRING("errorMessage", errorText)
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      KEY(Index_getDatabaseId(indexId)),
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* not NDEBUG */
          break;
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
                                    "INDEX_SET_STATE indexId=%lld indexState=%'s lastCheckedDateTime=%llu errorMessage=%'S",
                                    indexId,
                                    Index_stateToString(indexState,NULL),
                                    lastCheckedDateTime,
                                    errorText
                                   );
  }
  if (error != ERROR_NONE)
  {
    if (errorMessage != NULL) String_delete(errorText);
    return error;
  }

  // free resources
  if (errorMessage != NULL) String_delete(errorText);

  return ERROR_NONE;
}

long Index_countStorageState(IndexHandle *indexHandle,
                             IndexStates indexState
                            )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  long                count;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  INDEX_DOX(count,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(INT),
                             "SELECT COUNT(id) \
                              FROM storages \
                              WHERE state=? \
                             ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                               UINT(indexState)
                             )
                            );
    if (error != ERROR_NONE)
    {
      return -1L;
    }

    if (!Database_getNextRow(&databaseStatementHandle,
                             "%ld",
                             &count
                            )
       )
    {
      Database_finalize(&databaseStatementHandle);
      return -1L;
    }

    Database_finalize(&databaseStatementHandle);

    return count;
  });

  return count;
}

Errors Index_getStoragesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
                              ConstString   jobUUID,
                              ConstString   scheduleUUID,
                              const IndexId indexIds[],
                              ulong         indexIdCount,
                              IndexTypeSet  indexTypeSet,
                              IndexStateSet indexStateSet,
                              IndexModeSet  indexModeSet,
                              ConstString   name,
                              ulong         *totalStorageCount,
                              uint64        *totalStorageSize,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize,
                              uint64        *totalEntryContentSize
                             )
{
  String              ftsName;
  String              filterString;
  String              uuidIdsString,entityIdsString,storageIdsString;
  ulong               i;
  String              filterIdsString;
  String              string;
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  double              totalStorageSize_,totalEntryCount_,totalEntrySize_,totalEntryContentSize_;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64              t0,t1;
  #endif /* INDEX_DEBUG_LIST_INFO */

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_NONE(entityId) || INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0L) || (indexIds != NULL));

  // init variables
  if (totalStorageCount     != NULL) (*totalStorageCount    ) = 0L;
  if (totalStorageSize      != NULL) (*totalStorageSize     ) = 0LL;
  if (totalEntryCount       != NULL) (*totalEntryCount      ) = 0L;
  if (totalEntrySize        != NULL) (*totalEntrySize       ) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filterIdsString = String_new();
  string          = String_new();
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  IndexCommon_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","entity.uuidId=%lld",Index_getDatabaseId(uuidId));
  IndexCommon_filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  IndexCommon_filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  IndexCommon_filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  IndexCommon_filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  IndexCommon_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_getStoragesInfos ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: indexTypeSet=%s%s%s\n",__FILE__,__LINE__,
            IN_SET(indexTypeSet,INDEX_TYPE_UUID) ? " UUID" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) ? " entity" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) ? " storage" : ""
           );
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));

    t0 = Misc_getTimestamp();
  #endif /* INDEX_DEBUG_LIST_INFO */

  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    // get storage count, storage size, entry count, entry size
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,INT64,INT,INT64),
//TODO newest
                             stringFormat(sqlCommand,sizeof(sqlCommand),
                                          "SELECT COUNT(storages.id), \
                                                  SUM(storages.size), \
                                                  SUM(storages.totalEntryCount), \
                                                  SUM(storages.totalEntrySize) \
                                           FROM storages \
                                             LEFT JOIN entities ON entities.id=storages.entityId \
                                             LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                           WHERE     storages.deletedFlag!=1 \
                                                 AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    #ifdef INDEX_DEBUG_LIST_INFO
      Database_debugPrintQueryInfo(&databaseStatementHandle);
    #endif
    if (Database_getNextRow(&databaseStatementHandle,
                            "%lu %lf %lf %lf",
                            totalStorageCount,
                            &totalStorageSize_,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
      if (totalStorageSize != NULL) (*totalStorageSize) = (totalStorageSize_ >= 0.0) ? (uint64)totalStorageSize_ : 0LL;
//TODO: may happen?
//      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount  != NULL) (*totalEntryCount ) = (totalEntryCount_  >= 0.0) ? (ulong)totalEntryCount_   : 0L;
      if (totalEntrySize   != NULL) (*totalEntrySize  ) = (totalEntrySize_   >= 0.0) ? (uint64)totalEntrySize_   : 0LL;
    }
    Database_finalize(&databaseStatementHandle);

    if (totalEntryContentSize != NULL)
    {
      // get entry content size
      if      (!String_isEmpty(uuidIdsString))
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT64),
//TODO newest
                                 stringFormat(sqlCommand,sizeof(sqlCommand),
                                              "SELECT SUM(directoryEntries.totalEntrySize) \
                                               FROM storages \
                                                 LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                                 LEFT JOIN entities ON entities.id=storages.entityId \
                                                 LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                               WHERE %s \
                                              ",
                                              String_cString(filterString)
                                             ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                 )
                                );
      }
      else if (   !String_isEmpty(entityIdsString)
               || !INDEX_ID_IS_ANY(uuidId)
               || !INDEX_ID_IS_ANY(entityId)
               || (jobUUID != NULL)
               || (scheduleUUID != NULL)
              )
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT),
//TODO newest
                                 stringFormat(sqlCommand,sizeof(sqlCommand),
                                              "SELECT SUM(directoryEntries.totalEntrySize) \
                                               FROM storages \
                                                 LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                                 LEFT JOIN entities         ON entities.id=storages.entityId \
                                               WHERE %s \
                                              ",
                                              String_cString(filterString)
                                             ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                 )
                                );
      }
      else
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        error = Database_prepare(&databaseStatementHandle,
                                 &indexHandle->databaseHandle,
                                 DATABASE_COLUMN_TYPES(INT64),
//TODO newest
                                 stringFormat(sqlCommand,sizeof(sqlCommand),
                                              "SELECT SUM(directoryEntries.totalEntrySize) \
                                               FROM storages \
                                                 LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                               WHERE %s \
                                              ",
                                              String_cString(filterString)
                                             ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                 )
                                );
      }
      #ifdef INDEX_DEBUG_LIST_INFO
        Database_debugPrintQueryInfo(&databaseStatementHandle);
      #endif
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (Database_getNextRow(&databaseStatementHandle,
                              "%lf",
                              &totalEntryContentSize_
                             )
            )
      {
//TODO: may happen?
//      assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (uint64)totalEntryContentSize_ : 0LL;
      }
      Database_finalize(&databaseStatementHandle);
    }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(filterString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    t1 = Misc_getTimestamp();
    fprintf(stderr,"%s, %d: totalStorageCount=%lu totalStorageSize=%lf totalEntryCount_=%lf totalEntrySize_=%lf\n",__FILE__,__LINE__,*totalStorageCount,totalStorageSize_,totalEntryCount_,totalEntrySize_);
    fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // free resources
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(filterString);
  String_delete(ftsName);

  return ERROR_NONE;
}

Errors Index_updateStorageInfos(IndexHandle *indexHandle,
                                IndexId     storageId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexStorage_updateAggregates(indexHandle,Index_getDatabaseId(storageId));
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_UPDATE_INFOS storageId=%lld",
                                    storageId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListStorages(IndexQueryHandle      *indexQueryHandle,
                              IndexHandle           *indexHandle,
                              IndexId               uuidId,
                              IndexId               entityId,
                              ConstString           jobUUID,
                              ConstString           scheduleUUID,
                              const IndexId         indexIds[],
                              ulong                 indexIdCount,
                              IndexTypeSet          indexTypeSet,
                              IndexStateSet         indexStateSet,
                              IndexModeSet          indexModeSet,
                              ConstString           hostName,
                              ConstString           userName,
//TODO: name+pattern
                              ConstString           name,
                              IndexStorageSortModes sortMode,
                              DatabaseOrdering      ordering,
                              uint64                offset,
                              uint64                limit
                             )
{
  String ftsName;
  String filterString;
  String orderString;
  ulong  i;
  String uuidIdsString,entityIdsString,storageIdsString;
  String filterIdsString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_ANY(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0L) || (indexIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_newCString("1");
  orderString  = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  // get filters
  filterIdsString = String_new();
  string          = String_new();
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  IndexCommon_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  IndexCommon_filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%lld",Index_getDatabaseId(entityId));
  IndexCommon_filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  IndexCommon_filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  IndexCommon_filterAppend(filterString,!String_isEmpty(hostName),"AND","entities.hostName LIKE %S",hostName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(userName),"AND","storages.userName LIKE %S",userName);
// TODO:  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE FTS_storages MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  IndexCommon_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  // get sort mode, ordering
  IndexCommon_appendOrdering(orderString,sortMode != INDEX_STORAGE_SORT_MODE_NONE,INDEX_STORAGE_SORT_MODE_COLUMNS[sortMode],ordering);

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListStorages ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: indexTypeSet=%s%s%s\n",__FILE__,__LINE__,
            IN_SET(indexTypeSet,INDEX_TYPE_UUID) ? " UUID" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) ? " entity" : "",
            IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) ? " storage" : ""
           );
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));
    fprintf(stderr,"%s, %d: hostName=%s\n",__FILE__,__LINE__,String_cString(hostName));
    fprintf(stderr,"%s, %d: userName=%s\n",__FILE__,__LINE__,String_cString(userName));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,STRING,STRING,STRING,UINT64,INT,KEY,STRING,UINT64,INT64,INT,INT,UINT64,STRING,INT,INT64),
//TODO newest
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT IFNULL(uuids.id,0), \
                                                 entities.jobUUID, \
                                                 IFNULL(entities.id,0), \
                                                 entities.scheduleUUID, \
                                                 storages.hostName, \
                                                 storages.userName, \
                                                 storages.comment, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entities.type, \
                                                 storages.id, \
                                                 storages.name, \
                                                 UNIX_TIMESTAMP(storages.created), \
                                                 storages.size, \
                                                 storages.state, \
                                                 storages.mode, \
                                                 UNIX_TIMESTAMP(storages.lastChecked), \
                                                 storages.errorMessage, \
                                                 storages.totalEntryCount, \
                                                 storages.totalEntrySize \
                                          FROM storages \
                                            LEFT JOIN entities ON entities.id=storages.entityId \
                                            LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                          WHERE     storages.deletedFlag!=1 \
                                                AND %s \
                                          GROUP BY storages.id \
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
    String_delete(orderString);
    String_delete(filterString);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           scheduleUUID,
                          String           hostName,
                          String           userName,
                          String           comment,
                          uint64           *createdDateTime,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *dateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize
                         )
{
  DatabaseId uuidDatabaseId,entityDatabaseId,storageDatabaseId;
  uint       indexState_,indexMode_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %S %lld %S %S %S %S %llu %u %lld %S %llu %llu %u %u %llu %S %lu %llu",
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           comment,
                           createdDateTime,
                           archiveType,
                           &storageDatabaseId,
                           storageName,
                           dateTime,
                           size,
                           &indexState_,
                           &indexMode_,
                           lastCheckedDateTime,
                           errorMessage,
                           totalEntryCount,
                           totalEntrySize
                          )
    )
  {
    return FALSE;
  }
  if (uuidId     != NULL) (*uuidId    ) = INDEX_ID_UUID   (uuidDatabaseId   );
  if (entityId   != NULL) (*entityId  ) = INDEX_ID_ENTITY (entityDatabaseId );
  if (storageId  != NULL) (*storageId ) = INDEX_ID_STORAGE(storageDatabaseId);
  if (indexState != NULL) (*indexState) = (IndexStates)indexState_;
  if (indexMode  != NULL) (*indexMode ) = (IndexModes)indexMode_;

  return TRUE;
}

Errors Index_newStorage(IndexHandle *indexHandle,
                        IndexId     uuidId,
                        IndexId     entityId,
                        ConstString hostName,
                        ConstString userName,
                        ConstString storageName,
                        uint64      dateTime,
                        uint64      size,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       )
{
  Errors     error;
  DatabaseId databaseId;

  assert(indexHandle != NULL);
  assert(storageId != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

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
      StaticString (s,MISC_UUID_STRING_LENGTH);

//TODO: remove with index version 8 without storage constraint
      Misc_getUUID(s);

      // insert storage
fprintf(stderr,"%s:%d: *******************\n",__FILE__,__LINE__);
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "storages",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                KEY   ("uuidId",      Index_getDatabaseId(uuidId)),
                                KEY   ("entityId",    Index_getDatabaseId(entityId)),
                                STRING("hostName",    hostName),
                                STRING("userName",    userName),
                                STRING("name",        String_isEmpty(storageName) ? s : storageName),
                                UINT64("created",     dateTime),
                                UINT64("size",        size),
                                UINT  ("state",       indexState),
                                UINT  ("mode",        indexMode),
                                UINT64("lastChecked", "NOW()")
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
      databaseId = Database_getLastRowId(&indexHandle->databaseHandle);

      // insert FTS storage
// TODO: do this again with a trigger?
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                   NULL,  // changedRowCount
                                   "FTS_storages",
                                   DATABASE_FLAG_NONE,
                                   DATABASE_VALUES2
                                   (
                                     KEY   ("storageId", databaseId),
                                     STRING("name",      storageName)
                                   )
                                  );
          if (error != ERROR_NONE)
          {
            (void)Database_delete(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  "id=?",
                                  DATABASE_VALUES
                                  (
                                    { "id", DATABASE_DATATYPE_KEY, {(intptr_t)databaseId }}
                                  )
                                 );
            return error;
          }
          break;
        case DATABASE_TYPE_MYSQL:
          break;
      }

      return ERROR_NONE;
    });

    (*storageId) = INDEX_ID_STORAGE(databaseId);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_LAMBDA_(Errors,(const StringMap resultMap, void *userData),
                                    {
                                      assert(resultMap != NULL);

                                      UNUSED_VARIABLE(userData);

                                      if (StringMap_getInt64(resultMap,"storageId",storageId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_STORAGE uuidId=%lld entityId=%lld hostName=%'S userName=%'S storageName=%'S dateTime=%llu size=%llu indexState=%s indexMode=%s",
                                    uuidId,
                                    entityId,
                                    hostName,
                                    userName,
                                    storageName,
                                    dateTime,
                                    size,
                                    Index_stateToString(indexState,NULL),
                                    Index_modeToString(indexMode,NULL)
                                   );
  }

  return error;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return clearStorage(indexHandle,
                      Index_getDatabaseId(storageId),
                      NULL  // progressInfo
                     );
}

Errors Index_updateStorage(IndexHandle  *indexHandle,
                           IndexId      storageId,
                           ConstString  hostName,
                           ConstString  userName,
                           ConstString  storageName,
                           uint64       dateTime,
                           uint64       size,
                           ConstString  comment,
                           bool         updateNewest
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assertx(Index_getType(storageId) == INDEX_TYPE_STORAGE,"storageId=%"PRIi64"",storageId);

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
      if (hostName != NULL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("hostName", hostName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  KEY(Index_getDatabaseId(storageId)),
                                )
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      if (userName != NULL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("userName", userName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  KEY(Index_getDatabaseId(storageId)),
                                )
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      if (storageName != NULL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("name", storageName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  KEY(Index_getDatabaseId(storageId)),
                                )
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }

        switch (Database_getType(&indexHandle->databaseHandle))
        {
          case DATABASE_TYPE_SQLITE3:
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "FTS_storages",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      STRING("name", storageName),
                                    ),
                                    "storageId=?",
                                    DATABASE_FILTERS
                                    (
                                      KEY(Index_getDatabaseId(storageId)),
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
            break;
          case DATABASE_TYPE_MYSQL:
            break;
        }
      }

      if (dateTime != 0LL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  DATETIME("created", dateTime),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  KEY(Index_getDatabaseId(storageId)),
                                )
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "storages",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                UINT64("size", size),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                KEY(Index_getDatabaseId(storageId)),
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (comment != NULL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES2
                                (
                                  STRING("comment", comment),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  KEY(Index_getDatabaseId(storageId)),
                                )
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      return ERROR_NONE;
    });

    if (updateNewest)
    {
      error = IndexStorage_addToNewest(indexHandle,
                                              Index_getDatabaseId(storageId),
                                              NULL  // progressInfo
                                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_UPDATE storageId=%lld hostName=%'S userName=%'S storageName=%'S dateTime=%llu storageSize=%llu comment=%'S updateNewest=%y",
                                    storageId,
                                    hostName,
                                    userName,
                                    storageName,
                                    dateTime,
                                    size,
                                    comment,
                                    updateNewest
                                   );
  }

  return error;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors              error;
  DatabaseId          entityId;
  DatabaseStatementHandle databaseStatementHandle;
  ulong               totalEntryCount;
  uint64              totalEntrySize;
  ulong               totalFileCount;
  uint64              totalFileSize;
  ulong               totalImageCount;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

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
      // get entity id
      error = Database_getId(&indexHandle->databaseHandle,
                             &entityId,
                             "storages",
                             "entityId",
                             "WHERE id=%lld \
                             ",
                             Index_getDatabaseId(storageId)
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get aggregate data
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(INT,INT64,INT,INT64,INT,INT64,INT,INT,INT,INT64,INT),
                               "SELECT totalEntryCount, \
                                       totalEntrySize, \
                                       totalFileCount, \
                                       totalFileSize, \
                                       totalImageCount, \
                                       totalImageSize, \
                                       totalDirectoryCount, \
                                       totalLinkCount, \
                                       totalHardlinkCount, \
                                       totalHardlinkSize, \
                                       totalSpecialCount \
                                FROM storages \
                                WHERE     deletedFlag!=1 \
                                      AND id=? \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                                 KEY (Index_getDatabaseId(storageId))
                               )
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (!Database_getNextRow(&databaseStatementHandle,
                               "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                               &totalEntryCount,
                               &totalEntrySize,
                               &totalFileCount,
                               &totalFileSize,
                               &totalImageCount,
                               &totalImageSize,
                               &totalDirectoryCount,
                               &totalLinkCount,
                               &totalHardlinkCount,
                               &totalHardlinkSize,
                               &totalSpecialCount
                              )
        )
      {
        totalEntryCount     = 0;
        totalEntrySize      = 0LL;
        totalFileCount      = 0;
        totalFileSize       = 0LL;
        totalImageCount     = 0;
        totalImageSize      = 0LL;
        totalDirectoryCount = 0;
        totalLinkCount      = 0;
        totalHardlinkCount  = 0;
        totalHardlinkSize   = 0LL;
        totalSpecialCount   = 0;
      }
      Database_finalize(&databaseStatementHandle);

      // set deleted flag
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE storages \
                                SET deletedFlag=1 \
                                WHERE id=%lld \
                               ",
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
//fprintf(stderr,"%s, %d: deleted storageId=%lld\n",__FILE__,__LINE__,Index_getDatabaseId(storageId));
#if 0
fprintf(stderr,"%s, %d: deleted storage %lld\n",__FILE__,__LINE__,Index_getDatabaseId(storageId));
fprintf(stderr,"%s, %d: totalEntry=%lu %llu  totalFile=%lu %llu  totalImage=%lu %llu  totalDirectory=%lu  totalLink=%lu  totalHardlink=%lu %llu totalSpecial=%lu\n",__FILE__,__LINE__,
                               totalEntryCount,
                               totalEntrySize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount
);
#endif

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

//TODO: too slow to do it immediately; postpone to index thread
#if 0
    // remove from newest entries
    error = IndexStorage_removeFromNewest(indexHandle,
                                    Index_getDatabaseId(storageId),
                                    NULL  // progressInfo
                                   );
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    INDEX_DOX(error,
              indexHandle,
    {
      // update aggregates
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE entities \
                                SET totalEntryCount    =totalEntryCount    -%lu, \
                                    totalEntrySize     =totalEntrySize     -%llu, \
                                    totalFileCount     =totalFileCount     -%lu, \
                                    totalFileSize      =totalFileSize      -%llu, \
                                    totalImageCount    =totalImageCount    -%lu, \
                                    totalImageSize     =totalImageSize     -%llu, \
                                    totalDirectoryCount=totalDirectoryCount-%lu, \
                                    totalLinkCount     =totalLinkCount     -%lu, \
                                    totalHardlinkCount =totalHardlinkCount -%lu, \
                                    totalHardlinkSize  =totalHardlinkSize  -%llu, \
                                    totalSpecialCount  =totalSpecialCount  -%lu \
                                WHERE id=%lld \
                               ",
                               totalEntryCount,
                               totalEntrySize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount,
                               entityId
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // prune entity
      error = IndexEntity_prune(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      return error;
    }

    // trigger clean-up thread
    Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_DELETE storageId=%lld",
                                    storageId
                                   );
  }

  return error;
}

bool Index_hasDeletedStorages(IndexHandle *indexHandle,
                              ulong       *deletedStorageCount
                             )
{
  Errors error;
  int64  n;

  assert(indexHandle != NULL);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getInteger64(&indexHandle->databaseHandle,
                                   &n,
                                   "storages",
                                   "COUNT(id)",
                                   "WHERE deletedFlag=1"
                                  );
    });
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
  }
  else
  {
    // slave mode: no deleted
    n = 0LL;
  }
  if (deletedStorageCount != NULL) (*deletedStorageCount) = (ulong)n;

  return (n > 0LL);
}

bool Index_isDeletedStorage(IndexHandle *indexHandle,
                            IndexId     storageId
                           )
{
  bool deletedFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(deletedFlag,
              indexHandle,
    {
      return !Database_existsValue(&indexHandle->databaseHandle,
                                   "storages",
                                   "id",
                                   "WHERE id=%lld AND deletedFlag!=1",
                                   Index_getDatabaseId(storageId)
                                  );
    });
  }
  else
  {
    // slave mode: always deleted
    deletedFlag = TRUE;
  }

  return deletedFlag;
}

bool Index_isEmptyStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(emptyFlag,
              indexHandle,
    {
      return IndexStorage_isEmpty(indexHandle,
                            Index_getDatabaseId(storageId)
                           );
    });
  }
  else
  {
    // slave mode: always empty
    emptyFlag = TRUE;
  }

  return emptyFlag;
}

Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       scheduleUUID,
                        ArchiveTypes archiveType,
                        String       storageName,
                        uint64       *dateTime,
                        uint64       *size,
                        IndexStates  *indexState,
                        IndexModes   *indexMode,
                        uint64       *lastCheckedDateTime,
                        String       errorMessage,
                        uint64       *totalEntryCount,
                        uint64       *totalEntrySize
                       )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          uuidDatabaseId,entityDatabaseId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,INT,STRING,UINT64,INT64,INT,INT,UINT64,STRING,INT,INT64),
                             "SELECT uuids.id, \
                                     uuids.jobUUID, \
                                     entities.id, \
                                     entities.scheduleUUID, \
                                     entities.type, \
                                     storages.name, \
                                     UNIX_TIMESTAMP(storages.created), \
                                     storages.size, \
                                     storages.state, \
                                     storages.mode, \
                                     UNIX_TIMESTAMP(storages.lastChecked), \
                                     storages.errorMessage, \
                                     storages.totalEntryCount, \
                                     storages.totalEntrySize \
                              FROM storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE id=? \
                             ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                               KEY (Index_getDatabaseId(storageId))
                             )
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (!Database_getNextRow(&databaseStatementHandle,
                             "%llu %S %llu %S %u %S %llu %llu %u %u %llu %S %llu %llu",
                             &uuidDatabaseId,
                             jobUUID,
                             &entityDatabaseId,
                             scheduleUUID,
                             archiveType,
                             storageName,
                             dateTime,
                             size,
                             indexState,
                             indexMode,
                             lastCheckedDateTime,
                             errorMessage,
                             totalEntryCount,
                             totalEntrySize
                            )
       )
    {
      Database_finalize(&databaseStatementHandle);
      return ERROR_DATABASE_INDEX_NOT_FOUND;
    }
    if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_(INDEX_TYPE_UUID,  uuidDatabaseId  );
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityDatabaseId);
    Database_finalize(&databaseStatementHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          IndexId     indexId
                         )
{
  Errors  error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  INDEX_DOX(error,
            indexHandle,
  {
    return pruneStorage(indexHandle,
                        Index_getDatabaseId(indexId),
                        NULL  // progressInfo
                       );
  });

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
