/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/arrays.h,v $
* $Revision: 1.3 $
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
* Purpose: create new array
* Input  : elementSize - element size (in bytes)
*          length      - start length of array
* Output : -
* Return : array or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Array Array_new(ulong elementSize, ulong length);
#else /* not NDEBUG */
Array __Array_new(const char *fileName, ulong lineNb, ulong elementSize, ulong length);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Array_delete
* Purpose: delete array
* Input  : array                    - array to delete
*          arrayElementFreeFunction - array element free function or NULL
*          arrayElementFreeUserData - free function user data or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_delete(Array array, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData);

/***********************************************************************\
* Name   : Array_clear
* Purpose: clear array
* Input  : array                    - array to clear
*          arrayElementFreeFunction - array element free function or NULL
*          arrayElementFreeUserData - free function user data or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_clear(Array array, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData);

/***********************************************************************\
* Name   : Array_length
* Purpose: get array length
* Input  : array - array
* Output : -
* Return : number of elements in array
* Notes  : -
\***********************************************************************/

ulong Array_length(Array array);

/***********************************************************************\
* Name   : Array_put
* Purpose: put element into array
* Input  : array - array
*          index - index of element
*          data  - data
* Output : -
* Return : TRUE if element stored in array, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Array_put(Array array, ulong index, const void *data);

/***********************************************************************\
* Name   : Array_get
* Purpose: get element from array
* Input  : array - array
*          index - index of element
*          data  - variable for data (can be NULL)
* Output : data
* Return : -
* Notes  : if no data variable is supplied (NULL) a pointer to the
*          data element in the array is returned
\***********************************************************************/

void *Array_get(Array array, ulong index, void *data);

/***********************************************************************\
* Name   : Array_insert
* Purpose: insert element into array
* Input  : array     - array
*          nextIndex - index of next element or ARRAY_END
*          data      - element data
* Output : -
* Return : TRUE if element inserted, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_insert(Array array, long nextIndex, const void *data);

/***********************************************************************\
* Name   : Array_append
* Purpose: append element to array
* Input  : array - array
*          data  - element data
* Output : -
* Return : TRUE if element appended, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_append(Array array, const void *data);

/***********************************************************************\
* Name   : Array_remove
* Purpose: remove element from array
* Input  : array                    - array
*          index                    - index of element to remove
*          arrayElementFreeFunction - array element free function or NULL
*          arrayElementFreeUserData - free function user data or NULL
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
* Return : C-array with data pointers
* Notes  : -
\***********************************************************************/

const void *Array_cArray(Array array);

#ifndef NDEBUG
/***********************************************************************\
* Name   : Array_debugDone
* Purpose: done array debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_debugDone(void);

/***********************************************************************\
* Name   : Array_debugDumpInfo, Array_debugPrintInfo
* Purpose: array debug function: output allocated arrays
* Input  : handle - output channel
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_debugDumpInfo(FILE *handle);
void Array_debugPrintInfo(void);

/***********************************************************************\
* Name   : Array_debugPrintStatistics
* Purpose: array debug function: output arrays statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_debugPrintStatistics(void);

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __ARRAYS__ */


/* end of file */

