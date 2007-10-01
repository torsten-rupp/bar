/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.h,v $
* $Revision: 1.2 $
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

Errors Server_init(void);

void Server_done(void);

bool Server_run(uint       serverPport,
                const char *serverPassword
               );

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER__ */

/* end of file */
