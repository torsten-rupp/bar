/***********************************************************************\
*
* $Revision: 6281 $
* $Date: 2016-08-28 12:43:29 +0200 (Sun, 28 Aug 2016) $
* $Author: torsten $
* Contents: index import functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/dictionaries.h"
#include "common/progressinfo.h"

#include "bar_common.h"
#include "index/index.h"
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
* Name   : upgradeFromVersion7_importFileEntry
* Purpose: import file entry
* Input  : oldDatabaseHandle,newDatabaseHandle - database handles
*          storageIdDictionary                 - storage id dictionary
*          fromEntryId,toEntry                 - from/to entry id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion7_importFileEntry(DatabaseHandle *oldDatabaseHandle,
                                                 DatabaseHandle *newDatabaseHandle,
                                                 Dictionary     *storageIdDictionary,
                                                 DatabaseId     fromEntryId,
                                                 DatabaseId     toEntryId
                                                )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors     error;
  int64      size;
  DatabaseId fromStorageId;
  DatabaseId toStorageId;
  int64      fragmentOffset;
  int64      fragmentSize;

  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           size = values[0].u64;

                           DIMPORT("import file entry %ld -> %ld: %"PRIi64" bytes",fromEntryId,toEntryId,size);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "fileEntries",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                     DATABASE_VALUE_UINT64("size",    size)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
 //TODO newest
                         DATABASE_TABLES
                         (
                           "fileEntries"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           ValueData valueData;

                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           fromStorageId  = values[0].id;
                           fragmentOffset = values[1].u64;
                           fragmentSize   = values[2].u64;

                           if (Dictionary_find(storageIdDictionary,
                                               &fromStorageId,
                                               sizeof(DatabaseId),
                                               &valueData.value,
                                               NULL
                                              )
                              )
                           {
                             toStorageId = *valueData.id;
                           }
                           else
                           {
                             toStorageId = DATABASE_ID_NONE;
                           }

                           DIMPORT("import file fragment %ld -> %ld: offset=%"PRIi64" size=%"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "entryFragments",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                     DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                     DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                     DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
//TODO newest
                         DATABASE_TABLES
                         (
                           "entryFragments"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("storageId"),
                           DATABASE_COLUMN_UINT64("offset"),
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }

  return error;
}

/***********************************************************************\
* Name   : upgradeFromVersion7_importImageEntry
* Purpose: import image entry
* Input  : oldIndexHandle,newIndexHandle - index handles
*          storageIdDictionary           - storage id dictionary
*          fromEntryId,toEntry           - from/to entry id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion7_importImageEntry(DatabaseHandle *oldDatabaseHandle,
                                                  DatabaseHandle *newDatabaseHandle,
                                                  Dictionary     *storageIdDictionary,
                                                  DatabaseId     fromEntryId,
                                                  DatabaseId     toEntryId
                                                 )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors     error;
  int64      size;
  DatabaseId fromStorageId;
  ValueData  valueData;
  DatabaseId toStorageId;
  int64      fragmentOffset;
  int64      fragmentSize;

  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           size = values[0].u64;

                           DIMPORT("import image entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "imageEntries",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                     DATABASE_VALUE_UINT64("size",    size)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
//TODO newest
                         DATABASE_TABLES
                         (
                           "imageEntries"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           fromStorageId  = values[0].id;
                           fragmentOffset = values[1].u64;
                           fragmentSize   = values[2].u64;

                           if (Dictionary_find(storageIdDictionary,
                                               &fromStorageId,
                                               sizeof(DatabaseId),
                                               &valueData.value,
                                               NULL
                                              )
                              )
                           {
                             toStorageId = *valueData.id;
                           }
                           else
                           {
                             toStorageId = DATABASE_ID_NONE;
                           }

                           DIMPORT("import image fragment %ld -> %ld: offset=%"PRIi64" size=%"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "entryFragments",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                     DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                     DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                     DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
//TODO newest
                         DATABASE_TABLES
                         (
                           "entryFragments"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("storageId"),
                           DATABASE_COLUMN_UINT64("offset"),
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }

  return error;
}

/***********************************************************************\
* Name   : upgradeFromVersion7_importHardlinkEntry
* Purpose: import hardlink entry
* Input  : oldIndexHandle,newIndexHandle - index handles
*          storageIdDictionary           - storage id dictionary
*          fromEntryId,toEntry           - from/to entry id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion7_importHardlinkEntry(DatabaseHandle *oldDatabaseHandle,
                                                     DatabaseHandle *newDatabaseHandle,
                                                     Dictionary     *storageIdDictionary,
                                                     DatabaseId     fromEntryId,
                                                     DatabaseId     toEntryId
                                                    )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors     error;
  int64      size;
  DatabaseId fromStorageId;
  ValueData  valueData;
  DatabaseId toStorageId;
  int64      fragmentOffset;
  int64      fragmentSize;

  error = ERROR_NONE;

  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 1);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           size = values[0].u64;

                           DIMPORT("import hardlink entry %ld -> %ld: %"PRIi64" bytes",fromEntryId,toEntryId,size);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "hardlinkEntries",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                     DATABASE_VALUE_UINT64("size",    size)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
//TODO newest
                         DATABASE_TABLES
                         (
                           "hardlinkEntries"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }
  if (error == ERROR_NONE)
  {
    error = Database_get(oldDatabaseHandle,
                         CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                         {
                           assert(values != NULL);
                           assert(valueCount == 3);

                           UNUSED_VARIABLE(userData);
                           UNUSED_VARIABLE(valueCount);

                           fromStorageId  = values[0].id;
                           fragmentOffset = values[1].u64;
                           fragmentSize   = values[2].u64;

                           if (Dictionary_find(storageIdDictionary,
                                               &fromStorageId,
                                               sizeof(DatabaseId),
                                               &valueData.value,
                                               NULL
                                              )
                              )
                           {
                             toStorageId = *valueData.id;
                           }
                           else
                           {
                             toStorageId = DATABASE_ID_NONE;
                           }

                           DIMPORT("import hardlink fragment %ld -> %ld: offset=%"PRIi64" size=%"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                           error = Database_insert(newDatabaseHandle,
                                                   NULL,  // insertRowId
                                                   "entryFragments",
                                                   DATABASE_FLAG_NONE,
                                                   DATABASE_VALUES
                                                   (
                                                     DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                     DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                     DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                     DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                   ),
                                                   DATABASE_COLUMNS_NONE,
                                                   DATABASE_FILTERS_NONE
                                                  );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
//TODO newest
                         DATABASE_TABLES
                         (
                           "entryFragments"
                         ),
                         DATABASE_FLAG_FETCH_ALL,
                         DATABASE_COLUMNS
                         (
                           DATABASE_COLUMN_KEY   ("storageId"),
                           DATABASE_COLUMN_UINT64("offset"),
                           DATABASE_COLUMN_UINT64("size"),
                         ),
                         "entryId=?",
                         DATABASE_FILTERS
                         (
                           DATABASE_FILTER_KEY (fromEntryId)
                         ),
                         NULL,  // groupBy
                         NULL,  // orderBy
                         0LL,
                         DATABASE_UNLIMITED
                       );
  }

  return error;
}

/***********************************************************************\
* Name   : upgradeFromVersion7_importEntry
* Purpose: import entry
* Input  : oldDatabaseHandle,newDatabaseHandle - database handles
*          storageIdDictionary                 - storage id dictionary
*          type                                - entry type
*          fromEntryId,toEntryId               - fromt/to entry id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion7_importEntry(DatabaseHandle *oldDatabaseHandle,
                                             DatabaseHandle *newDatabaseHandle,
                                             Dictionary     *storageIdDictionary,
                                             IndexTypes     type,
                                             DatabaseId     fromEntryId,
                                             DatabaseId     toEntryId
                                            )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors error;

  error = ERROR_UNKNOWN;
  switch (type)
  {
    case INDEX_TYPE_FILE:
      DIMPORT("import file entry %ld -> %ld",fromEntryId,toEntryId);
      error = upgradeFromVersion7_importFileEntry(oldDatabaseHandle,
                                                  newDatabaseHandle,
                                                  storageIdDictionary,
                                                  fromEntryId,
                                                  toEntryId
                                                 );
      break;
    case INDEX_TYPE_IMAGE:
      DIMPORT("import image entry %ld -> %ld",fromEntryId,toEntryId);
      error = upgradeFromVersion7_importImageEntry(oldDatabaseHandle,
                                                   newDatabaseHandle,
                                                   storageIdDictionary,
                                                   fromEntryId,
                                                   toEntryId
                                                  );
      break;
    case INDEX_TYPE_DIRECTORY:
      DIMPORT("import directory entry %ld -> %ld",fromEntryId,toEntryId);
      error = Database_copyTable(oldDatabaseHandle,
                                 newDatabaseHandle,
                                 "directoryEntries",
                                 "directoryEntries",
                                 FALSE,  // transaction flag
                                 NULL,  // duration
                                 CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                         DatabaseColumnInfo *toColumnInfo,
                                                         void               *userData
                                                        ),
                                 {
                                   DatabaseId fromStorageId;
                                   ValueData  valueData;
                                   DatabaseId toStorageId;

                                   assert(fromColumnInfo != NULL);
                                   assert(toColumnInfo != NULL);

                                   UNUSED_VARIABLE(toColumnInfo);
                                   UNUSED_VARIABLE(userData);

                                   fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                   assert(fromStorageId != DATABASE_ID_NONE);

                                   if (Dictionary_find(storageIdDictionary,
                                                       &fromStorageId,
                                                       sizeof(DatabaseId),
                                                       &valueData.value,
                                                       NULL
                                                      )
                                      )
                                   {
                                     toStorageId = *valueData.id;
                                   }
                                   else
                                   {
                                     toStorageId = DATABASE_ID_NONE;
                                   }

                                   (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                   (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                   return ERROR_NONE;
                                 },NULL),
                                 CALLBACK_(NULL,NULL),  // post-copy
                                 CALLBACK_(NULL,NULL),  // pause
                                 CALLBACK_(NULL,NULL),  // progress
                                 "entryId=?",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_KEY(fromEntryId)
                                 ),
                                 NULL,  // groupBy
                                 NULL,  // orderby
                                 0L,
                                 1L
                                );
      break;
    case INDEX_TYPE_LINK:
      DIMPORT("import link entry %ld -> %ld",fromEntryId,toEntryId);
      error = Database_copyTable(oldDatabaseHandle,
                                 newDatabaseHandle,
                                 "linkEntries",
                                 "linkEntries",
                                 FALSE,  // transaction flag
                                 NULL,  // duration
                                 CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                         DatabaseColumnInfo *toColumnInfo,
                                                         void               *userData
                                                        ),
                                 {
                                   DatabaseId fromStorageId;
                                   ValueData  valueData;
                                   DatabaseId toStorageId;

                                   assert(fromColumnInfo != NULL);
                                   assert(toColumnInfo != NULL);

                                   UNUSED_VARIABLE(toColumnInfo);
                                   UNUSED_VARIABLE(userData);

                                   fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                   assert(fromStorageId != DATABASE_ID_NONE);
                                   if (Dictionary_find(storageIdDictionary,
                                                       &fromStorageId,
                                                       sizeof(DatabaseId),
                                                       &valueData.value,
                                                       NULL
                                                      )
                                      )
                                   {
                                     toStorageId = *valueData.id;
                                   }
                                   else
                                   {
                                     toStorageId = DATABASE_ID_NONE;
                                   }

                                   (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                   (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                   return ERROR_NONE;
                                 },NULL),
                                 CALLBACK_(NULL,NULL),  // post-copy
                                 CALLBACK_(NULL,NULL),  // pause
                                 CALLBACK_(NULL,NULL),  // pause
                                 "entryId=?",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_KEY(fromEntryId)
                                 ),
                                 NULL,  // groupBy
                                 NULL,  // orderby
                                 0L,
                                 1L
                                );
      break;
    case INDEX_TYPE_HARDLINK:
      DIMPORT("import hardlink entry %ld -> %ld",fromEntryId,toEntryId);
      error = upgradeFromVersion7_importHardlinkEntry(oldDatabaseHandle,
                                                      newDatabaseHandle,
                                                      storageIdDictionary,
                                                      fromEntryId,
                                                      toEntryId
                                                     );
      break;
    case INDEX_TYPE_SPECIAL:
      DIMPORT("import special entry %ld -> %ld",fromEntryId,toEntryId);
      error = Database_copyTable(oldDatabaseHandle,
                                 newDatabaseHandle,
                                 "specialEntries",
                                 "specialEntries",
                                 FALSE,  // transaction flag
                                 NULL,  // duration
                                 CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                         DatabaseColumnInfo *toColumnInfo,
                                                         void               *userData
                                                        ),
                                 {
                                   DatabaseId fromStorageId;
                                   ValueData  valueData;
                                   DatabaseId toStorageId;

                                   assert(fromColumnInfo != NULL);
                                   assert(toColumnInfo != NULL);

                                   UNUSED_VARIABLE(fromColumnInfo);
                                   UNUSED_VARIABLE(toColumnInfo);
                                   UNUSED_VARIABLE(userData);

                                   fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                   assert(fromStorageId != DATABASE_ID_NONE);
                                   if (Dictionary_find(storageIdDictionary,
                                                       &fromStorageId,
                                                       sizeof(DatabaseId),
                                                       &valueData.value,
                                                       NULL
                                                      )
                                      )
                                   {
                                     toStorageId = *valueData.id;
                                   }
                                   else
                                   {
                                     toStorageId = DATABASE_ID_NONE;
                                   }

                                   (void)Database_setTableColumnId(toColumnInfo,"entryId",toEntryId);
                                   (void)Database_setTableColumnId(toColumnInfo,"storageId",toStorageId);

                                   return ERROR_NONE;
                                 },NULL),
                                 CALLBACK_(NULL,NULL),  // post-copy
                                 CALLBACK_(NULL,NULL),  // pause
                                 CALLBACK_(NULL,NULL),  // pause
                                 "entryId=?",
                                 DATABASE_FILTERS
                                 (
                                   DATABASE_FILTER_KEY(fromEntryId)
                                 ),
                                 NULL,  // groupBy
                                 NULL,  // orderby
                                 0L,
                                 1L
                                );
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* not NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : getEntityImportStepsVersion7
* Purpose: get number of import steps for index version 7
* Input  : databaseHandle- database handle
*          entityId      - entity database id
* Output : -
* Return : number of import steps for entity
* Notes  : -
\***********************************************************************/

LOCAL uint64 getEntityImportStepsVersion7(DatabaseHandle *databaseHandle,
                                          DatabaseId     entityId
                                         )
{
  uint64 maxSteps;
  Errors error;
  uint64 n;

  assert(databaseHandle != NULL);

  maxSteps = 0LL;

  // get max. steps (storages, entries)
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "storages",
                             "COUNT(id)",
                             "entityId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY (entityId)
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "entries",
                             "COUNT(id)",
                             "entityId=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY (entityId)
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }

  return maxSteps;
}

/***********************************************************************\
* Name   : getImportStepsVersion7
* Purpose: get number of import steps for index version 7
* Input  : databaseHandle     - old database handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL uint64 getImportStepsVersion7(DatabaseHandle *databaseHandle)
{
  uint64 maxSteps;
  Errors error;
  uint64 n;

  assert(databaseHandle != NULL);

  maxSteps = 0LL;

  // get max. steps (uuids+entities+storages+entries)
  error = Database_getUInt64(databaseHandle,
                             &n,
                             "uuids",
                             "COUNT(id)",
                             "id!=0",
                             DATABASE_FILTERS
                             (
                             ),
                             NULL  // group
                            );
  if (error == ERROR_NONE)
  {
    maxSteps += n;
  }

  Database_get(databaseHandle,
               CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
               {
                 DatabaseId entityId;

                 assert(values != NULL);
                 assert(valueCount == 1);

                 UNUSED_VARIABLE(userData);
                 UNUSED_VARIABLE(valueCount);

                 entityId = values[0].id;

                 maxSteps += 1+getEntityImportStepsVersion7(databaseHandle,entityId);

                 return ERROR_NONE;
               },NULL),
               NULL,  // changedRowCount
               DATABASE_TABLES
               (
                 "entities"
               ),
               DATABASE_FLAG_FETCH_ALL,
               DATABASE_COLUMNS
               (
                 DATABASE_COLUMN_KEY     ("id"),
               ),
// TODO:
#if 1
               DATABASE_FILTERS_NONE,
#else
               "deletedFlag!=TRUE \
               ",
               DATABASE_FILTERS
               (
               ),
#endif
               NULL,  // groupBy
               NULL,  // orderBy
               0LL,
               DATABASE_UNLIMITED
              );

  return maxSteps;
}

/***********************************************************************\
* Name   : importIndexVersion7
* Purpose: import index version 7
* Input  : oldDatabaseHandle,newDatabaseHandle - database handles
*          progressInfo                        - progress info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion7(DatabaseHandle *oldDatabaseHandle,
                                 DatabaseHandle *newDatabaseHandle,
                                 ProgressInfo   *progressInfo
                                )
{
  Errors     error;
  Dictionary storageIdDictionary;

  // init variables
  error = ERROR_NONE;
  Dictionary_init(&storageIdDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // transfer uuids (if not exists, ignore errors)
  (void)Database_copyTable(oldDatabaseHandle,
                           newDatabaseHandle,
                           "uuids",
                           "uuids",
                           FALSE,  // transaction flag
                           NULL,  // duration
                           CALLBACK_(NULL,NULL),  // pre-copy
                           CALLBACK_(NULL,NULL),  // post-copy
                           CALLBACK_(getCopyPauseCallback(),NULL),
                           CALLBACK_(ProgressInfo_step,progressInfo),
                           DATABASE_FILTERS_NONE,
                           NULL,  // groupBy
                           NULL,  // orderby
                           0L,
                           DATABASE_UNLIMITED
                          );
  DIMPORT("imported UUIDs");

  // transfer entities with storages and entries
  error = Database_copyTable(oldDatabaseHandle,
                             newDatabaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             NULL,  // duration
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               DatabaseId fromEntityId;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(fromColumnInfo);
                               UNUSED_VARIABLE(toColumnInfo);
                               UNUSED_VARIABLE(userData);

                               // keep default entity entries
                               fromEntityId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               if (fromEntityId == INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)
                               {
                                 Database_setTableColumnId(toColumnInfo,"id",INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID);
                               }

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storages, entries
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               DatabaseId   fromStorageId;
                               DatabaseId   toStorageId;
                               DatabaseId   fromEntityId;
                               DatabaseId   toEntityId;
                               uint64       maxSteps;
                               ProgressInfo subProgressInfo;
                               Errors       error;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);

                               DIMPORT("import entity %ld -> %ld: jobUUID=%s",fromEntityId,toEntityId,Database_getTableColumnCString(fromColumnInfo,"jobUUID",NULL));

                               maxSteps = getEntityImportStepsVersion7(oldDatabaseHandle,
                                                                       fromEntityId
                                                                      );

                               ProgressInfo_init(&subProgressInfo,
                                                 progressInfo,
                                                 32,  // filterWindowSize
                                                 500,  // reportTime
                                                 maxSteps,
                                                 CALLBACK_(outputProgressInit,NULL),
                                                 CALLBACK_(outputProgressDone,NULL),
                                                 CALLBACK_(formatSubProgressInfo,NULL),
                                                 "Import entity #%"PRIi64" '%s': ",
                                                 fromEntityId,
                                                 Database_getTableColumnCString(fromColumnInfo,"jobUUID","")
                                                );

                               // transfer storages of entity
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "storages",
                                                          "storages",
                                                          FALSE,  // transaction flag
                                                          NULL,  // duration
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(fromColumnInfo);
                                                            UNUSED_VARIABLE(toColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);
                                                            (void)Database_setTableColumnEnum(toColumnInfo,"state",INDEX_STATE_CREATE);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: get from/to storage ids
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);

                                                            DIMPORT("import storage %ld -> %ld: %s",fromStorageId,toStorageId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                            (void)Database_setTableColumnEnum(toColumnInfo,"state",Database_getTableColumnEnum(fromColumnInfo,"state",INDEX_STATE_OK));

                                                            Dictionary_add(&storageIdDictionary,
                                                                           &fromStorageId,
                                                                           sizeof(DatabaseId),
                                                                           &toStorageId,
                                                                           sizeof(DatabaseId)
                                                                          );

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                          "entityId=?",
                                                          DATABASE_FILTERS
                                                          (
                                                            DATABASE_FILTER_KEY(fromEntityId)
                                                          ),
                                                          NULL,  // groupBy
                                                          NULL,  // orderby
                                                          0L,
                                                          DATABASE_UNLIMITED
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 ProgressInfo_done(&subProgressInfo);
                                 return error;
                               }

                               // transfer entries of entity
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "entries",
                                                          "entries",
                                                          FALSE,  // transaction flag
                                                          NULL,  // duration
                                                          // pre: transfer entry
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(fromColumnInfo);
                                                            UNUSED_VARIABLE(toColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: transfer files, images, directories, links, special entries
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            DatabaseId fromEntryId;
                                                            IndexTypes type;
                                                            DatabaseId toEntryId;

                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(toColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            fromEntryId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);

                                                            type = Database_getTableColumnId(fromColumnInfo,"type",INDEX_TYPE_NONE);
                                                            toEntryId = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);

                                                            return upgradeFromVersion7_importEntry(oldDatabaseHandle,
                                                                                                   newDatabaseHandle,
                                                                                                   &storageIdDictionary,
                                                                                                   type,
                                                                                                   fromEntryId,
                                                                                                   toEntryId
                                                                                                  );
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(ProgressInfo_step,&subProgressInfo),
                                                          "entityId=?",
                                                          DATABASE_FILTERS
                                                          (
                                                            DATABASE_FILTER_KEY(fromEntityId)
                                                          ),
                                                          NULL,  // groupBy
                                                          NULL,  // orderby
                                                          0L,
                                                          DATABASE_UNLIMITED
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 ProgressInfo_done(&subProgressInfo);
                                 return error;
                               }

                               ProgressInfo_done(&subProgressInfo);

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(ProgressInfo_step,progressInfo),
                             "id!=?",
                             DATABASE_FILTERS
                             (
                               DATABASE_FILTER_KEY(DATABASE_ID_NONE)
                             ),
                             NULL,  // groupBy
                             NULL,  // orderby
                             0L,
                             DATABASE_UNLIMITED
                            );
  if (error != ERROR_NONE)
  {
    Dictionary_done(&storageIdDictionary);
    return error;
  }

  // free resources
  Dictionary_done(&storageIdDictionary);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
