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
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/dictionaries.h"

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
  ValueData  valueData;
  DatabaseId toStorageId;
  int64      fragmentOffset;
  int64      fragmentSize;

  return Database_get(oldDatabaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        size = values[0].u64;

                        DIMPORT("import entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
                        error = Database_insert(newDatabaseHandle,
                                                NULL,  // changedRowCount
                                                "fileEntries",
                                                DATABASE_FLAG_NONE,
                                                DATABASE_VALUES
                                                (
                                                  DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                  DATABASE_VALUE_UINT64("size",    size)
                                                )
                                               );
                        if (error != ERROR_NONE)
                        {
                          return error;
                        }

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

                                               DIMPORT("import file fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                                               error = Database_insert(newDatabaseHandle,
                                                                       NULL,  // changedRowCount
                                                                       "entryFragments",
                                                                       DATABASE_FLAG_NONE,
                                                                       DATABASE_VALUES
                                                                       (
                                                                         DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                                         DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                         DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                                         DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                                       )
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
                                             NULL, // orderGroup
                                             0LL,
                                             DATABASE_UNLIMITED
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
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_UINT64("size"),
                      ),
                      "entryId=?",
                      DATABASE_FILTERS
                      (
                        DATABASE_FILTER_KEY (fromEntryId)
                      ),
                      NULL,  // orderGroup
                      0LL,
                      DATABASE_UNLIMITED
                    );
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

  return Database_get(oldDatabaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        size = values[0].u64;

                        DIMPORT("import entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
                        error = Database_insert(newDatabaseHandle,
                                                NULL,  // changedRowCount
                                                "imageEntries",
                                                DATABASE_FLAG_NONE,
                                                DATABASE_VALUES
                                                (
                                                  DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                  DATABASE_VALUE_UINT64("size",    size)
                                                )
                                               );
                        if (error != ERROR_NONE)
                        {
                          return error;
                        }

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

                                               DIMPORT("import image fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                                               error = Database_insert(newDatabaseHandle,
                                                                       NULL,  // changedRowCount
                                                                       "entryFragments",
                                                                       DATABASE_FLAG_NONE,
                                                                       DATABASE_VALUES
                                                                       (
                                                                         DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                                         DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                         DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                                         DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                                       )
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
                                             NULL, // orderGroup
                                             0LL,
                                             DATABASE_UNLIMITED
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
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_UINT64("size"),
                      ),
                      "entryId=?",
                      DATABASE_FILTERS
                      (
                        DATABASE_FILTER_KEY (fromEntryId)
                      ),
                      NULL,  // orderGroup
                      0LL,
                      DATABASE_UNLIMITED
                    );
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

  return Database_get(oldDatabaseHandle,
                      CALLBACK_INLINE(Errors,(const DatabaseValue values[], uint valueCount, void *userData),
                      {
                        assert(values != NULL);
                        assert(valueCount == 1);

                        UNUSED_VARIABLE(userData);
                        UNUSED_VARIABLE(valueCount);

                        size = values[0].u64;

                        DIMPORT("import entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
                        error = Database_insert(newDatabaseHandle,
                                                NULL,  // changedRowCount
                                                "hardlinkEntries",
                                                DATABASE_FLAG_NONE,
                                                DATABASE_VALUES
                                                (
                                                  DATABASE_VALUE_KEY   ("entryId", toEntryId),
                                                  DATABASE_VALUE_UINT64("size",    size)
                                                )
                                               );
                        if (error != ERROR_NONE)
                        {
                          return error;
                        }

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

                                               DIMPORT("import hardlink fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromStorageId,toStorageId,fragmentOffset,fragmentSize);
                                               error = Database_insert(newDatabaseHandle,
                                                                       NULL,  // changedRowCount
                                                                       "entryFragments",
                                                                       DATABASE_FLAG_NONE,
                                                                       DATABASE_VALUES
                                                                       (
                                                                         DATABASE_VALUE_KEY   ("entryId",  toEntryId),
                                                                         DATABASE_VALUE_KEY   ("storageId",toStorageId),
                                                                         DATABASE_VALUE_UINT64("offset",   fragmentOffset),
                                                                         DATABASE_VALUE_UINT64("size",     fragmentSize)
                                                                       )
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
                                             NULL, // orderGroup
                                             0LL,
                                             DATABASE_UNLIMITED
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
                      DATABASE_COLUMNS
                      (
                        DATABASE_COLUMN_UINT64("size"),
                      ),
                      "entryId=?",
                      DATABASE_FILTERS
                      (
                        DATABASE_FILTER_KEY (fromEntryId)
                      ),
                      NULL,  // orderGroup
                      0LL,
                      DATABASE_UNLIMITED
                    );
}

/***********************************************************************\
* Name   : getImportStepsVersion7
* Purpose: get number of import steps for index version 7
* Input  : oldIndexHandle     - old index handle
*          uuidFactor         - UUID count factor (>= 1)
*          entityCountFactor  - entity count factor (>= 1)
*          storageCountFactor - storage count factor (>= 1)
* Output : -
* Return : number of import steps
* Notes  : -
\***********************************************************************/

LOCAL ulong getImportStepsVersion7(DatabaseHandle *oldDatabaseHandle,
                                   uint           uuidCountFactor,
                                   uint           entityCountFactor,
                                   uint           storageCountFactor
                                  )
{
  ulong  maxSteps;
  Errors error;
  int64  uuidCount,entityCount,storageCount,entriesCount;
  int64  fileEntryCount,imageEntryCount,directoryEntryCount,linkEntryCount,hardlinkEntryCount,specialEntryCount;
  int64  entryFragmentsCount;

  assert(uuidCountFactor >= 1);
  assert(entityCountFactor >= 1);
  assert(storageCountFactor >= 1);

  maxSteps = 0;

  maxSteps += 6;

  // get max. steps (entities+storages+entries)
  error = Database_getInt64(oldDatabaseHandle,
                            &uuidCount,
                            "uuids",
                            "COUNT(id)",
                            "WHERE id!=0",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += uuidCount*(ulong)uuidCountFactor;
  }
  else
  {
    uuidCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &entityCount,
                            "entities",
                            "COUNT(id)",
                            "WHERE id!=0",
                            DATABASE_FILTERS
                            (
                            ),
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += entityCount*(ulong)entityCountFactor;
  }
  else
  {
    entityCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &storageCount,
                            "storages",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += storageCount*(ulong)storageCountFactor;
  }
  else
  {
    storageCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &entriesCount,
                            "entries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += entriesCount;
  }
  else
  {
    entriesCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &fileEntryCount,
                            "fileEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            "GROUP BY entryId"
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += fileEntryCount;
  }
  else
  {
    fileEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &imageEntryCount,
                            "imageEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            "GROUP BY entryId"
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += imageEntryCount;
  }
  else
  {
    imageEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &directoryEntryCount,
                            "directoryEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += directoryEntryCount;
  }
  else
  {
    directoryEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &linkEntryCount,
                            "linkEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            "GROUP BY entryId"
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += linkEntryCount;
  }
  else
  {
    linkEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &hardlinkEntryCount,
                            "hardlinkEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += hardlinkEntryCount;
  }
  else
  {
    hardlinkEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &specialEntryCount,
                            "specialEntries",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            "GROUP BY entryId"
                           );
  if (error == ERROR_NONE)
  {
    maxSteps += specialEntryCount;
  }
  else
  {
    specialEntryCount = 0LL;
  }
  error = Database_getInt64(oldDatabaseHandle,
                            &entryFragmentsCount,
                            "entryFragments",
                            "COUNT(id)",
                            DATABASE_FILTERS_NONE,
                            NULL  // group
                           );
  if (error == ERROR_NONE)
  {
    // nothing to do
  }
  else
  {
    entryFragmentsCount = 0LL;
  }
// TODO:
#if 0
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "%lld entities/%lld storages/%lld entries/%lld fragments to import",
              entityCount,
              storageCount,
              entriesCount,
              entryFragmentsCount
             );
#endif
  DIMPORT("import %"PRIu64" entities",         entityCount);
  DIMPORT("import %"PRIu64" storages",         storageCount);
  DIMPORT("import %"PRIu64" entries",          entriesCount);
  DIMPORT("import %"PRIu64" file entries",     fileEntryCount);
  DIMPORT("import %"PRIu64" image entries",    imageEntryCount);
  DIMPORT("import %"PRIu64" directory entries",directoryEntryCount);
  DIMPORT("import %"PRIu64" link entries",     linkEntryCount);
  DIMPORT("import %"PRIu64" hardlink entries", hardlinkEntryCount);
  DIMPORT("import %"PRIu64" special entries",  specialEntryCount);
  DIMPORT("import %"PRIu64" fragments",        entryFragmentsCount);

  return maxSteps;
}

/***********************************************************************\
* Name   : importIndexVersion7
* Purpose: import index version 7
* Input  : oldDatabaseHandle,newDatabaseHandle - database handles
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion7XXX(DatabaseHandle *oldDatabaseHandle,
                                 DatabaseHandle *newDatabaseHandle
                                )
{
  typedef union
  {
    const void       *value;
    const DatabaseId *id;
  } KeyData;
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors             error;
  uint64             duration;
  Dictionary         storageIdDictionary;
  DictionaryIterator dictionaryIterator;
  KeyData            keyData;
  ValueData          valueData;
  String             storageIdsString;
  DatabaseId         toEntityId;

  error = ERROR_NONE;

fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
// TODO:
#if 0
  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");     IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"files");       IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"images");      IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"directories"); IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"links");       IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"special");     IndexCommon_progressStep(&importProgressInfo);
  DIMPORT("fixed broken ids");
#endif

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
                           CALLBACK_(progressStep,&importProgressInfo),  // progress
                           NULL  // filter
                          );
  DIMPORT("imported UUIDs");

  // transfer entities with storages and entries
  duration = 0LL;
  Dictionary_init(&storageIdDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  storageIdsString = String_new();
  error = Database_copyTable(oldDatabaseHandle,
                             newDatabaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer entity
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

                               // currently nothing special to do

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storages, entries
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;
                               DatabaseId fromEntityId;
                               DatabaseId toEntityId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "storages",
                                                          "storages",
                                                          FALSE,  // transaction flag
                                                          &duration,
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
                                                            Dictionary_add(&storageIdDictionary,
                                                                           &fromStorageId,
                                                                           sizeof(DatabaseId),
                                                                           &toStorageId,
                                                                           sizeof(DatabaseId)
                                                                          );

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(progressStep,NULL),
                                                          "WHERE entityId=%lld",
                                                          fromEntityId
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }

                               // get storage id filter
                               String_clear(storageIdsString);
                               Dictionary_initIterator(&dictionaryIterator,&storageIdDictionary);
                               while (Dictionary_getNext(&dictionaryIterator,
                                                         &keyData.value,
                                                         NULL,
                                                         NULL,
                                                         NULL
                                                        )
                                     )
                               {
                                 if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
                                 String_formatAppend(storageIdsString,"%d",*keyData.id);
                               }
                               Dictionary_doneIterator(&dictionaryIterator);

                               // transfer entries of entity
                               DIMPORT("import entity %ld -> %ld: jobUUID=%s, storages=%s",fromEntityId,toEntityId,Database_getTableColumnCString(fromColumnInfo,"jobUUID",NULL),String_cString(storageIdsString));
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "entries",
                                                          "entries",
                                                          FALSE,  // transaction flag
                                                          &duration,
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

                                                            error = ERROR_NONE;

                                                            switch (type)
                                                            {
                                                              case INDEX_TYPE_FILE:
                                                                error = upgradeFromVersion7_importFileEntry(oldDatabaseHandle,
                                                                                                            newDatabaseHandle,
                                                                                                            &storageIdDictionary,
                                                                                                            fromEntryId,
                                                                                                            toEntryId
                                                                                                           );
                                                                break;
                                                              case INDEX_TYPE_IMAGE:
                                                                error = upgradeFromVersion7_importImageEntry(oldDatabaseHandle,
                                                                                                             newDatabaseHandle,
                                                                                                             &storageIdDictionary,
                                                                                                             fromEntryId,
                                                                                                             toEntryId
                                                                                                            );
                                                                break;
                                                              case INDEX_TYPE_DIRECTORY:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "directoryEntries",
                                                                                           "directoryEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(toColumnInfo);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              case INDEX_TYPE_LINK:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "linkEntries",
                                                                                           "linkEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(toColumnInfo);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              case INDEX_TYPE_HARDLINK:
                                                                error = upgradeFromVersion7_importHardlinkEntry(oldDatabaseHandle,
                                                                                                                newDatabaseHandle,
                                                                                                                &storageIdDictionary,
                                                                                                                fromEntryId,
                                                                                                                toEntryId
                                                                                                               );
                                                                break;
                                                              case INDEX_TYPE_SPECIAL:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "specialEntries",
                                                                                           "specialEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(fromColumnInfo);
                                                                                             UNUSED_VARIABLE(toColumnInfo);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              default:
                                                                #ifndef NDEBUG
                                                                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                                                #endif /* not NDEBUG */
                                                                break;
                                                            }

                                                            return error;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(progressStep,NULL),
                                                          "WHERE entityId=%lld",
                                                          fromEntityId
                                                         );
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }
                               t1 = Misc_getTimestamp();

                               logImportProgress("Imported entity #%"PRIi64": '%s' (%llus)",
                                                 toEntityId,
                                                 Database_getTableColumnCString(fromColumnInfo,"jobUUID",""),
                                                 (t1-t0)/US_PER_SECOND
                                                );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(progressStep,NULL),
                             "WHERE id!=0"
                            );
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    Dictionary_done(&storageIdDictionary);
    return error;
  }

  // transfer storages and entries without entity
  error = Database_copyTable(oldDatabaseHandle,
                             newDatabaseHandle,
                             "storages",
                             "storages",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer storage and create entity
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               Errors error;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(userData);

                               error = initEntity(oldDatabaseHandle,
                                                  newDatabaseHandle,
                                                  Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE),
                                                  &toEntityId
                                                 );
                               (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);

                               return error;
                             },NULL),
                             // post: copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                     DatabaseColumnInfo *toColumnInfo,
                                                     void               *userData
                                                    ),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               assert(fromColumnInfo != NULL);
                               assert(toColumnInfo != NULL);

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                               assert(fromStorageId != DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                               assert(toStorageId != DATABASE_ID_NONE);

#if 1
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(oldDatabaseHandle,
                                                          newDatabaseHandle,
                                                          "entries",
                                                          "entries",
                                                          TRUE,  // transaction flag
                                                          &duration,
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(fromColumnInfo);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnId(toColumnInfo,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                  DatabaseColumnInfo *toColumnInfo,
                                                                                  void               *userData
                                                                                 ),
                                                          {
                                                            Errors     error;
                                                            DatabaseId fromEntryId;
//                                                            IndexTypes type;
                                                            DatabaseId toEntryId;

                                                            assert(fromColumnInfo != NULL);
                                                            assert(toColumnInfo != NULL);

                                                            UNUSED_VARIABLE(userData);

                                                            fromEntryId = Database_getTableColumnId(fromColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
//                                                            type = Database_getTableColumnId(fromColumnInfo,"type",INDEX_TYPE_NONE);
                                                            toEntryId   = Database_getTableColumnId(toColumnInfo,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);

                                                            error = ERROR_NONE;

// TODO:
#if 0
                                                            switch (type)
                                                            {
                                                              case INDEX_TYPE_FILE:
                                                                error = upgradeFromVersion7_importFileEntry(oldDatabaseHandle,
                                                                                                            newDatabaseHandle,
                                                                                                            &storageIdDictionary,
                                                                                                            fromEntryId,
                                                                                                            toEntryId
                                                                                                           );
                                                                break;
                                                              case INDEX_TYPE_IMAGE:
                                                                error = upgradeFromVersion7_importImageEntry(oldDatabaseHandle,
                                                                                                             newDatabaseHandle,
                                                                                                             &storageIdDictionary,
                                                                                                             fromEntryId,
                                                                                                             toEntryId
                                                                                                            );
                                                                break;
                                                              case INDEX_TYPE_DIRECTORY:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "directoryEntries",
                                                                                           "directoryEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(fromColumns);
                                                                                             UNUSED_VARIABLE(toColumns);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              case INDEX_TYPE_LINK:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "linkEntries",
                                                                                           "linkEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(fromColumns);
                                                                                             UNUSED_VARIABLE(toColumns);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              case INDEX_TYPE_HARDLINK:
                                                                error = upgradeFromVersion7_importHardlinkEntry(oldDatabaseHandle,
                                                                                                                newDatabaseHandle,
                                                                                                                &storageIdDictionary,
                                                                                                                fromEntryId,
                                                                                                                toEntryId
                                                                                                               );
                                                                break;
                                                              case INDEX_TYPE_SPECIAL:
                                                                DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnCString(fromColumnInfo,"name",NULL));
                                                                error = Database_copyTable(oldDatabaseHandle,
                                                                                           newDatabaseHandle,
                                                                                           "specialEntries",
                                                                                           "specialEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(DatabaseColumnInfo *fromColumnInfo,
                                                                                                                   DatabaseColumnInfo *toColumnInfo,
                                                                                                                   void               *userData
                                                                                                                  ),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             assert(fromColumnInfo != NULL);
                                                                                             assert(toColumnInfo != NULL);

                                                                                             UNUSED_VARIABLE(toColumns);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnId(fromColumnInfo,"storageId",DATABASE_ID_NONE);
                                                                                             assert(fromStorageId != DATABASE_ID_NONE);
                                                                                             if (Dictionary_find(&storageIdDictionary,
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
                                                                                           "WHERE entryId=%lld LIMIT 0,1",
                                                                                           fromEntryId
                                                                                          );
                                                                break;
                                                              default:
                                                                #ifndef NDEBUG
                                                                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                                                #endif /* not NDEBUG */
                                                                break;
                                                            }
#else
error = ERROR_NONE;
#endif
                                                            return error;
                                                          },NULL),
                                                          CALLBACK_(NULL,NULL),  // pause
                                                          CALLBACK_(NULL,NULL),  // progress
                                                          "WHERE storageId=%lld",
                                                          fromStorageId
                                                         );
                               (void)unlockEntity(newDatabaseHandle,toEntityId);
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }
                               t1 = Misc_getTimestamp();
#endif

                               logImportProgress("Imported storage #%"PRIi64": '%s' (%llus)",
                                                 toStorageId,
                                                 Database_getTableColumnCString(fromColumnInfo,"name",""),
                                                 (t1-t0)/US_PER_SECOND
                                                );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(NULL,NULL),  // pause
                             CALLBACK_(progressStep,NULL),
                             "WHERE entityId IS NULL"
                            );
  String_delete(storageIdsString);
  Dictionary_done(&storageIdDictionary);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
