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

#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"
#include "bar.h"
#include "bar_global.h"

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
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM FTS_entries \
                              WHERE entryId=%lld \
                             ",
                             entryId
                            );
  }

  // update newest entries
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entriesNewest \
                              WHERE entryId=%lld \
                             ",
                             entryId
                            );
//TODO
#if 0
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "INSERT OR IGNORE INTO entriesNewest \
                                ( \
                                 uuidId,\
                                 entityId, \
                                 type, \
                                 name, \
                                 timeLastAccess, \
                                 timeModified, \
                                 timeLastChanged, \
                                 userId, \
                                 groupId, \
                                 permission \
                                ) \
                              SELECT \
                                uuidId,\
                                entityId, \
                                type, \
                                name, \
                                timeLastAccess, \
                                timeModified, \
                                timeLastChanged, \
                                userId, \
                                groupId, \
                                permission \
                              FROM entries \
                              WHERE id!=%lld AND name=%'S \
                              ORDER BY timeModified DESC \
                              LIMIT 0,1 \
                              WHERE name \
                             ",
                             entryId,
                             name
                            );
#endif
  }

  // delete entry
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entries \
                              WHERE id=%lld \
                             ",
                             entryId
                            );
  }

  // free resources

  return error;
}
#endif

#if 0
//TODO: still not used
/***********************************************************************\
* Name   : pruneEntries
* Purpose: prune all entries (file, image, hardlink) with no fragments
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneEntries(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter
                         )
{
  Array         databaseIds;
  Errors        error;
  ArrayIterator arrayIterator;
  DatabaseId    databaseId;

  assert(indexHandle != NULL);
  assert(doneFlag != NULL);
  assert(deletedCounter != NULL);

  // init vairables
  Array_init(&databaseIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // get all file entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "fileEntries",
                          "fileEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge file entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !indexQuitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "fileEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all image entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "imageEntries",
                          "imageEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge image entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !indexQuitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "imageEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // get all hardlink entries without fragments
  Array_clear(&databaseIds);
  error = Database_getIds(&indexHandle->databaseHandle,
                          &databaseIds,
                          "hardlinkEntries",
                          "hardlinkEntries.id",
                          "LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.id \
                           WHERE entryFragments.id IS NULL \
                          "
                         );
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // purge hardlink entries without fragments
  ARRAY_ITERATEX(&databaseIds,arrayIterator,databaseId,error == ERROR_NONE && (*doneFlag) && !indexQuitFlag)
  {
    error = purge(indexHandle,
                  doneFlag,
                  deletedCounter,
                  "hardlinkEntries",
                  "id=%lld",
                  databaseId
                 );
  }
  if (error != ERROR_NONE)
  {
    Array_done(&databaseIds);
    return error;
  }

  // free resources
  Array_done(&databaseIds);

  return ERROR_NONE;
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
//l=0;Database_getInteger64(&indexHandle->databaseHandle,&l,"entryFragments","count(id)","WHERE storageId=%lld",storageId);fprintf(stderr,"%s, %lld: fragments %d: %lld\n",__FILE__,__LINE__,storageId,l);
        doneFlag = TRUE;
        error = IndexCommon_purge(indexHandle,
                                  &doneFlag,
                                  #ifndef NDEBUG
                                    &deletedCounter,
                                  #else
                                    NULL,  // deletedCounter
                                  #endif
                                  "entryFragments",
                                  "storageId=%lld",
                                  storageId
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
    fprintf(stderr,"%s, %d: error: %s, purged fragments: %llums\n",__FILE__,__LINE__,
            Error_getText(error),
            (Misc_getTimestamp()-t0)/US_PER_MS
           );
  #endif

  return error;
}

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
    // atomic add/get entry
    DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // get existing newest entry
      error = Database_prepare(&databaseStatementHandle,
                               &indexHandle->databaseHandle,
                               DATABASE_COLUMN_TYPES(KEY,UINT64),
                               "SELECT id, \
                                       UNIX_TIMESTAMP(timeLastChanged) \
                                FROM entriesNewest\
                                WHERE name=? \
                               ",
                               DATABASE_VALUES2
                               (
                               ),
                               DATABASE_FILTERS
                               (
                                 STRING(name)
                               )
                              );
      if (error == ERROR_NONE)
      {
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lld %llu",
                                 &newestEntryId,
                                 &newestTimeLastChanged
                                )
           )
        {
          newestEntryId         = DATABASE_ID_NONE;
          newestTimeLastChanged = 0LL;
        }
        Database_finalize(&databaseStatementHandle);
      }

      // insert/update newest
      if (error == ERROR_NONE)
      {
        if (timeLastChanged > newestTimeLastChanged)
        {
          if (newestEntryId != DATABASE_ID_NONE)
          {
            // update
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK_(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     DATABASE_COLUMN_TYPES(),
                                     "UPDATE entriesNewest \
                                      SET entryId=%lld, \
                                          uuidId=%lld, \
                                          entityId=%lld, \
                                          type=%u, \
                                          timeLastChanged=%lld, \
                                          userId=%u, \
                                          groupId=%u, \
                                          permission=%u \
                                      WHERE id=%lld \
                                     ",
                                     entryId,
                                     uuidId,
                                     entityId,
                                     indexType,
                                     timeLastChanged,
                                     userId,
                                     groupId,
                                     permission,
                                     newestEntryId
                                    );
          }
          else
          {
            // insert
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "entriesNewest",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      KEY   ("entryId",         entryId),
                                      KEY   ("uuidId",          uuidId),
                                      KEY   ("entityId",        entityId),
                                      UINT  ("type",            indexType),
                                      STRING("name",            name),
                                      UINT64("timeLastChanged", timeLastChanged),
                                      UINT  ("userId",          userId),
                                      UINT  ("groupId",         groupId),
                                      UINT  ("permission",      permission)
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
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entriesNewest \
                              WHERE id=%lld \
                             ",
                             entryId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "INSERT OR IGNORE INTO entriesNewest \
                                ( \
                                 entryId, \
                                 uuidId, \
                                 entityId, \
                                 type, \
                                 name, \
                                 timeLastChanged, \
                                 userId, \
                                 groupId, \
                                 permission, \
                                ) \
                              SELECT id, \
                                     uuidId, \
                                     entityId, \
                                     type, \
                                     name, \
                                     timeLastChanged, \
                                     userId, \
                                     groupId, \
                                     permission \
                               FROM entries \
                               WHERE name=%'S \
                               ORDER BY timeLastChanged DESC \
                               LIMIT 0,1 \
                             ",
                             name
                            );
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
                         "WHERE entryId=%lld",
                         entryId
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  directoryName = File_getDirectoryName(String_new(),fileName);
  error = ERROR_NONE;
  while ((error == ERROR_NONE) && !String_isEmpty(directoryName))
  {
//fprintf(stderr,"%s, %d: directoryName=%s %llu\n",__FILE__,__LINE__,String_cString(directoryName),size);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "UPDATE directoryEntries \
                              SET totalEntryCount=totalEntryCount+1, \
                                  totalEntrySize =totalEntrySize +%llu \
                              WHERE     storageId=%lld \
                                    AND name=%'S \
                             ",
                             size,
                             storageId,
                             directoryName
                            );
    if (error != ERROR_NONE)
    {
      break;
    }

    if (databaseId != DATABASE_ID_NONE)
    {
// TODO:
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               DATABASE_COLUMN_TYPES(),
                               "UPDATE directoryEntries \
                                SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                    totalEntrySizeNewest =totalEntrySizeNewest +%llu \
                                WHERE     storageId=%lld \
                                      AND name=%'S \
                               ",
                               size,
                               storageId,
                               directoryName
                              );
      if (error != ERROR_NONE)
      {
        break;
      }
    }

    File_getDirectoryName(directoryName,directoryName);
  }
  String_delete(directoryName);

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_collectIds
* Purpose: collect entry ids for storage
* Input  : entryIds     - entry ids array variable
*          indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : entryIds     - entry ids array
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_collectIds(Array        *entryIds,
                             IndexHandle  *indexHandle,
                             DatabaseId   storageId,
                             ProgressInfo *progressInfo
                            )
{
  Errors error;
  #ifdef INDEX_DEBUG_PURGE
    uint64             t0,dt[10];
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
    // get file/image/hardlink entry ids to check for purge
    INDEX_DOX(error,
              indexHandle,
    {
      return Database_getIds(&indexHandle->databaseHandle,
                             entryIds,
                             "entryFragments",
                             "entryId",
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[0] = Misc_getTimestamp()-t0;
  #endif

  // collect directory/link/special entries to purge
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
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[1] = Misc_getTimestamp()-t0;
  #endif
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
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[2] = Misc_getTimestamp()-t0;
  #endif
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
                             "WHERE storageId=%lld",
                             storageId
                            );
    });
  }
  #ifdef INDEX_DEBUG_PURGE
    dt[3] = Misc_getTimestamp()-t0;
  #endif
  #ifdef INDEX_DEBUG_PURGE
    fprintf(stderr,"%s, %d: error: %s, %lu entries to purge: fragment %llums, directory %llums, link %llums, special %llums\n",__FILE__,__LINE__,
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

// ----------------------------------------------------------------------

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            ulong         indexIdCount,
                            const IndexId entryIds[],
                            ulong         entryIdCount,
                            IndexTypes    indexType,
                            ConstString   name,
                            bool          newestOnly,
                            ulong         *totalStorageCount,
                            uint64        *totalStorageSize,
                            ulong         *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           )
{
  DatabaseStatementHandle databaseStatementHandle;
  Errors              error;
  String              ftsName;
  String              uuidIdsString,entityIdsString;
  String              entryIdsString;
  ulong               i;
  String              filterString;
  int64               totalStorageCount_,totalEntryCount_;
  double              totalStorageSize_,totalEntrySize_,totalEntryContentSize_;
  #ifdef INDEX_DEBUG_LIST_INFO
    uint64              t0,t1;
  #endif

  assert(indexHandle != NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName         = String_new();
  uuidIdsString   = String_new();
  entityIdsString = String_new();
  entryIdsString  = String_new();
  filterString    = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
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
    String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_getEntriesInfo --------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: entryIdsString=%s\n",__FILE__,__LINE__,String_cString(entryIdsString));
    fprintf(stderr,"%s, %d: ftsName=%s\n",__FILE__,__LINE__,String_cString(ftsName));
  #endif /* INDEX_DEBUG_LIST_INFO */

  // get filters
  String_setCString(filterString,"1");
  IndexCommon_filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  error = ERROR_NONE;
  if (String_isEmpty(ftsName))
  {
    // no names
    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalEntryCountNewest)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalEntrySizeNewest)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalFileCountNewest)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalFileSizeNewest)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalImageCountNewest)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalImageSizeNewest)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalDirectoryCountNewest)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalLinkCountNewest)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalHardlinkCountNewest)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalHardlinkSizeNewest)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalSpecialCountNewest)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                 "entriesNewest \
                                    LEFT JOIN entities ON entities.id=entriesNewest.entityId \
                                    LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                 ",
                                 DATABASE_COLUMNS
                                 (
                                   DATABASE_COLUMN_UINT  ("0"),
                                   DATABASE_COLUMN_UINT64("0"),
                                   DATABASE_COLUMN_UINT  ("COUNT(entriesNewest.id)"),
                                   DATABASE_COLUMN_UINT64("SUM(entriesNewest.size)")
                                 ),
                                 stringFormat(sqlCommand,sizeof(sqlCommand),
                                              "    %s \
                                               AND entities.deletedFlag!=1 \
                                              ",
                                              String_cString(filterString)
                                             ),
                                 DATABASE_FILTERS
                                 (
                                 )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalEntryCount)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalEntrySize)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalFileCount)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalFileSize)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalImageCount)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalImageSize)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalDirectoryCount)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalLinkCount)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalHardlinkCount)"),
                                       DATABASE_COLUMN_UINT64("SUM(entities.totalHardlinkSize)")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                     "entities \
                                        LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                     ",
                                     DATABASE_COLUMNS
                                     (
                                       DATABASE_COLUMN_UINT  ("0"),
                                       DATABASE_COLUMN_UINT64("0"),
                                       DATABASE_COLUMN_UINT  ("SUM(entities.totalSpecialCount)"),
                                       DATABASE_COLUMN_UINT64("0")
                                     ),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "    %s \
                                                   AND entities.deletedFlag!=1 \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_FILTERS
                                     (
                                     )
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
                                 "entries \
                                    LEFT JOIN entities ON entities.id=entries.entityId \
                                    LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                 ",
                                 DATABASE_COLUMNS
                                 (
                                   DATABASE_COLUMN_UINT  ("0"),
                                   DATABASE_COLUMN_UINT64("0"),
                                   DATABASE_COLUMN_UINT  ("COUNT(entries.id)"),
                                   DATABASE_COLUMN_UINT64("SUM(entries.size)")
                                 ),
                                 stringFormat(sqlCommand,sizeof(sqlCommand),
                                              "    %s \
                                               AND entities.deletedFlag!=1 \
                                              ",
                                              String_cString(filterString)
                                             ),
                                 DATABASE_FILTERS
                                 (
                                 )
                                );
          }
        }
        if (error != ERROR_NONE)
        {
          return error;
        }

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalStorageCount_=%"PRIi64" totalStorageSize_=%lf totalEntryCount_=%"PRIi64" totalEntrySize_=%lf\n",__FILE__,__LINE__,totalStorageCount_,totalStorageSize_,totalEntryCount_,totalEntrySize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        // get entry content size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT),
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "SELECT SUM(directoryEntries.totalEntrySize) \
                                                 FROM entriesNewest \
                                                   LEFT JOIN entryFragments   ON entryFragments.entryId=entriesNewest.entryId \
                                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                                   LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                                   LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.entryId \
                                                   LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                 WHERE     entities.deletedFlag!=1 \
                                                       AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_VALUES2
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                   )
                                  );
        }
        else
        {
          if (String_isEmpty(entryIdsString))
          {
            // no storages selected, no entries selected -> get aggregated data from entities
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "SELECT SUM(entities.totalEntrySize) \
                                                   FROM entities \
                                                     LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                                   WHERE     entities.deletedFlag!=1 \
                                                         AND %s \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_VALUES2
                                     (
                                     ),
                                     DATABASE_FILTERS
                                     (
                                     )
                                    );
          }
          else
          {
            // entries selected -> get aggregated data from entries
            error = Database_prepare(&databaseStatementHandle,
                                     &indexHandle->databaseHandle,
                                     DATABASE_COLUMN_TYPES(INT64),
                                     stringFormat(sqlCommand,sizeof(sqlCommand),
                                                  "SELECT SUM(directoryEntries.totalEntrySize) \
                                                   FROM entries \
                                                     LEFT JOIN entryFragments   ON entryFragments.entryId=entries.id \
                                                     LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                                     LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                                     LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                                     LEFT JOIN entities         ON entities.id=entries.entityId \
                                                     LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                   WHERE     entities.deletedFlag!=1 \
                                                         AND %s \
                                                  ",
                                                  String_cString(filterString)
                                                 ),
                                     DATABASE_VALUES2
                                     (
                                     ),
                                     DATABASE_FILTERS
                                     (
                                     )
                                    );
          }
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lf",
                                 &totalEntryContentSize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries content size");
        }
//TODO: may happend?
//        assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryContentSize_=%lf\n",__FILE__,__LINE__,totalEntryContentSize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names selected

    // get filters
    IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH '%S'",ftsName);
    if (newestOnly)
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      IndexCommon_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
    }
    else
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      IndexCommon_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        // get entry count, entry size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT,INT64),
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "SELECT COUNT(entriesNewest.id), \
                                                        SUM(entriesNewest.size) \
                                                 FROM FTS_entries \
                                                   LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                                   LEFT JOIN entities      ON entities.id=entriesNewest.entityId \
                                                   LEFT JOIN uuids         ON uuids.jobUUID=entities.jobUUID \
                                                 WHERE     entities.deletedFlag!=1 \
                                                       AND entriesNewest.id IS NOT NULL \
                                                       AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_VALUES2
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                   )
                                  );
        }
        else
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT,INT64),
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "SELECT COUNT(entries.id), \
                                                        SUM(entries.size) \
                                                 FROM FTS_entries \
                                                   LEFT JOIN entries  ON entries.id=FTS_entries.entryId \
                                                   LEFT JOIN entities ON entities.id=entries.entityId \
                                                   LEFT JOIN uuids    ON uuids.jobUUID=entities.jobUUID \
                                                 WHERE     entities.deletedFlag!=1 \
                                                       AND entries.id IS NOT NULL \
                                                       AND %s \
                                                ",
                                                String_cString(filterString)
                                              ),
                                   DATABASE_VALUES2
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                   )
                                  );
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lld %lf",
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries count/size");
        }
        assert(totalEntrySize_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
        if (totalEntrySize  != NULL) (*totalEntrySize ) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryCount_=%"PRIi64" totalEntrySize_=%lf\n",__FILE__,__LINE__,totalEntryCount_,totalEntrySize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }

    if (error == ERROR_NONE)
    {
      #ifdef INDEX_DEBUG_LIST_INFO
        t0 = Misc_getTimestamp();
      #endif /* INDEX_DEBUG_LIST_INFO */

      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

        // get entry content size
        if (newestOnly)
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT64),
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "SELECT SUM(directoryEntries.totalEntrySize) \
                                                 FROM FTS_entries \
                                                   LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                                   LEFT JOIN entries          ON entries.id=entriesNewest.entryId \
                                                   LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                                   LEFT JOIN entities         ON entities.id=storages.entityId \
                                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                 WHERE     storages.deletedFlag!=1 \
                                                       AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_VALUES2
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                   )
                                  );
        }
        else
        {
          error = Database_prepare(&databaseStatementHandle,
                                   &indexHandle->databaseHandle,
                                   DATABASE_COLUMN_TYPES(INT64),
                                   stringFormat(sqlCommand,sizeof(sqlCommand),
                                                "SELECT SUM(directoryEntries.totalEntrySize) \
                                                 FROM FTS_entries \
                                                   LEFT JOIN directoryEntries ON directoryEntries.entryId=FTS_entries.entryId \
                                                   LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                                   LEFT JOIN storages         ON storages.id=directoryEntries.storageId \
                                                   LEFT JOIN entities         ON entities.id=storages.entityId \
                                                   LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                 WHERE     storages.deletedFlag!=1 \
                                                       AND %s \
                                                ",
                                                String_cString(filterString)
                                               ),
                                   DATABASE_VALUES2
                                   (
                                   ),
                                   DATABASE_FILTERS
                                   (
                                   )
                                  );
        }
        if (error != ERROR_NONE)
        {
          return error;
        }
        #ifdef INDEX_DEBUG_LIST_INFO
          Database_debugPrintQueryInfo(&databaseStatementHandle);
        #endif
        if (!Database_getNextRow(&databaseStatementHandle,
                                 "%lf",
                                 &totalEntryContentSize_
                                )
           )
        {
          Database_finalize(&databaseStatementHandle);
          return ERRORX_(DATABASE,0,"get entries content size");
        }
//TODO: may happend?
//        assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
        Database_finalize(&databaseStatementHandle);

        return ERROR_NONE;
      });

      #ifdef INDEX_DEBUG_LIST_INFO
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: totalEntryContentSize_=%lf\n",__FILE__,__LINE__,totalEntryContentSize_);
        fprintf(stderr,"%s, %d: time=%"PRIu64"us\n",__FILE__,__LINE__,(t1-t0));
        fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
      #endif
    }
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

  return error;
}

Errors Index_initListEntries(IndexQueryHandle    *indexQueryHandle,
                             IndexHandle         *indexHandle,
                             const IndexId       indexIds[],
                             ulong               indexIdCount,
                             const IndexId       entryIds[],
                             ulong               entryIdCount,
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
  String ftsName;
  String uuidIdsString,entityIdsString;
  String entryIdsString;
  ulong  i;
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((indexIdCount == 0L) || (indexIds != NULL));
  assert((entryIdCount == 0L) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName          = String_new();
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  entryIdsString   = String_new();
  orderString      = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_appendFormat(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
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
    String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  // get filters
  filterString = String_newCString("1");
  IndexCommon_filterAppend(filterString,!String_isEmpty(uuidIdsString),"AND","uuids.id IN (%S)",uuidIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
  if (newestOnly)
  {
    IndexCommon_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entriesNewest.type=%u",indexType);
  }
  else
  {
    IndexCommon_filterAppend(filterString,indexType != INDEX_TYPE_ANY,"AND","entries.type=%u",indexType);
  }

  // get sort mode, ordering
  if (newestOnly)
  {
    IndexCommon_appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[sortMode],ordering);
  }
  else
  {
    IndexCommon_appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_SORT_MODE_COLUMNS[sortMode],ordering);
  }

  #ifdef INDEX_DEBUG_LIST_INFO
    fprintf(stderr,"%s, %d: Index_initListEntries -------------------------------------------------------\n",__FILE__,__LINE__);
    fprintf(stderr,"%s, %d: uuidIdsString=%s\n",__FILE__,__LINE__,String_cString(uuidIdsString));
    fprintf(stderr,"%s, %d: entityIdsString=%s\n",__FILE__,__LINE__,String_cString(entityIdsString));
    fprintf(stderr,"%s, %d: offset=%"PRIu64", limit=%"PRIu64"\n",__FILE__,__LINE__,offset,limit);
  #endif /* INDEX_DEBUG_LIST_INFO */

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  if (String_isEmpty(ftsName))
  {
    // entries selected

    // get additional filters
    if (newestOnly)
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    INDEX_DOX(error,
              indexHandle,
    {
      char sqlCommand[MAX_SQL_COMMAND_LENGTH];

      if (newestOnly)
      {
fprintf(stderr,"%s:%d: 1\n",__FILE__,__LINE__);
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,STRING,STRING,INT,KEY,INT,STRING,DATETIME,INT,INT,INT,INT64,INT,KEY,STRING,INT64,INT64,INT,INT,INT64,STRING,INT64),
                                stringFormat(sqlCommand,sizeof(sqlCommand),
                                             "SELECT uuids.id, \
                                                     uuids.jobUUID, \
                                                     entities.id, \
                                                     entities.scheduleUUID, \
                                                     entities.hostName, \
                                                     entities.userName, \
                                                     entities.type, \
                                                     entriesNewest.entryId, \
                                                     entriesNewest.type, \
                                                     entriesNewest.name, \
                                                     entriesNewest.timeLastChanged, \
                                                     entriesNewest.userId, \
                                                     entriesNewest.groupId, \
                                                     entriesNewest.permission, \
                                                     entriesNewest.size, \
                                                     %s, \
                                                     CASE entries.type \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN directoryEntries.storageId \
                                                       WHEN %u THEN linkEntries.storageId \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN specialEntries.storageId \
                                                     END, \
                                                     (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN directoryEntries.storageId \
                                                                                           WHEN %u THEN linkEntries.storageId \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN specialEntries.storageId \
                                                                                         END \
                                                     ), \
                                                     fileEntries.size, \
                                                     imageEntries.size, \
                                                     imageEntries.fileSystemType, \
                                                     imageEntries.blockSize, \
                                                     directoryEntries.totalEntrySizeNewest, \
                                                     linkEntries.destinationName, \
                                                     hardlinkEntries.size \
                                              FROM entriesNewest \
                                                LEFT JOIN entities         ON entities.id=entriesNewest.entityid \
                                                LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                              WHERE     entities.deletedFlag!=1 \
                                                    AND entriesNewest.id IS NOT NULL \
                                                    AND %s \
                                              %s \
                                              LIMIT ?,? \
                                             ",
                                             fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0",

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             String_cString(filterString),
                                             String_cString(orderString)
                                            ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_UINT64(offset),
                                   DATABASE_FILTER_UINT64(limit)
                                 )
                               );
      }
      else
      {
fprintf(stderr,"%s:%d: 2xxxx\n",__FILE__,__LINE__);
        return Database_select2(&indexQueryHandle->databaseStatementHandle,
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
                                 DATABASE_COLUMN_KEY   ("uuids.id"),
                                 DATABASE_COLUMN_STRING("uuids.jobUUID"),
                                 DATABASE_COLUMN_KEY   ("entities.id"),
                                 DATABASE_COLUMN_STRING("entities.scheduleUUID"),
                                 DATABASE_COLUMN_STRING("entities.hostName"),
                                 DATABASE_COLUMN_STRING("entities.userName"),
                                 DATABASE_COLUMN_UINT  ("entities.type"),
                                 DATABASE_COLUMN_KEY   ("entries.id"),
                                 DATABASE_COLUMN_UINT  ("entries.type"),
                                 DATABASE_COLUMN_STRING("entries.name"),
                                 DATABASE_COLUMN_UINT64("entries.timeLastChanged"),
                                 DATABASE_COLUMN_UINT  ("entries.userId"),
                                 DATABASE_COLUMN_UINT  ("entries.groupId"),
                                 DATABASE_COLUMN_UINT  ("entries.permission"),
                                 DATABASE_COLUMN_UINT64("entries.size"),
                                 DATABASE_COLUMN_UINT  (fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0"),
                                 DATABASE_COLUMN_KEY   ("CASE entries.type \
                                                           WHEN ? THEN 0 \
                                                           WHEN ? THEN 0 \
                                                           WHEN ? THEN directoryEntries.storageId \
                                                           WHEN ? THEN linkEntries.storageId \
                                                           WHEN ? THEN 0 \
                                                           WHEN ? THEN specialEntries.storageId \
                                                         END"
                                                       ),
                                 DATABASE_COLUMN_STRING("(SELECT name FROM storages WHERE id=CASE entries.type \
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
                                 DATABASE_COLUMN_UINT64("fileEntries.size"),
                                 DATABASE_COLUMN_UINT64("imageEntries.size"),
                                 DATABASE_COLUMN_UINT  ("imageEntries.fileSystemType"),
                                 DATABASE_COLUMN_UINT  ("imageEntries.blockSize"),
                                 DATABASE_COLUMN_UINT64("directoryEntries.totalEntrySize"),
                                 DATABASE_COLUMN_STRING("linkEntries.destinationName"),
                                 DATABASE_COLUMN_UINT64("hardlinkEntries.size")
                               ),
                               stringFormat(sqlCommand,sizeof(sqlCommand),
                                            "     entities.deletedFlag!=1 \
                                             AND %s \
                                             %s \
                                             LIMIT ?,? \
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
                                 DATABASE_FILTER_UINT  (INDEX_TYPE_SPECIAL),

                                 DATABASE_FILTER_UINT64(offset),
                                 DATABASE_FILTER_UINT64(limit)
                               )
                              );
      }
    });
  }
  else /* !String_isEmpty(ftsName) */
  {
    // names (and optional entries) selected

    // get additional filters
    IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH '%S'",ftsName);
    if (newestOnly)
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    if (newestOnly)
    {
      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

fprintf(stderr,"%s:%d: 3\n",__FILE__,__LINE__);
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,STRING,STRING,INT,KEY,INT,STRING,DATETIME,INT,INT,INT,INT64,INT,KEY,STRING,INT64,INT64,INT,INT,INT64,STRING,INT64),
                                stringFormat(sqlCommand,sizeof(sqlCommand),
                                             "SELECT uuids.id, \
                                                     uuids.jobUUID, \
                                                     entities.id, \
                                                     entities.scheduleUUID, \
                                                     entities.hostName, \
                                                     entities.userName, \
                                                     entities.type, \
                                                     entriesNewest.entryId, \
                                                     entriesNewest.type, \
                                                     entriesNewest.name, \
                                                     entriesNewest.timeLastChanged, \
                                                     entriesNewest.userId, \
                                                     entriesNewest.groupId, \
                                                     entriesNewest.permission, \
                                                     entriesNewest.size, \
                                                     %s, \
                                                     CASE entries.type \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN directoryEntries.storageId \
                                                       WHEN %u THEN linkEntries.storageId \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN specialEntries.storageId \
                                                     END, \
                                                     (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN directoryEntries.storageId \
                                                                                           WHEN %u THEN linkEntries.storageId \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN specialEntries.storageId \
                                                                                         END \
                                                     ), \
                                                     fileEntries.size, \
                                                     imageEntries.size, \
                                                     imageEntries.fileSystemType, \
                                                     imageEntries.blockSize, \
                                                     directoryEntries.totalEntrySizeNewest, \
                                                     linkEntries.destinationName, \
                                                     hardlinkEntries.size \
                                              FROM FTS_entries \
                                                LEFT JOIN entriesNewest    ON entriesNewest.entryId=FTS_entries.entryId \
                                                LEFT JOIN entities         ON entities.id=entriesNewest.entityId \
                                                LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                LEFT JOIN fileEntries      ON fileEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN imageEntries     ON imageEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN linkEntries      ON linkEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entriesNewest.entryId \
                                                LEFT JOIN specialEntries   ON specialEntries.entryId=entriesNewest.id \
                                              WHERE     entities.deletedFlag!=1 \
                                                    AND entriesNewest.id IS NOT NULL \
                                                    AND %s \
                                              %s \
                                              LIMIT ?,? \
                                             ",
                                             fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entriesNewest.id)" : "0",

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             String_cString(filterString),
                                             String_cString(orderString)
                                            ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_UINT64(offset),
                                   DATABASE_FILTER_UINT64(limit)
                                 )
                               );
      });
    }
    else
    {
      INDEX_DOX(error,
                indexHandle,
      {
        char sqlCommand[MAX_SQL_COMMAND_LENGTH];

fprintf(stderr,"%s:%d: 4\n",__FILE__,__LINE__);
        return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                                &indexHandle->databaseHandle,
                                DATABASE_COLUMN_TYPES(KEY,STRING,KEY,STRING,STRING,STRING,INT,KEY,INT,STRING,DATETIME,INT,INT,INT,INT64,INT,KEY,STRING,INT64,INT64,INT,INT,INT64,STRING,INT64),
                                stringFormat(sqlCommand,sizeof(sqlCommand),
                                             "SELECT uuids.id, \
                                                     uuids.jobUUID, \
                                                     entities.id, \
                                                     entities.scheduleUUID, \
                                                     entities.hostName, \
                                                     entities.userName, \
                                                     entities.type, \
                                                     entries.id, \
                                                     entries.type, \
                                                     entries.name, \
                                                     entries.timeLastChanged, \
                                                     entries.userId, \
                                                     entries.groupId, \
                                                     entries.permission, \
                                                     entries.size, \
                                                     %s, \
                                                     CASE entries.type \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN directoryEntries.storageId \
                                                       WHEN %u THEN linkEntries.storageId \
                                                       WHEN %u THEN 0 \
                                                       WHEN %u THEN specialEntries.storageId \
                                                     END, \
                                                     (SELECT name FROM storages WHERE id=CASE entries.type \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN directoryEntries.storageId \
                                                                                           WHEN %u THEN linkEntries.storageId \
                                                                                           WHEN %u THEN 0 \
                                                                                           WHEN %u THEN specialEntries.storageId \
                                                                                         END \
                                                     ), \
                                                     fileEntries.size, \
                                                     imageEntries.size, \
                                                     imageEntries.fileSystemType, \
                                                     imageEntries.blockSize, \
                                                     directoryEntries.totalEntrySize, \
                                                     linkEntries.destinationName, \
                                                     hardlinkEntries.size \
                                               FROM FTS_entries \
                                                 LEFT JOIN entries          ON entries.id=FTS_entries.entryId \
                                                 LEFT JOIN entities         ON entities.id=entries.entityId \
                                                 LEFT JOIN uuids            ON uuids.jobUUID=entities.jobUUID \
                                                 LEFT JOIN fileEntries      ON fileEntries.entryId=entries.id \
                                                 LEFT JOIN imageEntries     ON imageEntries.entryId=entries.id \
                                                 LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                                 LEFT JOIN linkEntries      ON linkEntries.entryId=entries.id \
                                                 LEFT JOIN hardlinkEntries  ON hardlinkEntries.entryId=entries.id \
                                                 LEFT JOIN specialEntries   ON specialEntries.entryId=entries.id \
                                               WHERE     entities.deletedFlag!=1 \
                                                     AND %s \
                                               %s \
                                               LIMIT ?,? \
                                             ",
                                             fragmentsCount ? "(SELECT COUNT(id) FROM entryFragments WHERE entryId=entries.id)" : "0",

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             INDEX_TYPE_FILE,
                                             INDEX_TYPE_IMAGE,
                                             INDEX_TYPE_DIRECTORY,
                                             INDEX_TYPE_LINK,
                                             INDEX_TYPE_HARDLINK,
                                             INDEX_TYPE_SPECIAL,

                                             String_cString(filterString),
                                             String_cString(orderString)
                                            ),
                                 DATABASE_VALUES2
                                 (
                                 ),
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_UINT64(offset),
                                   DATABASE_FILTER_UINT64(limit)
                                 )
                               );
      });
    }
  }
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(ftsName);
    return error;
  }
  #ifdef INDEX_DEBUG_LIST_INFO
    Database_debugPrintQueryInfo(&indexQueryHandle->databaseStatementHandle);
    fprintf(stderr,"%s, %d: -----------------------------------------------------------------------------\n",__FILE__,__LINE__);
  #endif

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(ftsName);

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
                        ulong            *fragmentCount,
                        String           destinationName,
                        FileSystemTypes  *fileSystemType,
                        ulong            *blockSize
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
                           "%lld %S %llu %S %S %S %u %llu %u %S %llu %u %u %u %llu %u %llu %S %llu %llu %u %u %llu %S %llu",
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
  if (uuidId         != NULL) (*uuidId        ) = INDEX_ID_(INDEX_TYPE_UUID,   uuidDatabaseId   );
  if (entityId       != NULL) (*entityId      ) = INDEX_ID_(INDEX_TYPE_ENTITY, entityDatabaseId );
  if (entryId        != NULL) (*entryId       ) = INDEX_ID_(indexType,         entryDatabaseId  );
  if (storageId      != NULL) (*storageId     ) = INDEX_ID_(INDEX_TYPE_STORAGE,storageDatabaseId);
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
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE)
         || (Index_getType(entryId) == INDEX_TYPE_IMAGE)
         || (Index_getType(entryId) == INDEX_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_TYPE_LINK)
         || (Index_getType(entryId) == INDEX_TYPE_HARDLINK)
         || (Index_getType(entryId) == INDEX_TYPE_SPECIAL)
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
    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,KEY,STRING,UINT64,INT64,INT64),
                            "SELECT entryFragments.id, \
                                    storages.id, \
                                    storages.name, \
                                    UNIX_TIMESTAMP(storages.created), \
                                    entryFragments.offset, \
                                    entryFragments.size \
                             FROM entryFragments \
                               LEFT JOIN storages ON storages.id=entryFragments.storageId \
                             WHERE     storages.deletedFlag!=1 \
                                   AND entryFragments.entryId=? \
                             ORDER BY offset ASC \
                             LIMIT ?,? \
                            ",
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY   (Index_getDatabaseId(entryId)),
                               DATABASE_FILTER_UINT64(offset),
                               DATABASE_FILTER_UINT64(limit)

                             )
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
  int64      fragmentOffset_,fragmentSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%llu %llu %'S %llu %llu %llu",
                           &entryFragmentDatabaseId,
                           &storageDatabaseId,
                           storageName,
                           storageDateTime,
                           &fragmentOffset_,
                           &fragmentSize_
                          )
     )
  {
    return FALSE;
  }
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  if (entryFragmentId != NULL) (*entryFragmentId) = INDEX_ID_(INDEX_TYPE_STORAGE,entryFragmentDatabaseId);
  if (storageId       != NULL) (*storageId      ) = INDEX_ID_(INDEX_TYPE_STORAGE,storageDatabaseId);
  if (fragmentOffset  != NULL) (*fragmentOffset ) = (uint64)fragmentOffset_;
  if (fragmentSize    != NULL) (*fragmentSize   ) = (uint64)fragmentSize_;

  return TRUE;
}

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE)
         || (Index_getType(entryId) == INDEX_TYPE_IMAGE)
         || (Index_getType(entryId) == INDEX_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_TYPE_LINK)
         || (Index_getType(entryId) == INDEX_TYPE_HARDLINK)
         || (Index_getType(entryId) == INDEX_TYPE_SPECIAL)
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

    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM fileEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM imageEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM directoryEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_LINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM linkEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM hardlinkEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK_(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 DATABASE_COLUMN_TYPES(),
                                 "DELETE FROM specialEntries WHERE entryId=%lld",
                                 Index_getDatabaseId(entryId)
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

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entriesNewest WHERE entryId=%lld",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM entries WHERE id=%lld",
                             Index_getDatabaseId(entryId)
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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_FILE);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,INT64,UINT64,INT,INT,INT),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 entries.size, \
                                                 entries.timeModified, \
                                                 entries.userId, \
                                                 entries.groupId, \
                                                 entries.permission \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
                           "%lld %llu %S %llu %llu %d %d %d",
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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,INT,INT,INT64),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 imageEntries.fileSystemType, \
                                                 imageEntries.blockSize, \
                                                 entries.size \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
  int        blockSize_;
  int64      blockOffset_,blockCount_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseStatementHandle,
                           "%lld %llu %S %u %u %llu %llu %llu",
                           &databaseId,
                           createdDateTime,
                           imageName,
                           &fileSystemType_,
                           &blockSize_,
                           size,
                           &blockOffset_,
                           &blockCount_
                          )
     )
  {
    return FALSE;
  }
  assert(fileSystemType_ >= 0);
  assert(blockSize_ >= 0);
  assert(blockOffset_ >= 0LL);
  assert(blockCount_ >= 0LL);
  if (indexId != NULL) (*indexId) = INDEX_ID_IMAGE(databaseId);
  if (fileSystemType != NULL) (*fileSystemType) = (FileSystemTypes)fileSystemType_;
  if (blockSize      != NULL) (*blockSize     ) = (blockSize_ >= 0) ? (uint)blockSize_ : 0LL;
  if (blockOffset    != NULL) (*blockOffset   ) = (blockOffset_ >= 0LL) ? (uint64)blockOffset_ : 0LL;
  if (blockCount     != NULL) (*blockCount    ) = (blockCount_ >= 0LL) ? (uint64)blockCount_ : 0LL;

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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,UINT64,INT,INT,INT),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 entries.timeModified, \
                                                 entries.userId, \
                                                 entries.groupId, \
                                                 entries.permission \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
                           "%lld %llu %S %llu %d %d %d",
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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }
  String_appendCString(entryIdsString,"))");

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,STRING,UINT64,INT,INT,INT),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 linkEntries.destinationName, \
                                                 entries.timeModified, \
                                                 entries.userId, \
                                                 entries.groupId, \
                                                 entries.permission \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     storages.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
                           "%lld %llu %S %S %llu %d %d %d",
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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entries.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,INT64,UINT64,INT,INT,INT),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 entries.size, \
                                                 entries.timeModified, \
                                                 entries.userId, \
                                                 entries.groupId, \
                                                 entries.permission \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
                           "%lld %llu %S %llu %llu %d %d %d",
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
  String ftsName;
  String entityIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((entityIdCount == 0) || (entityIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  filterString = String_new();

  // get FTS
  IndexCommon_getFTSString(ftsName,name);

  // get id sets
  entityIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < entityIdCount; i++)
  {
    assert(Index_getType(entityIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
    String_appendFormat(entityIdsString,"%lld",Index_getDatabaseId(entityIds[i]));
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_appendFormat(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  IndexCommon_filterAppend(filterString,TRUE,"AND","entries.type=%u",INDEX_TYPE_DIRECTORY);
  IndexCommon_filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH '%S')",ftsName);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entityIdsString),"AND","entities.id IN (%S)",entityIdsString);
  IndexCommon_filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // prepare list
  IndexCommon_initIndexQueryHandle(indexQueryHandle,indexHandle);
  INDEX_DOX(error,
            indexHandle,
  {
    char sqlCommand[MAX_SQL_COMMAND_LENGTH];

    return Database_prepare(&indexQueryHandle->databaseStatementHandle,
                            &indexHandle->databaseHandle,
                            DATABASE_COLUMN_TYPES(KEY,UINT64,STRING,UINT64,INT,INT,INT),
                            stringFormat(sqlCommand,sizeof(sqlCommand),
                                         "SELECT entries.id, \
                                                 UNIX_TIMESTAMP(entities.created), \
                                                 entries.name, \
                                                 entries.timeModified, \
                                                 entries.userId, \
                                                 entries.groupId, \
                                                 entries.permission \
                                          FROM entries \
                                            LEFT JOIN entities ON entities.id=entries.entityId \
                                          WHERE     entities.deletedFlag!=1 \
                                                AND %s \
                                         ",
                                         String_cString(filterString)
                                        ),
                             DATABASE_VALUES2
                             (
                             ),
                             DATABASE_FILTERS
                             (
                             )
                           );
  });
  if (error != ERROR_NONE)
  {
    IndexCommon_doneIndexQueryHandle(indexQueryHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(entityIdsString);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(entityIdsString);
  String_delete(ftsName);

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
                           "%lld %llu %S %llu %d %d %d",
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
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_FILE,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                    DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_FILE),
                                    DATABASE_VALUE_STRING("name",            name),
                                    DATABASE_VALUE_UINT64("timeLastAccess",  timeLastAccess),
                                    DATABASE_VALUE_UINT64("timeModified",    timeModified),
                                    DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                    DATABASE_VALUE_UINT  ("userId",          userId),
                                    DATABASE_VALUE_UINT  ("groupId",         groupId),
                                    DATABASE_VALUE_UINT  ("permission",      permission),

                                    DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                    DATABASE_VALUE_UINT64("size",            size)
                                  )
                                 );

          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
// TODO: do this wit a trigger again?
          switch (Database_getType(&indexHandle->databaseHandle))
          {
            case DATABASE_TYPE_SQLITE3:
              if (error == ERROR_NONE)
              {
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // changedRowCount
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES2
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", entryId),
                                          DATABASE_VALUE_STRING("name",    name)
                                        )
                                       );
              }
              break;
            case DATABASE_TYPE_MYSQL:
              break;
          }

          // add file entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "fileEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", entryId),
                                      DATABASE_VALUE_UINT64("size",    size)
                                    )
                                   );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add file entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",   entryId),
                                DATABASE_VALUE_KEY   ("storageId", Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_UINT64("offset",    fragmentOffset),
                                DATABASE_VALUE_UINT64("size",      fragmentSize)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
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
                                    "INDEX_ADD_FILE uuidId=%lld entityId=%lld storageId=%llu name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
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
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_IMAGE,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entries",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                    DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_IMAGE),
                                    DATABASE_VALUE_STRING("name",            name),
                                    DATABASE_VALUE_UINT64("timeLastAccess",  0LL),
                                    DATABASE_VALUE_UINT64("timeLastAccess",  0LL),
                                    DATABASE_VALUE_UINT64("timeLastChanged", 0LL),
                                    DATABASE_VALUE_UINT  ("userId",          0),
                                    DATABASE_VALUE_UINT  ("groupId",         0),
                                    DATABASE_VALUE_UINT  ("permission",      0),

                                    DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                    DATABASE_VALUE_UINT64("size",            size)
                                  )
                                 );
          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
          if (error == ERROR_NONE)
          {
            switch (Database_getType(&indexHandle->databaseHandle))
            {
              case DATABASE_TYPE_SQLITE3:
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // changedRowCount
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES2
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", entryId),
                                          DATABASE_VALUE_STRING("name",    name)
                                        )
                                       );
                break;
              case DATABASE_TYPE_MYSQL:
                break;
            }
          }

          // add image entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "imageEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_KEY   ("entryId",        entryId),
                                      DATABASE_VALUE_UINT  ("fileSystemType", fileSystemType),
                                      DATABASE_VALUE_UINT64("size",           size),
                                      DATABASE_VALUE_UINT  ("blockSize",      blockSize)
                                    )
                                   );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add image entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",   entryId),
                                DATABASE_VALUE_KEY   ("storageId", Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_UINT64("offset",    blockOffset*(uint64)blockSize),
                                DATABASE_VALUE_UINT64("size",      blockCount*(uint64)blockSize)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%lld AND totalEntrySize<0",Index_getDatabaseId(storageId));
        IndexCommon_verify(indexHandle,"storages","COUNT(id)",0,"WHERE id=%lld AND totalFileSize<0",Index_getDatabaseId(storageId));
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
                                    "INDEX_ADD_IMAGE uuidId=%lld entityId=%lld storageId=%llu type=IMAGE name=%'S fileSystemType=%'s size=%llu blockSize=%lu blockOffset=%llu blockCount=%llu",
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
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
                              NULL,  // changedRowCount
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_DIRECTORY),
                                DATABASE_VALUE_STRING("name",            name),
                                DATABASE_VALUE_UINT64("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_UINT64("timeModified",    timeModified),
                                DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT  ("userId",          userId),
                                DATABASE_VALUE_UINT  ("groupId",         groupId),
                                DATABASE_VALUE_UINT  ("permission",      permission),

                                DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                DATABASE_VALUE_UINT64("size",            0)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
// TODO: do this again with a trigger?
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "FTS_entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entryId", entryId),
                                    DATABASE_VALUE_STRING("name",    name)
                                  )
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          break;
        case DATABASE_TYPE_MYSQL:
          break;
      }

      // add directory entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "directoryEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",   entryId),
                                DATABASE_VALUE_KEY   ("storageId", Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_STRING("name",      name)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
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
                                    "INDEX_ADD_DIRECTORY uuidId=%lld entityId=%lld storageId=%llu type=DIRECTORY name=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
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
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
                              NULL,  // changedRowCount
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_LINK),
                                DATABASE_VALUE_STRING("name",            linkName),
                                DATABASE_VALUE_UINT64("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_UINT64("timeModified",    timeModified),
                                DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT  ("userId",          userId),
                                DATABASE_VALUE_UINT  ("groupId",         groupId),
                                DATABASE_VALUE_UINT  ("permission",      permission),

                                DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                DATABASE_VALUE_UINT64("size",            0)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
// TODO: do this in a trigger again?
      switch (Database_getType(&indexHandle->databaseHandle))
      {
        case DATABASE_TYPE_SQLITE3:
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "FTS_entries",
                                  DATABASE_FLAG_NONE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entryId", entryId),
                                    DATABASE_VALUE_STRING("name",    linkName)
                                  )
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          break;
        case DATABASE_TYPE_MYSQL:
          break;
      }

      // add link entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "linkEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",         entryId),
                                DATABASE_VALUE_KEY   ("storageId",       Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_STRING("destinationName", destinationName)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
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
                                    "INDEX_ADD_LINK uuidId=%lld entityId=%lld storageId=%llu type=LINK name=%'S destinationName=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
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
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
      // atomic add/get entry
      DATABASE_LOCKED_DO(&indexHandle->databaseHandle,DATABASE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get existing entry id
        error = Database_getId(&indexHandle->databaseHandle,
                               &entryId,
                               "entries",
                               "id",
                               "WHERE     entityId=%lld \
                                      AND type=%u \
                                      AND name=%'S \
                               ",
                               Index_getDatabaseId(entityId),
                               INDEX_TYPE_HARDLINK,
                               name
                              );
        if ((error == ERROR_NONE) && (entryId == DATABASE_ID_NONE))
        {
          // add entry
          error = Database_insert(&indexHandle->databaseHandle,
                                  NULL,  // changedRowCount
                                  "entries",
                                  DATABASE_FLAG_IGNORE,
                                  DATABASE_VALUES2
                                  (
                                    DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                    DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_HARDLINK),
                                    DATABASE_VALUE_STRING("name",            name),
                                    DATABASE_VALUE_UINT64("timeLastAccess",  timeLastAccess),
                                    DATABASE_VALUE_UINT64("timeModified",    timeModified),
                                    DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                    DATABASE_VALUE_UINT  ("userId",          userId),
                                    DATABASE_VALUE_UINT  ("groupId",         groupId),
                                    DATABASE_VALUE_UINT  ("permission",      permission),

                                    DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                    DATABASE_VALUE_UINT64("size",            size)
                                  )
                                 );

          // get entry id
          if (error == ERROR_NONE)
          {
            entryId = Database_getLastRowId(&indexHandle->databaseHandle);
          }

          // add FTS entry
// TODO: do this in a trigger again?
          switch (Database_getType(&indexHandle->databaseHandle))
          {
            case DATABASE_TYPE_SQLITE3:
              if (error == ERROR_NONE)
              {
                error = Database_insert(&indexHandle->databaseHandle,
                                        NULL,  // changedRowCount
                                        "FTS_entries",
                                        DATABASE_FLAG_NONE,
                                        DATABASE_VALUES2
                                        (
                                          DATABASE_VALUE_KEY   ("entryId", entryId),
                                          DATABASE_VALUE_STRING("name",    name)
                                        )
                                       );
              }
              break;
            case DATABASE_TYPE_MYSQL:
              break;
          }

          // add hard link entry
          if (error == ERROR_NONE)
          {
            error = Database_insert(&indexHandle->databaseHandle,
                                    NULL,  // changedRowCount
                                    "hardlinkEntries",
                                    DATABASE_FLAG_NONE,
                                    DATABASE_VALUES2
                                    (
                                      DATABASE_VALUE_KEY   ("entryId", entryId),
                                      DATABASE_VALUE_UINT64("size",    size)
                                    )
                                   );
          }
        }
      }
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add hard link entry fragment
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "entryFragments",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",   entryId),
                                DATABASE_VALUE_KEY   ("storageId", Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_UINT64("offset",    fragmentOffset),
                                DATABASE_VALUE_UINT64("size",      fragmentSize)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
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
                                    "INDEX_ADD_HARDLINK uuidId=%lld entityId=%lld storageId=%llu type=HARDLINK name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
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
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(INDEX_ID_IS_NONE(entityId) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
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
                              NULL,  // changedRowCount
                              "entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entityId",        Index_getDatabaseId(entityId)),
                                DATABASE_VALUE_UINT  ("type",            INDEX_TYPE_SPECIAL),
                                DATABASE_VALUE_STRING("name",            name),
                                DATABASE_VALUE_UINT64("timeLastAccess",  timeLastAccess),
                                DATABASE_VALUE_UINT64("timeModified",    timeModified),
                                DATABASE_VALUE_UINT64("timeLastChanged", timeLastChanged),
                                DATABASE_VALUE_UINT  ("userId",          userId),
                                DATABASE_VALUE_UINT  ("groupId",         groupId),
                                DATABASE_VALUE_UINT  ("permission",      permission),

                                DATABASE_VALUE_KEY   ("uuidId",          Index_getDatabaseId(uuidId)),
                                DATABASE_VALUE_UINT64("size",            0)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get entry id
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add FTS entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "FTS_entries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId", entryId),
                                DATABASE_VALUE_STRING("name",    name)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // add special entry
      error = Database_insert(&indexHandle->databaseHandle,
                              NULL,  // changedRowCount
                              "specialEntries",
                              DATABASE_FLAG_NONE,
                              DATABASE_VALUES2
                              (
                                DATABASE_VALUE_KEY   ("entryId",     entryId),
                                DATABASE_VALUE_KEY   ("storageId",   Index_getDatabaseId(storageId)),
                                DATABASE_VALUE_UINT  ("specialType", specialType),
                                DATABASE_VALUE_UINT  ("major",       major),
                                DATABASE_VALUE_UINT  ("minor",       minor)
                              )
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               Index_getDatabaseId(storageId),
                                               entryId,
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
                                    "INDEX_ADD_SPECIAL uuidId=%lld entityId=%lld storageId=%llu type=SPECIAL name=%'S specialType=%s timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o major=%u minor=%u",
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
                                  ulong            indexIdCount,
                                  const IndexId    entryIds[],
                                  ulong            entryIdCount,
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
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
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
                            NULL,  // changedRowCount
                            "",
                            DATABASE_FLAG_NONE,
                            DATABASE_VALUES2
                            (
                              DATABASE_VALUE_KEY   ("entityId", Index_getDatabaseId(entityId)),
                              DATABASE_VALUE_UINT  ("type",     indexType),
                              DATABASE_VALUE_STRING("name",     entryName)
                            )
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
  assert(   (Index_getType(indexId) == INDEX_TYPE_FILE           )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_IMAGE    )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_DIRECTORY)
         || (Index_getType(indexId) == INDEX_CONST_TYPE_LINK     )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_HARDLINK )
         || (Index_getType(indexId) == INDEX_CONST_TYPE_SPECIAL  )
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

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK_(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             DATABASE_COLUMN_TYPES(),
                             "DELETE FROM skippedEntries WHERE id=%lld",
                             Index_getDatabaseId(indexId)
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
