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
* Name   : upgradeFromVersion2
* Purpose: upgrade index from version 2 to current version
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion2(IndexHandle *oldIndexHandle, IndexHandle *newIndexHandle)
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
  fixBrokenIds(oldIndexHandle,"hardlinks");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storages",
                             FALSE,  // transaction flag
                             NULL,  // duration
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);

                               UNUSED_VARIABLE(fromColumnList);
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
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // post: transfer files, images, directories, links, hardlinks, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               IndexId fromStorageId;
                               IndexId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
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
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_(NULL,NULL),  // post-copy
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
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
                                                            NULL,  // duration
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
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
                                                            NULL,  // duration
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
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
                                                            NULL,  // duration
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
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
                                                            NULL,  // duration
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
                                                            CALLBACK_(getCopyPauseCallback(),NULL),
                                                            CALLBACK_(NULL,NULL),  // progress
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               (void)Index_unlockEntity(newIndexHandle,entityId);

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(NULL,NULL),  // progress
                             NULL  // filter
                            );

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
