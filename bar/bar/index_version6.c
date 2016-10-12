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
  Errors error;

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer uuids (if not exists, ignore errors)
  (void)Database_copyTable(&oldIndexHandle->databaseHandle,
                           &newIndexHandle->databaseHandle,
                           "uuids",
                           "uuids",
                           FALSE,  // transaction flag
                           CALLBACK(NULL,NULL),  // pre-copy
                           CALLBACK(NULL,NULL),  // post-copy
                           CALLBACK(pauseCallback,NULL),
                           NULL  // filter
                          );

  // transfer entities with storages and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
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
                               IndexId fromEntityId;
                               IndexId toEntityId;
                               uint64  t0;
                               uint64  t1;
                               Errors  error;

                               UNUSED_VARIABLE(toColumnList);
                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);
//fprintf(stderr,"%s, %d: entity %llu -> %llu: copy storage jobUUID=%s\n",__FILE__,__LINE__,fromEntityId,toEntityId,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL));

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
                                                            UNUSED_VARIABLE(toColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListInt64(toColumnList,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            IndexId fromStorageId;
                                                            IndexId toStorageId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);

 //fprintf(stderr,"%s, %d: copy entries %llu %llu\n",__FILE__,__LINE__,fromStorageId,toStorageId);
                                                            return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                      &newIndexHandle->databaseHandle,
                                                                                      "entries",
                                                                                      "entries",
                                                                                      TRUE,  // transaction flag
                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                      {
                                                                                        UNUSED_VARIABLE(fromColumnList);
                                                                                        UNUSED_VARIABLE(userData);

                                                                                        (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                        return ERROR_NONE;
                                                                                      },NULL),
                                                                                      CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                      {
                                                                                        Errors  error;
                                                                                        IndexId fromEntryId;
                                                                                        IndexId toEntryId;

                                                                                        UNUSED_VARIABLE(userData);

                                                                                        fromEntryId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                                                                                        assert(fromEntryId != DATABASE_ID_NONE);
                                                                                        toEntryId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                                                                                        assert(toEntryId != DATABASE_ID_NONE);

                                                                                        error = ERROR_NONE;

                                                                                        if (error == ERROR_NONE)
                                                                                        {
 //fprintf(stderr,"%s, %d: copy file %llu -> %llu\n",__FILE__,__LINE__,fromEntryId,toEntryId);
                                                                                          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "fileEntries",
                                                                                                                     "fileEntries",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       UNUSED_VARIABLE(fromColumnList);
                                                                                                                       UNUSED_VARIABLE(userData);

                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                       return ERROR_NONE;
                                                                                                                     },NULL),
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                        }
                                                                                        if (error == ERROR_NONE)
                                                                                        {
 //fprintf(stderr,"%s, %d: copy i\n",__FILE__,__LINE__);
                                                                                          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "imageEntries",
                                                                                                                     "imageEntries",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       UNUSED_VARIABLE(fromColumnList);
                                                                                                                       UNUSED_VARIABLE(userData);

                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                       return ERROR_NONE;
                                                                                                                     },NULL),
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
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
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       UNUSED_VARIABLE(fromColumnList);
                                                                                                                       UNUSED_VARIABLE(userData);

                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                       return ERROR_NONE;
                                                                                                                     },NULL),
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                        }
                                                                                        if (error == ERROR_NONE)
                                                                                        {
 //fprintf(stderr,"%s, %d: copy l\n",__FILE__,__LINE__);
                                                                                          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "linkEntries",
                                                                                                                     "linkEntries",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       UNUSED_VARIABLE(fromColumnList);
                                                                                                                       UNUSED_VARIABLE(userData);

                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                       return ERROR_NONE;
                                                                                                                     },NULL),
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                        }
                                                                                        if (error == ERROR_NONE)
                                                                                        {
 //fprintf(stderr,"%s, %d: copy h\n",__FILE__,__LINE__);
                                                                                          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "hardlinkEntries",
                                                                                                                     "hardlinkEntries",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       UNUSED_VARIABLE(fromColumnList);
                                                                                                                       UNUSED_VARIABLE(userData);

                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                                                       (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                                                       return ERROR_NONE;
                                                                                                                     },NULL),
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                        }
                                                                                        if (error == ERROR_NONE)
                                                                                        {
 //fprintf(stderr,"%s, %d: copy s\n",__FILE__,__LINE__);
                                                                                          error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "specialEntries",
                                                                                                                     "specialEntries",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     CALLBACK(NULL,NULL),  // pre-copy
                                                                                                                     CALLBACK(NULL,NULL),  // post-copy
                                                                                                                     CALLBACK(NULL,NULL),  // pause
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                        }

                                                                                        return error;
                                                                                      },NULL),
                                                                                      CALLBACK(NULL,NULL),  // pause
                                                                                      "WHERE storageId=%lld",
                                                                                      fromStorageId
                                                                                     );
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
                             "WHERE id!=0"
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

//fprintf(stderr,"%s, %d: copy en %llu %llu\n",__FILE__,__LINE__,fromStorageId,toStorageId);
                               return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                         &newIndexHandle->databaseHandle,
                                                         "entries",
                                                         "entries",
                                                         TRUE,  // transaction flag
                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                         {
                                                           UNUSED_VARIABLE(fromColumnList);
                                                           UNUSED_VARIABLE(userData);

                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                           return ERROR_NONE;
                                                         },NULL),
                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                         {
                                                           Errors  error;
                                                           IndexId fromEntryId;
                                                           IndexId toEntryId;

                                                           UNUSED_VARIABLE(userData);

                                                           fromEntryId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                                                           assert(fromEntryId != DATABASE_ID_NONE);
                                                           toEntryId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
                                                           assert(toEntryId != DATABASE_ID_NONE);

                                                           error = ERROR_NONE;

                                                           if (error == ERROR_NONE)
                                                           {
//fprintf(stderr,"%s, %d: copy f\n",__FILE__,__LINE__);
                                                             error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                        &newIndexHandle->databaseHandle,
                                                                                        "fileEntries",
                                                                                        "fileEntries",
                                                                                        FALSE,  // transaction flag
                                                                                        CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                        {
                                                                                          UNUSED_VARIABLE(fromColumnList);
                                                                                          UNUSED_VARIABLE(userData);

                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                          return ERROR_NONE;
                                                                                        },NULL),
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
                                                                                        fromEntryId
                                                                                       );
                                                           }
                                                           if (error == ERROR_NONE)
                                                           {
//fprintf(stderr,"%s, %d: copy i\n",__FILE__,__LINE__);
                                                             error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                        &newIndexHandle->databaseHandle,
                                                                                        "imageEntries",
                                                                                        "imageEntries",
                                                                                        FALSE,  // transaction flag
                                                                                        CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                        {
                                                                                          UNUSED_VARIABLE(fromColumnList);
                                                                                          UNUSED_VARIABLE(userData);

                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                          return ERROR_NONE;
                                                                                        },NULL),
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
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
                                                                                        CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                        {
                                                                                          UNUSED_VARIABLE(fromColumnList);
                                                                                          UNUSED_VARIABLE(userData);

                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                          return ERROR_NONE;
                                                                                        },NULL),
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
                                                                                        fromEntryId
                                                                                       );
                                                           }
                                                           if (error == ERROR_NONE)
                                                           {
//fprintf(stderr,"%s, %d: copy l\n",__FILE__,__LINE__);
                                                             error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                        &newIndexHandle->databaseHandle,
                                                                                        "linkEntries",
                                                                                        "linkEntries",
                                                                                        FALSE,  // transaction flag
                                                                                        CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                        {
                                                                                          UNUSED_VARIABLE(fromColumnList);
                                                                                          UNUSED_VARIABLE(userData);

                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                          return ERROR_NONE;
                                                                                        },NULL),
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
                                                                                        fromEntryId
                                                                                       );
                                                           }
                                                           if (error == ERROR_NONE)
                                                           {
//fprintf(stderr,"%s, %d: copy h\n",__FILE__,__LINE__);
                                                             error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                        &newIndexHandle->databaseHandle,
                                                                                        "hardlinkEntries",
                                                                                        "hardlinkEntries",
                                                                                        FALSE,  // transaction flag
                                                                                        CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                        {
                                                                                          UNUSED_VARIABLE(fromColumnList);
                                                                                          UNUSED_VARIABLE(userData);

                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                          (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

                                                                                          return ERROR_NONE;
                                                                                        },NULL),
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
                                                                                        fromEntryId
                                                                                       );
                                                           }
                                                           if (error == ERROR_NONE)
                                                           {
//fprintf(stderr,"%s, %d: copy s\n",__FILE__,__LINE__);
                                                             error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                        &newIndexHandle->databaseHandle,
                                                                                        "specialEntries",
                                                                                        "specialEntries",
                                                                                        FALSE,  // transaction flag
                                                                                        CALLBACK(NULL,NULL),  // pre-copy
                                                                                        CALLBACK(NULL,NULL),  // post-copy
                                                                                        CALLBACK(NULL,NULL),  // pause
                                                                                        "WHERE entryId=%lld",
                                                                                        fromEntryId
                                                                                       );
                                                           }

                                                           return error;
                                                         },NULL),
                                                         CALLBACK(NULL,NULL),  // pause
                                                         "WHERE storageId=%lld",
                                                         fromStorageId
                                                        );
                             },NULL),
                             CALLBACK(NULL,NULL),
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
