/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index storage functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_STORAGES__
#define __INDEX_STORAGES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/progressinfo.h"
#include "errors.h"

#include "index/index_common.h"

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
* Name   : IndexStorage_new
* Purpose: create new storage index
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          userName        - user name (can be NULL)
*          hostName        - host name (can be NULL)
*          storageName     - storage name (can be NULL)
*          createdDateTime - create date/time
*          size            - size [bytes]
*          indexState      - index state
*          indexMode       - index mode
* Output : storageId - index id of new storage
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_new(IndexHandle *indexHandle,
                        IndexId     uuidId,
                        IndexId     entityId,
                        ConstString userName,
                        ConstString hostName,
                        ConstString storageName,
                        uint64      createdDateTime,
                        uint64      size,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       );

/***********************************************************************\
* Name   : IndexStorage_clear
* Purpose: clear index storage content
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_clear(IndexHandle *indexHandle,
                          IndexId    storageId
                         );

/***********************************************************************\
* Name   : IndexStorage_hasDeleted
* Purpose: check if deleted storages exists
* Input  : indexQueryHandle    - index query handle
*          deletedStorageCount - delete storage count variable (can be
*                                NULL)
* Return : TRUE iff deleted storages exists
* Notes  : -
\***********************************************************************/

bool IndexStorage_hasDeleted(IndexHandle *indexHandle,
                             uint        *deletedStorageCount
                            );

/***********************************************************************\
* Name   : IndexStorage_isDeleted
* Purpose: check if storage entry is deleted
* Input  : indexQueryHandle - index query handle
*          storageId        - index id of storage
* Return : TRUE iff deleted
* Notes  : -
\***********************************************************************/

bool IndexStorage_isDeleted(IndexHandle *indexHandle,
                            IndexId     storageId
                           );

/***********************************************************************\
* Name   : IndexStorage_isEmpty
* Purpose: check if storage entry is empty (no entries)
* Input  : indexQueryHandle - index query handle
*          storageId        - index id of storage
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

bool IndexStorage_isEmpty(IndexHandle *indexHandle,
                          IndexId     storageId
                         );

/***********************************************************************\
* Name   : IndexStorage_get
* Purpose: get storage index name/entries/size
* Input  : indexHandle - index handle
*          storageId   - index id of storage index
* Output:  uuidId              - index if of uuid
*          jobUUID             - job UUID (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          entityUUID          - schedule UUID (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storageName         - storage name (can be NULL)
*          createdDateTime     - created date/time (can be NULL)
-*         size                - size of stroage (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time (can be NULL)
*          errorMessage        - last error message (can be NULL)
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_get(IndexHandle  *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       entityUUID,
                        ArchiveTypes *archiveType,
                        String       storageName,
                        uint64       *createdDateTime,
                        uint64       *size,
                        IndexStates  *indexState,
                        IndexModes   *indexMode,
                        uint64       *lastCheckedDateTime,
                        String       errorMessage,
                        uint64       *totalEntryCount,
                        uint64       *totalEntrySize
                       );

/***********************************************************************\
* Name   : IndexStorage_getState
* Purpose: get index storage state
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
*          errorMessage        - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_getState(IndexHandle  *indexHandle,
                             IndexId      storageId,
                             IndexStates  *indexState,
                             uint64       *lastCheckedDateTime,
                             String       errorMessage
                            );

/***********************************************************************\
* Name   : IndexStorage_setState
* Purpose: set storage index state
* Input  : indexHandle         - index handle
*          storageId           - index id of storage
*          indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be 0LL)
*          errorMessage        - error message (can be NULL)
*          ...                 - optional arguments for error message
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_setState(IndexHandle *indexHandle,
                             IndexId     storageId,
                             IndexStates indexState,
                             uint64      lastCheckedDateTime,
                             const char  *errorMessage,
                             ...
                            );

/***********************************************************************\
* Name   : IndexStorage_countState
* Purpose: get number of storage entries with specific state
* Input  : indexHandle - index handle
*          indexState  - index state; see IndexStates
* Output : -
* Return : number of entries or -1
* Notes  : -
\***********************************************************************/

long IndexStorage_countState(IndexHandle *indexHandle,
                             IndexStates indexState
                            );

/***********************************************************************\
* Name   : IndexStorage_update
* Purpose: update storage index
* Input  : indexHandle     - index handle
*          storageId       - index id of storage
*          hostName          - host name (can be NULL)
*          userName        - user name (can be NULL)
*          storageName     - storage name (can be NULL)
*          createdDateTime - create date/time (can be 0)
*          size            - size [bytes]
*          comment         - comment (can be NULL)
*          updateNewest    - TRUE to update newest entries (insert if
*                            needed)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_update(IndexHandle  *indexHandle,
                           IndexId      storageId,
                           ConstString  hostName,
                           ConstString  userName,
                           ConstString  storageName,
                           uint64       createdDateTime,
                           uint64       size,
                           ConstString  comment,
                           bool         updateNewest
                          );

/***********************************************************************\
* Name   : IndexStorage_initList
* Purpose: list storage entries
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          entityId         - index id of entity entry id or INDEX_ID_ANY
*          jobUUID          - job UUID or NULL
*          entityUUID       - schedule UUID or NULL
*          indexIds         - index ids or NULL
*          indexIdCount     - index id count or 0
*          indexTypeSet     - index type set
*          indexStateSet    - index state set
*          IndexModeSet     - index mode set
*          userName         - user name (can be NULL)
*          hostName         - host name (can be NULL)
*          name             - name (can be NULL)
*          sortMode         - sort mode; see IndexStorageSortModes
*          ordering         - ordering
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_initList(IndexQueryHandle      *indexQueryHandle,
                             IndexHandle           *indexHandle,
                             IndexId               uuidId,
                             IndexId               entityId,
                             const char            *jobUUID,
                             const char            *entityUUID,
                             const IndexId         indexIds[],
                             uint                  indexIdCount,
                             IndexTypeSet          indexTypeSet,
                             IndexStateSet         indexStateSet,
                             IndexModeSet          indexModeSet,
                             ConstString           userName,
                             ConstString           hostName,
                             ConstString           name,
                             IndexStorageSortModes sortMode,
                             DatabaseOrdering      ordering,
                             uint64                offset,
                             uint64                limit
                            );

/***********************************************************************\
* Name   : IndexStorage_getNext
* Purpose: get next index storage entry
* Input  : IndexQueryHandle    - index query handle
* Output : uuidId              - index id of UUID (can be NULL)
*          jobUUID             - job UUID (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          entityUUID          - UUID (can be NULL)
*          hostName            - host name (can be NULL)
*          userName            - user name (can be NULL)
*          createdDateTime     - created date/time (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storage             - index id of storage
*          storageName         - storage name (can be NULL)
*          dateTime            - date/time stamp [s] (can be NULL)
*          size                - storage size [bytes]
*          userName            - user name (can be NULL)
*          comment             - comment (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be
*                                NULL)
*          errorMessage        - last error message
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexStorage_getNext(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           entityUUID,
                          String           hostName,
                          String           userName,
                          String           comment,
                          uint64           *createdDateTime,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *dateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage,
                          uint             *totalEntryCount,
                          uint64           *totalEntrySize
                         );

/***********************************************************************\
* Name   : IndexStorage_getsInfos
* Purpose: get storages info
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          entityId         - index id of entity entry id or INDEX_ID_ANY
*          jobUUID          - job UUID or NULL
*          entityUUID       - schedule UUID or NULL
*          indexIds         - index ids or NULL
*          indexIdCount     - index id count or 0
*          indexTypeSet     - index type set
*          indexStateSet    - index state set or INDEX_STATE_SET_ANY
*          IndexModeSet     - index mode set
*          name             - name pattern (glob, can be NULL)
* Output : totalStorageCount - total number of storages (can be NULL)
*          totalStorageSize  - total storage size [bytes] (can be NULL)
*          totalEntryCount   - total number of entries (can be NULL)
*          totalEntrySize    - total entry size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_getsInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
                              ConstString   jobUUID,
                              ConstString   entityUUID,
                              const IndexId indexIds[],
                              uint          indexIdCount,
                              IndexTypeSet  indexTypeSet,
                              IndexStateSet indexStateSet,
                              IndexModeSet  indexModeSet,
                              ConstString   name,
                              uint          *totalStorageCount,
                              uint64        *totalStorageSize,
                              uint          *totalEntryCount,
                              uint64        *totalEntrySize
                             );

/***********************************************************************\
* Name   : IndexStorage_updateInfos
* Purpose: update storages info (aggregated values)
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_updateInfos(IndexHandle *indexHandle,
                                IndexId     storageId
                               );

/***********************************************************************\
* Name   : IndexStorage_cleanUp
* Purpose: clean-up
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_cleanUp(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : IndexStroage_addToNewest
* Purpose: add storage entries to newest entries (if newest)
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_addToNewest(IndexHandle  *indexHandle,
                                IndexId      storageId,
                                ProgressInfo *progressInfo
                               );

/***********************************************************************\
* Name   : IndexStorage_removeFromNewest
* Purpose: remove storage entries from newest entries
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_removeFromNewest(IndexHandle  *indexHandle,
                                     IndexId      storageId,
                                     ProgressInfo *progressInfo
                                    );

/***********************************************************************\
* Name   : IndexStorage_updateAggregates
* Purpose: update storage aggregates
* Input  : indexHandle - index handle
*          storageId   - storage index id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_updateAggregates(IndexHandle *indexHandle,
                                     IndexId     storageId
                                    );

/***********************************************************************\
* Name   : IndexStorage_findById
* Purpose: find info by storage id
* Input  : indexHandle   - index handle
*          findStorageId - index id of storage to find
* Output : jobUUID             - job UUID (can be NULL)
*          entityUUID          - schedule UUID (can be NULL)
*          uuidId              - index id of UUID entry (can be NULL)
*          entityId            - index id of entity entry (can be NULL)
*          storageName         - storage name (can be NULL)
*          dateTime            - date/time stamp [s] (can be NULL)
*          size                - storage size [bytes]
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be
*                                NULL)
*          errorMessage        - last error message
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : TRUE iff found
* Notes  : -
\***********************************************************************/

bool IndexStorage_findById(IndexHandle *indexHandle,
                           IndexId     findStorageId,
                           String      jobUUID,
                           String      entityUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           uint64      *dateTime,
                           uint64      *size,
                           IndexStates *indexState,
                           IndexModes  *indexMode,
                           uint64      *lastCheckedDateTime,
                           String      errorMessage,
                           uint        *totalEntryCount,
                           uint64      *totalEntrySize
                          );

/***********************************************************************\
* Name   : IndexStorage_findByName
* Purpose: find info by storage name
* Input  : indexHandle          - index handle
*          findStorageSpecifier - storage specifier to find
*          findArchiveName      - archive name to find (can be NULL)
* Output : uuidId              - index id of UUID entry (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          jobUUID             - job UUID (can be NULL)
*          entityUUID          - schedule UUID (can be NULL)
*          entityId            - index id of entity entry (can be NULL)
*          storageId           - index id of storage entry (can be NULL)
*          createdDateTime     - created date/time stamp [s] (can be NULL)
*          size                - storage size [bytes]
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be
*                                NULL)
*          errorMessage        - last error message
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : TRUE iff found
* Notes  : -
\***********************************************************************/

bool IndexStorage_findByName(IndexHandle            *indexHandle,
                             const StorageSpecifier *findStorageSpecifier,
                             ConstString            findArchiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 entityUUID,
                             IndexId                *storageId,
                             uint64                 *createdDateTime,
                             uint64                 *size,
                             IndexStates            *indexState,
                             IndexModes             *indexMode,
                             uint64                 *lastCheckedDateTime,
                             String                 errorMessage,
                             uint                   *totalEntryCount,
                             uint64                 *totalEntrySize
                            );

/***********************************************************************\
* Name   : IndexStorage_findByState
* Purpose: find storage info by state
* Input  : indexHandle       - index handle
*          findIndexStateSet - index state set to find
* Output : uuidId              - index id of UUID entry (can be NULL)
*          jobUUID             - job UUID (can be NULL)
*          entityUUID          - schedule UUID (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          storageId           - index id of storage (can be NULL)
*          storageName         - storage name (can be NULL)
*          dateTime            - date/time stamp [s] (can be NULL)
*          size                - storage size [bytes]
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be
*                                NULL)
*          errorMessage        - last error message
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : TRUE iff storage found
* Notes  : -
\***********************************************************************/

bool IndexStorage_findByState(IndexHandle   *indexHandle,
                              IndexStateSet findIndexStateSet,
                              IndexId       *uuidId,
                              String        jobUUID,
                              IndexId       *entityId,
                              String        entityUUID,
                              IndexId       *storageId,
                              String        storageName,
                              uint64        *dateTime,
                              uint64        *size,
                              IndexModes    *indexMode,
                              uint64        *lastCheckedDateTime,
                              String        errorMessage,
                              uint          *totalEntryCount,
                              uint64        *totalEntrySize
                             );

/***********************************************************************\
* Name   : IndexStorage_isEmpty
* Purpose: check if storage if empty
* Input  : indexHandle - index handle
*          storageId   - storage database id
* Output : -
* Return : TRUE iff entity is empty
* Notes  : -
\***********************************************************************/

bool IndexStorage_isEmpty(IndexHandle *indexHandle,
                          IndexId     storageId
                         );

/***********************************************************************\
* Name   : IndexStorage_delete
* Purpose: delete storage from index
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_delete(IndexHandle  *indexHandle,
                           IndexId      storageId,
                           ProgressInfo *progressInfo
                          );

/***********************************************************************\
* Name   : IndexStorage_purge
* Purpose: purge storage (mark as "deleted")
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_purge(IndexHandle  *indexHandle,
                          IndexId      storageId,
                          ProgressInfo *progressInfo
                         );

/***********************************************************************\
* Name   : IndexStorage_purgeAllById
* Purpose: purge all storages by entity id (mark as "deleted")
* Input  : indexHandle   - index handle
*          entityId      - entity id
*          keepStorageId - storage database id to keep
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_purgeAllById(IndexHandle  *indexHandle,
                                 IndexId      entityId,
                                 IndexId      keepStorageId,
                                 ProgressInfo *progressInfo
                                );

/***********************************************************************\
* Name   : IndexStorage_purgeAllByName
* Purpose: purge all storages by name (mark as "deleted")
* Input  : indexHandle      - index handle
*          storageSpecifier - storage specifier or NULL
*          archiveName      - archive name or NULL
*          keepStorageId    - storage database id to keep
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_purgeAllByName(IndexHandle            *indexHandle,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName,
                                   IndexId                keepStorageId,
                                   ProgressInfo           *progressInfo
                                  );

/***********************************************************************\
* Name   : IndexStorage_prune
* Purpose: purge storage (mark as "deleted") if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          storageId      - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_prune(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter,
                          IndexId     storageId
                         );

/***********************************************************************\
* Name   : IndexStorage_pruneAll
* Purpose: purge all storages (mark as "deleted") if
*            - empty and
*            - with state OK or ERROR
* Input  : indexHandle - index handle
*          doneFlag       - done flag variable (can be NULL)
*          deletedCounter - deleted entries count variable (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_pruneAll(IndexHandle *indexHandle,
                             bool        *doneFlag,
                             ulong       *deletedCounter
                            );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_STORAGES__ */

/* end of file */
