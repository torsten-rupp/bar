/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index database functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "global.h"
#include "threads.h"
#include "strings.h"
#include "database.h"
#include "files.h"
#include "filesystems.h"
#include "misc.h"
#include "errors.h"

#include "storage.h"
#include "index_definition.h"
#include "bar.h"

#include "index.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[] =
{
  { "none",             INDEX_STATE_NONE             },
  { "ok",               INDEX_STATE_OK               },
  { "create",           INDEX_STATE_CREATE           },
  { "update_requested", INDEX_STATE_UPDATE_REQUESTED },
  { "update",           INDEX_STATE_UPDATE           },
  { "error",            INDEX_STATE_ERROR            }
};

LOCAL const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[] =
{
  { "MANUAL", INDEX_MODE_MANUAL },
  { "AUTO",   INDEX_MODE_AUTO   },
  { "*",      INDEX_MODE_ALL    }
};

// entry table names
const char *ENTRY_TABLE_NAMES[] =
{
  "files",
  "images",
  "directories",
  "links",
  "hardlinks",
  "special"
};

// sleep time [s]
#define SLEEP_TIME_INDEX_CLEANUP_THREAD (4*60*60)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL Thread cleanupIndexThread;    // clean-up thread

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define openIndex(...)  __openIndex(__FILE__,__LINE__,__VA_ARGS__)
  #define createIndex(...) __createIndex(__FILE__,__LINE__,__VA_ARGS__)
  #define closeIndex(...) __closeIndex(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : openIndex
* Purpose: open index database
* Input  : databaseFileName - database file name
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle *indexHandle,
                         const char  *databaseFileName
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char  *__fileName__,
                           uint        __lineNb__,
                           IndexHandle *indexHandle,
                           const char  *databaseFileName
                          )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // open index database
  #ifdef NDEBUG
    error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createIndex
* Purpose: create empty index database
* Input  : databaseFileName - database file name
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createIndex(IndexHandle *indexHandle,
                           const char  *databaseFileName
                          )
#else /* not NDEBUG */
  LOCAL Errors __createIndex(const char  *__fileName__,
                             uint        __lineNb__,
                             IndexHandle *indexHandle,
                             const char  *databaseFileName
                            )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // create index database
  (void)File_deleteCString(databaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  (void)Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);

  // create tables
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           INDEX_TABLE_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      Database_close(&indexHandle->databaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
    #endif /* NDEBUG */
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeIndex
* Purpose: close index database
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors closeIndex(IndexHandle *indexHandle)
#else /* not NDEBUG */
  LOCAL Errors __closeIndex(const char  *__fileName__,
                            uint        __lineNb__,
                            IndexHandle *indexHandle
                           )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  #ifdef NDEBUG
    Database_close(&indexHandle->databaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getIndexVersion
* Purpose: get index version
* Input  : databaseFileName - database file name
* Output : indexVersion - index version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getIndexVersion(const char *databaseFileName, int64 *indexVersion)
{
  Errors         error;
  IndexHandle indexHandle;

  // open index database
  error = openIndex(&indexHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getInteger64(&indexHandle.databaseHandle,
                                indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&indexHandle);
    return error;
  }

  // close index database
  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : fixBrokenIds
* Purpose: fix broken ids
* Input  : indexHandle - index handle
*          tableName   - table name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fixBrokenIds(IndexHandle *indexHandle, const char *tableName)
{
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE %s SET id=rowId WHERE id IS NULL;",
                         tableName
                        );
}

/***********************************************************************\
* Name   : upgradeFromVersion1
* Purpose: upgrade index from version 1 to current version
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion1(IndexHandle *oldIndexHandle, IndexHandle *newIndexHandle)
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

  // transfer index data to new index
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               DatabaseId   entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       ARCHIVE_TYPE_FULL,
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL  // filter
                            );

  return ERROR_NONE;
}

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
  Errors error;

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"hardlinks");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer index data to new index
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               DatabaseId   entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       ARCHIVE_TYPE_FULL,
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // copy files, images, directories, links, hardlinks, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "hardlinks",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL  // filter
                            );

  return error;
}

/***********************************************************************\
* Name   : upgradeFromVersion3
* Purpose: upgrade index from version 3 to current version
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion3(IndexHandle *oldIndexHandle, IndexHandle *newIndexHandle)
{
  Errors              error;
  String              name1,name2;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  DatabaseId          storageId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityId;
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

  // transfer index data to new index
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               DatabaseId   entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       ARCHIVE_TYPE_FULL,
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // copy files, images, directories, links, hardlinks, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "hardlinks",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL  // filter
                            );

  // try to set entityId in storage entries
  name1 = String_new();
  name2 = String_new();
  error = Database_prepare(&databaseQueryHandle1,
                           &oldIndexHandle->databaseHandle,
                           "SELECT uuid, \
                                   name, \
                                   STRFTIME('%%s',created) \
                            FROM storage \
                            GROUP BY uuid \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %S %lld",
                               uuid,
                               name1,
                               &createdDateTime
                              )
       )
    {
      // insert entity
      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
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
                                   DATETIME(%llu,'unixepoch'), \
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
        entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

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
                                     "%lld %S",
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
                                     CALLBACK(NULL,NULL),
                                     "UPDATE storage \
                                      SET entityId=%lld \
                                      WHERE id=%lld; \
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

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : upgradeFromVersion4
* Purpose: upgrade index from version 4 to current version
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion4(IndexHandle *oldIndexHandle, IndexHandle *newIndexHandle)
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

  // transfer index data to new index
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               DatabaseId   entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       ARCHIVE_TYPE_FULL,
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "hardlinks",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL  // filter
                            );

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : upgradeFromVersion5
* Purpose: upgrade index from version 5 to current version
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion5(IndexHandle *oldIndexHandle, IndexHandle *newIndexHandle)
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

  // transfer index data to new index
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               DatabaseId   entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               error = Index_newEntity(newIndexHandle,
                                                       Misc_getUUID(jobUUID),
                                                       NULL,  // scheduleUUID
                                                       ARCHIVE_TYPE_FULL,
                                                       &entityId
                                                      );
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "files",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "images",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "directories",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "links",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "hardlinks",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               if (error == ERROR_NONE)
                               {
                                 error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                            &newIndexHandle->databaseHandle,
                                                            "special",
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK(NULL,NULL),
                                                            "WHERE storageId=%lld",
                                                            fromStorageId
                                                           );
                               }

                               return ERROR_NONE;
                             },NULL),
                             NULL  // filter
                            );

  // create full-text-search indizes
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_files SELECT id,name FROM files;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_images SELECT id,name FROM images;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_directories SELECT id,name FROM directories;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_links SELECT id,name FROM links;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_hardlinks SELECT id,name FROM hardlinks;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&newIndexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO FTS_special SELECT id,name FROM special;"
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : importIndex
* Purpose: upgrade and import index
* Input  : indexHandle         - index handle
*          oldDatabaseFileName - old database file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndex(IndexHandle *indexHandle, ConstString oldDatabaseFileName)
{
  Errors      error;
  IndexHandle oldIndexHandle;
  int64       indexVersion;

  // open old index
  error = openIndex(&oldIndexHandle,String_cString(oldDatabaseFileName));
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get index version
  error = Database_getInteger64(&oldIndexHandle.databaseHandle,
                                &indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&oldIndexHandle);
    return error;
  }

  if (indexVersion < INDEX_VERSION)
  {
    // upgrade index structure
    switch (indexVersion)
    {
      case 1:
        error = upgradeFromVersion1(&oldIndexHandle,indexHandle);
        break;
      case 2:
        error = upgradeFromVersion2(&oldIndexHandle,indexHandle);
        break;
      case 3:
        error = upgradeFromVersion3(&oldIndexHandle,indexHandle);
        break;
      case 4:
        error = upgradeFromVersion4(&oldIndexHandle,indexHandle);
        break;
      case 5:
        error = upgradeFromVersion5(&oldIndexHandle,indexHandle);
        break;
      case 6:
        // nothing to do
        break;
      default:
        // unknown version if index
        error = ERROR_DATABASE_VERSION_UNKNOWN;
        break;
    }
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Upgraded from version %d to %d\n",
                  indexVersion,
                  INDEX_VERSION
                 );
    }
    else
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Upgrade version from %d to %d fail: %s\n",
                  indexVersion,
                  INDEX_VERSION,
                  Error_getText(error)
                 );
    }
  }

  // close old index
  (void)closeIndex(&oldIndexHandle);

  return error;
}

/***********************************************************************\
* Name   : cleanUpDuplicateMeta
* Purpose: delete duplicate meta data
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateMeta(IndexHandle *indexHandle)
{
  String              name;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  name = String_new();

  if (Database_prepare(&databaseQueryHandle,
                       &indexHandle->databaseHandle,
                       "SELECT name FROM meta GROUP BY name"
                      ) == ERROR_NONE
     )
  {
    while (Database_getNextRow(&databaseQueryHandle,
                               "%S",
                               name
                              )
          )
    {
      (void)Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM meta \
                              WHERE     name=%'S \
                                    AND rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1); \
                             ",
                             name,
                             name
                            );
    }
    Database_finalize(&databaseQueryHandle);
  }

  // free resources
  String_delete(name);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteUpdate
* Purpose: reset incomplete updated database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteUpdate(IndexHandle *indexHandle)
{
  Errors           error;
  DatabaseId       storageId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  error = ERROR_NONE;
  while (Index_findByState(indexHandle,
                           INDEX_STATE_SET(INDEX_STATE_UPDATE),
                           NULL, // jobUUID
                           NULL, // scheduleUUID
                           &storageId,
                           NULL, // storageName
                           NULL  // lastCheckedTimestamp
                          )
         && (error == ERROR_NONE)
        )
  {
    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    error = Index_setState(indexHandle,
                           storageId,
                           INDEX_STATE_UPDATE_REQUESTED,
                           0LL,
                           NULL
                          );
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Requested update index #%lld: %s\n",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteCreate
* Purpose: delete incomplete created database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteCreate(IndexHandle *indexHandle)
{
  Errors           error;
  DatabaseId       storageId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  error = ERROR_NONE;
  while (Index_findByState(indexHandle,
                           INDEX_STATE_SET(INDEX_STATE_CREATE),
                           NULL, // jobUUID
                           NULL, // scheduleUUID
                           &storageId,
                           storageName,
                           NULL  // lastCheckedTimestamp
                          )
         && (error == ERROR_NONE)
        )
  {
    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    error = Index_deleteStorage(indexHandle,storageId);
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Deleted incomplete index #%lld: %s\n",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpOrphanedEntries
* Purpose: delete orphaned entries (entries without storage)
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpOrphanedEntries(IndexHandle *indexHandle)
{
  Errors           error;
  String           storageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       databaseId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // initialize variables
  storageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListFiles(&indexQueryHandle,
                              indexHandle,
                              NULL,  // storageIds
                              0,  // storageIdCount
                              NULL,  // entryIds
                              0,  // entryIdCount
                              NULL  // pattern
                             );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextFile(&indexQueryHandle,
                                &databaseId,
                                storageName,
                                NULL,  // storageDateTime,
                                NULL,  // name,
                                NULL,  // size,
                                NULL,  // timeModified,
                                NULL,  // userId,
                                NULL,  // groupId,
                                NULL,  // permission,
                                NULL,  // fragmentOffset,
                                NULL   // fragmentSize
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteFile(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListImages(&indexQueryHandle,
                               indexHandle,
                               NULL,  // storageIds
                               0,  // storageIdCount
                               NULL,  // entryIds
                               0,  // entryIdCount
                               NULL  // pattern
                              );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextImage(&indexQueryHandle,
                                 &databaseId,
                                 storageName,
                                 NULL,  // storageDateTime,
                                 NULL,  // imageName,
                                 NULL,  // fileSystemType,
                                 NULL,  // size,
                                 NULL,  // blockOffset,
                                 NULL   // blockCount
                                )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteImage(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListDirectories(&indexQueryHandle,
                                    indexHandle,
                                    NULL,  // storageIds
                                    0,  // storageIdCount
                                    NULL,  // entryIds
                                    0,  // entryIdCount
                                    NULL  // pattern
                                   );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextDirectory(&indexQueryHandle,
                                     &databaseId,
                                     storageName,
                                     NULL,  // storageDateTime,
                                     NULL,  // directoryName,
                                     NULL,  // timeModified,
                                     NULL,  // userId,
                                     NULL,  // groupId,
                                     NULL   // permission
                                    )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteDirectory(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListLinks(&indexQueryHandle,
                                    indexHandle,
                                    NULL,  // storageIds
                                    0,  // storageIdCount
                                    NULL,  // entryIds
                                    0,  // entryIdCount
                                    NULL  // pattern
                                   );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextLink(&indexQueryHandle,
                                &databaseId,
                                storageName,
                                NULL,  // storageDateTime,
                                NULL,  // name,
                                NULL,  // destinationName
                                NULL,  // timeModified,
                                NULL,  // userId,
                                NULL,  // groupId,
                                NULL   // permission
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteLink(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListHardLinks(&indexQueryHandle,
                                  indexHandle,
                                  NULL,  // storageIds
                                  0,  // storageIdCount
                                  NULL,  // entryIds
                                  0,  // entryIdCount
                                  NULL  // pattern
                                 );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextHardLink(&indexQueryHandle,
                                    &databaseId,
                                    storageName,
                                    NULL,  // storageDateTime,
                                    NULL,  // fileName,
                                    NULL,  // size,
                                    NULL,  // timeModified,
                                    NULL,  // userId,
                                    NULL,  // groupId,
                                    NULL,  // permission,
                                    NULL,  // fragmentOffset
                                    NULL   // fragmentSize
                                   )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteHardLink(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListSpecial(&indexQueryHandle,
                                indexHandle,
                                NULL,  // storageIds
                                0,  // storageIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                NULL  // pattern
                               );
  if (error == ERROR_NONE)
  {
    while (   !indexHandle->quitFlag
           && Index_getNextSpecial(&indexQueryHandle,
                                   &databaseId,
                                   storageName,
                                   NULL,  // storageDateTime,
                                   NULL,  // name,
                                   NULL,  // timeModified,
                                   NULL,  // userId,
                                   NULL,  // groupId,
                                   NULL   // permission
                                  )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteSpecial(indexHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  if (n > 0L) plogMessage(NULL,  // logHandle
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Cleaned %lu orphaned entries\n",
                          n
                         );

  // free resources
  String_delete(storageName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStoragenNoName
* Purpose: delete storage entries without name
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoName(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  ulong            n;
  DatabaseId       storageId;
  IndexQueryHandle indexQueryHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListStorage(&indexQueryHandle,
                                indexHandle,
                                NULL, // uuid
                                DATABASE_ID_ANY, // entity id
                                STORAGE_TYPE_ANY,
                                NULL, // storageName
                                NULL, // hostName
                                NULL, // loginName
                                NULL, // deviceName
                                NULL, // fileName
                                INDEX_STATE_SET_ALL,
                                NULL, // storageIds
                                0  // storageIdCount
                               );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                &storageId,
                                NULL, // entity id
                                NULL, // job UUID
                                NULL, // schedule UUID
                                NULL, // archive type
                                storageName,
                                NULL, // createdDateTime
                                NULL, // entries
                                NULL, // size
                                NULL, // indexState
                                NULL, // indexMode
                                NULL, // lastCheckedDateTime
                                NULL  // errorMessage
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteStorage(indexHandle,storageId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  if (n > 0L) plogMessage(NULL,  // logHandle
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Cleaned %lu indizes without name\n",
                          n
                         );

  // free resource
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStorageNoEntity
* Purpose: assign entity to storage entries without any entity
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoEntity(IndexHandle *indexHandle)
{
#if 0
  Errors              error;
  String              name1,name2;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  DatabaseId          storageId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityId;
  bool                equalsFlag;
  ulong               i;
  String              oldDatabaseFileName;

  // try to set entityId in storage entries
  name1 = String_new();
  name2 = String_new();
  error = Database_prepare(&databaseQueryHandle1,
                           &indexHandle->databaseHandle,
                           "SELECT uuid, \
                                   name, \
                                   STRFTIME('%%s',created) \
                            FROM storage \
                            WHERE entityId=0 \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %lld",
                               uuid,
                               name1,
                               &createdDateTime
                              )
       )
    {
      // find matching entity/create default entity
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
                                   "%lld %S",
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
                                   CALLBACK(NULL,NULL),
                                   "UPDATE storage \
                                    SET entityId=%lld \
                                    WHERE id=%lld; \
                                   ",
                                   entityId,
                                   storageId
                                  );
          }
        }
        Database_finalize(&databaseQueryHandle2);
      }

      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
                               "INSERT INTO entities \
                                  ( \
                                   jobUUID, \
                                   scheduleUUID, \
                                   created, \
                                   type, \
                                   bidFlag\
                                  ) \
                                VALUES \
                                  ( \
                                   %'S, \
                                   '', \
                                   DATETIME(%llu,'unixepoch'), \
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
        entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

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
                                     "%lld %S",
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
                                     CALLBACK(NULL,NULL),
                                     "UPDATE storage \
                                      SET entityId=%lld \
                                      WHERE id=%lld; \
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
#else
UNUSED_VARIABLE(indexHandle);
return ERROR_NONE;
#endif
}

/***********************************************************************\
* Name   : cleanUpDuplicateIndizes
* Purpose: delete duplicate storage entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateIndizes(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           duplicateStorageName;
  String           printableStorageName;
  ulong            n;
  DatabaseId       storageId;
  bool             deletedIndexFlag;
  IndexQueryHandle indexQueryHandle1,indexQueryHandle2;
  int64            duplicateStorageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  duplicateStorageName = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  do
  {
    deletedIndexFlag = FALSE;

    // get storage entry
    error = Index_initListStorage(&indexQueryHandle1,
                                  indexHandle,
                                  NULL, // uuid
                                  DATABASE_ID_ANY, // entity id
                                  STORAGE_TYPE_ANY,
                                  NULL, // storageName
                                  NULL, // hostName
                                  NULL, // loginName
                                  NULL, // deviceName
                                  NULL, // fileName
                                  INDEX_STATE_SET_ALL,
                                  NULL, // storageIds
                                  0  // storageIdCount
                                 );
    if (error != ERROR_NONE)
    {
      break;
    }
    while (   !indexHandle->quitFlag
           && !deletedIndexFlag
           && Index_getNextStorage(&indexQueryHandle1,
                                   &storageId,
                                   NULL, // entity id
                                   NULL, // job UUID
                                   NULL, // schedule UUID
                                   NULL, // archive type
                                   storageName,
                                   NULL, // createdDateTime
                                   NULL, // entries
                                   NULL, // size
                                   NULL, // indexState
                                   NULL, // indexMode
                                   NULL, // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      // check for duplicate entry
      error = Index_initListStorage(&indexQueryHandle2,
                                    indexHandle,
                                    NULL, // uuid
                                    DATABASE_ID_ANY, // entity id
                                    STORAGE_TYPE_ANY,
                                    NULL, // storageName
                                    NULL, // hostName
                                    NULL, // loginName
                                    NULL, // deviceName
                                    NULL, // fileName
                                    INDEX_STATE_SET_ALL,
                                    NULL, // storageIds
                                    0  // storageIdCount
                                   );
      if (error != ERROR_NONE)
      {
        continue;
      }
      while (   !indexHandle->quitFlag
             && !deletedIndexFlag
             && Index_getNextStorage(&indexQueryHandle2,
                                     &duplicateStorageId,
                                     NULL, // entity id
                                     NULL, // job UUID
                                     NULL, // schedule UUID
                                     NULL, // archive type
                                     duplicateStorageName,
                                     NULL, // createdDateTime
                                     NULL, // entries
                                     NULL, // size
                                     NULL, // indexState
                                     NULL, // indexMode
                                     NULL, // lastCheckedDateTime
                                     NULL  // errorMessage
                                    )
            )
      {
        if (   (storageId != duplicateStorageId)
            && Storage_equalNames(storageName,duplicateStorageName)
           )
        {
          // get printable name (if possible)
          error = Storage_parseName(&storageSpecifier,duplicateStorageName);
          if (error == ERROR_NONE)
          {
            String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
          }
          else
          {
            String_set(printableStorageName,duplicateStorageName);
          }

          // delete storage
          error = Index_deleteStorage(indexHandle,duplicateStorageId);
          if (error == ERROR_NONE)
          {
            plogMessage(NULL,  // logHandle
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Deleted duplicate index #%lld: '%s'\n",
                        duplicateStorageId,
                        String_cString(printableStorageName)
                       );
            n++;
            deletedIndexFlag = TRUE;
          }
        }
      }
      Index_doneList(&indexQueryHandle2);

      // request update index
      if (deletedIndexFlag)
      {
        (void)Index_setState(indexHandle,
                             storageId,
                             INDEX_STATE_UPDATE_REQUESTED,
                             0LL,  // lastCheckedTimestamp
                             NULL  // errorMessage
                            );
      }
    }
    Index_doneList(&indexQueryHandle1);
  }
  while (!indexHandle->quitFlag && deletedIndexFlag);
  if (n > 0L)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Cleaned %lu duplicate indizes\n",
                n
               );
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(duplicateStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanupIndexThreadCode
* Purpose: cleanup index thread
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanupIndexThreadCode(IndexHandle *indexHandle)
{
  String              absoluteFileName;
  String              pathName;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              prefixFileName;
  String              oldDatabaseFileName;
  uint                sleepTime;

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Start upgrade index database\n"
             );

  // get absolute file name
  absoluteFileName = File_getAbsoluteFileNameCString(String_new(),indexHandle->databaseFileName);

  // open directory
  pathName = File_getFilePathName(String_new(),absoluteFileName);
  error = File_openDirectoryList(&directoryListHandle,pathName);
  if (error != ERROR_NONE)
  {
    String_delete(pathName);
    String_delete(absoluteFileName);
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Upgrade index database '%s' fail: %s\n",
                indexHandle->databaseFileName,
                Error_getText(error)
               );
    return;
  }
  String_delete(pathName);

  // process all *.oldNNNN files
  prefixFileName      = String_appendCString(String_duplicate(absoluteFileName),".old");
  oldDatabaseFileName = String_new();
  while (File_readDirectoryList(&directoryListHandle,oldDatabaseFileName) == ERROR_NONE)
  {
    if (String_startsWith(oldDatabaseFileName,prefixFileName))
    {
      error = importIndex(indexHandle,oldDatabaseFileName);
      if (error == ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Imported index database '%s'\n",
                    String_cString(oldDatabaseFileName)
                   );
        (void)File_delete(oldDatabaseFileName,FALSE);
      }
      else
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Import index database '%s' fail: %s\n",
                    String_cString(oldDatabaseFileName),
                    Error_getText(error)
                   );
      }
      if (indexHandle->upgradeError == ERROR_NONE) indexHandle->upgradeError = error;
    }
  }
  String_delete(oldDatabaseFileName);
  String_delete(prefixFileName);

  // close directory
  File_closeDirectoryList(&directoryListHandle);

  // free resources
  String_delete(absoluteFileName);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Done upgrade index database\n"
             );

  // single clean-ups
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up index database\n"
             );
  (void)cleanUpDuplicateMeta(indexHandle);
  (void)cleanUpIncompleteUpdate(indexHandle);
  (void)cleanUpIncompleteCreate(indexHandle);
  (void)cleanUpStorageNoName(indexHandle);
  (void)cleanUpStorageNoEntity(indexHandle);
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Done clean-up index database\n"
             );

  // regular clean-ups
  while (!indexHandle->quitFlag)
  {
    // clean-up database
    (void)cleanUpOrphanedEntries(indexHandle);
    (void)cleanUpDuplicateIndizes(indexHandle);

    // sleep, check quit flag
    sleepTime = 0;
    while ((sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD) && !indexHandle->quitFlag)
    {
      Misc_udelay(10LL*MISC_US_PER_SECOND);
      sleepTime += 10;
    }
  }
}

/***********************************************************************\
* Name   : initIndexQueryHandle
* Purpose: init index query handle
* Input  : indexQueryHandle - index query handle
*          indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  indexQueryHandle->indexHandle                = indexHandle;
  indexQueryHandle->storage.type               = STORAGE_TYPE_NONE;
  indexQueryHandle->storage.storageNamePattern = NULL;
  indexQueryHandle->storage.hostNamePattern    = NULL;
  indexQueryHandle->storage.loginNamePattern   = NULL;
  indexQueryHandle->storage.deviceNamePattern  = NULL;
  indexQueryHandle->storage.fileNamePattern    = NULL;
}

/***********************************************************************\
* Name   : doneIndexQueryHandle
* Purpose: done index query handle
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneIndexQueryHandle(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  if (indexQueryHandle->storage.fileNamePattern    != NULL) Pattern_delete(indexQueryHandle->storage.fileNamePattern);
  if (indexQueryHandle->storage.deviceNamePattern  != NULL) Pattern_delete(indexQueryHandle->storage.deviceNamePattern);
  if (indexQueryHandle->storage.loginNamePattern   != NULL) Pattern_delete(indexQueryHandle->storage.loginNamePattern);
  if (indexQueryHandle->storage.hostNamePattern    != NULL) Pattern_delete(indexQueryHandle->storage.hostNamePattern);
  if (indexQueryHandle->storage.storageNamePattern != NULL) Pattern_delete(indexQueryHandle->storage.storageNamePattern);
}

/***********************************************************************\
* Name   : getIndexStateSetString
* Purpose: get index state filter string
* Input  : string        - string variable
*          indexStateSet - index state set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexStateSetString(String string, IndexStateSet indexStateSet)
{
  uint i;

  String_clear(string);
  for (i = INDEX_STATE_OK; i < INDEX_STATE_UNKNOWN; i++)
  {
    if ((indexStateSet & (1 << i)) != 0)
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",i);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getREGEXPString
* Purpose: get REGEXP filter string
* Input  : string      - string variable
*          columnName  - column name
*          patternText - pattern text
* Output : -
* Return : string for WHERE statement
* Notes  : -
\***********************************************************************/

LOCAL String getREGEXPString(String string, const char *columnName, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  ulong           i;
  char            ch;

  String_setCString(string,"1");
  if (!String_isEmpty(patternText))
  {
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_appendCString(string," AND REGEXP('");
      i = 0;
      while (i < String_length(token))
      {
        ch = String_index(token,i);
        switch (ch)
        {
          case '.':
          case '[':
          case ']':
          case '(':
          case ')':
          case '{':
          case '}':
          case '+':
          case '|':
          case '^':
          case '$':
          case '\\':
            String_appendChar(string,'\\');
            String_appendChar(string,ch);
            i++;
            break;
          case '*':
            String_appendCString(string,".*");
            i++;
            break;
          case '?':
            String_appendChar(string,'.');
            i++;
            break;
          case '\'':
            String_appendCString(string,"''");
            i++;
            break;
          default:
            String_appendChar(string,ch);
            i++;
            break;
        }
      }
      String_format(string,"',0,%s)",columnName);
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
}

/***********************************************************************\
* Name   : getFTSString
* Purpose: get full-text-search filter string
* Input  : string      - string variable
*          columnName  - column name
*          patternText - pattern text
* Output : -
* Return : string for WHERE statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSString(String string, const char *tableName, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedPatternFlag;
  ulong           i;
  char            ch;

  String_setCString(string,"1");
  if (!String_isEmpty(patternText))
  {
    String_format(string," AND (%s MATCH '",tableName);
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      addedPatternFlag = FALSE;
      i                = 0;
      while (i < String_length(token))
      {
        ch = String_index(token,i);
        if (isalnum(ch))
        {
          if (addedPatternFlag)
          {
            String_appendChar(string,' ');
            addedPatternFlag = FALSE;
          }
          String_appendChar(string,ch);
        }
        else
        {
          if (!addedPatternFlag)
          {
            String_appendChar(string,'*');
            addedPatternFlag = TRUE;
          }
        }
        i++;
      }
      if (!String_isEmpty(string) && !addedPatternFlag) String_appendChar(string,'*');
    }
    String_doneTokenizer(&stringTokenizer);
    String_appendCString(string,"')");
  }

  return string;
}

/***********************************************************************\
* Name   : getOrderingString
* Purpose: get SQL ordering string
* Input  : ordering - database ordering
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

LOCAL const char *getOrderingString(DatabaseOrdering ordering)
{
  const char *s = NULL;

  switch (ordering)
  {
    case DATABASE_ORDERING_ASCENDING:  s = "ASC";  break;
    case DATABASE_ORDERING_DESCENDING: s = "DESC"; break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return s;
}

/***********************************************************************\
* Name   : assignStorageToStorage
* Purpose: assign storage entries to other storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageToStorage(IndexHandle *indexHandle,
                                    DatabaseId  storageId,
                                    DatabaseId  toStorageId
                                   )
{
  Errors           error;
  uint             i;

  // assign storage entries to other storage
  for (i = 0; i < SIZE_OF_ARRAY(ENTRY_TABLE_NAMES); i++)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE %s \
                              SET storageId=%lld \
                              WHERE storageId=%lld; \
                             ",
                             ENTRY_TABLE_NAMES[i],
                             toStorageId,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // delete storage if empty
  error = Index_pruneStorage(indexHandle,storageId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignEntityToStorage
* Purpose: assign all storage entries of entity to other storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToStorage(IndexHandle *indexHandle,
                                   DatabaseId  entityId,
                                   DatabaseId  toStorageId
                                  )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       storageId;

  error = Index_initListStorage(&indexQueryHandle,
                                indexHandle,
                                NULL, // uuid
                                entityId,
                                STORAGE_TYPE_ANY,
                                NULL, // storageName
                                NULL, // hostName
                                NULL, // loginName
                                NULL, // deviceName
                                NULL, // fileName
                                INDEX_STATE_SET_ALL,
                                NULL, // storageIds
                                0  // storageIdCount
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              &storageId,
                              NULL, // entity id
                              NULL, // job UUID
                              NULL, // schedule UUID
                              NULL, // archive type
                              NULL, // storageName
                              NULL, // createdDateTime
                              NULL, // entries
                              NULL, // size
                              NULL, // indexState
                              NULL, // indexMode
                              NULL, // lastCheckedDateTime
                              NULL  // errorMessage
                             )
        )
  {
    // assign storage entries to other storage
    error = assignStorageToStorage(indexHandle,storageId,toStorageId);
    if (error != ERROR_NONE)
    {
      Index_doneList(&indexQueryHandle);
      return error;
    }

    // delete storage if empty
    error = Index_pruneStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  // delete entity if empty
  error = Index_pruneEntity(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignJobToStorage
* Purpose: assign all storage entries of all entities of job to other
*          storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToStorage(IndexHandle *indexHandle,
                                ConstString jobUUID,
                                DatabaseId  toStorageId
                               )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entityId;

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL   // offset
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             &entityId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // createdDateTime,
                             NULL,  // archiveType,
                             NULL,  // totalEntries,
                             NULL,  // totalSize,
                             NULL   // lastErrorMessage
                            )
        )
  {
    // assign all storage entries of entity to other storage
    error = assignEntityToStorage(indexHandle,entityId,toStorageId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignStorageToStorage
* Purpose: assign storage entries to other entity
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toEntityId  - to entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageToEntity(IndexHandle *indexHandle,
                                   DatabaseId  storageId,
                                   DatabaseId  toEntityId
                                  )
{
  Errors error;

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET entityId=%lld \
                            WHERE id=%lld; \
                           ",
                           toEntityId,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignEntityToEntity
* Purpose: assign all storage entries of entity to other entity
* Input  : indexHandle - index handle
*          entityId    - entity id
*          toEntityId  - to entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle *indexHandle,
                                  DatabaseId  entityId,
                                  DatabaseId  toEntityId
                                 )
{
  Errors error;

  // assign to entity
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET entityId=%lld \
                            WHERE entityId=%lld; \
                           ",
                           toEntityId,
                           entityId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // delete entity if empty
  error = Index_pruneEntity(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignJobToEntity
* Purpose: assign all entities of job to other entity
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          toEntityId  - to entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToEntity(IndexHandle *indexHandle,
                               ConstString jobUUID,
                               DatabaseId  toEntityId
                              )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entityId;

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL   // offset
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             &entityId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // createdDateTime,
                             NULL,  // archiveType,
                             NULL,  // totalEntries,
                             NULL,  // totalSize,
                             NULL   // lastErrorMessage
                            )
        )
  {
    // assign all storage entries of entity to other entity
    error = assignEntityToEntity(indexHandle,entityId,toEntityId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}

#if 0
// still not used
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

LOCAL Errors assignJobToJob(IndexHandle *indexHandle,
                            ConstString jobUUID,
                            ConstString toJobUUID
                           )
{
  Errors error;

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE entity \
                            SET jobUUID=%'S \
                            WHERE jobUUID=%'S; \
                           ",
                           toJobUUID,
                           jobUUID
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  return ERROR_NONE;
}

void Index_doneAll(void)
{
}

const char *Index_stateToString(IndexStates indexState, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[z].indexState != indexState)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState)
{
  uint z;

  assert(name != NULL);
  assert(indexState != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[z].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[z].indexMode != indexMode)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode)
{
  uint z;

  assert(name != NULL);
  assert(indexMode != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[z].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Index_init(IndexHandle *indexHandle,
                  const char  *databaseFileName
                 )
{
  Errors error;
  int64  indexVersion;
  String oldDatabaseFileName;
  uint   n;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // init variables
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->upgradeError     = ERROR_NONE;
  indexHandle->quitFlag         = FALSE;

  // check if index exists
  if (File_existsCString(indexHandle->databaseFileName))
  {
    // get index version
    error = getIndexVersion(indexHandle->databaseFileName,&indexVersion);
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (indexVersion < INDEX_VERSION)
    {
      // rename existing index for upgrade
      oldDatabaseFileName = String_new();
      n = 0;
      do
      {
        oldDatabaseFileName = String_newCString(indexHandle->databaseFileName);
        String_appendCString(oldDatabaseFileName,".old");
        String_format(oldDatabaseFileName,"%03d",n);
        n++;
      }
      while (File_exists(oldDatabaseFileName));
      (void)File_renameCString(indexHandle->databaseFileName,
                               String_cString(oldDatabaseFileName),
                               NULL
                              );
      String_delete(oldDatabaseFileName);

      // create new index
      error = createIndex(indexHandle,indexHandle->databaseFileName);
      if (error != ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Create new index database '$s' fail: %s\n",
                    indexHandle->databaseFileName,
                    Error_getText(error)
                   );
        return error;
      }
    }
    else
    {
      // open index
      error = openIndex(indexHandle,indexHandle->databaseFileName);
      if (error != ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Cannot open index database '$s' fail: %s\n",
                    indexHandle->databaseFileName,
                    Error_getText(error)
                   );
        return error;
      }
    }
  }
  else
  {
    error = createIndex(indexHandle,indexHandle->databaseFileName);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create index database '$s' fail: %s\n",
                  indexHandle->databaseFileName,
                  Error_getText(indexHandle->upgradeError)
                 );
      return error;
    }
  }

  // start clean-up thread
  if (!Thread_init(&cleanupIndexThread,"Index clean-up",0,cleanupIndexThreadCode,indexHandle))
  {
    HALT_FATAL_ERROR("Cannot initialize index clean-up thread!");
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Index_done(IndexHandle *indexHandle)
#else /* not NDEBUG */
  void __Index_done(const char  *__fileName__,
                    uint        __lineNb__,
                    IndexHandle *indexHandle
                   )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  indexHandle->quitFlag = TRUE;
  Thread_join(&cleanupIndexThread);
//  Thread_join(&cleanupIndexThread);

  #ifdef NDEBUG
    (void)closeIndex(indexHandle);
  #else /* not NDEBUG */
    (void)__closeIndex(__fileName__,__lineNb__,indexHandle);
  #endif /* NDEBUG */
}

bool Index_findById(IndexHandle *indexHandle,
                    DatabaseId  storageId,
                    String      jobUUID,
                    String      scheduleUUID,
                    String      storageName,
                    IndexStates *indexState,
                    uint64      *lastCheckedTimestamp
                   )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                            WHERE storage.id=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  result = Database_getNextRow(&databaseQueryHandle,
                               "%S %S %S %d %llu",
                               jobUUID,
                               scheduleUUID,
                               storageName,
                               indexState,
                               lastCheckedTimestamp
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

bool Index_findByName(IndexHandle  *indexHandle,
                      StorageTypes storageType,
                      ConstString  hostName,
                      ConstString  loginName,
                      ConstString  deviceName,
                      ConstString  fileName,
                      String       jobUUID,
                      String       scheduleUUID,
                      DatabaseId   *storageId,
                      IndexStates  *indexState,
                      uint64       *lastCheckedTimestamp
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = DATABASE_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.id, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                           "
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  storageName = String_new();
  Storage_initSpecifier(&storageSpecifier);
  foundFlag   = FALSE;
  while (   !foundFlag
         && Database_getNextRow(&databaseQueryHandle,
                                "%S %S %lld %S %d %llu",
                                jobUUID,
                                scheduleUUID,
                                storageId,
                                storageName,
                                indexState,
                                lastCheckedTimestamp
                               )
        )
  {
    if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_FILESYSTEM))
                      && ((fileName == NULL) || String_equals(fileName,storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_FTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_SSH) || (storageType == STORAGE_TYPE_SCP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_SFTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_WEBDAV))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_CD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_DVD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_BD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.archiveName));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_DEVICE))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.archiveName));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
  }
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);

  Database_finalize(&databaseQueryHandle);

  return foundFlag;
}

bool Index_findByState(IndexHandle   *indexHandle,
                       IndexStateSet indexStateSet,
                       String        jobUUID,
                       String        scheduleUUID,
                       DatabaseId    *storageId,
                       String        storageName,
                       uint64        *lastCheckedTimestamp
                      )
{
  Errors              error;
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = DATABASE_ID_NONE;
  if (storageName != NULL) String_clear(storageName);
  if (lastCheckedTimestamp != NULL) (*lastCheckedTimestamp) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  indexStateSetString = String_new();
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                            WHERE storage.state IN (%S) \
                           ",
                           getIndexStateSetString(indexStateSetString,indexStateSet)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }
  String_delete(indexStateSetString);
  result = Database_getNextRow(&databaseQueryHandle,
                               "%S %S %lld %S %llu",
                               jobUUID,
                               scheduleUUID,
                               storageId,
                               storageName,
                               lastCheckedTimestamp
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

Errors Index_getState(IndexHandle *indexHandle,
                      DatabaseId  storageId,
                      IndexStates *indexState,
                      uint64      *lastCheckedTimestamp,
                      String      errorMessage
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT state, \
                                   STRFTIME('%%s',lastChecked), \
                                   errorMessage \
                            FROM storage \
                            WHERE id=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%d %llu %S",
                           indexState,
                           lastCheckedTimestamp,
                           errorMessage
                          )
     )
  {
    (*indexState) = INDEX_STATE_UNKNOWN;
    if (errorMessage != NULL) String_clear(errorMessage);
  }
  Database_finalize(&databaseQueryHandle);

  return ERROR_NONE;
}

Errors Index_setState(IndexHandle *indexHandle,
                      DatabaseId  storageId,
                      IndexStates indexState,
                      uint64      lastCheckedTimestamp,
                      const char  *errorMessage,
                      ...
                     )
{
  Errors  error;
  va_list arguments;
  String  s;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET state=%d, \
                                errorMessage=NULL \
                            WHERE id=%lld; \
                           ",
                           indexState,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (lastCheckedTimestamp != 0LL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET lastChecked=DATETIME(%llu,'unixepoch') \
                              WHERE id=%lld; \
                             ",
                             lastCheckedTimestamp,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    s = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET errorMessage=%'S \
                              WHERE id=%lld; \
                             ",
                             s,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      String_delete(s);
      return error;
    }

    String_delete(s);
  }
  else
  {
  }

  return ERROR_NONE;
}

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  long                count;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(id) \
                            FROM storage \
                            WHERE state=%d \
                           ",
                           indexState
                          );
  if (error != ERROR_NONE)
  {
    return -1L;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%ld",
                           &count
                          )
     )
  {
    Database_finalize(&databaseQueryHandle);
    return -1L;
  }
  Database_finalize(&databaseQueryHandle);

  return count;
}

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle
                          )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
#if 0
                           "SELECT entities.jobUUID, \
                                   STRFTIME('%%s',(SELECT created FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1)), \
                                   (SELECT SUM(entries) FROM storage LEFT JOIN entities AS storageEntities ON storage.entityId=storageEntities.id WHERE storageEntities.jobUUID=entities.jobUUID), \
                                   (SELECT SUM(size) FROM storage LEFT JOIN entities AS storageEntities ON storage.entityId=storageEntities.id WHERE storageEntities.jobUUID=entities.jobUUID), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities \
                            GROUP BY entities.jobUUID; \
                           "
#else
                           "SELECT entities1.jobUUID, \
                                   STRFTIME('%%s',(SELECT created FROM storage WHERE storage.entityId=entities1.id ORDER BY created DESC LIMIT 0,1)), \
                                   ( \
                                      (SELECT COUNT(id) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT COUNT(id) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT COUNT(id) FROM directories WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT COUNT(id) FROM links WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT COUNT(id) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT COUNT(id) FROM special WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                   ), \
                                   ( \
                                      (SELECT TOTAL(size) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities AS entities2 WHERE entities2.jobUUID=entities1.jobUUID) \
                                        ) \
                                      ) \
                                   ), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities1.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities AS entities1 \
                            GROUP BY entities1.jobUUID; \
                           "
#endif
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       String           jobUUID,
                       uint64           *lastCreatedDateTime,
                       uint64           *totalEntries,
                       uint64           *totalSize,
                       String           lastErrorMessage
                      )
{
  double totalSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

#if 0
  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%S %llu %llu %llu %S",
                             jobUUID,
                             lastCreatedDateTime,
                             totalEntries,
                             totalSize,
                             lastErrorMessage
                            );
#else
  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%S %llu %llu %lf %S",
                           jobUUID,
                           lastCreatedDateTime,
                           totalEntries,
                           &totalSize_,
                           lastErrorMessage
                          )
     )
  {
    return FALSE;
  }
  if (totalSize != NULL) (*totalSize) = (uint64)totalSize_;

  return TRUE;
#endif
}

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        ConstString jobUUID
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          entityId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete entities of UUID
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM entities \
                            WHERE jobUUID=%'S; \
                           ",
                           jobUUID
                          );
  if (error == ERROR_NONE)
  {
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,entityId);
    }
    Database_finalize(&databaseQueryHandle);
  }

  return error;
}

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              DatabaseOrdering ordering,
                              ulong            offset
                             )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }
/*
                                        (SELECT id FROM storage WHERE entityId IN \
                                          (SELECT id FROM entities WHERE (%d OR jobUUID=%'S) AND (%d OR scheduleUUID=%'S)) \
                                        ) \

                                   ( \
                                      (SELECT COUNT(id) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM directories WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM links WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM special WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                   ), \
                                   ( \
                                      (SELECT TOTAL(size) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                   ), \
*/

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
#if 0
                           "SELECT entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   STRFTIME('%%s',entities.created), \
                                   entities.type, \
                                   (SELECT SUM(entries) FROM storage WHERE storage.entityId=entities.id), \
                                   (SELECT SUM(size) FROM storage WHERE storage.entityId=entities.id), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities \
                            WHERE     (%d OR jobUUID=%'S) \
                                  AND (%d OR scheduleUUID=%'S) \
                            ORDER BY entities.created %s \
                            LIMIT -1 OFFSET %lu \
                           ",
                           String_isEmpty(jobUUID) ? 1 : 0,
                           jobUUID,
                           String_isEmpty(scheduleUUID) ? 1 : 0,
                           scheduleUUID,
                           getOrderingString(ordering),
                           offset
#else
                           "SELECT entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   STRFTIME('%%s',entities.created), \
                                   entities.type, \
                                   ( \
                                      (SELECT COUNT(id) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM directories WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM links WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT COUNT(id) FROM special WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                   ), \
                                   ( \
                                      (SELECT TOTAL(size) FROM files WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM images WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                     +(SELECT TOTAL(size) FROM hardlinks WHERE storageId IN \
                                        (SELECT id FROM storage WHERE entityId=entities.id) \
                                      ) \
                                   ), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities \
                            WHERE     (%d OR jobUUID=%'S) \
                                  AND (%d OR scheduleUUID=%'S) \
                            ORDER BY entities.created %s \
                            LIMIT -1 OFFSET %lu \
                           ",
                           String_isEmpty(jobUUID     ) ? 1 : 0, jobUUID,
                           String_isEmpty(scheduleUUID) ? 1 : 0, scheduleUUID,
                           getOrderingString(ordering),
                           offset
#endif
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         DatabaseId       *databaseId,
                         String           jobUUID,
                         String           scheduleUUID,
                         uint64           *createdDateTime,
                         ArchiveTypes     *archiveType,
                         uint64           *totalEntries,
                         uint64           *totalSize,
                         String           lastErrorMessage
                        )
{
  double totalSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

#if 0
  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %S %llu %u %llu %llu %S",
                             databaseId,
                             jobUUID,
                             scheduleUUID,
                             createdDateTime,
                             archiveType,
                             totalEntries,
                             totalSize,
                             lastErrorMessage
                            );
#else
  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %S %llu %u %llu %lf %S",
                           databaseId,
                           jobUUID,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
                           totalEntries,
                           &totalSize_,
                           lastErrorMessage
                          )
     )
  {
    return FALSE;
  }
  if (totalSize != NULL) (*totalSize) = (uint64)totalSize_;

  return TRUE;
#endif
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ArchiveTypes archiveType,
                       DatabaseId   *entityId
                      )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(entityId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO entities \
                              ( \
                               jobUUID, \
                               scheduleUUID, \
                               created, \
                               type, \
                               parentJobUUID, \
                               bidFlag\
                              ) \
                            VALUES \
                              ( \
                               %'S, \
                               %'S, \
                               DATETIME('now'), \
                               %u, \
                               '', \
                               0\
                              ); \
                           ",
                           jobUUID,
                           scheduleUUID,
                           archiveType
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*entityId) = Database_getLastRowId(&indexHandle->databaseHandle);
fprintf(stderr,"%s, %d: >>>>>>>>>>>>>>>>Index_newEntity %llu\n",__FILE__,__LINE__,(*entityId));

  return ERROR_NONE;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          DatabaseId  entityId
                         )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete storage of entity
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM storage \
                            WHERE entityId=%lld; \
                           ",
                           entityId
                          );
  if (error == ERROR_NONE)
  {
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &storageId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteStorage(indexHandle,storageId);
    }
    Database_finalize(&databaseQueryHandle);
  }

  // delete entity
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entities WHERE id=%lld;",
                             entityId
                            );
  }

  return error;
}

Errors Index_initListStorage(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             ConstString      jobUUID,
                             DatabaseId       entityId,
                             StorageTypes     storageType,
                             ConstString      storageName,
                             ConstString      hostName,
                             ConstString      loginName,
                             ConstString      deviceName,
                             ConstString      fileName,
                             IndexStateSet    indexStateSet,
                             const DatabaseId storageIds[],
                             uint             storageIdCount
                            )
{
  Errors error;
  String indexStateSetString;
  String storageIdsString;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);
  indexQueryHandle->storage.type = storageType;
  if (storageName != NULL) indexQueryHandle->storage.storageNamePattern = Pattern_new(storageName,PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (hostName    != NULL) indexQueryHandle->storage.hostNamePattern    = Pattern_new(hostName,   PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (loginName   != NULL) indexQueryHandle->storage.loginNamePattern   = Pattern_new(loginName,  PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (deviceName  != NULL) indexQueryHandle->storage.deviceNamePattern  = Pattern_new(deviceName, PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (fileName    != NULL) indexQueryHandle->storage.fileNamePattern    = Pattern_new(fileName,   PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);

  indexStateSetString = String_new();
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
#if 0
                           "SELECT storage.id, \
                                   storage.entityId, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   storage.entries, \
                                   storage.size, \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                            LEFT JOIN entities ON entities.id=storage.entityId \
                            WHERE     (%d OR (entities.jobUUID='%S')) \
                                  AND (%d OR (storage.entityId=%lld)) \
                                  AND storage.state IN (%S) \
                                  AND %S \
                            ORDER BY storage.created DESC \
                           ",
#else
                           "SELECT storage.id, \
                                   storage.entityId, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   ( \
                                      (SELECT COUNT(id) FROM files       WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM images      WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM directories WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM links       WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM hardlinks   WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM special     WHERE storageId=storage.id) \
                                   ), \
                                   ( \
                                      (SELECT TOTAL(size) FROM files     WHERE storageId=storage.id) \
                                     +(SELECT TOTAL(size) FROM images    WHERE storageId=storage.id) \
                                     +(SELECT TOTAL(size) FROM hardlinks WHERE storageId=storage.id) \
                                   ), \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                            LEFT JOIN entities ON entities.id=storage.entityId \
                            WHERE     (%d OR (entities.jobUUID='%S')) \
                                  AND (%d OR (storage.entityId=%lld)) \
                                  AND storage.state IN (%S) \
                                  AND %S \
                            ORDER BY storage.created DESC \
                           ",
#endif
                           String_isEmpty(jobUUID) ? 1 : 0,
                           jobUUID,
                           (entityId == DATABASE_ID_ANY) ? 1 : 0,
                           entityId,
                           getIndexStateSetString(indexStateSetString,indexStateSet),
                           storageIdsString
                          );
  String_delete(storageIdsString);
  String_delete(indexStateSetString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          DatabaseId       *storageId,
                          DatabaseId       *entityId,
                          String           jobUUID,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          String           storageName,
                          uint64           *createdDateTime,
                          uint64           *entries,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage
                         )
{
  StorageSpecifier storageSpecifier;
  bool             foundFlag;
  String           storageName_;
  double           size_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  Storage_initSpecifier(&storageSpecifier);
  storageName_ = String_new();
  foundFlag    = FALSE;
  while (   !foundFlag
#if 0
         && Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                                "%lld %lld %S %S %d %S %llu %llu %llu %d %d %llu %S",
                                storageId,
                                entityId,
                                jobUUID,
                                scheduleUUID,
                                archiveType,
                                storageName,
                                createdDateTime,
                                entries,
                                size,
                                indexState,
                                indexMode,
                                lastCheckedDateTime,
                                errorMessage
                               )
#else
         && Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                                "%lld %lld %S %S %d %S %llu %llu %lf %d %d %llu %S",
                                storageId,
                                entityId,
                                jobUUID,
                                scheduleUUID,
                                archiveType,
                                storageName_,
                                createdDateTime,
                                entries,
                                &size_,
                                indexState,
                                indexMode,
                                lastCheckedDateTime,
                                errorMessage
                               )
#endif
        )
  {
    if (Storage_parseName(&storageSpecifier,storageName_) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,             PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FTP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,                PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern,   storageSpecifier.hostName,   PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,  storageSpecifier.loginName,  PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,   storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SSH) || (indexQueryHandle->storage.type == STORAGE_TYPE_SCP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,              PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName,   PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,  PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SFTP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,              PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName,   PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,  PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_WEBDAV))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,              PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName,   PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,  PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_CD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DVD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_BD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,                PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DEVICE))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.archiveName,PATTERN_MATCH_MODE_ANY));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
    else
    {
      foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM))
                  && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName_,PATTERN_MATCH_MODE_ANY));
    }
    if (storageName != NULL) String_set(storageName,storageName_);
    if (size != NULL) (*size) = (uint64)size_;
  }
  String_delete(storageName_);
  Storage_doneSpecifier(&storageSpecifier);

  return foundFlag;
}

Errors Index_newStorage(IndexHandle *indexHandle,
                        DatabaseId  entityId,
                        ConstString storageName,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        DatabaseId  *storageId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO storage \
                              ( \
                               entityId, \
                               name, \
                               created, \
                               state, \
                               mode, \
                               lastChecked\
                              ) \
                            VALUES \
                              ( \
                               %d, \
                               %'S, \
                               DATETIME('now'), \
                               %d, \
                               %d, \
                               DATETIME('now')\
                              ); \
                           ",
                           entityId,
                           storageName,
                           indexState,
                           indexMode
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*storageId) = Database_getLastRowId(&indexHandle->databaseHandle);

  return ERROR_NONE;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           DatabaseId  storageId
                          )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM storage WHERE id=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;

  return ERROR_NONE;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          DatabaseId  storageId
                         )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%lld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%lld;",
                           storageId
                          );

  return ERROR_NONE;
}


Errors Index_getStorage(IndexHandle *indexHandle,
                        DatabaseId  storageId,
                        String      storageName,
                        uint64      *createdDateTime,
                        uint64      *entries,
                        uint64      *size,
                        IndexStates *indexState,
                        IndexModes  *indexMode,
                        uint64      *lastCheckedDateTime,
                        String      errorMessage
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
#if 0
                           "SELECT storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   storage.entries, \
                                   storage.size, \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                            WHERE id=%d \
                           ",
#else
#warning TODO replace size
                           "SELECT storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   ( \
                                      (SELECT COUNT(id) FROM files       WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM images      WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM directories WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM links       WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM hardlinks   WHERE storageId=storage.id) \
                                     +(SELECT COUNT(id) FROM special     WHERE storageId=storage.id) \
                                   ), \
                                   storage.size, \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                            WHERE id=%d \
                           ",
#endif
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%S %lld %lld %lld %d %d %lld %S",
                           storageName,
                           createdDateTime,
                           entries,
                           size,
                           indexState,
                           indexMode,
                           lastCheckedDateTime,
                           errorMessage
                          )
     )
  {
    Database_finalize(&databaseQueryHandle);
    return ERROR_DATABASE_INDEX_NOT_FOUND;
  }
  Database_finalize(&databaseQueryHandle);

  return ERROR_NONE;
}

Errors Index_storageUpdate(IndexHandle *indexHandle,
                           DatabaseId  storageId,
                           ConstString storageName,
                           uint64      storageSize
                          )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // update name
  if (storageName != NULL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET name=%'S \
                              WHERE id=%lld; \
                             ",
                             storageName,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // update size
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET size=%llu \
                            WHERE id=%lld; \
                           ",
                           storageSize,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const DatabaseId storageIds[],
                           uint             storageIdCount,
                           const DatabaseId entryIds[],
                           uint             entryIdCount,
                           String           pattern
                          )
{
  Errors error;
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"files.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_files",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(files.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(files.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT files.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   files.name, \
                                   files.size, \
                                   files.timeModified, \
                                   files.userId, \
                                   files.groupId, \
                                   files.permission, \
                                   files.fragmentOffset, \
                                   files.fragmentSize\
                            FROM files \
                              LEFT JOIN storage ON storage.id=files.storageId \
                            WHERE     (files.id IN (SELECT fileId FROM FTS_files WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       DatabaseId       *databaseId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           fileName,
                       uint64           *size,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission,
                       uint64           *fragmentOffset,
                       uint64           *fragmentSize
                      )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_deleteFile(IndexHandle *indexHandle,
                        DatabaseId  databaseId
                       )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%lld;",
                          databaseId
                         );
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const DatabaseId *storageIds,
                            uint             storageIdCount,
                            const DatabaseId entryIds[],
                            uint             entryIdCount,
                            String           pattern
                           )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"images.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_images",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(images.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(images.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT images.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   images.name, \
                                   images.fileSystemType, \
                                   images.size, \
                                   images.blockOffset, \
                                   images.blockCount \
                            FROM images \
                              LEFT JOIN storage ON storage.id=images.storageId \
                            WHERE     (images.id IN (SELECT imageId FROM FTS_images WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        DatabaseId       *databaseId,
                        String           storageName,
                        uint64           *storageDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %u %llu %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             imageName,
                             fileSystemType,
                             size,
                             blockOffset,
                             blockCount
                            );
}

Errors Index_deleteImage(IndexHandle *indexHandle,
                         DatabaseId  databaseId
                        )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%lld;",
                          databaseId
                         );
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const DatabaseId *storageIds,
                                 uint             storageIdCount,
                                 const DatabaseId entryIds[],
                                 uint             entryIdCount,
                                 String           pattern
                                )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"directories.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_directories",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(directories.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(directories.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT directories.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   directories.name, \
                                   directories.timeModified, \
                                   directories.userId, \
                                   directories.groupId, \
                                   directories.permission \
                            FROM directories \
                              LEFT JOIN storage ON storage.id=directories.storageId \
                            WHERE     (directories.id IN (SELECT directoryId FROM FTS_directories WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            DatabaseId       *databaseId,
                            String           storageName,
                            uint64           *storageDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             directoryName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             DatabaseId  databaseId
                            )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM directories WHERE id=%lld;",
                          databaseId
                         );
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const DatabaseId *storageIds,
                           uint             storageIdCount,
                           const DatabaseId entryIds[],
                           uint             entryIdCount,
                           String           pattern
                          )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"links.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_links",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(links.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(links.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT links.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   links.name, \
                                   links.destinationName, \
                                   links.timeModified, \
                                   links.userId, \
                                   links.groupId, \
                                   links.permission \
                            FROM links \
                              LEFT JOIN storage ON storage.id=links.storageId \
                            WHERE     (links.id IN (SELECT linkId FROM FTS_links WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       DatabaseId       *databaseId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           linkName,
                       String           destinationName,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission
                      )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             linkName,
                             destinationName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteLink(IndexHandle *indexHandle,
                        DatabaseId  databaseId
                       )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM links WHERE id=%lld;",
                          databaseId
                         );
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const DatabaseId *storageIds,
                               uint             storageIdCount,
                               const DatabaseId entryIds[],
                               uint             entryIdCount,
                               String           pattern
                              )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"hardlinks.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_hardlinks",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(hardlinks.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(hardlinks.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT hardlinks.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   hardlinks.name, \
                                   hardlinks.size, \
                                   hardlinks.timeModified, \
                                   hardlinks.userId, \
                                   hardlinks.groupId, \
                                   hardlinks.permission, \
                                   hardlinks.fragmentOffset, \
                                   hardlinks.fragmentSize\
                            FROM hardlinks \
                              LEFT JOIN storage ON storage.id=hardlinks.storageId \
                            WHERE     (hardlinks.id IN (SELECT hardlinkId FROM FTS_hardlinks WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));


  return error;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           DatabaseId       *databaseId,
                           String           storageName,
                           uint64           *storageDateTime,
                           String           fileName,
                           uint64           *size,
                           uint64           *timeModified,
                           uint32           *userId,
                           uint32           *groupId,
                           uint32           *permission,
                           uint64           *fragmentOffset,
                           uint64           *fragmentSize
                          )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            DatabaseId  databaseId
                           )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM hardlinks WHERE id=%lld;",
                          databaseId
                         );
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const DatabaseId *storageIds,
                             uint             storageIdCount,
                             const DatabaseId entryIds[],
                             uint             entryIdCount,
                             String           pattern
                            )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"special.name",pattern);
  ftsString    = getFTSString   (String_new(),"FTS_special",pattern);
  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(special.storageId IN (");
    for (i = 0; i < storageIdCount; i++)
    {
      if (i > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[i]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }
  if (entryIds != NULL)
  {
    entryIdsString = String_newCString("(special.id IN (");
    for (i = 0; i < entryIdCount; i++)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
    String_appendCString(entryIdsString,"))");
  }
  else
  {
    entryIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT special.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   special.name, \
                                   special.timeModified, \
                                   special.userId, \
                                   special.groupId, \
                                   special.permission \
                            FROM special \
                              LEFT JOIN storage ON storage.id=special.storageId \
                            WHERE     (special.id IN (SELECT specialId FROM FTS_special WHERE %S)) \
                                  AND (%S) \
                                  AND (%S) \
                                  AND (%S) \
                           ",
                           ftsString,
                           regexpString,
                           storageIdsString,
                           entryIdsString
                          );
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(ftsString);
  String_delete(regexpString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return error;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          DatabaseId       *databaseId,
                          String           storageName,
                          uint64           *storageDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             name,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           DatabaseId  databaseId
                          )
{
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM special WHERE id=%lld;",
                          databaseId
                         );
}

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  Database_finalize(&indexQueryHandle->databaseQueryHandle);
  doneIndexQueryHandle(indexQueryHandle);
}

Errors Index_addFile(IndexHandle *indexHandle,
                     DatabaseId  storageId,
                     ConstString fileName,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission,
                     uint64      fragmentOffset,
                     uint64      fragmentSize
                    )
{
  Errors     error;
  DatabaseId fileId;

  assert(indexHandle != NULL);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add file entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO files \
                              ( \
                               storageId, \
                               name, \
                               size, \
                               timeLastAccess, \
                               timeModified, \
                               timeLastChanged, \
                               userId, \
                               groupId, \
                               permission, \
                               fragmentOffset, \
                               fragmentSize\
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %u, \
                               %u, \
                               %u, \
                               %lu, \
                               %lu\
                              ); \
                           ",
                           storageId,
                           fileName,
                           size,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission,
                           fragmentOffset,
                           fragmentSize
                          );
  if (error == ERROR_NONE)
  {
    // get file id
    fileId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_files \
                                ( \
                                 fileId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             fileId,
                             fileName
                            );
  }

  return error;
}

Errors Index_addImage(IndexHandle     *indexHandle,
                      DatabaseId      storageId,
                      ConstString     imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors     error;
  DatabaseId imageId;

  assert(indexHandle != NULL);
  assert(imageName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add image entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO images \
                              ( \
                               storageId, \
                               name, \
                               fileSystemType, \
                               size, \
                               blockSize, \
                               blockOffset, \
                               blockCount\
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %d, \
                               %lu, \
                               %u, \
                               %lu, \
                               %lu\
                              ); \
                           ",
                           storageId,
                           imageName,
                           fileSystemType,
                           size,
                           blockSize,
                           blockOffset,
                           blockCount
                          );
  if (error == ERROR_NONE)
  {
    // get image id
    imageId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_images \
                                ( \
                                 imageId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             imageId,
                             imageName
                            );
  }

  return error;
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          DatabaseId  storageId,
                          String      directoryName,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  Errors     error;
  DatabaseId directoryId;

  assert(indexHandle != NULL);
  assert(directoryName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add directory entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO directories \
                              ( \
                               storageId, \
                               name, \
                               timeLastAccess, \
                               timeModified, \
                               timeLastChanged, \
                               userId, \
                               groupId, \
                               permission \
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %u, \
                               %u, \
                               %u \
                              ); \
                           ",
                           storageId,
                           directoryName,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission
                          );
  if (error == ERROR_NONE)
  {
    // get directory id
    directoryId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_directories \
                                ( \
                                 directoryId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             directoryId,
                             directoryName
                            );
  }

  return error;
}

Errors Index_addLink(IndexHandle *indexHandle,
                     DatabaseId  storageId,
                     ConstString linkName,
                     ConstString destinationName,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    )
{
  Errors     error;
  DatabaseId linkId;

  assert(indexHandle != NULL);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add link entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO links \
                              ( \
                               storageId, \
                               name, \
                               destinationName, \
                               timeLastAccess, \
                               timeModified, \
                               timeLastChanged, \
                               userId, \
                               groupId, \
                               permission\
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %'S, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %u, \
                               %u, \
                               %u\
                              ); \
                            ",
                           storageId,
                           linkName,
                           destinationName,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission
                          );
  if (error == ERROR_NONE)
  {
    // get link id
    linkId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_links \
                                ( \
                                 linkId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             linkId,
                             linkName
                            );
  }

  return error;
}

Errors Index_addHardLink(IndexHandle *indexHandle,
                         DatabaseId  storageId,
                         ConstString fileName,
                         uint64      size,
                         uint64      timeLastAccess,
                         uint64      timeModified,
                         uint64      timeLastChanged,
                         uint32      userId,
                         uint32      groupId,
                         uint32      permission,
                         uint64      fragmentOffset,
                         uint64      fragmentSize
                        )
{
  Errors     error;
  DatabaseId hardLinkId;

  assert(indexHandle != NULL);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add hard link entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO hardlinks \
                              ( \
                               storageId, \
                               name, \
                               size, \
                               timeLastAccess, \
                               timeModified, \
                               timeLastChanged, \
                               userId, \
                               groupId, \
                               permission, \
                               fragmentOffset, \
                               fragmentSize\
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %u, \
                               %u, \
                               %u, \
                               %lu, \
                               %lu\
                              ); \
                           ",
                           storageId,
                           fileName,
                           size,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission,
                           fragmentOffset,
                           fragmentSize
                          );
  if (error == ERROR_NONE)
  {
    // get hard link id
    hardLinkId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_hardLinks \
                                ( \
                                 hardLinkId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             hardLinkId,
                             fileName
                            );
  }

  return error;
}

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        DatabaseId       storageId,
                        ConstString      name,
                        FileSpecialTypes specialType,
                        uint64           timeLastAccess,
                        uint64           timeModified,
                        uint64           timeLastChanged,
                        uint32           userId,
                        uint32           groupId,
                        uint32           permission,
                        uint32           major,
                        uint32           minor
                       )
{
  Errors     error;
  DatabaseId specialId;

  assert(indexHandle != NULL);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // add special entry
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO special \
                              ( \
                               storageId, \
                               name, \
                               specialType, \
                               timeLastAccess, \
                               timeModified, \
                               timeLastChanged, \
                               userId, \
                               groupId, \
                               permission, \
                               major, \
                               minor \
                              ) \
                            VALUES \
                              ( \
                               %lu, \
                               %'S, \
                               %u, \
                               %lu, \
                               %lu, \
                               %lu, \
                               %u, \
                               %u, \
                               %u, \
                               %d, \
                               %u\
                              ); \
                           ",
                           storageId,
                           name,
                           specialType,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission,
                           major,
                           minor
                          );

  if (error == ERROR_NONE)
  {
    // get special id
    specialId = Database_getLastRowId(&indexHandle->databaseHandle);

    // add full text search
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO FTS_special \
                                ( \
                                 specialId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lu, \
                                 %'S \
                                ); \
                             ",
                             specialId,
                             name
                            );
  }

  return error;
}

Errors Index_assignTo(IndexHandle *indexHandle,
                      ConstString jobUUID,
                      DatabaseId  entityId,
                      DatabaseId  storageId,
                      DatabaseId  toEntityId,
                      DatabaseId  toStorageId
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if      (toEntityId != DATABASE_ID_NONE)
  {
    // assign to other entity

    if (storageId != DATABASE_ID_NONE)
    {
      // assign storage to other entity
      error = assignStorageToEntity(indexHandle,
                                    storageId,
                                    toEntityId
                                   );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (entityId != DATABASE_ID_NONE)
    {
      // assign all storage entries of entity to other entity
      error = assignEntityToEntity(indexHandle,
                                   entityId,
                                   toEntityId
                                  );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (!String_isEmpty(jobUUID))
    {
      // assign all entities of job to other entity
      error = assignJobToEntity(indexHandle,
                                jobUUID,
                                toEntityId
                               );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else if (toStorageId != DATABASE_ID_NONE)
  {
    // assign to other storage

    if (storageId != DATABASE_ID_NONE)
    {
      // assign storage entries to other storage
      error = assignStorageToStorage(indexHandle,storageId,toStorageId);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (entityId != DATABASE_ID_NONE)
    {
      // assign all storage entries of entity to other storage
      error = assignEntityToStorage(indexHandle,entityId,toStorageId);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (!String_isEmpty(jobUUID))
    {
      // assign all storage entries of all entities of job to other storage
      error = assignJobToStorage(indexHandle,
                                 jobUUID,
                                 toStorageId
                                );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }

  return ERROR_NONE;
}

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          DatabaseId  storageId
                         )
{
  bool       existsFlag;
  uint       i;
  DatabaseId id;
  Errors     error;

  // check if entries exists for storage
  existsFlag = FALSE;
  for (i = 0; i < SIZE_OF_ARRAY(ENTRY_TABLE_NAMES); i++)
  {
    if (Database_exists(&indexHandle->databaseHandle,
                        ENTRY_TABLE_NAMES[i],
                        "id",
                        "WHERE storageId=%lld",
                        storageId
                       )
       )
    {
      existsFlag = TRUE;
      break;
    }
    UNUSED_VARIABLE(id);
  }

  // prune storage if empty
  if (!existsFlag)
  {
    // delete storage entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM storage WHERE id=%lld;",
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete directory if empty
  }

  return ERROR_NONE;
}

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         DatabaseId  entityId
                         )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       storageId;
  bool             existsFlag;

fprintf(stderr,"%s, %d: try prune entiry %llu\n",__FILE__,__LINE__,entityId);

  // prune storage of entity
  error = Index_initListStorage(&indexQueryHandle,
                                indexHandle,
                                NULL, // uuid
                                entityId,
                                STORAGE_TYPE_ANY,
                                NULL, // storageName
                                NULL, // hostName
                                NULL, // loginName
                                NULL, // deviceName
                                NULL, // fileName
                                INDEX_STATE_SET_ALL,
                                NULL,  // storageIds
                                0   // storageIdCount
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              &storageId,
                              NULL, // entity id
                              NULL, // job UUID
                              NULL, // schedule UUID
                              NULL, // archive type
                              NULL, // storageName,
                              NULL, // createdDateTime
                              NULL, // entries
                              NULL, // size
                              NULL, // indexState
                              NULL, // indexMode
                              NULL, // lastCheckedDateTime
                              NULL  // errorMessage
                             )
        )
  {
    error = Index_pruneStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      break;
    }
  }
  Index_doneList(&indexQueryHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // check if storage exists
  existsFlag = FALSE;
  if (Database_exists(&indexHandle->databaseHandle,
                      "storage",
                      "id",
                      "WHERE entityId=%lld",
                      entityId
                     )
     )
  {
    existsFlag = TRUE;
  }
  UNUSED_VARIABLE(storageId);

  // prune entity if empty
  if (!existsFlag)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entities WHERE id=%lld;",
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
