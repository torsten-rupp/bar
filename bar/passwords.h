/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/passwords.h,v $
* $Revision: 1.9 $
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
#include "strings.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_PASSWORD_LENGTH 255

/***************************** Datatypes *******************************/
typedef struct
{
  char *data;
  uint length;
  #ifndef HAVE_GCRYPT
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
* Name   : Password_initAll
* Purpose: initialize secure password functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Password_initAll(void);

/***********************************************************************\
* Name   : Password_doneAll
* Purpose: deinitialize secure password functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_doneAll(void);

void Password_init(Password *password);
void Password_done(Password *password);

/***********************************************************************\
* Name   : Password_new, Password_newCString
* Purpose: create new password
* Input  : s - password string
* Output : -
* Return : password
* Notes  : -
\***********************************************************************/

Password *Password_new(void);
Password *Password_newCString(const char *s);

/***********************************************************************\
* Name   : Password_duplicate
* Purpose: duplicate a password
* Input  : fromPassword - source password
* Output : -
* Return : password (password is still empty)
* Notes  : -
\***********************************************************************/

Password *Password_duplicate(const Password *fromPassword);

/***********************************************************************\
* Name   : Password_delete
* Purpose: delete password
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_delete(Password *password);

/***********************************************************************\
* Name   : Password_clear
* Purpose: clear password
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_clear(Password *password);

/***********************************************************************\
* Name   : Passwort_set, Password_setCString
* Purpose: set password from C-string
* Input  : password - password
*          s        - C-string
* Output : -
* Return : -
* Notes  : avoid usage of this functions, because 's' is insecure!
\***********************************************************************/

void Password_set(Password *password, const Password *fromPassword);
void Password_setString(Password *password, const String string);
void Password_setCString(Password *password, const char *s);

/***********************************************************************\
* Name   : Password_appendChar
* Purpose: append character to password
* Input  : password - password
*          ch       - character to append
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_appendChar(Password *password, char ch);

/***********************************************************************\
* Name   : Password_length
* Purpose: get length of password
* Input  : password - password
* Output : -
* Return : length of password
* Notes  : -
\***********************************************************************/

uint Password_length(const Password *password);

/***********************************************************************\
* Name   : Password_empty
* Purpose: check if password is empty
* Input  : password - password
* Output : -
* Return : TRUE iff password is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Password_empty(const Password *password);

/***********************************************************************\
* Name   : Password_getQualityLevel
* Purpose: get quality level of password
* Input  : password - password
* Output : -
* Return : quality level (0=bad..1=good)
* Notes  : -
\***********************************************************************/

double Password_getQualityLevel(const Password *password);

/***********************************************************************\
* Name   : Password_getChar
* Purpose: get chararacter of password
* Input  : password - password
*          index    - index (0..n)
* Output : -
* Return : character a position specified by index
* Notes  : -
\***********************************************************************/

char Password_getChar(const Password *password, uint index);

/***********************************************************************\
* Name   : Password_deploy
* Purpose: deploy password as C-string
* Input  : password - password
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
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_undeploy(Password *password);

/***********************************************************************\
* Name   : Password_inputStdin
* Purpose: input password from stdin (without echo characters)
* Input  : password - password
*          title    - dialog title text
* Output : -
* Return : TRUE if password read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Password_input(Password *password, const char *title);

#ifdef __cplusplus
  }
#endif

#endif /* __PASSWORDS__ */

/* end of file */
