/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/passwords.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: functions for secure storage of passwords
* Systems: all
*
\***********************************************************************/

#ifndef __PASSWORDS__
#define __PASSWORDS__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_PASSWORD_LENGTH 255

/***************************** Datatypes *******************************/
typedef struct
{
  char data[MAX_PASSWORD_LENGTH+1];
  uint length;
  #ifdef HAVE_GCRYPT
    char plain[MAX_PASSWORD_LENGTH+1];     /* needed for temporary storage
                                              of plain password if secure
                                              memory from gcrypt is not
                                              available
                                           */
  #endif /* HAVE_GCRYPT */
} Password;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Password_init
* Purpose: initialize secure password functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Password_init(void);

/***********************************************************************\
* Name   : Password_done
* Purpose: deinitialize secure password functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_done(void);

/***********************************************************************\
* Name   : Password_new
* Purpose: create new password
* Input  : -
* Output : -
* Return : password handle (password is still empty)
* Notes  : -
\***********************************************************************/

Password *Password_new(void);

/***********************************************************************\
* Name   : Password_delete
* Purpose: delete password
* Input  : password - password handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_delete(Password *password);

/***********************************************************************\
* Name   : Password_copy
* Purpose: copy a password
* Input  : sourcePassword - source password
* Output : -
* Return : password handle (password is still empty)
* Notes  : -
\***********************************************************************/

Password *Password_copy(Password *sourcePassword);

/***********************************************************************\
* Name   : Password_setCString
* Purpose: set password from C-string
* Input  : password - password handle
*          s        - C-string
* Output : -
* Return : -
* Notes  : avoid usage of this functions, because 's' is insecure!
\***********************************************************************/

void Password_setCString(Password *password, const char *s);

/***********************************************************************\
* Name   : Password_appendChar
* Purpose: append character to password
* Input  : password - password handle
*          ch       - character to append
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_appendChar(Password *password, char ch);

/***********************************************************************\
* Name   : Password_length
* Purpose: get length of password
* Input  : password - password handle
* Output : -
* Return : length of password
* Notes  : -
\***********************************************************************/

uint Password_length(const Password *password);

/***********************************************************************\
* Name   : Password_getChar
* Purpose: get chararacter of password
* Input  : password - password handle
*          index    - index (0..n)
* Output : -
* Return : character a position specified by index
* Notes  : -
\***********************************************************************/

char Password_getChar(const Password *password, uint index);

/***********************************************************************\
* Name   : Password_deploy
* Purpose: deploy password as C-string
* Input  : password - password handle
* Output : -
* Return : C-string
* Notes  : use depoy as less and as short a possible! If no secure
*          memory is available the password will be stored a plain text
*          in memory.
\***********************************************************************/

const char *Password_deploy(Password *password);

/***********************************************************************\
* Name   : Password_undeploy
* Purpose: undeploy password
* Input  : password - password handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_undeploy(Password *password);

#ifdef __cplusplus
  }
#endif

#endif /* __PASSWORDS__ */

/* end of file */
