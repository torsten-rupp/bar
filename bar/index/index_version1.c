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
* Name   : getImportStepsVersion1
* Purpose: get number of import steps for index version 1
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL ulong getImportStepsVersion1(IndexHandle *oldIndexHandle,
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
* Name   : importIndexVersion1
* Purpose: imort index from version 1
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion1(IndexHandle *oldIndexHandle,
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

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storages",
                             FALSE,  // transaction flag
                             NULL,  // duration
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);

                               UNUSED_VARIABLE(fromColumnInfo);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       NULL,  // hostName
                                                       NULL,  // userName
                                                       ARCHIVE_TYPE_FULL,
                                                       0LL,  // createdDateTime
                                                       TRUE,  // locked
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",Index_getDatabaseId(entityId));

                               return error;
                             },NULL),
                             // post: transfer files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);

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

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64"",
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

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
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
                                                                                      Database_getTableColumnListInt64(fromColumnInfo,"size",0LL),
                                                                                      Database_getTableColumnListInt64(fromColumnInfo,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListInt64(fromColumnInfo,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64"",
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

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
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
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnInfo,"size",0LL),
                                                                                      (int)Database_getTableColumnListInt64(fromColumnInfo,"fileSystemType",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnInfo,"blockSize",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnInfo,"blockOffset",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnInfo,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64"",
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

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64"",
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
                                                                                      (int)Database_getTableColumnListInt64(fromColumnInfo,"specialType",0LL),
                                                                                      (uint)Database_getTableColumnListInt64(fromColumnInfo,"major",0LL),
                                                                                      (uint)Database_getTableColumnListInt64(fromColumnInfo,"minor",0LL)
                                                                                     );
                                                            },NULL),
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%"PRIi64"",
                                                            fromStorageId
                                                           );
                               }

                               (void)Index_unlockEntity(newIndexHandle,entityId);

                               return error;
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

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
