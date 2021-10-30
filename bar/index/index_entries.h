/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index entry functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_ENTRIES__
#define __INDEX_ENTRIES__

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
* Name   : IndexEntry_collectIds
* Purpose: collect entry ids for storage
* Input  : entryIds     - entry ids array variable
*          indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : entryIds     - entry ids array
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_collectIds(Array        *entryIds,
                             IndexHandle  *indexHandle,
                             DatabaseId   storageId,
                             ProgressInfo *progressInfo
                            );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_ENTRIES__ */

/* end of file */
