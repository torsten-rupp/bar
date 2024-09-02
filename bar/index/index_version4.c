/***********************************************************************\
*
* $Revision: 6281 $
* $Date: 2016-08-28 12:43:29 +0200 (Sun, 28 Aug 2016) $
* $Author: torsten $
* Contents: index import functions
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
* Name   : getImportStepsVersion4
* Purpose: get number of import steps for index version 4
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL ulong getImportStepsVersion4(IndexHandle *oldIndexHandle,
                                   uint        uuidCountFactor,
                                   uint        entityCountFactor,
                                   uint        storageCountFactor
                                  )
{
  UNUSED_VARIABLE(oldIndexHandle);
  UNUSED_VARIABLE(uuidCountFactor);
  UNUSED_VARIABLE(entityCountFactor);
  UNUSED_VARIABLE(storageCountFactor);

  return 0L;
}

/***********************************************************************\
* Name   : importIndexVersion4
* Purpose: import index from version 4
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion4(IndexHandle *oldIndexHandle,
                                 IndexHandle *newIndexHandle,
                                 ProgressInfo   *progressInfo
                                )
{
  Errors  error;
  IndexId entityId;

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer entities with storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             NULL,  // duration
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               // map type
                               Database_setTableColumnListInt64(toColumnList,
                                                                "type",
                                                                1+Database_getTableColumnListInt64(fromColumnInfo,"type",DATABASE_ID_NONE)
                                                               );

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storages
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromEntityId;
                               DatabaseId toEntityId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
                                                          "storage",
                                                          TRUE,  // transaction flag
                                                          NULL,  // duration
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListInt64(toColumnList,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            Errors     error;
                                                            DatabaseId fromStorageId;
                                                            DatabaseId toStorageId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
//fprintf(stderr,"%s, %d: copy storage %s of entity %"PRIu64": %"PRIu64" -> %"PRIu64"\n",__FILE__,__LINE__,Database_getTableColumnListCString(fromColumnInfo,"name",NULL),toEntityId,fromStorageId,toStorageId);

                                                            error = ERROR_NONE;

                                                            // Note: first directories to update totalEntryCount/totalEntrySize
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "directories",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_DIRECTORY);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                   NULL,  // changedRowCount
                                                                                                                   "INSERT INTO directoryEntries \
                                                                                                                      ( \
                                                                                                                       storageId, \
                                                                                                                       entryId, \
                                                                                                                       name \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %"PRIi64", \
                                                                                                                       %"PRIi64", \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnInfo,"name",NULL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "files",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           DatabaseId toEntryId;
                                                                                           Errors     error;

                                                                                           UNUSED_VARIABLE(userData);

                                                                                           toEntryId = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                                                           assert(toEntryId != DATABASE_ID_NONE);
                                                                                           DIMPORT("import entry %ld -> %ld",Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE),toEntryId);

                                                                                           DIMPORT("import file entry %ld: %s",toEntryId,Database_getTableColumnListCString(fromColumnInfo,"name",NULL));
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
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64" \
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL)
                                                                                                                   );

                                                                                           DIMPORT("import file fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL));
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
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64", \
                                                                                                                        %"PRIu64"\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "images",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_IMAGE);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           DatabaseId toEntryId;
                                                                                           ulong      blockSize;
                                                                                           Errors     error;

                                                                                           UNUSED_VARIABLE(userData);

                                                                                           toEntryId  = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                                                           assert(toEntryId != DATABASE_ID_NONE);
                                                                                           blockSize  = (ulong)Database_getTableColumnListInt64(fromColumnInfo,"blockSize",DATABASE_ID_NONE);
                                                                                           DIMPORT("import entry %ld -> %ld",Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),toEntryId);

                                                                                           DIMPORT("import image entry %ld: %s",toEntryId,Database_getTableColumnListCString(fromColumnInfo,"name",NULL));
                                                                                           error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                    NULL,  // changedRowCount
                                                                                                                    "INSERT INTO imageEntries \
                                                                                                                       ( \
                                                                                                                        entryId, \
                                                                                                                        size, \
                                                                                                                        fileSystemType, \
                                                                                                                        blockSize \
                                                                                                                       ) \
                                                                                                                     VALUES \
                                                                                                                       ( \
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64", \
                                                                                                                        %d, \
                                                                                                                        %lu \
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL),
                                                                                                                    Database_getTableColumnListInt(fromColumnInfo,"fileSystemType",0LL),
                                                                                                                    blockSize
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           DIMPORT("import file fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL));
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
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64", \
                                                                                                                        %"PRIu64"\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"blockOffset",0LL)*(uint64)blockSize,
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"blockCount",0LL)*(uint64)blockSize
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "links",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_LINK);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                   NULL,  // changedRowCount
                                                                                                                   "INSERT INTO linkEntries \
                                                                                                                      ( \
                                                                                                                       storageId, \
                                                                                                                       entryId, \
                                                                                                                       destinationName \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %"PRIi64", \
                                                                                                                       %"PRIi64", \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnInfo,"destinationName",NULL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "hardlinks",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_HARDLINK);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           DatabaseId toEntryId;
                                                                                           Errors     error;

                                                                                           UNUSED_VARIABLE(userData);

                                                                                           toEntryId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                                                           assert(toEntryId != DATABASE_ID_NONE);
                                                                                           DIMPORT("import entry %ld -> %ld",Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE),toEntryId);

                                                                                           DIMPORT("import hardlink entry %ld: %s",toEntryId,Database_getTableColumnListCString(fromColumnInfo,"name",NULL));
                                                                                           error = Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                    CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                    NULL,  // changedRowCount
                                                                                                                    "INSERT INTO hardlinkEntries \
                                                                                                                       ( \
                                                                                                                        entryId, \
                                                                                                                        size \
                                                                                                                       ) \
                                                                                                                     VALUES \
                                                                                                                       ( \
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64" \
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           DIMPORT("import hardlink fragment %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL));
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
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIi64", \
                                                                                                                        %"PRIu64", \
                                                                                                                        %"PRIu64"\
                                                                                                                       ); \
                                                                                                                    ",
                                                                                                                    toEntryId,
                                                                                                                    toStorageId,
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),
                                                                                                                    Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL)
                                                                                                                   );
                                                                                           if (error != ERROR_NONE)
                                                                                           {
                                                                                             return error;
                                                                                           }

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "special",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         NULL,  // duration
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnInfo);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_SPECIAL);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                                                   NULL,  // changedRowCount
                                                                                                                   "INSERT INTO specialEntries \
                                                                                                                      ( \
                                                                                                                       storageId, \
                                                                                                                       entryId, \
                                                                                                                       specialType, \
                                                                                                                       major, \
                                                                                                                       minor \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %"PRIi64", \
                                                                                                                       %"PRIi64", \
                                                                                                                       %d, \
                                                                                                                       %u, \
                                                                                                                       %u \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt(fromColumnInfo,"specialType",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnInfo,"major",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnInfo,"minor",0)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(NULL,NULL),  // progress
                                                                                         "WHERE storageId=%"PRIi64" \
                                                                                          GROUP BY storageId \
                                                                                         ",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            return error;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(NULL,NULL),  // progress
                                                          "WHERE entityId=%"PRIi64"",
                                                          fromEntityId
                                                         );

                               t1 = Misc_getTimestamp();
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }

                               plogMessage(NULL,  // logHandle
                                           LOG_TYPE_INDEX,
                                           "INDEX",
                                           "Imported entity #%"PRIu64": '%s' (%"PRIu64"s)\n",
                                           toEntityId,
                                           Database_getTableColumnListCString(fromColumnInfo,"jobUUID",""),
                                           (t1-t0)/US_PER_SECOND
                                          );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(ProgressInfo_step,progressInfo),
                             DATABASE_FILTERS_NONE,
                             NULL,  // groupBy
                             NULL,  // orderby
                             0L,
                             DATABASE_UNLIMITED
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storage",
                             TRUE,  // transaction flag
                             NULL,  // duration
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);

                               UNUSED_VARIABLE(fromColumnInfo);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findStorageById(oldIndexHandle,
                                                            INDEX_ID_STORAGE(Database_getTableColumnListInt64(fromColumnInfo,"id",DATABASE_ID_NONE)),
                                                            NULL,  // jobUUID
                                                            NULL,  // scheduleUUDI
                                                            NULL,  // uuidId
                                                            &entityId,
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
                                                       NULL,  // findScheduleUUID
                                                       NULL,  // findHostName
                                                       ARCHIVE_TYPE_ANY,
                                                       0LL,  // findCreatedDate
                                                       0L,  // findCreatedTime
                                                       NULL,  // jobUUID
                                                       NULL,  // scheduleUUID
                                                       NULL,  // uuidId
                                                       &entityId,
                                                       NULL,  // createdDateTime
                                                       NULL,  // archiveType
                                                       NULL,  // lastErrorMessage
                                                       NULL,  // totalEntryCount
                                                       NULL  // totalEntrySize
                                                      )
                                  )
                               {
                                 error = ERROR_NONE;
                               }
                               else
                               {
                                 error = Index_newEntity(newIndexHandle,
                                                         Misc_getUUID(jobUUID),
                                                         NULL,  // hostName
                                                         NULL,  // userName
                                                         NULL,  // scheduleUUID
                                                         ARCHIVE_TYPE_FULL,
                                                         0LL,  // createdDateTime
                                                         TRUE,  // locked
                                                         &entityId
                                                        );
                               }
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",INDEX_DATABASE_ID(entityId));

                               return error;
                             },NULL),
                             // post: transfer files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors     error;
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);

                               error = ERROR_NONE;

                               // Note: first directories to update totalEntryCount/totalEntrySize
                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_DIRECTORY);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO directoryEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          name \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnInfo,"name",NULL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64" \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_IMAGE);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO imageEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fileSystemType, \
                                                                                          blockSize, \
                                                                                          blockOffset, \
                                                                                          blockCount \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %"PRIu64", \
                                                                                          %d, \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64" \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL),
                                                                                      Database_getTableColumnListInt(fromColumnInfo,"fileSystemType",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"blockSize",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"blockOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_LINK);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnInfo,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "hardlinks",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListId(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_HARDLINK);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO hardlinkEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64", \
                                                                                          %"PRIu64" \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnInfo,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_SPECIAL);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK_(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %"PRIi64", \
                                                                                          %"PRIi64", \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnInfo,"specialType",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnInfo,"major",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnInfo,"minor",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // pause
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64" \
                                                             GROUP BY storageId \
                                                            ",
                                                            fromStorageId
                                                           );
                               }

                               (void)Index_unlockEntity(newIndexHandle,entityId);

                               return error;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(NULL,NULL),  // progress
                             "WHERE entityId IS NULL"
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
