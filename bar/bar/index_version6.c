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

/***********************************************************************\
* Name   : upgradeFromVersion6
* Purpose: upgrade index from version 6 to current version
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion6(IndexHandle *oldIndexHandle,
                                 IndexHandle *newIndexHandle
                                )
{
  Errors  error;
  int64   entityCount,storageCount,entriesCount;
  uint64  duration;
  IndexId toEntityId;

  DIMPORT("version 6");

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");
  DIMPORT("fixed broken ids");

  // transfer uuids (if not exists, ignore errors)
  (void)Database_copyTable(&oldIndexHandle->databaseHandle,
                           &newIndexHandle->databaseHandle,
                           "uuids",
                           "uuids",
                           FALSE,  // transaction flag
                           NULL,  // duration
                           CALLBACK_(NULL,NULL),  // pre-copy
                           CALLBACK_(NULL,NULL),  // post-copy
                           CALLBACK_(getCopyPauseCallback(),NULL),
                           CALLBACK_(NULL,NULL),  // progress
                           NULL  // filter
                          );
  DIMPORT("imported UUIDs");

  // get max. steps (entities+storages+entries)
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entityCount,
                                "entities",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &storageCount,
                                "storage",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entriesCount,
                                "entries",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "%lld entities/%lld storages/%lld entries to import",
              entityCount,
              storageCount,
              entriesCount
             );

  initImportProgress(entityCount+storageCount+entriesCount);

  // transfer entities with storages and entries
  duration = 0LL;
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(toColumnList);
                               UNUSED_VARIABLE(userData);

                               // currently nothing special to do

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storage
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromEntityId;
                               DatabaseId toEntityId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               UNUSED_VARIABLE(toColumnList);
                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);
                               DIMPORT("import entity %ld -> %ld: jobUUID=%s",fromEntityId,toEntityId,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL));

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
                                                          "storages",
                                                          FALSE,  // transaction flag
                                                          &duration,
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(toColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListId(toColumnList,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            DatabaseId fromStorageId;
                                                            DatabaseId toStorageId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);
                                                            DIMPORT("import storage %ld -> %ld: %s",fromStorageId,toStorageId,Database_getTableColumnListCString(fromColumnList,"name",NULL));

                                                            t0 = Misc_getTimestamp();
                                                            error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                       &newIndexHandle->databaseHandle,
                                                                                       "entries",
                                                                                       "entries",
                                                                                       TRUE,  // transaction flag
                                                                                       &duration,
                                                                                       CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                       {
                                                                                         UNUSED_VARIABLE(fromColumnList);
                                                                                         UNUSED_VARIABLE(userData);

                                                                                         (void)Database_setTableColumnListId(toColumnList,"entityId",toEntityId);

                                                                                         return ERROR_NONE;
                                                                                       },NULL),
                                                                                       CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                       {
                                                                                         DatabaseId fromEntryId;
                                                                                         DatabaseId toEntryId;
                                                                                         Errors     error;

                                                                                         UNUSED_VARIABLE(userData);

                                                                                         fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                                                         assert(fromEntryId != DATABASE_ID_NONE);
                                                                                         toEntryId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                                                         assert(toEntryId != DATABASE_ID_NONE);
                                                                                         DIMPORT("import entry %ld -> %ld",fromEntryId,toEntryId);

                                                                                         error = ERROR_NONE;

                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "fileEntries",
                                                                                                                      "fileEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                                                        DIMPORT("import file fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                                                        error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                                                 NULL,  // changedRowCount
                                                                                                                                                 "INSERT INTO entryFragments \
                                                                                                                                                    ( \
                                                                                                                                                     entryId, \
                                                                                                                                                     storageId, \
                                                                                                                                                     offset, \
                                                                                                                                                     size\
                                                                                                                                                    ) \
                                                                                                                                                  VALUES \
                                                                                                                                                    ( \
                                                                                                                                                     %lld, \
                                                                                                                                                     %lld, \
                                                                                                                                                     %llu, \
                                                                                                                                                     %llu\
                                                                                                                                                    ); \
                                                                                                                                                 ",
                                                                                                                                                 toEntryId,
                                                                                                                                                 toStorageId,
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                                                );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "imageEntries",
                                                                                                                      "imageEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                       ulong  blockSize;
                                                                                                                       Errors error;

                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                                                        blockSize = (ulong)Database_getTableColumnListInt64(fromColumnList,"blockSize",DATABASE_ID_NONE);

                                                                                                                        DIMPORT("import image fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                                                        error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                                                 NULL,  // changedRowCount
                                                                                                                                                 "INSERT INTO entryFragments \
                                                                                                                                                    ( \
                                                                                                                                                     entryId, \
                                                                                                                                                     storageId, \
                                                                                                                                                     offset, \
                                                                                                                                                     size\
                                                                                                                                                    ) \
                                                                                                                                                  VALUES \
                                                                                                                                                    ( \
                                                                                                                                                     %lld, \
                                                                                                                                                     %lld, \
                                                                                                                                                     %llu, \
                                                                                                                                                     %llu\
                                                                                                                                                    ); \
                                                                                                                                                 ",
                                                                                                                                                 toEntryId,
                                                                                                                                                 toStorageId,
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"blockOffset",0LL)*(uint64)blockSize,
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"blockCount",0LL)*(uint64)blockSize
                                                                                                                                                );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "directoryEntries",
                                                                                                                      "directoryEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "linkEntries",
                                                                                                                      "linkEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "hardlinkEntries",
                                                                                                                      "hardlinkEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                                                        DIMPORT("import hardlink fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                                                        error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                                                 NULL,  // changedRowCount
                                                                                                                                                 "INSERT INTO entryFragments \
                                                                                                                                                    ( \
                                                                                                                                                     entryId, \
                                                                                                                                                     storageId, \
                                                                                                                                                     offset, \
                                                                                                                                                     size\
                                                                                                                                                    ) \
                                                                                                                                                  VALUES \
                                                                                                                                                    ( \
                                                                                                                                                     %lld, \
                                                                                                                                                     %lld, \
                                                                                                                                                     %llu, \
                                                                                                                                                     %llu\
                                                                                                                                                    ); \
                                                                                                                                                 ",
                                                                                                                                                 toEntryId,
                                                                                                                                                 toStorageId,
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                                                 Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                                                );
                                                                                                                        if (error != ERROR_NONE)
                                                                                                                        {
                                                                                                                          return error;
                                                                                                                        }

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }
                                                                                         if (error == ERROR_NONE)
                                                                                         {
                                                                                           error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                      &newIndexHandle->databaseHandle,
                                                                                                                      "specialEntries",
                                                                                                                      "specialEntries",
                                                                                                                      FALSE,  // transaction flag
                                                                                                                      &duration,
                                                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                      {
                                                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                                                        UNUSED_VARIABLE(userData);

                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                        return ERROR_NONE;
                                                                                                                      },NULL),
                                                                                                                      CALLBACK_(NULL,NULL),  // post-copy
                                                                                                                      CALLBACK_(NULL,NULL),  // pause
                                                                                                                      CALLBACK_(importProgress,NULL),  // progress
                                                                                                                      "WHERE entryId=%lld \
                                                                                                                       GROUP BY entryId \
                                                                                                                      ",
                                                                                                                      fromEntryId
                                                                                                                     );
                                                                                         }

                                                                                         return error;
                                                                                       },NULL),
                                                                                       CALLBACK_(NULL,NULL),  // pause
                                                                                       CALLBACK_(NULL,NULL),  // progress
                                                                                       "WHERE storageId=%lld",
                                                                                       fromStorageId
                                                                                      );
                                                            if (error != ERROR_NONE)
                                                            {
                                                              return error;
                                                            }
                                                            t1 = Misc_getTimestamp();

                                                            logImportProgress("Imported storage #%"PRIi64": '%s' (%llus)",
                                                                              toStorageId,
                                                                              Database_getTableColumnListCString(fromColumnList,"name",""),
                                                                              (t1-t0)/US_PER_SECOND
                                                                             );

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(importProgress,NULL),  // progress
                                                          "WHERE entityId=%lld",
                                                          fromEntityId
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }
                               t1 = Misc_getTimestamp();

                               plogMessage(NULL,  // logHandle
                                           LOG_TYPE_INDEX,
                                           "INDEX",
                                           "Imported entity #%"PRIi64": '%s' (%llus)",
                                           toEntityId,
                                           Database_getTableColumnListCString(fromColumnList,"jobUUID",""),
                                           (t1-t0)/US_PER_SECOND
                                          );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(importProgress,NULL),  // progress
                             "WHERE id!=0"
                            );
  if (error != ERROR_NONE)
  {
    doneImportProgress();
    return error;
  }

  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storages",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer storage and create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findStorageById(oldIndexHandle,
                                                            INDEX_ID_STORAGE(Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE)),
                                                            jobUUID,
                                                            NULL,  // scheduleUUDI
                                                            NULL,  // uuidId
                                                            NULL,  // entityId
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
                                   && Index_findEntity(newIndexHandle,
                                                       INDEX_ID_NONE,
                                                       jobUUID,
                                                       NULL,  // scheduleUUID
                                                       NULL,  // hostName
                                                       ARCHIVE_TYPE_NONE,
                                                       0LL,  // createdDateTime
                                                       NULL,  // jobUUID
                                                       NULL,  // scheduleUUID
                                                       NULL,  // uuidId
                                                       &toEntityId,
                                                       NULL,  // createdDateTime
                                                       NULL,  // archiveType
                                                       NULL,  // lastErrorMessage
                                                       NULL,  // totalEntryCount
                                                       NULL  // totalEntrySize,
                                                      )
                                  )
                               {
                                 error = ERROR_NONE;
                               }
                               else
                               {
                                 error = Index_newEntity(newIndexHandle,
                                                         Misc_getUUID(jobUUID),
                                                         NULL,  // scheduleUUID
                                                         NULL,  // hostName
                                                         NULL,  // userName
                                                         ARCHIVE_TYPE_FULL,
                                                         0LL,  // createdDateTime
                                                         TRUE,  // locked
                                                         &toEntityId
                                                        );
                               }
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",Index_getDatabaseId(toEntityId));

                               return error;
                             },NULL),
                             // post: copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromStorageId != DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toStorageId != DATABASE_ID_NONE);

                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "entries",
                                                          "entries",
                                                          TRUE,  // transaction flag
                                                          &duration,
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListId(toColumnList,"entityId",Index_getDatabaseId(toEntityId));

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            DatabaseId fromEntryId;
                                                            DatabaseId toEntryId;
                                                            Errors     error;

                                                            UNUSED_VARIABLE(userData);

                                                            fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
                                                            toEntryId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);
                                                            DIMPORT("import entry %ld -> %ld",fromEntryId,toEntryId);

                                                            error = ERROR_NONE;

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "fileEntries",
                                                                                         "fileEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           DIMPORT("import file fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                    NULL,  // changedRowCount
                                                                                                                    "INSERT INTO entryFragments \
                                                                                                                       ( \
                                                                                                                        entryId, \
                                                                                                                        storageId, \
                                                                                                                        offset, \
                                                                                                                        size\
                                                                                                                       ) \
                                                                                                                     VALUES \
                                                                                                                       ( \
                                                                                                                        %lld, \
                                                                                                                        %lld, \
                                                                                                                        %llu, \
                                                                                                                        %llu\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "imageEntries",
                                                                                         "imageEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           DIMPORT("import image fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                    NULL,  // changedRowCount
                                                                                                                    "INSERT INTO entryFragments \
                                                                                                                       ( \
                                                                                                                        entryId, \
                                                                                                                        storageId, \
                                                                                                                        offset, \
                                                                                                                        size\
                                                                                                                       ) \
                                                                                                                     VALUES \
                                                                                                                       ( \
                                                                                                                        %lld, \
                                                                                                                        %lld, \
                                                                                                                        %llu, \
                                                                                                                        %llu\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
//fprintf(stderr,"%s, %d: copy d\n",__FILE__,__LINE__);
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "directoryEntries",
                                                                                         "directoryEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "linkEntries",
                                                                                         "linkEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "hardlinkEntries",
                                                                                         "hardlinkEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           DIMPORT("import hardlink fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                    NULL,  // changedRowCount
                                                                                                                    "INSERT INTO entryFragments \
                                                                                                                       ( \
                                                                                                                        entryId, \
                                                                                                                        storageId, \
                                                                                                                        offset, \
                                                                                                                        size\
                                                                                                                       ) \
                                                                                                                     VALUES \
                                                                                                                       ( \
                                                                                                                        %lld, \
                                                                                                                        %lld, \
                                                                                                                        %llu, \
                                                                                                                        %llu\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                    Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "specialEntries",
                                                                                         "specialEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // post-copy
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),  // progress
                                                                                         "WHERE entryId=%lld \
                                                                                          GROUP BY entryId \
                                                                                         ",
                                                                                         fromEntryId
                                                                                        );
                                                            }

                                                            return error;
                                                          },NULL),
                                                          CALLBACK_(NULL,NULL),  // pause
                                                          CALLBACK_(NULL,NULL),  // progress
                                                          "WHERE storageId=%lld",
                                                          fromStorageId
                                                         );
                               (void)Index_unlockEntity(newIndexHandle,toEntityId);
                               t1 = Misc_getTimestamp();

                               plogMessage(NULL,  // logHandle
                                           LOG_TYPE_INDEX,
                                           "INDEX",
                                           "Imported storage #"PRIi64": '%s' (%llus)",
                                           toStorageId,
                                           Database_getTableColumnListCString(fromColumnList,"name",""),
                                           (t1-t0)/US_PER_SECOND
                                          );

                               return error;
                             },NULL),
                             CALLBACK_(NULL,NULL),
                             CALLBACK_(importProgress,NULL),  // progress
                             "WHERE entityId IS NULL"
                            );
  if (error != ERROR_NONE)
  {
    doneImportProgress();
    return error;
  }

  doneImportProgress();

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
