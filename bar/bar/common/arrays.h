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

// delete array data element function
/***********************************************************************\
* Name   : ArrayFreeFunction
* Purpose: free array data element
* Input  : data     - array data element
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ArrayFreeFunction)(void *data, void *userData);

/***********************************************************************\
* Name   : ArrayCompareFunction
* Purpose: compare array data elements function
* Input  : data1,data2 - array data elements to compare
*          userData - user data
* Output : -
* Return : -1/0/-1 iff </=/>
* Notes  : -
\***********************************************************************/

typedef int(*ArrayCompareFunction)(const void *data1, const void *data2, void *userData);

/***********************************************************************\
* Name   : ArrayIterateFunction
* Purpose: iterator array data elements function
* Input  : data     - array data element
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ArrayIterateFunction)(void *data, void *userData);

// array find modes
typedef enum
{
  ARRAY_FIND_FORWARD,
  ARRAY_FIND_BACKWARD
} ArrayFindModes;

// array handle
typedef struct
{
  ulong                elementSize;  // size of element
  ulong                length;       // current length of array
  ulong                maxLength;    // current maximal length of array
  ArrayFreeFunction    arrayFreeFunction;
  void                 *arrayFreeUserData;
  ArrayCompareFunction arrayCompareFunction;
  void                 *arrayCompareUserData;
  byte                 *data;        // array data
} Array;

#ifndef NDEBUG
/***********************************************************************\
* Name   : ArrayDumpInfoFunction
* Purpose: string dump info call-back function
* Input  : array    - array
*          fileName - file name
*          lineNb   - line number
*          n        - array number [0..count-1]
*          count    - total array count
*          userData - user data
* Output : -
* Return : TRUE for continue, FALSE for abort
* Notes  : -
\***********************************************************************/

typedef bool(*ArrayDumpInfoFunction)(const Array *array,
                                     const char  *fileName,
                                     ulong       lineNb,
                                     ulong       n,
                                     ulong       count,
                                     void        *userData
                                    );
#endif /* not NDEBUG */

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Array_init(...) __Array_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Array_new(...)  __Array_new(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : ARRAY_ITERATE
* Purpose: iterated over array and execute block
* Input  : array     - array
*          variable - iteration variable
*          data      - pointer to data element
* Output : -
* Return : -
* Notes  : variable will contain all indizes in array
*          usage:
*            ulong i;
*
*            ARRAY_ITERATE(array,i,data)
*            {
*              ... = data
*            }
\***********************************************************************/

#define ARRAY_ITERATE(array,variable,data) \
  for ((variable) = 0, Array_get(array,0,&(data)); \
       (variable) < Array_length(array); \
       (variable)++, Array_get(array,variable,&(data)) \
      )

/***********************************************************************\
* Name   : ARRAY_ITERATEX
* Purpose: iterated over array and execute block
* Input  : array     - array
*          variable  - iteration variable
*          data      - pointer to data element
*          condition - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all indizes in array
*          usage:
*            ulong i;
*
*            ARRAY_ITERATEX(array,i,data,TRUE)
*            {
*              ... = data
*            }
\***********************************************************************/

#define ARRAY_ITERATEX(array,variable,data,condition) \
  for ((variable) = 0, Array_get(array,0,&(data)); \
       ((variable) < Array_length(array)) && (condition); \
       (variable)++, Array_get(array,variable,&(data)) \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Array_init
* Purpose: init array
* Input  : array                - array variable
*          elementSize          - element size (in bytes)
*          length               - start length of array
*          arrayFreeFunction    - free element function or NULL
*          arrayFreeUserData    - free function user data
*          arrayCompareFunction - compare element function or
*                                        NULL
*          arrayCompareUserData - compare function user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Array_init(Array                *array,
                ulong                elementSize,
                ulong                length,
                ArrayFreeFunction    arrayFreeFunction,
                void                 *arrayFreeUserData,
                ArrayCompareFunction arrayCompareFunction,
                void                 *arrayCompareUserData
               );
#else /* not NDEBUG */
void __Array_init(const char           *__fileName__,
                  ulong                __lineNb__,
                  Array                *array,
                  ulong                elementSize,
                  ulong                length,
                  ArrayFreeFunction    arrayFreeFunction,
                  void                 *arrayFreeUserData,
                  ArrayCompareFunction arrayCompareFunction,
                  void                 *arrayCompareUserData
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
* Input  : elementSize          - data element size (in bytes)
*          length               - start length of array
*          arrayFreeFunction    - free data element function or NULL
*          arrayFreeUserData    - free function user data
*          arrayCompareFunction - compare data element function or NULL
*          arrayCompareUserData - compare function user data
* Output : -
* Return : array or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Array *Array_new(ulong                elementSize,
                 ulong                length,
                 ArrayFreeFunction    arrayFreeFunction,
                 void                 *arrayFreeUserData,
                 ArrayCompareFunction arrayCompareFunction,
                 void                 *arrayCompareUserData
                );
#else /* not NDEBUG */
Array *__Array_new(const char           *__fileName__,
                   ulong                __lineNb__,
                   ulong                elementSize,
                   ulong                length,
                   ArrayFreeFunction    arrayFreeFunction,
                   void                 *arrayFreeUserData,
                   ArrayCompareFunction arrayCompareFunction,
                   void                 *arrayCompareUserData
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
* Return : number of data elements in array
* Notes  : -
\***********************************************************************/

INLINE ulong Array_length(const Array *array);
#if defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)
INLINE ulong Array_length(const Array *array)
{
  return (array != NULL) ? array->length : 0;
}
#endif // defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)

/***********************************************************************\
* Name   : Array_isEmpty
* Purpose: check if array is empty
* Input  : array - array
* Output : -
* Return : TRUE iff array is empty
* Notes  : -
\***********************************************************************/

INLINE bool Array_isEmpty(const Array *array);
#if defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)
INLINE bool Array_isEmpty(const Array *array)
{
  return (array != NULL) ? (array->length == 0) : TRUE;
}
#endif // defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)

/***********************************************************************\
* Name   : Array_put
* Purpose: put data element into array
* Input  : array - array
*          index - index of data element
*          data  - data
* Output : -
* Return : TRUE if data element stored in array, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Array_put(Array *array, ulong index, const void *data);

/***********************************************************************\
* Name   : Array_get
* Purpose: get data element from array
* Input  : array - array
*          index - index of data element
*          data  - variable for data (can be NULL)
* Output : data
* Return : -
* Notes  : if no data variable is supplied (NULL) a pointer to the
*          data data element in the array is returned
\***********************************************************************/

void *Array_get(const Array *array, ulong index, void *data);

/***********************************************************************\
* Name   : Array_insert
* Purpose: insert data element into array
* Input  : array     - array
*          nextIndex - index of next data element or
*                      ARRAY_BEGIN/ARRAY_END
*          data      - data element data
* Output : -
* Return : TRUE if data element inserted, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_insert(Array *array, long nextIndex, const void *data);

/***********************************************************************\
* Name   : Array_append
* Purpose: append data element to array
* Input  : array - array
*          data  - data element
* Output : -
* Return : TRUE if data element appended, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Array_append(Array *array, const void *data);

/***********************************************************************\
* Name   : Array_remove
* Purpose: remove data element from array
* Input  : array - array
*          index - index of data element to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_remove(Array *array, ulong index);

/***********************************************************************\
* Name   : Array_removeAll
* Purpose: remove all specific data elements from array
* Input  : array                - array
*          data                 - data element
*          arrayCompareFunction - compare function or NULL for memcmp
*          arrayCompareUserData - user data for compare function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_removeAll(Array                *array,
                     const void           *data,
                     ArrayCompareFunction arrayCompareFunction,
                     void                 *arrayCompareUserData
                    );

/***********************************************************************\
* Name   : Array_contains
* Purpose: check if array contain data element
* Input  : array                - array
*          data                 - data element
*          arrayCompareFunction - compare function or NULL for memcmp
*          arrayCompareUserData - user data for compare function
* Output : -
* Return : TRUE if array contain data element, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Array_contains(const Array          *array,
                    const void           *data,
                    ArrayCompareFunction arrayCompareFunction,
                    void                 *arrayCompareUserData
                   );

/***********************************************************************\
* Name   : Array_findFirst
* Purpose: find data element in array
* Input  : array                - array
*          arrayFindMode        - array find mode
*          arrayCompareFunction - compare function or NULL for memcmp
*          arrayCompareUserData - user data for compare function
* Output : -
* Return : index or -1 if not found
* Notes  : -
\***********************************************************************/

long Array_findFirst(const Array          *array,
                     ArrayFindModes       arrayFindMode,
                     void                 *data,
                     ArrayCompareFunction arrayCompareFunction,
                     void                 *arrayCompareUserData
                    );

/***********************************************************************\
* Name   : Array_findNext
* Purpose: find next data element in array
* Input  : array                - array
*          arrayFindMode        - array find mode
*          data                 - data element
*          index                - previous index
*          arrayCompareFunction - compare function or NULL for memcmp
*          arrayCompareUserData - user data for compare function
* Output : -
* Return : index or -1 if not found
* Notes  : -
\***********************************************************************/

long Array_findNext(const Array          *array,
                    ArrayFindModes       arrayFindMode,
                    void                 *data,
                    ulong                index,
                    ArrayCompareFunction arrayCompareFunction,
                    void                 *arrayCompareUserData
                   );

/***********************************************************************\
* Name   : Array_sort
* Purpose: sort array
* Input  : array                - array
*          arrayCompareFunction - compare function or NULL for memcmp
*          arrayCompareUserData - user data for compare function
* Output : -
* Return : -
* Notes  : use temporary O(n) memory
\***********************************************************************/

void Array_sort(Array                *array,
                ArrayCompareFunction arrayCompareFunction,
                void                 *arrayCompareUserData
               );

/***********************************************************************\
* Name   : Array_toCArray
* Purpose: get C-array data pointer
* Input  : array - array
* Output : -
* Return : C-array with data pointers
* Notes  : -
\***********************************************************************/

INLINE const void *Array_cArray(const Array *array);
#if defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)
INLINE const void *Array_cArray(const Array *array)
{
  return (array != NULL) ? array->data : NULL;
}
#endif // defined(NDEBUG) || defined(__ARRAYS_IMPLEMENTATION__)

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
* Input  : handle                - output channel
*          arrayDumpInfoFunction - array dump info call-back or NULL
*          arrayDumpInfoUserData - array dump info user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Array_debugDumpInfo(FILE *handle,
                         ArrayDumpInfoFunction arrayDumpInfoFunction,
                         void                  *arrayDumpInfoUserData
                        );
void Array_debugPrintInfo(ArrayDumpInfoFunction arrayDumpInfoFunction,
                          void                  *arrayDumpInfoUserData
                         );

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
