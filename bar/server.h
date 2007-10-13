/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

#ifndef __SERVER__
#define __SERVER__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

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
* Name   : Server_init
* Purpose: initialize server
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_init(void);

/***********************************************************************\
* Name   : Server_done
* Purpose: deinitialize server
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Server_done(void);

/***********************************************************************\
* Name   : Server_run
* Purpose: run server
* Input  : serverPort     - server port
*          serverPassword - server authenfication password
*          serverType     - server type; see SERVER_TYPE_*
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_run(uint        serverPport,
                  const char  *serverPassword,
                  ServerTypes serverType
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER__ */

/* end of file */
