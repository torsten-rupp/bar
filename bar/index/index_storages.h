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
* Purpose: purge storage (mark as "deleted")
* Input  : indexHandle  - index handle
*          storageId    - storage database id
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_purge(IndexHandle  *indexHandle,
                          DatabaseId   storageId,
                          ProgressInfo *progressInfo
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

/***********************************************************************\
* Name   : IndexStorage_pruneEmpty
* Purpose: prune all empty storages which have state OK or ERROR
* Input  : indexHandle  - index handle
*          progressInfo - progress info (or NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexStorage_pruneEmpty(IndexHandle  *indexHandle,
                               ProgressInfo *progressInfo
                              );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_STORAGES__ */

/* end of file */
