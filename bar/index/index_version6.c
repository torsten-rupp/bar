/***********************************************************************\
*
* $Revision: 6281 $
* $Date: 2016-08-28 12:43:29 +0200 (Sun, 28 Aug 2016) $
* $Author: torsten $
* Contents: index functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/database.h"

#include "errors.h"
#include "index_definition.h"

#include "index.h"

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

#if 0
//TODO: faster implementation?
/***********************************************************************\
* Name   : upgradeFromVersion7_importFileEntry
* Purpose: import file entry
* Input  : oldIndexHandle,newIndexHandle - index handles
*          storageIdDictionary           - storage id dictionary
*          fromEntryId,toEntry           - from/to entry id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion6_importEntries(IndexHandle *oldIndexHandle,
                                               IndexHandle *newIndexHandle,
                                               Dictionary  *storageIdDictionary,
                                               DatabaseId  fromStorageId,
                                               DatabaseId  toStorageId
                                              )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  int64               size;
  DatabaseId          fromStorageId;
  ValueData           valueData;
  DatabaseId          toStorageId;
  int64               fragmentOffset;
  int64               fragmentSize;

  error = Database_prepare(&databaseQueryHandle,
                          &oldIndexHandle->databaseHandle,
                          "SELECT entries.id, \
                                  entries.name, \
                                  entries.type, \
                                  entries.timeLastAccess, \
                                  entries.timeModified, \
                                  entries.timeLastChanged, \
                                  entries.userId, \
                                  entries.groupId, \
                                  entries.permission, \
                                  \
                                  fileEntries.size, \
                                  fileEntries.fragmentOffset, \
                                  fileEntries.fragmentSize, \
                                  \
                                  imageEntries.size, \
                                  imageEntries.fileSystemType, \
                                  imageEntries.blockSize, \
                                  imageEntries.blockOffset, \
                                  imageEntries.blockCount, \
                                  \
                                  linkEntries.destinationName, \
                                  \
                                  hardlinkEntries.size, \
                                  hardlinkEntries.fragmentOffset, \
                                  hardlinkEntries.fragmentSize, \
                                  \
                                  specialEntries.specialType, \
                                  specialEntries.major, \
                                  specialEntries.minor, \
                           FROM entries \
                             LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                             LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                             LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                             LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                             LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                             LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                           WHERE entries.storageId=%lld \
                          ",
                          fromStorageId
                         );
  if (error == ERROR_NONE)
  {
    if (Database_getNextRow(&databaseQueryHandle,
                            "%lld %lld %lld %lld",
                            &size,
                            &fromStorageId,
                            &fragmentOffset,
                            &fragmentSize
       )
    {
      DIMPORT("import entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);

      switch (type)
      {
        case INDEX_TYPE_FILE:
          error = upgradeFromVersion7_importFileEntry(oldIndexHandle,
                                                      newIndexHandle,
                                                      &storageIdDictionary,
                                                      fromEntryId,
                                                      toEntryId
                                                     );
          break;
        case INDEX_TYPE_IMAGE:
          error = upgradeFromVersion7_importImageEntry(oldIndexHandle,
                                                       newIndexHandle,
                                                       &storageIdDictionary,
                                                       fromEntryId,
                                                       toEntryId
                                                      );
          break;
        case INDEX_TYPE_DIRECTORY:
          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                     &newIndexHandle->databaseHandle,
                                     "directoryEntries",
                                     "directoryEntries",
                                     FALSE,  // transaction flag
                                     &duration,
                                     CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                             DatabaseColumnInfo *toColumnInfo,
                                                             void               *userData
                                                            ),
                                     {
                                       DatabaseId fromStorageId;
                                       DatabaseId toStorageId;

                                       UNUSED_VARIABLE(fromColumnInfo);
                                       UNUSED_VARIABLE(userData);

                                       fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                       assert(fromStorageId != DATABASE_ID_NONE);
                                       if (Dictionary_find(&storageIdDictionary,
                                                           &fromStorageId,
                                                           sizeof(DatabaseId),
                                                           &valueData.value,
                                                           NULL
                                                          )
                                          )
                                       {
                                         toStorageId = *valueData.id;
                                       }
                                       else
                                       {
                                         toStorageId = DATABASE_ID_NONE;
                                       }

                                       (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                       (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                       return ERROR_NONE;
                                     },NULL),
                                     CALLBACK_(NULL,NULL),  // post-copy
                                     CALLBACK_(NULL,NULL),  // pause
                                     CALLBACK_(NULL,NULL),  // progress
                                     "entryId=?",
                                     DATABASE_FILTERS
                                     (
                                       DATABASE_FILTER_KEY(fromEntryId)
                                     ),
                                     NULL,  // groupBy
                                     NULL,  // orderby
                                     0L,
                                     1L
                                    );
          break;
        case INDEX_TYPE_LINK:
          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                     &newIndexHandle->databaseHandle,
                                     "linkEntries",
                                     "linkEntries",
                                     FALSE,  // transaction flag
                                     &duration,
                                     CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                             DatabaseColumnInfo *toColumnInfo,
                                                             void               *userData
                                                            ),
                                     {
                                       DatabaseId fromStorageId;
                                       DatabaseId toStorageId;

                                       UNUSED_VARIABLE(fromColumnInfo);
                                       UNUSED_VARIABLE(userData);

                                       fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                       assert(fromStorageId != DATABASE_ID_NONE);
                                       if (Dictionary_find(&storageIdDictionary,
                                                           &fromStorageId,
                                                           sizeof(DatabaseId),
                                                           &valueData.value,
                                                           NULL
                                                          )
                                          )
                                       {
                                         toStorageId = *valueData.id;
                                       }
                                       else
                                       {
                                         toStorageId = DATABASE_ID_NONE;
                                       }

                                       (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                       (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                       return ERROR_NONE;
                                     },NULL),
                                     CALLBACK_(NULL,NULL),  // post-copy
                                     CALLBACK_(NULL,NULL),  // pause
                                     CALLBACK_(NULL,NULL),  // progress
                                     "entryId=?",
                                     DATABASE_FILTERS
                                     (
                                       DATABASE_FILTER_KEY(fromEntryId)
                                     ),
                                     NULL,  // groupBy
                                     NULL,  // orderby
                                     0L,
                                     1L
                                    );
          break;
        case INDEX_TYPE_HARDLINK:
          error = upgradeFromVersion7_importHardlinkEntry(oldIndexHandle,
                                                          newIndexHandle,
                                                          &storageIdDictionary,
                                                          fromEntryId,
                                                          toEntryId
                                                         );
          break;
        case INDEX_TYPE_SPECIAL:
          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                     &newIndexHandle->databaseHandle,
                                     "specialEntries",
                                     "specialEntries",
                                     FALSE,  // transaction flag
                                     &duration,
                                     CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                             DatabaseColumnInfo *toColumnInfo,
                                                             void               *userData
                                                            ),
                                     {
                                       DatabaseId fromStorageId;
                                       DatabaseId toStorageId;

                                       UNUSED_VARIABLE(fromColumnInfo);
                                       UNUSED_VARIABLE(userData);

                                       fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                       assert(fromStorageId != DATABASE_ID_NONE);
                                       if (Dictionary_find(&storageIdDictionary,
                                                           &fromStorageId,
                                                           sizeof(DatabaseId),
                                                           &valueData.value,
                                                           NULL
                                                          )
                                          )
                                       {
                                         toStorageId = *valueData.id;
                                       }
                                       else
                                       {
                                         toStorageId = DATABASE_ID_NONE;
                                       }

                                       (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                       (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                       return ERROR_NONE;
                                     },NULL),
                                     CALLBACK_(NULL,NULL),  // post-copy
                                     CALLBACK_(NULL,NULL),  // pause
                                     CALLBACK_(NULL,NULL),  // progress
                                     "entryId=?",
                                     DATABASE_FILTERS
                                     (
                                       DATABASE_FILTER_KEY(fromEntryId)
                                     ),
                                     NULL,  // groupBy
                                     NULL,  // orderby
                                     0L,
                                     1L
                                    );
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* not NDEBUG */
          break;
      }

      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO fileEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld \
                                  ); \
                               ",
                               toEntryId,
                               size
                              );
      if (error == ERROR_NONE)
      {
        do
        {
          if (Dictionary_find(storageIdDictionary,
                              &fromStorageId,
                              sizeof(DatabaseId),
                              &valueData.value,
                              NULL
                             )
             )
          {
            toStorageId = *valueData.id;
          }
          else
          {
            toStorageId = DATABASE_ID_NONE;
          }

          DIMPORT("import file fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromEntryId,toEntryId,fragmentOffset,fragmentSize);
          error = Database_execute(&newIndexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT INTO entryFragments \
                                      ( \
                                       entryId, \
                                       storageId, \
                                       offset, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %lld, \
                                       %lld, \
                                       %lld \
                                      ); \
                                   ",
                                   toEntryId,
                                   toStorageId,
                                   fragmentOffset,
                                   fragmentSize
                                  );
         }
         while (   (error == ERROR_NONE)
                && Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize)
               );
       }
    }
    Database_finalize(&databaseQueryHandle);
  }

  return error;
}
#endif

/***********************************************************************\
* Name   : getEntityImportStepsVersion6
* Purpose: get number of import steps for index version 6
* Input  : databaseHandle- database handle
*          entityId      - entity database id
* Output : -
* Return : number of import steps for entity
* Notes  : -
\***********************************************************************/

LOCAL uint64 getEntityImportStepsVersion6(DatabaseHandle *databaseHandle,
                                          DatabaseId     entityId
                                         )
{
  uint64 maxSteps;
  Errors error;
  uint64 n;

  assert(databaseHandle != NULL);

  maxSteps = 0LL;

  // get max. steps (storages, entries)
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "storages",
                             "COUNT(id)",
                             "entityId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY (entityId)
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "entries",
                             "COUNT(id)",
                             "entityId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY (entityId)
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }

  return maxSteps;
}

/***********************************************************************\
* Name   : getImportStepsVersion6
* Purpose: get number of import steps for index version 6
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL uint64 getImportStepsVersion6(DatabaseHandle *databaseHandle)
{
  uint64 maxSteps;
  Errors error;
  uint64 n;

  maxSteps = 0LL;

  // get max. steps (entities+storages+entries)
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "uuids",
                             "COUNT(id)",
                             "id!=0",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }

  Database_get(databaseHandle,
               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
               {
                 DatabaseId entityId;

                 assert(values != NULL);
                 assert(valueCount == 1);

                 UNUSED_VARIABLE(userData);
                 UNUSED_VARIABLE(valueCount);

                 entityId = values[0].id;

                 maxSteps += 1+getEntityImportStepsVersion6(databaseHandle,entityId);

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
                 DATABASE_COLUMN_KEY     ("id"),
               ),
               "deletedFlag!=TRUE \
               ",
               DATABASE_FILTERS
               (
               ),
               NULL,  // groupBy
               NULL,  // orderBy
               0LL,
               DATABASE_UNLIMITED
              );

  return maxSteps;
}

/***********************************************************************\
* Name   : importIndexVersion6
* Purpose: import index version 6
* Input  : oldDatabaseHandle,newDatabaseHandle - database handles
*          progressInfo                        - progress info
*          progressInitFunction                - init function
*          progressInitUserData                - init function user data
*          progressDoneFunction                - done function
*          progressDoneUserData                - done function user data
*          progressInfoFunction                - info function
*          progressInfoUserData                - info function user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion6(DatabaseHandle       *oldDatabaseHandle,
                                 DatabaseHandle       *newDatabaseHandle,
                                 ProgressInfo         *progressInfo,
                                 ProgressInitFunction progressInitFunction,
                                 void                 *progressInitUserData,
                                 ProgressDoneFunction progressDoneFunction,
                                 void                 *progressDoneUserData,
                                 ProgressInfoFunction progressInfoFunction,
                                 void                 *progressInfoUserData
                                )
{
  Errors     error;
  Dictionary storageIdDictionary;

  DIMPORT("version 6");

  // init variables
  error = ERROR_NONE;
  Dictionary_init(&storageIdDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // transfer uuids (if not exists, ignore errors)
  (void)Database_copyTable(oldDatabaseHandle,
                           newDatabaseHandle,
                           "uuids",
                           "uuids",
                           FALSE,  // transaction flag
                           NULL,  // duration
                           CALLBACK_(NULL,NULL),  // pre-copy
                           CALLBACK_(NULL,NULL),  // post-copy
                           CALLBACK_(getCopyPauseCallback(),NULL),
                           CALLBACK_(ProgressInfo_step,progressInfo),
                           DATABASE_FILTERS_NONE,
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  DIMPORT("imported UUIDs");

  // transfer entities with storages and entries
  error = Database_copyTable(oldDatabaseHandle,
                             newDatabaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             NULL,  // duration,
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(fromColumnInfo);
                               UNUSED_VARIABLE(toColumnInfo);
                               UNUSED_VARIABLE(userData);

                               // currently nothing special to do

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storage
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               DatabaseId   fromEntityId;
                               DatabaseId   toEntityId;
                               uint64       maxSteps;
                               ProgressInfo subProgressInfo;
                               Errors       error;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);

                               DIMPORT("import entity %ld -> %ld: jobUUID=%s",fromEntityId,toEntityId,Database_getTableColumnCString(fromColumnInfo,"jobUUID",NULL));

                               maxSteps = getEntityImportStepsVersion6(oldDatabaseHandle,
                                                                       fromEntityId
                                                                      );

                               ProgressInfo_init(&subProgressInfo,
                                                 progressInfo,
                                                 32,  // filterWindowSize
                                                 500,  // reportTime
                                                 maxSteps,
                                                 CALLBACK_(progressInitFunction,progressInitUserData),
                                                 CALLBACK_(progressDoneFunction,progressDoneUserData),
                                                 CALLBACK_(progressInfoFunction,progressInfoUserData),
                                                 "Import entity #%"PRIi64" '%s': ",
                                                 fromEntityId,
                                                 Database_getTableColumnCString(fromColumnInfo,"jobUUID","")
                                                );

                               // transfer storages of entity
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "storage",
                                                          "storages",
                                                          FALSE,  // transaction flag
                                                          NULL,  // duration,
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnInfo);
                                                            UNUSED_VARIABLE(toColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            DatabaseId fromStorageId;
                                                            DatabaseId toStorageId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);

                                                            DIMPORT("import storage %ld -> %ld: %s",fromStorageId,toStorageId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));

                                                            error = Database_copyTable(oldDatabaseHandle,
                                                                                       newDatabaseHandle,
                                                                                       "entries",
                                                                                       "entries",
                                                                                       TRUE,  // transaction flag
                                                                                       NULL,  // duration,
                                                                                       CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                               DatabaseColumnInfo *toColumnInfo,
                                                                                                               void               *userData
                                                                                                              ),
                                                                                       {
                                                                                         UNUSED_VARIABLE(fromColumnInfo);
                                                                                         UNUSED_VARIABLE(userData);

                                                                                         (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);

                                                                                         return ERROR_NONE;
                                                                                       },NULL),
                                                                                       CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                               DatabaseColumnInfo *toColumnInfo,
                                                                                                               void               *userData
                                                                                                              ),
                                                                                       {
                                                                                         DatabaseId fromEntryId;
                                                                                         DatabaseId toEntryId;
                                                                                         Errors     error;

                                                                                         UNUSED_VARIABLE(userData);

                                                                                         fromEntryId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                                                         assert(fromEntryId != DATABASE_ID_NONE);
                                                                                         toEntryId   = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                                                                                         assert(toEntryId != DATABASE_ID_NONE);
                                                                                         DIMPORT("import entry %ld -> %ld",fromEntryId,toEntryId);

                                                                                         error = ERROR_NONE;

                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "fileEntries",
                                                                                                                      "fileEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                              void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnInfo);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);

                                                                                                                        DIMPORT("import file fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnUInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnUInt64(fromColumnInfo,"fragmentSize",0LL));
                                                                                                                        error = Database_insert(newDatabaseHandle,
                                                                                                                                                NULL,  // changedRowCount
                                                                                                                                                "entryFragments",
                                                                                                                                                DATABASE_FLAG_NONE,
                                                                                                                                                DATABASE_VALUES
                                                                                                                                                (
                                                                                                                                                  DATABASE_VALUE_KEY   ("entryId",  toEntryId  ),
                                                                                                                                                  DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                                                                                                  DATABASE_VALUE_UINT64("offset",   Database_getTableColumnUInt64(fromColumnInfo,"fragmentOffset",0LL)),
                                                                                                                                                  DATABASE_VALUE_UINT64("size",     Database_getTableColumnUInt64(fromColumnInfo,"fragmentSize",0LL))
                                                                                                                                                ),
                                                                                                                                                DATABASE_COLUMNS_NONE,
                                                                                                                                                DATABASE_FILTERS_NONE
                                                                                                                                               );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "imageEntries",
                                                                                                                      "imageEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                              void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                       ulong  blockSize;
                                                                                                                       Errors error;

                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);

                                                                                                                        blockSize = (ulong)Database_getTableColumnUInt64(fromColumnInfo,"blockSize",DATABASE_ID_NONE);

                                                                                                                        DIMPORT("import image fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnUInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnUInt64(fromColumnInfo,"fragmentSize",0LL));
                                                                                                                        error = Database_insert(newDatabaseHandle,
                                                                                                                                                NULL,  // changedRowCount
                                                                                                                                                "entryFragments",
                                                                                                                                                DATABASE_FLAG_NONE,
                                                                                                                                                DATABASE_VALUES
                                                                                                                                                (
                                                                                                                                                  DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                                                                                                                  DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                                                                                                  DATABASE_VALUE_UINT64("offset",   Database_getTableColumnUInt64(fromColumnInfo,"blockOffset",0LL)*(uint64)blockSize),
                                                                                                                                                  DATABASE_VALUE_UINT64("size",     Database_getTableColumnUInt64(fromColumnInfo,"blockCount",0LL)*(uint64)blockSize)
                                                                                                                                                ),
                                                                                                                                                DATABASE_COLUMNS_NONE,
                                                                                                                                                DATABASE_FILTERS_NONE
                                                                                                                                               );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "directoryEntries",
                                                                                                                      "directoryEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                              void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnInfo);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "linkEntries",
                                                                                                                      "linkEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                               void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnInfo);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "hardlinkEntries",
                                                                                                                      "hardlinkEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                              void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnInfo);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);

                                                                                                                        DIMPORT("import hardlink fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnUInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnUInt64(fromColumnInfo,"fragmentSize",0LL));
                                                                                                                        error = Database_insert(newDatabaseHandle,
                                                                                                                                                NULL,  // changedRowCount
                                                                                                                                                "entryFragments",
                                                                                                                                                DATABASE_FLAG_NONE,
                                                                                                                                                DATABASE_VALUES
                                                                                                                                                (
                                                                                                                                                  DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                                                                                                                  DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                                                                                                  DATABASE_VALUE_UINT64("offset",   Database_getTableColumnUInt64(fromColumnInfo,"fragmentOffset",0LL)),
                                                                                                                                                  DATABASE_VALUE_UINT64("size",     Database_getTableColumnUInt64(fromColumnInfo,"fragmentSize",0LL))
                                                                                                                                                ),
                                                                                                                                                DATABASE_COLUMNS_NONE,
                                                                                                                                                DATABASE_FILTERS_NONE
                                                                                                                                               );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(oldDatabaseHandle,
                                                                                                                      newDatabaseHandle,
                                                                                                                      "specialEntries",
                                                                                                                      "specialEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      NULL,  // duration,
                                                                                                                      CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                                              DatabaseColumnInfo *toColumnInfo,
                                                                                                                                              void               *userData
                                                                                                                                             ),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnInfo);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                                                                                      "entryId=?",
                                                                                                                      DATABASE_FILTERS
                                                                                                                      (
                                                                                                                        DATABASE_FILTER_KEY(fromEntryId)
                                                                                                                      ),
                                                                                                                      "entryId",
                                                                                                                      NULL,  // orderby
                                                                                                                      0L,
                                                                                                                      DATABASE_UNLIMITED
                                                                                                                     );
                                                                                         }

                                                                                         return error;
                                                                                       },NULL),
                                                                                       CALLBACK_(NULL,NULL),  // pause
                                                                                       CALLBACK_(NULL,NULL),  // progress
                                                                                       "storageId=?",
                                                                                       DATABASE_FILTERS
                                                                                       (
                                                                                         DATABASE_FILTER_KEY(fromStorageId)
                                                                                       ),
                                                                                       NULL,  // groupBy
                                                                                       NULL,  // orderby
                                                                                       0L,
                                                                                       DATABASE_UNLIMITED
                                                                                      );
                                                            if (error != ERROR_NONE)
                                                            {
                                                              return error;
                                                            }

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                          "entityId=?",
                                                          DATABASE_FILTERS
                                                          (
                                                            DATABASE_FILTER_KEY(fromEntityId)
                                                          ),
                                                          NULL,  // groupBy
                                                          NULL,  // orderby
                                                          0L,
                                                          DATABASE_UNLIMITED
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }

                               ProgressInfo_done(&subProgressInfo);

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(ProgressInfo_step,progressInfo),
                             "id!=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(DATABASE_ID_NONE)
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0L,
                             DATABASE_UNLIMITED
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
