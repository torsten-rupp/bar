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
  { "error",            INDEX_STATE_ERROR            },
  { "purge",            INDEX_STATE_PURGE            }
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
const char *__databaseFileName = NULL;

LOCAL Thread cleanupIndexThread;    // clean-up thread
LOCAL bool   quitFlag;

/****************************** Macros *********************************/

// create index id
#define INDEX_ID_(type,databaseId) ((IndexId)((__IndexId){{type,databaseId}}).data)

#ifndef NDEBUG
  #define createIndex(...) __createIndex(__FILE__,__LINE__, ## __VA_ARGS__)
  #define openIndex(...)  __openIndex(__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeIndex(...) __closeIndex(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : createIndex
* Purpose: create new empty index database
* Input  : databaseFileName - database file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createIndex(const char  *databaseFileName)
#else /* not NDEBUG */
  LOCAL Errors __createIndex(const char  *__fileName__,
                             uint        __lineNb__,
                             const char  *databaseFileName
                            )
#endif /* NDEBUG */
{
  Errors         error;
  DatabaseHandle databaseHandle;

  assert(databaseFileName != NULL);

  // create index database
  (void)File_deleteCString(databaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(&databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,0);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,0);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  (void)Database_setEnabledSync(&databaseHandle,FALSE);

  // create tables
  error = Database_execute(&databaseHandle,
                           CALLBACK(NULL,NULL),
                           INDEX_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      Database_close(&databaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,&databaseHandle);
    #endif /* NDEBUG */
    return error;
  }

  // close database
  #ifdef NDEBUG
    Database_close(&databaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,&databaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

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
                         IndexOpenModes indexOpenMode,
                         long           timeout
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char     *__fileName__,
                           uint           __lineNb__,
                           IndexHandle    *indexHandle,
                           const char     *databaseFileName,
                           IndexOpenModes indexOpenMode,
                           long           timeout
                          )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // open index database
  #ifdef NDEBUG
    error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,timeout);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,timeout);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->upgradeError     = ERROR_NONE;
  #ifdef NDEBUG
    indexHandle->threadId = pthread_self();
  #endif /* NDEBUG */

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
  error = openIndex(&indexHandle,databaseFileName,INDEX_OPEN_MODE_READ,0);
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

#if 0
TODO obsolete
/***********************************************************************\
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle     - index handle
*          entryId         - index entry id
*          storageId       - index storage id
*          name            - name
*          size            - size or 0
*          timeLastChanged - time stamp last changed [s]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateNewestInfo(IndexHandle *indexHandle,
                              DatabaseId  storageId,
                              DatabaseId  entryId,
                              IndexTypes  type,
                              const char  *name,
                              uint64      size,
                              uint64      timeLastChanged
                             )
{
  Errors           error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       newestStorageId;
  DatabaseId       newestEntryId;
  uint64           newestSize;
  uint64           newestTimeLastChanged;

return ERROR_NONE;

  // get current newest entry data (if exists)
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT storageId,entryId,size,MAX(timeLastChanged) \
                              FROM entriesNewest \
                              WHERE name=%'s \
                           ",
                           name
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%lld %lld %llu %llu",
                           &newestStorageId,
                           &newestEntryId,
                           &newestSize,
                           &newestTimeLastChanged
                          )
     )
  {
    newestStorageId       = DATABASE_ID_NONE;
    newestEntryId         = DATABASE_ID_NONE;
    newestSize            = 0LL;
    newestTimeLastChanged = 0LL;
  }
  Database_finalize(&databaseQueryHandle);

  // update newest entry data
  if ((newestEntryId == DATABASE_ID_NONE) || (timeLastChanged >= newestTimeLastChanged))
  {
    // update newest info
    if (newestEntryId != DATABASE_ID_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
                               "UPDATE entriesNewest \
                                 SET entryId=%lld,size=%llu,timeLastChanged=%llu \
                                 WHERE id=%llu",
                               entryId,
                               size,
                               timeLastChanged,
                               newestEntryId
                              );
    }
    else
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
                               "INSERT INTO entriesNewest \
                                   ( \
                                    entryId, \
                                    storageId, \
                                    name, \
                                    size, \
                                    timeLastChanged \
                                   ) \
                                 VALUES \
                                   ( \
                                    %lld, \
                                    %lld, \
                                    %'s, \
                                    %llu, \
                                    %llu \
                                   )",
                               entryId,
                               storageId,
                               name,
                               size,
                               timeLastChanged
                              );
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update newest counter/size
    switch (type)
    {
      case INDEX_TYPE_FILE     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest-1, \
                                        totalEntrySizeNewest =totalEntrySizeNewest -%llu, \
                                        totalFileCountNewest =totalFileCountNewest -1, \
                                        totalFileSizeNewest  =totalFileSizeNewest  -%llu \
                                    WHERE id=%llu \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_IMAGE    :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest-1, \
                                        totalEntrySizeNewest =totalEntrySizeNewest -%llu, \
                                        totalImageCountNewest=totalImageCountNewest-1, \
                                        totalImageSizeNewest =totalImageSizeNewest -%llu \
                                    WHERE id=%lld \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                    WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_LINK     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                    WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_HARDLINK :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest   =totalEntryCountNewest   -1, \
                                        totalEntrySizeNewest    =totalEntrySizeNewest    -%llu, \
                                        totalHardlinkCountNewest=totalHardlinkCountNewest-1, \
                                        totalHardlinkSizeNewest =totalHardlinkSizeNewest -%llu \
                                    WHERE id=%lld \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_SPECIAL  :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                    WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    }
    if (error != ERROR_NONE)
    {
      return error;
    }
//if (size > 0) fprintf(stderr,"%s, %d: sub %llu add %llu \n",__FILE__,__LINE__,newestSize,size);
    switch (type)
    {
      case INDEX_TYPE_FILE     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                        totalEntrySizeNewest =totalEntrySizeNewest +%llu, \
                                        totalFileCountNewest =totalFileCountNewest +1, \
                                        totalFileSizeNewest  =totalFileSizeNewest  +%llu \
                                    WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_IMAGE    :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                        totalEntrySizeNewest =totalEntrySizeNewest +%llu, \
                                        totalImageCountNewest=totalImageCountNewest+1, \
                                        totalImageSizeNewest =totalImageSizeNewest +%llu \
                                    WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                    WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      case INDEX_TYPE_LINK     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                    WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      case INDEX_TYPE_HARDLINK :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest   =totalEntryCountNewest   +1, \
                                        totalEntrySizeNewest    =totalEntrySizeNewest    +%llu, \
                                        totalHardlinkCountNewest=totalHardlinkCountNewest+1, \
                                        totalHardlinkSizeNewest =totalHardlinkSizeNewest +%llu \
                                    WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_SPECIAL  :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "UPDATE storage \
                                    SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                    WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    }
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}
#endif

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
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storage",
                             FALSE,
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
                                                            "directories",
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
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
                                                            TRUE,
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO imageEntries \
                                                                                         ( \
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
                                                                                          %llu, \
                                                                                          %d, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnList,"size",0LL),
                                                                                      (int)Database_getTableColumnListInt64(fromColumnList,"fileSystemType",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnList,"blockSize",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnList,"blockOffset",0LL),
                                                                                      (uint64)Database_getTableColumnListInt64(fromColumnList,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              return ERROR_NONE;
                                                            },NULL),
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(userData);

                                                              return Database_execute(&newIndexHandle->databaseHandle,
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      (int)Database_getTableColumnListInt64(fromColumnList,"specialType",0LL),
                                                                                      (uint)Database_getTableColumnListInt64(fromColumnList,"major",0LL),
                                                                                      (uint)Database_getTableColumnListInt64(fromColumnList,"minor",0LL)
                                                                                     );
                                                            },NULL),
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
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"hardlinks");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storage",
                             FALSE,
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
                                                            "directories",
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
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
                                                            TRUE,
                                                            CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                            {
                                                              UNUSED_VARIABLE(fromColumnList);
                                                              UNUSED_VARIABLE(userData);

                                                              (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);
                                                              (void)Database_setTableColumnListInt64(toColumnList,"type",INDEX_TYPE_FILE);

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
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO hardlinkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                     );
                                                            },NULL),
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
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"hardlinks");
  fixBrokenIds(oldIndexHandle,"special");

  error = ERROR_NONE;

  // transfer storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
                             "storage",
                             FALSE,
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
                                                            "directories",
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO imageEntries \
                                                                                         ( \
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
                                                                                          %llu, \
                                                                                          %d, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListInt(fromColumnList,"fileSystemType",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO hardlinkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnList,"specialType",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"major",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"minor",0LL)
                                                                                     );
                                                            },NULL),
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
                                   CASE ceated WHEN 0 THEN 0 ELSE STRFTIME('%%s',created) END \
                            FROM storage \
                            GROUP BY uuid \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %S %llu",
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

fprintf(stderr,"%s, %d: vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n",__FILE__,__LINE__);
  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer entities with storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,
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
                             // post: transfer storages
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors  error;
                               IndexId fromEntityId;
                               IndexId toEntityId;

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListInt64(fromColumnList,"id",DATABASE_ID_NONE);
                               toEntityId   = Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE);

                               // transfer storages of entity
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
                                                          "storage",
                                                          FALSE,
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(userData);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
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

                                                            error = ERROR_NONE;

                                                            // Note: first directories to update totalEntryCount/totalEntrySize
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "directories",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
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
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: c\n",__FILE__,__LINE__); exit(12); }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "files",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO fileEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       size, \
                                                                                                                       fragmentOffset, \
                                                                                                                       fragmentSize \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: a %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "images",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO imageEntries \
                                                                                                                      ( \
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
                                                                                                                       %llu, \
                                                                                                                       %d, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"fileSystemType",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: b %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "links",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO linkEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       destinationName \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: d %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "hardlinks",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO hardlinkEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       size, \
                                                                                                                       fragmentOffset, \
                                                                                                                       fragmentSize \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: e %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "special",
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO specialEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       specialType, \
                                                                                                                       major, \
                                                                                                                       minor \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %d, \
                                                                                                                       %u, \
                                                                                                                       %u \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }
if (error != ERROR_NONE) { fprintf(stderr,"%s, %d: f %s\n",__FILE__,__LINE__,Error_getText(error)); exit(12); }

                                                            return error;
                                                          },NULL),
                                                          "WHERE entityId=%lld",
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
                             "storage",
                             FALSE,
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
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO imageEntries \
                                                                                         ( \
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
                                                                                          %llu, \
                                                                                          %d, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListInt(fromColumnList,"fileSystemType",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO hardlinkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnList,"specialType",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"major",0LL),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"minor",0LL)
                                                                                     );
                                                            },NULL),
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
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");

  // transfer entities with storage and entries
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,
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
                                                          "storage",
                                                          FALSE,
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
                                                                                         "entries",
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
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
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO fileEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       size, \
                                                                                                                       fragmentOffset, \
                                                                                                                       fragmentSize \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
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
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO imageEntries \
                                                                                                                      ( \
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
                                                                                                                       %llu, \
                                                                                                                       %d, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"fileSystemType",0),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                                                  );
                                                                                         },NULL),
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
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO linkEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       destinationName \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %'s \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                                                  );
                                                                                         },NULL),
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
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO hardlinkEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       size, \
                                                                                                                       fragmentOffset, \
                                                                                                                       fragmentSize \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %llu, \
                                                                                                                       %llu, \
                                                                                                                       %llu \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                                                   Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                                                  );
                                                                                         },NULL),
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
                                                                                         TRUE,
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
                                                                                                                   CALLBACK(NULL,NULL),
                                                                                                                   "INSERT INTO specialEntries \
                                                                                                                      ( \
                                                                                                                       entryId, \
                                                                                                                       specialType, \
                                                                                                                       major, \
                                                                                                                       minor \
                                                                                                                      ) \
                                                                                                                    VALUES \
                                                                                                                      ( \
                                                                                                                       %lld, \
                                                                                                                       %d, \
                                                                                                                       %u, \
                                                                                                                       %u \
                                                                                                                      ); \
                                                                                                                   ",
                                                                                                                   Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                                                   Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                                                   Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                                                  );
                                                                                         },NULL),
                                                                                         "WHERE storageId=%lld",
                                                                                         fromStorageId
                                                                                        );
                                                            }

                                                            return error;
                                                          },NULL),
                                                          "WHERE entityId=%lld",
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
                             "storage",
                             FALSE,
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
                                                            "entries",
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO fileEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO imageEntries \
                                                                                         ( \
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
                                                                                          %llu, \
                                                                                          %d, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListInt(fromColumnList,"fileSystemType",0),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockSize",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"blockCount",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO linkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          destinationName \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %'s \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListCString(fromColumnList,"destinationName",NULL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO hardlinkEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          size, \
                                                                                          fragmentOffset, \
                                                                                          fragmentSize \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %llu, \
                                                                                          %llu, \
                                                                                          %llu \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"size",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentOffset",0LL),
                                                                                      Database_getTableColumnListUInt64(fromColumnList,"fragmentSize",0LL)
                                                                                     );
                                                            },NULL),
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
                                                            TRUE,
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
                                                                                      CALLBACK(NULL,NULL),
                                                                                      "INSERT INTO specialEntries \
                                                                                         ( \
                                                                                          entryId, \
                                                                                          specialType, \
                                                                                          major, \
                                                                                          minor \
                                                                                         ) \
                                                                                       VALUES \
                                                                                         ( \
                                                                                          %lld, \
                                                                                          %d, \
                                                                                          %u, \
                                                                                          %u \
                                                                                         ); \
                                                                                      ",
                                                                                      Database_getTableColumnListInt64(toColumnList,"id",DATABASE_ID_NONE),
                                                                                      Database_getTableColumnListInt(fromColumnList,"specialType",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"major",0),
                                                                                      Database_getTableColumnListUInt(fromColumnList,"minor",0)
                                                                                     );
                                                            },NULL),
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
  error = openIndex(&oldIndexHandle,String_cString(oldDatabaseFileName),INDEX_OPEN_MODE_READ,0);
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
                  "Deleted incomplete index #%lld: '%s'\n",
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
  IndexId          entryId;
  IndexId          indexId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // initialize variables
  storageName = String_new();

  n = 0L;

  // clean-up
  error = Index_beginTransaction(indexHandle,"purge orphaned entries");
  if (error != ERROR_NONE)
  {
    String_delete(storageName);
    return error;
  }
  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // storageIds
                                0,  // storageIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                INDEX_TYPE_SET_ANY,
                                NULL,  // entryPattern,
                                FALSE,  // newestEntriesOnly
                                0LL,
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    Index_rollbackTransaction(indexHandle,"purge orphaned entries");
    String_delete(storageName);
    return error;
  }
  while (   !quitFlag
         && Index_getNextEntry(&indexQueryHandle,
                               &entryId,
                               storageName,
                               NULL,  // storageDateTime,
                               NULL,  // name,
                               NULL,  // destinationName,
                               NULL,  // fileSystemType,
                               NULL,  // size,
                               NULL,  // timeModified,
                               NULL,  // userId,
                               NULL,  // groupId,
                               NULL,  // permission,
                               NULL,  // fragmentOrBlockOffset,
                               NULL   // fragmentOrBlockSize
                              )
        )
  {
    if (String_isEmpty(storageName))
    {
//fprintf(stderr,"%s, %d: entryId=%ld\n",__FILE__,__LINE__,entryId);
      Index_deleteEntry(indexHandle,entryId);
      n++;
    }
  }
  Index_doneList(&indexQueryHandle);
  Index_endTransaction(indexHandle,"purge orphaned entries");

  if (n > 0L) plogMessage(NULL,  // logHandle
                          LOG_TYPE_INDEX,
                          "INDEX",
                          "Cleaned %lu orphaned entries\n",
                          n
                         );


  // free resources
  String_delete(storageName);

  return error;
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
                                 NULL,  // storageIds
                                 0,  // storageIdCount,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // jobUUID
                                NULL,  // schedule UUID
                                NULL,  // uuidId
                                NULL,  // entityId
                                NULL,  // archive type
                                &storageId,
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
                                   CASE created WHEN 0 THEN 0 ELSE STRFTIME('%%s',created) END \
                            FROM storage \
                            WHERE entityId=0 \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %llu",
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
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO
#if 0
LOCAL Errors rebuildNewestInfo(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entryId;
  String           name;
  uint64           size;
  uint64           timeModified;


  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // storageIds
                                0,  // storageIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                INDEX_TYPE_SET_ANY,
                                NULL,  // entryPattern,
                                FALSE,  // newestEntriesOnly
                                0LL,
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  name = String_new();
  while (Index_getNextEntry(&indexQueryHandle,
                            &entryId,
                            NULL,  // storageName,
                            NULL,  // storageDateTime,
                            name,
                            NULL,  // destinationName,
                            NULL,  // fileSystemType,
                            &size,
                            &timeModified,
                            NULL,  // userId,
                            NULL,  // groupId,
                            NULL,  // permission,
                            NULL,  // fragmentOrBlockOffset,
                            NULL   // fragmentOrBlockSize
                           )
        )
  {
fprintf(stderr,"%s, %d: %llu\n",__FILE__,__LINE__,entryId);
  }
  String_delete(name);
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}
#endif

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
  ulong            i;
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
                                   NULL,  // storageIds
                                   0,  // storageIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      break;
    }
    i = 0L;
    while (   !quitFlag
           && !deletedIndexFlag
           && Index_getNextStorage(&indexQueryHandle1,
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // uuidId
                                   NULL,  // entityId
                                   NULL,  // archiveType
                                   &storageId,
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
                                     NULL,  // storageIds
                                     0,  // storageIdCount
                                     INDEX_STATE_SET_ALL,
                                     INDEX_MODE_SET_ALL,
                                     NULL,  // name
                                     i,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        continue;
      }
      while (   !quitFlag
             && !deletedIndexFlag
             && Index_getNextStorage(&indexQueryHandle2,
                                     NULL,  // jobUUID
                                     NULL,  // scheduleUUID
                                     NULL,  // uuidId
                                     NULL,  // entityId
                                     NULL,  // archiveType
                                     &duplicateStorageId,
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

      i++;
    }
    Index_doneList(&indexQueryHandle1);
  }
  while (!quitFlag && deletedIndexFlag);
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

LOCAL void cleanupIndexThreadCode(void)
{
  IndexHandle         indexHandle;
  String              absoluteFileName;
  String              pathName;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              oldDatabaseFileName;
  uint                oldDatabaseCount;
  uint                sleepTime;

  assert(__databaseFileName != NULL);

  // open index
  error = openIndex(&indexHandle,__databaseFileName,INDEX_OPEN_MODE_READ_WRITE,WAIT_FOREVER);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot open index database '$s' fail: %s\n",
                __databaseFileName,
                Error_getText(error)
               );
    return;
  }

  // get absolute database file name
  absoluteFileName = File_getAbsoluteFileNameCString(String_new(),__databaseFileName);

  // open directory where database is located
  pathName = File_getFilePathName(String_new(),absoluteFileName);
  error = File_openDirectoryList(&directoryListHandle,pathName);
  if (error != ERROR_NONE)
  {
    String_delete(pathName);
    closeIndex(&indexHandle);
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Upgrade index database '%s' fail: %s\n",
                __databaseFileName,
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

      error = importIndex(&indexHandle,oldDatabaseFileName);
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
      if (&indexHandle.upgradeError == ERROR_NONE)
      {
        indexHandle.upgradeError = error;
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
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Started initial clean-up index database\n"
             );
  (void)cleanUpDuplicateMeta(&indexHandle);
  (void)cleanUpIncompleteUpdate(&indexHandle);
  (void)cleanUpIncompleteCreate(&indexHandle);
  (void)cleanUpStorageNoName(&indexHandle);
  (void)cleanUpStorageNoEntity(&indexHandle);
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Done initial clean-up index database\n"
             );

  // regular clean-ups
  while (!quitFlag)
  {
    // clean-up database
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Started regular clean-up index database\n"
               );
    (void)cleanUpOrphanedEntries(&indexHandle);
    (void)cleanUpDuplicateIndizes(&indexHandle);
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done regular clean-up index database\n"
               );

    // sleep, check quit flag
    sleepTime = 0;
    while ((sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD) && !quitFlag)
    {
      Misc_udelay(10LL*MISC_US_PER_SECOND);
      sleepTime += 10;
    }
  }

  // free resources
  closeIndex(&indexHandle);
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

  indexQueryHandle->indexHandle = indexHandle;
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
* Name   : getIndexTypeSetString
* Purpose: get index type filter string
* Input  : string       - string variable
*          indexTypeSet - index type set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexTypeSetString(String string, IndexTypeSet indexTypeSet)
{
  IndexTypes indexType;

  String_clear(string);
  for (indexType = INDEX_TYPE_MIN; indexType <= INDEX_TYPE_MAX; indexType++)
  {
    if (IN_SET(indexTypeSet,indexType))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexType);
    }
  }

  return string;
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
  IndexStates indexState;

  String_clear(string);
  for (indexState = INDEX_STATE_MIN; indexState <= INDEX_STATE_MAX; indexState++)
  {
    if (IN_SET(indexStateSet,indexState))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexState);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getIndexModeSetString
* Purpose: get index mode filter string
* Input  : string       - string variable
*          indexModeSet - index mode set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexModeSetString(String string, IndexModeSet indexModeSet)
{
  IndexModes indexMode;

  String_clear(string);
  for (indexMode = INDEX_MODE_MIN; indexMode <= INDEX_MODE_MAX; indexMode++)
  {
    if (IN_SET(indexModeSet,indexMode))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexMode);
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
                                 NULL,  // storageIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              NULL,  // jobUUID
                              NULL,  // scheduleUUID
                              NULL,  // uuidId
                              NULL,  // entityId
                              NULL,  // archiveType
                              &storageId,
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
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // uuidId,
                             &entityId,
                             NULL,  // archiveType,
                             NULL,  // createdDateTime,
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount,
                             NULL  // totalEntrySize,
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
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
*          toEntityId    - to entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle  *indexHandle,
                                  IndexId      entityId,
                                  ArchiveTypes toArchiveType,
                                  IndexId      toEntityId
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
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             NULL,  // uuidId,
                             &entityId,
                             NULL,  // archiveType,
                             NULL,  // createdDateTime,
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount,
                             NULL  // totalEntrySize,
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
  Errors error;

  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    return error;
  }

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

Errors Index_init(const char *fileName)
{
  Errors error;
  int64  indexVersion;
  String oldDatabaseFileName;
  uint   n;

  assert(fileName != NULL);

  // get database file name
  __databaseFileName = strdup(fileName);
  if (__databaseFileName == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // check if index exists
  if (File_existsCString(__databaseFileName))
  {
    // get index version
    error = getIndexVersion(__databaseFileName,&indexVersion);
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
        oldDatabaseFileName = String_newCString(__databaseFileName);
        String_appendCString(oldDatabaseFileName,".old");
        String_format(oldDatabaseFileName,"%03d",n);
        n++;
      }
      while (File_exists(oldDatabaseFileName));
      (void)File_renameCString(__databaseFileName,
                               String_cString(oldDatabaseFileName),
                               NULL
                              );
      String_delete(oldDatabaseFileName);

      // create new index
      error = createIndex(__databaseFileName);
      if (error != ERROR_NONE)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Create new index database '$s' fail: %s\n",
                    __databaseFileName,
                    Error_getText(error)
                   );
        return error;
      }
    }
  }
  else
  {
    error = createIndex(__databaseFileName);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create index database '$s' fail: %s\n",
                  __databaseFileName,
                  Error_getText(error)
                 );
      return error;
    }
  }

  // start clean-up thread
  quitFlag = FALSE;
  if (!Thread_init(&cleanupIndexThread,"Index clean-up",0,cleanupIndexThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize index clean-up thread!");
  }

  return ERROR_NONE;
}

void Index_done(void)
{
  // stop threads
  quitFlag = TRUE;
  Thread_join(&cleanupIndexThread);

  // free resources
  free((char*)__databaseFileName);
}

#ifdef NDEBUG
IndexHandle *Index_open(long timeout);
#else /* not NDEBUG */
IndexHandle *__Index_open(const char *__fileName__,
                          uint       __lineNb__,
                          long       timeout
                         )
#endif /* NDEBUG */
{
  IndexHandle *indexHandle;
  Errors error;

  indexHandle = NULL;

  if (Index_isAvailable())
  {
    indexHandle = (IndexHandle*)malloc(sizeof(IndexHandle));
    if (indexHandle == NULL)
    {
      return NULL;
    }

    error = openIndex(indexHandle,
                      __databaseFileName,
                      DATABASE_OPENMODE_READWRITE,
                      timeout
                     );
    if (error != ERROR_NONE)
    {
      free(indexHandle);
      return NULL;
    }
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(indexHandle,sizeof(IndexHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,indexHandle,sizeof(IndexHandle));
  #endif /* NDEBUG */

  return indexHandle;
}

void Index_close(IndexHandle *indexHandle)
{
  if (indexHandle != NULL)
  {
    #ifdef NDEBUG
      assert(pthread_self(),pthread_equals(indexHandle->threadId));;
    #endif /* NDEBUG */

    DEBUG_REMOVE_RESOURCE_TRACE(indexHandle,sizeof(IndexHandle));

    closeIndex(indexHandle);
    free(indexHandle);
  }
}

Errors Index_beginTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  return Database_beginTransaction(&indexHandle->databaseHandle,name);
}

Errors Index_endTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  return Database_endTransaction(&indexHandle->databaseHandle,name);
}

Errors Index_rollbackTransaction(IndexHandle *indexHandle, const char *name)
{
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

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
                         ArchiveTypes *archiveType,
                         uint64       *createdDateTime,
                         String       lastErrorMessage,
                         ulong        *totalEntryCount,
                         uint64       *totalEntrySize
                        )
{
  String              filter;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

//TODO
UNUSED_VARIABLE(jobUUID);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // get filter
  filter = String_newCString("1");
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","jobUUID=%'S",scheduleUUID);
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","scheduleUUID=%'S",scheduleUUID);

//TODO get errorMessage
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT uuids.id, \
                                     entities.id, \
                                     CASE entities.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',entities.created) END, \
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
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %lld %lld %d %S %lu %llu",
                                 &uuidId_,
                                 &entityId_,
                                 createdDateTime,
                                 archiveType,
                                 lastErrorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(filter);
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);

  // free resources
  String_delete(filter);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT uuids.id, \
                                     entities.id, \
                                     entities.jobUUID, \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     storage.state, \
                                     CASE storage.lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.lastChecked) END \
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
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %lld %S %S %S %d %llu",
                                 &uuidId_,
                                 &entityId_,
                                 jobUUID,
                                 scheduleUUID,
                                 storageName,
                                 indexState,
                                 lastCheckedDateTime
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);

  return result;
}

bool Index_findByStorageName(IndexHandle            *indexHandle,
                             const StorageSpecifier *storageSpecifier,
                             ConstString            archiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 scheduleUUID,
                             IndexId                *storageId,
                             IndexStates            *indexState,
                             uint64                 *lastCheckedDateTime
                            )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  String              tmpStorageName;
  StorageSpecifier    tmpStorageSpecifier;
  bool                foundFlag;
  DatabaseId          uuidId_,entityId_,storageId_;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(storageId != NULL);

  (*storageId) = INDEX_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT uuids.id, \
                                     entities.id, \
                                     storage.id, \
                                     entities.jobUUID, \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     storage.state, \
                                     CASE storage.lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.lastChecked) END \
                              FROM storage \
                                LEFT JOIN entities ON storage.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              GROUP BY storage.id \
                             "
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    tmpStorageName = String_new();
    Storage_initSpecifier(&tmpStorageSpecifier);
    foundFlag   = FALSE;
    while (   !foundFlag
           && Database_getNextRow(&databaseQueryHandle,
                                  "%lld %lld %lld %S %S %S %d %llu",
                                  &uuidId_,
                                  &entityId_,
                                  &storageId_,
                                  jobUUID,
                                  scheduleUUID,
                                  tmpStorageName,
                                  indexState,
                                  lastCheckedDateTime
                                 )
          )
    {
      if (Storage_parseName(&tmpStorageSpecifier,tmpStorageName) == ERROR_NONE)
      {
        foundFlag = Storage_equalSpecifiers(storageSpecifier,archiveName,
                                            &tmpStorageSpecifier,NULL
                                           );
        if (foundFlag)
        {
          if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
          if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
          if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);
        }
      }
    }
    Storage_doneSpecifier(&tmpStorageSpecifier);
    String_delete(tmpStorageName);

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
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

  // init variables
  indexStateSetString = String_new();

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT uuids.id, \
                                     entities.id, \
                                     storage.id, \
                                     entities.jobUUID, \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     CASE storage.lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.lastChecked) END \
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
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %lld %lld %S %S %S %llu",
                                 &uuidId_,
                                 &entityId_,
                                 &storageId_,
                                 jobUUID,
                                 scheduleUUID,
                                 storageName,
                                 lastCheckedDateTime
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
  if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);

  // free resources
  String_delete(indexStateSetString);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT state, \
                                     CASE lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',lastChecked) END, \
                                     errorMessage \
                              FROM storage \
                              WHERE id=%lld \
                             ",
                             INDEX_DATABASE_ID_(storageId)
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
  });

  return error;
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
  String  errorText;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    errorText = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);
  }
  else
  {
    errorText = NULL;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
                               INDEX_DATABASE_ID_(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    if (errorText != NULL)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
                               "UPDATE storage \
                                SET errorMessage=%'S \
                                WHERE id=%lld; \
                               ",
                               errorText,
                               INDEX_DATABASE_ID_(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    return ERROR_NONE;
  });

  // free resources
  if (errorMessage != NULL) String_delete(errorText);

  return error;
}

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  long                count;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  BLOCK_DOX(count,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
  });

  return count;
}

Errors Index_initListHistory(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             ConstString      jobUUID,
                             uint64           offset,
                             uint64           limit
                            )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id, \
                                   jobUUID, \
                                   scheduleUUID, \
                                   CASE lastCreated WHEN 0 THEN 0 ELSE STRFTIME('%%s',lastCreated) END, \
                                   errorMessage, \
                                   totalEntryCount, \
                                   totalEntrySize, \
                                   duration \
                            FROM uuids \
                            LIMIT %llu,%llu \
                           ",
                           offset,
                           limit
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextHistory(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          String           jobUUID,
                          String           scheduleUUID,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          uint64           *duration
                         )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return indexQueryHandle->indexHandle->upgradeError;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %S %llu %S %lu %llu %llu",
                           &databaseId,
                           jobUUID,
                           scheduleUUID,
                           createdDateTime,
                           errorMessage,
                           totalEntryCount,
                           totalEntrySize,
                           duration
                          )
     )
  {
    return FALSE;
  }
  if (historyId != NULL) (*historyId) = INDEX_ID_(INDEX_TYPE_HISTORY,databaseId);

  return ERROR_NONE;
}

Errors Index_newHistory(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  scheduleUUID,
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        ulong        totalEntryCount,
                        uint64       totalEntrySize,
                        uint64       duration,
                        IndexId      *historyId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    return Database_execute(&indexHandle->databaseHandle,
                            CALLBACK(NULL,NULL),
                            "INSERT INTO history \
                               ( \
                                jobUUID, \
                                scheduleUUID, \
                                type, \
                                created, \
                                errorMessage, \
                                totalEntryCount, \
                                totalEntrySize, \
                                duration \
                               ) \
                             VALUES \
                               ( \
                                %'S, \
                                %'S, \
                                %d, \
                                %llu, \
                                %'s, \
                                %lu, \
                                %llu, \
                                %llu \
                               ); \
                            ",
                            jobUUID,
                            scheduleUUID,
                            archiveType,
                            createdDateTime,
                            errorMessage,
                            totalEntryCount,
                            totalEntrySize,
                            duration
                           );
  });
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (historyId != NULL) (*historyId) = INDEX_ID_(INDEX_TYPE_HISTORY,Database_getLastRowId(&indexHandle->databaseHandle));

  return ERROR_NONE;
}

Errors Index_deleteHistory(IndexHandle *indexHandle,
                           IndexId     historyId
                          )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete history entry
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    return Database_execute(&indexHandle->databaseHandle,
                            CALLBACK(NULL,NULL),
                            "DELETE FROM history WHERE id=%lld;",
                            INDEX_DATABASE_ID_(historyId)
                           );
  });

  return error;
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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id, \
                                   jobUUID, \
                                   CASE lastCreated WHEN 0 THEN 0 ELSE STRFTIME('%%s',lastCreated) END, \
                                   lastErrorMessage, \
                                   totalEntryCount, \
                                   totalEntrySize \
                            FROM uuids \
                            LIMIT %llu,%llu \
                           ",
                           offset,
                           limit
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
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
                       ulong            *totalEntryCount,
                       uint64           *totalEntrySize
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %lu %llu",
                           &databaseId,
                           jobUUID,
                           lastCreatedDateTime,
                           lastErrorMessage,
                           totalEntryCount,
                           totalEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,databaseId);

  return TRUE;
}

Errors Index_newUUID(IndexHandle *indexHandle,
                     ConstString jobUUID,
                     IndexId     *uuidId
                    )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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

    return ERROR_NONE;
  });
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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete entities of UUID
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT id \
                              FROM entities \
                              WHERE jobUUID=%'S; \
                             ",
                             jobUUID
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,INDEX_ID_(INDEX_TYPE_ENTITY,entityId));
    }

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get filter
  filter = String_newCString("1");
  filterAppend(filter,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",INDEX_DATABASE_ID_(uuidId));
  filterAppend(filter,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  filterAppend(filter,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id,\
                                   entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   CASE entities.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',entities.created) END, \
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
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         String           jobUUID,
                         String           scheduleUUID,
                         IndexId          *uuidId,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         String           lastErrorMessage,
                         ulong            *totalEntryCount,
                         uint64           *totalEntrySize
                        )
{
  DatabaseId uuidId_,entityId_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %lld %S %S %llu %u %S %lu %llu",
                           &uuidId_,
                           &entityId_,
                           jobUUID,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
                           lastErrorMessage,
                           totalEntryCount,
                           totalEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_ENTITY,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);

  return TRUE;
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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(entityId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return error;
  }

  (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,Database_getLastRowId(&indexHandle->databaseHandle));

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    // delete storage of entity
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT id \
                              FROM storage \
                              WHERE entityId=%lld; \
                             ",
                             entityId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &storageId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteStorage(indexHandle,INDEX_ID_(INDEX_TYPE_STORAGE,storageId));
    }
    Database_finalize(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete entity
    error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),
                               "DELETE FROM entities WHERE id=%lld;",
                               INDEX_DATABASE_ID_(entityId)
                              );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_getStoragesInfo(IndexHandle   *indexHandle,
                             const IndexId indexIds[],
                             uint          indexIdCount,
                             IndexStateSet indexStateSet,
                             IndexModeSet  indexModeSet,
                             ConstString   name,
                             ulong         *storageCount,
                             ulong         *totalEntryCount,
                             uint64        *totalEntrySize
                            )
{
  String              ftsName;
  String              regexpName;
//TODO
//  String              uuidIdsString;
//  String              entityIdsString;
  String              storageIdsString;
  uint                i;
  String              filter,unionFilter,storageFilter;
  String              indexStateSetString;
  String              indexModeSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  double              totalEntryCount_,totalEntrySize_;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((indexIdCount == 0) || (indexIds != NULL));

  // init variables
  if (storageCount != NULL) (*storageCount) = 0L;
  if (totalEntryCount != NULL) (*totalEntryCount) = 0L;
  if (totalEntrySize != NULL) (*totalEntrySize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
//  uuidIdsString    = String_new();
//  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
/*      case INDEX_TYPE_UUID:
        if (i > 0) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (i > 0) String_appendChar(entityIdsString,',');
        String_format(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;*/
      case INDEX_TYPE_STORAGE:
        if (i > 0) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filter              = String_newCString("1");
  indexStateSetString = String_new();
  indexModeSetString  = String_new();

#if 0
  if (   !String_isEmpty(ftsName)
//      || !String_isEmpty(regexpName)
      || (indexIds != NULL)
     )
  {
    storageFilter = String_new();
    if (!String_isEmpty(ftsName) || !String_isEmpty(regexpName) || (indexIds != NULL))
    {
      filterAppend(storageFilter,!String_isEmpty(storageIdsString),"AND","storage.id IN (%S)",storageIdsString);
      filterAppend(storageFilter,!String_isEmpty(ftsName),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
//      filterAppend(storageFilter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,storage.name)",regexpName);
    }

    unionFilter = String_new();
    filterAppend(unionFilter,!String_isEmpty(storageFilter),"UNION ALL","SELECT storage.id FROM storage WHERE %S",storageFilter);
    filterAppend(unionFilter,!String_isEmpty(entityIdsString),"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (%S)",entityIdsString);
    filterAppend(unionFilter,!String_isEmpty(uuidIdsString),"UNION ALL","SELECT storage.id FROM storage WHERE storage.entityId IN (SELECT entities.id FROM entities WHERE entities.jobUUID IN (SELECT entities.jobUUID FROM entities WHERE entities.id IN (%S)))",uuidIdsString);

    filterAppend(filter,TRUE,"AND","storage.id IN (%S)",unionFilter);

    String_delete(storageFilter);
    String_delete(unionFilter);
  }
#endif
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","storage.id IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,storage.name)",regexpName);
  filterAppend(filter,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));
  filterAppend(filter,TRUE,"AND","storage.mode IN (%S)",getIndexModeSetString(indexModeSetString,indexModeSet));

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
  //TODO newest
                             "SELECT COUNT(storage.id),\
                                     TOTAL(storage.totalEntryCount), \
                                     TOTAL(storage.totalEntrySize) \
                              FROM storage \
                              WHERE %s \
                             ",
                             String_cString(filter)
                            );
  //Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle,
                            "%lu %lf %lf",
                            storageCount,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
      if (totalEntrySize != NULL) (*totalEntrySize) = (uint64)totalEntrySize_;
    }
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexModeSetString);
    String_delete(indexStateSetString);
    String_delete(filter);
    String_delete(storageIdsString);
//    String_delete(entityIdsString);
//    String_delete(uuidIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(indexModeSetString);
  String_delete(indexStateSetString);
  String_delete(filter);
  String_delete(storageIdsString);
//  String_delete(entityIdsString);
//  String_delete(uuidIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  return ERROR_NONE;
}

Errors Index_initListStorages(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
                              IndexId          entityId,
                              ConstString      jobUUID,
                              const IndexId    indexIds[],
                              uint             indexIdCount,
                              IndexStateSet    indexStateSet,
                              IndexModeSet     indexModeSet,
                              ConstString      name,
                              uint64           offset,
                              uint64           limit
                             )
{
  String ftsName;
  String regexpName;
//  String uuidIdsString;
//  String entityIdsString;
  String storageIdsString;
  Errors error;
  String filter;
  String indexStateSetString;
  String indexModeSetString;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(entityId) == INDEX_TYPE_UUID));
  assert((entityId == INDEX_ID_ANY) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0) || (indexIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

//TODO required?
  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
//  uuidIdsString    = String_new();
//  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
/*
      case INDEX_TYPE_UUID:
        if (i > 0) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (i > 0) String_appendChar(entityIdsString,',');
        String_format(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;*/
      case INDEX_TYPE_STORAGE:
        if (i > 0) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  // get filter
  filter              = String_newCString("1");
  indexStateSetString = String_new();
  indexModeSetString  = String_new();

  filterAppend(filter,(entityId != INDEX_ID_ANY),"AND","storage.entityId=%lld",INDEX_DATABASE_ID_(entityId));
  filterAppend(filter,!String_isEmpty(jobUUID),"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","storage.id IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,storage.name)",regexpName);
  filterAppend(filter,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));
  filterAppend(filter,TRUE,"AND","storage.mode IN (%S)",getIndexModeSetString(indexModeSetString,indexModeSet));

//Database_debugEnable(1);
  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO newest
                           "SELECT uuids.id, \
                                   entities.id, \
                                   storage.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   storage.totalEntryCount, \
                                   storage.totalEntrySize, \
                                   storage.state, \
                                   storage.mode, \
                                   CASE storage.lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.lastChecked) END, \
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
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(indexModeSetString);
    String_delete(indexStateSetString);
    String_delete(filter);
    String_delete(storageIdsString);
//    String_delete(entityIdsString);
//    String_delete(uuidIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  String_delete(indexModeSetString);
  String_delete(indexStateSetString);
  String_delete(filter);
  String_delete(storageIdsString);
//  String_delete(entityIdsString);
//  String_delete(uuidIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          String           jobUUID,
                          String           scheduleUUID,
                          IndexId          *uuidId,
                          IndexId          *entityId,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *createdDateTime,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage
                         )
{
  DatabaseId uuidId_,entityId_,storageId_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %lld %lld %S %S %d %S %llu %lu %llu %d %d %llu %S",
                           &uuidId_,
                           &entityId_,
                           &storageId_,
                           jobUUID,
                           scheduleUUID,
                           archiveType,
                           storageName,
                           createdDateTime,
                           totalEntryCount,
                           totalEntrySize,
                           indexState,
                           indexMode,
                           lastCheckedDateTime,
                           errorMessage
                          )
    )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_(INDEX_TYPE_UUID,uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
  if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);

  return TRUE;
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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(storageId != NULL);
  assert((entityId == INDEX_ID_NONE) || (INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
  });

  return error;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long global locking of database!
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM fileEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_FILE
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_FILE
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM imageEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_IMAGE
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_IMAGE
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM directoryEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_DIRECTORY
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_DIRECTORY
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM linkEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_LINK
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_LINK
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM hardlinkEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_HARDLINK
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_HARDLINK
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM specialEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_SPECIAL
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_SPECIAL
                            );
    if (error != ERROR_NONE) return error;
  fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM storage WHERE id=%lld;",
                             INDEX_DATABASE_ID_(storageId)
                            );
    if (error != ERROR_NONE) return error;

    return ERROR_NONE;
  });

  return error;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // Note: do in single steps to avoid long-time-locking of database!
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM fileEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_FILE
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_FILE
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM imageEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_IMAGE
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_IMAGE
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM directoryEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_DIRECTORY
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_DIRECTORY
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM linkEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_LINK
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_LINK
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM hardlinkEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_HARDLINK
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_HARDLINK
                            );
    if (error != ERROR_NONE) return error;

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM specialEntries WHERE entryId IN (SELECT id FROM entries WHERE storageId=%lld AND type=%d)",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_SPECIAL
                            );
    if (error != ERROR_NONE) return error;
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE storageId=%lld AND type=%d",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_SPECIAL
                            );
    if (error != ERROR_NONE) return error;

    return ERROR_NONE;
  });

  return error;
}


Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId     storageId,
                        String      storageName,
                        uint64      *createdDateTime,
                        uint64      *size,
                        uint64      *totalEntryCount,
                        uint64      *totalEntrySize,
                        IndexStates *indexState,
                        IndexModes  *indexMode,
                        uint64      *lastCheckedDateTime,
                        String      errorMessage
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT storage.name, \
                                     CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                     storage.size, \
                                     storage.totalEntryCount, \
                                     storage.totalEntrySize, \
                                     storage.state, \
                                     storage.mode, \
                                     CASE storage.lastChecked WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.lastChecked) END, \
                                     storage.errorMessage \
                              FROM storage \
                              WHERE id=%d \
                             ",
                             INDEX_DATABASE_ID_(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (!Database_getNextRow(&databaseQueryHandle,
                             "%S %llu %llu %ld %llu %d %d %llu %S",
                             storageName,
                             createdDateTime,
                             size,
                             totalEntryCount,
                             totalEntrySize,
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
  });

  return error;
}

Errors Index_storageUpdate(IndexHandle *indexHandle,
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      storageSize
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
  });

  return error;
}

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId storageIds[],
                            uint          storageIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            ConstString   name,
                            bool          newestEntriesOnly,
                            ulong         *count,
                            uint64        *size
                           )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              ftsName;
  String              regexpName;
  String              storageIdsString;
  String              entryIdsString;
  uint                i;
  String              filter;
  String              indexTypeSetString;
  double              count_;
  double              size_;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));
  assert(count != NULL);

  // init variables
  if (count != NULL) (*count) = 0L;
  if (size != NULL) (*size) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  error              = ERROR_NONE;
  filter             = String_newCString("1");
  indexTypeSetString = String_new();

//Database_debugEnable(1);
  if (String_isEmpty(ftsName) && (entryIdCount == 0))
  {
    // not pattern/entries selected
    filterAppend(filter,!String_isEmpty(storageIdsString),"AND","id IN (%S)",storageIdsString);

    BLOCK_DOX(error,
              Database_lock(&indexHandle->databaseHandle),
              Database_unlock(&indexHandle->databaseHandle),
    {
      if (IN_SET(indexTypeSet,INDEX_TYPE_FILE))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalFileCountNewest" : "totalFileCount",
                                 newestEntriesOnly ? "totalFileSizeNewest" : "totalFileSize",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf %lf",
                                &count_,
                                &size_
                               )
           )
        {
          assert(count_ >= 0.0);
          assert(size_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_IMAGE))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalImageCountNewest" : "totalImageCount",
                                 newestEntriesOnly ? "totalImageSizeNewest" : "totalImageSize",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf %lf",
                                &count_,
                                &size_
                               )
           )
        {
          assert(count_ >= 0.0);
          assert(size_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_DIRECTORY))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalDirectoryCountNewest" : "totalDirectoryCount",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf",
                                &count_
                               )
           )
        {
          assert(count_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
        }
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_LINK))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalLinkCountNewest" : "totalLinkCount",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf",
                                &count_
                               )
           )
        {
          assert(count_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
        }
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_HARDLINK))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalHardlinkCountNewest" : "totalHardlinkCount",
                                 newestEntriesOnly ? "totalHardlinkSizeNewest" : "totalHardlinkSize",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf %lf",
                                &count_,
                                &size_
                               )
           )
        {
          assert(count_ >= 0.0);
          assert(size_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
          if (size != NULL) (*size) += (uint64)size_;
        }
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_SPECIAL))
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                    FROM storage \
                                    WHERE %S \
                                 ",
                                 newestEntriesOnly ? "totalSpecialCountNewest" : "totalSpecialCount",
                                 filter
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (Database_getNextRow(&databaseQueryHandle,
                                "%lf",
                                &count_
                               )
           )
        {
          assert(count_ >= 0.0);
          if (count != NULL) (*count) += (uint64)count_;
        }
        Database_finalize(&databaseQueryHandle);
      }

      return ERROR_NONE;
    });
  }
  else
  {
    // get filter
    filterAppend(filter,!String_isEmpty(storageIdsString),"AND","storageId IN (%S)",storageIdsString);
    filterAppend(filter,!String_isEmpty(ftsName),"AND","%s IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",newestEntriesOnly ? "entryId" : "id",ftsName);
//    filterAppend(filter,!String_isEmpty(pattern),"AND","REGEXP(%S,0,entries.name)",regexpString);
    filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

    BLOCK_DOX(error,
              Database_lock(&indexHandle->databaseHandle),
              Database_unlock(&indexHandle->databaseHandle),
    {
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(id),TOTAL(size) \
                                  FROM %s \
                                  WHERE     %S \
                                        AND type IN (%S) \
                               ",
                               newestEntriesOnly ? "entriesNewest" : "entries",
                               filter,
                               getIndexTypeSetString(indexTypeSetString,indexTypeSet)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      if (Database_getNextRow(&databaseQueryHandle,
                              "%lf %lf",
                              &count_,
                              &size_
                             )
         )
      {
        assert(count_ >= 0.0);
        assert(size_ >= 0.0);
        if (count != NULL) (*count) += (uint64)count_;
        if (size != NULL) (*size) += (uint64)size_;
      }
      Database_finalize(&databaseQueryHandle);

      return ERROR_NONE;
    });
  }
//Database_debugEnable(0);

  // free resources
  String_delete(indexTypeSetString);
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  return error;
}

Errors Index_initListEntries(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             IndexTypeSet     indexTypeSet,
                             ConstString      name,
                             bool             newestEntriesOnly,
                             uint64           offset,
                             uint64           limit
                            )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  String indexTypeSetString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  // get filter
  indexTypeSetString = String_new();
  filter = String_newCString("1");
  if (newestEntriesOnly)
  {
    // get filter
    filterAppend(filter,!String_isEmpty(ftsName),"AND","entriesNewest.entryId IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//    filterAppend(filter,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,entries.name)",regexpString);
    filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entriesNewest.storageId IN (%S)",storageIdsString);
    filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    filterAppend(filter,TRUE,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
  }
  else
  {
    // get filter
    filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//    filterAppend(filter,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,entries.name)",regexpString);
    filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
    filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    filterAppend(filter,TRUE,"AND","entries.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
  }

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  if (newestEntriesOnly)
  {
    error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT entriesNewest.entryId, \
                                     storage.name, \
                                     CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                     entriesNewest.type, \
                                     entriesNewest.name, \
                                     entriesNewest.timeLastChanged, \
                                     entriesNewest.userId, \
                                     entriesNewest.groupId, \
                                     entriesNewest.permission, \
                                     fileEntries.size, \
                                     fileEntries.fragmentOffset, \
                                     fileEntries.fragmentSize, \
                                     imageEntries.size, \
                                     imageEntries.fileSystemType, \
                                     imageEntries.blockSize, \
                                     imageEntries.blockOffset, \
                                     imageEntries.blockCount, \
                                     linkEntries.destinationName, \
                                     hardlinkEntries.size \
                              FROM entriesNewest \
                                LEFT JOIN storage ON storage.id=entriesNewest.storageId \
                                LEFT JOIN fileEntries ON fileEntries.entryId=entriesNewest.entryId \
                                LEFT JOIN imageEntries ON imageEntries.entryId=entriesNewest.entryId \
                                LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.entryId \
                              WHERE %S \
                              ORDER BY entriesNewest.name \
                              LIMIT %llu,%llu; \
                             ",
                             filter,
                             offset,
                             limit
                            );
  }
  else
  {
    error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT entries.id, \
                                     storage.name, \
                                     CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                     entries.type, \
                                     entries.name, \
                                     entries.timeLastChanged, \
                                     entries.userId, \
                                     entries.groupId, \
                                     entries.permission, \
                                     fileEntries.size, \
                                     fileEntries.fragmentOffset, \
                                     fileEntries.fragmentSize, \
                                     imageEntries.size, \
                                     imageEntries.fileSystemType, \
                                     imageEntries.blockSize, \
                                     imageEntries.blockOffset, \
                                     imageEntries.blockCount, \
                                     linkEntries.destinationName, \
                                     hardlinkEntries.size \
                              FROM entries \
                                LEFT JOIN storage ON storage.id=entries.storageId \
                                LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                                LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                                LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                                LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                              WHERE %S \
                              ORDER BY entries.name \
                              LIMIT %llu,%llu; \
                             ",
                             filter,
                             offset,
                             limit
                            );
  }

  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextEntry(IndexQueryHandle  *indexQueryHandle,
                        IndexId           *entryId,
                        String            storageName,
                        uint64            *storageDateTime,
                        String            entryName,
                        String            destinationName,
                        FileSystemTypes   *fileSystemType,
                        uint64            *size,
                        uint64            *timeModified,
                        uint32            *userId,
                        uint32            *groupId,
                        uint32            *permission,
                        uint64            *fragmentOffsetOrBlockOffset,
                        uint64            *fragmentSizeOrBlockCount
                       )
{
  IndexTypes indexType;
  DatabaseId databaseId;
  uint64     fileSize_,imageSize_,hardlinkSize_;
  uint64     fragmentOffset_,fragmentSize_;
  uint64     blockOffset_,blockCount_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %d %S %llu %d %d %d %llu %llu %llu %llu %d %d %llu %llu %S",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           &indexType,
                           entryName,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           &fileSize_,
                           &fragmentOffset_,
                           &fragmentSize_,
                           &imageSize_,
                           fileSystemType,
                           NULL,  // imageEntryBlockSize,
                           &blockOffset_,
                           &blockCount_,
                           destinationName,
                           &hardlinkSize_
                          )
     )
  {
    return FALSE;
  }
  assert(fileSize_ >= 0.0);
  assert(fragmentOffset_ >= 0.0);
  assert(fragmentSize_ >= 0.0);
  assert(blockOffset_ >= 0.0);
  assert(blockCount_ >= 0.0);
  assert(hardlinkSize_ >= 0.0);
  if (entryId != NULL) (*entryId) = INDEX_ID_(indexType,databaseId);
  if (size != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:     (*size) = fileSize_;     break;
      case INDEX_TYPE_IMAGE:    (*size) = imageSize_;    break;
      case INDEX_TYPE_HARDLINK: (*size) = hardlinkSize_; break;
      default:                  (*size) = 0LL;           break;
    }
  }
  if (fragmentOffsetOrBlockOffset != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentOffsetOrBlockOffset) = fragmentOffset_; break;
      case INDEX_TYPE_IMAGE: (*fragmentOffsetOrBlockOffset) = blockOffset_;    break;
      default:               (*fragmentOffsetOrBlockOffset) = 0LL;             break;
    }
  }
  if (fragmentSizeOrBlockCount != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentSizeOrBlockCount) = fragmentSize_; break;
      case INDEX_TYPE_IMAGE: (*fragmentSizeOrBlockCount) = blockCount_;   break;
      default:               (*fragmentSizeOrBlockCount) = 0LL;           break;
    }
  }

  return TRUE;
}

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(   (INDEX_TYPE_(entryId) == INDEX_TYPE_FILE)
         || (INDEX_TYPE_(entryId) == INDEX_TYPE_IMAGE)
         || (INDEX_TYPE_(entryId) == INDEX_TYPE_DIRECTORY)
         || (INDEX_TYPE_(entryId) == INDEX_TYPE_LINK)
         || (INDEX_TYPE_(entryId) == INDEX_TYPE_HARDLINK)
         || (INDEX_TYPE_(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM fileEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM imageEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM directoryEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      case INDEX_TYPE_LINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM linkEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),
                                 "DELETE FROM specialEntries WHERE entryId=%lld;",
                                 INDEX_DATABASE_ID_(entryId)
                                );
        break;
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(entryId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_FILE);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   entries.size, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission, \
                                   fileEntries.fragmentOffset, \
                                   fileEntries.fragmentSize \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=entries.storageId \
                              LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_FILE,databaseId);

  return TRUE;
}

Errors Index_deleteFile(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_FILE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM fileEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    storageIds[],
                            uint             storageIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      name
                           )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   imageEntries.fileSystemType, \
                                   entries.size, \
                                   imageEntries.blockOffset, \
                                   imageEntries.blockCount \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=images.storageId \
                              LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_IMAGE,databaseId);

  return TRUE;
}

Errors Index_deleteImage(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_IMAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM imageEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    storageIds[],
                                 uint             storageIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
//Database_debugEnable(1);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=entries.storageId \
                              LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_DIRECTORY,databaseId);

  return TRUE;
}

Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             IndexId     indexId
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_DIRECTORY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM directoryEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }
  String_appendCString(entryIdsString,"))");

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   linkEntries.destinationName, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=links.storageId \
                              LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_LINK,databaseId);

  return TRUE;
}

Errors Index_deleteLink(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_LINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM linkEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    storageIds[],
                               uint             storageIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      name
                              )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   entries.size, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission, \
                                   hardlinkEntries.fragmentOffset, \
                                   hardlinkEntries.fragmentSize \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=hardlinks.storageId \
                              LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_HARDLINK,databaseId);

  return TRUE;
}

Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            IndexId     indexId
                           )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_HARDLINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      name
                            )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filter;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  // get FTS/regex patterns
  ftsName    = getFTSString   (String_new(),name);
  regexpName = getREGEXPString(String_new(),name);

  // get id sets
  storageIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filter
  filter = String_new();
  filterAppend(filter,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filter,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filter,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,special.name)",regexpString);
  filterAppend(filter,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filter,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   CASE storage.created WHEN 0 THEN 0 ELSE STRFTIME('%%s',storage.created) END, \
                                   entries.name, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=special.storageId \
                              LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                            WHERE %s \
                           ",
                           String_cString(filter)
                          );
  if (error != ERROR_NONE)
  {
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filter);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    doneIndexQueryHandle(indexQueryHandle);
    return error;
  }

  // free resources
  String_delete(filter);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexQueryHandle->indexHandle->threadId));;
  #endif /* NDEBUG */

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
  if (indexId != NULL) (*indexId) = INDEX_ID_(INDEX_TYPE_SPECIAL,databaseId);

  return TRUE;
}

Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           IndexId     indexId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(indexId) == INDEX_TYPE_SPECIAL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM specialEntries WHERE entryId=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entries WHERE id=%lld;",
                             INDEX_DATABASE_ID_(indexId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  Database_finalize(&indexQueryHandle->databaseQueryHandle);
  Database_unlock(&indexQueryHandle->indexHandle->databaseHandle);
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
  #define TRANSACTION_NAME "ADD_FILE"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    // start transaction
#if 0
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add file entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_FILE,
                             fileName,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission
                            );
    if (error != ERROR_NONE)
    {
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO fileEntries \
                                ( \
                                 entryId, \
                                 size, \
                                 fragmentOffset, \
                                 fragmentSize\
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %llu, \
                                 %llu, \
                                 %llu\
                                ); \
                             ",
                             entryId,
                             size,
                             fragmentOffset,
                             fragmentSize
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

    // end transaction
#if 0
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
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
  #define TRANSACTION_NAME "ADD_IMAGE"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(imageName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    // start transaction
#if 0
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add image entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_IMAGE,
                             imageName,
                             0LL,
                             0LL,
                             0LL,
                             0,
                             0,
                             0
                            );
    if (error != ERROR_NONE)
    {
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO imageEntries \
                                ( \
                                 entryId, \
                                 size, \
                                 fragmentOffset, \
                                 fragmentSize\
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %d, \
                                 %llu, \
                                 %u, \
                                 %llu, \
                                 %llu\
                                ); \
                             ",
                             entryId,
                             fileSystemType,
                             size,
                             blockSize,
                             blockOffset,
                             blockCount
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

#if 0
    // end transaction
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
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
  #define TRANSACTION_NAME "ADD_DIRECTORY"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(directoryName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
#if 0
    // start transaction
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add directory entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_DIRECTORY,
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
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO directoryEntries \
                                ( \
                                 entryId, \
                                 storageId, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %lld, \
                                 %'S\
                                ); \
                             ",
                             entryId,
                             INDEX_DATABASE_ID_(storageId),
                             directoryName
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

#if 0
    // end transaction
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
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
  #define TRANSACTION_NAME "ADD_LINK"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
#if 0
    // start transaction
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add link entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_LINK,
                             linkName,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission
                            );
    if (error != ERROR_NONE)
    {
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO linkEntries \
                                ( \
                                 entryId, \
                                 destinationName, \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %'S \
                                ); \
                             ",
                             entryId,
                             destinationName
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

#if 0
    // end transaction
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
}

Errors Index_addHardlink(IndexHandle *indexHandle,
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
  #define TRANSACTION_NAME "ADD_HARDLINK"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
#if 0
    // start transaction
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add hard link entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_HARDLINK,
                             fileName,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission
                            );
    if (error != ERROR_NONE)
    {
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO hardlinkEntries \
                                ( \
                                 entryId, \
                                 size, \
                                 fragmentOffset, \
                                 fragmentSize\
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %llu, \
                                 %llu, \
                                 %llu\
                                ); \
                             ",
                             entryId,
                             size,
                             fragmentOffset,
                             fragmentSize
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

#if 0
    // end transaction
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
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
  #define TRANSACTION_NAME "ADD_SPECIAL"

  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
#if 0
    // start transaction
    error = Database_beginTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    // add special entry
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO entries \
                                ( \
                                 storageId, \
                                 type, \
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
                                 %lld, \
                                 %d, \
                                 %'S, \
                                 %llu, \
                                 %llu, \
                                 %llu, \
                                 %u, \
                                 %u, \
                                 %u \
                                ); \
                             ",
                             INDEX_DATABASE_ID_(storageId),
                             INDEX_TYPE_SPECIAL,
                             name,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission
                            );
    if (error != ERROR_NONE)
    {
//      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }
    entryId = Database_getLastRowId(&indexHandle->databaseHandle);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "INSERT INTO specialEntries \
                                ( \
                                 entryId, \
                                 specialType, \
                                 major, \
                                 minor \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %d, \
                                 %d, \
                                 %d\
                                ); \
                             ",
                             entryId,
                             specialType,
                             major,
                             minor
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_rollbackTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
      return error;
    }

#if 0
    // end transaction
    error = Database_endTransaction(&indexHandle->databaseHandle,TRANSACTION_NAME);
    if (error != ERROR_NONE)
    {
      return error;
    }
#endif

    return ERROR_NONE;
  });

  return error;

  #undef TRANSACTION_NAME
}

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
                     )
{
  Errors error;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */

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
  Errors  error;
  bool    existsFlag;
  uint    i;
  IndexId id;

  assert(indexHandle != NULL);
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(storageId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
  });

  return error;
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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(uuidId) == INDEX_TYPE_UUID);

fprintf(stderr,"%s, %d: try prune uuid %llu\n",__FILE__,__LINE__,uuidId);

  // prune entities of uuid
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
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
                               NULL,  // jobUUID,
                               NULL,  // scheduleUUID,
                               NULL,  // uuidId,
                               &entityId,
                               NULL,  // archiveType,
                               NULL,  // createdDateTime,
                               NULL,  // lastErrorMessage
                               NULL,  // totalEntryCount,
                               NULL  // totalEntrySize,
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

    // prune uuid if empty
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
  });

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
  #ifdef NDEBUG
    assert(pthread_self(),pthread_equals(indexHandle->threadId));;
  #endif /* NDEBUG */
  assert(INDEX_TYPE_(entityId) == INDEX_TYPE_ENTITY);

fprintf(stderr,"%s, %d: try prune entiry %llu\n",__FILE__,__LINE__,entityId);

  // prune storage of entity
  BLOCK_DOX(error,
            Database_lock(&indexHandle->databaseHandle),
            Database_unlock(&indexHandle->databaseHandle),
  {
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,   // storageIds
                                   0,  // storageIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   0LL,   // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // jobUUID
                                NULL,  // scheduleUUID
                                NULL,  // uuidId
                                NULL,  // entityId
                                NULL,  // archiveType
                                &storageId,
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
  });

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
