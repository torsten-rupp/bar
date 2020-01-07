/***********************************************************************\
* Name   : importCurrentVersion
* Purpose: import current version of index
* Input  : oldIndexHandle,newIndexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors importCurrentVersion(IndexHandle *oldIndexHandle,
                                  IndexHandle *newIndexHandle
                                 )
{
  Errors                                             error;
  int64                                              entityCount,storageCount,entriesCount,entryFragmentsCount;
  uint64                                             duration;
  Dictionary                                         storageIdDictionary;
  DictionaryIterator                                 dictionaryIterator;
  union { const void *value; const DatabaseId *id; } keyData;
  union { void *value; DatabaseId *id; }             valueData;
  String                                             storageIdsString;
  IndexId                                            toEntityId;

  error = ERROR_NONE;

  // fix possible broken ids
  fixBrokenIds(oldIndexHandle,"storages");
  fixBrokenIds(oldIndexHandle,"files");
  fixBrokenIds(oldIndexHandle,"images");
  fixBrokenIds(oldIndexHandle,"directories");
  fixBrokenIds(oldIndexHandle,"links");
  fixBrokenIds(oldIndexHandle,"special");
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
                           CALLBACK_(NULL,NULL),  // progress
                           NULL  // filter
                          );
  DIMPORT("imported UUIDs");

  // get max. steps (entities+storages+entries)
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entityCount,
                                "entities",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &storageCount,
                                "storage",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entriesCount,
                                "entries",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_getInteger64(&oldIndexHandle->databaseHandle,
                                &entryFragmentsCount,
                                "entryFragments",
                                "COUNT(id)",
                                "WHERE id!=0"
                               );
  if (error != ERROR_NONE)
  {
    return error;
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
  DIMPORT("import %"PRIu64" entries",entityCount+storageCount+entriesCount+entryFragmentsCount);

  // transfer entities with storages and entries
  initImportProgress(entityCount+storageCount+entriesCount+entryFragmentsCount);
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
                               DIMPORT("import entity %ld -> %ld: jobUUID=%s",fromEntityId,toEntityId,Database_getTableColumnListCString(fromColumnList,"jobUUID",NULL));

                               // transfer storages of entity
                               t0 = Misc_getTimestamp();
                               error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                          &newIndexHandle->databaseHandle,
                                                          "storage",
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

                                                            Dictionary_add(&storageIdDictionary,
                                                                           &fromStorageId,
                                                                           sizeof(DatabaseId),
                                                                           &toStorageId,
                                                                           sizeof(DatabaseId)
                                                                          );
                                                            DIMPORT("import storage %ld -> %ld: %s",fromStorageId,toStorageId,Database_getTableColumnListCString(fromColumnList,"name",NULL));

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(importProgress,NULL),
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
fprintf(stderr,"%s, %d: storageIdsString=%s\n",__FILE__,__LINE__,String_cString(storageIdsString));

                               // transfer entries of entity
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
                                                            DatabaseId toEntryId;

                                                            UNUSED_VARIABLE(userData);
                                                            UNUSED_VARIABLE(toColumnList);

                                                            fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
                                                            toEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);
                                                            DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnListCString(fromColumnList,"name",NULL));
#warning
if (fromEntryId==6123020) {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//asm("int3");
}

                                                            error = ERROR_NONE;

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "fileEntries",
                                                                                         "fileEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer file entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import file fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "imageEntries",
                                                                                         "imageEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer image entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import image fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "hardlinkEntries",
                                                                                         "hardlinkEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer hardlink entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import hardlink fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }

                                                            return ERROR_NONE;
                                                          },NULL),
                                                          CALLBACK_(getCopyPauseCallback(),NULL),
                                                          CALLBACK_(importProgress,NULL),
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
                             CALLBACK_(importProgress,NULL),
                             "WHERE id!=0"
                            );
  if (error != ERROR_NONE)
  {
    doneImportProgress();
    return error;
  }

  // transfer storages and entries without entity
  error = Database_copyTable(&oldIndexHandle->databaseHandle,
                             &newIndexHandle->databaseHandle,
                             "storage",
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
                                                            DatabaseId toEntryId;

                                                            UNUSED_VARIABLE(userData);

                                                            fromEntryId = Database_getTableColumnListId(fromColumnList,"id",DATABASE_ID_NONE);
                                                            assert(fromEntryId != DATABASE_ID_NONE);
                                                            toEntryId   = Database_getTableColumnListId(toColumnList,"id",DATABASE_ID_NONE);
                                                            assert(toEntryId != DATABASE_ID_NONE);
                                                            DIMPORT("import entry %ld -> %ld: %s",fromEntryId,toEntryId,Database_getTableColumnListCString(fromColumnList,"name",NULL));

                                                            error = ERROR_NONE;

                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "fileEntries",
                                                                                         "fileEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer file entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import file fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "imageEntries",
                                                                                         "imageEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer image entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import image fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
//fprintf(stderr,"%s, %d: copy d\n",__FILE__,__LINE__);
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
//fprintf(stderr,"%s, %d: copy l\n",__FILE__,__LINE__);
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
//fprintf(stderr,"%s, %d: copy h\n",__FILE__,__LINE__);
                                                              error = Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                         &newIndexHandle->databaseHandle,
                                                                                         "hardlinkEntries",
                                                                                         "hardlinkEntries",
                                                                                         FALSE,  // transaction flag
                                                                                         &duration,
                                                                                         // pre: transfer hardlink entry
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           (void)Database_setTableColumnListInt64(toColumnList,"entryId",toEntryId);

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         // post: transfer entry fragments
                                                                                         CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                         {
                                                                                           UNUSED_VARIABLE(fromColumnList);
                                                                                           UNUSED_VARIABLE(toColumnList);
                                                                                           UNUSED_VARIABLE(userData);

                                                                                           DIMPORT("import hardlink fragments %ld: %"PRIi64", %"PRIi64"",toEntryId,Database_getTableColumnListInt64(fromColumnList,"fragmentOffset",0LL),Database_getTableColumnListInt64(fromColumnList,"fragmentSize",0LL));
                                                                                           return Database_copyTable(&oldIndexHandle->databaseHandle,
                                                                                                                     &newIndexHandle->databaseHandle,
                                                                                                                     "entryFragments",
                                                                                                                     "entryFragments",
                                                                                                                     FALSE,  // transaction flag
                                                                                                                     &duration,
                                                                                                                     CALLBACK_INLINE(Errors,(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData),
                                                                                                                     {
                                                                                                                       DatabaseId fromStorageId;
                                                                                                                       DatabaseId toStorageId;

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
                                                                                                                     CALLBACK_(importProgress,NULL),
                                                                                                                     "WHERE entryId=%lld",
                                                                                                                     fromEntryId
                                                                                                                    );

                                                                                           return ERROR_NONE;
                                                                                         },NULL),
                                                                                         CALLBACK_(NULL,NULL),  // pause
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
                                                            }
                                                            if (error == ERROR_NONE)
                                                            {
//fprintf(stderr,"%s, %d: copy s\n",__FILE__,__LINE__);
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
                                                                                         CALLBACK_(importProgress,NULL),
                                                                                         "WHERE entryId=%lld",
                                                                                         fromEntryId
                                                                                        );
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
                             CALLBACK_(importProgress,NULL),
                             "WHERE entityId IS NULL"
                            );
  String_delete(storageIdsString);
  Dictionary_done(&storageIdDictionary);
  if (error != ERROR_NONE)
  {
    doneImportProgress();
    return error;
  }
  doneImportProgress();

  return error;
}
