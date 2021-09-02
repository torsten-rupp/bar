/***********************************************************************\
*
* $Source$
* $Revision$
* $Author$
* Contents: crash mini dump functions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory 

#include <stdlib.h>
#include <assert.h>

#include "common/global.h"

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
* Name   : MiniDump_init
* Purpose: initialize minidump
* Input  : -
* Output : -
* Return : TRUE iff minidump initialized
* Notes  : -
\***********************************************************************/

bool MiniDump_init(void);

/***********************************************************************\
* Name   : MiniDump_done
* Purpose: deinitialize minidump
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MiniDump_done(void);

#ifdef __cplusplus
  }
#endif


/* end of file */
