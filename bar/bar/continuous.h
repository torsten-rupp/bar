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
* Name   : Continuous_add
* Purpose: add continuous entry
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          name        - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_add(ConstString jobUUID,
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
* Name   : Continuous_initList
* Purpose: list continous entries
* Input  : jobUUID - job UUID
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_initList(DatabaseQueryHandle *databaseQueryHandle,
                           ConstString         jobUUID
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
