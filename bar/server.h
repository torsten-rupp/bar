/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.h,v $
* $Revision: 1.7 $
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

#include "passwords.h"

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
* Input  : serverPort     - server port (or 0 to disable)
*          serverSSLPort  - server SSL port (or 0 to disable)
*          caFileName   - file with TLS CA or NULL
*          certFileName - file with TLS cerificate or NULL
*          keyFileName  - file with TLS key or NULL
*          serverPassword - server authenfication password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_run(uint       serverPort,
                  uint       serverSSLPort,
                  const char *caFileName,
                  const char *certFileName,
                  const char *keyFileName,
                  Password   *serverPassword
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER__ */

/* end of file */
