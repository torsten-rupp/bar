/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/arrays.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: dynamic array functions
* Systems: all
*
\***********************************************************************/

#ifndef __ARRAYS__
#define __ARRAYS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define ARRAY_BEGIN 0
#define ARRAY_END   -1

/***************************** Datatypes *******************************/

typedef struct __Array* Array;

/* delete array element function */
typedef void(*ArrayElementFreeFunction)(void *data, void *userData);

/* comparison, iteration functions */
typedef int(*ArrayElementCompareFunction)(void *userData, void *data1, void *data2);
typedef char(*ArrayElementIterateFunction)(void *userData, void *data);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Array_new(elementSize,length) __Array_new(__FILE__,__LINE__,elementSize,length)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Array_new
* Purpose: create new string
* Input  : s            - C-string
*          ch           - character
*          buffer       - buffer
*          bufferLength - length of buffer
* Output : -
* Return : string or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void *Array_new(ulong elementSize, ulong length);
#else /* not NDEBUG */
void *__Array_new(const char *fileName, ulong lineNb, ulong elementSize, ulong length);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Array_delete
* Purpose: delete array
* Input  : array - string to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_delete(Array array, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData);

/***********************************************************************\
* Name   : Array_clear
* Purpose: clear string
* Input  : string - string to clear
* Output : -
* Return : cleared string (empty)
* Notes  : -
\***********************************************************************/

void Array_clear(Array array);

ulong Array_length(Array array);

/***********************************************************************\
* Name   : String_set, String_setCString, String_setChar,
*          Stirng_setBuffer
* Purpose: set string (copy string)
* Input  : string       - string to set
*          sourceString - source string
*          s            - C-string
*          ch           - character
*          buffer       - buffer
*          bufferLength - length of buffer
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

bool Array_put(Array array, ulong index, const void *data);
void Array_get(Array array, ulong index, void *data);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Array_insert(Array array, long nextIndex, const void *data);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Array_append(Array array, const void *data);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_remove(Array array, ulong index, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData);

/***********************************************************************\
* Name   : Array_toCArray
* Purpose: get C-array data pointer
* Input  : array - array
* Output : -
* Return : C-array data pointer
* Notes  : -
\***********************************************************************/

void *Array_cArray(Array array);

#ifndef NDEBUG
void Array_debug(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __ARRAYS__ */

/* end of file */
