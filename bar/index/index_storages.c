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

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

#include "index/index.h"
#include "index/index_common.h"
#include "index/index_entities.h"
#include "index/index_entries.h"
#include "index/index_uuids.h"

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

// TODO: use?
#if 0
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
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT("lockedCount", 0),
                          ),
                          DATABASE_FILTERS_NONE
                         );

    // clear state of deleted storages
    (void)Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT("state", INDEX_STATE_NONE),
                          ),
                          "deletedFlag=TRUE",
                          DATABASE_FILTERS
                          (
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
                                   ) == ERROR_NONE
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
                    "Requested update index #%"PRIi64": %s",
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
#endif

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
                                 INDEX_TYPESET_ALL,
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
        (void)IndexStorage_purge(indexHandle,
                                 storageId,
                                 NULL  // progressInfo
                                );
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
  DatabaseId              storageId;
  StaticString            (uuid,MISC_UUID_STRING_LENGTH);
  uint64                  createdDateTime;
  DatabaseId              entityId;
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
   return Database_get(&indexHandle->databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 3);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        String_setBuffer(uuid,values[0].text.data,values[0].text.length);
                        String_setBuffer(name1,values[1].text.data,values[0].text.length);
                        createdDateTime = values[2].dateTime;

                        // find matching entity/create default entity
                        error = Database_get(&indexHandle->databaseHandle,
                                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                             {
                                               assert(values != NULL);
                                               assert(valueCount == 3);

                                               UNUSED_VARIABLE(userData);
                                               UNUSED_VARIABLE(valueCount);

                                               storageId = values[0].id;
                                               String_setBuffer(name2,values[0].text.data,values[0].text.length);

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
                                                 (void)Database_update(&indexHandle->databaseHandle,
                                                                       NULL,  // changedRowCount
                                                                       "storages",
                                                                       DATABASE_FLAG_NONE,
                                                                       DATABASE_VALUES
                                                                       (
                                                                         DATABASE_VALUE_KEY("entityId", entityId),
                                                                       ),
                                                                       "id=?",
                                                                       DATABASE_FILTERS
                                                                       (
                                                                         DATABASE_FILTER_KEY(storageId)
                                                                       )
                                                                       );
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
                                               DATABASE_COLUMN_KEY   ("id"),
                                               DATABASE_COLUMN_STRING("name"),
                                             ),
                                             "uuid=?",
                                             DATABASE_FILTERS
                                             (
                                               DATABASE_FILTER_STRING(uuid)
                                             ),
                                             NULL,  // orderGroup
                                             0LL,
                                             DATABASE_UNLIMITED
                                            );

                        if (error == ERROR_NONE)
                        {
                          error = Database_insert(&indexHandle->databaseHandle,
                                                  NULL,  // insertRowId
                                                  "entities",
                                                  DATABASE_FLAG_NONE,
                                                  DATABASE_VALUES
                                                  (
                                                    DATABASE_VALUE_STRING  ("jobUUID", uuid),
                                                    DATABASE_VALUE_DATETIME("created", createdDateTime),
                                                    DATABASE_VALUE_UINT    ("type",    ARCHIVE_TYPE_FULL),
                                                  )
                                                 );
                        }
                        if (error == ERROR_NONE)
                        {
                          // get entity id
                          entityId = Database_getLastRowId(&indexHandle->databaseHandle);

                          // assign entity id for all storage entries with same uuid and matching name (equals except digits)
                          error = Database_get(&indexHandle->databaseHandle,
                                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                               {
                                                 assert(values != NULL);
                                                 assert(valueCount == 3);

                                                 UNUSED_VARIABLE(userData);
                                                 UNUSED_VARIABLE(valueCount);

                                                 storageId = values[0].id;
                                                 String_setBuffer(name2,values[1].text.data,values[1].text.length);

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
                                                   (void)Database_update(&indexHandle->databaseHandle,
                                                                         NULL,  // changedRowCount
                                                                         "storages",
                                                                         DATABASE_FLAG_NONE,
                                                                         DATABASE_VALUES
                                                                         (
                                                                           DATABASE_VALUE_KEY("entityId", entityId),
                                                                         ),
                                                                         "id=?",
                                                                         DATABASE_FILTERS
                                                                         (
                                                                           DATABASE_FILTER_KEY(storageId)
                                                                         )
                                                                         );
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
                                                 DATABASE_COLUMN_KEY   ("id"),
                                                 DATABASE_COLUMN_STRING("name")
                                               ),
                                               "uuid=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_STRING(uuid)
                                               ),
                                               NULL,  // orderGroup
                                               0LL,
                                               1LL
                                              );
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
                        DATABASE_COLUMN_KEY     ("id"),
                        DATABASE_COLUMN_STRING  ("name"),
                        DATABASE_COLUMN_DATETIME("created"),
                      ),
                      "entityId=0",
                      DATABASE_FILTERS
                      (
                      ),
                      NULL,  // orderGroup
                      0LL,
                      DATABASE_UNLIMITED
                     );
  });
  if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;

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
  ulong      n;
  Errors     error;
  DatabaseId databaseId;
  IndexId    storageId;

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
                             &databaseId,
                             "storages",
                             "id",
                             "    ((state<?) OR (state>?)) \
                              AND deletedFlag!=TRUE \
                             ",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_STATE_MIN),
                               DATABASE_FILTER_UINT(INDEX_STATE_MAX)
                             )
                            );
      storageId = INDEX_ID_STORAGE(databaseId);
      if ((error == ERROR_NONE) && !INDEX_ID_IS_NONE(storageId))
      {
        error = IndexStorage_purge(indexHandle,
                                   storageId,
                                   NULL  // progressInfo
                                  );
        n++;
      }
    }
    while ((error == ERROR_NONE) && !INDEX_ID_IS_NONE(storageId));

    return error;
  });
  if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;

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
  Errors       error;
  String       name1,name2;
  DatabaseId   storageId;
  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  uint64       createdDateTime;
  DatabaseId   entityId;
  bool         equalsFlag;
  ulong        i;

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
    return Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 3);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          String_set(uuid,values[0].string);
                          String_set(name1,values[1].string);
                          createdDateTime = values[2].dateTime;

                          // find matching entity/create default entity
                          error = Database_get(&indexHandle->databaseHandle,
                                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                               {
                                                 assert(values != NULL);
                                                 assert(valueCount == 2);

                                                 UNUSED_VARIABLE(userData);
                                                 UNUSED_VARIABLE(valueCount);

                                                 storageId = values[0].id;
                                                 String_set(name2,values[0].string);

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
                                                   (void)Database_update(&indexHandle->databaseHandle,
                                                                         NULL,  // changedRowCount
                                                                         "storages",
                                                                         DATABASE_FLAG_NONE,
                                                                         DATABASE_VALUES
                                                                         (
                                                                           DATABASE_VALUE_KEY("entityId", entityId),
                                                                         ),
                                                                         "id=?",
                                                                         DATABASE_FILTERS
                                                                         (
                                                                           DATABASE_FILTER_KEY(storageId)
                                                                         )
                                                                        );
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
                                                 DATABASE_COLUMN_KEY   ("id"),
                                                 DATABASE_COLUMN_STRING("name")
                                               ),
                                               "uuid=?",
                                               DATABASE_FILTERS
                                               (
                                                 DATABASE_FILTER_STRING(uuid)
                                               ),
                                               NULL,  // groupBy
                                               NULL,  // orderBy
                                               0LL,
                                               DATABASE_UNLIMITED
                                              );

                          if (error == ERROR_NONE)
                          {
                            error = Database_insert(&indexHandle->databaseHandle,
                                                    &entityId,
                                                    "entities",
                                                    DATABASE_FLAG_NONE,
                                                    DATABASE_VALUES
                                                    (
                                                      DATABASE_VALUE_STRING  ("jobUUID", uuid),
                                                      DATABASE_VALUE_DATETIME("created", createdDateTime),
                                                      DATABASE_VALUE_UINT    ("type",    ARCHIVE_TYPE_FULL),
                                                    ),
                                                    DATABASE_COLUMNS_NONE,
                                                    DATABASE_FILTERS_NONE
                                                   );
                          }

                          return error;
                        },NULL),
                        NULL,  // changedRowCount
                        DATABASE_TABLES
                        (
                          "storages"
                        ),
                        DATABASE_FLAG_NONE,
                        DATABASE_COLUMNS
                        (
                          DATABASE_COLUMN_KEY     ("id"),
                          DATABASE_COLUMN_STRING  ("name"),
                          DATABASE_COLUMN_DATETIME("created")
                        ),
                        "entityId=0",
                        DATABASE_FILTERS
                        (
                        ),
                        NULL,  // groupBy
                        "ORDER BY name ASC",
                        0LL,
                        DATABASE_UNLIMITED
                      );
  });
  if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;

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

  return error;
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

LOCAL Errors getStorageState(IndexHandle *indexHandle,
                             DatabaseId  storageId,
                             IndexStates *indexState,
                             uint64      *lastCheckedDateTime,
                             String      errorMessage
                            )
{
  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);

  return Database_get(&indexHandle->databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 3);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        if (indexState          != NULL) (*indexState)          = (IndexStates)values[0].u;
                        if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = values[1].u64;
                        if (errorMessage        != NULL) String_set(errorMessage,values[2].string);

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
                        DATABASE_COLUMN_ENUM    ("state"),
                        DATABASE_COLUMN_DATETIME("lastChecked"),
                        DATABASE_COLUMN_STRING  ("errorMessage")
                      ),
                      "id=?",
                      DATABASE_FILTERS
                      (
                        DATABASE_FILTER_KEY (storageId)
                      ),
                      NULL,  // groupBy
                      NULL,  // orderBy
                      0LL,
                      1LL
                     );
}

/***********************************************************************\
* Name   : deleteStorageFragments
* Purpose: delete storage fragments
* Input  : indexHandle  - in dex handle
*          storageId    - st orage id
*          progressInfo - pr ogress info
* Output : -
* Return : ERROR_NONE or err or code
* Notes  : -
\*************************** ********************************************/

LOCAL Errors deleteStorageFragments(IndexHandle  *indexHandle,
                                    IndexId      storageId,
                                    ProgressInfo *progressInfo
                                   )
{
  bool     transactionFlag;
  Errors   error;
  bool     doneFlag;

  #ifndef NDEBUG
    ulong  deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  // delete fragments
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
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"entryFragments","count(id)","WHERE storageId=%"PRIi64"",storageId);fprintf(stderr,"%s, %"PRIi64": fragments %d: %"PRIi64"\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_delete(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "entryFragments",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                  )
                                 );
        if (error == ERROR_NONE)
        {
//          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
//fprintf(stderr,"%s, %d: 1 fragments done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, deleted fragments: %"PRIu64"ms\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

/***********************************************************************\
* Name   : deleteFTSEntry
* Purpose: delete FTS entry
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryId      - entry id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteFTSEntry(IndexHandle  *indexHandle,
                            ProgressInfo *progressInfo,
                            DatabaseId   entryId
                           )
{
  Errors error;

  assert(indexHandle != NULL);

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_UNKNOWN;
  switch (Database_getType(&indexHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      error = Database_delete(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount,
                              "FTS_entries",
                              DATABASE_FLAG_NONE,
                              "entryId=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(entryId)
                              ),
                              DATABASE_UNLIMITED
                             );
      break;
    case DATABASE_TYPE_MARIADB:
      error = ERROR_NONE;
      break;
    case DATABASE_TYPE_POSTGRESQL:
      error = Database_delete(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount,
                              "FTS_entries",
                              DATABASE_FLAG_NONE,
                              "entryId=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(entryId)
                              ),
                              DATABASE_UNLIMITED
                             );
      break;
  }
  assert(error != ERROR_UNKNOWN);

  // free resources

  return error;
}

/***********************************************************************\
* Name   : deleteSubEntry
* Purpose: delete file/image/directory/link/hardlink/special sub entry
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryId      - entry id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteSubEntry(IndexHandle  *indexHandle,
                            ProgressInfo *progressInfo,
                            DatabaseId   entryId
                           )
{
  Errors error;

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    // purge file entries without fragments
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "fileEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  if (error == ERROR_NONE)
  {
    // purge image entries without fragments
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "imageEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  if (error == ERROR_NONE)
  {
    // purge directory entries
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "directoryEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  if (error == ERROR_NONE)
  {
    // purge link entries
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "linkEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  if (error == ERROR_NONE)
  {
    // purge hardlink entries without fragments
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "hardlinkEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  if (error == ERROR_NONE)
  {
    // purge special entries
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
                            "specialEntries",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : deleteEntry
* Purpose: purge entry
* Input  : indexHandle  - index handle
*          progressInfo - progress info
*          entryId      - entry id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteEntry(IndexHandle  *indexHandle,
                         ProgressInfo *progressInfo,
                         DatabaseId   entryId
                        )
{
  Errors error;

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount,
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

  // free resources

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
                                    IndexId      storageId,
                                    ProgressInfo *progressInfo
                                   )
{
  Errors     error;
  DatabaseId databaseId;
  IndexId    entityId;
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0,dt[10];
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
      return Database_update(&indexHandle->databaseHandle,
                             NULL,  // changedRowCount
                             "storages",
                             DATABASE_FLAG_NONE,
                             DATABASE_VALUES
                             (
                               DATABASE_VALUE_UINT("totalEntryCount",           0),
                               DATABASE_VALUE_UINT("totalEntrySize",            0),
                               DATABASE_VALUE_UINT("totalFileCount",            0),
                               DATABASE_VALUE_UINT("totalFileSize",             0),
                               DATABASE_VALUE_UINT("totalImageCount",           0),
                               DATABASE_VALUE_UINT("totalImageSize",            0),
                               DATABASE_VALUE_UINT("totalDirectoryCount",       0),
                               DATABASE_VALUE_UINT("totalLinkCount",            0),
                               DATABASE_VALUE_UINT("totalHardlinkCount",        0),
                               DATABASE_VALUE_UINT("totalHardlinkSize",         0),
                               DATABASE_VALUE_UINT("totalSpecialCount",         0),
                               DATABASE_VALUE_UINT("totalEntryCountNewest",     0),
                               DATABASE_VALUE_UINT("totalEntrySizeNewest",      0),
                               DATABASE_VALUE_UINT("totalFileCountNewest",      0),
                               DATABASE_VALUE_UINT("totalFileSizeNewest",       0),
                               DATABASE_VALUE_UINT("totalImageCountNewest",     0),
                               DATABASE_VALUE_UINT("totalImageSizeNewest",      0),
                               DATABASE_VALUE_UINT("totalDirectoryCountNewest", 0),
                               DATABASE_VALUE_UINT("totalLinkCountNewest",      0),
                               DATABASE_VALUE_UINT("totalHardlinkCountNewest",  0),
                               DATABASE_VALUE_UINT("totalHardlinkSizeNewest",   0),
                               DATABASE_VALUE_UINT("totalSpecialCountNewest",   0)
                             ),
                             "id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             )
                            );
    });
  }
  if (error == ERROR_NONE)
  {
    // update entity aggregates
    error = Database_getId(&indexHandle->databaseHandle,
                           &databaseId,
                           "storages",
                           "entityId",
                           "id=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                           )
                          );
    entityId = INDEX_ID_ENTITY(databaseId);
    if (error == ERROR_NONE)
    {
      error = IndexEntity_updateAggregates(indexHandle,entityId);
    }
  }
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, updated aggregates: %"PRIu64"ms\n",__FILE__,__LINE__,
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
                          IndexId      storageId,
                          ProgressInfo *progressInfo
                         )
{
  Array                entryIds;
  ArrayIterator        arrayIterator;
  DatabaseId           entryId;
  bool                 transactionFlag;
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
  error = ERROR_NONE;

//TODO: do regulary in index thread?
#if 0
  /* get entries to purge without associated file/image/directory/link/hardlink/special entry
     Note: may be left from interrupted purge of previous run
  */
//l=0; Database_getInteger64(&indexHandle->databaseHandle,&l,"entries","count(id)",""); fprintf(stderr,"%s, %d: l=%"PRIi64"\n",__FILE__,__LINE__,l);
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
                             "entries \
                                LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND fileEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_FILE)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND imageEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND directoryEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_DIRECTORY)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND linkEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_LINK)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND hardlinkEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK)
                             ),
                             DATABASE_UNLIMITED
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
                             "entries \
                                LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                             ",
                             "entries.id",
                             "entries.type=? AND specialEntries.id IS NULL",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_UINT(INDEX_TYPE_SPECIAL)
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[5] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries without associated entry to purge: file %"PRIu64"ms, image %"PRIu64"ms, directory %"PRIu64"ms, link %"PRIu64"ms, hardlink %"PRIu64"ms, special %"PRIu64"ms\n",__FILE__,__LINE__,
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
fprintf(stderr,"%s:%d: %lu\n",__FILE__,__LINE__,Array_length(entryIds));
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
          String_formatAppend(entryIdsString,"%"PRIi64,entryId);
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
    fprintf(stderr,"%s, %d: error: %s, purged orphaned entries: %"PRIu64"ms\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif
#endif

//fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__); asm("int3");

  // collect file/image/directory/link/hardlink/special entries to delete
  if (error == ERROR_NONE)
  {
    error = IndexEntry_collectIds(&entryIds,
                                  indexHandle,
                                  storageId,
                                  progressInfo
                                 );
  }

  // delete storage fragments
  if (error == ERROR_NONE)
  {
    error = deleteStorageFragments(indexHandle,
                                   storageId,
                                   progressInfo
                                  );
  }

  INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                    indexHandle,
                                    transactionFlag,
  {
    ARRAY_ITERATEX(&entryIds,arrayIterator,entryId,error == ERROR_NONE)
    {
      // delete entry if there exists no other fragments
      if (!Database_existsValue(&indexHandle->databaseHandle,
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
        // delete FTS entry
        if (error == ERROR_NONE)
        {
          switch (Database_getType(&indexHandle->databaseHandle))
          {
            case DATABASE_TYPE_SQLITE3:
              error = deleteFTSEntry(indexHandle,
                                     progressInfo,
                                     entryId
                                    );
              break;
            case DATABASE_TYPE_MARIADB:
              // nothing to do (using a view)
              break;
            case DATABASE_TYPE_POSTGRESQL:
              error = deleteFTSEntry(indexHandle,
                                    progressInfo,
                                    entryId
                                   );
              break;
          }
        }

        // delete file/image/directory/link/hardlink/special entry
        if (error == ERROR_NONE)
        {
          error = deleteSubEntry(indexHandle,
                                 progressInfo,
                                 entryId
                                );
        }

        // delete entry
        if (error == ERROR_NONE)
        {
          error = deleteEntry(indexHandle,
                              progressInfo,
                              entryId
                             );
        }
      }

      if (error == ERROR_NONE)
      {
        error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
      }
    }

    return error;
  });

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
      fprintf(stderr,"%s, %d: error: %s, removed from newest entries: %"PRIu64"ms\n",__FILE__,__LINE__,
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
                                 INDEX_TYPESET_ANY,
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

Errors IndexStorage_cleanUp(IndexHandle *indexHandle)
{
  Errors error;

  assert(indexHandle != NULL);

  error = ERROR_NONE;

  if (error == ERROR_NONE) error = cleanUpStorageNoName(indexHandle);
  if (error == ERROR_NONE) error = cleanUpStorageNoName(indexHandle);
  if (error == ERROR_NONE) error = cleanUpStorageNoEntity(indexHandle);
  if (error == ERROR_NONE) error = cleanUpStorageInvalidState(indexHandle);

  return error;
}

bool IndexStorage_isEmpty(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  assert(indexHandle != NULL);

  return    !INDEX_ID_IS_NONE(storageId)
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "entryFragments",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                  )
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "directoryEntries",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                  )
                                )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "linkEntries",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                  )
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "specialEntries",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                  )
                                 );
}

Errors IndexStorage_delete(IndexHandle  *indexHandle,
// TODO:
                           IndexId      storageId,
                           ProgressInfo *progressInfo
                          )
{
  String name;
  String string;
  Errors error;
  uint64 createdDateTime;
  bool   transactionFlag;
  bool   doneFlag;
  #ifndef NDEBUG
    ulong deletedCounter;
  #endif

  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert(!INDEX_ID_IS_NONE(storageId));

  // init variables
  name   = String_new();
  string = String_new();

  // get storage name, created date/time
  error = Database_get(&indexHandle->databaseHandle,
                       CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                       {
                         assert(values != NULL);
                         assert(valueCount == 2);

                         UNUSED_VARIABLE(userData);
                         UNUSED_VARIABLE(valueCount);

                         String_set(name, values[0].string);
                         createdDateTime = values[1].dateTime;

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
                         DATABASE_COLUMN_STRING  ("name"),
                         DATABASE_COLUMN_DATETIME("created"),
                       ),
                       "storages.id=?",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                       ),
                       NULL,  // groupBy
                       NULL,  // orderBy
                       0LL,
                       1LL
                      );
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

  switch (Database_getType(&indexHandle->databaseHandle))
  {
    case DATABASE_TYPE_SQLITE3:
      // delete FTS storages
      if (error == ERROR_NONE)
      {
        INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                          indexHandle,
                                          transactionFlag,
        {
          do
          {
            doneFlag = TRUE;
// TODO:
            error = IndexCommon_delete(indexHandle,
                                      &doneFlag,
                                      #ifndef NDEBUG
                                        &deletedCounter,
                                      #else
                                        NULL,  // deletedCounter
                                      #endif
                                      "FTS_storages",
                                      "storageId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                      )
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
      break;
    case DATABASE_TYPE_MARIADB:
      break;
    case DATABASE_TYPE_POSTGRESQL:
      // delete FTS storages
      if (error == ERROR_NONE)
      {
        INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                          indexHandle,
                                          transactionFlag,
        {
          do
          {
            doneFlag = TRUE;
// TODO:
            error = IndexCommon_delete(indexHandle,
                                      &doneFlag,
                                      #ifndef NDEBUG
                                        &deletedCounter,
                                      #else
                                        NULL,  // deletedCounter
                                      #endif
                                      "FTS_storages",
                                      "storageId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                                      )
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
      break;
  }

  // delete storage
  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "storages",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                            ),
                            DATABASE_UNLIMITED
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
              "Removed storage #%"PRIu64" from index: %s, created at %s",
              storageId,
              String_cString(name),
              (createdDateTime != 0LL)
                ? String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,FALSE,NULL))
                : "unknown"
             );

  // free resources
  String_delete(string);
  String_delete(name);

  return ERROR_NONE;
}

Errors IndexStorage_purge(IndexHandle  *indexHandle,
                          IndexId      storageId,
                          ProgressInfo *progressInfo
                         )
{
  Errors               error;
  DatabaseId           databaseId;
  IndexId              entityId;
  uint                 totalEntryCount;
  uint64               totalEntrySize;
  uint                 totalFileCount;
  uint64               totalFileSize;
  uint                 totalImageCount;
  uint64               totalImageSize;
  uint                 totalDirectoryCount;
  uint                 totalLinkCount;
  uint                 totalHardlinkCount;
  uint64               totalHardlinkSize;
  uint                 totalSpecialCount;
  Array                entryIds;
  ArraySegmentIterator arraySegmentIterator;
  ArrayIterator        arrayIterator;
  IndexId              entryId;
  String               entryIdsString;
  String               filterString;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

// TODO: progress info
UNUSED_VARIABLE(progressInfo);

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
                             &databaseId,
                             "storages",
                             "entityId",
                             "id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             )
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entityId = INDEX_ID_ENTITY(databaseId);

      // get aggregate data
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 11);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             totalEntryCount     = values[ 0].u;
                             totalEntrySize      = values[ 1].u64;
                             totalFileCount      = values[ 2].u;
                             totalFileSize       = values[ 3].u64;
                             totalImageCount     = values[ 4].u;
                             totalImageSize      = values[ 5].u64;
                             totalDirectoryCount = values[ 6].u;
                             totalLinkCount      = values[ 7].u;
                             totalHardlinkCount  = values[ 8].u;
                             totalHardlinkSize   = values[ 9].u64;
                             totalSpecialCount   = values[10].u;

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
                             DATABASE_COLUMN_UINT  ("totalEntryCount"),
                             DATABASE_COLUMN_UINT64("totalEntrySize"),
                             DATABASE_COLUMN_UINT  ("totalFileCount"),
                             DATABASE_COLUMN_UINT64("totalFileSize"),
                             DATABASE_COLUMN_UINT  ("totalImageCount"),
                             DATABASE_COLUMN_UINT64("totalImageSize"),
                             DATABASE_COLUMN_UINT  ("totalDirectoryCount"),
                             DATABASE_COLUMN_UINT64("totalLinkCount"),
                             DATABASE_COLUMN_UINT  ("totalHardlinkCount"),
                             DATABASE_COLUMN_UINT64("totalHardlinkSize"),
                             DATABASE_COLUMN_UINT  ("totalSpecialCount")
                           ),
                           "    deletedFlag!=TRUE \
                            AND id=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                           ),
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
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

      Array_init(&entryIds,sizeof(DatabaseId),64,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

      // get entry ids to purge
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DatabaseId entryId;
                             uint64     entrySize;
                             uint64     fragmentSizeSum;

                             assert(values != NULL);
                             assert(valueCount == 3);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryId         = values[0].id;
                             assert(entryId != DATABASE_ID_NONE);
                             entrySize       = values[1].u64;
                             fragmentSizeSum = values[2].u64;

                             // check if entry is completely covery by fragments
                             if (entrySize == fragmentSizeSum)
                             {
                               Array_append(&entryIds,&entryId);
                             }

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("entries.id"),
                             DATABASE_COLUMN_UINT64("entries.size"),
                             DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                           ),
                           "entryFragments.storageId=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(storageId))
                           ),
                           "entries.id",
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DatabaseId entryId;

                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryId = values[0].id;
                             assert(entryId != DATABASE_ID_NONE);

                             Array_append(&entryIds,&entryId);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("entries.id")
                           ),
                           "    entries.type=? \
                            AND directoryEntries.storageId=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_TYPE_DIRECTORY),
                             DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(storageId))
                           ),
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DatabaseId entryId;

                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryId = values[0].id;
                             assert(entryId != DATABASE_ID_NONE);

                             Array_append(&entryIds,&entryId);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("entries.id")
                           ),
                           "    entries.type=? \
                            AND linkEntries.storageId=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_TYPE_LINK),
                             DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(storageId))
                           ),
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             DatabaseId entryId;

                             assert(values != NULL);
                             assert(valueCount == 1);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryId = values[0].id;
                             assert(entryId != DATABASE_ID_NONE);

                             Array_append(&entryIds,&entryId);

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entries \
                                LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY   ("entries.id")
                           ),
                           "    entries.type=? \
                            AND specialEntries.storageId=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_UINT  (INDEX_CONST_TYPE_SPECIAL),
                             DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(storageId))
                           ),
                           NULL,  // groupBy
                           NULL,  // orderBy
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      if (error != ERROR_NONE)
      {
        return error;
      }

      // purge entries (mark as deleted)
      entryIdsString = String_new();
      filterString   = String_new();
      ARRAY_SEGMENTX(&entryIds,arraySegmentIterator,1024,error == ERROR_NONE)
      {
        String_clear(entryIdsString);
        ARRAY_SEGMENT_ITERATE(&entryIds,arraySegmentIterator,arrayIterator,entryId)
        {
          if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
          String_appendFormat(entryIdsString,"%"PRIi64,entryId);
        }
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(entryIdsString));

        DATABASE_TRANSACTION_DO(&indexHandle->databaseHandle,DATABASE_TRANSACTION_TYPE_EXCLUSIVE,WAIT_FOREVER)
        {
          error = Database_update(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_BOOL  ("deletedFlag",TRUE)
                                  ),
                                  String_cString(String_format(filterString,
                                                               "id IN (%S)",
                                                               entryIdsString
                                                              )
                                                ),
                                  DATABASE_FILTERS
                                  (
                                  )
                                 );
        }
      }
      String_delete(filterString);
      String_delete(entryIdsString);

      Array_done(&entryIds);

      // purge storage (mark as deleted)
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "storages",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_BOOL("deletedFlag", TRUE),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update aggregates
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT  ("totalEntryCount",      "totalEntryCount-?",     totalEntryCount),
                                DATABASE_VALUE_UINT64("totalEntrySize",       "totalEntrySize-?",      totalEntrySize),
                                DATABASE_VALUE_UINT  ("totalFileCount",       "totalFileCount-?",      totalFileCount),
                                DATABASE_VALUE_UINT64("totalFileSize",        "totalFileSize-?",       totalFileSize),
                                DATABASE_VALUE_UINT  ("totalImageCount",      "totalImageCount-?",     totalImageCount),
                                DATABASE_VALUE_UINT64("totalImageSize",       "totalImageSize-?",      totalImageSize),
                                DATABASE_VALUE_UINT  ("totalDirectoryCount",  "totalDirectoryCount-?", totalDirectoryCount),
                                DATABASE_VALUE_UINT  ("totalLinkCount",       "totalLinkCount-?",      totalLinkCount),
                                DATABASE_VALUE_UINT  ("totalHardlinkCount",   "totalHardlinkCount-?",  totalHardlinkCount),
                                DATABASE_VALUE_UINT64("totalHardlinkSize",    "totalHardlinkSize-?",   totalHardlinkSize),
                                DATABASE_VALUE_UINT  ("totalSpecialCount",    "totalSpecialCount-?",   totalSpecialCount),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entityId))
                              )
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
                                    "INDEX_STORAGE_PURGE storageId=%"PRIi64,
                                    storageId
                                   );
  }

  return error;
}


Errors IndexStorage_purgeAllById(IndexHandle  *indexHandle,
                                 IndexId      entityId,
                                 IndexId      keepStorageId,
                                 ProgressInfo *progressInfo
                                )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

// TODO: progress info
UNUSED_VARIABLE(progressInfo);

  if (indexHandle->masterIO == NULL)
  {
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPESET_ALL,
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
    while (   (error == ERROR_NONE)
           && Index_getNextStorage(&indexQueryHandle,
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
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // size
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      error = IndexStorage_purge(indexHandle,
                                 storageId,
                                 NULL  // progressInfo
                                );
    }
    Index_doneList(&indexQueryHandle);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_PURGE_ALL entityId=%"PRIi64" keepStorageId=%"PRIi64,
                                    entityId,
                                    keepStorageId
                                   );
  }

  return error;
}

Errors IndexStorage_purgeAllByName(IndexHandle            *indexHandle,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName,
                                   IndexId                keepStorageId,
                                   ProgressInfo           *progressInfo
                                  )
{
  IndexId          oldUUIDId,oldEntityId,oldStorageId;
  String           oldStorageName;
  StorageSpecifier oldStorageSpecifier;
  Array            uuidIds,entityIds,storageIds;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  ArrayIterator    arrayIterator;
  IndexId          storageId;
  String           storageName;
  IndexId          entityId;

  assert(indexHandle != NULL);
  assert(storageSpecifier != NULL);

// TODO: progress info
UNUSED_VARIABLE(progressInfo);

  if (indexHandle->masterIO == NULL)
  {
    // init variables
    oldStorageName = String_new();
    Storage_initSpecifier(&oldStorageSpecifier);
    Array_init(&uuidIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
    Array_init(&entityIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
    Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

    // get storage ids to purge, enities/UUIDs to prune
    if (storageSpecifier != NULL)
    {
      error = Index_initListStorages(&indexQueryHandle,
                                     indexHandle,
                                     INDEX_ID_ANY, // uuidId
                                     INDEX_ID_ANY, // entityId
                                     NULL,  // jobUUID,
                                     NULL,  // scheduleUUID,
                                     NULL,  // indexIds
                                     0,  // indexIdCount
                                     INDEX_TYPESET_ALL,
                                     INDEX_STATE_SET_ALL,
                                     INDEX_MODE_SET_ALL,
                                     NULL,  // hostName
                                     NULL,  // userName
                                     archiveName,
                                     INDEX_STORAGE_SORT_MODE_NONE,
                                     DATABASE_ORDERING_NONE,
                                     0LL,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuidIds);
        Storage_doneSpecifier(&oldStorageSpecifier);
        String_delete(oldStorageName);
        return error;
      }
      while (Index_getNextStorage(&indexQueryHandle,
                                  &oldUUIDId,
                                  NULL,  // job UUID
                                  &oldEntityId,
                                  NULL,  // schedule UUID
                                  NULL,  // hostName
                                  NULL,  // userName
                                  NULL,  // comment
                                  NULL,  // createdDateTime
                                  NULL,  // archiveType
                                  &oldStorageId,
                                  oldStorageName,
                                  NULL,  // createdDateTime
                                  NULL,  // size
                                  NULL,  // indexState
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
            )
      {
        if (   !INDEX_ID_EQUALS(oldStorageId,keepStorageId)
            && (Storage_parseName(&oldStorageSpecifier,oldStorageName) == ERROR_NONE)
            && Storage_equalSpecifiers(storageSpecifier,archiveName,&oldStorageSpecifier,NULL)
           )
        {
          if (!INDEX_ID_IS_NONE(oldUUIDId)) Array_append(&uuidIds,&oldUUIDId);
          if (!INDEX_ID_IS_DEFAULT_ENTITY(oldEntityId)) Array_append(&entityIds,&oldEntityId);
          if (!INDEX_ID_IS_NONE(oldStorageId)) Array_append(&storageIds,&oldStorageId);
        }
      }
      Index_doneList(&indexQueryHandle);
      if (error != ERROR_NONE)
      {
        Array_done(&storageIds);
        Array_done(&entityIds);
        Array_done(&uuidIds);
        Storage_doneSpecifier(&oldStorageSpecifier);
        String_delete(oldStorageName);
        return error;
      }
    }

    // delete old indizes for same storage file
    ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
    {
      // purge storage
      error = IndexStorage_purge(indexHandle,
                                 storageId,
                                 NULL  // progressInfo
                                );
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); }
    }

    // prune entity index
    ARRAY_ITERATEX(&entityIds,arrayIterator,entityId,error == ERROR_NONE)
    {
      error = IndexEntity_prune(indexHandle,NULL,NULL,entityId);
      if (error != ERROR_NONE)
      {
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
    }

    // prune uuid index
    ARRAY_ITERATEX(&uuidIds,arrayIterator,entityId,error == ERROR_NONE)
    {
      error = IndexUUID_prune(indexHandle,NULL,NULL,entityId);
      if (error != ERROR_NONE)
      {
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
    }

    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuidIds);
      Storage_doneSpecifier(&oldStorageSpecifier);
      String_delete(oldStorageName);
      return error;
    }

    // free resoruces
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuidIds);
    Storage_doneSpecifier(&oldStorageSpecifier);
    String_delete(oldStorageName);
  }
  else
  {
    storageName = Storage_getPrintableName(String_new(),storageSpecifier,archiveName);
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_PURGE_ALL storageName=%'S keepStorageId=%"PRIi64,
                                    storageName,
                                    keepStorageId
                                   );
    String_delete(storageName);
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors IndexStorage_prune(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter,
                          IndexId     storageId
                         )
{
  Errors      error;
  IndexStates indexState;
  String      storageName;

  assert(indexHandle != NULL);

  UNUSED_VARIABLE(doneFlag);
  UNUSED_VARIABLE(deletedCounter);

  // init variables
  storageName = String_new();

  // get storage name, state
  error = Database_get(&indexHandle->databaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 2);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        String_set(storageName,values[0].string);
                        indexState = (IndexStates)values[1].u;

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
                        DATABASE_COLUMN_STRING  ("name"),
                        DATABASE_COLUMN_ENUM    ("state")
                      ),
                      "id=?",
                      DATABASE_FILTERS
                      (
                        DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                      ),
                      NULL,  // groupBy
                      NULL,  // orderBy
                      0LL,
                      1LL
                     );
  if (error != ERROR_NONE)
  {
    String_delete(storageName);
    return error;
  }

  // purge storage if OK and empty
  if ((indexState == INDEX_STATE_OK) && IndexStorage_isEmpty(indexHandle,storageId))
  {
    error = IndexStorage_purge(indexHandle,
                               storageId,
                               NULL  // progressInfo
                              );
    if (error != ERROR_NONE)
    {
      String_delete(storageName);
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Purged storage #%"PRIu64", '%s': empty",
                storageId,
                String_cString(storageName)
               );
  }

  // free resources
  String_delete(storageName);

  return ERROR_NONE;
}

Errors IndexStorage_pruneAll(IndexHandle *indexHandle,
                             bool        *doneFlag,
                             ulong       *deletedCounter
                            )
{
  Array         storageIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);

  // init variables
  Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all storage ids
  error = Database_getIds(&indexHandle->databaseHandle,
                          &storageIds,
                          "storages",
                          "id",
                          "state IN (?,?)",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_UINT(INDEX_STATE_OK),
                            DATABASE_FILTER_UINT(INDEX_STATE_ERROR)
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    return error;
  }

  // prune storages
  ARRAY_ITERATEX(&storageIds,arrayIterator,databaseId,error == ERROR_NONE)
  {
    error = IndexStorage_prune(indexHandle,doneFlag,deletedCounter,INDEX_ID_STORAGE(databaseId));
// TODO: progressInfo
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

Errors IndexStorage_addToNewest(IndexHandle  *indexHandle,
                                IndexId      storageId,
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

  EntryList entryList;
  Errors    error;
  EntryNode *entryNode;

  assert(indexHandle != NULL);
  assert(!INDEX_ID_IS_NONE(storageId));

  // init variables
  List_init(&entryList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
  error = ERROR_NONE;

  // get entries info to add
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 10);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             entryNode = LIST_NEW_NODE(EntryNode);
                             if (entryNode == NULL)
                             {
                               HALT_INSUFFICIENT_MEMORY();
                             }

                             entryNode->entryId                = values[0].id;
                             entryNode->uuidId                 = values[1].id;
                             entryNode->entityId               = values[2].id;
                             entryNode->indexType              = (IndexTypes)values[3].i;
                             entryNode->name                   = String_duplicate(values[4].string);
                             entryNode->timeLastChanged        = values[5].dateTime;
                             entryNode->userId                 = (uint32)values[6].u;
                             entryNode->groupId                = (uint32)values[7].u;
                             entryNode->permission             = (uint32)values[8].u;
                             entryNode->size                   = values[9].u64;
                             entryNode->newest.entryId         = DATABASE_ID_NONE;
                             entryNode->newest.timeLastChanged = 0LL;
                             assert(entryNode->entryId != DATABASE_ID_NONE);

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
                           "    storages.id=? \
                            AND entries.deletedFlag!=TRUE \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                           ),
                           "entries.id",
                           "timeLastChanged DESC",
                           0LL,
                           DATABASE_UNLIMITED
                          );
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;

      return error;
    });
  }

  // find newest entries for entries to add
  ProgressInfo_reset(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_get(&indexHandle->databaseHandle,
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
                          NULL,  // orderBy
                          0LL,
                          1LL
                         );
    });
    if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
    ProgressInfo_step(progressInfo);
  }

  // update/add entries to newest entries
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    if (entryNode->timeLastChanged > entryNode->newest.timeLastChanged)
    {
      INDEX_DOX(error,
                indexHandle,
      {
        if (entryNode->newest.entryId != DATABASE_ID_NONE)
        {
          return Database_update(&indexHandle->databaseHandle,
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
          return Database_insert(&indexHandle->databaseHandle,
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
                                 "EXCLUDED.name=?",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_STRING(entryNode->name)
                                 )
                                );
        }
      });
    }
  }
//fprintf(stderr,"%s, %d: add entries to newest entries %d done\n",__FILE__,__LINE__,List_count(&entryList));

  // free resources
  List_done(&entryList);

  return error;
}

Errors IndexStorage_removeFromNewest(IndexHandle  *indexHandle,
                                     IndexId      storageId,
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

  EntryList  entryList;
  String     entryName;
  Errors     error;
  EntryNode  *entryNode;
  bool       transactionFlag;

  assert(indexHandle != NULL);
  assert(!INDEX_ID_IS_NONE(storageId));

  // init variables
  List_init(&entryList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
  entryName = String_new();
  error     = ERROR_NONE;

  // get entries info to remove
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      if (error == ERROR_NONE)
      {
        error = Database_get(&indexHandle->databaseHandle,
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
                               entryNode->name           = String_duplicate(values[1].string);
                               entryNode->newest.entryId = DATABASE_ID_NONE;
                               assert(entryNode->entryId != DATABASE_ID_NONE);

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
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             NULL,  // groupBy
                             NULL,  // orderBy
                             0LL,
                             DATABASE_UNLIMITED
                            );
      }
      if (error == ERROR_NONE)
      {
        error = Database_get(&indexHandle->databaseHandle,
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
                               entryNode->name           = String_duplicate(values[1].string);
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
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             NULL,  // groupBy
                             NULL,  // orderBy
                             0LL,
                             DATABASE_UNLIMITED
                            );
      }
      if (error == ERROR_NONE)
      {
        error = Database_get(&indexHandle->databaseHandle,
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
                               entryNode->name           = String_duplicate(values[1].string);
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
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             NULL,  // groupBy
                             NULL,  // orderBy
                             0LL,
                             DATABASE_UNLIMITED
                            );
      }
      if (error == ERROR_NONE)
      {
        error = Database_get(&indexHandle->databaseHandle,
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
                               entryNode->name           = String_duplicate(values[1].string);
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
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             NULL,  // groupBy
                             NULL,  // orderBy
                             0LL,
                             DATABASE_UNLIMITED
                            );
      }

      return error;
    });
  }

  // find new newest entries for entries to remove
  ProgressInfo_reset(progressInfo,List_count(&entryList));
  LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
  {
    // wait until index is unused
    WAIT_NOT_IN_USE(5LL*MS_PER_SECOND);

    if ((entryNode->prev == NULL) || !String_equals(entryNode->prev->name,entryNode->name))
    {
      INDEX_DOX(error,
                indexHandle,
      {
        return Database_get(&indexHandle->databaseHandle,
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
                            "ORDER BY entries.timeLastChanged DESC",
                            0LL,
                            1LL
                           );
      });
    }
    ProgressInfo_step(progressInfo);
  }

  // remove/update entries from newest entries
  INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,
  {
    LIST_ITERATEX(&entryList,entryNode,error == ERROR_NONE)
    {
      INDEX_DOX(error,
                indexHandle,
      {
        error = Database_delete(&indexHandle->databaseHandle,
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
          return error;
        }

        if (entryNode->newest.entryId != DATABASE_ID_NONE)
        {
          error = Database_insert(&indexHandle->databaseHandle,
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
                                  "EXCLUDED.name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(entryNode->name)
                                  )
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

  // free resources
  String_delete(entryName);
  List_done(&entryList);

  return error;
}

Errors IndexStorage_updateAggregates(IndexHandle *indexHandle,
                                     IndexId     storageId
                                    )
{
  Errors     error;

  ulong      totalFileCount;
  uint64     totalFileSize;
  ulong      totalImageCount;
  uint64     totalImageSize;
  ulong      totalDirectoryCount;
  ulong      totalLinkCount;
  ulong      totalHardlinkCount;
  uint64     totalHardlinkSize;
  ulong      totalSpecialCount;

  ulong      totalFileCountNewest;
  uint64     totalFileSizeNewest;
  ulong      totalImageCountNewest;
  uint64     totalImageSizeNewest;
  ulong      totalDirectoryCountNewest;
  ulong      totalLinkCountNewest;
  ulong      totalHardlinkCountNewest;
  uint64     totalHardlinkSizeNewest;
  ulong      totalSpecialCountNewest;

  DatabaseId databaseId;

  assert(indexHandle != NULL);
  assert(!INDEX_ID_IS_NONE(storageId));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  // get file aggregate data
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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

  // update aggregate data
  error = Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT  ("totalEntryCount",     totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount),
                            DATABASE_VALUE_UINT64("totalEntrySize",      totalFileSize+totalImageSize+totalHardlinkSize),
                            DATABASE_VALUE_UINT  ("totalFileCount",      totalFileCount),
                            DATABASE_VALUE_UINT64("totalFileSize",       totalFileSize),
                            DATABASE_VALUE_UINT  ("totalImageCount",     totalImageCount),
                            DATABASE_VALUE_UINT64("totalImageSize",      totalImageSize),
                            DATABASE_VALUE_UINT  ("totalDirectoryCount", totalDirectoryCount),
                            DATABASE_VALUE_UINT  ("totalLinkCount",      totalLinkCount),
                            DATABASE_VALUE_UINT  ("totalHardlinkCount",  totalHardlinkCount),
                            DATABASE_VALUE_UINT64("totalHardlinkSize",   totalHardlinkSize),
                            DATABASE_VALUE_UINT  ("totalSpecialCount",   totalSpecialCount)
                          ),
                          "id=?",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId)),
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
  error = Database_get(&indexHandle->databaseHandle,
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
                         DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
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

  // update newest aggregate data
  error = Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "storages",
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
                            DATABASE_VALUE_UINT64("totalFileCountNewest",      totalFileCountNewest),
                            DATABASE_VALUE_UINT64("totalFileSizeNewest",       totalFileSizeNewest),
                            DATABASE_VALUE_UINT64("totalImageCountNewest",     totalImageCountNewest),
                            DATABASE_VALUE_UINT64("totalImageSizeNewest",      totalImageSizeNewest),
                            DATABASE_VALUE_UINT64("totalDirectoryCountNewest", totalDirectoryCountNewest),
                            DATABASE_VALUE_UINT64("totalLinkCountNewest",      totalLinkCountNewest),
                            DATABASE_VALUE_UINT64("totalHardlinkCountNewest",  totalHardlinkCountNewest),
                            DATABASE_VALUE_UINT64("totalHardlinkSizeNewest",   totalHardlinkSizeNewest),
                            DATABASE_VALUE_UINT64("totalSpecialCountNewest",   totalSpecialCountNewest)
                          ),
                          "id=?",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                          )
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // update entity aggregates
  error = Database_getId(&indexHandle->databaseHandle,
                         &databaseId,
                         "storages",
                         "entityId",
                         "id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                         )
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = IndexEntity_updateAggregates(indexHandle,INDEX_ID_ENTITY(databaseId));
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

Errors Index_findStorageById(IndexHandle *indexHandle,
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
                             uint        *totalEntryCount,
                             uint64      *totalEntrySize
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(findStorageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return ERROR_DATABASE_NOT_FOUND;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    return Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 13);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          if (jobUUID             != NULL) String_set(jobUUID,values[0].string);
                          if (scheduleUUID        != NULL) String_set(scheduleUUID,values[1].string);
                          if (uuidId              != NULL) (*uuidId)              = INDEX_ID_UUID  (values[2].id);
                          if (entityId            != NULL) (*entityId)            = INDEX_ID_ENTITY(values[3].id);
                          if (storageName         != NULL) String_set(storageName,values[4].string);
                          if (dateTime            != NULL) (*dateTime)            = values[5].u64;
                          if (size                != NULL) (*size)                = values[6].u64;
                          if (indexState          != NULL) (*indexState)          = values[7].u;
                          if (indexMode           != NULL) (*indexMode)           = values[8].u;
                          if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = values[9].u64;
                          if (errorMessage        != NULL) String_set(errorMessage,values[10].string);
                          if (totalEntryCount     != NULL) (*totalEntryCount)     = values[11].u;
                          if (totalEntrySize      != NULL) (*totalEntrySize)      = values[12].u64;

                          return ERROR_NONE;
                        },NULL),
                        NULL,  // changedRowCount
                        DATABASE_TABLES
                        (
                          "storages \
                               LEFT JOIN entities ON storages.entityId=entities.id \
                               LEFT JOIN uuids ON entities.jobUUID=uuids.jobUUID \
                          "
                        ),
                        DATABASE_FLAG_NONE,
                        DATABASE_COLUMNS
                        (
                          DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                          DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                          DATABASE_COLUMN_UINT    ("COALESCE(uuids.id,0)"),
                          DATABASE_COLUMN_UINT    ("COALESCE(entities.id,0)"),
                          DATABASE_COLUMN_STRING  ("storages.name"),
                          DATABASE_COLUMN_DATETIME("storages.created"),
                          DATABASE_COLUMN_UINT64  ("storages.size"),
                          DATABASE_COLUMN_UINT    ("storages.state"),
                          DATABASE_COLUMN_UINT    ("storages.mode"),
                          DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                          DATABASE_COLUMN_STRING  ("storages.errorMessage"),
                          DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                          DATABASE_COLUMN_UINT64  ("storages.totalEntrySize")
                        ),
                        "    storages.deletedFlag!=TRUE \
                         AND storages.id=? \
                        ",
                        DATABASE_FILTERS
                        (
                          DATABASE_FILTER_KEY (INDEX_DATABASE_ID(findStorageId))
                        ),
                        NULL,  // groupBy
                        NULL,  // orderBy
                        0LL,
                        1LL
                       );
  });

  return error;
}

Errors Index_findStorageByName(IndexHandle            *indexHandle,
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
                               uint                   *totalEntryCount,
                               uint64                 *totalEntrySize
                              )
{
  bool             foundFlag;
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           filterString;

  assert(indexHandle != NULL);
  assert(findStorageSpecifier != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return ERROR_DATABASE_NOT_FOUND;
  }

  // init variables
  foundFlag   = FALSE;
  Storage_initSpecifier(&storageSpecifier);
  storageName = String_new();

  // get archive name
  if (findArchiveName == NULL) findArchiveName = findStorageSpecifier->archiveName;

  // get filter string
  filterString = String_format(String_new(),
                               "    storages.deletedFlag!=TRUE \
                                AND storages.name LIKE '%%%S' \
                               ",
                               findArchiveName
                              );

  INDEX_DOX(error,
            indexHandle,
  {
    return Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 14);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          String_set(storageName,values[5].string);

                          if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
                          {
                            if (Storage_equalSpecifiers(findStorageSpecifier,
                                                        findArchiveName,
                                                        &storageSpecifier,
                                                        NULL
                                                       )
                               )
                            {
                              if (uuidId              != NULL) (*uuidId)              = INDEX_ID_UUID  (values[0].id);
                              if (entityId            != NULL) (*entityId)            = INDEX_ID_ENTITY(values[1].id);
                              if (jobUUID             != NULL) String_set(jobUUID,values[2].string);
                              if (scheduleUUID        != NULL) String_set(scheduleUUID,values[3].string);
                              if (storageId           != NULL) (*storageId)           = INDEX_ID_STORAGE(values[4].id);
                              if (dateTime            != NULL) (*dateTime)            = values[6].u64;
                              if (size                != NULL) (*size)                = values[7].u64;
                              if (indexState          != NULL) (*indexState)          = values[8].u;
                              if (indexMode           != NULL) (*indexMode)           = values[9].u;
                              if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = values[10].u64;
                              if (errorMessage        != NULL) String_set(errorMessage,values[11].string);
                              if (totalEntryCount     != NULL) (*totalEntryCount)     = values[12].u;
                              if (totalEntrySize      != NULL) (*totalEntrySize)      = values[13].u64;

                              foundFlag = TRUE;
                            }
                          }

// TODO: add conditional parameter for get/select
                          return foundFlag ? ERROR_ABORTED : ERROR_NONE;
                        },NULL),
                        NULL,  // changedRowCount
                        DATABASE_TABLES
                        (
                          "storages \
                             LEFT JOIN entities ON storages.entityId=entities.id \
                             LEFT JOIN uuids ON entities.jobUUID=uuids.jobUUID \
                          "
                        ),
                        DATABASE_FLAG_NONE,
                        DATABASE_COLUMNS
                        (
                          DATABASE_COLUMN_UINT    ("COALESCE(uuids.id,0)"),
                          DATABASE_COLUMN_UINT    ("COALESCE(entities.id,0)"),
                          DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                          DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                          DATABASE_COLUMN_UINT    ("storages.id"),
                          DATABASE_COLUMN_STRING  ("storages.name"),
                          DATABASE_COLUMN_DATETIME("storages.created"),
                          DATABASE_COLUMN_UINT64  ("storages.size"),
                          DATABASE_COLUMN_UINT    ("storages.state"),
                          DATABASE_COLUMN_UINT    ("storages.mode"),
                          DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                          DATABASE_COLUMN_STRING  ("storages.errorMessage"),
                          DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                          DATABASE_COLUMN_UINT64  ("storages.totalEntrySize")
                        ),
                        String_cString(filterString),
                        DATABASE_FILTERS
                        (
                        ),
                        NULL,  // groupBy
                        NULL,  // orderBy
                        0LL,
                        DATABASE_UNLIMITED
                       );
  });
  if ((error != ERROR_NONE) && (error != ERROR_ABORTED))
  {
    String_delete(filterString);
    String_delete(storageName);
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }
  assert(!foundFlag || (storageId == NULL) || !INDEX_ID_IS_NONE(*storageId));

  // free resources
  String_delete(filterString);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return foundFlag ? ERROR_NONE : ERROR_DATABASE_ENTRY_NOT_FOUND;
}

Errors Index_findStorageByState(IndexHandle   *indexHandle,
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
                                uint          *totalEntryCount,
                                uint64        *totalEntrySize
                               )
{
  String indexStateSetString;
  Errors error;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return ERROR_DATABASE_NOT_FOUND;
  }

  // init variables
  indexStateSetString = String_new();

  IndexCommon_getIndexStateSetString(indexStateSetString,findIndexStateSet);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 13);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          if (uuidId              != NULL) (*uuidId)              = INDEX_ID_UUID  (values[0].id);
                          if (jobUUID             != NULL) String_set(jobUUID,values[1].string);
                          if (entityId            != NULL) (*entityId)            = INDEX_ID_ENTITY(values[2].id);
                          if (scheduleUUID        != NULL) String_set(scheduleUUID,values[3].string);
                          if (storageId           != NULL) (*storageId)           = INDEX_ID_STORAGE(values[4].id);
                          if (storageName         != NULL) String_set(storageName,values[5].string);
                          if (dateTime            != NULL) (*dateTime)            = values[6].u64;
                          if (size                != NULL) (*size)                = values[7].u64;
//                          if (indexState          != NULL) (*indexState)          = values[8].u;
                          if (indexMode           != NULL) (*indexMode)           = values[8].u;
                          if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = values[9].u64;
                          if (errorMessage        != NULL) String_set(errorMessage,values[10].string);
                          if (totalEntryCount     != NULL) (*totalEntryCount)     = values[11].u;
                          if (totalEntrySize      != NULL) (*totalEntrySize)      = values[12].u64;

                          return ERROR_NONE;
                        },NULL),
                        NULL,  // changedRowCount
                        DATABASE_TABLES
                        (
                          "storages \
                             LEFT JOIN entities ON storages.entityId=entities.id \
                             LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                          "
                        ),
                        DATABASE_FLAG_NONE,
                        DATABASE_COLUMNS
                        (
                          DATABASE_COLUMN_UINT    ("COALESCE(uuids.id,0)"),
                          DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                          DATABASE_COLUMN_UINT    ("COALESCE(entities.id,0)"),
                          DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                          DATABASE_COLUMN_UINT    ("storages.id"),
                          DATABASE_COLUMN_STRING  ("storages.name"),
                          DATABASE_COLUMN_DATETIME("storages.created"),
                          DATABASE_COLUMN_UINT64  ("storages.size"),
                          DATABASE_COLUMN_UINT    ("storages.mode"),
                          DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                          DATABASE_COLUMN_STRING  ("storages.errorMessage"),
                          DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                          DATABASE_COLUMN_UINT64  ("storages.totalEntrySize")
                        ),
                        stringFormat(sqlString,sizeof(sqlString),
                                     "    storages.deletedFlag!=TRUE \
                                      AND (storages.state IN (%s)) \
                                     ",
                                     String_cString(indexStateSetString)
                                    ),
                        DATABASE_FILTERS
                        (
                        ),
                        NULL,  // groupBy
                        NULL,  // orderBy
                        0LL,
                        1LL
                       );
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return error;
  }
  assert((storageId == NULL) || !INDEX_ID_IS_NONE(*storageId));

  // free resources
  String_delete(indexStateSetString);

  return error;
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
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    return getStorageState(indexHandle,
                           INDEX_DATABASE_ID(storageId),
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
                             const char  *errorFormat,
                             ...
                            )
{
  Errors  error;
  va_list arguments;
  String  errorMessage;

  assert(indexHandle != NULL);
  assert((INDEX_TYPE(indexId) == INDEX_TYPE_ENTITY) || (INDEX_TYPE(indexId) == INDEX_TYPE_STORAGE));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // format error message (if any)
  if (errorFormat != NULL)
  {
    va_start(arguments,errorFormat);
    errorMessage = String_vformat(String_new(),errorFormat,arguments);
    va_end(arguments);
  }
  else
  {
    errorMessage = NULL;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      switch (INDEX_TYPE(indexId))
      {
        case INDEX_TYPE_ENTITY:
          error = Database_update(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_ENUM  ("state",        indexState),
                                    DATABASE_VALUE_STRING("errorMessage", errorMessage)
                                  ),
                                  "entityId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(indexId))
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
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_DATETIME("lastChecked", lastCheckedDateTime)
                                    ),
                                    "entityId=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(INDEX_DATABASE_ID(indexId))
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
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_ENUM  ("state",        indexState),
                                    DATABASE_VALUE_STRING("errorMessage", errorMessage)
                                  ),
                                  "id=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(INDEX_DATABASE_ID(indexId)),
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
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_DATETIME("lastChecked", lastCheckedDateTime)
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(INDEX_DATABASE_ID(indexId)),
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
                                    "INDEX_SET_STATE indexId=%"PRIi64" indexState=%'s lastCheckedDateTime=%"PRIu64" errorMessage=%'S",
                                    indexId,
                                    Index_stateToString(indexState,NULL),
                                    lastCheckedDateTime,
                                    errorMessage
                                   );
  }
  if (error != ERROR_NONE)
  {
    if (errorFormat != NULL) String_delete(errorMessage);
    return error;
  }

  // free resources
  if (errorFormat != NULL) String_delete(errorMessage);

  return ERROR_NONE;
}

long Index_countStorageState(IndexHandle *indexHandle,
                             IndexStates indexState
                            )
{
  long count;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  count = -1L;

  INDEX_DOX(count,
            indexHandle,
  {
    uint n;

    if (Database_getUInt(&indexHandle->databaseHandle,
                         &n,
                         "storages",
                         "COUNT(id)",
                         "state=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_UINT(indexState)
                         ),
                         NULL  // group
                        ) == ERROR_NONE
       )
    {
      return (long)n;
    }
    else
    {
      return -1;
    }
  });

  return count;
}

Errors Index_getStoragesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
                              ConstString   jobUUID,
                              ConstString   entityUUID,
                              const IndexId indexIds[],
                              uint          indexIdCount,
                              IndexTypeSet  indexTypeSet,
                              IndexStateSet indexStateSet,
                              IndexModeSet  indexModeSet,
                              ConstString   name,
                              uint          *totalStorageCount,
                              uint64        *totalStorageSize,
                              uint          *totalEntryCount,
                              uint64        *totalEntrySize,
                              uint64        *totalEntryContentSize
                             )
{
  String ftsMatchString;
  String filterString;
  String uuidIdsString,entityIdsString,storageIdsString;
  ulong  i;
  String filterIdsString;
  String string;
  Errors error;
//  double                  totalStorageSize_,totalEntryCount_,totalEntrySize_,totalEntryContentSize_;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64 t0,t1;
  #endif /* INDEX_DEBUG_LIST_INFO */

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (INDEX_TYPE(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_NONE(entityId) || INDEX_ID_IS_ANY(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
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
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_storages.name",name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (INDEX_TYPE(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filterIdsString = String_new();
  string          = String_new();
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","entity.uuidId=%"PRIi64,INDEX_DATABASE_ID(uuidId));
  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%"PRIi64,INDEX_DATABASE_ID(entityId));
  Database_filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  Database_filterAppend(filterString,entityUUID != NULL,"AND","entities.scheduleUUID='%S'",entityUUID);
  Database_filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  Database_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
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
    fprintf(stderr,"%s, %d: ftsMatchString=%s\n",__FILE__,__LINE__,String_cString(ftsMatchString));

    t0 = Misc_getTimestamp();
  #endif /* INDEX_DEBUG_LIST_INFO */

  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    // get storage count, storage size, entry count, entry size
    if (   (totalStorageCount != NULL)
        || (totalStorageSize  != NULL)
        || (totalEntryCount   != NULL)
        || (totalEntrySize    != NULL)
       )
    {
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 4);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                             if (totalStorageSize  != NULL) (*totalStorageSize)  = values[1].u64;
                             if (totalEntryCount   != NULL) (*totalEntryCount)   = values[2].u;
                             if (totalEntrySize    != NULL) (*totalEntrySize)    = values[3].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "storages \
                                LEFT JOIN entities ON entities.id=storages.entityId \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                             "
                           ),
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_UINT  ("COUNT(storages.id)"),
                             DATABASE_COLUMN_UINT64("SUM(storages.size)"),
                             DATABASE_COLUMN_UINT  ("SUM(storages.totalEntryCount)"),
                             DATABASE_COLUMN_UINT64("SUM(storages.totalEntrySize)")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    storages.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
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
      if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (totalEntryContentSize != NULL)
    {
      // get entry content size
      if      (!String_isEmpty(uuidIdsString))
      {
        error = Database_getUInt64(&indexHandle->databaseHandle,
                                   totalEntryContentSize,
                                   "storages \
                                      LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                      LEFT JOIN entities ON entities.id=storages.entityId \
                                      LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                   ",
                                   "SUM(directoryEntries.totalEntrySize)",
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "%s",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL  // group
                                  );
        if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      }
      else if (   !String_isEmpty(entityIdsString)
               || !INDEX_ID_IS_ANY(uuidId)
               || !INDEX_ID_IS_ANY(entityId)
               || (jobUUID != NULL)
               || (entityUUID != NULL)
              )
      {
        error = Database_getUInt64(&indexHandle->databaseHandle,
                                   totalEntryContentSize,
                                   "storages \
                                      LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                      LEFT JOIN entities         ON entities.id=storages.entityId \
                                   ",
                                   "SUM(directoryEntries.totalEntrySize)",
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "%s",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL  // group
                                  );
        if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      }
      else
      {
        error = Database_getUInt64(&indexHandle->databaseHandle,
                                   totalEntryContentSize,
                                   "storages \
                                      LEFT JOIN directoryEntries ON directoryEntries.storageId=storages.id \
                                   ",
                                   "SUM(directoryEntries.totalEntrySize)",
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "%s",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL  // group
                                  );
        if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND) error = ERROR_NONE;
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    Database_deleteFilter(filterString);
    String_delete(ftsMatchString);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    t1 = Misc_getTimestamp();
    fprintf(stderr,"%s, %d: totalStorageCount=%lu totalStorageSize=%lu totalEntryCount_=%lu totalEntrySize_=%lu\n",__FILE__,__LINE__,*totalStorageCount,*totalStorageSize,*totalEntryCount,*totalEntrySize);
    fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // free resources
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  Database_deleteFilter(filterString);
  String_delete(ftsMatchString);

  return ERROR_NONE;
}

Errors Index_updateStorageInfos(IndexHandle *indexHandle,
                                IndexId     storageId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexStorage_updateAggregates(indexHandle,storageId);
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_STORAGE_UPDATE_INFOS storageId=%"PRIi64,
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
                              const char            *jobUUID,
                              const char            *entityUUID,
                              const IndexId         indexIds[],
                              uint                  indexIdCount,
                              IndexTypeSet          indexTypeSet,
                              IndexStateSet         indexStateSet,
                              IndexModeSet          indexModeSet,
                              ConstString           hostName,
                              ConstString           userName,
                              ConstString           name,
                              IndexStorageSortModes sortMode,
                              DatabaseOrdering      ordering,
                              uint64                offset,
                              uint64                limit
                             )
{
  String ftsMatchString;
  String filterString;
  String orderString;
  ulong  i;
  String uuidIdsString,entityIdsString,storageIdsString;
  String filterIdsString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (INDEX_TYPE(uuidId) == INDEX_TYPE_UUID));
  assert(INDEX_ID_IS_ANY(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0L) || (indexIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();
  orderString    = String_new();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_storages.name",name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (INDEX_TYPE(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_appendFormat(storageIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  // get filters
  filterIdsString = String_new();
  string          = String_new();
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_UUID) && !String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_ENTITY) && !String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterIdsString,IN_SET(indexTypeSet,INDEX_TYPE_STORAGE) && !String_isEmpty(storageIdsString),"OR","storages.id IN (%S)",storageIdsString);
  Database_filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%"PRIi64,INDEX_DATABASE_ID(uuidId));
  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(entityId),"AND","storages.entityId=%"PRIi64,INDEX_DATABASE_ID(entityId));
  Database_filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%s'",jobUUID);
  Database_filterAppend(filterString,entityUUID != NULL,"AND","entities.scheduleUUID='%s'",entityUUID);
  Database_filterAppend(filterString,!String_isEmpty(hostName),"AND","entities.hostName LIKE %S",hostName);
  Database_filterAppend(filterString,!String_isEmpty(userName),"AND","storages.userName LIKE %S",userName);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","storages.id IN (SELECT storageId FROM FTS_storages WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  Database_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  // get sort mode, ordering
  IndexCommon_appendOrdering(orderString,
                             sortMode != INDEX_STORAGE_SORT_MODE_NONE,
                             INDEX_STORAGE_SORT_MODE_COLUMNS[sortMode],
                             ordering
                            );

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
    fprintf(stderr,"%s, %d: ftsMatchString=%s\n",__FILE__,__LINE__,String_cString(ftsMatchString));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
//TODO newest
                           "storages \
                              LEFT JOIN entities ON entities.id=storages.entityId \
                              LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("COALESCE(uuids.id,0)"),
                             DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                             DATABASE_COLUMN_KEY     ("COALESCE(entities.id,0)"),
                             DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                             DATABASE_COLUMN_STRING  ("storages.hostName"),
                             DATABASE_COLUMN_STRING  ("storages.userName"),
                             DATABASE_COLUMN_STRING  ("storages.comment"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_UINT    ("entities.type"),
                             DATABASE_COLUMN_KEY     ("storages.id"),
                             DATABASE_COLUMN_STRING  ("storages.name"),
                             DATABASE_COLUMN_DATETIME("storages.created"),
                             DATABASE_COLUMN_UINT64  ("storages.size"),
                             DATABASE_COLUMN_UINT    ("storages.state"),
                             DATABASE_COLUMN_UINT    ("storages.mode"),
                             DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                             DATABASE_COLUMN_STRING  ("storages.errorMessage"),
                             DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                             DATABASE_COLUMN_UINT64  ("storages.totalEntrySize")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    storages.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           "uuids.id,entities.id,storages.id",
                           String_cString(orderString),
                           offset,
                           limit
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    Database_deleteFilter(filterString);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsMatchString);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  Database_deleteFilter(filterString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           entityUUID,
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
                          uint             *totalEntryCount,
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
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           entityUUID,
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
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));

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

//TODO: remove with index version 8 without storage constraint name not NULL
      Misc_getUUID(s);

      // insert storage
      error = Database_insert(&indexHandle->databaseHandle,
                              &databaseId,
                              "storages",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY     ("uuidId",      INDEX_DATABASE_ID(uuidId)),
                                DATABASE_VALUE_KEY     ("entityId",    INDEX_DATABASE_ID(entityId)),
                                DATABASE_VALUE_STRING  ("hostName",    hostName),
                                DATABASE_VALUE_STRING  ("userName",    userName),
                                DATABASE_VALUE_STRING  ("name",        String_isEmpty(storageName) ? s : storageName),
                                DATABASE_VALUE_DATETIME("created",     dateTime),
                                DATABASE_VALUE_UINT64  ("size",        size),
                                DATABASE_VALUE_ENUM    ("state",       indexState),
                                DATABASE_VALUE_ENUM    ("mode",        indexMode),
                                DATABASE_VALUE         ("lastChecked", "NOW()")
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // insert FTS storage
// TODO: do this again with a trigger?
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                   NULL,  // insertRowId
                                   "FTS_storages",
                                   DATABASE_FLAG_NONE,
                                   DATABASE_VALUES
                                   (
                                     DATABASE_VALUE_KEY   ("storageId", databaseId),
                                     DATABASE_VALUE_STRING("name",      storageName)
                                   ),
                                   DATABASE_COLUMNS_NONE,
                                   DATABASE_FILTERS_NONE
                                  );
          break;
        case DATABASE_TYPE_MARIADB:
          error = ERROR_NONE;
          break;
        case DATABASE_TYPE_POSTGRESQL:
          {
            String tokens;

            tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),storageName);
            error = Database_insert(&indexHandle->databaseHandle,
                                     NULL,  // insertRowId
                                     "FTS_storages",
                                     DATABASE_FLAG_NONE,
                                     DATABASE_VALUES
                                     (
                                       DATABASE_VALUE_KEY   ("storageId", databaseId),
                                       DATABASE_VALUE_STRING("name",      "to_tsvector(?)",tokens)
                                     ),
                                     DATABASE_COLUMNS_NONE,
                                     DATABASE_FILTERS_NONE
                                    );
            String_delete(tokens);
          }
          break;
      }
      if (error != ERROR_NONE)
      {
        (void)Database_delete(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "storages",
                              DATABASE_FLAG_NONE,
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(databaseId)
                              ),
                              DATABASE_UNLIMITED
                             );
        return error;
      }

      return ERROR_NONE;
    });

    if (storageId != NULL)
    {
      (*storageId) = INDEX_ID_STORAGE(databaseId);
    }
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

                                      if (StringMap_getIndexId(resultMap,"storageId",storageId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_STORAGE uuidId=%"PRIi64" entityId=%"PRIi64" hostName=%'S userName=%'S storageName=%'S dateTime=%"PRIu64" size=%"PRIu64" indexState=%s indexMode=%s",
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
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return clearStorage(indexHandle,
                      storageId,
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
  assertx(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE,"storageId=%"PRIu64"",storageId.data);

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
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_STRING("hostName", hostName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId)),
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
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_STRING("userName", userName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId)),
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
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_STRING("name", storageName),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId)),
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
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_STRING("name", storageName),
                                    ),
                                    "storageId=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId)),
                                    )
                                   );
            if (error != ERROR_NONE)
            {
              return error;
            }
            break;
          case DATABASE_TYPE_MARIADB:
            // nothing to do (use a view)
            break;
          case DATABASE_TYPE_POSTGRESQL:
            {
              String tokens;

              tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),storageName);
              error = Database_update(&indexHandle->databaseHandle,
                                      NULL,  // changedRowCount
                                      "FTS_storages",
                                      DATABASE_FLAG_NONE,
                                      DATABASE_VALUES
                                      (
                                        DATABASE_VALUE_KEY   ("storageId", INDEX_DATABASE_ID(storageId)),
                                        DATABASE_VALUE_STRING("name",      "to_tsvector(?)",tokens)
                                      ),
                                      "storageId=?",
                                      DATABASE_FILTERS
                                      (
                                        DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId)),
                                      )
                                     );
              String_delete(tokens);
            }
            break;
        }
      }

      if (dateTime != 0LL)
      {
        error = Database_update(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "storages",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_DATETIME("created", dateTime),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
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
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT64("size", size),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
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
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_STRING("comment", comment),
                                ),
                                "id=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
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
                                       storageId,
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
                                    "INDEX_STORAGE_UPDATE storageId=%"PRIi64" hostName=%'S userName=%'S storageName=%'S dateTime=%"PRIu64" storageSize=%"PRIu64" comment=%'S updateNewest=%y",
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

bool Index_hasDeletedStorages(IndexHandle *indexHandle,
                              uint        *deletedStorageCount
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
      return Database_getInt64(&indexHandle->databaseHandle,
                               &n,
                               "storages",
                               "COUNT(id)",
                               "deletedFlag=TRUE",
                               DATABASE_FILTERS
                               (
                               ),
                               NULL  // group
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
  if (deletedStorageCount != NULL) (*deletedStorageCount) = (uint )n;

  return (n > 0LL);
}

bool Index_isDeletedStorage(IndexHandle *indexHandle,
                            IndexId     storageId
                           )
{
  bool deletedFlag;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(deletedFlag,
              indexHandle,
    {
      return !Database_existsValue(&indexHandle->databaseHandle,
                                   "storages",
                                   DATABASE_FLAG_NONE,
                                   "id",
                                   "id=? AND deletedFlag!=TRUE",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                                   )
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
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(emptyFlag,
              indexHandle,
    {
      return IndexStorage_isEmpty(indexHandle,
                            storageId
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
                        ArchiveTypes *archiveType,
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
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_get(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 14);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           if (uuidId              != NULL) (*uuidId)              = INDEX_ID_UUID(values[0].id);
                           if (jobUUID             != NULL) String_set(jobUUID, values[1].string);
                           if (entityId            != NULL) (*entityId)            = INDEX_ID_ENTITY(values[2].id);
                           if (scheduleUUID        != NULL) String_set(scheduleUUID, values[3].string);
                           if (archiveType         != NULL) (*archiveType)         = values[ 4].u;
                           if (storageName         != NULL) String_set(storageName, values[5].string);
                           if (dateTime            != NULL) (*dateTime)            = values[ 6].u64;
                           if (size                != NULL) (*size)                = values[ 7].u64;
                           if (indexState          != NULL) (*indexState)          = values[ 8].u;
                           if (indexMode           != NULL) (*indexMode)           = values[ 9].u;
                           if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = values[10].u64;
                           if (errorMessage        != NULL) String_set(errorMessage, values[11].string);
                           if (totalEntryCount     != NULL) (*totalEntryCount)     = values[12].u;
                           if (totalEntrySize      != NULL) (*totalEntrySize)      = values[13].u64;

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
                           DATABASE_COLUMN_KEY     ("uuids.id"),
                           DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                           DATABASE_COLUMN_KEY     ("entities.id"),
                           DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                           DATABASE_COLUMN_UINT    ("entities.type"),
                           DATABASE_COLUMN_STRING  ("storages.name"),
                           DATABASE_COLUMN_DATETIME("storages.created"),
                           DATABASE_COLUMN_UINT64  ("storages.size"),
                           DATABASE_COLUMN_UINT    ("storages.state"),
                           DATABASE_COLUMN_UINT    ("storages.mode"),
                           DATABASE_COLUMN_DATETIME("storages.lastChecked"),
                           DATABASE_COLUMN_STRING  ("storages.errorMessage"),
                           DATABASE_COLUMN_UINT    ("storages.totalEntryCount"),
                           DATABASE_COLUMN_UINT64  ("storages.totalEntrySize")
                         ),
                         "id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (INDEX_DATABASE_ID(storageId))
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         1LL
                        );
    if (error != ERROR_NONE)
    {
      return ERROR_DATABASE_INDEX_NOT_FOUND;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_purgeAllStoragesById(IndexHandle  *indexHandle,
                                  IndexId      entityId,
                                  IndexId      keepIndexId
                                 )
{
  Errors  error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY);
  assert(INDEX_TYPE(keepIndexId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  INDEX_DOX(error,
            indexHandle,
  {
    return IndexStorage_purgeAllById(indexHandle,
                                     entityId,
                                     keepIndexId,
                                     NULL  // proressInfo
                                    );
  });

  return error;
}

Errors Index_purgeAllStoragesByName(IndexHandle            *indexHandle,
                                    const StorageSpecifier *storageSpecifier,
                                    ConstString            archiveName,
                                    IndexId                keepIndexId
                                   )
{
  Errors  error;

  assert(indexHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(INDEX_TYPE(keepIndexId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  INDEX_DOX(error,
            indexHandle,
  {
    DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      error = IndexStorage_purgeAllByName(indexHandle,
                                          storageSpecifier,
                                          archiveName,
                                          keepIndexId,
                                          NULL  // progressInfo
                                         );
    }

    return error;
  });

  return error;
}

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          IndexId     indexId
                         )
{
  Errors  error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(indexId) == INDEX_TYPE_STORAGE);

  INDEX_DOX(error,
            indexHandle,
  {
    return IndexStorage_prune(indexHandle,
                              NULL,  // doneFlag
                              NULL,  // deletedCounter
                              indexId
                             );
  });

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
