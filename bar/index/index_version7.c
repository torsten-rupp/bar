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

#include "errors.h"
#include "index_definition.h"

#include "index/index.h"

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
* Input  : oldIndexHandle,newIndexHandle - index handles
*          storageIdDictionary           - storage id dictionary
*          fromEntryId,toEntry           - from/to entry id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeFromVersion7_importFileEntry(IndexHandle *oldIndexHandle,
                                                 IndexHandle *newIndexHandle,
                                                 Dictionary  *storageIdDictionary,
                                                 DatabaseId  fromEntryId,
                                                 DatabaseId  toEntryId
                                                )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  int64               size;
  DatabaseId          fromStorageId;
  ValueData           valueData;
  DatabaseId          toStorageId;
  int64               fragmentOffset;
  int64               fragmentSize;

  error = Database_prepare(&databaseQueryHandle,
                          &oldIndexHandle->databaseHandle,
                          "SELECT fileEntries.size, \
                                  entryFragments.storageId, \
                                  entryFragments.offset AS fragmentOffset, \
                                  entryFragments.size AS fragmentSize \
                           FROM fileEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=fileEntries.entryId \
                           WHERE fileEntries.entryId=%lld \
                          ",
                          fromEntryId
                         );
  if (error == ERROR_NONE)
  {
    if (Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize))
    {
      DIMPORT("import file entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO fileEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld \
                                  ); \
                               ",
                               toEntryId,
                               size
                              );
      if (error == ERROR_NONE)
      {
        do
        {
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

          DIMPORT("import file fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromEntryId,toEntryId,fragmentOffset,fragmentSize);
          error = Database_execute(&newIndexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT INTO entryFragments \
                                      ( \
                                       entryId, \
                                       storageId, \
                                       offset, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %lld, \
                                       %lld, \
                                       %lld \
                                      ); \
                                   ",
                                   toEntryId,
                                   toStorageId,
                                   fragmentOffset,
                                   fragmentSize
                                  );
         }
         while (   (error == ERROR_NONE)
                && Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize)
               );
       }
    }
    Database_finalize(&databaseQueryHandle);
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

LOCAL Errors upgradeFromVersion7_importImageEntry(IndexHandle *oldIndexHandle,
                                                  IndexHandle *newIndexHandle,
                                                  Dictionary  *storageIdDictionary,
                                                  DatabaseId  fromEntryId,
                                                  DatabaseId  toEntryId
                                                 )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  int64               size;
  DatabaseId          fromStorageId;
  ValueData           valueData;
  DatabaseId          toStorageId;
  int64               fragmentOffset;
  int64               fragmentSize;

  error = Database_prepare(&databaseQueryHandle,
                          &oldIndexHandle->databaseHandle,
                          "SELECT imageEntries.size, \
                                  entryFragments.storageId, \
                                  entryFragments.offset AS fragmentOffset, \
                                  entryFragments.size AS fragmentSize \
                           FROM imageEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=imageEntries.entryId \
                           WHERE imageEntries.entryId=%lld \
                          ",
                          fromEntryId
                         );
  if (error == ERROR_NONE)
  {
    if (Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize))
    {
      DIMPORT("import file entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO imageEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld \
                                  ); \
                               ",
                               toEntryId,
                               size
                              );
      if (error == ERROR_NONE)
      {
        do
        {
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

          DIMPORT("import file fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromEntryId,toEntryId,fragmentOffset,fragmentSize);
          error = Database_execute(&newIndexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT INTO entryFragments \
                                      ( \
                                       entryId, \
                                       storageId, \
                                       offset, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %lld, \
                                       %lld, \
                                       %lld \
                                      ); \
                                   ",
                                   toEntryId,
                                   toStorageId,
                                   fragmentOffset,
                                   fragmentSize
                                  );
         }
         while (   (error == ERROR_NONE)
                && Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize)
               );
       }
    }
    Database_finalize(&databaseQueryHandle);
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

LOCAL Errors upgradeFromVersion7_importHardlinkEntry(IndexHandle *oldIndexHandle,
                                                     IndexHandle *newIndexHandle,
                                                     Dictionary  *storageIdDictionary,
                                                     DatabaseId  fromEntryId,
                                                     DatabaseId  toEntryId
                                                    )
{
  typedef union
  {
    void       *value;
    DatabaseId *id;
  } ValueData;

  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  int64               size;
  DatabaseId          fromStorageId;
  ValueData           valueData;
  DatabaseId          toStorageId;
  int64               fragmentOffset;
  int64               fragmentSize;

  error = Database_prepare(&databaseQueryHandle,
                          &oldIndexHandle->databaseHandle,
                          "SELECT hardlinkEntries.size, \
                                  entryFragments.storageId, \
                                  entryFragments.offset AS fragmentOffset, \
                                  entryFragments.size AS fragmentSize \
                           FROM hardlinkEntries \
                             LEFT JOIN entryFragments ON entryFragments.entryId=hardlinkEntries.entryId \
                           WHERE hardlinkEntries.entryId=%lld \
                          ",
                          fromEntryId
                         );
  if (error == ERROR_NONE)
  {
    if (Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize))
    {
      DIMPORT("import file entry %ld -> %ld: %"PRIi64"",fromEntryId,toEntryId,size);
      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK_(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO hardlinkEntries \
                                  ( \
                                   entryId, \
                                   size \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld \
                                  ); \
                               ",
                               toEntryId,
                               size
                              );
      if (error == ERROR_NONE)
      {
        do
        {
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

          DIMPORT("import file fragment %ld -> %ld: %"PRIi64", %"PRIi64"",fromEntryId,toEntryId,fragmentOffset,fragmentSize);
          error = Database_execute(&newIndexHandle->databaseHandle,
                                   CALLBACK_(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "INSERT INTO entryFragments \
                                      ( \
                                       entryId, \
                                       storageId, \
                                       offset, \
                                       size \
                                      ) \
                                    VALUES \
                                      ( \
                                       %lld, \
                                       %lld, \
                                       %lld, \
                                       %lld \
                                      ); \
                                   ",
                                   toEntryId,
                                   toStorageId,
                                   fragmentOffset,
                                   fragmentSize
                                  );
         }
         while (   (error == ERROR_NONE)
                && Database_getNextRow(&databaseQueryHandle,"%lld %lld %lld %lld",&size,&fromStorageId,&fragmentOffset,&fragmentSize)
               );
       }
    }
    Database_finalize(&databaseQueryHandle);
  }

  return error;
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

LOCAL ulong getImportStepsVersion7(IndexHandle *oldIndexHandle,
                                   uint        uuidCountFactor,
                                   uint        entityCountFactor,
                                   uint        storageCountFactor
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
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &uuidCount,
                                "uuids",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += uuidCount*(ulong)uuidCountFactor;
  }
  else
  {
    uuidCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entityCount,
                                "entities",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += entityCount*(ulong)entityCountFactor;
  }
  else
  {
    entityCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &storageCount,
                                "storages",
                                "COUNT(id)",
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += storageCount*(ulong)storageCountFactor;
  }
  else
  {
    storageCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entriesCount,
                                "entries",
                                "COUNT(id)",
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += entriesCount;
  }
  else
  {
    entriesCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &fileEntryCount,
                                "fileEntries",
                                "COUNT(id)",
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
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &imageEntryCount,
                                "imageEntries",
                                "COUNT(id)",
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
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &directoryEntryCount,
                                "directoryEntries",
                                "COUNT(id)",
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += directoryEntryCount;
  }
  else
  {
    directoryEntryCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &linkEntryCount,
                                "linkEntries",
                                "COUNT(id)",
                                "GROUP BY entryId"
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += linkEntryCount;
  }
  else
  {
    linkEntryCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &hardlinkEntryCount,
                                "hardlinkEntries",
                                "COUNT(id)",
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += hardlinkEntryCount;
  }
  else
  {
    hardlinkEntryCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &specialEntryCount,
                                "specialEntries",
                                "COUNT(id)",
                                "GROUP BY entryId"
                                ""
                               );
  if (error == ERROR_NONE)
  {
    maxSteps += specialEntryCount;
  }
  else
  {
    specialEntryCount = 0LL;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entryFragmentsCount,
                                "entryFragments",
                                "COUNT(id)",
                                ""
                               );
  if (error == ERROR_NONE)
  {
    // nothing to do
  }
  else
  {
    entryFragmentsCount = 0LL;
  }
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "%lld entities/%lld storages/%lld entries/%lld fragments to import",
              entityCount,
              storageCount,
              entriesCount,
              entryFragmentsCount
             );
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
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndexVersion7(IndexHandle *oldIndexHandle,
                                 IndexHandle *newIndexHandle
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
  IndexId            toEntityId;

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storage");     IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"files");       IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"images");      IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"directories"); IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"links");       IndexCommon_progressStep(&importProgressInfo);
  fixBrokenIds(oldIndexHandle,"special");     IndexCommon_progressStep(&importProgressInfo);
  DIMPORT("fixed broken ids");

  // transfer uuids (if not exists, ignore errors)
  (void)Database_copyTable(&oldIndexHandle->databaseHandle,
                           &newIndexHandle->databaseHandle,
                           "uuids",
                           "uuids",
                           FALSE,  // transaction flag
                           NULL,  // duration
                           CALLBACK_(NULL,NULL),  // pre-copy
                           CALLBACK_(NULL,NULL),  // post-copy
                           CALLBACK_(getCopyPauseCallback(),NULL),
                           CALLBACK_(IndexCommon_progressStep,&importProgressInfo),  // progress
                           NULL  // filter
                          );
  DIMPORT("imported UUIDs");

  // transfer entities with storages and entries
  duration = 0LL;
  Dictionary_init(&storageIdDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  storageIdsString = String_new();
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "entities",
                             "entities",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(toColumnList);
                               UNUSED_VARIABLE(userData);

                               // currently nothing special to do

                               return ERROR_NONE;
                             },NULL),
                             // post: transfer storages, entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;
                               DatabaseId fromEntityId;
                               DatabaseId toEntityId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               UNUSED_VARIABLE(toColumnList);
                               UNUSED_VARIABLE(userData);

                               fromEntityId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromEntityId != DATABASE_ID_NONE);
                               toEntityId = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toEntityId != DATABASE_ID_NONE);

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storages",
                                                          "storages",
                                                          FALSE,  // transaction flag
                                                          &duration,
                                                          // pre: transfer storage
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(toColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListInt64(toColumnList,"entityId",toEntityId);

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          // post: get from/to storage ids
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(userData);

                                                            fromStorageId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromStorageId != DATABASE_ID_NONE);
                                                            toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toStorageId != DATABASE_ID_NONE);

                                                            DIMPORT("import storage %ld -> %ld: %s",fromStorageId,toStorageId,Database_getTableColumnListCString(fromColumnList,"name",NULL));
                                                            Dictionary_add(&storageIdDictionary,
                                                                           &fromStorageId,
                                                                           sizeof(DatabaseId),
                                                                           &toStorageId,
                                                                           sizeof(DatabaseId)
                                                                          );

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(IndexCommon_progressStep,NULL),
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
                               DIMPORT("import entity %ld -> %ld: jobUUID=%s, storages=%s",fromEntityId,toEntityId,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL),String_cString(storageIdsString));
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "entries",
                                                          "entries",
                                                          FALSE,  // transaction flag
                                                          &duration,
                                                          // pre: transfer entry
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
                                                            DatabaseId fromEntryId;
                                                            IndexTypes type;
                                                            DatabaseId toEntryId;

                                                            UNUSED_VARIABLE(userData);
                                                            UNUSED_VARIABLE(toColumnList);

                                                            fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
                                                            type = Database_getTableColumnListId(fromColumnList,"type",INDEX_TYPE_NONE);
                                                            toEntryId = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);

                                                            error = ERROR_NONE;

                                                            DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnListCString(fromColumnList,"name",NULL));
                                                            switch (type)
                                                            {
                                                              case INDEX_TYPE_FILE:
                                                                error = upgradeFromVersion7_importFileEntry(oldIndexHandle,
                                                                                                            newIndexHandle,
                                                                                                            &storageIdDictionary,
                                                                                                            fromEntryId,
                                                                                                            toEntryId
                                                                                                           );
                                                                break;
                                                              case INDEX_TYPE_IMAGE:
                                                                error = upgradeFromVersion7_importImageEntry(oldIndexHandle,
                                                                                                             newIndexHandle,
                                                                                                             &storageIdDictionary,
                                                                                                             fromEntryId,
                                                                                                             toEntryId
                                                                                                            );
                                                                break;
                                                              case INDEX_TYPE_DIRECTORY:
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "directoryEntries",
                                                                                           "directoryEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "linkEntries",
                                                                                           "linkEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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
                                                                error = upgradeFromVersion7_importHardlinkEntry(oldIndexHandle,
                                                                                                                newIndexHandle,
                                                                                                                &storageIdDictionary,
                                                                                                                fromEntryId,
                                                                                                                toEntryId
                                                                                                               );
                                                                break;
                                                              case INDEX_TYPE_SPECIAL:
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "specialEntries",
                                                                                           "specialEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(IndexCommon_progressStep,NULL),
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
                                                 Database_getTableColumnListCString(fromColumnList,"jobUUID",""),
                                                 (t1-t0)/US_PER_SECOND
                                                );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(getCopyPauseCallback(),NULL),
                             CALLBACK_(IndexCommon_progressStep,NULL),
                             "WHERE id!=0"
                            );
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    Dictionary_done(&storageIdDictionary);
    return error;
  }

  // transfer storages and entries without entity
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storages",
                             "storages",
                             FALSE,  // transaction flag
                             &duration,
                             // pre: transfer storage and create entity
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               Errors       error;
                               StaticString (jobUUID,MISC_UUID_STRING_LENGTH);

                               UNUSED_VARIABLE(fromColumnList);
                               UNUSED_VARIABLE(userData);

                               if (   Index_findStorageById(oldIndexHandle,
                                                            INDEX_ID_STORAGE(Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE)),
                                                            jobUUID,
                                                            NULL,  // scheduleUUDI
                                                            NULL,  // uuidId
                                                            NULL,  // entityId
                                                            NULL,  // storageName
                                                            NULL,  // createdDateTime
                                                            NULL,  // size
                                                            NULL,  // indexState
                                                            NULL,  // indexMode
                                                            NULL,  // lastCheckedDateTime
                                                            NULL,  // errorMessage
                                                            NULL,  // totalEntryCount
                                                            NULL  // totalEntrySize
                                                           )
                                   && Index_findEntity(newIndexHandle,
                                                       INDEX_ID_NONE,
                                                       jobUUID,
                                                       NULL,  // scheduleUUID
                                                       NULL,  // hostName
                                                       ARCHIVE_TYPE_NONE,
                                                       0LL,  // createdDateTime
                                                       NULL,  // jobUUID
                                                       NULL,  // scheduleUUID
                                                       NULL,  // uuidId
                                                       &toEntityId,
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
                                                         NULL,  // hostName
                                                         NULL,  // userName
                                                         ARCHIVE_TYPE_FULL,
                                                         0LL,  // createdDateTime
                                                         TRUE,  // locked
                                                         &toEntityId
                                                        );
                               }
                               (void)Database_setTableColumnListInt64(toColumnList,"entityId",Index_getDatabaseId(toEntityId));

                               return error;
                             },NULL),
                             // post: copy files, images, directories, links, special entries
                             CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                             {
                               DatabaseId fromStorageId;
                               DatabaseId toStorageId;
                               uint64     t0;
                               uint64     t1;
                               Errors     error;

                               UNUSED_VARIABLE(userData);

                               fromStorageId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                               assert(fromStorageId != DATABASE_ID_NONE);
                               toStorageId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                               assert(toStorageId != DATABASE_ID_NONE);

                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "entries",
                                                          "entries",
                                                          TRUE,  // transaction flag
                                                          &duration,
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            UNUSED_VARIABLE(fromColumnList);
                                                            UNUSED_VARIABLE(userData);

                                                            (void)Database_setTableColumnListInt64(toColumnList,"entityId",Index_getDatabaseId(toEntityId));

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                          {
                                                            Errors     error;
                                                            DatabaseId fromEntryId;
                                                            IndexTypes type;
                                                            DatabaseId toEntryId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
                                                            type = Database_getTableColumnListId(fromColumnList,"type",INDEX_TYPE_NONE);
                                                            toEntryId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);

                                                            error = ERROR_NONE;

                                                            DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnListCString(fromColumnList,"name",NULL));
                                                            switch (type)
                                                            {
                                                              case INDEX_TYPE_FILE:
                                                                error = upgradeFromVersion7_importFileEntry(oldIndexHandle,
                                                                                                            newIndexHandle,
                                                                                                            &storageIdDictionary,
                                                                                                            fromEntryId,
                                                                                                            toEntryId
                                                                                                           );
                                                                break;
                                                              case INDEX_TYPE_IMAGE:
                                                                error = upgradeFromVersion7_importImageEntry(oldIndexHandle,
                                                                                                             newIndexHandle,
                                                                                                             &storageIdDictionary,
                                                                                                             fromEntryId,
                                                                                                             toEntryId
                                                                                                            );
                                                                break;
                                                              case INDEX_TYPE_DIRECTORY:
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "directoryEntries",
                                                                                           "directoryEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "linkEntries",
                                                                                           "linkEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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
                                                                error = upgradeFromVersion7_importHardlinkEntry(oldIndexHandle,
                                                                                                                newIndexHandle,
                                                                                                                &storageIdDictionary,
                                                                                                                fromEntryId,
                                                                                                                toEntryId
                                                                                                               );
                                                                break;
                                                              case INDEX_TYPE_SPECIAL:
                                                                error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                           &newIndexHandle->databaseHandle,
                                                                                           "specialEntries",
                                                                                           "specialEntries",
                                                                                           FALSE,  // transaction flag
                                                                                           &duration,
                                                                                           CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                           {
                                                                                             DatabaseId fromStorageId;
                                                                                             DatabaseId toStorageId;

                                                                                             UNUSED_VARIABLE(fromColumnList);
                                                                                             UNUSED_VARIABLE(userData);

                                                                                             fromStorageId = Database_getTableColumnListId(fromColumnList,"storageId",DATABASE_ID_NONE);
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

                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);
                                                                                             (void)Database_setTableColumnListInt64(toColumnList,"storageId",toStorageId);

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
                                                          CALLBACK_(NULL,NULL),  // pause
                                                          CALLBACK_(NULL,NULL),  // progress
                                                          "WHERE storageId=%lld",
                                                          fromStorageId
                                                         );
                               (void)Index_unlockEntity(newIndexHandle,toEntityId);
                               if (error != ERROR_NONE)
                               {
                                 return error;
                               }
                               t1 = Misc_getTimestamp();

                               logImportProgress("Imported storage #%"PRIi64": '%s' (%llus)",
                                                 toStorageId,
                                                 Database_getTableColumnListCString(fromColumnList,"name",""),
                                                 (t1-t0)/US_PER_SECOND
                                                );

                               return ERROR_NONE;
                             },NULL),
                             CALLBACK_(NULL,NULL),  // pause
                             CALLBACK_(IndexCommon_progressStep,NULL),
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
