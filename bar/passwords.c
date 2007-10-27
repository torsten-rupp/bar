/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/passwords.c,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: functions for secure storage of passwords
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_GCRYPT
  #include <gcrypt.h>
#endif /* HAVE_GCRYPT */
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

#include "passwords.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifndef HAVE_GCRYPT
  LOCAL char obfuscator[MAX_PASSWORD_LENGTH];
#endif /* not HAVE_GCRYPT */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Password_initAll(void)
{
  #ifndef HAVE_GCRYPT
    int z;
  #endif /* not HAVE_GCRYPT */

  #ifndef HAVE_GCRYPT
    /* if libgrypt is not available a simple obfuscator is used here to
       avoid plain text passwords in memory as much as possible
    */
    srandom((unsigned int)time(NULL));
    for (z = 0; z < MAX_PASSWORD_LENGTH; z++)
    {
      obfuscator[z] = (char)(random()%256);
    }
  #endif /* not HAVE_GCRYPT */

  return ERROR_NONE;
}

void Password_doneAll(void)
{
}

Password *Password_new(void)
{
  Password *password;

  #ifdef HAVE_GCRYPT
    password = (Password*)gcry_malloc_secure(sizeof(Password));
  #else /* not HAVE_GCRYPT */
    password = (Password*)malloc(sizeof(Password));
  #endif /* HAVE_GCRYPT */
  if (password == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  password->length = 0;

  return password;
}

void Password_delete(Password *password)
{
  assert(password != NULL);

  #ifdef HAVE_GCRYPT
    gcry_free(password);
  #else /* not HAVE_GCRYPT */
    memset(password,0,sizeof(Password));
    free(password);
  #endif /* HAVE_GCRYPT */
}

Password *Password_copy(Password *sourcePassword)
{
  Password *destinationPassword;

  assert(sourcePassword != NULL);

  destinationPassword = Password_new();
  assert(destinationPassword != NULL);
  memcpy(destinationPassword,sourcePassword,sizeof(Password));

  return destinationPassword;
}

void Password_set(Password *password, const String string)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  password->length = MIN(String_length(string),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,String_cString(string),password->length);
  #else /* not HAVE_GCRYPT */
    for (z = 0; z < MIN(String_length(string),MAX_PASSWORD_LENGTH); z++)
    {
      password->data[z] = String_index(string,z)^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[password->length] = '\0';
}

void Password_setCString(Password *password, const char *s)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  password->length = MIN(strlen(s),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,s,password->length);
  #else /* not HAVE_GCRYPT */
    for (z = 0; z < MIN(strlen(s),MAX_PASSWORD_LENGTH); z++)
    {
      password->data[z] = s[z]^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[password->length] = '\0';
}

void Password_appendChar(Password *password, char ch)
{
  assert(password != NULL);

  if (password->length < MAX_PASSWORD_LENGTH)
  {
    #ifdef HAVE_GCRYPT
      password->data[password->length] = ch;
    #else /* not HAVE_GCRYPT */
      password->data[password->length] = ch^obfuscator[password->length];
    #endif /* HAVE_GCRYPT */
    password->length++;
    password->data[password->length] = '\0';
  }
}

uint Password_length(const Password *password)
{
  assert(password != NULL);

  return password->length;
}

char Password_getChar(const Password *password, uint index)
{
  assert(password != NULL);

  if (index < password->length)
  {
    #ifdef HAVE_GCRYPT
      return password->data[index];
    #else /* not HAVE_GCRYPT */
      return password->data[index]^obfuscator[index];
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return '\0';
  }
}

const char *Password_deploy(Password *password)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);
  assert(password->length <= MAX_PASSWORD_LENGTH);

  #ifdef HAVE_GCRYPT
    return password->data;
  #else /* not HAVE_GCRYPT */
    for (z = 0; z < password->length; z++)
    {
      password->plain[z] = password->data[z]^obfuscator[z];
    }
    password->plain[password->length] = '\0';
    return password->plain;
  #endif /* HAVE_GCRYPT */
}

void Password_undeploy(Password *password)
{
  assert(password != NULL);

  #ifdef HAVE_GCRYPT
    UNUSED_VARIABLE(password);
  #else /* not HAVE_GCRYPT */
    memset(password->plain,0,MAX_PASSWORD_LENGTH);
  #endif /* HAVE_GCRYPT */
}

#ifdef __cplusplus
  }
#endif

/* end of file */
