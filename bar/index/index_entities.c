/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index entity functions
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
#include "index/index_storages.h"
#include "index/index_uuids.h"

#include "index_entities.h"

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

// TODO: not used?
#if 0
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
   error = Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 3);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          String_setBuffer(uuid,values[0].text.data,values[0].text.length);
                          String_setBuffer(name1,values[1].text.data,values[1].text.length);
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
                                                String_setBuffer(name2,values[1].text.data,values[0].text.length);

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

                          if (error -= ERROR_NONE)
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
                                                   assert(valueCount == 2);

                                                   UNUSED_VARIABLE(userData);
                                                   UNUSED_VARIABLE(valueCount);

                                                   storageId = values[0].id;
                                                   String_setBuffer(name2,values[1].text.data,values[0].text.length);

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
                                                                             DATABASE_VALUE_UINT("entityId", entityId),
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
                          DATABASE_COLUMN_STRING  ("uuid"),
                          DATABASE_COLUMN_STRING  ("name"),
                          DATABASE_COLUMN_DATETIME("created")
                        ),
                        "entityId=0",
                        DATABASE_FILTERS
                        (
                        ),
                        "ORDER BY id,created ASC",  // orderGroup
                        0LL,
                        DATABASE_UNLIMITED
                       );

    return error;
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
#endif

/***********************************************************************\
* Name   : isEmptyEntity
* Purpose: check if entity if empty
* Input  : indexHandle - index handle
*          entityId    - entity database id
* Output : -
* Return : TRUE iff entity is empty
* Notes  : -
\***********************************************************************/

LOCAL bool isEmptyEntity(IndexHandle *indexHandle,
                         DatabaseId  entityId
                        )
{
  assert(indexHandle != NULL);

  return    (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "storages",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "entityId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(entityId)
                                  )
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "entries",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "entityId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(entityId)
                                  )
                                 )
         && !Database_existsValue(&indexHandle->databaseHandle,
                                  "entriesNewest",
                                  DATABASE_FLAG_NONE,
                                  "id",
                                  "entityId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(entityId)
                                  )
                                 );
}

// TODO: remove?
#if 0
/***********************************************************************\
* Name   : refreshEntitiesInfos
* Purpose: refresh entities infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshEntitiesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 NULL,  // jobUUID,
                                 NULL,  // scheduldUUID
                                 ARCHIVE_TYPE_ANY,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_ENTITY_SORT_MODE_NONE,
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextEntity(&indexQueryHandle,
                               NULL,  // uuidId,
                               NULL,  // jobUUID,
                               NULL,  // scheduleUUID,
                               &entityId,
                               NULL,  // archiveType,
                               NULL,  // createdDateTime,
                               NULL,  // lastErrorMessage
                               NULL,  // totalSize
                               NULL,  // totalEntryCount
                               NULL,  // totalEntrySize
                               NULL  // lockedCount
                              )
          )
    {
      error = Index_updateEntityInfos(indexHandle,entityId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos fail (error: %s)",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}
#endif

/***********************************************************************\
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO
#if 0
// not used
LOCAL Errors rebuildNewestInfo(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entryId;
  String           name;
  uint64           size;
  uint64           timeModified;


  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // indexIds
                                0L,  // indexIdCount
                                NULL,  // entryIds
                                0L,  // entryIdCount
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                FALSE,  // newestOnly
                                FALSE,  // fragmentsCount
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  name = String_new();
  while (Index_getNextEntry(&indexQueryHandle,
                            NULL,  // uuidId
                            NULL,  // jobUUID
                            NULL,  // entityId
                            NULL,  // scheduleUUID
                            NULL,  // hostName
                            NULL,  // userName
                            NULL,  // archiveType
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // storageDateTime
                            &entryId,
                            name,
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // destinationName
                            NULL,  // fileSystemType
                            &size,
                            &timeModified,
                            NULL,  // userId
                            NULL,  // groupId
                            NULL,  // permission
                            NULL  // fragmentCount
                           )
        )
  {
  }
  String_delete(name);
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}
#endif

// ----------------------------------------------------------------------

#if 0
//TODO: trigger or implementation here?
/***********************************************************************\
* Name   : insertUpdateNewestEntry
* Purpose: insert or update newest entry
* Input  : indexHandle     - index handle
*          entryId         - entry database id
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          name            - file name
*          size            - size [bytes]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors insertUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     DatabaseId  uuidId,
                                     DatabaseId  entityId,
                                     ConstString name,
                                     IndexTypes  indexType,
                                     uint64      size,
                                     uint64      timeLastChanged,
                                     uint32      userId,
                                     uint32      groupId,
                                     uint32      permission
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          newestEntryId;
  uint64              newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    // atomic add/get entry
    DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // get existing newest entry
      error = Database_get(&databaseStatementHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 2);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             newestEntryId         = values[0].id;
                             newestTimeLastChanged = values[1].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest"
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("id"),
                             DATABASE_COLUMN_DATETIME("timeLastChanged")
                           ),
                           "name=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_STRING(name)
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        newestEntryId         = DATABASE_ID_NONE;
        newestTimeLastChanged = 0LL;
      }

      // insert/update newest
      if (error == ERROR_NONE)
      {
        if (timeLastChanged > newestTimeLastChanged)
        {
          if (newestEntryId != DATABASE_ID_NONE)
          {
            // update
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId",         entryId),
                                      DATABASE_VALUE_KEY   ("uuidId",          uuidId),
                                      DATABASE_VALUE_KEY   ("entityId",        entityId),
                                      DATABASE_VALUE_UINT  ("type",            indexType),
                                      DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT  ("userId",          userId),
                                      DATABASE_VALUE_UINT  ("groupId",         groupId),
                                      DATABASE_VALUE_UINT  ("permission",      permission)
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(newestEntryId)
                                    )
                                   );
          }
          else
          {
            // insert
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId",         entryId),
                                      DATABASE_VALUE_KEY   ("uuidId",          uuidId),
                                      DATABASE_VALUE_KEY   ("entityId",        entityId),
                                      DATABASE_VALUE_UINT  ("type",            indexType),
                                      DATABASE_VALUE_STRING("name",            name),
                                      DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT  ("userId",          userId),
                                      DATABASE_VALUE_UINT  ("groupId",         groupId),
                                      DATABASE_VALUE_UINT  ("permission",      permission)
                                    )
                                   );
          }
        }
      }
    }

    return error;
  });

  return error;
}

/***********************************************************************\
* Name   : removeUpdateNewestEntry
* Purpose: remove or update newest entry
* Input  : indexHandle - index handle
*          entryId     - entry database id
*          name        - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     ConstString name
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          newestEntryId;
  uint64              newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY   ("entryId")
                                    DATABASE_VALUE_KEY   ("uuidId")
                                    DATABASE_VALUE_KEY   ("entityId")
                                    DATABASE_VALUE_UINT  ("type")
                                    DATABASE_VALUE_STRING("name")
                                    DATABASE_VALUE_UINT64("timeLastChanged")
                                    DATABASE_VALUE_UINT  ("userId")
                                    DATABASE_VALUE_UINT  ("groupId")
                                    DATABASE_VALUE_UINT  ("permission")
                                  ),
                                  DATABASE_TABLES
                                  (
                                    "entries"
                                  ),
                                  DATABASE_COLUMNS
                                  (
                                    DATABASE_COLUMN_KEY   ("id"),
                                    DATABASE_COLUMN_KEY   ("uuidId"),
                                    DATABASE_COLUMN_KEY   ("entityId"),
                                    DATABASE_COLUMN_UINT  ("type"),
                                    DATABASE_COLUMN_STRING("name"),
                                    DATABASE_COLUMN_UINT64("timeLastChanged"),
                                    DATABASE_COLUMN_UINT  ("userId"),
                                    DATABASE_COLUMN_UINT  ("groupId"),
                                    DATABASE_COLUMN_UINT  ("permission")
                                  ),
                                  "name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(name)
                                  ),
                                  "ORDER BY timeLastChanged DESC",
                                  0LL,
                                  1LL
                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}
#endif

/*---------------------------------------------------------------------*/

Errors IndexEntity_purge(IndexHandle *indexHandle,
                         bool        *doneFlag,
                         ulong       *deletedCounter,
                         DatabaseId  entityId
                        )
{
  String       string;
  Errors       error;
  DatabaseId   uuidId;
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64       createdDateTime;
  ArchiveTypes archiveType;

  assert(indexHandle != NULL);

  // init variables
  string = String_new();

  if (indexHandle->masterIO == NULL)
  {
    if (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
    {
      // get uuid id, job UUID, created date/time, archive type
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 4);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             uuidId          = values[0].id;
                             String_set(jobUUID,values[1].string);
                             createdDateTime = values[2].u64;
                             archiveType     = values[3].u;

                             return ERROR_NONE;
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
                             DATABASE_COLUMN_KEY     ("uuids.id"),
                             DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_UINT    ("entities.type")
                           ),
                           "entities.id=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY (entityId)
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        uuidId          = DATABASE_ID_NONE;
        String_clear(jobUUID);
        createdDateTime = 0LL;
        archiveType     = ARCHIVE_TYPE_NONE;
      }

      // delete entity from index
      error = Database_delete(&indexHandle->databaseHandle,
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
      if (error != ERROR_NONE)
      {
        String_delete(string);
        return error;
      }

      // purge skipped entries of entity
      error = IndexCommon_purge(indexHandle,
                                doneFlag,
                                deletedCounter,
                                "skippedEntries",
                                "entityId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(entityId)
                                )
                               );
      if (error != ERROR_NONE)
      {
        String_delete(string);
        return error;
      }

      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Purged entity #%"PRIu64", job %s, type '%s' created at %s: no archives",
                  entityId,
                  String_cString(jobUUID),
                  Archive_archiveTypeToString(archiveType),
                  String_cString(Misc_formatDateTime(String_clear(string),createdDateTime,FALSE,NULL))
                 );

      // prune UUID
      if (uuidId != DATABASE_ID_NONE)
      {
        error = IndexUUID_prune(indexHandle,
                                doneFlag,
                                deletedCounter,
                                uuidId
                               );
        if (error != ERROR_NONE)
        {
          String_delete(string);
          return error;
        }
      }
    }
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_UUID_PURGE entityId=%"PRIi64,
                                    INDEX_ID_ENTITY(entityId)
                                   );
  }

  // free resources
  String_delete(string);


  return ERROR_NONE;
}

Errors IndexEntity_prune(IndexHandle *indexHandle,
                         bool        *doneFlag,
                         ulong       *deletedCounter,
                         DatabaseId  entityId
                        )
{
  uint   lockedCount;
  Errors error;

  assert(indexHandle != NULL);

  if (entityId != INDEX_DEFAULT_ENTITY_DATABASE_ID)
  {
    // get locked count
    error = Database_getUInt(&indexHandle->databaseHandle,
                             &lockedCount,
                             "entities",
                             "lockedCount",
                             "id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(entityId)
                             ),
                             NULL  // group
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete if not locked and entity is empty
    if ((lockedCount == 0L) && isEmptyEntity(indexHandle,entityId))
    {
      error = IndexEntity_purge(indexHandle,
                                doneFlag,
                                deletedCounter,
                                entityId
                               );
      if (error != ERROR_NONE)
      {
        return error;
      }

      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Purged entity #%"PRIu64": empty",
                  entityId
                 );
    }
  }

  // free resources

  return ERROR_NONE;
}

Errors IndexEntity_pruneAll(IndexHandle *indexHandle,
                            bool        *doneFlag,
                            ulong       *deletedCounter
                           )
{
  Array         entityIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    entityId;

  assert(indexHandle != NULL);

  // init variables
  Array_init(&entityIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all entity ids (Note: skip default entity!)
  error = Database_getIds(&indexHandle->databaseHandle,
                          &entityIds,
                          "entities",
                          "id",
                          "id!=? \
                          ",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_KEY(INDEX_DEFAULT_ENTITY_DATABASE_ID)
                          )
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&entityIds);
    return error;
  }

  // prune entities
  ARRAY_ITERATEX(&entityIds,arrayIterator,entityId,error == ERROR_NONE)
  {
    error = IndexEntity_prune(indexHandle,doneFlag,deletedCounter,entityId);
  }
  if (error != ERROR_NONE)
  {
    Array_done(&entityIds);
    return error;
  }

  // free resources
  Array_done(&entityIds);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : IndexEntity_updateAggregates
* Purpose: update entity aggregates
* Input  : indexHandle - index handle
*          entityId    - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_updateAggregates(IndexHandle *indexHandle,
                                          DatabaseId  entityId
                                         )
{
  Errors error;
  ulong  totalFileCount;
  uint64 totalFileSize;
  ulong  totalImageCount;
  uint64 totalImageSize;
  ulong  totalDirectoryCount;
  ulong  totalLinkCount;
  ulong  totalHardlinkCount;
  uint64 totalHardlinkSize;
  ulong  totalSpecialCount;

  assert(indexHandle != NULL);

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
                         "entries \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_FILE),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
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
                         "entries \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
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
                         "entries"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_DIRECTORY),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
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
                         "entries"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_LINK),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
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
                         "entries \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entries.id \
                        "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
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
                         "entries"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entries.id)")
                       ),
                       "    entries.type=? \
                        AND entries.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_SPECIAL),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // update entity aggregate data
  error = Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "entities",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT  ("totalEntryCount",     totalFileCount+
                                                                         totalImageCount+
                                                                         totalDirectoryCount+
                                                                         totalLinkCount+
                                                                         totalHardlinkCount+
                                                                         totalSpecialCount
                                                 ),
                            DATABASE_VALUE_UINT64("totalEntrySize",      totalFileSize+
                                                                         totalImageSize+
                                                                         totalHardlinkSize
                                                 ),
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
                            DATABASE_FILTER_KEY(entityId),
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
                         totalFileSize  = values[1].u64;

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entriesNewest \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_FILE),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalFileCount = 0L;
    totalFileSize  = 0LL;
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
                         totalImageSize  = values[1].u64;

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entriesNewest \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_IMAGE),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalImageCount = 0L;
    totalImageSize  = 0LL;
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
                         "entriesNewest"
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_DIRECTORY),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalDirectoryCount = 0L;
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
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_LINK),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalLinkCount = 0L;
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
                         totalHardlinkSize  = values[1].u64;

                         return ERROR_NONE;
                       },NULL),
                       NULL,  // changedRowCount
                       DATABASE_TABLES
                       (
                         "entriesNewest \
                            LEFT JOIN entryFragments ON entryFragments.entryId=entriesNewest.id \
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)"),
                         DATABASE_COLUMN_UINT64("SUM(entryFragments.size)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_HARDLINK),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalHardlinkCount = 0L;
    totalHardlinkSize  = 0LL;
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
                         "
                       ),
                       DATABASE_FLAG_NONE,
                       DATABASE_COLUMNS
                       (
                         DATABASE_COLUMN_UINT  ("COUNT(DISTINCT entriesNewest.id)")
                       ),
                       "    entriesNewest.type=? \
                        AND entriesNewest.entityId=? \
                       ",
                       DATABASE_FILTERS
                       (
                         DATABASE_FILTER_UINT(INDEX_TYPE_SPECIAL),
                         DATABASE_FILTER_KEY (entityId)
                       ),
                       NULL,  // groupBy
                       NULL,  // orderby
                       0LL,
                       1LL
                      );
  if (error != ERROR_NONE)
  {
    totalSpecialCount = 0L;
  }

  // update newest aggregate data
  error = Database_update(&indexHandle->databaseHandle,
                          NULL,  // changedRowCount
                          "entities",
                          DATABASE_FLAG_NONE,
                          DATABASE_VALUES
                          (
                            DATABASE_VALUE_UINT  ("totalEntryCountNewest",      totalFileCount
                                                                               +totalImageCount
                                                                               +totalDirectoryCount
                                                                               +totalLinkCount
                                                                               +totalHardlinkCount
                                                                               +totalSpecialCount
                                                 ),
                            DATABASE_VALUE_UINT64("totalEntrySizeNewest",       totalFileSize
                                                                               +totalImageSize
                                                                               +totalHardlinkSize
                                                 ),
                            DATABASE_VALUE_UINT  ("totalFileCountNewest",      totalFileCount),
                            DATABASE_VALUE_UINT64("totalFileSizeNewest",       totalFileSize),
                            DATABASE_VALUE_UINT  ("totalImageCountNewest",     totalImageCount),
                            DATABASE_VALUE_UINT64("totalImageSizeNewest",      totalImageSize),
                            DATABASE_VALUE_UINT  ("totalDirectoryCountNewest", totalDirectoryCount),
                            DATABASE_VALUE_UINT  ("totalLinkCountNewest",      totalLinkCount),
                            DATABASE_VALUE_UINT  ("totalHardlinkCountNewest",  totalHardlinkCount),
                            DATABASE_VALUE_UINT64("totalHardlinkSizeNewest",   totalHardlinkSize),
                            DATABASE_VALUE_UINT  ("totalSpecialCountNewest",   totalSpecialCount)
                          ),
                          "id=?",
                          DATABASE_FILTERS
                          (
                            DATABASE_FILTER_KEY(entityId)
                          )
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

bool Index_findEntity(IndexHandle  *indexHandle,
                      IndexId      findEntityId,
                      ConstString  findJobUUID,
                      ConstString  findScheduleUUID,
                      ConstString  findHostName,
                      ArchiveTypes findArchiveType,
                      uint64       findCreatedDateTime,
                      String       jobUUID,
                      String       scheduleUUID,
                      IndexId      *uuidId,
                      IndexId      *entityId,
                      ArchiveTypes *archiveType,
                      uint64       *createdDateTime,
                      String       lastErrorMessage,
                      uint         *totalEntryCount,
                      uint64       *totalEntrySize
                     )
{
  String filterString;
  Errors error;
  bool   result;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // get filters
  filterString = Database_newFilter();
  Database_filterAppend(filterString,!INDEX_ID_IS_NONE(findEntityId),"AND","entities.id=%lld",Index_getDatabaseId(findEntityId));
  Database_filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","entities.jobUUID=%'S",findJobUUID);
  Database_filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","entities.scheduleUUID=%'S",findScheduleUUID);
  Database_filterAppend(filterString,!String_isEmpty(findHostName),"AND","entities.hostName=%'S",findHostName);
  Database_filterAppend(filterString,findArchiveType != ARCHIVE_TYPE_ANY,"AND","entities.type=%u",findArchiveType);
  Database_filterAppend(filterString,findCreatedDateTime != 0LL,"AND","entities.created=%"PRIu64,findCreatedDateTime);
//fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,String_cString(filterString));

  result = FALSE;

  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_get(&indexHandle->databaseHandle,
                        CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                        {
                          assert(values != NULL);
                          assert(valueCount == 9);

                          UNUSED_VARIABLE(userData);
                          UNUSED_VARIABLE(valueCount);

                          if (uuidId           != NULL) (*uuidId)          = INDEX_ID_UUID(values[0].id);
                          if (jobUUID          != NULL) String_set(jobUUID,values[1].string);
                          if (entityId         != NULL) (*entityId)        = INDEX_ID_ENTITY(values[2].id);
                          if (scheduleUUID     != NULL) String_set(scheduleUUID,values[3].string);
                          if (createdDateTime  != NULL) (*createdDateTime) = values[4].id;
                          if (archiveType      != NULL) (*archiveType)     = values[5].u;
                          if (lastErrorMessage != NULL) String_set(lastErrorMessage,values[6].string);
                          if (totalEntryCount  != NULL) (*totalEntryCount) = values[7].u;
                          if (totalEntrySize   != NULL) (*totalEntrySize)  = values[8].u64;

                          result = TRUE;

                          return ERROR_NONE;
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
                          DATABASE_COLUMN_KEY     ("COALESCE(uuids.id,0)"),
                          DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                          DATABASE_COLUMN_KEY     ("COALESCE(entities.id,0)"),
                          DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                          DATABASE_COLUMN_DATETIME("entities.created"),
                          DATABASE_COLUMN_UINT    ("entities.type"),
                          DATABASE_COLUMN_STRING  ("(SELECT storages.errorMessage FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storages.created DESC LIMIT 1)"),
                          DATABASE_COLUMN_UINT    ("(SELECT SUM(storages.totalEntryCount) FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID)"),
                          DATABASE_COLUMN_UINT64  ("(SELECT SUM(storages.totalEntrySize)  FROM entities LEFT JOIN storages ON storages.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID)")
                        ),
                        stringFormat(sqlString,sizeof(sqlString),
                                     "    entities.deletedFlag!=TRUE \
                                      AND %s",
                                      String_cString(filterString)
                                    ),
                        DATABASE_FILTERS
                        (
                        ),
                        NULL,  // groupBy
                        NULL,  // orderby
                        0LL,
                        1LL
                       );
  });
  if (error != ERROR_NONE)
  {
    Database_deleteFilter(filterString);
    return FALSE;
  }

  // free resources
  Database_deleteFilter(filterString);

  return result;
}

Errors Index_getEntitiesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
//TODO: remove?
                              ConstString   jobUUID,
                              const IndexId indexIds[],
                              uint          indexIdCount,
                              ConstString   name,
                              uint          *totalStorageCount,
                              uint64        *totalStorageSize,
                              uint          *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
UNUSED_VARIABLE(entityId);
UNUSED_VARIABLE(jobUUID);
UNUSED_VARIABLE(indexIds);
UNUSED_VARIABLE(indexIdCount);
UNUSED_VARIABLE(name);
UNUSED_VARIABLE(totalStorageCount);
UNUSED_VARIABLE(totalStorageSize);
UNUSED_VARIABLE(totalEntryCount);
UNUSED_VARIABLE(totalEntrySize);

return ERROR_STILL_NOT_IMPLEMENTED;
}

Errors Index_updateEntityInfos(IndexHandle *indexHandle,
                               IndexId     entityId
                              )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexEntity_updateAggregates(indexHandle,Index_getDatabaseId(entityId));
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ENTITY_UPDATE_INFOS entityId=%lld",
                                    entityId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListEntities(IndexQueryHandle     *indexQueryHandle,
                              IndexHandle          *indexHandle,
                              IndexId              uuidId,
                              ConstString          jobUUID,
                              ConstString          scheduleUUID,
                              ArchiveTypes         archiveType,
                              IndexStateSet        indexStateSet,
                              IndexModeSet         indexModeSet,
                              ConstString          name,
                              IndexEntitySortModes sortMode,
                              DatabaseOrdering     ordering,
                              uint64               offset,
                              uint64               limit
                             )
{
  String ftsMatchString;
  String filterString;
  String string;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert(INDEX_ID_IS_ANY(uuidId) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

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

  // get filters
  string = String_new();
  Database_filterAppend(filterString,TRUE,"AND","entities.id!=%lld",INDEX_DEFAULT_ENTITY_DATABASE_ID);
  Database_filterAppend(filterString,!INDEX_ID_IS_ANY(uuidId),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  Database_filterAppend(filterString,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  Database_filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);
  Database_filterAppend(filterString,archiveType != ARCHIVE_TYPE_ANY,"AND","entities.type=%u",archiveType);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","EXISTS(SELECT storageId FROM FTS_storages WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,TRUE,"AND","storages.state IN (%S)",IndexCommon_getIndexStateSetString(string,indexStateSet));
  Database_filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storages.mode IN (%S)",IndexCommon_getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // get sort mode, ordering
  IndexCommon_appendOrdering(orderString,
                             sortMode != INDEX_ENTITY_SORT_MODE_NONE,
                             INDEX_ENTITY_SORT_MODE_COLUMNS[sortMode],
                             ordering
                            );

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntities ------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: jobUUID=%s\n",__FILE__,__LINE__,String_cString(jobUUID));
    fprintf(stderr,"%s, %d: archiveType=%u\n",__FILE__,__LINE__,archiveType);
    fprintf(stderr,"%s, %d: ftsMatchString=%s\n",__FILE__,__LINE__,String_cString(ftsMatchString));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  if (String_isEmpty(ftsMatchString))
  {
    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      return Database_select(&indexQueryHandle->databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             "entities \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                LEFT JOIN storages ON storages.entityId=entities.id AND storages.deletedFlag!=TRUE \
                             ",
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_KEY     ("COALESCE(uuids.id,0)"),
                               DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                               DATABASE_COLUMN_KEY     ("entities.id"),
                               DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                               DATABASE_COLUMN_DATETIME("entities.created"),
                               DATABASE_COLUMN_UINT    ("entities.type"),
                               DATABASE_COLUMN_STRING  ("(SELECT errorMessage FROM storages WHERE storages.entityId=entities.id ORDER BY created DESC LIMIT 1)"),
                               DATABASE_COLUMN_UINT64  ("(SELECT SUM(size) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT    ("(SELECT SUM(storages.totalEntryCount) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT64  ("(SELECT SUM(storages.totalEntrySize) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT    ("entities.lockedCount")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    entities.deletedFlag!=TRUE \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             "uuids.id,entities.id",
                             String_cString(orderString),
                             offset,
                             limit
                            );
    });
  }
  else
  {
    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      return Database_select(&indexQueryHandle->databaseStatementHandle,
                             &indexHandle->databaseHandle,
                             "FTS_storages \
                                LEFT JOIN storages ON storages.id=FTS_storages.storageId AND storages.deletedFlag!=TRUE \
                                LEFT JOIN entities ON entities.id=storages.entityId AND entities.deletedFlag!=TRUE \
                                LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                             ",
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_KEY     ("COALESCE(uuids.id,0)"),
                               DATABASE_COLUMN_STRING  ("entities.jobUUID"),
                               DATABASE_COLUMN_KEY     ("entities.id"),
                               DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                               DATABASE_COLUMN_DATETIME("entities.created"),
                               DATABASE_COLUMN_UINT    ("entities.type"),
                               DATABASE_COLUMN_STRING  ("(SELECT errorMessage FROM storages WHERE storages.entityId=entities.id ORDER BY created DESC LIMIT 1)"),
                               DATABASE_COLUMN_UINT64  ("(SELECT SUM(size) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT    ("(SELECT SUM(storages.totalEntryCount) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT64  ("(SELECT SUM(storages.totalEntrySize) FROM storages WHERE entityId=entities.id)"),
                               DATABASE_COLUMN_UINT    ("entities.lockedCount")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    entities.deletedFlag!=TRUE \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             "uuids.id,entities.id",
                             String_cString(orderString),
                             offset,
                             limit
                            );
    });
  }
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    Database_deleteFilter(filterString);
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
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *uuidId,
                         String           jobUUID,
                         String           scheduleUUID,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         uint             *lastErrorCode,
                         String           lastErrorData,
                         uint64           *totalSize,
                         uint             *totalEntryCount,
                         uint64           *totalEntrySize,
                         uint             *lockedCount
                        )
{
  DatabaseId uuidDatabaseId,entityDatatabaseId;

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
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatatabaseId,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
// TODO:                           lastErrorCode
                           lastErrorData,
                           totalSize,
                           totalEntryCount,
                           totalEntrySize,
                           lockedCount
                          )
     )
  {
    return FALSE;
  }
  if (uuidId   != NULL) (*uuidId  ) = INDEX_ID_ENTITY(uuidDatabaseId);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityDatatabaseId);

  return TRUE;
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       const char   *jobUUID,
                       const char   *scheduleUUID,
                       const char   *hostName,
                       const char   *userName,
                       ArchiveTypes archiveType,
                       uint64       createdDateTime,
                       bool         locked,
                       IndexId      *entityId
                      )
{
  Errors     error;
  DatabaseId uuidId;

  assert(indexHandle != NULL);
  assert(jobUUID != NULL);
  assert(entityId != NULL);

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

      // create UUID (if it does not exists)
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "uuids",
                              DATABASE_FLAG_IGNORE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_CSTRING("jobUUID", jobUUID)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get uuid id
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
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

      // create entity
      if (createdDateTime == 0LL) createdDateTime = Misc_getCurrentDateTime();

      error = Database_insert(&indexHandle->databaseHandle,
                              &databaseId,
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY     ("uuidId",       uuidId),
                                DATABASE_VALUE_CSTRING ("jobUUID",      jobUUID),
                                DATABASE_VALUE_CSTRING ("scheduleUUID", scheduleUUID),
                                DATABASE_VALUE_CSTRING ("hostName",     hostName),
                                DATABASE_VALUE_CSTRING ("userName",     userName),
                                DATABASE_VALUE_DATETIME("created",      createdDateTime),
                                DATABASE_VALUE_UINT    ("type",         archiveType),
                                DATABASE_VALUE_UINT    ("lockedCount",  locked ? 1 : 0)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*entityId) = INDEX_ID_ENTITY(databaseId);

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

                                      if (StringMap_getInt64 (resultMap,"entityId",entityId,INDEX_ID_NONE))
                                      {
                                        return ERROR_NONE;
                                      }
                                      else
                                      {
                                        return ERROR_EXPECTED_PARAMETER;
                                      }
                                    },NULL),
                                    "INDEX_NEW_ENTITY jobUUID=%s scheduleUUID=%s hostName=%'s userName=%'s archiveType=%s createdDateTime=%"PRIu64" locked=%y",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? scheduleUUID : "",
                                    hostName,
                                    userName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime,
                                    locked
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_updateEntity(IndexHandle  *indexHandle,
                          IndexId      entityId,
                          const char   *jobUUID,
                          const char   *scheduleUUID,
                          const char   *hostName,
                          const char   *userName,
                          ArchiveTypes archiveType,
                          uint64       createdDateTime
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

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
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_CSTRING ("jobUUID",      jobUUID),
                                DATABASE_VALUE_CSTRING ("scheduleUUID", scheduleUUID),
                                DATABASE_VALUE_CSTRING ("hostName",     hostName),
                                DATABASE_VALUE_CSTRING ("userName",     userName),
                                DATABASE_VALUE_DATETIME("created",      (createdDateTime != 0LL) ? createdDateTime : Misc_getCurrentDateTime()),
                                DATABASE_VALUE_UINT    ("type",         archiveType)
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
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
                                    CALLBACK_(NULL,NULL),
                                    "INDEX_UPDATE_ENTITY jobUUID=%s scheduleUUID=%s hostName=%'s userName=%'s archiveType=%s createdDateTime=%"PRIu64,
                                    jobUUID,
                                    (scheduleUUID != NULL) ? scheduleUUID : "",
                                    hostName,
                                    userName,
                                    Archive_archiveTypeToString(archiveType),
                                    createdDateTime
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_lockEntity(IndexHandle *indexHandle,
                        IndexId     entityId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entities",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE("lockedCount", "lockedCount+1"),
                            ),
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
                            )
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_unlockEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

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
      return Database_update(&indexHandle->databaseHandle,
                             NULL,  // changedRowCount
                             "entities",
                             DATABASE_FLAG_NONE,
                             DATABASE_VALUES
                             (
                               DATABASE_VALUE("lockedCount", "lockedCount-1"),
                             ),
                             "id=? AND lockedCount>0",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
                             )
                            );
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ENTITY_UNLOCK entityId=%lld",
                                    entityId
                                   );
  }

  return error;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors        error;
  DatabaseId    uuidId;
  Array         storageIds;
  ArrayIterator arrayIterator;
  DatabaseId    storageId;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    // init variables
    Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

    INDEX_DOX(error,
              indexHandle,
    {
      // get UUID id
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
                             "entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             ",
                             "uuids.id",
                             "entities.id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
                             )
                            );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get storages to delete
      error = Database_getIds(&indexHandle->databaseHandle,
                              &storageIds,
                              "storages \
                                 LEFT JOIN entities ON entities.id=storages.entityId \
                              ",
                              "storages.id",
                              "entities.id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    // delete storages
    if (error == ERROR_NONE)
    {
      ARRAY_ITERATEX(&storageIds,arrayIterator,storageId,error == ERROR_NONE)
      {
        error = Index_deleteStorage(indexHandle,INDEX_ID_STORAGE(storageId));
      }
    }
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    INDEX_DOX(error,
              indexHandle,
    {
      // set deleted flag
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_BOOL("deletedFlag", TRUE),
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
                              )
                             );
      if (error == ERROR_NONE)
      {
        return error;
      }

      // prune UUID
      error = IndexUUID_prune(indexHandle,
                              NULL,  // doneFlag
                              NULL,  // deletedCounter
                              uuidId
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      return error;
    }

    if (error == ERROR_NONE)
    {
      // trigger clean-up thread
      Semaphore_signalModified(&indexThreadTrigger,SEMAPHORE_SIGNAL_MODIFY_ALL);
    }

    // free resources
    Array_done(&storageIds);
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ENTITY_DELETE entityId=%lld",
                                    entityId
                                   );
  }

  return error;
}

bool Index_isDeletedEntity(IndexHandle *indexHandle,
                           IndexId     entityId
                          )
{
  bool deletedFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(deletedFlag,
              indexHandle,
    {
      return !Database_existsValue(&indexHandle->databaseHandle,
                                   "entities",
                                   DATABASE_FLAG_NONE,
                                   "id",
                                   "id=? AND deletedFlag!=1",
                                   DATABASE_FILTERS
                                   (
                                     DATABASE_FILTER_KEY(Index_getDatabaseId(entityId))
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

bool Index_isEmptyEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                        )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return isEmptyEntity(indexHandle,
                         Index_getDatabaseId(entityId)
                        );
  });

  return emptyFlag;
}

Errors Index_purgeEntity(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_ENTITY);
  assert(Index_getDatabaseId(indexId) != INDEX_DEFAULT_ENTITY_DATABASE_ID);

  // prune storages of entity if not default entity
  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexEntity_purge(indexHandle,
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
                                    "INDEX_ENTITY_PURGE entityId=%lld",
                                    indexId
                                   );
  }

  return error;
}

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_ENTITY);
  assert(Index_getDatabaseId(indexId) != INDEX_DEFAULT_ENTITY_DATABASE_ID);

  // prune storages of entity if not default entity
  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return IndexEntity_prune(indexHandle,
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
                                    "INDEX_ENTITY_PRUNE entityId=%lld",
                                    indexId
                                   );
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
