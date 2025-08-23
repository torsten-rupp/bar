/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index history functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_HISTORY__
#define __INDEX_HISTORY__

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
* Name   : IndexHistory_new
* Purpose: create new history entry
* Input  : indexHandle  - index handle
*          jobUUID           - job UUID
*          entityUUID        - schedule UUID (can be NULL)
*          hostName          - host name (can be NULL)
*          userName          - user name (can be NULL)
*          archiveType       - archive type
*          createdDateTime   - create date/time stamp [s]
*          errorMessage      - last storage error message (can be NULL)
*          duration          - duration [s]
*          totalEntryCount   - total number of entries
*          totalEntrySize    - total storage size [bytes]
*          skippedEntryCount - number of skipped entries
*          skippedEntrySize  - size of skipped entries [bytes]
*          errorEntryCount   - number of error entries
*          errorEntrySize    - size of error entries [bytes]
* Output : historyId - index id of new history entry (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexHistory_new(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  entityUUID,
                        ConstString  userName,
                        ConstString  hostName,
//TODO: entityId?
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        uint64       duration,
                        uint         totalEntryCount,
                        uint64       totalEntrySize,
                        uint         skippedEntryCount,
                        uint64       skippedEntrySize,
                        uint         errorEntryCount,
                        uint64       errorEntrySize,
                        IndexId      *historyId
                       );

/***********************************************************************\
* Name   : IndexHistory_delete
* Purpose: delete history entry
* Input  : indexQueryHandle - index query handle
*          historyId        - index id of history entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexHistory_delete(IndexHandle *indexHandle,
                           IndexId     historyId
                          );

/***********************************************************************\
* Name   : IndexHistory_initList
* Purpose: list history
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry (can be NULL)
*          jobUUID          - unique job UUID (can be NULL)
*          ordering         - ordering
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexHistory_initList(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             ConstString      jobUUID,
                             DatabaseOrdering ordering,
                             uint64           offset,
                             uint64           limit
                            );

/***********************************************************************\
* Name   : IndexHistory_getNext
* Purpose: get next history entry
* Input  : IndexQueryHandle - index query handle
* Output : historyId         - index id of history entry (can be NULL)
*          uuidId            - UUID index id (can be NULL)
*          jobUUID           - job UUID (can be NULL)
*          entityUUID        - schedule UUID (can be NULL)
*          hostName          - host name (can be NULL)
*          userName          - user name (can be NULL)
*          createdDateTime   - create date/time stamp [s] (can be NULL)
*          errorMessage      - last storage error message (can be NULL)
*          duration          - duration [s]
*          totalEntryCount   - total number of entries (can be NULL)
*          totalEntrySize    - total storage size [bytes] (can be NULL)
*          skippedEntryCount - number of skipped entries (can be NULL)
*          skippedEntrySize  - size of skipped entries [bytes] (can be
*                              NULL)
*          errorEntryCount   - number of error entries (can be NULL)
*          errorEntrySize    - size of error entries [bytes] (can be
*                              NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexHistory_getNext(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          IndexId          *uuidId,
                          String           jobUUID,
                          String           entityUUID,
//TODO: entityId?
                          String           hostName,
                          String           userName,
                          ArchiveTypes     *archiveType,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          uint64           *duration,
                          uint             *totalEntryCount,
                          uint64           *totalEntrySize,
                          uint             *skippedEntryCount,
                          uint64           *skippedEntrySize,
                          uint             *errorEntryCount,
                          uint64           *errorEntrySize
                         );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_HISTORY__ */

/* end of file */
