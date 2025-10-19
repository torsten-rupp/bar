/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index UUID functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_UUIDS__
#define __INDEX_UUIDS__

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
* Name   : IndexUUID_find
* Purpose: find UUID
* Input  : indexHandle    - index handle
*          findJobUUID    - unique job UUID or NULL
*          findEntityUUID - unique schedule UUID or NULL
* Output : uuidId                      - index id of UUID entry
*          executionCountNormal        - number of 'normal' executions
*          executionCountFull          - number of 'full' executions
*          executionCountIncremental   - number of 'incremental' executions
*          executionCountDifferential  - number of 'differential' executions
*          executionCountContinuous    - number of 'continuous' executions
*          averageDurationNormal       - average duration of 'normal' execution
*          averageDurationFull         - average duration of 'full' execution
*          averageDurationIncremental  - average duration of 'incremental' execution
*          averageDurationDifferential - average duration of 'differential' execution
*          averageDurationContinuous   - average duration of 'continuous' execution
*          totalEntityCount            - total number of entities (can be NULL)
*          totalStorageCount           - total number of storages (can be NULL)
*          totalStorageSize            - total storages size [bytes] (can be NULL)
*          totalEntryCount             - total entry count (can be NULL)
*          totalEntrySize              - total entries size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

bool IndexUUID_find(IndexHandle  *indexHandle,
                    const char   *findJobUUID,
                    const char   *findEntityUUID,
                    IndexId      *uuidId,
                    uint         *executionCountNormal,
                    uint         *executionCountFull,
                    uint         *executionCountIncremental,
                    uint         *executionCountDifferential,
                    uint         *executionCountContinuous,
                    uint64       *averageDurationNormal,
                    uint64       *averageDurationFull,
                    uint64       *averageDurationIncremental,
                    uint64       *averageDurationDifferential,
                    uint64       *averageDurationContinuous,
                    uint         *totalEntityCount,
                    uint         *totalStorageCount,
                    uint64       *totalStorageSize,
                    uint         *totalEntryCount,
                    uint64       *totalEntrySize
                   );

/***********************************************************************\
* Name   : IndexUUID_getInfos
* Purpose: get UUIDs infos
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          jobUUID          - unique job UUID or NULL
*          entityUUID       - unique schedule UUID or NULL
*          name             - name pattern (glob, can be NULL)
* Output : lastExecutedDateTime - last executed date/time (can be NULL)
*          totalEntityCount     - total number of entities (can be NULL)
*          totalEntryCount      - total entry count (can be NULL)
*          totalEntrySize       - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_getInfos(IndexHandle *indexHandle,
                           IndexId     uuidId,
//TODO: remove?
                           ConstString jobUUID,
                           ConstString entityUUID,
                           ConstString name,
                           uint64      *lastExecutedDateTime,
                           uint        *totalEntityCount,
                           uint        *totalEntryCount,
                           uint64      *totalEntrySize
                          );

/***********************************************************************\
* Name   : IndexUUID_updateInfos
* Purpose: update UUID infos (aggregated values)
* Input  : indexHandle - index handle
*          uuidId      - index of UUID entry
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_updateInfos(IndexHandle *indexHandle,
                             IndexId     uuidId
                            );

/***********************************************************************\
* Name   : IndexUUID_initList
* Purpose: list uuid entries and aggregated data of entities
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexStateSet    - index state set or INDEX_STATE_SET_ANY
*          IndexModeSet     - index mode set
*          name             - storage name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_initList(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           IndexStateSet    indexStateSet,
                           IndexModeSet     indexModeSet,
                           ConstString      name,
                           uint64           offset,
                           uint64           limit
                          );

/***********************************************************************\
* Name   : IndexUUID_getNext
* Purpose: get next index uuid entry
* Input  : IndexQueryHandle - index query handle
* Output : indexId              - index id
*          jobUUID              - unique job UUID (can be NULL)
*          lastExecutedDateTime - last executed date/time stamp [s] (can
*                                 be NULL)
*          lastErrorCode        - last error code (can be NULL)
*          lastErrorData        - last error data (can be NULL)
*          totalSize            - total sum of storage size [bytes] (can
*                                 be NULL)
*          totalEntryCount      - total number of entries (can be NULL)
*          totalEntrySize       - total sum of entry size [bytes] (can
*                                 be NULL)
*          maxIndexState        - max. index state (can be NULL)
*          maxIndexMode         - max. index mode (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexUUID_getNext(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           jobUUID,
                       uint64           *lastExecutedDateTime,
                       uint             *lastErrorCode,
                       String           lastErrorData,
                       uint64           *totalSize,
                       uint             *totalEntryCount,
                       uint64           *totalEntrySize,
                       IndexStates      *maxIndexState,
                       IndexModes       *maxIndexMode
                      );

/***********************************************************************\
* Name   : IndexUUID_new
* Purpose: create new UUID index
* Input  : indexHandle  - index handle
*          jobUUID      - unique job UUID (can be NULL)
* Output : uuidId - index id of new UUID entry (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_new(IndexHandle *indexHandle,
                     const char  *jobUUID,
                     IndexId     *uuidId
                    );

/***********************************************************************\
* Name   : IndexUUID_isEmpty
* Purpose: check if UUID if empty
* Input  : indexHandle - index handle
*          uuidId      - UUID database id
* Output : -
* Return : TRUE iff UUID is empty
* Notes  : -
\***********************************************************************/

bool IndexUUID_isEmpty(IndexHandle *indexHandle,
                       IndexId     uuidId
                      );

/***********************************************************************\
* Name   : IndexUUID_delete
* Purpose: delete job UUID index including all attached index entries
*          (entities, entries)
* Input  : indexQueryHandle - index query handle
*          uuidId           - index id of UUID entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_delete(IndexHandle *indexHandle,
                        IndexId     uuidId
                       );

/***********************************************************************\
* Name   : IndexUUID_cleanUp
* Purpose: clean-up
* Input  : indexHandle    - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_cleanUp(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : IndexUUID_prune
* Purpose: purge UUID (mark as "deleted")
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          uuidId         - UUID database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_purge(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       IndexId     uuidId
                      );

/***********************************************************************\
* Name   : IndexUUID_prune
* Purpose: purge UUID (mark as "deleted") if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
*          uuidId         - UUID database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_prune(IndexHandle *indexHandle,
                       bool        *doneFlag,
                       ulong       *deletedCounter,
                       IndexId     uuidId
                      );

/***********************************************************************\
* Name   : IndexUUID_pruneAll
* Purpose: purge all UUIDs (mark as "deleted") if empty
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexUUID_pruneAll(IndexHandle *indexHandle,
                          bool        *doneFlag,
                          ulong       *deletedCounter
                         );


#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_UUIDS__ */

/* end of file */
