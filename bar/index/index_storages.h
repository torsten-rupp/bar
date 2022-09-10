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
* Name   : IndexStorage_cleanUp
* Purpose: clean-up
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_cleanUp(IndexHandle *indexHandle);

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
                          DatabaseId  storageId
                         );

/***********************************************************************\
* Name   : purgeStorage
* Purpose: delete storage
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_delete(IndexHandle  *indexHandle,
                           DatabaseId   storageId,
                           ProgressInfo *progressInfo
                          );

/***********************************************************************\
* Name   : IndexStorage_purge
* Purpose: purge storage (mark as "deleted")
* Input  : indexHandle - index handle
*          storageId   - storage database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_purge(IndexHandle *indexHandle,
                          DatabaseId  storageId
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

Errors IndexStorage_purgeAllById(IndexHandle *indexHandle,
                                 DatabaseId  entityId,
                                 DatabaseId  keepStorageId
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
                                   DatabaseId             keepStorageId
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
                          DatabaseId  storageId
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
                                DatabaseId   storageId,
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
                                     DatabaseId   storageId,
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
                                     DatabaseId  storageId
                                    );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_STORAGES__ */

/* end of file */
