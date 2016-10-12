/***********************************************************************\
*
* $Revision: 6281 $
* $Date: 2016-08-28 12:43:29 +0200 (Sun, 28 Aug 2016) $
* $Author: torsten $
* Contents: index functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "database.h"
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
* Name   : upgradeFromVersion5
* Purpose: upgrade index from version 5 to current version
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion5(IndexHandle *oldIndexHandle,
                                 IndexHandle *newIndexHandle
                                )
{
  Errors error;

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
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               // map type
                               Database_setTableColumnListInt64(toColumnList,
                                                                "type",
                                                                1+Database_getTableColumnListInt64(fromColumnList,"type",DATABASE_ID_NONE)
                                                               );

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storage
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               IndexId fromEntityId;
                               IndexId toEntityId;
                               uint64  t0;
                               uint64  t1;
                               Errors  error;

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);
//fprintf(stderr,"%s, %d: jobUUID=%s\n",__FILE__,__LINE__,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL));

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
                                                          "storage",
                                                          FALSE,  // transaction flag
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListInt64(toColumnList,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            Errors  error;
                                                            IndexId fromStorageId;
                                                            IndexId toStorageId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);

//fprintf(stderr,"%s, %d: start storage %llu -> %llu\n",__FILE__,__LINE__,fromStorageId,toStorageId);
                                                            error = ERROR_NONE;

                                                            // Note: first directories to update totalEntryCount/totalEntrySize
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "directories",
                                                                                         "entries",
                                                                                         TRUE,  // transaction flag
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_DIRECTORY);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
                                                                                                                   NULL,  // changedRowCount
                                                                                                                   "INSERT INTO directoryEntries \
                                                                                                                      ( \
                                                                                                                       storageId, \
                                                                                                                       entryId, \
                                                                                                                       name \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnList,"name",NULL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
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
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
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
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_IMAGE);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %d, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"fileSystemType",0),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
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
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_LINK);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
                                                                                                                   NULL,  // changedRowCount
                                                                                                                   "INSERT INTO linkEntries \
                                                                                                                      ( \
                                                                                                                       storageId, \
                                                                                                                       entryId, \
                                                                                                                       destinationName \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
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
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_HARDLINK);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
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
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_SPECIAL);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           return Database_execute(&newIndexHandle->databaseHandle,
                                                                                                                   CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                                                       %lld, \
                                                                                                                       %lld, \
                                                                                                                       %d, \
                                                                                                                       %u, \
                                                                                                                       %u \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   toStorageId,
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),  // pause
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            return error;
                                                          },NULL),
                                                          CALLBACK(pauseCallback,NULL),
                                                          "WHERE entityId=%lld",
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
                                           "Imported entity #%llu: '%s' (%llus)\n",
                                           toEntityId,
                                           Database_getTableColumnListCString(fromColumnList,"jobUUID",""),
                                           (t1-t0)/US_PER_SECOND
                                          );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK(pauseCallback,NULL),
                             NULL  // filter
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storage",
                             FALSE,  // transaction flag
                             // pre: transfer storage and create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findStorageById(oldIndexHandle,
                                                            INDEX_ID_STORAGE(Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE)),
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
                                   && Index_findEntityByJobUUID(newIndexHandle,
                                                                jobUUID,
                                                                NULL,  // uuidId
                                                                &entityId,
                                                                NULL,  // scheduleUUDI
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
                                                         ARCHIVE_TYPE_FULL,
                                                         0LL,  // createdDateTime
                                                         &entityId
                                                        );
                               }
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // post: copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               IndexId fromStorageId;
                               IndexId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromStorageId != DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toStorageId != DATABASE_ID_NONE);

                               error = ERROR_NONE;

                               // Note: first directories to update totalEntryCount/totalEntrySize
                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_DIRECTORY);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO directoryEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          name \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"name",NULL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
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
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
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
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_IMAGE);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %d, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListInt(fromColumnList,"fileSystemType",0),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
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
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_LINK);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
                                                                                      NULL,  // changedRowCount
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          storageId, \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
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
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_HARDLINK);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
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
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_SPECIAL);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),  // databaseRowFunction
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
                                                                                          %lld, \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      toStorageId,
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),  // pause
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return error;
                             },NULL),
                             CALLBACK(pauseCallback,NULL),
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
