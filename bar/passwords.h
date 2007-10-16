/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/passwords.h,v $
* $Revision: 1.1 $
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

Errors Password_init(void);

void Password_done(void);

Password *Password_new(void);

void Password_delete(Password *password);

void Password_setCString(Password *password, const char *s);
void Password_appendChar(Password *password, char ch);

uint Password_length(const Password *password);

char Password_get(const Password *password, uint index);

const char *Password_deploy(Password *password);

void Password_undeploy(Password *password);

#ifdef __cplusplus
  }
#endif

#endif /* __PASSWORDS__ */

/* end of file */
