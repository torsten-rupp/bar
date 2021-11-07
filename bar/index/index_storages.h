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
