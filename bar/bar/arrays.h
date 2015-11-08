/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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

#define ARRAY_BEGIN 0L
#define ARRAY_END   -1L

/***************************** Datatypes *******************************/

// delete array element function
typedef void(*ArrayElementFreeFunction)(void *data, void *userData);

// element comparison, iteration functions
typedef int(*ArrayElementCompareFunction)(void *userData, void *data1, void *data2);
typedef char(*ArrayElementIterateFunction)(void *userData, void *data);

// array handle
typedef struct
{
  ulong                       elementSize;  // size of element
  ulong                       length;       // current length of array
  ulong                       maxLength;    // current maximal length of array
  ArrayElementFreeFunction    arrayElementFreeFunction;
  void                        *arrayElementFreeUserData;
  ArrayElementCompareFunction arrayElementCompareFunction;
  void                        *arrayElementCompareUserData;
  byte                        *data;        // array data
} Array;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Array_init(...) __Array_init(__FILE__,__LINE__,__VA_ARGS__)
  #define Array_new(...)  __Array_new(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : ARRAY_ITERATE
* Purpose: iterated over array and execute block
* Input  : array     - array
*          variable - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all elements in array
*          usage:
*            ARRAY_ITERATE(array,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define ARRAY_ITERATE(array,variable) \
  for ((variable) = 0, Array_get(array,0,variable); \
       (variable) < Array_length(array); \
       (variable)++, Array_get(array,0,variable) \
      )

/***********************************************************************\
* Name   : ARRAY_ITERATEX
* Purpose: iterated over array and execute block
* Input  : array     - array
*          variable  - iteration variable
*          condition - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all elements in array
*          usage:
*            ARRAY_ITERATEX(array,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define ARRAY_ITERATEX(array,variable,condition) \
  for ((variable) = 0, Array_get(array,0,variable); \
       ((variable) < Array_length(array)) && (condition); \
       (variable)++, Array_get(array,0,variable) \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Array_init
* Purpose: init array
* Input  : array                       - array variable
*          elementSize                 - element size (in bytes)
*          length                      - start length of array
*          arrayElementFreeFunction    - free element function or NULL
*          arrayElementFreeUserData    - free function user data
*          arrayElementCompareFunction - compare element function or
*                                        NULL
*          arrayElementCompareUserData - compare function user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Array_init(Array                       *array,
                ulong                       elementSize,
                ulong                       length,
                ArrayElementFreeFunction    arrayElementFreeFunction,
                void                        *arrayElementFreeUserData,
                ArrayElementCompareFunction arrayElementCompareFunction,
                void                        *arrayElementCompareUserData
               );
#else /* not NDEBUG */
void __Array_init(const char                  *__fileName__,
                  ulong                       __lineNb__,
                  Array                       *array,
                  ulong                       elementSize,
                  ulong                       length,
                  ArrayElementFreeFunction    arrayElementFreeFunction,
                  void                        *arrayElementFreeUserData,
                  ArrayElementCompareFunction arrayElementCompareFunction,
                  void                        *arrayElementCompareUserData
                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Array_done
* Purpose: done array
* Input  : array - array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_done(Array *array);

/***********************************************************************\
* Name   : Array_new
* Purpose: create new array
* Input  : elementSize                 - element size (in bytes)
*          length                      - start length of array
*          arrayElementFreeFunction    - free element function or NULL
*          arrayElementFreeUserData    - free function user data
*          arrayElementCompareFunction - compare element function or
*                                        NULL
*          arrayElementCompareUserData - compare function user data
* Output : -
* Return : array or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Array *Array_new(ulong                       elementSize,
                 ulong                       length,
                 ArrayElementFreeFunction    arrayElementFreeFunction,
                 void                        *arrayElementFreeUserData,
                 ArrayElementCompareFunction arrayElementCompareFunction,
                 void                        *arrayElementCompareUserData
                );
#else /* not NDEBUG */
Array *__Array_new(const char                  *__fileName__,
                   ulong                       __lineNb__,
                   ulong                       elementSize,
                   ulong                       length,
                   ArrayElementFreeFunction    arrayElementFreeFunction,
                   void                        *arrayElementFreeUserData,
                   ArrayElementCompareFunction arrayElementCompareFunction,
                   void                        *arrayElementCompareUserData
                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Array_delete
* Purpose: delete array
* Input  : array - array to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_delete(Array *array);

/***********************************************************************\
* Name   : Array_clear
* Purpose: clear array
* Input  : array - array to clear
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_clear(Array *array);

/***********************************************************************\
* Name   : Array_length
* Purpose: get array length
* Input  : array - array
* Output : -
* Return : number of elements in array
* Notes  : -
\***********************************************************************/

INLINE ulong Array_length(const Array *array);
#if defined(NDEBUG) || defined(__ARRAYS_IMPLEMENATION__)
INLINE ulong Array_length(const Array *array)
{
  return (array != NULL) ? array->length : 0;
}
#endif // defined(NDEBUG) || defined(__ARRAYS_IMPLEMENATION__)

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

bool Array_put(Array *array, ulong index, const void *data);

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

void *Array_get(const Array *array, ulong index, void *data);

/***********************************************************************\
* Name   : Array_insert
* Purpose: insert element into array
* Input  : array     - array
*          nextIndex - index of next element or ARRAY_BEGIN/ARRAY_END
*          data      - element data
* Output : -
* Return : TRUE if element inserted, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_insert(Array *array, long nextIndex, const void *data);

/***********************************************************************\
* Name   : Array_append
* Purpose: append element to array
* Input  : array - array
*          data  - element data
* Output : -
* Return : TRUE if element appended, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_append(Array *array, const void *data);

/***********************************************************************\
* Name   : Array_remove
* Purpose: remove element from array
* Input  : array - array
*          index - index of element to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_remove(Array *array, ulong index);

/***********************************************************************\
* Name   : Array_toCArray
* Purpose: get C-array data pointer
* Input  : array - array
* Output : -
* Return : C-array with data pointers
* Notes  : -
\***********************************************************************/

INLINE const void *Array_cArray(const Array *array);
#if defined(NDEBUG) || defined(__ARRAYS_IMPLEMENATION__)
INLINE const void *Array_cArray(const Array *array)
{
  return (array != NULL) ? array->data : NULL;
}
#endif // defined(NDEBUG) || defined(__ARRAYS_IMPLEMENATION__)

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

/***********************************************************************\
* Name   : Array_debugCheck
* Purpose: array debug function: output allocated arrays and statistics,
*          check for lost resources
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_debugCheck(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __ARRAYS__ */


/* end of file */
