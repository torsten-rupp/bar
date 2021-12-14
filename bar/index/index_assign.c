/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index functions
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
#include "index/index_entities.h"
#include "index/index_storages.h"

#include "index/index_assign.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : assignStorageEntriesToStorage
* Purpose: assign all entries of storage to other storage and remove
*          storage index
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toStorageId - to-storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageEntriesToStorage(IndexHandle *indexHandle,
                                           DatabaseId  storageId,
                                           DatabaseId  toStorageId
                                          )
{
  Errors     error;
  DatabaseId toUUIDId,toEntityId;

  /* steps:
     - get to-UUID id, to-entity id
     - assign entries of storage
     - update storage aggregates
     - prune storage
  */

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toStorageId != DATABASE_ID_NONE);

  // init vairables

  error = ERROR_NONE;

  // get to-uuid id, to-entity id
  if (error == ERROR_NONE)
  {
    error = Database_get(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 2);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           toUUIDId   = values[0].id;
                           toEntityId = values[1].u64;

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
                           DATABASE_COLUMN_KEY   ("uuids.id"),
                           DATABASE_COLUMN_KEY   ("entities.id")
                         ),
                         "storages.id=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (storageId)
                         ),
                         NULL,  // orderGroup
                         0LL,
                         1LL
                        );
  }

  // set uuid, entity id of all entries of storage
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "id IN (      SELECT entryId FROM entryFragments   WHERE storageId=? \
                                    UNION SELECT entryId FROM directoryEntries WHERE storageId=? \
                                    UNION SELECT entryId FROM linkEntries      WHERE storageId=? \
                                    UNION SELECT entryId FROM specialEntries   WHERE storageId=? \
                                   ) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "id IN (      SELECT entryId FROM entryFragments   WHERE storageId=? \
                                    UNION SELECT entryId FROM directoryEntries WHERE storageId=? \
                                    UNION SELECT entryId FROM linkEntries      WHERE storageId=? \
                                    UNION SELECT entryId FROM specialEntries   WHERE storageId=? \
                                   ) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }

  // set storage id of all entry fragments/entries of storage
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entryFragments",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("storageId",toStorageId)
                            ),
                            "storageId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "directoryEntries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("storageId",toStorageId)
                            ),
                            "storageId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "linkEntries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("storageId",toStorageId)
                            ),
                            "storageId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "specialEntries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("storageId",toStorageId)
                            ),
                            "storageId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }

  // update aggregates
  if (error == ERROR_NONE)
  {
    error = IndexStorage_updateAggregates(indexHandle,toStorageId);
  }

  // delete storage index
  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
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

  // free resources

  return error;
}

/***********************************************************************\
* Name   : assignEntityStoragesToEntity
* Purpose: assign storages of entity to other entity
* Input  : indexHandle - index handle
*          entityId    - entity database id
*          toUUIDId    - to-UUID database id
*          toEntityId  - to-entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityStoragesToEntity(IndexHandle *indexHandle,
                                          DatabaseId  entityId,
                                          DatabaseId  toUUIDId,
                                          DatabaseId  toEntityId
                                         )
{
  Errors error;

  error = ERROR_NONE;

  // set uuid/entity id of all storages of entity
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "storages",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entityId)
                            )
                           );
  }

  return error;
}

/***********************************************************************\
* Name   : assignEntityEntriesToEntity
* Purpose: assign entries of entity to other entity
* Input  : indexHandle - index handle
*          entityId    - entity database id
*          toUUIDId    - to-UUID database id
*          toEntityId  - to-entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityEntriesToEntity(IndexHandle *indexHandle,
                                         DatabaseId  entityId,
                                         DatabaseId  toUUIDId,
                                         DatabaseId  toEntityId
                                        )
{
  Errors error;

  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "entityId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entityId)
                            )
                           );
  }
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "entryId IN (SELECT entries.id \
                                         FROM entries \
                                           LEFT JOIN entriesNewest ON entriesNewest.entryId=entries.id \
                                         WHERE entries.entityId=? \
                                        ) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entityId)
                            )
                           );
  }

  return error;
}

/***********************************************************************\
* Name   : assignStorageToEntity
* Purpose: assign all entries of storage and storage to other entity
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          toEntityId  - to entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : all entries are assigned to new entity where some fragment
*          is inside storage!
\***********************************************************************/

LOCAL Errors assignStorageToEntity(IndexHandle *indexHandle,
                                   DatabaseId  storageId,
                                   DatabaseId  toEntityId
                                  )
{
  DatabaseId entityId;
  DatabaseId toUUIDId;
  Errors     error;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(toEntityId != DATABASE_ID_NONE);

  /* steps to do:
     - get entity id, UUID id
     - get to-UUID id
     - assign entries of storage
     - assign storage
     - update entity aggregates
     - prune entity
  */

  // init variables

  error = ERROR_NONE;

  // get entity id
  if (error == ERROR_NONE)
  {
    error = Database_getId(&indexHandle->databaseHandle,
                           &entityId,
                           "storages \
                              LEFT JOIN entities ON entities.id=storages.entityId \
                           ",
                           "entities.id",
                           "storages.id=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY (storageId)
                           )
                          );
  }

  // get to-uuid id
  if (error == ERROR_NONE)
  {
    error = Database_getId(&indexHandle->databaseHandle,
                           &toUUIDId,
                           "entities \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                           ",
                           "uuids.id",
                           "entities.id=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY(toEntityId)
                           )
                          );
  }

  // assign storage to new entity
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "storages",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }

  // assign entries of storage to new entity
  if (error == ERROR_NONE)
  {
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY("uuidId",  toUUIDId),
                              DATABASE_VALUE_KEY("entityId",toEntityId)
                            ),
                            "id IN (      SELECT entryId FROM entryFragments   WHERE storageId=? \
                                    UNION SELECT entryId FROM directoryEntries WHERE storageId=? \
                                    UNION SELECT entryId FROM linkEntries      WHERE storageId=? \
                                    UNION SELECT entryId FROM specialEntries   WHERE storageId=? \
                                   ) \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId),
                              DATABASE_FILTER_KEY(storageId)
                            )
                           );
  }

  // update entity aggregates
  if (error == ERROR_NONE)
  {
    error = IndexEntity_updateAggregates(indexHandle,toEntityId);
  }

  // prune entity
  if (error == ERROR_NONE)
  {
    error = IndexEntity_prune(indexHandle,
                                    NULL,  // doneFlag
                                    NULL,  // deletedCounter
                                    entityId
                                   );
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : assignEntityToEntity
* Purpose: assign entity to other entity
* Input  : indexHandle   - index handle
*          entityId      - entity database id
*          toEntityId    - to entity database id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle  *indexHandle,
                                  DatabaseId   entityId,
                                  DatabaseId   toEntityId,
                                  ArchiveTypes toArchiveType
                                 )
{
  Errors     error;
  DatabaseId toUUIDId;

  assert(indexHandle != NULL);

  /* steps to do:
       - assign all storages of entity
       - assign all entries of entity
       - update entity aggregates
       - prune entity
       - set archive type
  */

  error = ERROR_NONE;

  // assign to entity, update aggregates, prune entity
  if (entityId != toEntityId)
  {
    // get to-uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &toUUIDId,
                             "entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             ",
                             "uuids.id",
                             "entities.id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(toEntityId)
                             )
                            );
    }

    // assign entries of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityEntriesToEntity(indexHandle,
                                          entityId,
                                          toUUIDId,
                                          toEntityId
                                         );
    }

    // assign storages of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityStoragesToEntity(indexHandle,
                                           entityId,
                                           toUUIDId,
                                           toEntityId
                                          );
    }

    // update entities aggregates
    if (error == ERROR_NONE)
    {
      error = IndexEntity_updateAggregates(indexHandle,entityId);
    }
    if (error == ERROR_NONE)
    {
      error = IndexEntity_updateAggregates(indexHandle,toEntityId);
    }

    // prune entity
    if (error == ERROR_NONE)
    {
      error = IndexEntity_prune(indexHandle,
                          NULL,  // doneFlag
                          NULL,  // deletedCounter
                          entityId
                         );
    }
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    if (error == ERROR_NONE)
    {
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT("type",toArchiveType)
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(toEntityId)
                              )
                             );
    }
  }

  return error;
}

/***********************************************************************\
* Name   : assignEntityToJob
* Purpose: assign entity to other job
* Input  : indexHandle   - index handle
*          entityId      - entity index id
*          toJobUUID     - to job UUID
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToJob(IndexHandle  *indexHandle,
                               DatabaseId   entityId,
                               ConstString  toJobUUID,
                               ArchiveTypes toArchiveType
                              )
{
  Errors     error;
  DatabaseId uuidId;
  DatabaseId toUUIDId;

  assert(indexHandle != NULL);
  assert(entityId != DATABASE_ID_NONE);
  assert(toJobUUID != NULL);

  /* steps to do:
       - assign all storages of entity
       - assign entity
       - prune UUID
  */

  error = ERROR_NONE;

  if (toJobUUID != NULL)
  {
    // get uuid id, to-uuid id
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &uuidId,
                             "entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                             ",
                             "uuids.id",
                             "entities.id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(entityId)
                             )
                            );
    }
    if (error == ERROR_NONE)
    {
      error = Database_getId(&indexHandle->databaseHandle,
                             &toUUIDId,
                             "uuids",
                             "id",
                             "jobUUID=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_STRING(toJobUUID)
                             )
                            );
    }

    // assign storages of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityStoragesToEntity(indexHandle,
                                           entityId,
                                           toUUIDId,
                                           entityId
                                          );
    }

    // assign entries of entity to other entity
    if (error == ERROR_NONE)
    {
      error = assignEntityEntriesToEntity(indexHandle,
                                          entityId,
                                          toUUIDId,
                                          entityId
                                         );
    }

    // assign entity to job
    if (error == ERROR_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE entities \
                                SET uuidId =%lld, \
                                    jobUUID=%'S \
                                WHERE id=%lld \
                               ",
                               toUUIDId,
                               toJobUUID,
                               entityId
                              );
    }

    // prune UUID
    if (error == ERROR_NONE)
    {
      error = IndexUUID_pruneEmpty(indexHandle,
                                   NULL,  // doneFlag
                                   NULL  // deletedCounter
                                  );
    }
  }

  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    // set entity type
    if (error == ERROR_NONE)
    {
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT("type",toArchiveType)
                              ),
                              "id=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY(entityId)
                              )
                             );
    }
  }

  return error;
}

/***********************************************************************\
* Name   : assignJobToJob
* Purpose: assign all entities of job to other job
*          entity
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          toJobUUID   - to job UUID
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToJob(IndexHandle  *indexHandle,
                            ConstString  jobUUID,
                            ConstString  toJobUUID,
                            ArchiveTypes toArchiveType
                           )
{
  Errors           error;
  DatabaseId       toUUIDId;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entityId;

  assert(indexHandle != NULL);
  assert(!String_isEmpty(toJobUUID));

  /* steps to do:
     - get to-uuid id
     - assign all entities of job
  */

  error = ERROR_NONE;

  // get to-uuid id
  if (error == ERROR_NONE)
  {
    error = Database_getId(&indexHandle->databaseHandle,
                           &toUUIDId,
                           "uuids",
                           "id",
                           "jobUUID=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_STRING(toJobUUID)
                           )
                          );
  }

  // assign all enties of job to other job
  if (error == ERROR_NONE)
  {
    error = Index_initListEntities(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   jobUUID,
                                   NULL,  // scheduleUUID,
                                   ARCHIVE_TYPE_ANY,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name,
                                   INDEX_ENTITY_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    while (   (error == ERROR_NONE)
           && Index_getNextEntity(&indexQueryHandle,
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
      error = assignEntityToEntity(indexHandle,
                                   entityId,
                                   toUUIDId,
                                   entityId
                                  );
    }
    Index_doneList(&indexQueryHandle);
  }

  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    if (error == ERROR_NONE)
    {
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entities",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_UINT("type",toArchiveType)
                              ),
                              "jobUUID=?",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_STRING(jobUUID)
                              )
                             );
    }
  }

  return error;
}

// ----------------------------------------------------------------------

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      ConstString  toJobUUID,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
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
      if      (toJobUUID != NULL)
      {
        if (!INDEX_ID_IS_NONE(entityId) && !INDEX_ID_IS_DEFAULT_ENTITY(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign entity to other job
          error = assignEntityToJob(indexHandle,
                                    Index_getDatabaseId(entityId),
                                    toJobUUID,
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all entities of job to other job
          error = assignJobToJob(indexHandle,
                                 jobUUID,
                                 toJobUUID,
                                 toArchiveType
                                );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
      else if (!INDEX_ID_IS_NONE(toEntityId))
      {
        // assign to other entity

        if (!INDEX_ID_IS_NONE(storageId))
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage to other entity
          error = assignStorageToEntity(indexHandle,
                                        Index_getDatabaseId(storageId),
                                        Index_getDatabaseId(toEntityId)
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!INDEX_ID_IS_NONE(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign entity to other entity
          error = assignEntityToEntity(indexHandle,
                                       Index_getDatabaseId(entityId),
                                       Index_getDatabaseId(toEntityId),
                                       toArchiveType
                                      );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
#if 0
//TODO: not used
          // assign all entities of job to other entity
          error = assignJobToEntity(indexHandle,
                                    jobUUID,
                                    Index_getDatabaseId(toEntityId),
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignJobToEntity");
#endif
        }
      }
      else if (!INDEX_ID_IS_NONE(toStorageId))
      {
        // assign to other storage

        if (!INDEX_ID_IS_NONE(storageId))
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage entries to other storage
          error = assignStorageEntriesToStorage(indexHandle,
                                                Index_getDatabaseId(storageId),
                                                Index_getDatabaseId(toStorageId)
                                               );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!INDEX_ID_IS_NONE(entityId))
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

#if 0
//TODO: not used
          // assign all storage entries of entity to other storage
          error = assignEntityToStorage(indexHandle,
                                        Index_getDatabaseId(entityId),
                                        Index_getDatabaseId(toStorageId)
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignEntityToStorage");
#endif
        }

        if (!String_isEmpty(jobUUID))
        {
#if 0
//TODO: not used
          // assign all storage entries of all entities of job to other storage
          error = assignJobToStorage(indexHandle,
                                     jobUUID,
                                     Index_getDatabaseId(toStorageId)
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }
#else
return ERRORX_(STILL_NOT_IMPLEMENTED,0,"assignJobToStorage");
#endif
        }
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageSize<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkSize<0");

        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ERROR_STILL_NOT_IMPLEMENTED;
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
