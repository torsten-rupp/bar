/***********************************************************************\
*
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

#include "common/global.h"
#include "common/strings.h"

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

/***********************************************************************\
* Name   : PASSWORD_DEPLOY_DO
* Purpose: deploy password and execute block
* Input  : plainPassword - plain password
*          password      - password
* Output : -
* Return : -
* Notes  : usage:
*            PASSWORD_DEPLOY_DO(plainPassword,password)
*            {
*              ...
*            }
*
*          plainPassword must be undeployed manually if 'break' or
*          'return' is used!
\***********************************************************************/

#define PASSWORD_DEPLOY_DO(plainPassword,password) \
  for (const char *plainPassword = Password_deploy(password); \
       plainPassword != NULL; \
       Password_undeploy(password,plainPassword), plainPassword = NULL \
      )

#ifndef NDEBUG
  #define Password_init(...)          __Password_init         (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_initDuplicate(...) __Password_initDuplicate(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_done(...)          __Password_done         (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_new(...)           __Password_new          (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_newString(...)     __Password_newString    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_newCString(...)    __Password_newCString   (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_duplicate(...)     __Password_duplicate    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Password_delete(...)        __Password_delete       (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
* Name   : Password_init
* Purpose: initialize password
* Input  : password - password variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Password_init(Password *password);
#else /* not NDEBUG */
  void __Password_init(const char *__fileName__,
                       ulong      __lineNb__,
                       Password   *password
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Password_intiDuplicate
* Purpose: initialize duplicate password
* Input  : password     - password variable
*          fromPassword - from password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Password_initDuplicate(Password *password, const Password *fromPassword);
#else /* not NDEBUG */
  void __Password_initDuplicate(const char     *__fileName__,
                                ulong          __lineNb__,
                                Password       *password,
                                const Password *fromPassword
                               );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Password_done
* Purpose: deinitialize password
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Password_done(Password *password);
#else /* not NDEBUG */
  void __Password_done(const char *__fileName__,
                       ulong      __lineNb__,
                       Password   *password
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Password_new, Password_newString, Password_newCString
* Purpose: create new password
* Input  : s - password string
* Output : -
* Return : password
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Password *Password_new(void);
#else /* not NDEBUG */
  Password *__Password_new(const char *__fileName__,
                           ulong      __lineNb__
                          );
#endif /* NDEBUG */
#ifdef NDEBUG
  Password *Password_newString(const String string);
#else /* not NDEBUG */
  Password *__Password_newString(const char   *__fileName__,
                                 ulong        __lineNb__,
                                 const String string
                                );
#endif /* NDEBUG */
#ifdef NDEBUG
  Password *Password_newCString(const char *s);
#else /* not NDEBUG */
  Password *__Password_newCString(const char *__fileName__,
                                  ulong      __lineNb__,
                                  const char *s
                                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Password_duplicate
* Purpose: duplicate a password
* Input  : fromPassword - source password
* Output : -
* Return : password (password is still empty)
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Password *Password_duplicate(const Password *fromPassword);
#else /* not NDEBUG */
  Password *__Password_duplicate(const char     *__fileName__,
                                 ulong          __lineNb__,
                                 const Password *fromPassword
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Password_delete
* Purpose: delete password
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Password_delete(Password *password);
#else /* not NDEBUG */
  void __Password_delete(const char *__fileName__,
                         ulong      __lineNb__,
                         Password   *password
                        );
#endif /* NDEBUG */

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
* Input  : password      - password
*          plainPassword - plain password text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Password_undeploy(const Password *password, const char *plainPassword);

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

#ifndef NDEBUG
/***********************************************************************\
* Name   : Password_dump
* Purpose: dump password
* Input  : password - password
* Output : -
* Return : -
* Notes  : Debug only!
\***********************************************************************/

void Password_dump(const Password *password);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __PASSWORDS__ */

/* end of file */
