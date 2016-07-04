/***********************************************************************\
*
* $Revision: 4195 $
* $Date: 2015-10-17 10:41:02 +0200 (Sat, 17 Oct 2015) $
* $Author: torsten $
* Contents: continous functions
* Systems: all
*
\***********************************************************************/

#ifndef __CONTINUOUS__
#define __CONTINUOUS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "database.h"
#include "errors.h"

#include "entrylists.h"
#include "patternlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Continuous_initAll
* Purpose: initialize continuous functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Continuous_initAll(void);

/***********************************************************************\
* Name   : Continuous_doneAll
* Purpose: deinitialize continuous functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_doneAll(void);

/***********************************************************************\
* Name   : Continuous_init
* Purpose: init continuous
* Input  : databaseFileName - database file name or NULL for in-memory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Continuous_init(const char *databaseFileName);

/***********************************************************************\
* Name   : Continuous_done
* Purpose: done continuous
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_done(void);

/***********************************************************************\
* Name   : Continuous_initNotify
* Purpose: init notift for job
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_initNotify(ConstString     jobUUID,
                             ConstString     scheduleUUID,
                             const EntryList *entryList
                            );

/***********************************************************************\
* Name   : Continuous_doneNotify
* Purpose: done notify for job
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_doneNotify(ConstString jobUUID,
                             ConstString scheduleUUID
                            );

/***********************************************************************\
* Name   : Continuous_add
* Purpose: add continuous entry
* Input  : indexHandle  - index handle
*          jobUUID      - job UUID
*          scheduleUUID - schedule UUID
*          name         - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_add(ConstString jobUUID,
                      ConstString scheduleUUID,
                      ConstString name
                     );

/***********************************************************************\
* Name   : Continuous_remove
* Purpose: remove continuous entry
* Input  : databaseId - database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_remove(DatabaseId databaseId);

/***********************************************************************\
* Name   : Continuous_removeNext
* Purpose: remove and return next continuous entry
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : name - name of entry
* Return : TRUE if entry removed
* Notes  : -
\***********************************************************************/

bool Continuous_removeNext(ConstString jobUUID,
                           ConstString scheduleUUID,
                           String      name
                          );

/***********************************************************************\
* Name   : Continuous_isAvailable
* Purpose: check if continuous entries available
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : -
* Return : TRUE if continuous entries available
* Notes  : -
\***********************************************************************/

bool Continuous_isAvailable(ConstString jobUUID,
                            ConstString scheduleUUID
                           );

/***********************************************************************\
* Name   : Continuous_initList
* Purpose: list continous entries
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_initList(DatabaseQueryHandle *databaseQueryHandle,
                           ConstString         jobUUID,
                           ConstString         scheduleUUID
                          );

/***********************************************************************\
* Name   : Continuous_doneList
* Purpose: done index list
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_doneList(DatabaseQueryHandle *databaseQueryHandle);

/***********************************************************************\
* Name   : Continuous_getNext
* Purpose: get next special entry
* Input  : databaseQueryHandle - database query handle
* Output : databaseId - database id of entry
*          name       - name
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Continuous_getNext(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              name
                       );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX__ */

/* end of file */
