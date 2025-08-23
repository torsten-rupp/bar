/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index assign functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_ASSIGN__
#define __INDEX_ASSIGN__

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
* Name   : IndexAssign_to
* Purpose: assign job/entity/storage to other entity/storage
* Input  : indexHandle   - index handle
*          jobUUID       - job UUID (can be NULL)
*          entityId      - index id of entity index (can be
*                          INDEX_ID_NONE)
*          storageId     - index id of storage index (can be
*                          INDEX_ID_NONE)
*          toJobUUID     - to job UUID (can be NULL)
*          toEntityId    - to entity id (can be INDEX_ID_NONE)
*          toArchiveType - to archive type (can be ARCHIVE_TYPE_NONE)
*          toStorageId   - to storage id (can be INDEX_ID_NONE)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexAssign_to(IndexHandle  *indexHandle,
//TODO: uuidId?
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      ConstString  toJobUUID,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_ASSIGN__ */

/* end of file */
