/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index entity functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_ENTITIES__
#define __INDEX_ENTITIES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "errors.h"

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
* Name   : IndexEntity_new
* Purpose: create new entity index and new uuid index (if need)
* Input  : indexHandle     - index handle
*          jobUUID         - job UUID
*          entityUUID      - schedule UUID (can be NULL)
*          hostName        - host name (can be NULL)
*          userName        - user name (can be NULL)
*          archiveType     - archive type
*          createdDateTime - created date/time stamp or 0
*          locked          - TRUE for locked entity
* Output : entityId - index id of new entity index
* Return : ERROR_NONE or error code
* Notes  : entity have to be unlocked!
\***********************************************************************/

Errors IndexEntity_new(IndexHandle  *indexHandle,
                       const char   *jobUUID,
                       const char   *entityUUID,
                       const char   *hostName,
                       const char   *userName,
                       ArchiveTypes archiveType,
                       uint64       createdDateTime,
                       bool         locked,
                       IndexId      *entityId
                      );

/***********************************************************************\
* Name   : IndexEntity_update
* Purpose: update storage index
* Input  : indexHandle     - index handle
*          entityId        - index id of entity
*          jobUUID         - job UUID (can be NULL)
*          entityUUID      - schedule UUID (can be NULL)
*          hostName        - host name (can be NULL)
*          userName        - user name (can be NULL)
*          archiveType     - archive type
*          createdDateTime - create date/time
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_update(IndexHandle  *indexHandle,
                          IndexId      entityId,
                          const char   *jobUUID,
                          const char   *entityUUID,
                          const char   *hostName,
                          const char   *userName,
                          ArchiveTypes archiveType,
                          uint64       createdDateTime
                         );

/***********************************************************************\
* Name   : IndexEntity_lock
* Purpose: lock entity index
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_lock(IndexHandle *indexHandle,
                        IndexId     entityId
                       );

/***********************************************************************\
* Name   : IndexEntity_unlock
* Purpose: unlock entity index
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_unlock(IndexHandle *indexHandle,
                          IndexId     entityId
                         );

/***********************************************************************\
* Name   : IndexEntity_isLocked
* Purpose: check if entity is locked
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity
* Return : TRUE iff entity is locked
* Notes  : -
\***********************************************************************/

bool IndexEntity_isLocked(IndexHandle *indexHandle,
                          IndexId     entityId
                         );

/***********************************************************************\
* Name   : IndexEntity_isEmpty
* Purpose: check if entity entry is empty (no storages)
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

bool IndexEntity_isEmpty(IndexHandle *indexHandle,
                         IndexId     entityId
                        );

/***********************************************************************\
* Name   : IndexEntity_initList
* Purpose: list entity entries and aggregated data of storage
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry
*          jobUUID          - job UUID (can be NULL)
*          archiveType      - archive type or ARCHIVE_TYPE_ANY
*          entityUUID       - schedule UUID (can be NULL)
*          indexStateSet    - index state set or INDEX_STATE_SET_ANY
*          IndexModeSet     - index mode set
*          name             - storage name (can be NULL)
*          sortMode         - sort mode; see IndexEntitySortModes
*          ordering         - ordering mode
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_initList(IndexQueryHandle     *indexQueryHandle,
                            IndexHandle          *indexHandle,
                            IndexId              uuidId,
//TODO: remove?
                            ConstString          jobUUID,
                            ConstString          entityUUID,
                            ArchiveTypes         archiveType,
                            IndexStateSet        indexStateSet,
                            IndexModeSet         indexModeSet,
                            ConstString          name,
                            IndexEntitySortModes sortMode,
                            DatabaseOrdering     ordering,
                            uint64               offset,
                            uint64               limit
                           );

/***********************************************************************\
* Name   : IndexEntity_getNext
* Purpose: get next index entity entry
* Input  : IndexQueryHandle - index query handle
* Output : indexUUIDId      - index id of UUID (can be NULL)
*          jobUUID          - job UUID (can be NULL)
*          entityUUID       - schedule UUID (can be NULL)
*          entityId         - index id of entity
*          archiveType      - archive type (can be NULL)
*          createdDateTime  - created date/time stamp [s] (can be NULL)
*          lastErrorCode    - last storage error code (can be NULL)
*          lastErrorData    - last storage error data (can be NULL)
*          totalSize        - total sum of storage size [bytes] (can be
*                             NULL)
*          totalEntryCount  - total number of entries (can be NULL)
*          totalEntrySize   - total sum of entry size [bytes] (can be
*                             NULL)
*          maxIndexState    - max. index state (can be NULL)
*          maxIndexMode     - max. index mode (can be NULL)
*          lockedCount      - locked count (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntity_getNext(IndexQueryHandle *indexQueryHandle,
                         IndexId          *indexUUIDId,
                         String           jobUUID,
                         String           entityUUID,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         uint             *lastErrorCode,
                         String           lastErrorData,
                         uint64           *totalSize,
                         uint             *totalEntryCount,
                         uint64           *totalEntrySize,
                         IndexStates      *maxIndexState,
                         IndexModes       *maxIndexMode,
                         uint             *lockedCount
                        );

/***********************************************************************\
* Name   : IndexEntity_find
* Purpose: find entity info
* Input  : indexHandle         - index handle
*          findEntityId        - index id of entity to find (can be
*                                INDEX_ID_NONE)
*          findJobUUID         - unique job UUID to find (can be NULL)
*          findEntityUUID      - unique entity UUID to find (can be
*                                NULL)
*          findHostName        - host name (can be NULL)
*          findArchiveType     - archive type to find (can be
*                                ARCHIVE_TYPE_NONE)
*          findCreatedDateTime - create date/time to find (can be 0)
* Output : jobUUID          - job UUID (can be NULL)
*          entityUUID       - schedule UUID (can be NULL)
*          uuidId           - index id of UUID entry (can be NULL)
*          entityId         - index id of entity entry (can be NULL)
*          archiveType      - archive type (can be NULL)
*          createdDateTime  - created date/time stamp [s] (can be NULL)
*          lastErrorMessage - last error message (can be NULL)
*          totalEntryCount  - total number of entries (can be NULL)
*          totalEntrySize   - total size [bytes] (can be NULL)
* Return : TRUE iff found
* Notes  : -
\***********************************************************************/

bool IndexEntity_find(IndexHandle  *indexHandle,
                      IndexId      findEntityId,
                      ConstString  findJobUUID,
                      ConstString  findEntityUUID,
                      ConstString  findHostName,
                      ArchiveTypes findArchiveType,
                      uint64       findCreatedDate,
                      uint64       findCreatedTime,
                      String       jobUUID,
                      String       entityUUID,
                      IndexId      *uuidId,
                      IndexId      *entityId,
                      ArchiveTypes *archiveType,
                      uint64       *createdDateTime,
                      String       lastErrorMessage,
                      uint         *totalEntryCount,
                      uint64       *totalEntrySize
                     );

/***********************************************************************\
* Name   : IndexEntity_getInfos
* Purpose: get entities infos
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          entityId         - index id of entity entry id or INDEX_ID_ANY
*          jobUUID          - unique job UUID or NULL
*          indexIds         - index ids or NULL
*          indexIdCount     - index id count or 0
*          name             - name pattern (glob, can be NULL)
* Output : totalStorageCount - total number of storages (can be NULL)
*          totalStorageSize  - total size of entries [bytes] (can be
*                              NULL)
*          totalEntryCount   - total entry count (can be NULL)
*          totalEntrySize    - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_getInfos(IndexHandle   *indexHandle,
                            IndexId       uuidId,
                            IndexId       entityId,
//TODO: remove?
                            ConstString   jobUUID,
                            const IndexId indexIds[],
                            uint          indexIdCount,
                            ConstString   name,
                            uint          *totalStorageCount,
                            uint64        *totalStorageSize,
                            uint          *totalEntryCount,
                            uint64        *totalEntrySize
                           );

/***********************************************************************\
* Name   : IndexEntity_updateInfos
* Purpose: update storages info (aggregated values)
* Input  : indexHandle - index handle
*          entityId    - index id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_updateInfos(IndexHandle *indexHandle,
                               IndexId     entityId
                              );

/***********************************************************************\
* Name   : IndexEntity_updateAggregates
* Purpose: update entity aggregates
* Input  : indexHandle - index handle
*          entityId    - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_updateAggregates(IndexHandle *indexHandle,
                                    IndexId     entityId
                                   );

/***********************************************************************\
* Name   : IndexEntity_cleanUp
* Purpose: clean-up
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_cleanUp(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : IndexEntity_delete
* Purpose: delete entity
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          entityId       - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

// TODO: purge?
Errors IndexEntity_delete(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter,
                          IndexId     entityId
                         );

/***********************************************************************\
* Name   : IndexEntity_isDeleted
* Purpose: check if entity entry is deleted
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity
* Return : TRUE iff deleted
* Notes  : -
\***********************************************************************/

bool IndexEntity_isDeleted(IndexHandle *indexHandle,
                           IndexId     entityId
                          );

/***********************************************************************\
* Name   : IndexEntity_purge
* Purpose: purge entity from index if empty (mark as "deleted")
* Input  : indexHandle - index handle
*          entityId     - index id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_purge(IndexHandle *indexHandle,
                         IndexId     entityId
                        );

/***********************************************************************\
* Name   : IndexEntity_purge
* Purpose: purge entity (mark as "deleted")
* Input  : indexHandle - index handle
*          entityId    - entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_purge(IndexHandle *indexHandle,
                         IndexId     entityId
                        );

/***********************************************************************\
* Name   : IndexEntity_prune
* Purpose: purge entity (mark as "deleted") if
*            - empty,
*            - is not default entity and
*            - is not locked
*          purge UUID of entity if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          entityId       - entity database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_prune(IndexHandle *indexHandle,
                         bool        *doneFlag,
                         ulong       *deletedCounter,
                         IndexId     entityId
                        );

/***********************************************************************\
* Name   : IndexEntity_pruneAll
* Purpose: purge all entities (mark as "deleted") if
*            - empty,
*            - is not default entity and
*            - is not locked
*          purge UUID of entity if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag variable (can be NULL)
*          deletedCounter - deleted entries count variable (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_pruneAll(IndexHandle *indexHandle,
                            bool        *doneFlag,
                            ulong       *deletedCounter
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_ENTITIES__ */

/* end of file */
