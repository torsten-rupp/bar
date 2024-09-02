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
* Name   : getImportStepsVersion3
* Purpose: get number of import steps for index version 3
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL ulong getImportStepsVersion3(IndexHandle *oldIndexHandle,
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
* Name   : importIndexVersion3
* Purpose: import index from version 3
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion3(IndexHandle *oldIndexHandle,
                                 IndexHandle *newIndexHandle,
                                 ProgressInfo   *progressInfo
                                )
{
  Errors              error;
  String              name1,name2;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  DatabaseId          storageId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  IndexId             entityId;
  bool                equalsFlag;
  ulong               i;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"hardlinks");
  fixBrokenIds(oldIndexHandle,"special");

  error = ERROR_NONE;

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storages",
                             TRUE,  // transaction flag
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
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",INDEX_DATABASE_ID(entityId));

                               return error;
                             },NULL),
                             // post: transfer files, images, directories, links, hardlinks, special entries
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

                                                              (void)Database_getTableColumnListId(toColumnList,"storageId",toStorageId);
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

                                                              (void)Database_getTableColumnListId(toColumnList,"storageId",toStorageId);
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
                                                            "hardlinks",
                                                            "entries",
                                                            TRUE,  // transaction flag
                                                            NULL,  // duration
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnInfo, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnInfo);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
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

  // try to set entityId in storage entries
  name1 = String_new();
  name2 = String_new();
  error = Database_prepare(&databaseQueryHandle1,
                           &oldIndexHandle->databaseHandle,
                           "SELECT uuid, \
                                   name, \
                                   UNIXTIMESTAMP(created) \
                            FROM storage \
                            GROUP BY uuid \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %S %"PRIu64"",
                               uuid,
                               name1,
                               &createdDateTime
                              )
       )
    {
      // insert entity
      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
                                   jobUUID, \
                                   scheduleUUID, \
                                   created, \
                                   type, \
                                   bidFlag \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S, \
                                   '', \
                                   DATETIME(%"PRIu64",'unixepoch'), \
                                   %d, \
                                   %d\
                                  ); \
                               ",
                               uuid,
                               createdDateTime,
                               ARCHIVE_TYPE_FULL,
                               0
                              );
      if (error == ERROR_NONE)
      {
        // get entity id
        entityId = INDEX_ID_ENTITY(Database_getLastRowId(&newIndexHandle->databaseHandle));

        // assign entity id for all storage entries with same uuid and matching name (equals except digits)
        error = Database_prepare(&databaseQueryHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 "SELECT id, \
                                         name \
                                  FROM storage \
                                  WHERE uuid=%'S \
                                 ",
                                 uuid
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseQueryHandle2,
                                     "%"PRIi64" %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
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
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET entityId=%"PRIi64" \
                                      WHERE id=%"PRIi64"; \
                                     ",
                                     entityId,
                                     storageId
                                    );
            }
          }
          Database_finalize(&databaseQueryHandle2);
        }
      }
    }
    Database_finalize(&databaseQueryHandle1);
  }
  String_delete(name2);
  String_delete(name1);
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
