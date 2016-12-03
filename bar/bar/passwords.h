/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for secure storage of passwords
* Systems: all
*
\***********************************************************************/

#ifndef __PASSWORDS__
#define __PASSWORDS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_PASSWORD_LENGTH 256

#define PASSWORD_INPUT_MODE_CONSOLE (1 << 0)
#define PASSWORD_INPUT_MODE_GUI     (1 << 1)

#define PASSWORD_INPUT_MODE_ANY     (PASSWORD_INPUT_MODE_CONSOLE | \
                                     PASSWORD_INPUT_MODE_GUI \
                                    )

/***************************** Datatypes *******************************/
typedef struct
{
  char *data;
  uint dataLength;
  void *keyData;
  uint keyDataLength;
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
* Return : ERROR_NONE or error code
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

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Password_allocSecure
* Purpose: allocate secure memory
* Input  : size - size of memory block
* Output : -
* Return : secure memory or NULL iff insufficient memory
* Notes  : -
\***********************************************************************/

void *Password_allocSecure(size_t size);

/***********************************************************************\
* Name   : Password_freeSecure
* Purpose: free secure memory
* Input  : p - secure memory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_freeSecure(void *p);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Password_init
* Purpose: initialize password
* Input  : password - password variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_init(Password *password);

/***********************************************************************\
* Name   : Password_done
* Purpose: deinitialize password
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

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
Password *Password_newString(const String string);
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
* Purpose: set password
* Input  : password - password
*          fromPassword - set from password
*          string       - string
*          s            - C-string
*          buffer       - buffer
*          length       - length of buffer
* Output : -
* Return : -
* Notes  : avoid usage of functions Password_setCString and
*          Password_setBuffer, because data is insecure!
\***********************************************************************/

void Password_set(Password *password, const Password *fromPassword);
void Password_setString(Password *password, const String string);
void Password_setCString(Password *password, const char *s);
void Password_setBuffer(Password *password, const void *buffer, uint length);

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
* Name   : Password_random
* Purpose: set password to random value
* Input  : password - password
*          length   - length of password (bytes)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_random(Password *password, uint length);

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
* Name   : Password_isEmpty
* Purpose: check if password is empty
* Input  : password - password
* Output : -
* Return : TRUE iff password is NULL or empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Password_isEmpty(const Password *password);

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
* Return : plain password text
* Notes  : use deploy as less and as short a possible! If no secure
*          memory is available the password will be stored as plain text
*          in standard memory which maybe read
\***********************************************************************/

const char *Password_deploy(const Password *password);

/***********************************************************************\
* Name   : Password_undeploy
* Purpose: undeploy password
* Input  : password - password
*          plain    - plain password text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_undeploy(const Password *password, const char *plain);

/***********************************************************************\
* Name   : Password_equals
* Purpose: check if passwords are equal
* Input  : password0,password1 - passwords to compare
* Output : -
* Return : TURE iff passwords are equal
* Notes  : -
\***********************************************************************/

bool Password_equals(const Password *password0, const Password *password1);

/***********************************************************************\
* Name   : Password_input
* Purpose: input password from stdin (without echo characters)
* Input  : password - password
*          message  - message text
*          modes    - input mode set; see PASSWORD_INPUT_MODE_*
* Output : -
* Return : TRUE if password read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Password_input(Password   *password,
                    const char *message,
                    uint       modes
                   );

/***********************************************************************\
* Name   : Password_inputVerify
* Purpose: verify input of password
* Input  : password - password to verify
*          message  - message text
*          modes    - input mode set; see PASSWORD_INPUT_MODE_*
* Output : -
* Return : TRUE if passwords equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Password_inputVerify(const Password *password,
                          const char     *message,
                          uint           modes
                         );

#ifdef __cplusplus
  }
#endif

#endif /* __PASSWORDS__ */

/* end of file */
