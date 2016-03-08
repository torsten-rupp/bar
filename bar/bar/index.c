/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index functions
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
//TODO: requried?
  { "*",      INDEX_MODE_SET_ALL }
};

LOCAL const struct
{
  const char *name;
  IndexTypes indexType;
} INDEX_TYPES[] =
{
  { "FILE",      INDEX_TYPE_FILE      },
  { "IMAGE",     INDEX_TYPE_IMAGE     },
  { "DIRECTORY", INDEX_TYPE_DIRECTORY },
  { "LINK",      INDEX_TYPE_LINK      },
  { "HARDLINK",  INDEX_TYPE_HARDLINK  },
  { "SPECIAL",   INDEX_TYPE_SPECIAL   },
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
typedef enum
{
  INDEX_OPEN_MODE_READ,
  INDEX_OPEN_MODE_READ_WRITE,
} IndexOpenModes;

/***************************** Variables *******************************/
LOCAL Thread cleanupIndexThread;    // clean-up thread

/****************************** Macros *********************************/

// create index id
#define INDEX_ID_(type,databaseId) ((IndexId)((__IndexId){{type,databaseId}}).data)

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
*          indexOpenMode    - open mode; see IndexOpenModes
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle    *indexHandle,
                         const char     *databaseFileName,
                         IndexOpenModes indexOpenMode
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char     *__fileName__,
                           uint           __lineNb__,
                           IndexHandle    *indexHandle,
                           const char     *databaseFileName,
                           IndexOpenModes indexOpenMode
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
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->upgradeError     = ERROR_NONE;
  indexHandle->quitFlag         = FALSE;

  if (indexOpenMode == INDEX_OPEN_MODE_READ)
  {
    // disable synchronous mode and journal to increase transaction speed
    (void)Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);

    // enable foreign key constrains
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
  }

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
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->upgradeError     = ERROR_NONE;
  indexHandle->quitFlag         = FALSE;

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

  // enable foreign key constrains
  Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

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
  error = openIndex(&indexHandle,databaseFileName,INDEX_OPEN_MODE_READ);
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

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

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
                             // post: transfer files, images, directories, links, special entries
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

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

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

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

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

uint64 tx;
  // transfer entities with storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             // pre: transfer entity
                             CALLBACK(NULL,NULL),
                             // post: transfer storages
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors  error;
                               IndexId fromEntityId;
                               IndexId toEntityId;

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toEntityId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               // transfer storages of entity
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
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
                                                            toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
fprintf(stderr,"%s, %d: copy storage %s of entity %llu: %llu -> %llu\n",__FILE__,__LINE__,Database_getTableColumnListCString(fromColumnList,"name",NULL),toEntityId,fromStorageId,toStorageId);
uint64 t0 = Misc_getTimestamp();

                                                            error = ERROR_NONE;

                                                            // Note: first directories to update totalEntryCount/totalEntrySize
tx=Misc_getTimestamp();
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "directories",
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

#if 0
fprintf(stderr,"%s, %d: copty dir %llu: %s\n",__FILE__,__LINE__,
Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE),
Database_getTableColumnListCString(fromColumnList,"name",NULL)
);
#endif
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: c\n",__FILE__,__LINE__); exit(12); }
fprintf(stderr,"%s, %d: copt dir %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);

tx=Misc_getTimestamp();
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "files",
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

#if 0
fprintf(stderr,"%s, %d: copty file %llu: %s %llubytes\n",__FILE__,__LINE__,
Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE),
Database_getTableColumnListCString(fromColumnList,"name",NULL),
Database_getTableColumnListInt64(fromColumnList,"size",0)
);
#endif
                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK(NULL,NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: a %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }
fprintf(stderr,"%s, %d: copt files %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);

tx=Misc_getTimestamp();
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
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: b %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }
fprintf(stderr,"%s, %d: copt image %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);

tx=Misc_getTimestamp();
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
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: d %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }
fprintf(stderr,"%s, %d: copt links %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);

tx=Misc_getTimestamp();
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
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: e %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }
fprintf(stderr,"%s, %d: copt hardlinks %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);

tx=Misc_getTimestamp();
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
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: f %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }
fprintf(stderr,"%s, %d: copt special %llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-tx)/1000);
uint64 t1 = Misc_getTimestamp();
fprintf(stderr,"%s, %d: copt storage total %llums\n\n",__FILE__,__LINE__,(t1-t0)/1000);

                                                            return error;
                                                          },NULL),
                                                          "WHERE entityId=%llu",
                                                          fromEntityId
                                                         );

                               return error;
                             },NULL),
                             NULL  // filter
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // pre: transfer storage and create entities
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findByStorageId(oldIndexHandle,
                                                            INDEX_ID_(INDEX_TYPE_STORAGE,Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE)),
                                                            NULL,  // jobUUID
                                                            NULL,  // scheduleUUDI
                                                            NULL,  // uuidId
                                                            &entityId,
                                                            NULL,  // storageName
                                                            NULL,  // indexState
                                                            NULL  // lastCheckedDateTime
                                                           )
                                   && Index_findByJobUUID(newIndexHandle,
                                                          jobUUID,
                                                          NULL,  // scheduleUUDI
                                                          NULL,  // uuidId
                                                          &entityId,
                                                          NULL,  // createdDateTime
                                                          NULL,  // archiveType
                                                          NULL,  // lastErrorMessage
                                                          NULL,  // totalEntryCount
                                                          NULL  // totalSize
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
                                                         &entityId
                                                        );
                               }
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",entityId);

                               return error;
                             },NULL),
                             // post: transfer files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors  error;
                               IndexId fromStorageId;
                               IndexId toStorageId;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               error = ERROR_NONE;

                               // Note: first directories to update totalEntryCount/totalEntrySize
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

                               return error;
                             },NULL),
                             "WHERE entityId IS NULL"
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

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

  // transfer entities with storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             // pre: transfer entity
                             CALLBACK(NULL,NULL),
                             // post: transfer storage
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors  error;
                               IndexId fromEntityId;
                               IndexId toEntityId;

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toEntityId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);
//fprintf(stderr,"%s, %d: jobUUID=%s\n",__FILE__,__LINE__,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL));

                               // transfer storages of entity
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
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
                                                            toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

//fprintf(stderr,"%s, %d: start storage %llu -> %llu\n",__FILE__,__LINE__,fromStorageId,toStorageId);
                                                            error = ERROR_NONE;

                                                            // Note: first directories to update totalEntryCount/totalEntrySize
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

                                                            return error;
                                                          },NULL),
                                                          "WHERE entityId=%llu",
                                                          fromEntityId
                                                         );

                               return error;
                             },NULL),
                             NULL  // filter
                            );
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             // pre: transfer storage and create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
                               IndexId      entityId;

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findByStorageId(oldIndexHandle,
                                                            INDEX_ID_(INDEX_TYPE_STORAGE,Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE)),
                                                            jobUUID,
                                                            NULL,  // scheduleUUDI
                                                            NULL,  // uuidId
                                                            NULL,  // entityId
                                                            NULL,  // storageName
                                                            NULL,  // indexState
                                                            NULL  // lastCheckedDateTime
                                                           )
                                   && Index_findByJobUUID(newIndexHandle,
                                                          jobUUID,
                                                          NULL,  // scheduleUUDI
                                                          NULL,  // uuidId
                                                          &entityId,
                                                          NULL,  // createdDateTime
                                                          NULL,  // archiveType
                                                          NULL,  // lastErrorMessage
                                                          NULL,  // totalEntryCount
                                                          NULL  // totalSize,
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
                               toStorageId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               error = ERROR_NONE;

                               // Note: first directories to update totalEntryCount/totalEntrySize
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

                               return error;
                             },NULL),
                             "WHERE entityId IS NULL"
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
  error = openIndex(&oldIndexHandle,String_cString(oldDatabaseFileName),INDEX_OPEN_MODE_READ);
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
                                    AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1)); \
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
  IndexId          storageId;
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
                           NULL,  // jobUUID
                           NULL,  // scheduleUUID
                           NULL,  // uuidId
                           NULL,  // entityId
                           &storageId,
                           NULL,  // storageName
                           NULL  // lastCheckedDateTime
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
  IndexId          storageId;
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
                           NULL,  // jobUUID
                           NULL,  // scheduleUUID
                           NULL,  // uuidId
                           NULL,  // entityId
                           &storageId,
                           storageName,
                           NULL  // lastCheckedDateTime
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
  IndexId          indexId;

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
                                &indexId,
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
        (void)Index_deleteFile(indexHandle,indexId);
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
                                 &indexId,
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
        (void)Index_deleteImage(indexHandle,indexId);
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
                                     &indexId,
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
        (void)Index_deleteDirectory(indexHandle,indexId);
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
                                &indexId,
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
        (void)Index_deleteLink(indexHandle,indexId);
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
                                    &indexId,
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
        (void)Index_deleteHardLink(indexHandle,indexId);
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
                                   &indexId,
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
        (void)Index_deleteSpecial(indexHandle,indexId);
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
  IndexId          storageId;
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
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 STORAGE_TYPE_ANY,
                                 NULL,  // storageIds
                                 0,  // storageIdCount,
                                 INDEX_STATE_SET_ALL,
                                 NULL,  // pattern
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // entityId
                                &storageId,
                                NULL,  // jobUUID
                                NULL,  // schedule UUID
                                NULL,  // archive type
                                storageName,
                                NULL,  // createdDateTime
                                NULL,  // entries
                                NULL,  // size
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
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
  IndexId          storageId;
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
    error = Index_initListStorages(&indexQueryHandle1,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entityId
                                   NULL,  // jobUUID
                                   STORAGE_TYPE_ANY,
                                   NULL,  // storageIds
                                   0,  // storageIdCount
                                   INDEX_STATE_SET_ALL,
                                   NULL,  // pattern
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      break;
    }
    while (   !indexHandle->quitFlag
           && !deletedIndexFlag
           && Index_getNextStorage(&indexQueryHandle1,
                                   NULL,  // uuidId
                                   NULL,  // entityId
                                   &storageId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // archiveType
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // entries
                                   NULL,  // size
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL   // errorMessage
                                  )
          )
    {
      // check for duplicate entry
      error = Index_initListStorages(&indexQueryHandle2,
                                     indexHandle,
                                     INDEX_ID_ANY,  // uuidId
                                     INDEX_ID_ANY,  // entityId
                                     NULL,  // jobUUID
                                     STORAGE_TYPE_ANY,
                                     NULL,  // storageIds
                                     0,  // storageIdCount
                                     INDEX_STATE_SET_ALL,
                                     NULL,  // pattern
                                     0LL,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        continue;
      }
      while (   !indexHandle->quitFlag
             && !deletedIndexFlag
             && Index_getNextStorage(&indexQueryHandle2,
                                     NULL,  // uuidId
                                     NULL,  // entityId
                                     &duplicateStorageId,
                                     NULL,  // jobUUID
                                     NULL,  // scheduleUUID
                                     NULL,  // archiveType
                                     duplicateStorageName,
                                     NULL,  // createdDateTime
                                     NULL,  // entries
                                     NULL,  // size
                                     NULL,  // indexState
                                     NULL,  // indexMode
                                     NULL,  // lastCheckedDateTime
                                     NULL   // errorMessage
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
                             0LL,  // lastCheckedDateTime
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
  String              oldDatabaseFileName;
  uint                oldDatabaseCount;
  uint                sleepTime;

  // get absolute file name of database
  absoluteFileName = File_getAbsoluteFileNameCString(String_new(),indexHandle->databaseFileName);

  // open directory
  pathName = File_getFilePathName(String_new(),absoluteFileName);
fprintf(stderr,"%s, %d: File_openDirectoryList %s \n",__FILE__,__LINE__,String_cString(pathName));
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

  // process all *.oldNNN files
  oldDatabaseFileName = String_new();
  oldDatabaseCount    = 0;
  while (File_readDirectoryList(&directoryListHandle,oldDatabaseFileName) == ERROR_NONE)
  {
    if (   String_startsWith(oldDatabaseFileName,absoluteFileName)\
        && String_matchCString(oldDatabaseFileName,STRING_BEGIN,".*\\.old\\d\\d\\d$",NULL,NULL)
       )
    {
      if (oldDatabaseCount == 0)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "Start upgrade index database\n"
                   );
      }
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Started import index database '%s'\n",
                  String_cString(oldDatabaseFileName)
                 );

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
                    "Import index database '%s' fail\n",
                    String_cString(oldDatabaseFileName)
                   );
      }
      if (indexHandle->upgradeError == ERROR_NONE)
      {
        indexHandle->upgradeError = error;
      }

      oldDatabaseCount++;
    }
  }
  if (oldDatabaseCount > 0)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Upgrade index database done\n"
               );
  }
  String_delete(oldDatabaseFileName);

  // close directory
  File_closeDirectoryList(&directoryListHandle);

  // free resources
  String_delete(absoluteFileName);

  // single clean-ups
#warning remove
#if 0
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
#endif
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

#warning TODO: remove
LOCAL void initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  indexQueryHandle->indexHandle                = indexHandle;
  indexQueryHandle->storage.type               = STORAGE_TYPE_NONE;
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
* Name   : getFTSString
* Purpose: get full-text-search filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedPatternFlag;
  ulong           i;
  char            ch;

  String_clear(string);
  if (!String_isEmpty(patternText))
  {
    String_clear(string);
    String_appendChar(string,'\'');
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
    String_appendChar(string,'\'');
  }

  return string;
}

/***********************************************************************\
* Name   : getREGEXPString
* Purpose: get REGEXP filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getREGEXPString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  ulong           i;
  char            ch;

  String_clear(string);
  if (!String_isEmpty(patternText))
  {
    String_clear(string);
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_appendChar(string,'\'');
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
      String_appendChar(string,'\'');
    }
    String_doneTokenizer(&stringTokenizer);
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
* Name   : filterAppend
* Purpose: append filter
* Input  : filter       - filter string
*          condition    - append iff true
*          concatenator - concatenator string
*          format       - format string (printf-style)
*          ...          - optional arguments for format
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void filterAppend(String filter, bool condition, const char *concatenator, const char *format, ...)
{
  va_list arguments;

  if (condition)
  {
    if (!String_isEmpty(filter))
    {
      String_appendChar(filter,' ');
      String_appendCString(filter,concatenator);
      String_appendChar(filter,' ');
    }
    va_start(arguments,format);
    String_vformat(filter,format,arguments);
    va_end(arguments);
  }
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
                                    IndexId     storageId,
                                    IndexId     toStorageId
                                   )
{
  Errors           error;
  uint             i;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(INDEX_TYPE_(toStorageId) == INDEX_TYPE_STORAGE);

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
                             INDEX_DATABASE_ID_(toStorageId),
                             INDEX_DATABASE_ID_(storageId)
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
                                   IndexId     entityId,
                                   IndexId     toStorageId
                                  )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);
  assert(INDEX_TYPE_(toStorageId) == INDEX_TYPE_STORAGE);

  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 STORAGE_TYPE_ANY,
                                 NULL,  // storageIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 NULL,  // pattern
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              NULL,  // uuidId
                              NULL,  // entityId
                              &storageId,
                              NULL,  // jobUUID
                              NULL,  // scheduleUUID
                              NULL,  // archiveType
                              NULL,  // storageName
                              NULL,  // createdDateTime
                              NULL,  // entries
                              NULL,  // size
                              NULL,  // indexState
                              NULL,  // indexMode
                              NULL,  // lastCheckedDateTime
                              NULL   // errorMessage
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
                                IndexId     toStorageId
                               )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(toStorageId) == INDEX_TYPE_STORAGE);

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidId,
                             &entityId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // createdDateTime,
                             NULL,  // archiveType,
                             NULL,   // lastErrorMessage
                             NULL,  // totalEntryCount,
                             NULL  // totalSize,
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
                                   IndexId     storageId,
                                   IndexId     toEntityId
                                  )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(INDEX_TYPE_(toEntityId) == INDEX_TYPE_ENTITY);

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET entityId=%lld \
                            WHERE id=%lld; \
                           ",
                           INDEX_DATABASE_ID_(toEntityId),
                           INDEX_DATABASE_ID_(storageId)
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
* Input  : indexHandle   - index handle
*          entityId      - entity id
*          toEntityId    - to entity id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle  *indexHandle,
                                  IndexId      entityId,
                                  IndexId      toEntityId,
                                  ArchiveTypes toArchiveType
                                 )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);
  assert(INDEX_TYPE_(toEntityId) == INDEX_TYPE_ENTITY);

  // assign to entity
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET entityId=%lld \
                            WHERE entityId=%lld; \
                           ",
                           INDEX_DATABASE_ID_(toEntityId),
                           INDEX_DATABASE_ID_(entityId)
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

  // set archive type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE entities \
                              SET type=%d \
                              WHERE id=%lld; \
                             ",
                             toArchiveType,
                             INDEX_DATABASE_ID_(toEntityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignJobToEntity
* Purpose: assign all entities of job to other entity
* Input  : indexHandle   - index handle
*          jobUUID       - job UUID
*          toEntityId    - to entity id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToEntity(IndexHandle  *indexHandle,
                               ConstString  jobUUID,
                               IndexId      toEntityId,
                               ArchiveTypes toArchiveType
                              )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(toEntityId) == INDEX_TYPE_ENTITY);

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidId,
                             &entityId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // createdDateTime,
                             NULL,  // archiveType,
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount,
                             NULL  // totalSize,
                            )
        )
  {
    // assign all storage entries of entity to other entity
    error = assignEntityToEntity(indexHandle,entityId,toEntityId,toArchiveType);
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

  assert(indexHandle != NULL);

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
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[i].indexState != indexState)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState)
{
  uint i;

  assert(name != NULL);
  assert(indexState != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[i].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[i].indexMode != indexMode)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode)
{
  uint i;

  assert(name != NULL);
  assert(indexMode != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[i].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseType(const char *name, IndexTypes *indexType)
{
  uint i;

  assert(name != NULL);
  assert(indexType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_TYPES))
         && !stringEqualsIgnoreCase(INDEX_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_TYPES))
  {
    (*indexType) = INDEX_TYPES[i].indexType;
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

  // check if index exists
  if (File_existsCString(databaseFileName))
  {
    // get index version
    error = getIndexVersion(databaseFileName,&indexVersion);
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
        oldDatabaseFileName = String_newCString(databaseFileName);
        String_appendCString(oldDatabaseFileName,".old");
        String_format(oldDatabaseFileName,"%03d",n);
        n++;
      }
      while (File_exists(oldDatabaseFileName));
      (void)File_renameCString(databaseFileName,
                               String_cString(oldDatabaseFileName),
                               NULL
                              );
      String_delete(oldDatabaseFileName);

      // create new index
      error = createIndex(indexHandle,databaseFileName);
      if (error != ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Create new index database '$s' fail: %s\n",
                    databaseFileName,
                    Error_getText(error)
                   );
        return error;
      }
    }
    else
    {
      // open index
      error = openIndex(indexHandle,databaseFileName,INDEX_OPEN_MODE_READ_WRITE);
      if (error != ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Cannot open index database '$s' fail: %s\n",
                    databaseFileName,
                    Error_getText(error)
                   );
        return error;
      }
    }
  }
  else
  {
    error = createIndex(indexHandle,databaseFileName);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create index database '$s' fail: %s\n",
                  databaseFileName,
                  Error_getText(error)
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

Errors Index_beginTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);

  return Database_beginTransaction(&indexHandle->databaseHandle,name);
}

Errors Index_endTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);

  return Database_endTransaction(&indexHandle->databaseHandle,name);
}

Errors Index_rollbackTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);

  return Database_rollbackTransaction(&indexHandle->databaseHandle,name);
}

bool Index_containsType(const IndexId indexIds[],
                        uint          indexIdCount,
                        IndexTypes    indexType
                       )
{
  uint i;

  assert(indexIds != NULL);

  for (i = 0; i < indexIdCount; i++)
  {
    if (Index_getType(indexIds[i]) == indexType)
    {
      return TRUE;
    }
  }

  return FALSE;
}

bool Index_findByJobUUID(IndexHandle  *indexHandle,
                         ConstString  jobUUID,
                         ConstString  scheduleUUID,
                         IndexId      *uuidId,
                         IndexId      *entityId,
                         uint64       *createdDateTime,
                         ArchiveTypes *archiveType,
                         String       lastErrorMessage,
                         uint64       *totalEntryCount,
                         uint64       *totalSize
                        )
{
  String              filter;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","jobUUID=%'S",scheduleUUID);
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","scheduleUUID=%'S",scheduleUUID);
//TODO errorMessage
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   entities.id, \
                                   STRFTIME('%%s',entities.created), \
                                   entities.type, \
                                   '', \
                                   entities.totalEntryCount, \
                                   entities.totalEntrySize \
                            FROM entities \
                              LEFT JOIN uuids ON uuids.jobUUID=entityId.jobUUID \
                            WHERE %s \
                            GROUP BY entities.id \
                            LIMIT 0,1 \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(filter);
    return FALSE;
  }
  result = Database_getNextRow(&databaseQueryHandle,
                               "%llu %llu %llu %d %S %llu %llu",
                               &uuidId_,
                               &entityId_,
                               createdDateTime,
                               archiveType,
                               lastErrorMessage,
                               totalEntryCount,
                               totalSize
                              );
  Database_finalize(&databaseQueryHandle);

  String_delete(filter);

  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);

  return result;
}

bool Index_findByStorageId(IndexHandle *indexHandle,
                           IndexId     storageId,
                           String      jobUUID,
                           String      scheduleUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           IndexStates *indexState,
                           uint64      *lastCheckedDateTime
                          )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                              LEFT JOIN entities ON storage.entityId=entities.id \
                              LEFT JOIN uuids ON entitys.jobUUID=uuids.jobUUID \
                            WHERE storage.id=%lld \
                            GROUP BY storage.id \
                            LIMIT 0,1 \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  result = Database_getNextRow(&databaseQueryHandle,
                               "%llu %llu %S %S %S %d %llu",
                               &uuidId_,
                               &entityId_,
                               jobUUID,
                               scheduleUUID,
                               storageName,
                               indexState,
                               lastCheckedDateTime
                              );
  Database_finalize(&databaseQueryHandle);
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);

  return result;
}

bool Index_findByStorageName(IndexHandle  *indexHandle,
                             StorageTypes storageType,
                             ConstString  hostName,
                             ConstString  loginName,
                             ConstString  deviceName,
                             ConstString  fileName,
                             IndexId      *uuidId,
                             IndexId      *entityId,
                             String       jobUUID,
                             String       scheduleUUID,
                             IndexId      *storageId,
                             IndexStates  *indexState,
                             uint64       *lastCheckedDateTime
                            )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;
  DatabaseId          uuidId_,entityId_,storageId_;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = INDEX_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   entities.id, \
                                   storage.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                              LEFT JOIN entities ON storage.entityId=entities.id \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                            GROUP BY storage.id \
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
                                "%llu %llu %lld %S %S %S %d %llu",
                                &uuidId_,
                                &entityId_,
                                &storageId_,
                                jobUUID,
                                scheduleUUID,
                                storageName,
                                indexState,
                                lastCheckedDateTime
                               )
        )
  {
    if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
    if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);
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
                       IndexId       *uuidId,
                       IndexId       *entityId,
                       IndexId       *storageId,
                       String        storageName,
                       uint64        *lastCheckedDateTime
                      )
{
  Errors              error;
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          uuidId_,entityId_,storageId_;
  bool                result;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  if (entityId != NULL) (*entityId) = INDEX_ID_NONE;
  (*storageId) = INDEX_ID_NONE;
  if (storageName != NULL) String_clear(storageName);
  if (lastCheckedDateTime != NULL) (*lastCheckedDateTime) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  indexStateSetString = String_new();
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   entities.id, \
                                   storage.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                              LEFT JOIN entities ON storage.entityId=entities.id \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                            WHERE (storage.state IN (%S)) \
                            LIMIT 0,1 \
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
                               "%llu %llu %lld %S %S %S %llu",
                               &uuidId_,
                               &entityId_,
                               &storageId_,
                               jobUUID,
                               scheduleUUID,
                               storageName,
                               lastCheckedDateTime
                              );
  Database_finalize(&databaseQueryHandle);

  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
  if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);

  return result;
}

Errors Index_getState(IndexHandle *indexHandle,
                      IndexId     storageId,
                      IndexStates *indexState,
                      uint64      *lastCheckedDateTime,
                      String      errorMessage
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

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
                           lastCheckedDateTime,
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
                      IndexId     storageId,
                      IndexStates indexState,
                      uint64      lastCheckedDateTime,
                      const char  *errorMessage,
                      ...
                     )
{
  Errors  error;
  va_list arguments;
  String  s;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

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
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (lastCheckedDateTime != 0LL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET lastChecked=DATETIME(%llu,'unixepoch') \
                              WHERE id=%lld; \
                             ",
                             lastCheckedDateTime,
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
                           IndexHandle      *indexHandle,
                           uint64           offset,
                           uint64           limit
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

//TODO
//Database_debugEnable(1);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
#if 0
                           "SELECT entities.jobUUID, \
                                   STRFTIME('%%s',(SELECT created FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1)), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1), \
                                   (SELECT SUM(entries) FROM storage LEFT JOIN entities AS storageEntities ON storage.entityId=storageEntities.id WHERE storageEntities.jobUUID=entities.jobUUID), \
                                   (SELECT SUM(size) FROM storage LEFT JOIN entities AS storageEntities ON storage.entityId=storageEntities.id WHERE storageEntities.jobUUID=entities.jobUUID) \
                            FROM entities \
                            GROUP BY entities.jobUUID; \
                           "
#elif 0
//TODO: usage total*
                           "SELECT entities1.id, \
                                   entities1.jobUUID, \
                                   STRFTIME('%%s',(SELECT MAX(created) FROM storage WHERE storage.entityId=entities1.id)), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities1.id ORDER BY created DESC LIMIT 0,1), \
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
                                   ) \
                            FROM entities AS entities1 \
                            GROUP BY entities1.jobUUID; \
                           "
#else
//TODO: usage total*
                           "SELECT id, \
                                   jobUUID, \
                                   lastCreated, \
                                   lastErrorMessage, \
                                   totalEntryCount, \
                                   totalEntrySize \
                            FROM uuids \
                            LIMIT %llu,%llu \
                           ",
                           offset,
                           limit
#endif
                          );
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *uuidId,
                       String           jobUUID,
                       uint64           *lastCreatedDateTime,
                       String           lastErrorMessage,
                       uint64           *totalEntries,
                       uint64           *totalSize
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

#if 0
  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%S %llu %S %llu %llu",
                             jobUUID,
                             lastCreatedDateTime,
                             lastErrorMessage,
                             totalEntries,
                             totalSize
                            );
#else
  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%llu %S %llu %S %llu %llu",
                           &databaseId,
                           jobUUID,
                           lastCreatedDateTime,
                           lastErrorMessage,
                           totalEntries,
                           totalSize
                          )
     )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,databaseId);

  return TRUE;
#endif
}

Errors Index_newUUID(IndexHandle *indexHandle,
                     ConstString jobUUID,
                     IndexId     *uuidId
                    )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO uuids \
                              ( \
                               jobUUID \
                              ) \
                            VALUES \
                              ( \
                               %'S \
                              ); \
                           ",
                           jobUUID
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,Database_getLastRowId(&indexHandle->databaseHandle));

  return ERROR_NONE;
}

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        ConstString jobUUID
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexId             entityId;

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
                              IndexId          uuidId,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              DatabaseOrdering ordering,
                              ulong            offset,
                              uint64           limit
                             )
{
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  filter = String_newCString("1");
  filterAppend(filter,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",INDEX_DATABASE_ID_(uuidId));
  filterAppend(filter,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id,\
                                   entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   STRFTIME('%%s',entities.created), \
                                   entities.type, \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1), \
                                   entities.totalEntryCount, \
                                   entities.totalEntrySize \
                            FROM entities \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                            WHERE %s \
                            ORDER BY entities.created %s \
                            LIMIT %llu,%llu \
                           ",
                           String_cString(filter),
                           getOrderingString(ordering),
                           offset,
                           limit
                          );
  String_delete(filter);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *uuidId,
                         IndexId          *entityId,
                         String           jobUUID,
                         String           scheduleUUID,
                         uint64           *createdDateTime,
                         ArchiveTypes     *archiveType,
                         String           lastErrorMessage,
                         uint64           *totalEntryCount,
                         uint64           *totalSize
                        )
{
  DatabaseId uuidId_,entityId_;
  double     totalSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

#if 0
  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %lld %S %S %llu %u %S %llu %llu",
                             &uuidId_,
                             &entityId_,
                             jobUUID,
                             scheduleUUID,
                             createdDateTime,
                             archiveType,
                             lastErrorMessage,
                             totalEntryCount,
                             totalSize
                            );
#else
  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %lld %S %S %llu %u %S %llu %lf",
                           &uuidId_,
                           &entityId_,
                           jobUUID,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
                           lastErrorMessage,
                           totalEntryCount,
                           &totalSize_
                          )
     )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_ENTITY,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
  if (totalSize != NULL) (*totalSize) = (uint64)totalSize_;

  return TRUE;
#endif
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ArchiveTypes archiveType,
                       IndexId      *entityId
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
  (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,Database_getLastRowId(&indexHandle->databaseHandle));
fprintf(stderr,"%s, %d: >>>>>>>>>>>>>>>>Index_newEntity %llu\n",__FILE__,__LINE__,(*entityId));

  return ERROR_NONE;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexId             storageId;

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
                             INDEX_DATABASE_ID_(entityId)
                            );
  }

  return error;
}

Errors Index_getStoragesInfo(IndexHandle   *indexHandle,
                             const IndexId indexIds[],
                             uint          indexIdCount,
                             IndexStateSet indexStateSet,
                             ConstString   pattern,
                             ulong         *count,
                             uint64        *size
                            )
{
  String              ftsString;
  String              regexpString;
  String              uuidIdsString,entityIdsString,storageIdsString;
  uint                i;
  String              filter,unionFilter,storageFilter;
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  ulong               count_;
  double              totalEntryCount_,totalEntrySize_;

  assert(indexHandle != NULL);
  assert((indexIdCount == 0) || (indexIds != NULL));
  assert(count != NULL);

  if (count != NULL) (*count) = 0L;
  if (size != NULL) (*size) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  if (indexIds != NULL)
  {
    for (i = 0; i < indexIdCount; i++)
    {
      switch (Index_getType(indexIds[i]))
      {
        case INDEX_TYPE_UUID:
          if (i > 0) String_appendChar(uuidIdsString,',');
          String_format(uuidIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        case INDEX_TYPE_ENTITY:
          if (i > 0) String_appendChar(entityIdsString,',');
          String_format(entityIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        case INDEX_TYPE_STORAGE:
          if (i > 0) String_appendChar(storageIdsString,',');
          String_format(storageIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        default:
          // ignore other types
          break;
      }
    }
  }

  if (   !String_isEmpty(ftsString)
      || !String_isEmpty(regexpString)
      || (indexIds != NULL)
     )
  {
    filter = String_newCString("1");

    storageFilter = String_new();
    if (!String_isEmpty(ftsString) || !String_isEmpty(regexpString) || (indexIds != NULL))
    {
      filterAppend(storageFilter,indexIds != NULL,"AND","storage.id IN (%S)",storageIdsString);
      filterAppend(storageFilter,!String_isEmpty(ftsString),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsString);
//      filterAppend(storageFilter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,storage.name)",regexpString);
    }

    unionFilter = String_new();
    filterAppend(unionFilter,!String_isEmpty(storageFilter),"UNION ALL","SELECT storage.id FROM storage WHERE %S",storageFilter);
    filterAppend(unionFilter,indexIds != NULL,"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (%S)",entityIdsString);
    filterAppend(unionFilter,indexIds != NULL,"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (SELECT entities.id FROM entities WHERE entities.jobUUID IN (SELECT entities.jobUUID FROM entities WHERE entities.id IN (%S)))",uuidIdsString);

    indexStateSetString = String_new();

    filterAppend(filter,TRUE,"AND","storage.id IN (%S)",unionFilter);
    filterAppend(filter,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));

    String_delete(indexStateSetString);
    String_delete(storageFilter);
    String_delete(unionFilter);
  }
  else
  {
    filter = String_newCString("1");
  }
//Database_debugEnable(1);
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT COUNT(storage.id),\
                                   TOTAL(storage.totalEntryCount), \
                                   TOTAL(storage.totalEntrySize) \
                            FROM storage \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    String_delete(filter);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(regexpString);
    String_delete(ftsString);
    return error;
  }
  if (Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf %lf",
                          &count_,
                          &totalEntryCount_,
                          &totalEntrySize_
                         )
        )
  {
    if (count != NULL) (*count) = count_;
//TODO
//    if (totalCount != NULL) (*size) = (uint64)totalEntrySize_;
    if (size != NULL) (*size) = (uint64)totalEntrySize_;
  }
  Database_finalize(&databaseQueryHandle);

  // free resources
  String_delete(filter);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(regexpString);
  String_delete(ftsString);

  return ERROR_NONE;
}

Errors Index_initListStorages(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
                              IndexId          entityId,
                              ConstString      jobUUID,
                              StorageTypes     storageType,
                              const IndexId    indexIds[],
                              uint             indexIdCount,
                              IndexStateSet    indexStateSet,
                              ConstString      pattern,
                              uint64           offset,
                              uint64           limit
                             )
{
  String regexpStorageName;
  String uuidIdsString,entityIdsString,storageIdsString;
  Errors error;
  String filter;
  String indexStateSetString;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityId == INDEX_ID_ANY) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

//TODO required?
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  indexQueryHandle->storage.type = storageType;

  regexpStorageName = getREGEXPString(String_new(),pattern);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  if (indexIds != NULL)
  {
    for (i = 0; i < indexIdCount; i++)
    {
      switch (Index_getType(indexIds[i]))
      {
        case INDEX_TYPE_UUID:
          if (i > 0) String_appendChar(uuidIdsString,',');
          String_format(uuidIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        case INDEX_TYPE_ENTITY:
          if (i > 0) String_appendChar(entityIdsString,',');
          String_format(entityIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        case INDEX_TYPE_STORAGE:
          if (i > 0) String_appendChar(storageIdsString,',');
          String_format(storageIdsString,"%d",Index_getDatabaseId(indexIds[i]));
          break;
        default:
          // ignore other types
          break;
      }
    }
  }

  indexStateSetString = String_new();

#if 0
  if (   !String_isEmpty(ftsString)
      || !String_isEmpty(regexpString)
      || (indexIds != NULL)
     )
  {
    filter = String_newCString("1");

    storageFilter = String_new();
    if (!String_isEmpty(ftsString) || !String_isEmpty(regexpString) || (indexIds != NULL))
    {
      filterAppend(storageFilter,!String_isEmpty(storageIdsString),"AND","storage.id IN (%S)",storageIdsString);
      filterAppend(storageFilter,!String_isEmpty(ftsString),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsString);
//      filterAppend(storageFilter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,storage.name)",regexpString);
    }

    unionFilter = String_new();
    filterAppend(unionFilter,!String_isEmpty(storageFilter),"UNION ALL","SELECT storage.id FROM storage WHERE %S",storageFilter);
    filterAppend(unionFilter,indexIds != NULL,"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (%S)",entityIdsString);
    filterAppend(unionFilter,indexIds != NULL,"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (SELECT entities.id FROM entities WHERE entities.jobUUID IN (SELECT entities.jobUUID FROM entities WHERE entities.id IN (%S)))",uuidIdsString);

    indexStateSetString = String_new();

    filterAppend(filter,TRUE,"AND","storage.id IN (%S)",unionFilter);
    filterAppend(filter,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));

    String_delete(indexStateSetString);
    String_delete(storageFilter);
    String_delete(unionFilter);
  }
  else
  {
    filter = String_newCString("1");
  }
#else
  filter = String_new();
  filterAppend(filter,(entityId != INDEX_ID_ANY),"AND","storage.entityId=%lld",INDEX_DATABASE_ID_(entityId));
  filterAppend(filter,!String_isEmpty(jobUUID),"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filter,indexIds != NULL,"AND","storage.id IN (%S)",storageIdsString);
  filterAppend(filter,(storageType != STORAGE_TYPE_ANY),"AND","entities.type!=%d",storageType);
  filterAppend(filter,!String_isEmpty(regexpStorageName),"AND","REGEXP(%S,0,storage.name)",regexpStorageName);
  filterAppend(filter,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));
#endif

Database_debugEnable(1);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   entities.id, \
                                   storage.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   storage.totalEntryCount, \
                                   storage.totalEntrySize, \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                              LEFT JOIN entities ON entities.id=storage.entityId \
                              LEFT JOIN uuids ON uuids.jobUUID=uuids.jobUUID \
                            WHERE %s \
                            GROUP BY storage.id \
                            ORDER BY storage.created DESC \
                            LIMIT %llu,%llu \
                           ",
                           String_cString(filter),
                           offset,
                           limit
                          );
Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    String_delete(filter);
    String_delete(indexStateSetString);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(regexpStorageName);
    return error;
  }

  String_delete(filter);
  String_delete(indexStateSetString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(regexpStorageName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          IndexId          *entityId,
                          IndexId          *storageId,
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
  DatabaseId       uuidId_,entityId_,storageId_;
  String           storageName_;
  double           size_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

#warning storageName_
  Storage_initSpecifier(&storageSpecifier);
  storageName_ = String_new();
  foundFlag    = FALSE;
  while (   !foundFlag
         && Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                                "%lld %lld %lld %S %S %d %S %llu %llu %lf %d %d %llu %S",
                                &uuidId_,
                                &entityId_,
                                &storageId_,
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
        )
  {
//fprintf(stderr,"%s, %d: storageName_=%s\n",__FILE__,__LINE__,String_cString(storageName_));
    if (Storage_parseName(&storageSpecifier,storageName_) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FTP));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SSH) || (indexQueryHandle->storage.type == STORAGE_TYPE_SCP));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SFTP));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_WEBDAV));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_CD));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DVD));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_BD));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DEVICE));
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
      foundFlag = ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM));
    }
    if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
    if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);
    if (storageName != NULL) String_set(storageName,storageName_);
    if (size != NULL) (*size) = (uint64)size_;
  }
  String_delete(storageName_);
  Storage_doneSpecifier(&storageSpecifier);

  return foundFlag;
}

Errors Index_newStorage(IndexHandle *indexHandle,
                        IndexId     entityId,
                        ConstString storageName,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(storageId != NULL);
  assert((entityId == INDEX_ID_NONE) || (INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY));

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
                           INDEX_DATABASE_ID_(entityId),
                           storageName,
                           indexState,
                           indexMode
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,Database_getLastRowId(&indexHandle->databaseHandle));

  return ERROR_NONE;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM storage WHERE id=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;

  return ERROR_NONE;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%lld;",
                           INDEX_DATABASE_ID_(storageId)
                          );

  return ERROR_NONE;
}


Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId     storageId,
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
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

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
                           INDEX_DATABASE_ID_(storageId)
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
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      storageSize
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // update name
  if (storageName != NULL)
  {
    // update name
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET name=%'S \
                              WHERE id=%lld; \
                             ",
                             storageName,
                             INDEX_DATABASE_ID_(storageId)
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
                           INDEX_DATABASE_ID_(storageId)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId storageIds[],
                            uint          storageIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            ConstString   pattern,
                            ulong         *count,
                            uint64        *size
                           )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              ftsString;
  String              regexpString;
  String              storageIdsString;
  String              fileIdsString,imageIdsString,directoryIdsString,linkIdsString,hardlinkIdsString,specialIdsString;
  uint                i;
  String              filter;
  ulong               count_;
  double              size_;

  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));
  assert(count != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }

  fileIdsString      = String_new();
  imageIdsString     = String_new();
  directoryIdsString = String_new();
  linkIdsString      = String_new();
  hardlinkIdsString  = String_new();
  specialIdsString   = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    switch (Index_getType(entryIds[i]))
    {
      case INDEX_TYPE_FILE:
        if (!String_isEmpty(fileIdsString)) String_appendChar(fileIdsString,',');
        String_format(fileIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_IMAGE:
        if (!String_isEmpty(imageIdsString)) String_appendChar(imageIdsString,',');
        String_format(imageIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_DIRECTORY:
        if (!String_isEmpty(directoryIdsString)) String_appendChar(directoryIdsString,',');
        String_format(directoryIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_LINK:
        if (!String_isEmpty(linkIdsString)) String_appendChar(linkIdsString,',');
        String_format(linkIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_HARDLINK:
        if (!String_isEmpty(hardlinkIdsString)) String_appendChar(hardlinkIdsString,',');
        String_format(hardlinkIdsString,"%ld",INDEX_DATABASE_ID_(entryIds[i]));
        break;
      case INDEX_TYPE_SPECIAL:
        if (!String_isEmpty(specialIdsString)) String_appendChar(specialIdsString,',');
        String_format(specialIdsString,"%ld",INDEX_DATABASE_ID_(entryIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  if (count != NULL) (*count) = 0L;
  if (size != NULL) (*size) = 0LL;

  error  = ERROR_NONE;
  filter = String_new();

  // files
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_FILE))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalFileCount),TOTAL(totalFileSize) \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
//Database_debugEnable(1);
        filterAppend(filter,!String_isEmpty(pattern),"AND","files.id IN (SELECT fileId FROM FTS_files WHERE FTS_files MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,files.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","files.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","files.id IN (%S)",fileIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(files.id),TOTAL(files.size) \
                                    FROM files \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
//Database_debugEnable(0);
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }

  // images
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_IMAGE))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalImageCount),TOTAL(totalImageSize) \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
        filterAppend(filter,!String_isEmpty(pattern),"AND","images.id IN (SELECT imageId FROM FTS_images WHERE FTS_images MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,images.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","images.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","images.id IN (%S)",imageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(images.id),TOTAL(images.size) \
                                    FROM images \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }

  // directories
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_DIRECTORY))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalDirectoryCount),0.0 \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
        filterAppend(filter,!String_isEmpty(pattern),"AND","directories.id IN (SELECT directoryId FROM FTS_directories WHERE FTS_directories MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,directories.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","directories.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","directories.id IN (%S)",directoryIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(directories.id),0.0 \
                                    FROM directories \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }

  // links
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_LINK))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalLinkCount),0.0 \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
        filterAppend(filter,!String_isEmpty(pattern),"AND","links.id IN (SELECT linkId FROM FTS_links WHERE FTS_links MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,links.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","links.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","links.id IN (%S)",linkIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(links.id),0.0 \
                                    FROM links \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }

  // hardlinks
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_HARDLINK))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalHardlinkCount),TOTAL(totalHardlinkSize) \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
        filterAppend(filter,!String_isEmpty(pattern),"AND","hardlinks.id IN (SELECT hardlinkId FROM FTS_hardlinks WHERE FTS_hardlinks MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,hardlinks.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","hardlinks.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","hardlinks.id IN (%S)",hardlinkIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(hardlinks.id),TOTAL(hardlinks.size) \
                                    FROM hardlinks \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }

  // special
  if (error == ERROR_NONE)
  {
    if (IN_SET(indexTypeSet,INDEX_TYPE_SPECIAL))
    {
      String_setCString(filter,"1");
      if (String_isEmpty(pattern) && (entryIdCount == 0))
      {
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(totalSpecialCount),0.0 \
                                    FROM storage \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
      else
      {
        filterAppend(filter,!String_isEmpty(pattern),"AND","special.id IN (SELECT specialId FROM FTS_special WHERE FTS_special MATCH %S)",ftsString);
//        filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,special.name)",regexpString);
        filterAppend(filter,!String_isEmpty(storageIdsString),"AND","special.storageId IN (%S)",storageIdsString);
        filterAppend(filter,!String_isEmpty(fileIdsString),"AND","special.id IN (%S)",specialIdsString);
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(special.id),0.0 \
                                    FROM special \
                                    WHERE %s \
                                 ",
                                 String_cString(filter)
                                );
      }
if (error !=  ERROR_NONE) fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      if (error == ERROR_NONE)
      {
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lu %lf",
                                &count_,
                                &size_
                               )
           )
        {
//fprintf(stderr,"%s, %d: %lu %lf\n",__FILE__,__LINE__,count_,size_);
          if (count != NULL) (*count) += count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
    }
  }
//Database_debugEnable(0);

  // free resources
  String_delete(filter);
  String_delete(specialIdsString);
  String_delete(hardlinkIdsString);
  String_delete(linkIdsString);
  String_delete(directoryIdsString);
  String_delete(imageIdsString);
  String_delete(fileIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);

  return error;
}

Errors Index_initListEntries(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             IndexTypeSet     indexTypeSet,
                             ConstString      pattern,
                             uint64           offset,
                             uint64           limit
                            )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String fileIdsString,imageIdsString,directoryIdsString,linkIdsString,hardlinkIdsString,specialIdsString;
  uint   i;
  String filesFilter,imagesFilter,directoriesFilter,linksFilter,hardlinksFilter,specialFilter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }

  fileIdsString      = String_new();
  imageIdsString     = String_new();
  directoryIdsString = String_new();
  linkIdsString      = String_new();
  hardlinkIdsString  = String_new();
  specialIdsString   = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    switch (Index_getType(entryIds[i]))
    {
      case INDEX_TYPE_FILE:
        if (!String_isEmpty(fileIdsString)) String_appendChar(fileIdsString,',');
        String_format(fileIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_IMAGE:
        if (!String_isEmpty(imageIdsString)) String_appendChar(imageIdsString,',');
        String_format(imageIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_DIRECTORY:
        if (!String_isEmpty(directoryIdsString)) String_appendChar(directoryIdsString,',');
        String_format(directoryIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_LINK:
        if (!String_isEmpty(linkIdsString)) String_appendChar(linkIdsString,',');
        String_format(linkIdsString,"%ld",Index_getDatabaseId(entryIds[i]));
        break;
      case INDEX_TYPE_HARDLINK:
        if (!String_isEmpty(hardlinkIdsString)) String_appendChar(hardlinkIdsString,',');
        String_format(hardlinkIdsString,"%ld",INDEX_DATABASE_ID_(entryIds[i]));
        break;
      case INDEX_TYPE_SPECIAL:
        if (!String_isEmpty(specialIdsString)) String_appendChar(specialIdsString,',');
        String_format(specialIdsString,"%ld",INDEX_DATABASE_ID_(entryIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filesFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_FILE))
  {
    filterAppend(filesFilter,!String_isEmpty(pattern),"AND","files.id IN (SELECT fileId FROM FTS_files WHERE FTS_files MATCH %S)",ftsString);
//    filterAppend(filesFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,files.name)",regexpString);
    filterAppend(filesFilter,!String_isEmpty(storageIdsString),"AND","files.storageId IN (%S)",storageIdsString);
    filterAppend(filesFilter,!String_isEmpty(fileIdsString),"AND","files.id IN (%S)",fileIdsString);
  }
  else
  {
    String_format(filesFilter,"0");
  }

  imagesFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_IMAGE))
  {
    filterAppend(imagesFilter,!String_isEmpty(pattern),"AND","images.id IN (SELECT imageId FROM FTS_images WHERE FTS_images MATCH %S)",ftsString);
//    filterAppend(imagesFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,images.name)",regexpString);
    filterAppend(imagesFilter,!String_isEmpty(storageIdsString),"AND","images.storageId IN (%S)",storageIdsString);
    filterAppend(imagesFilter,!String_isEmpty(imageIdsString),"AND","images.id IN (%S)",imageIdsString);
  }
  else
  {
    String_format(imagesFilter,"0");
  }

  directoriesFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_DIRECTORY))
  {
    filterAppend(directoriesFilter,!String_isEmpty(pattern),"AND","directories.id IN (SELECT directoryId FROM FTS_directories WHERE FTS_directories MATCH %S)",ftsString);
//    filterAppend(directoriesFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,directories.name)",regexpString);
    filterAppend(directoriesFilter,!String_isEmpty(storageIdsString),"AND","directories.storageId IN (%S)",storageIdsString);
    filterAppend(directoriesFilter,!String_isEmpty(directoryIdsString),"AND","directories.id IN (%S)",directoryIdsString);
  }
  else
  {
    String_format(directoriesFilter,"0");
  }

  linksFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_LINK))
  {
    filterAppend(linksFilter,!String_isEmpty(pattern),"AND","links.id IN (SELECT linkId FROM FTS_links WHERE FTS_links MATCH %S)",ftsString);
//    filterAppend(linksFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,links.name)",regexpString);
    filterAppend(linksFilter,!String_isEmpty(storageIdsString),"AND","links.storageId IN (%S)",storageIdsString);
    filterAppend(linksFilter,!String_isEmpty(linkIdsString),"AND","links.id IN (%S)",linkIdsString);
  }
  else
  {
    String_format(linksFilter,"0");
  }

  hardlinksFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_HARDLINK))
  {
    filterAppend(hardlinksFilter,!String_isEmpty(pattern),"AND","hardlinks.id IN (SELECT hardlinkId FROM FTS_hardlinks WHERE FTS_hardlinks MATCH %S)",ftsString);
//    filterAppend(hardlinksFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,hardlinks.name)",regexpString);
    filterAppend(hardlinksFilter,!String_isEmpty(storageIdsString),"AND","hardlinks.storageId IN (%S)",storageIdsString);
    filterAppend(hardlinksFilter,!String_isEmpty(hardlinkIdsString),"AND","hardlinks.id IN (%S)",hardlinkIdsString);
  }
  else
  {
    String_format(hardlinksFilter,"0");
  }

  specialFilter = String_newCString("1");
  if (IN_SET(indexTypeSet,INDEX_TYPE_SPECIAL))
  {
    filterAppend(specialFilter,!String_isEmpty(pattern),"AND","special.id IN (SELECT specialId FROM FTS_special WHERE FTS_special MATCH %S)",ftsString);
//    filterAppend(specialFilter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,special.name)",regexpString);
    filterAppend(specialFilter,!String_isEmpty(storageIdsString),"AND","special.storageId IN (%S)",storageIdsString);
    filterAppend(specialFilter,!String_isEmpty(specialIdsString),"AND","special.id IN (%S)",specialIdsString);
  }
  else
  {
    String_format(specialFilter,"0");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "  SELECT files.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     files.name, \
                                     '', \
                                     0, \
                                     files.size, \
                                     files.timeModified, \
                                     files.userId, \
                                     files.groupId, \
                                     files.permission, \
                                     files.fragmentOffset, \
                                     files.fragmentSize \
                              FROM files \
                                LEFT JOIN storage ON storage.id=files.storageId \
                              WHERE %s \
                           "
                           "UNION ALL"
                           "  SELECT images.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     images.name, \
                                     '', \
                                     images.fileSystemType, \
                                     images.size, \
                                     0, \
                                     0, \
                                     0, \
                                     0, \
                                     images.blockOffset, \
                                     images.blockCount \
                              FROM images \
                                LEFT JOIN storage ON storage.id=images.storageId \
                              WHERE %s \
                           "
                           "UNION ALL"
                           "  SELECT directories.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     directories.name, \
                                     '', \
                                     0, \
                                     0, \
                                     directories.timeModified, \
                                     directories.userId, \
                                     directories.groupId, \
                                     directories.permission, \
                                     0, \
                                     0 \
                              FROM directories \
                                LEFT JOIN storage ON storage.id=directories.storageId \
                              WHERE %s \
                           "
                           "UNION ALL"
                           "  SELECT links.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     links.name, \
                                     links.destinationName, \
                                     0, \
                                     0, \
                                     links.timeModified, \
                                     links.userId, \
                                     links.groupId, \
                                     links.permission, \
                                     0, \
                                     0 \
                              FROM links \
                                LEFT JOIN storage ON storage.id=links.storageId \
                              WHERE %s \
                           "
                           "UNION ALL"
                           "  SELECT hardlinks.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     hardlinks.name, \
                                     '', \
                                     0, \
                                     hardlinks.size, \
                                     hardlinks.timeModified, \
                                     hardlinks.userId, \
                                     hardlinks.groupId, \
                                     hardlinks.permission, \
                                     hardlinks.fragmentOffset, \
                                     hardlinks.fragmentSize \
                              FROM hardlinks \
                                LEFT JOIN storage ON storage.id=hardlinks.storageId \
                              WHERE %s \
                           "
                           "UNION ALL"
                           "  SELECT special.id, \
                                     storage.name, \
                                     STRFTIME('%%s',storage.created), \
                                     %d, \
                                     special.name, \
                                     '', \
                                     0, \
                                     0, \
                                     special.timeModified, \
                                     special.userId, \
                                     special.groupId, \
                                     special.permission, \
                                     0, \
                                     0 \
                              FROM special \
                                LEFT JOIN storage ON storage.id=special.storageId \
                              WHERE %s \
                           "
                           "LIMIT %llu,%llu",
                           INDEX_TYPE_FILE,     String_cString(filesFilter      ),
                           INDEX_TYPE_IMAGE,    String_cString(imagesFilter     ),
                           INDEX_TYPE_DIRECTORY,String_cString(directoriesFilter),
                           INDEX_TYPE_LINK,     String_cString(linksFilter      ),
                           INDEX_TYPE_HARDLINK, String_cString(hardlinksFilter  ),
                           INDEX_TYPE_SPECIAL,  String_cString(specialFilter    ),

                           offset,
                           limit
                          );
  String_delete(specialFilter);
  String_delete(hardlinksFilter);
  String_delete(linksFilter);
  String_delete(directoriesFilter);
  String_delete(imagesFilter);
  String_delete(filesFilter);
  String_delete(specialIdsString);
  String_delete(hardlinkIdsString);
  String_delete(linkIdsString);
  String_delete(directoryIdsString);
  String_delete(imageIdsString);
  String_delete(fileIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNext(IndexQueryHandle  *indexQueryHandle,
                   IndexId           *indexId,
                   String            storageName,
                   uint64            *storageDateTime,
                   String            name,
                   String            destinationName,
                   FileSystemTypes   *fileSystemType,
                   uint64            *size,
                   uint64            *timeModified,
                   uint32            *userId,
                   uint32            *groupId,
                   uint32            *permission,
                   uint64            *fragmentOffsetOrBlockOffset,
                   uint64            *fragmentSizeOrBlockSize
                  )
{
  IndexTypes indexType;
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %d %S %S %d %llu %llu %d %d %d %llu %llu",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           &indexType,
                           name,
                           destinationName,
                           fileSystemType,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           fragmentOffsetOrBlockOffset,
                           fragmentSizeOrBlockSize
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(indexType,databaseId);

  return TRUE;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      pattern
                          )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","files.id IN (SELECT fileId FROM FTS_files WHERE FTS_files MATCH %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,files.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","files.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","files.id IN (%S)",entryIdsString);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
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
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                           &databaseId,
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
                          )
     )
  {
    return FALSE;
  }
 if (indexId != NULL)  (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteFile(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_FILE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
                         );
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    storageIds[],
                            uint             storageIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      pattern
                           )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","images.id IN (SELECT images FROM FTS_files FTS_images FTS_files FTS_images %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,images.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","images.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","images.id IN (%S)",entryIdsString);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        IndexId          *indexId,
                        String           storageName,
                        uint64           *storageDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %u %llu %llu %llu",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           imageName,
                           fileSystemType,
                           size,
                           blockOffset,
                           blockCount
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteImage(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_IMAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
                         );
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    storageIds[],
                                 uint             storageIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      pattern
                                )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","directories.id IN (SELECT directoryId FROM FTS_directories WHERE FTS_directories MATCH %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,directories.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","directories.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","directories.id IN (%S)",entryIdsString);
Database_debugEnable(1);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
Database_debugEnable(0);
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            String           storageName,
                            uint64           *storageDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           directoryName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             IndexId     indexId
                            )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_DIRECTORY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM directories WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
                         );
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      pattern
                          )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }
  String_appendCString(entryIdsString,"))");

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","links.id IN (SELECT linkId FROM FTS_links WHERE FTS_links MATCH %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,links.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","links.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","links.id IN (%S)",entryIdsString);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
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
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           linkName,
                           destinationName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteLink(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_LINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM links WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
                         );
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    storageIds[],
                               uint             storageIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      pattern
                              )
{
  String ftsString;
  String regexpString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","hardlinks.id IN (SELECT hardlinkId FROM FTS_hardlinks WHERE FTS_hardlinks MATCH %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,hardlinks.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","hardlinks.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","hardlinks.id IN (%S)",entryIdsString);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           IndexId          *indexId,
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
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                           &databaseId,
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
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            IndexId     indexId
                           )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_HARDLINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM hardlinks WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
                         );
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      pattern
                            )
{
  String regexpString;
  String ftsString;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsString    = getFTSString   (String_new(),pattern);
  regexpString = getREGEXPString(String_new(),pattern);

  // get id sets
  storageIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(INDEX_TYPE_(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (i > 0) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%d",storageIds[i]);
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (i > 0) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%d",entryIds[i]);
    }
  }

  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(ftsString),"AND","special.id IN (SELECT specialId FROM FTS_special WHERE FTS_special MATCH %S)",ftsString);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,special.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","special.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","special.id IN (%S)",entryIdsString);
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
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpString);
  String_delete(ftsString);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          IndexId          *indexId,
                          String           storageName,
                          uint64           *storageDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           name,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_ENTITY,databaseId);

  return TRUE;
}

Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           IndexId     indexId
                          )
{
  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_SPECIAL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM special WHERE id=%lld;",
                          INDEX_DATABASE_ID_(indexId)
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
                     IndexId     storageId,
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
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
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
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addImage(IndexHandle     *indexHandle,
                      IndexId         storageId,
                      ConstString     imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
                           imageName,
                           fileSystemType,
                           size,
                           blockSize,
                           blockOffset,
                           blockCount
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          IndexId     storageId,
                          String      directoryName,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
                           directoryName,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addLink(IndexHandle *indexHandle,
                     IndexId     storageId,
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
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
                           linkName,
                           destinationName,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addHardLink(IndexHandle *indexHandle,
                         IndexId     storageId,
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
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
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
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        IndexId          storageId,
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
  Errors  error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
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
                           INDEX_DATABASE_ID_(storageId),
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

  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      IndexId      toEntityId,
                      IndexId      toStorageId,
                      ArchiveTypes toArchiveType
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if      (toEntityId != INDEX_ID_NONE)
  {
    // assign to other entity

    if (storageId != INDEX_ID_NONE)
    {
      assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

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

    if (entityId != INDEX_ID_NONE)
    {
      assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);

      // assign all storage entries of entity to other entity
      error = assignEntityToEntity(indexHandle,
                                   entityId,
                                   toEntityId,
                                   toArchiveType
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
                                toEntityId,
                                toArchiveType
                               );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else if (toStorageId != INDEX_ID_NONE)
  {
    // assign to other storage

    if (storageId != INDEX_ID_NONE)
    {
      assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

      // assign storage entries to other storage
      error = assignStorageToStorage(indexHandle,storageId,toStorageId);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (entityId != INDEX_ID_NONE)
    {
      assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);

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
                          IndexId     storageId
                         )
{
  bool    existsFlag;
  uint    i;
  IndexId id;
  Errors  error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  existsFlag = FALSE;
  for (i = 0; i < SIZE_OF_ARRAY(ENTRY_TABLE_NAMES); i++)
  {
    if (Database_exists(&indexHandle->databaseHandle,
                        ENTRY_TABLE_NAMES[i],
                        "id",
                        "WHERE storageId=%lld",
                        INDEX_DATABASE_ID_(storageId)
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
                             INDEX_DATABASE_ID_(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete directory if empty
  }

  return ERROR_NONE;
}

Errors Index_pruneUUID(IndexHandle *indexHandle,
                       IndexId     uuidId
                      )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;
  bool             existsFlag;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(uuidId) == INDEX_TYPE_UUID);

fprintf(stderr,"%s, %d: try prune uuid %llu\n",__FILE__,__LINE__,uuidId);

  // prune entities of uuid
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 uuidId,
                                 NULL,  // jobUUID,
                                 NULL,  // scheduldUUID
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidId,
                             &entityId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // createdDateTime,
                             NULL,  // archiveType,
                             NULL,   // lastErrorMessage
                             NULL,  // totalEntryCount,
                             NULL  // totalSize,
                            )
        )
  {
    error = Index_pruneEntity(indexHandle,entityId);
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

  // check if entity exists
  existsFlag = FALSE;
  if (Database_exists(&indexHandle->databaseHandle,
                      "entities",
                      "id",
                      "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID WHERE uuids.id=%lld",
                      INDEX_DATABASE_ID_(uuidId)
                     )
     )
  {
    existsFlag = TRUE;
  }

  // prune entity if empty
  if (!existsFlag)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM uuids WHERE id=%lld;",
                             INDEX_DATABASE_ID_(uuidId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                         )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  bool             existsFlag;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);

fprintf(stderr,"%s, %d: try prune entiry %llu\n",__FILE__,__LINE__,entityId);

  // prune storage of entity
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 STORAGE_TYPE_ANY,
                                 NULL,   // storageIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 NULL,  // pattern
                                 0LL,   // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              NULL,  // uuidId
                              NULL,  // entityId
                              &storageId,
                              NULL,  // jobUUID
                              NULL,  // scheduleUUID
                              NULL,  // archiveType
                              NULL,  // storageName,
                              NULL,  // createdDateTime
                              NULL,  // entries
                              NULL,  // size
                              NULL,  // indexState
                              NULL,  // indexMode
                              NULL,  // lastCheckedDateTime
                              NULL   // errorMessage
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
                      INDEX_DATABASE_ID_(entityId)
                     )
     )
  {
    existsFlag = TRUE;
  }

  // prune entity if empty
  if (!existsFlag)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entities WHERE id=%lld;",
                             INDEX_DATABASE_ID_(entityId)
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
