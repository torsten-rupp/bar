/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index entry functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "errors.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

#include "index/index.h"
#include "index/index_common.h"

#include "index/index_entries.h"

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
* Name   : purgeEntry
* Purpose: purge entry
* Input  : indexHandle - index handle
*          entryId     - entry database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: use
#if 0
LOCAL Errors purgeEntry(IndexHandle *indexHandle,
                        DatabaseId  entryId
                       )
{
  Errors              error;
//  DatabaseStatementHandle databaseStatementHandle;
//  uint64              createdDateTime;
//  bool                transactionFlag;
//  bool                doneFlag;
  #ifndef NDEBUG
//    ulong               deletedCounter;
  #endif

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);

  // init variables

//    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
//                                      indexHandle,
//                                      transactionFlag,
//    {

  error = ERROR_NONE;

  // purge FTS entry
  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                             NULL,  // changedRowCount
                             "FTS_entries"
                             DATABASE_FLAG_NONE,
                             "entryId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(entryId)
                             ),
                             DATABASE_UNLIMITED
                            );
  }

  // update newest entries
  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                             NULL,  // changedRowCount
                             "entriesNewest"
                             DATABASE_FLAG_NONE,
                             "entryId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(entryId)
                             ),
                             DATABASE_UNLIMITED
                            );
#if 0
    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("uuidId")
                                    DATABASE_VALUE_KEY     ("entityId")
                                    DATABASE_VALUE_UINT    ("type")
                                    DATABASE_VALUE_STRING  ("name")
                                    DATABASE_VALUE_DATETIME("timeLastAccess")
                                    DATABASE_VALUE_DATETIME("timeModified")
                                    DATABASE_VALUE_DATETIME("timeLastChanged")
                                    DATABASE_VALUE_UINT    ("userId")
                                    DATABASE_VALUE_UINT    ("groupId")
                                    DATABASE_VALUE_UINT    ("permission")
                                  ),
                                  DATABASE_TABLES
                                  (
                                    "entries"
                                  ),
                                  DATABASE_COLUMNS
                                  (
                                    DATABASE_COLUMN_KEY     ("uuidId"),
                                    DATABASE_COLUMN_KEY     ("entityId"),
                                    DATABASE_COLUMN_UINT    ("type"),
                                    DATABASE_COLUMN_STRING  ("name"),
                                    DATABASE_COLUMN_DATETIME("timeLastAccess"),
                                    DATABASE_COLUMN_DATETIME("timeModified"),
                                    DATABASE_COLUMN_DATETIME("timeLastChanged"),
                                    DATABASE_COLUMN_UINT    ("userId"),
                                    DATABASE_COLUMN_UINT    ("groupId"),
                                    DATABASE_COLUMN_UINT    ("permission")
                                  ),
                                  "id!=? AND name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY   (entryId),
                                    DATABASE_FILTER_STRING(name)
                                  ),
                                  "ORDER BY timeModified DESC",
                                  0LL,
                                  DATABASE_UNLIMITED
                                 );
#endif
  }

  // delete entry
  if (error == ERROR_NONE)
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entries"
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
  }

  // free resources

  return error;
}
#endif

/***********************************************************************\
* Name   : clearStorageFragments
* Purpose: purge storage fragments
* Input  : indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#if 0
// TODO: not needed?
LOCAL Errors clearStorageFragments(IndexHandle  *indexHandle,
                                   DatabaseId   storageId,
                                   ProgressInfo *progressInfo
                                  )
{
  bool                 transactionFlag;
  Errors               error;
  bool                 doneFlag;

  #ifndef NDEBUG
    ulong              deletedCounter;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  error = ERROR_NONE;

  // purge fragments
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_INTERRUPTABLE_OPERATION_DOX(error,
                                      indexHandle,
                                      transactionFlag,
    {
      do
      {
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"entryFragments","count(id)","WHERE storageId=%"PRIi64"",storageId);fprintf(stderr,"%s, %"PRIi64": fragments %d: %"PRIi64"\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "entryFragments",
                                  "storageId=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_KEY(storageId)
                                  )
                                 );
        if (error == ERROR_NONE)
        {
          error = IndexCommon_interruptOperation(indexHandle,&transactionFlag,SLEEP_TIME_PURGE*MS_PER_SECOND);
        }
      }
      while ((error == ERROR_NONE) && !doneFlag);

      return error;
    });
  }
#ifndef NDEBUG
//fprintf(stderr,"%s, %d: 1 fragments done=%d deletedCounter=%lu: %s\n",__FILE__,__LINE__,doneFlag,deletedCounter,Error_getText(error));
#endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, purged fragments: %"PRIu64"ms\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}
#endif

#if 0
//TODO: trigger or implementation here?
/***********************************************************************\
* Name   : insertUpdateNewestEntry
* Purpose: insert or update newest entry
* Input  : indexHandle     - index handle
*          entryId         - entry database id
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          name            - file name
*          size            - size [bytes]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors insertUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     DatabaseId  uuidId,
                                     DatabaseId  entityId,
                                     ConstString name,
                                     IndexTypes  indexType,
                                     uint64      size,
                                     uint64      timeLastChanged,
                                     uint32      userId,
                                     uint32      groupId,
                                     uint32      permission
                                    )
{
  Errors                  error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId              newestEntryId;
  uint64                  newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    // atomic add/get entry
    DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // get existing newest entry
      error = Database_get(&indexHandle->databaseHandle,
                           CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                           {
                             assert(values != NULL);
                             assert(valueCount == 3);

                             UNUSED_VARIABLE(userData);
                             UNUSED_VARIABLE(valueCount);

                             newestEntryId         = values[0].id;
                             newestTimeLastChanged = values[1].u64;

                             return ERROR_NONE;
                           },NULL),
                           NULL,  // changedRowCount
                           DATABASE_TABLES
                           (
                             "entriesNewest"
                           ),
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("id"),
                             DATABASE_COLUMN_DATETIME("timeLastChanged")
                           ),
                           "name=?",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_STRING(name)
                           ),
                           NULL,  // orderGroup
                           0LL,
                           1LL
                          );
      if (error != ERROR_NONE)
      {
        newestEntryId         = DATABASE_ID_NONE;
        newestTimeLastChanged = 0LL;
      }

      // insert/update newest
      if (error == ERROR_NONE)
      {
        if (timeLastChanged > newestTimeLastChanged)
        {
          if (newestEntryId != DATABASE_ID_NONE)
          {
            // update
            error = Database_update(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    DATABASE_COLUMN_TYPES(),
                                    "entriesNewest",
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY     ("entryId",         entryId),
                                      DATABASE_VALUE_KEY     ("uuidId",          uuidId),
                                      DATABASE_VALUE_KEY     ("entityId",        entityId),
                                      DATABASE_VALUE_UINT    ("type",            indexType),
                                      DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT    ("userId",          userId),
                                      DATABASE_VALUE_UINT    ("groupId",         groupId),
                                      DATABASE_VALUE_UINT    ("permission",      permission),
                                    ),
                                    "id=?",
                                    DATABASE_FILTERS
                                    (
                                      DATABASE_FILTER_KEY(newestEntryId)
                                    )
                                   );
          }
          else
          {
            // insert
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY     ("entryId",         entryId),
                                      DATABASE_VALUE_KEY     ("uuidId",          uuidId),
                                      DATABASE_VALUE_KEY     ("entityId",        entityId),
                                      DATABASE_VALUE_UINT    ("type",            indexType),
                                      DATABASE_VALUE_STROMG  ("name",            name),
                                      DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                      DATABASE_VALUE_UINT    ("userId",          userId),
                                      DATABASE_VALUE_UINT    ("groupId",         groupId),
                                      DATABASE_VALUE_UINT    ("permission",      permission)
                                    )
                                   );
          }
        }
      }
    }

    return error;
  });

  return error;
}

/***********************************************************************\
* Name   : removeUpdateNewestEntry
* Purpose: remove or update newest entry
* Input  : indexHandle - index handle
*          entryId     - entry database id
*          name        - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors removeUpdateNewestEntry(IndexHandle *indexHandle,
                                     DatabaseId  entryId,
                                     ConstString name
                                    )
{
  Errors              error;
  DatabaseStatementHandle databaseStatementHandle;
  DatabaseId          newestEntryId;
  uint64              newestTimeLastChanged;

  assert(indexHandle != NULL);
  assert(entryId != DATABASE_ID_NONE);
  assert(name != NULL);

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(entryId)
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

// TODO:
    error = Database_insert(&indexHandle->databaseHandle,
                            NULL,  // insertRowId
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY     ("entryId",         entryId),
                              DATABASE_VALUE_KEY     ("uuidId",          uuidId),
                              DATABASE_VALUE_KEY     ("entityId",        entityId),
                              DATABASE_VALUE_UINT    ("type",            indexType),
                              DATABASE_VALUE_STRING  ("name",            name),
                              DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                              DATABASE_VALUE_UINT    ("userId",          userId),
                              DATABASE_VALUE_UINT    ("groupId",         groupId),
                              DATABASE_VALUE_UINT    ("permission",      permission)
                            )
                           );
#if 0
    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("entryId")
                                    DATABASE_VALUE_KEY     ("uuidId")
                                    DATABASE_VALUE_KEY     ("entityId")
                                    DATABASE_VALUE_UINT    ("type")
                                    DATABASE_VALUE_STRING  ("name")
                                    DATABASE_VALUE_DATETIME("timeLastChanged")
                                    DATABASE_VALUE_UINT    ("userId")
                                    DATABASE_VALUE_UINT    ("groupId")
                                    DATABASE_VALUE_UINT    ("permission")
                                  ),
                                  DATABASE_TABLES
                                  (
                                    "entries"
                                  ),
                                  DATABASE_COLUMNS
                                  (
                                    DATABASE_COLUMN_KEY     ("id"),
                                    DATABASE_COLUMN_KEY     ("uuidId"),
                                    DATABASE_COLUMN_KEY     ("entityId"),
                                    DATABASE_COLUMN_UINT    ("type"),
                                    DATABASE_COLUMN_STRING  ("name"),
                                    DATABASE_VALUE_DATETIME("timeLastChanged"),
                                    DATABASE_COLUMN_UINT    ("userId"),
                                    DATABASE_COLUMN_UINT    ("groupId"),
                                    DATABASE_COLUMN_UINT    ("permission")
                                  ),
                                  "name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(name)
                                  ),
                                  "ORDER BY timeLastChanged DESC",
                                  0LL,
                                  1LL
                                 );
#else
    error = Database_insertSelect(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entriesNewest",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("entryId")
                                    DATABASE_VALUE_KEY     ("uuidId")
                                    DATABASE_VALUE_KEY     ("entityId")
                                    DATABASE_VALUE_UINT    ("type")
                                    DATABASE_VALUE_STRING  ("name")
                                    DATABASE_VALUE_DATETIME("timeLastChanged")
                                    DATABASE_VALUE_UINT    ("userId")
                                    DATABASE_VALUE_UINT    ("groupId")
                                    DATABASE_VALUE_UINT    ("permission")
                                  ),
                                  DATABASE_TABLES
                                  (
                                    "entries"
                                  ),
                                  DATABASE_COLUMNS
                                  (
                                    DATABASE_COLUMN_KEY     ("id"),
                                    DATABASE_COLUMN_KEY     ("uuidId"),
                                    DATABASE_COLUMN_KEY     ("entityId"),
                                    DATABASE_COLUMN_UINT    ("type"),
                                    DATABASE_COLUMN_STRING  ("name"),
                                    DATABASE_COLUMN_DATETIME("timeLastChanged"),
                                    DATABASE_COLUMN_UINT    ("userId"),
                                    DATABASE_COLUMN_UINT    ("groupId"),
                                    DATABASE_COLUMN_UINT    ("permission")
                                  ),
                                  "name=?",
                                  DATABASE_FILTERS
                                  (
                                    DATABASE_FILTER_STRING(name)
                                  ),
                                  "ORDER BY timeLastChanged DESC",
                                  0LL,
                                  1LL
                                 );
#endif
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}
#endif

/***********************************************************************\
* Name   : updateDirectoryContentAggregates
* Purpose: update directory content count/size
* Input  : indexHandle - index handle
*          storageId   - storage database id
*          entryId     - entry database id
*          fileName    - file name
*          size        - size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateDirectoryContentAggregates(IndexHandle *indexHandle,
                                              DatabaseId  storageId,
                                              DatabaseId  entryId,
                                              ConstString fileName,
                                              uint64      size
                                             )
{
  Errors     error;
  IndexId    entriesNewestId;
  DatabaseId databaseId;
  String     directoryName;

  assert(indexHandle != NULL);
  assert(storageId != DATABASE_ID_NONE);
  assert(entryId != DATABASE_ID_NONE);
  assert(fileName != NULL);

  // get newest id
  error = Database_getId(&indexHandle->databaseHandle,
                         &databaseId,
                         "entriesNewest",
                         "id",
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY(entryId)
                         )
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  entriesNewestId = INDEX_ID_ENTRY(databaseId);

  directoryName = File_getDirectoryName(String_new(),fileName);
  while (!String_isEmpty(directoryName) && (error == ERROR_NONE))
  {
    // update directory entry
    error = Database_update(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "directoryEntries",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE       ("totalEntryCount", "totalEntryCount+1"),
                              DATABASE_VALUE_UINT64("totalEntrySize",  "totalEntrySize+?", size)
                            ),
                            "    storageId=? \
                             AND name=? \
                            ",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY   (storageId),
                              DATABASE_FILTER_STRING(directoryName)
                            )
                           );
    if (error != ERROR_NONE)
    {
      break;
    }

    if (!INDEX_ID_IS_NONE(entriesNewestId))
    {
      // update directory entry newest
      error = Database_update(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "directoryEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE       ("totalEntryCountNewest", "totalEntryCountNewest+1"),
                                DATABASE_VALUE_UINT64("totalEntrySizeNewest",  "totalEntrySizeNewest+?", size),
                              ),
                              "    storageId=? \
                               AND name=? \
                              ",
                              DATABASE_FILTERS
                              (
                                DATABASE_FILTER_KEY   (storageId),
                                DATABASE_FILTER_STRING(directoryName)
                              )
                             );
      if (error != ERROR_NONE)
      {
        break;
      }
    }

    File_getDirectoryName(directoryName,directoryName);
  }
  String_delete(directoryName);

  return error;
}

// ----------------------------------------------------------------------

Errors IndexEntry_collectIds(Array        *entryIds,
                             IndexHandle  *indexHandle,
                             IndexId      storageId,
                             ProgressInfo *progressInfo
                            )
{
  Errors error;
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0,dt[10];
  #endif

  UNUSED_VARIABLE(progressInfo);

  // init variables
  Array_clear(entryIds);
  error = ERROR_NONE;

  // collect file/image/hardlink entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "entryFragments",
                             "entryId",
                             "storageId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[0] = Misc_getTimestamp()-t0;
  #endif

  // collect directory entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "directoryEntries",
                             "entryId",
                             "storageId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[1] = Misc_getTimestamp()-t0;
  #endif

  // collect link entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "linkEntries",
                             "entryId",
                             "storageId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[2] = Misc_getTimestamp()-t0;
  #endif

  // collect special entries to purge
  #ifdef INDEX_DEBUG_PURGE
    t0 = Misc_getTimestamp();
  #endif
  if (error == ERROR_NONE)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "specialEntries",
                             "entryId",
                             "storageId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(storageId))
                             ),
                             DATABASE_UNLIMITED
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[3] = Misc_getTimestamp()-t0;
  #endif

  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries to purge: fragment %"PRIu64"ms, directory %"PRIu64"ms, link %"PRIu64"ms, special %"PRIu64"ms\n",__FILE__,__LINE__,
            Error_getText(error),
            Array_length(&entryIds),
            dt[0]/US_PER_MS,
            dt[1]/US_PER_MS,
            dt[2]/US_PER_MS,
            dt[3]/US_PER_MS
           );
  #endif

  return error;
}

Errors IndexEntry_pruneAll(IndexHandle *indexHandle,
                           bool        *doneFlag,
                           ulong       *deletedCounter
                          )
{
  Array  databaseIds;
  Errors error;

  assert(indexHandle != NULL);

  // init vairables
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all file entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "fileEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.entryId \
                          ",
                          "fileEntries.id",
                          "entryFragments.id IS NULL",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // delete file entries without fragments
  error = IndexCommon_deleteByIds(indexHandle,
                                  doneFlag,
                                  deletedCounter,
                                  "fileEntries",
                                  "id",
                                  Array_cArray(&databaseIds),
                                  Array_length(&databaseIds)
                                 );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all image entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "imageEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.entryId \
                          ",
                          "imageEntries.id",
                          "entryFragments.id IS NULL",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // delete image entries without fragments
  error = IndexCommon_deleteByIds(indexHandle,
                                  doneFlag,
                                  deletedCounter,
                                  "imageEntries",
                                  "id",
                                  Array_cArray(&databaseIds),
                                  Array_length(&databaseIds)
                                 );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all hardlink entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "hardlinkEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.entryId \
                          ",
                          "hardlinkEntries.id",
                          "entryFragments.id IS NULL",
                          DATABASE_FILTERS
                          (
                          ),
                          DATABASE_UNLIMITED
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // delete hardlink entries without fragments
  error = IndexCommon_deleteByIds(indexHandle,
                                  doneFlag,
                                  deletedCounter,
                                  "hardlinkEntries",
                                  "id",
                                  Array_cArray(&databaseIds),
                                  Array_length(&databaseIds)
                                 );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // free resources
  Array_done(&databaseIds);

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            uint          indexIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypes    indexType,
                            ConstString   name,
                            bool          newestOnly,
                            uint          *totalStorageCount,
                            uint64        *totalStorageSize,
                            uint          *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           )
{
  Errors error;
  String ftsMatchString;
  String uuidIdsString,entityIdsString;
  String entryIdsString;
  ulong  i;
  String filterString;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64 t0,t1;
  #endif

  assert(indexHandle != NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  if (totalStorageCount     != NULL) (*totalStorageCount    ) = 0;
  if (totalStorageSize      != NULL) (*totalStorageSize     ) = 0LL;
  if (totalEntryCount       != NULL) (*totalEntryCount      ) = 0;
  if (totalEntrySize        != NULL) (*totalEntrySize       ) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString  = String_new();
  uuidIdsString   = String_new();
  entityIdsString = String_new();
  entryIdsString  = String_new();
  filterString    = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (INDEX_TYPE(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_getEntriesInfo --------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: entryIdsString=%s\n",__FILE__,__LINE__,String_cString(entryIdsString));
    fprintf(stderr,"%s, %d: ftsMatchString=%s\n",__FILE__,__LINE__,String_cString(ftsMatchString));
  #endif /* INDEX_DEBUG_LIST_INFO */

  // get filters
  Database_filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  error = ERROR_NONE;
  if (String_isEmpty(ftsMatchString))
  {
    // no names
    #ifdef INDEX_DEBUG_LIST_INFO
      t0 = Misc_getTimestamp();
    #endif /* INDEX_DEBUG_LIST_INFO */

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      // get total entry count, total fragment count, total entry size
      if (newestOnly)
      {
        // all newest entries
        if (String_isEmpty(entryIdsString))
        {
          switch (indexType)
          {
            case INDEX_TYPE_NONE:
            case INDEX_TYPE_ANY:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalEntryCountNewest)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalEntrySizeNewest)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_FILE:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalFileCountNewest)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalFileSizeNewest)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_IMAGE:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalImageCountNewest)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalImageSizeNewest)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_DIRECTORY:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalDirectoryCountNewest)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_LINK:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalLinkCountNewest)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_HARDLINK:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalHardlinkCountNewest)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalHardlinkSizeNewest)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_SPECIAL:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalSpecialCountNewest)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break;
          }
        }
        else
        {
          error = Database_get(&indexHandle->databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 4);

                                 UNUSED_VARIABLE(userData);
                                 UNUSED_VARIABLE(valueCount);

                                 if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                 if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                 if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                 if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_TABLES
                               (
                                 "entriesNewest \
                                    LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                    LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                 "
                               ),
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_UINT  ("0"),
                                 DATABASE_COLUMN_UINT64("0"),
                                 DATABASE_COLUMN_UINT  ("COUNT(entriesNewest.id)"),
                                 DATABASE_COLUMN_UINT64("SUM(entriesNewest.size)")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "    entities.deletedFlag!=TRUE \
                                             AND %s \
                                            ",
                                            String_cString(filterString)
                                           ),
                               DATABASE_FILTERS
                               (
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               0LL,
                               1LL
                              );
        }
      }
      else
      {
        // all entries
        if (String_isEmpty(entryIdsString))
        {
          switch (indexType)
          {
            case INDEX_TYPE_NONE:
            case INDEX_TYPE_ANY:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalEntryCount)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalEntrySize)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_FILE:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalFileCount)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalFileSize)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_IMAGE:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalImageCount)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalImageSize)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_DIRECTORY:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalDirectoryCount)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_LINK:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalLinkCount)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_HARDLINK:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalHardlinkCount)"),
                                     DATABASE_COLUMN_UINT64("SUM(entities.totalHardlinkSize)")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            case INDEX_TYPE_SPECIAL:
              error = Database_get(&indexHandle->databaseHandle,
                                   CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                                   {
                                     assert(values != NULL);
                                     assert(valueCount == 4);

                                     UNUSED_VARIABLE(userData);
                                     UNUSED_VARIABLE(valueCount);

                                     if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                     if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                     if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                     if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                     return ERROR_NONE;
                                   },NULL),
                                   NULL,  // changedRowCount
                                   DATABASE_TABLES
                                   (
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     "
                                   ),
                                   DATABASE_FLAG_NONE,
                                   DATABASE_COLUMNS
                                   (
                                     DATABASE_COLUMN_UINT  ("0"),
                                     DATABASE_COLUMN_UINT64("0"),
                                     DATABASE_COLUMN_UINT  ("SUM(entities.totalSpecialCount)"),
                                     DATABASE_COLUMN_UINT64("0")
                                   ),
                                   stringFormat(sqlString,sizeof(sqlString),
                                                "    entities.deletedFlag!=TRUE \
                                                 AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_FILTERS
                                   (
                                   ),
                                   NULL,  // groupBy
                                   NULL,  // orderby
                                   0LL,
                                   1LL
                                  );
              break;
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break;
          }
        }
        else
        {
          error = Database_get(&indexHandle->databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 4);

                                 UNUSED_VARIABLE(userData);
                                 UNUSED_VARIABLE(valueCount);

                                 if (totalStorageCount != NULL) (*totalStorageCount) = values[0].u;
                                 if (totalStorageSize  != NULL) (*totalStorageSize ) = values[1].u64;
                                 if (totalEntryCount   != NULL) (*totalEntryCount  ) = values[2].u;
                                 if (totalEntrySize    != NULL) (*totalEntrySize   ) = values[3].u64;

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_TABLES
                               (
                                 "entries \
                                    LEFT JOIN entities ON entities.id=entries.entityId \
                                    LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                 "
                               ),
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_UINT  ("0"),
                                 DATABASE_COLUMN_UINT64("0"),
                                 DATABASE_COLUMN_UINT  ("COUNT(entries.id)"),
                                 DATABASE_COLUMN_UINT64("SUM(entries.size)")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "    entities.deletedFlag!=TRUE \
                                             AND %s \
                                            ",
                                            String_cString(filterString)
                                           ),
                               DATABASE_FILTERS
                               (
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               0LL,
                               1LL
                              );
        }
      }

      return error;
    });
    assertx(   (error == ERROR_NONE)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY),
            "%s",Error_getText(error)
           );

    #ifdef INDEX_DEBUG_LIST_INFO
      t1 = Misc_getTimestamp();
      fprintf(stderr,"%s, %d: totalStorageCount=%lu totalStorageSize=%"PRIu64" totalEntryCount_=%lu totalEntrySize_=%"PRIu64"\n",__FILE__,__LINE__,*totalStorageCount,*totalStorageSize,*totalEntryCount,*totalEntrySize);
      fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
      fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
    #endif

    #ifdef INDEX_DEBUG_LIST_INFO
      t0 = Misc_getTimestamp();
    #endif /* INDEX_DEBUG_LIST_INFO */

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      // get entry content size
      if (newestOnly)
      {
        error = Database_get(&indexHandle->databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(userData);
                               UNUSED_VARIABLE(valueCount);

                               if (totalEntryContentSize != NULL) (*totalEntryContentSize) = values[0].u64;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "entriesNewest \
                                  LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                               "
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
// TODO: directory correct?
                               DATABASE_COLUMN_UINT64("SUM(directoryEntries.totalEntrySize)")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    entities.deletedFlag!=TRUE \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0LL,
                             1LL
                            );
      }
      else
      {
        if (String_isEmpty(entryIdsString))
        {
          // no storages selected, no entries selected -> get aggregated data from entities
          error = Database_get(&indexHandle->databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 1);

                                 UNUSED_VARIABLE(userData);
                                 UNUSED_VARIABLE(valueCount);

                                 if (totalEntryContentSize != NULL) (*totalEntryContentSize) = values[0].u64;

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_TABLES
                               (
                                 "entities \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                 "
                               ),
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_UINT64("SUM(entities.totalEntrySize)")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "    entities.deletedFlag!=TRUE \
                                             AND %s \
                                            ",
                                            String_cString(filterString)
                                           ),
                               DATABASE_FILTERS
                               (
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               0LL,
                               1LL
                             );
        }
        else
        {
          // entries selected -> get aggregated data from entries
          error = Database_get(&indexHandle->databaseHandle,
                               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                               {
                                 assert(values != NULL);
                                 assert(valueCount == 1);

                                 UNUSED_VARIABLE(userData);
                                 UNUSED_VARIABLE(valueCount);

                                 if (totalEntryContentSize != NULL) (*totalEntryContentSize) = values[0].u64;

                                 return ERROR_NONE;
                               },NULL),
                               NULL,  // changedRowCount
                               DATABASE_TABLES
                               (
                                 "entries \
                                    LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                    LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                    LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                    LEFT JOIN entities         ON entities.id=entries.entityId \
                                    LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                 "
                               ),
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_UINT64("SUM(directoryEntries.totalEntrySize)")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "    entities.deletedFlag!=TRUE \
                                             AND %s \
                                            ",
                                            String_cString(filterString)
                                           ),
                               DATABASE_FILTERS
                               (
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               0LL,
                               1LL
                              );
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    assertx(   (error == ERROR_NONE)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY),
            "%s",Error_getText(error)
           );

    #ifdef INDEX_DEBUG_LIST_INFO
      t1 = Misc_getTimestamp();
      fprintf(stderr,"%s, %d: totalEntryContentSize=%"PRIu64"\n",__FILE__,__LINE__,*totalEntryContentSize);
      fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
      fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
    #endif
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names selected

    // get filters
    Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","%S",ftsMatchString);
    if (newestOnly)
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      Database_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
    }
    else
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      Database_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
    }

    #ifdef INDEX_DEBUG_LIST_INFO
      t0 = Misc_getTimestamp();
    #endif /* INDEX_DEBUG_LIST_INFO */

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      // get entry count, entry size
      if (newestOnly)
      {
        error = Database_get(&indexHandle->databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(userData);
                               UNUSED_VARIABLE(valueCount);

                               if (totalEntryCount != NULL) (*totalEntryCount) = values[0].u;
                               if (totalEntrySize  != NULL) (*totalEntrySize ) = values[1].u64;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "FTS_entries \
                                  LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                  LEFT JOIN entities      ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids         ON uuids.jobUUID=entities.jobUUID \
                               "
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_UINT  ("COUNT(entriesNewest.id)"),
                               DATABASE_COLUMN_UINT64("SUM(entriesNewest.size)"),
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    entities.deletedFlag!=TRUE \
                                           AND entriesNewest.id IS NOT NULL \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0LL,
                             1LL
                            );
      }
      else
      {
        error = Database_get(&indexHandle->databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 2);

                               UNUSED_VARIABLE(userData);
                               UNUSED_VARIABLE(valueCount);

                               if (totalEntryCount != NULL) (*totalEntryCount) = values[0].u;
                               if (totalEntrySize  != NULL) (*totalEntrySize ) = values[1].u64;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "FTS_entries \
                                  LEFT JOIN entries  ON entries.id=FTS_entries.entryId \
                                  LEFT JOIN entities ON entities.id=entries.entityId \
                                  LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                               "
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_UINT  ("COUNT(entries.id)"),
                               DATABASE_COLUMN_UINT64("SUM(entries.size)")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    entities.deletedFlag!=TRUE \
                                           AND entries.id IS NOT NULL \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0LL,
                             1LL
                            );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    assertx(   (error == ERROR_NONE)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY),
            "%s",Error_getText(error)
           );

    #ifdef INDEX_DEBUG_LIST_INFO
      t1 = Misc_getTimestamp();
      fprintf(stderr,"%s, %d: totalEntryCount=%lu totalEntrySize_=%"PRIu64"\n",__FILE__,__LINE__,*totalEntryCount,*totalEntrySize);
      fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
      fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
    #endif

    #ifdef INDEX_DEBUG_LIST_INFO
      t0 = Misc_getTimestamp();
    #endif /* INDEX_DEBUG_LIST_INFO */

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      // get entry content size
      if (newestOnly)
      {
        error = Database_get(&indexHandle->databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(userData);
                               UNUSED_VARIABLE(valueCount);

                               if (totalEntryContentSize != NULL) (*totalEntryContentSize) = values[0].u64;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "FTS_entries \
                                  LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN entries          ON entries.id=entriesNewest.entryId \
                                  LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                  LEFT JOIN entities         ON entities.id=storages.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                               "
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_UINT64("SUM(directoryEntries.totalEntrySize)")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    storages.deletedFlag!=TRUE \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0LL,
                             1LL
                            );
      }
      else
      {
        error = Database_get(&indexHandle->databaseHandle,
                             CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                             {
                               assert(values != NULL);
                               assert(valueCount == 1);

                               UNUSED_VARIABLE(userData);
                               UNUSED_VARIABLE(valueCount);

                               if (totalEntryContentSize != NULL) (*totalEntryContentSize) = values[0].u64;

                               return ERROR_NONE;
                             },NULL),
                             NULL,  // changedRowCount
                             DATABASE_TABLES
                             (
                               "FTS_entries \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=FTS_entries.entryId \
                                  LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                  LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                  LEFT JOIN entities         ON entities.id=storages.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                               "
                             ),
                             DATABASE_FLAG_NONE,
                             DATABASE_COLUMNS
                             (
                               DATABASE_COLUMN_UINT64("SUM(directoryEntries.totalEntrySize)")
                             ),
                             stringFormat(sqlString,sizeof(sqlString),
                                          "    storages.deletedFlag!=TRUE \
                                           AND %s \
                                          ",
                                          String_cString(filterString)
                                         ),
                             DATABASE_FILTERS
                             (
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0LL,
                             1LL
                            );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
    assertx(   (error == ERROR_NONE)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_TIMEOUT)
            || (Error_getCode(error) == ERROR_CODE_DATABASE_BUSY),
            "%s",Error_getText(error)
           );

    #ifdef INDEX_DEBUG_LIST_INFO
      t1 = Misc_getTimestamp();
      fprintf(stderr,"%s, %d: totalEntryContentSize=%"PRIu64"\n",__FILE__,__LINE__,*totalEntryContentSize);
      fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
      fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
    #endif
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsMatchString);

  return ERROR_NONE;
}

Errors Index_initListEntries(IndexQueryHandle    *indexQueryHandle,
                             IndexHandle         *indexHandle,
                             const IndexId       indexIds[],
                             uint                indexIdCount,
                             const IndexId       entryIds[],
                             uint                entryIdCount,
                             IndexTypes          indexType,
                             ConstString         name,
                             bool                newestOnly,
                             bool                fragmentsCount,
                             IndexEntrySortModes sortMode,
                             DatabaseOrdering    ordering,
                             uint64              offset,
                             uint64              limit
                            )
{
  String ftsMatchString;
  String uuidIdsString,entityIdsString;
  String entryIdsString;
  ulong  i;
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString   = String_new();
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  entryIdsString   = String_new();
  orderString      = String_new();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (INDEX_TYPE(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
  }

  // get filters
  filterString = Database_newFilter();
  Database_filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
  if (newestOnly)
  {
    Database_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
  }
  else
  {
    Database_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
  }

  // get sort mode, ordering
  if (newestOnly)
  {
    IndexCommon_appendOrdering(orderString,
                               sortMode != INDEX_ENTRY_SORT_MODE_NONE,
                               INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[sortMode],
                               ordering
                              );
  }
  else
  {
    IndexCommon_appendOrdering(orderString,
                               sortMode != INDEX_ENTRY_SORT_MODE_NONE,
                               INDEX_ENTRY_SORT_MODE_COLUMNS[sortMode],
                               ordering
                              );
  }

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntries -------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  if (String_isEmpty(ftsMatchString))
  {
    // entries selected

    // get additional filters
    if (newestOnly)
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlString[MAX_SQL_COMMAND_LENGTH];

      if (newestOnly)
      {
        return Database_select(&indexQueryHandle->databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               "entriesNewest \
                                  LEFT JOIN entities         ON entities.id=entriesNewest.entityid \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                               ",
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_KEY     ("uuids.id"),
                                 DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                                 DATABASE_COLUMN_KEY     ("entities.id"),
                                 DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                                 DATABASE_COLUMN_STRING  ("entities.hostName"),
                                 DATABASE_COLUMN_STRING  ("entities.userName"),
                                 DATABASE_COLUMN_UINT    ("entities.type"),
                                 DATABASE_COLUMN_KEY     ("entriesNewest.entryId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.type"),
                                 DATABASE_COLUMN_STRING  ("entriesNewest.name"),
                                 DATABASE_COLUMN_DATETIME("entriesNewest.timeLastChanged"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.userId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.groupId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.permission"),
                                 DATABASE_COLUMN_UINT64  ("entriesNewest.size"),
                                 DATABASE_COLUMN_UINT    (fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0"),
                                 DATABASE_COLUMN_KEY     ("CASE entriesNewest.type \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN directoryEntries.storageId \
                                                             WHEN ? THEN linkEntries.storageId \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN specialEntries.storageId \
                                                           END \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_STRING  ("(SELECT name FROM storages WHERE id=CASE entriesNewest.type \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN directoryEntries.storageId \
                                                                                                 WHEN ? THEN linkEntries.storageId \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN specialEntries.storageId \
                                                                                               END \
                                                           ) \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_UINT64  ("fileEntries.size"),
                                 DATABASE_COLUMN_UINT64  ("imageEntries.size"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.fileSystemType"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.blockSize"),
                                 DATABASE_COLUMN_UINT64  ("directoryEntries.totalEntrySizeNewest"),
                                 DATABASE_COLUMN_STRING  ("linkEntries.destinationName"),
                                 DATABASE_COLUMN_UINT64  ("hardlinkEntries.size")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "     entities.deletedFlag!=TRUE \
                                             AND entriesNewest.id IS NOT NULL \
                                             AND %s \
                                             %s \
                                            ",
                                            String_cString(filterString),
                                            String_cString(orderString)
                                           ),
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),

                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL)
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               offset,
                               limit
                              );
      }
      else
      {
        return Database_select(&indexQueryHandle->databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               "entries \
                                  LEFT JOIN entities         ON entities.id=entries.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                  LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                  LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                  LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                  LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                               ",
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_KEY     ("uuids.id"),
                                 DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                                 DATABASE_COLUMN_KEY     ("entities.id"),
                                 DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                                 DATABASE_COLUMN_STRING  ("entities.hostName"),
                                 DATABASE_COLUMN_STRING  ("entities.userName"),
                                 DATABASE_COLUMN_UINT    ("entities.type"),
                                 DATABASE_COLUMN_KEY     ("entries.id"),
                                 DATABASE_COLUMN_UINT    ("entries.type"),
                                 DATABASE_COLUMN_STRING  ("entries.name"),
                                 DATABASE_COLUMN_DATETIME("entries.timeLastChanged"),
                                 DATABASE_COLUMN_UINT    ("entries.userId"),
                                 DATABASE_COLUMN_UINT    ("entries.groupId"),
                                 DATABASE_COLUMN_UINT    ("entries.permission"),
                                 DATABASE_COLUMN_UINT64  ("entries.size"),
                                 DATABASE_COLUMN_UINT    (fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0"),
                                 DATABASE_COLUMN_KEY     ("CASE entries.type \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN directoryEntries.storageId \
                                                             WHEN ? THEN linkEntries.storageId \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN specialEntries.storageId \
                                                           END"
                                                         ),
                                 DATABASE_COLUMN_STRING  ("(SELECT name FROM storages WHERE id=CASE entries.type \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN directoryEntries.storageId \
                                                                                                 WHEN ? THEN linkEntries.storageId \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN specialEntries.storageId \
                                                                                               END \
                                                           ) \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_UINT64  ("fileEntries.size"),
                                 DATABASE_COLUMN_UINT64  ("imageEntries.size"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.fileSystemType"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.blockSize"),
                                 DATABASE_COLUMN_UINT64  ("directoryEntries.totalEntrySize"),
                                 DATABASE_COLUMN_STRING  ("linkEntries.destinationName"),
                                 DATABASE_COLUMN_UINT64  ("hardlinkEntries.size")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "     entities.deletedFlag!=TRUE \
                                             AND %s \
                                             %s \
                                            ",
                                            String_cString(filterString),
                                            String_cString(orderString)
                                           ),
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),

                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL)
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               offset,
                               limit
                              );
      }
    });
  }
  else /* !String_isEmpty(ftsMatchString) */
  {
    // names (and optional entries) selected

    // get additional filters
    Database_filterAppend(filterString,TRUE,"AND",String_cString(ftsMatchString));
    if (newestOnly)
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    if (newestOnly)
    {
      INDEX_DOX(error,
                indexHandle,
      {
        char sqlString[MAX_SQL_COMMAND_LENGTH];

        return Database_select(&indexQueryHandle->databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               "FTS_entries \
                                  LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                  LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                               ",
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_KEY     ("uuids.id"),
                                 DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                                 DATABASE_COLUMN_KEY     ("entities.id"),
                                 DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                                 DATABASE_COLUMN_STRING  ("entities.hostName"),
                                 DATABASE_COLUMN_STRING  ("entities.userName"),
                                 DATABASE_COLUMN_UINT    ("entities.type"),
                                 DATABASE_COLUMN_KEY     ("entriesNewest.entryId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.type"),
                                 DATABASE_COLUMN_STRING  ("entriesNewest.name"),
                                 DATABASE_COLUMN_DATETIME("entriesNewest.timeLastChanged"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.userId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.groupId"),
                                 DATABASE_COLUMN_UINT    ("entriesNewest.permission"),
                                 DATABASE_COLUMN_UINT64  ("entriesNewest.size"),
                                 DATABASE_COLUMN_UINT    (fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0"),
                                 DATABASE_COLUMN_KEY     ("CASE entriesNewest.type \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN directoryEntries.storageId \
                                                             WHEN ? THEN linkEntries.storageId \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN specialEntries.storageId \
                                                           END \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_STRING  ("(SELECT name FROM storages WHERE id=CASE entriesNewest.type \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN directoryEntries.storageId \
                                                                                                 WHEN ? THEN linkEntries.storageId \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN specialEntries.storageId \
                                                                                               END \
                                                           ) \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_UINT64  ("fileEntries.size"),
                                 DATABASE_COLUMN_UINT64  ("imageEntries.size"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.fileSystemType"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.blockSize"),
                                 DATABASE_COLUMN_UINT64  ("directoryEntries.totalEntrySizeNewest"),
                                 DATABASE_COLUMN_STRING  ("linkEntries.destinationName"),
                                 DATABASE_COLUMN_UINT64  ("hardlinkEntries.size")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "     entities.deletedFlag!=TRUE \
                                              AND entriesNewest.id IS NOT NULL \
                                              AND %s \
                                              %s \
                                            ",
                                            String_cString(filterString),
                                            String_cString(orderString)
                                           ),
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),

                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL)
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               offset,
                               limit
                              );
      });
    }
    else
    {
      INDEX_DOX(error,
                indexHandle,
      {
        char sqlString[MAX_SQL_COMMAND_LENGTH];

        return Database_select(&indexQueryHandle->databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               "FTS_entries \
                                  LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                  LEFT JOIN entities         ON entities.id=entries.entityId \
                                  LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                  LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                  LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                  LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                  LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                               ",
                               DATABASE_FLAG_NONE,
                               DATABASE_COLUMNS
                               (
                                 DATABASE_COLUMN_KEY     ("uuids.id"),
                                 DATABASE_COLUMN_STRING  ("uuids.jobUUID"),
                                 DATABASE_COLUMN_KEY     ("entities.id"),
                                 DATABASE_COLUMN_STRING  ("entities.scheduleUUID"),
                                 DATABASE_COLUMN_STRING  ("entities.hostName"),
                                 DATABASE_COLUMN_STRING  ("entities.userName"),
                                 DATABASE_COLUMN_UINT    ("entities.type"),
                                 DATABASE_COLUMN_KEY     ("entries.id"),
                                 DATABASE_COLUMN_UINT    ("entries.type"),
                                 DATABASE_COLUMN_STRING  ("entries.name"),
                                 DATABASE_COLUMN_DATETIME("entries.timeLastChanged"),
                                 DATABASE_COLUMN_UINT    ("entries.userId"),
                                 DATABASE_COLUMN_UINT    ("entries.groupId"),
                                 DATABASE_COLUMN_UINT    ("entries.permission"),
                                 DATABASE_COLUMN_UINT64  ("entries.size"),
                                 DATABASE_COLUMN_UINT    (fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0"),
                                 DATABASE_COLUMN_KEY     ("CASE entries.type \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN directoryEntries.storageId \
                                                             WHEN ? THEN linkEntries.storageId \
                                                             WHEN ? THEN 0 \
                                                             WHEN ? THEN specialEntries.storageId \
                                                           END \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_STRING  ("(SELECT name FROM storages WHERE id=CASE entries.type \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN directoryEntries.storageId \
                                                                                                 WHEN ? THEN linkEntries.storageId \
                                                                                                 WHEN ? THEN 0 \
                                                                                                 WHEN ? THEN specialEntries.storageId \
                                                                                               END \
                                                           ) \
                                                          "
                                                         ),
                                 DATABASE_COLUMN_UINT64  ("fileEntries.size"),
                                 DATABASE_COLUMN_UINT64  ("imageEntries.size"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.fileSystemType"),
                                 DATABASE_COLUMN_UINT    ("imageEntries.blockSize"),
                                 DATABASE_COLUMN_UINT64  ("directoryEntries.totalEntrySizeNewest"),
                                 DATABASE_COLUMN_STRING  ("linkEntries.destinationName"),
                                 DATABASE_COLUMN_UINT64  ("hardlinkEntries.size")
                               ),
                               stringFormat(sqlString,sizeof(sqlString),
                                            "     entities.deletedFlag!=TRUE \
                                               AND %s \
                                               %s \
                                            ",
                                            String_cString(filterString),
                                            String_cString(orderString)
                                           ),
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),

                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_DIRECTORY),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_LINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL)
                               ),
                               NULL,  // groupBy
                               NULL,  // orderby
                               offset,
                               limit
                              );
      });
    }
  }
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsMatchString);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntry(IndexQueryHandle *indexQueryHandle,
                        IndexId          *uuidId,
                        String           jobUUID,
                        IndexId          *entityId,
                        String           scheduleUUID,
                        String           hostName,
                        String           userName,
                        ArchiveTypes     *archiveType,
                        IndexId          *entryId,
                        String           entryName,
                        IndexId          *storageId,
                        String           storageName,
                        uint64           *size,
                        uint64           *timeModified,
                        uint32           *userId,
                        uint32           *groupId,
                        uint32           *permission,
                        uint             *fragmentCount,
                        String           destinationName,
                        FileSystemTypes  *fileSystemType,
                        uint             *blockSize
                       )
{
  IndexTypes indexType;
  DatabaseId uuidDatabaseId,entityDatabaseId,entryDatabaseId;
  int        fileSystemType_;
  DatabaseId storageDatabaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &uuidDatabaseId,
                           jobUUID,
                           &entityDatabaseId,
                           scheduleUUID,
                           hostName,
                           userName,
                           archiveType,
                           &entryDatabaseId,
                           &indexType,
                           entryName,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           size,
                           fragmentCount,
                           &storageDatabaseId,
                           storageName,
                           NULL,  // fileSize,
                           NULL,  // imageSize,
                           &fileSystemType_,
                           &blockSize,
                           NULL,  // &directorySize,
                           destinationName,
                           NULL  // hardlinkSize
                          )
     )
  {
    return FALSE;
  }

  assert(fileSystemType_ >= 0);
//TODO: may happen
  if (uuidId         != NULL) (*uuidId        ) = INDEX_ID_UUID(uuidDatabaseId   );
  if (entityId       != NULL) (*entityId      ) = INDEX_ID_ENTITY(entityDatabaseId );
  if (entryId        != NULL) (*entryId       ) = INDEX_ID_(indexType,entryDatabaseId  );
  if (storageId      != NULL) (*storageId     ) = INDEX_ID_STORAGE(storageDatabaseId);
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;

  return TRUE;
}

Errors Index_initListEntryFragments(IndexQueryHandle *indexQueryHandle,
                                    IndexHandle      *indexHandle,
                                    IndexId          entryId,
                                    uint64           offset,
                                    uint64           limit
                                   )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert(   (INDEX_TYPE(entryId) == INDEX_TYPE_FILE)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_IMAGE)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_DIRECTORY)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_LINK)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_HARDLINK)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntryFragments ------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: entryId=%ld\n",__FILE__,__LINE__,entryId);
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entryFragments \
                              LEFT JOIN storages ON storages.id=entryFragments.storageId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entryFragments.id"),
                             DATABASE_COLUMN_KEY     ("storages.id"),
                             DATABASE_COLUMN_STRING  ("storages.name"),
                             DATABASE_COLUMN_DATETIME("storages.created"),
                             DATABASE_COLUMN_UINT64  ("entryFragments.offset"),
                             DATABASE_COLUMN_UINT64  ("entryFragments.size ")
                           ),
                           "    storages.deletedFlag!=TRUE \
                            AND entryFragments.entryId=? \
                           ",
                           DATABASE_FILTERS
                           (
                             DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(entryId))
                           ),
                           NULL,  // groupBy
                           "storages.name,entryFragments.offset ASC",
                           offset,
                           limit
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextEntryFragment(IndexQueryHandle *indexQueryHandle,
                                IndexId          *entryFragmentId,
                                IndexId          *storageId,
                                String           storageName,
                                uint64           *storageDateTime,
                                uint64           *fragmentOffset,
                                uint64           *fragmentSize
                               )
{
  DatabaseId entryFragmentDatabaseId,storageDatabaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &entryFragmentDatabaseId,
                           &storageDatabaseId,
                           storageName,
                           storageDateTime,
                           fragmentOffset,
                           fragmentSize
                          )
     )
  {
    return FALSE;
  }
  if (entryFragmentId != NULL) (*entryFragmentId) = INDEX_ID_STORAGE(entryFragmentDatabaseId);
  if (storageId       != NULL) (*storageId      ) = INDEX_ID_STORAGE(storageDatabaseId);

  return TRUE;
}

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (INDEX_TYPE(entryId) == INDEX_TYPE_FILE)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_IMAGE)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_DIRECTORY)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_LINK)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_HARDLINK)
         || (INDEX_TYPE(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

// TODO: use deleteSubEntry
    switch (INDEX_TYPE(entryId))
    {
      case INDEX_TYPE_FILE:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "fileEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "imageEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "directoryEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      case INDEX_TYPE_LINK:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "linkEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "hardlinkEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_delete(&indexHandle->databaseHandle,
                                NULL,  // changedRowCount
                                "specialEntries",
                                DATABASE_FLAG_NONE,
                                "entryId=?",
                                DATABASE_FILTERS
                                (
                                  DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                                ),
                                DATABASE_UNLIMITED
                               );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entriesNewest",
                            DATABASE_FLAG_NONE,
                            "entryId=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
// TODO: use deleteEntry
    error = Database_delete(&indexHandle->databaseHandle,
                            NULL,  // changedRowCount
                            "entries",
                            DATABASE_FLAG_NONE,
                            "id=?",
                            DATABASE_FILTERS
                            (
                              DATABASE_FILTER_KEY(INDEX_DATABASE_ID(entryId))
                            ),
                            DATABASE_UNLIMITED
                           );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalImageSize<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalDirectoryCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalLinkCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkCount<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalHardlinkSize<0");
      IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalSpecialCount<0");

      IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    entityIds[],
                           uint             entityIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_FILE);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_UINT64  ("entries.size"),
                             DATABASE_COLUMN_UINT64  ("entries.timeModified"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       uint64           *createdDateTime,
                       String           fileName,
                       uint64           *size,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_FILE(databaseId);

  return TRUE;
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    entityIds[],
                            uint             entityIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      name
                           )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_UINT    ("imageEntries.fileSystemType,"),
                             DATABASE_COLUMN_UINT    ("imageEntries.blockSize"),
                             DATABASE_COLUMN_UINT64  ("entries.size")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        IndexId          *indexId,
                        uint64           *createdDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint             *blockSize,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  DatabaseId databaseId;
  int        fileSystemType_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
                           imageName,
                           &fileSystemType_,
                           blockSize,
                           size,
                           blockOffset,
                           blockCount
                          )
     )
  {
    return FALSE;
  }
  assert(fileSystemType_ >= 0);
  if (indexId != NULL) (*indexId) = INDEX_ID_IMAGE(databaseId);
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;

  return TRUE;
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    entityIds[],
                                 uint             entityIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_DATETIME("entries.timeModified"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            uint64           *createdDateTime,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
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
  if (indexId != NULL) (*indexId) = INDEX_ID_DIRECTORY(databaseId);

  return TRUE;
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    entityIds[],
                           uint             entityIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }
  String_appendCString(entryIdsString,"))");

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_STRING  ("linkEntries.destinationName"),
                             DATABASE_COLUMN_UINT64  ("entries.size"),
                             DATABASE_COLUMN_DATETIME("entries.timeModified"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       uint64           *createdDateTime,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
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
  if (indexId != NULL) (*indexId) = INDEX_ID_LINK(databaseId);

  return TRUE;
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    entityIds[],
                               uint             entityIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      name
                              )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entries.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_UINT64  ("entries.size"),
                             DATABASE_COLUMN_DATETIME("entries.timeModified"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           IndexId          *indexId,
                           uint64           *createdDateTime,
                           String           fileName,
                           uint64           *size,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_HARDLINK(databaseId);

  return TRUE;
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    entityIds[],
                             uint             entityIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      name
                            )
{
  String ftsMatchString;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsMatchString = String_new();
  filterString   = Database_newFilter();

  // get FTS match string
  IndexCommon_getFTSMatchString(ftsMatchString,&indexHandle->databaseHandle,"FTS_entries","name",name);

  // get id sets
  entityIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(INDEX_TYPE(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%"PRIi64,INDEX_DATABASE_ID(entityIds[i]));
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (INDEX_TYPE(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%"PRIi64,INDEX_DATABASE_ID(entryIds[i]));
    }
  }

  // get filters
  Database_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  Database_filterAppend(filterString,!String_isEmpty(ftsMatchString),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE %S)",ftsMatchString);
  Database_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  Database_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlString[MAX_SQL_COMMAND_LENGTH];

    return Database_select(&indexQueryHandle->databaseStatementHandle,
                           &indexHandle->databaseHandle,
                           "entries \
                              LEFT JOIN entities ON entities.id=entries.entityId \
                           ",
                           DATABASE_FLAG_NONE,
                           DATABASE_COLUMNS
                           (
                             DATABASE_COLUMN_KEY     ("entries.id"),
                             DATABASE_COLUMN_DATETIME("entities.created"),
                             DATABASE_COLUMN_STRING  ("entries.name"),
                             DATABASE_COLUMN_DATETIME("entries.timeModified"),
                             DATABASE_COLUMN_UINT    ("entries.userId"),
                             DATABASE_COLUMN_UINT    ("entries.groupId"),
                             DATABASE_COLUMN_UINT    ("entries.permission")
                           ),
                           stringFormat(sqlString,sizeof(sqlString),
                                        "    entities.deletedFlag!=TRUE \
                                         AND %s \
                                        ",
                                        String_cString(filterString)
                                       ),
                           DATABASE_FILTERS
                           (
                           ),
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    Database_deleteFilter(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsMatchString);
    return error;
  }

  // free resources
  Database_deleteFilter(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsMatchString);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,IndexQueryHandle);

  return ERROR_NONE;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          IndexId          *indexId,
                          uint64           *createdDateTime,
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

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           &databaseId,
                           createdDateTime,
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
  if (indexId != NULL) (*indexId) = INDEX_ID_SPECIAL(databaseId);

  return TRUE;
}

Errors Index_addFile(IndexHandle *indexHandle,
                     IndexId     uuidId,
                     IndexId     entityId,
                     IndexId     storageId,
                     ConstString name,
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
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry (Note: file entry may already exists wheren there are multiple fragments)
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &databaseId,
                               "entries",
                               "id",
                               "    entityId=? \
                                AND type=? \
                                AND name=? \
                                AND deletedFlag!=TRUE \
                               ",
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(entityId)),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_FILE),
                                 DATABASE_FILTER_STRING(name)
                               )
                              );
        if ((error == ERROR_NONE) && (databaseId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  &databaseId,
                                  "entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                    DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_FILE),
                                    DATABASE_VALUE_STRING  ("name",            name),
                                    DATABASE_VALUE_DATETIME("timeLastAccess",  timeLastAccess),
                                    DATABASE_VALUE_DATETIME("timeModified",    timeModified),
                                    DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                    DATABASE_VALUE_UINT    ("userId",          userId),
                                    DATABASE_VALUE_UINT    ("groupId",         groupId),
                                    DATABASE_VALUE_UINT    ("permission",      permission),

                                    DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                    DATABASE_VALUE_UINT64  ("size",            size)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );
          entryId = INDEX_ID_ENTRY(databaseId);

          // add FTS entry
// TODO: do this with a trigger again?
          if (error == ERROR_NONE)
          {
            switch (Database_getType(&indexHandle->databaseHandle))
            {
              case DATABASE_TYPE_SQLITE3:
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // insertRowId
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                          DATABASE_VALUE_STRING("name",    name)
                                        ),
                                        DATABASE_COLUMNS_NONE,
                                        DATABASE_FILTERS_NONE
                                       );
                break;
              case DATABASE_TYPE_MARIADB:
                break;
              case DATABASE_TYPE_POSTGRESQL:
                {
                  String tokens;

                  tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),name);
                  error = Database_insert(&indexHandle->databaseHandle,
                                          NULL,  // insertRowId
                                          "FTS_entries",
                                          DATABASE_FLAG_NONE,
                                          DATABASE_VALUES
                                          (
                                            DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                            DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                          ),
                                          DATABASE_COLUMNS_NONE,
                                          DATABASE_FILTERS_NONE
                                         );
                  String_delete(tokens);
                }
                break;
            }
          }

          // add file entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "fileEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_UINT64("size",    size)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
          }
        }
        else
        {
          entryId = INDEX_ID_ENTRY(databaseId);
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add file entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",   INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId", INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_UINT64("offset",    fragmentOffset),
                                DATABASE_VALUE_UINT64("size",      fragmentSize)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               INDEX_DATABASE_ID(storageId),
                                               INDEX_DATABASE_ID(entryId),
                                               name,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");

        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_FILE uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIu64" name=%'S size=%"PRIu64" timeLastAccess=%"PRIu64" timeModified=%"PRIu64" timeLastChanged=%"PRIu64" userId=%u groupId=%u permission=%o fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64,
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
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
  }

  return error;
}

Errors Index_addImage(IndexHandle     *indexHandle,
                      IndexId         uuidId,
                      IndexId         entityId,
                      IndexId         storageId,
                      ConstString     name,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      uint            blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors     error;
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry (Note: image entry may already exists wheren there are multiple fragments)
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &databaseId,
                               "entries",
                               "id",
                               "    entityId=? \
                                AND type=? \
                                AND name=? \
                                AND deletedFlag!=TRUE \
                               ",
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(entityId)),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_IMAGE),
                                 DATABASE_FILTER_STRING(name)
                               )
                              );
        if ((error == ERROR_NONE) && (databaseId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  &databaseId,
                                  "entries",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                    DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_IMAGE),
                                    DATABASE_VALUE_STRING  ("name",            name),
                                    DATABASE_VALUE_DATETIME("timeLastAccess",  0LL),
                                    DATABASE_VALUE_DATETIME("timeModified",    0LL),
                                    DATABASE_VALUE_DATETIME("timeLastChanged", 0LL),
                                    DATABASE_VALUE_UINT    ("userId",          0),
                                    DATABASE_VALUE_UINT    ("groupId",         0),
                                    DATABASE_VALUE_UINT    ("permission",      0),

                                    DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                    DATABASE_VALUE_UINT64  ("size",            size)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );

          // add FTS entry
          if (error == ERROR_NONE)
          {
            switch (Database_getType(&indexHandle->databaseHandle))
            {
              case DATABASE_TYPE_SQLITE3:
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // insertRowId
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                          DATABASE_VALUE_STRING("name",    name)
                                        ),
                                        DATABASE_COLUMNS_NONE,
                                        DATABASE_FILTERS_NONE
                                       );
                break;
              case DATABASE_TYPE_MARIADB:
                break;
              case DATABASE_TYPE_POSTGRESQL:
                {
                  String tokens;

                  tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),name);
                  error = Database_insert(&indexHandle->databaseHandle,
                                          NULL,  // insertRowId
                                          "FTS_entries",
                                          DATABASE_FLAG_NONE,
                                          DATABASE_VALUES
                                          (
                                            DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                            DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                          ),
                                          DATABASE_COLUMNS_NONE,
                                          DATABASE_FILTERS_NONE
                                         );
                  String_delete(tokens);
                }
                break;
            }
          }

          // add image entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "imageEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId",        INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_UINT  ("fileSystemType", fileSystemType),
                                      DATABASE_VALUE_UINT64("size",           size),
                                      DATABASE_VALUE_UINT  ("blockSize",      blockSize)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
          }
        }
        else
        {
          entryId = INDEX_ID_ENTRY(databaseId);
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add image entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",   INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId", INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_UINT64("offset",    blockOffset*(uint64)blockSize),
                                DATABASE_VALUE_UINT64("size",      blockCount*(uint64)blockSize)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%"PRIi64" AND totalEntrySize<0",INDEX_DATABASE_ID(storageId));
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%"PRIi64" AND totalFileSize<0",INDEX_DATABASE_ID(storageId));
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_IMAGE uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIu64" type=IMAGE name=%'S fileSystemType=%'s size=%"PRIu64" blockSize=%lu blockOffset=%"PRIu64" blockCount=%"PRIu64,
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    FileSystem_fileSystemTypeToString(fileSystemType,"unknown"),
                                    size,
                                    blockSize,
                                    blockOffset,
                                    blockCount
                                   );
  }

  return error;
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          IndexId     uuidId,
                          IndexId     entityId,
                          IndexId     storageId,
                          String      name,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  Errors     error;
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_insert(&indexHandle->databaseHandle,
                              &databaseId,
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_DIRECTORY),
                                DATABASE_VALUE_STRING  ("name",            name),
                                DATABASE_VALUE_DATETIME("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_DATETIME("timeModified",    timeModified),
                                DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT    ("userId",          userId),
                                DATABASE_VALUE_UINT    ("groupId",         groupId),
                                DATABASE_VALUE_UINT    ("permission",      permission),

                                DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                DATABASE_VALUE_UINT64  ("size",            0)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(databaseId);

      // add FTS entry
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // insertRowId
                                  "FTS_entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                    DATABASE_VALUE_STRING("name",    name)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );
          break;
        case DATABASE_TYPE_MARIADB:
          break;
        case DATABASE_TYPE_POSTGRESQL:
          {
            String tokens;

            tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),name);
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "FTS_entries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
            String_delete(tokens);
          }
          break;
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add directory entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "directoryEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",   INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId", INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_STRING("name",      name)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               INDEX_DATABASE_ID(storageId),
                                               INDEX_DATABASE_ID(entryId),
                                               name,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalDirectoryCount<0");

        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_DIRECTORY uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIu64" type=DIRECTORY name=%'S timeLastAccess=%"PRIu64" timeModified=%"PRIu64" timeLastChanged=%"PRIu64" userId=%u groupId=%u permission=%o",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission
                                   );
  }

  return error;
}

Errors Index_addLink(IndexHandle *indexHandle,
                     IndexId     uuidId,
                     IndexId     entityId,
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
  Errors     error;
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_insert(&indexHandle->databaseHandle,
                              &databaseId,
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_LINK),
                                DATABASE_VALUE_STRING  ("name",            linkName),
                                DATABASE_VALUE_DATETIME("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_DATETIME("timeModified",    timeModified),
                                DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT    ("userId",          userId),
                                DATABASE_VALUE_UINT    ("groupId",         groupId),
                                DATABASE_VALUE_UINT    ("permission",      permission),

                                DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                DATABASE_VALUE_UINT64  ("size",            0)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(databaseId);

      // add FTS entry
// TODO: do this in a trigger again?
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // insertRowId
                                  "FTS_entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                    DATABASE_VALUE_STRING("name",    linkName)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );
          break;
        case DATABASE_TYPE_MARIADB:
          break;
        case DATABASE_TYPE_POSTGRESQL:
          {
            String tokens;

            tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),linkName);
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "FTS_entries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
            String_delete(tokens);
          }
          break;
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add link entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "linkEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",         INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId",       INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_STRING("destinationName", destinationName)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               INDEX_DATABASE_ID(storageId),
                                               INDEX_DATABASE_ID(entryId),
                                               linkName,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_LINK uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIi64" type=LINK name=%'S destinationName=%'S timeLastAccess=%"PRIu64" timeModified=%"PRIu64" timeLastChanged=%"PRIu64" userId=%u groupId=%u permission=%o",
                                    uuidId,
                                    entityId,
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
  }

  return error;
}

Errors Index_addHardlink(IndexHandle *indexHandle,
                         IndexId     uuidId,
                         IndexId     entityId,
                         IndexId     storageId,
                         ConstString name,
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
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // atomic add/get entry (Note: hardlink entry may already exists wheren there are multiple fragments)
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &databaseId,
                               "entries",
                               "id",
                               "    entityId=? \
                                AND type=? \
                                AND name=? \
                                AND deletedFlag!=TRUE \
                               ",
                               DATABASE_FILTERS
                               (
                                 DATABASE_FILTER_KEY   (INDEX_DATABASE_ID(entityId)),
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_HARDLINK),
                                 DATABASE_FILTER_STRING(name),
                               )
                              );
        if ((error == ERROR_NONE) && (databaseId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  &databaseId,
                                  "entries",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                    DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_HARDLINK),
                                    DATABASE_VALUE_STRING  ("name",            name),
                                    DATABASE_VALUE_DATETIME("timeLastAccess",  timeLastAccess),
                                    DATABASE_VALUE_DATETIME("timeModified",    timeModified),
                                    DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                    DATABASE_VALUE_UINT    ("userId",          userId),
                                    DATABASE_VALUE_UINT    ("groupId",         groupId),
                                    DATABASE_VALUE_UINT    ("permission",      permission),

                                    DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                    DATABASE_VALUE_UINT64  ("size",            size)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );
          entryId = INDEX_ID_ENTRY(databaseId);

          // add FTS entry
          if (error == ERROR_NONE)
          {
  // TODO: do this in a trigger again?
            switch (Database_getType(&indexHandle->databaseHandle))
            {
              case DATABASE_TYPE_SQLITE3:
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // insertRowId
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                          DATABASE_VALUE_STRING("name",    name)
                                        ),
                                        DATABASE_COLUMNS_NONE,
                                        DATABASE_FILTERS_NONE
                                       );
                break;
              case DATABASE_TYPE_MARIADB:
                break;
              case DATABASE_TYPE_POSTGRESQL:
                {
                  String tokens;

                  tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),name);
                  error = Database_insert(&indexHandle->databaseHandle,
                                          NULL,  // insertRowId
                                          "FTS_entries",
                                          DATABASE_FLAG_NONE,
                                          DATABASE_VALUES
                                          (
                                            DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                            DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                          ),
                                          DATABASE_COLUMNS_NONE,
                                          DATABASE_FILTERS_NONE
                                         );
                  String_delete(tokens);
                }
                break;
            }
          }

          // add hard link entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "hardlinkEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_UINT64("size",    size)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
          }
        }
        else
        {
          entryId = INDEX_ID_ENTRY(databaseId);
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add hard link entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",   INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId", INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_UINT64("offset",    fragmentOffset),
                                DATABASE_VALUE_UINT64("size",      fragmentSize)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               INDEX_DATABASE_ID(storageId),
                                               INDEX_DATABASE_ID(entryId),
                                               name,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalEntrySize<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileCount<0");
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE totalFileSize<0");

        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        IndexCommon_verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_HARDLINK uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIi64" type=HARDLINK name=%'S size=%"PRIu64" timeLastAccess=%"PRIu64" timeModified=%"PRIu64" timeLastChanged=%"PRIu64" userId=%u groupId=%u permission=%o fragmentOffset=%"PRIu64" fragmentSize=%"PRIu64,
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
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
  }

  return error;
}

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        IndexId          uuidId,
                        IndexId          entityId,
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
  Errors     error;
  DatabaseId databaseId;
  IndexId    entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY));
  assert(INDEX_TYPE(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_insert(&indexHandle->databaseHandle,
                              &databaseId,
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY     ("entityId",        INDEX_DATABASE_ID(entityId)),
                                DATABASE_VALUE_UINT    ("type",            INDEX_TYPE_SPECIAL),
                                DATABASE_VALUE_STRING  ("name",            name),
                                DATABASE_VALUE_DATETIME("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_DATETIME("timeModified",    timeModified),
                                DATABASE_VALUE_DATETIME("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT    ("userId",          userId),
                                DATABASE_VALUE_UINT    ("groupId",         groupId),
                                DATABASE_VALUE_UINT    ("permission",      permission),

                                DATABASE_VALUE_KEY     ("uuidId",          INDEX_DATABASE_ID(uuidId)),
                                DATABASE_VALUE_UINT64  ("size",            0)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(databaseId);

      // add FTS entry
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // insertRowId
                                  "FTS_entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES
                                  (
                                    DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                    DATABASE_VALUE_STRING("name",    name)
                                  ),
                                  DATABASE_COLUMNS_NONE,
                                  DATABASE_FILTERS_NONE
                                 );
          break;
        case DATABASE_TYPE_MARIADB:
          break;
        case DATABASE_TYPE_POSTGRESQL:
          {
            String tokens;

            tokens = IndexCommon_getPostgreSQLFTSTokens(String_new(),name);
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // insertRowId
                                    "FTS_entries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", INDEX_DATABASE_ID(entryId)),
                                      DATABASE_VALUE_STRING("name",    "to_tsvector(?)", tokens)
                                    ),
                                    DATABASE_COLUMNS_NONE,
                                    DATABASE_FILTERS_NONE
                                   );
            String_delete(tokens);
          }
          break;
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add special entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // insertRowId
                              "specialEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES
                              (
                                DATABASE_VALUE_KEY   ("entryId",     INDEX_DATABASE_ID(entryId)),
                                DATABASE_VALUE_KEY   ("storageId",   INDEX_DATABASE_ID(storageId)),
                                DATABASE_VALUE_UINT  ("specialType", specialType),
                                DATABASE_VALUE_UINT  ("major",       major),
                                DATABASE_VALUE_UINT  ("minor",       minor)
                              ),
                              DATABASE_COLUMNS_NONE,
                              DATABASE_FILTERS_NONE
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               INDEX_DATABASE_ID(storageId),
                                               INDEX_DATABASE_ID(entryId),
                                               name,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_DEBUG_LEVEL,
                                    SERVER_IO_TIMEOUT,
                                    CALLBACK_(NULL,NULL),  // commandResultFunction
                                    "INDEX_ADD_SPECIAL uuidId=%"PRIi64" entityId=%"PRIi64" storageId=%"PRIi64" type=SPECIAL name=%'S specialType=%s timeLastAccess=%"PRIu64" timeModified=%"PRIu64" timeLastChanged=%"PRIu64" userId=%u groupId=%u permission=%o major=%u minor=%u",
                                    uuidId,
                                    entityId,
                                    storageId,
                                    name,
                                    File_fileSpecialTypeToString(specialType,""),
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    major,
                                    minor
                                   );
  }

  return error;
}

Errors Index_initListSkippedEntry(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  uint             indexIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  IndexTypes       indexType,
                                  ConstString      name,
                                  DatabaseOrdering ordering,
                                  uint64           offset,
                                  uint64           limit
                                 )
{
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(indexIds);
  UNUSED_VARIABLE(indexIdCount);
  UNUSED_VARIABLE(entryIds);
  UNUSED_VARIABLE(entryIdCount);
  UNUSED_VARIABLE(indexType);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(ordering);
  UNUSED_VARIABLE(offset);
  UNUSED_VARIABLE(limit);

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->masterIO == NULL);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

bool Index_getNextSkippedEntry(IndexQueryHandle *indexQueryHandle,
                               IndexId          *uuidId,
                               String           jobUUID,
                               IndexId          *entityId,
                               String           scheduleUUID,
                               ArchiveTypes     *archiveType,
                               IndexId          *storageId,
                               String           storageName,
                               uint64           *storageDateTime,
                               IndexId          *entryId,
                               String           entryName
                              )
{
//TODO
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(uuidId);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(scheduleUUID);
  UNUSED_VARIABLE(archiveType);
  UNUSED_VARIABLE(storageId);
  UNUSED_VARIABLE(storageName);
  UNUSED_VARIABLE(storageDateTime);
  UNUSED_VARIABLE(entryId);
  UNUSED_VARIABLE(entryName);

  return FALSE;
}

Errors Index_addSkippedEntry(IndexHandle *indexHandle,
                             IndexId     entityId,
                             IndexTypes  indexType,
                             ConstString entryName
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(INDEX_TYPE(entityId) == INDEX_TYPE_ENTITY);
  assert(   (indexType == INDEX_TYPE_FILE           )
         || (indexType == INDEX_CONST_TYPE_IMAGE    )
         || (indexType == INDEX_CONST_TYPE_DIRECTORY)
         || (indexType == INDEX_CONST_TYPE_LINK     )
         || (indexType == INDEX_CONST_TYPE_HARDLINK )
         || (indexType == INDEX_CONST_TYPE_SPECIAL  )
        );
  assert(entryName != NULL);

  // check init errorindexId
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_insert(&indexHandle->databaseHandle,
                            NULL,  // insertRowId
                            "",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES
                            (
                              DATABASE_VALUE_KEY   ("entityId", INDEX_DATABASE_ID(entityId)),
                              DATABASE_VALUE_UINT  ("type",     indexType),
                              DATABASE_VALUE_STRING("name",     entryName)
                            ),
                            DATABASE_COLUMNS_NONE,
                            DATABASE_FILTERS_NONE
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_deleteSkippedEntry(IndexHandle *indexHandle,
                                IndexId     indexId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (INDEX_TYPE(indexId) == INDEX_TYPE_FILE           )
         || (INDEX_TYPE(indexId) == INDEX_CONST_TYPE_IMAGE    )
         || (INDEX_TYPE(indexId) == INDEX_CONST_TYPE_DIRECTORY)
         || (INDEX_TYPE(indexId) == INDEX_CONST_TYPE_LINK     )
         || (INDEX_TYPE(indexId) == INDEX_CONST_TYPE_HARDLINK )
         || (INDEX_TYPE(indexId) == INDEX_CONST_TYPE_SPECIAL  )
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_delete(&indexHandle->databaseHandle,
                             NULL,  // changedRowCount
                             "skippedEntries",
                             DATABASE_FLAG_NONE,
                             "id=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(INDEX_DATABASE_ID(indexId))
                             ),
                             DATABASE_UNLIMITED
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
