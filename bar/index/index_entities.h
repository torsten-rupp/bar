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
* Name   : IndexEntity_prune
* Purpose: purge entity if empty, is not default entity and is not
*          locked, purge UUID if empty
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
                         DatabaseId  entityId
                        );

/***********************************************************************\
* Name   : IndexEntity_pruneEmpty
* Purpose: prune all empty entities
* Input  : indexHandle    - index handle
*          doneFlag       - done flag (can be NULL)
*          deletedCounter - deleted entries count (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntity_pruneEmpty(IndexHandle *indexHandle,
                              bool        *doneFlag,
                              ulong       *deletedCounter
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
                                    DatabaseId  entityId
                                   );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_ENTITIES__ */

/* end of file */