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
* Name   : IndexUUID_prune
* Purpose: prune empty UUID
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
                       DatabaseId  uuidId
                      );

/***********************************************************************\
* Name   : IndexUUID_pruneAll
* Purpose: prune all empty UUIDs
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
