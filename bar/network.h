/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/network.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

#ifndef __NETWORK__
#define __NETWORK__

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
* Name   : Network_init
* Purpose: init network functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_init(void);

/***********************************************************************\
* Name   : Network_done
* Purpose: deinitialize network functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_done(void);

/***********************************************************************\
* Name   : Network_connect
* Purpose: connect to host
* Input  : hostName - host name
*          hostPort - host port
* Output : socketHandle - socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_connect(int    *socketHandle,
                       String hostName,
                       uint   hostPort
                      );

/***********************************************************************\
* Name   : Network_disconnect
* Purpose: disconnect from host
* Input  : socketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_disconnect(int socketHandle);

/***********************************************************************\
* Name   : Network_send
* Purpose: send data to host
* Input  : socketHandle - socket handle
*          buffer       - data buffer
*          length       - length of data (in bytes)
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_send(int        socketHandle,
                    const void *buffer,
                    ulong      length
                   );

/***********************************************************************\
* Name   : Network_receive
* Purpose: 
* Input  : socketHandle - socket handle
*          buffer       - data buffer
*          maxLength    - max. length of data (in bytes)
* Output : receivedBytes - number of bytes received
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_receive(int   socketHandle,
                       void  *buffer,
                       ulong maxLength,
                       ulong *receivedBytes
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __NETWORK__ */

/* end of file */
