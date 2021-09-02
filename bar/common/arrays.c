/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic array functions
* Systems: all
*
\***********************************************************************/
#define __ARRAYS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#ifndef NDEBUG
  #include <pthread.h>
  #include "common/lists.h"
#endif /* not NDEBUG */

#include "arrays.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY

/***************************** Constants *******************************/

#define DEFAULT_LENGTH 8L
#define DELTA_LENGTH   8L

/***************************** Datatypes *******************************/

#ifndef NDEBUG
  typedef struct DebugArrayNode
  {
    LIST_NODE_HEADER(struct DebugArrayNode);

    const char  *fileName;
    ulong       lineNb;
    const Array *array;
  } DebugArrayNode;

  typedef struct
  {
    LIST_HEADER(DebugArrayNode);
    ulong allocatedMemory;
  } DebugArrayList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  LOCAL pthread_once_t  debugArrayInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutex_t debugArrayLock;
  LOCAL DebugArrayList  debugArrayList;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/
#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : debugArrayInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
LOCAL void debugArrayInit(void)
{
  pthread_mutex_init(&debugArrayLock,NULL);
  List_init(&debugArrayList);
  debugArrayList.allocatedMemory = 0L;
}
#endif /* not NDEBUG */

// ----------------------------------------------------------------------

#ifdef NDEBUG
void Array_init(Array                *array,
                ulong                elementSize,
                ulong                length,
                ArrayFreeFunction    arrayFreeFunction,
                void                 *arrayFreeUserData,
                ArrayCompareFunction arrayCompareFunction,
                void                 *arrayCompareUserData
               )
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
                 )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DebugArrayNode *debugArrayNode;
  #endif /* not NDEBUG */

  assert(array != NULL);
  assert(elementSize > 0);

  if (length == 0) length = DEFAULT_LENGTH;
  array->data = (byte*)malloc(length*elementSize);
  if (array->data == NULL)
  {
    free(array);
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  array->elementSize          = elementSize;
  array->length               = 0;
  array->maxLength            = length;
  array->arrayFreeFunction    = arrayFreeFunction;
  array->arrayFreeUserData    = arrayFreeUserData;
  array->arrayCompareFunction = arrayCompareFunction;
  array->arrayCompareUserData = arrayCompareUserData;

  #ifndef NDEBUG
    pthread_once(&debugArrayInitFlag,debugArrayInit);

    pthread_mutex_lock(&debugArrayLock);
    {
      debugArrayNode = LIST_NEW_NODE(DebugArrayNode);
      if (debugArrayNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      debugArrayNode->fileName = __fileName__;
      debugArrayNode->lineNb   = __lineNb__;
      debugArrayNode->array    = array;
      List_append(&debugArrayList,debugArrayNode);
      debugArrayList.allocatedMemory += sizeof(DebugArrayNode)+array->maxLength*array->elementSize;
    }
    pthread_mutex_unlock(&debugArrayLock);
  #endif /* not NDEBUG */
}

void Array_done(Array *array)
{
  ulong z;
  #ifndef NDEBUG
    DebugArrayNode *debugArrayNode;
  #endif /* not NDEBUG */

  assert(array != NULL);
  assert(array->data != NULL);

  if (array->arrayFreeFunction != NULL)
  {
    for (z = 0; z < array->length; z++)
    {
      array->arrayFreeFunction(array->data+z*array->elementSize,array->arrayFreeUserData);
    }
  }

  #ifndef NDEBUG
    pthread_once(&debugArrayInitFlag,debugArrayInit);

    pthread_mutex_lock(&debugArrayLock);
    {
      debugArrayNode = debugArrayList.head;
      while ((debugArrayNode != NULL) && (debugArrayNode->array != array))
      {
        debugArrayNode = debugArrayNode->next;
      }
      if (debugArrayNode != NULL)
      {
        List_remove(&debugArrayList,debugArrayNode);
        assert(debugArrayList.allocatedMemory >= sizeof(DebugArrayNode)+array->maxLength*array->elementSize);
        debugArrayList.allocatedMemory -= sizeof(DebugArrayNode)+array->maxLength*array->elementSize;
        LIST_DELETE_NODE(debugArrayNode);
      }
      else
      {
        fprintf(stderr,"DEBUG WARNING: array %p not found in debug list!\n",
                array
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("array not found");
      }
    }
    pthread_mutex_unlock(&debugArrayLock);
  #endif /* not NDEBUG */

  free(array->data);
}

#ifdef NDEBUG
Array *Array_new(ulong                elementSize,
                 ulong                length,
                 ArrayFreeFunction    arrayFreeFunction,
                 void                 *arrayFreeUserData,
                 ArrayCompareFunction arrayCompareFunction,
                 void                 *arrayCompareUserData
                )
#else /* not NDEBUG */
Array *__Array_new(const char           *__fileName__,
                   ulong                __lineNb__,
                   ulong                elementSize,
                   ulong                length,
                   ArrayFreeFunction    arrayFreeFunction,
                   void                 *arrayFreeUserData,
                   ArrayCompareFunction arrayCompareFunction,
                   void                 *arrayCompareUserData
                  )
#endif /* NDEBUG */
{
  Array *array;

  assert(elementSize > 0);

  array = (Array*)malloc(sizeof(Array));
  if (array == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  #ifdef NDEBUG
    Array_init(array,elementSize,length,arrayFreeFunction,arrayFreeUserData,arrayCompareFunction,arrayCompareUserData);
  #else /* not NDEBUG */
    __Array_init(__fileName__,__lineNb__,array,elementSize,length,arrayFreeFunction,arrayFreeUserData,arrayCompareFunction,arrayCompareUserData);
  #endif /* NDEBUG */

  return array;
}

void Array_delete(Array *array)
{
  if (array != NULL)
  {
    Array_done(array);
    free(array);
  }
}

void Array_clear(Array *array)
{
  ulong z;

  if (array != NULL)
  {
    assert(array->data != NULL);

    if (array->arrayFreeFunction != NULL)
    {
      for (z = 0; z < array->length; z++)
      {
        array->arrayFreeFunction(array->data+z*array->elementSize,array->arrayFreeUserData);
      }
    }
    array->length = 0;
  }
}

bool Array_put(Array *array, ulong index, const void *data)
{
  void  *newData;
  ulong newMaxLength;

  if (array != NULL)
  {
    assert(array->data != NULL);

    // extend array if needed
    if (index >= array->maxLength)
    {
      newMaxLength = index+1;

      newData = realloc(array->data,newMaxLength*array->elementSize);
      if (newData == NULL)
      {
        return FALSE;
      }

      #ifndef NDEBUG
        pthread_once(&debugArrayInitFlag,debugArrayInit);

        pthread_mutex_lock(&debugArrayLock);
        {
          debugArrayList.allocatedMemory += (newMaxLength-array->maxLength)*array->elementSize;
        }
        pthread_mutex_unlock(&debugArrayLock);
      #endif /* not NDEBUG */

      array->maxLength = newMaxLength;
      array->data      = newData;
    }

    // store element
    memcpy(array->data+index*array->elementSize,data,array->elementSize);
    if (index > array->length) array->length = index+1;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

void *Array_get(const Array *array, ulong index, void *data)
{
  void *element;

  element = NULL;
  if (array != NULL)
  {
    assert(array->data != NULL);

    // get element
    if (index < array->length)
    {
      if (data != NULL)
      {
        memcpy(data,array->data+index*array->elementSize,array->elementSize);
        element = data;
      }
      else
      {
        element = array->data+index*array->elementSize;
      }
    }
  }

  return element;
}

bool Array_insert(Array *array, long nextIndex, const void *data)
{
  ulong newMaxLength;
  byte  *newData;

  if (array != NULL)
  {
    assert(array->data != NULL);

    if      (nextIndex != ARRAY_END)
    {
      // extend array if needed
      if ((ulong)nextIndex+1L >= array->maxLength)
      {
        newMaxLength = nextIndex+1L;

        newData = realloc(array->data,newMaxLength*array->elementSize);
        if (newData == NULL)
        {
          return FALSE;
        }

        #ifndef NDEBUG
          pthread_once(&debugArrayInitFlag,debugArrayInit);

          pthread_mutex_lock(&debugArrayLock);
          {
            debugArrayList.allocatedMemory += (newMaxLength-array->maxLength)*array->elementSize;
          }
          pthread_mutex_unlock(&debugArrayLock);
        #endif /* not NDEBUG */

        array->maxLength = newMaxLength;
        array->data      = newData;
      }

      // insert element
      if (nextIndex < (long)array->length)
      {
        memmove(array->data+(nextIndex+1)*array->elementSize,
                array->data+nextIndex*array->elementSize,
                array->elementSize*(array->length-nextIndex)
               );
      }
      memcpy(array->data+nextIndex*array->elementSize,data,array->elementSize);
      if (nextIndex > (long)array->length) array->length = (ulong)nextIndex+1;

      return TRUE;
    }
    else
    {
      // extend array if needed
      if (array->length >= array->maxLength)
      {
        newMaxLength = array->maxLength+DELTA_LENGTH;

        newData = realloc(array->data,newMaxLength*array->elementSize);
        if (newData == NULL)
        {
          return FALSE;
        }

        #ifndef NDEBUG
          pthread_once(&debugArrayInitFlag,debugArrayInit);

          pthread_mutex_lock(&debugArrayLock);
          {
            debugArrayList.allocatedMemory += (newMaxLength-array->maxLength)*array->elementSize;
          }
          pthread_mutex_unlock(&debugArrayLock);
        #endif /* not NDEBUG */

        array->maxLength = newMaxLength;
        array->data      = newData;
      }

      // store element
      memcpy(array->data+array->length*array->elementSize,data,array->elementSize);
      array->length++;

      return TRUE;
    }
  }
  else
  {
    return FALSE;
  }
}

bool Array_append(Array *array, const void *data)
{
  ulong newMaxLength;
  byte  *newData;

  if (array != NULL)
  {
    assert(array->data != NULL);

    // extend array if needed
    if (array->length >= array->maxLength)
    {
      newMaxLength = array->maxLength+DELTA_LENGTH;

      newData = realloc(array->data,newMaxLength*array->elementSize);
      if (newData == NULL)
      {
        return FALSE;
      }

      #ifndef NDEBUG
        pthread_once(&debugArrayInitFlag,debugArrayInit);

        pthread_mutex_lock(&debugArrayLock);
        {
          debugArrayList.allocatedMemory += (newMaxLength-array->maxLength)*array->elementSize;
        }
        pthread_mutex_unlock(&debugArrayLock);
      #endif /* not NDEBUG */

      array->maxLength = newMaxLength;
      array->data      = newData;
    }

    // store element
    memcpy(array->data+array->length*array->elementSize,data,array->elementSize);
    array->length++;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

void Array_remove(Array *array, ulong index)
{
  if (array != NULL)
  {
    assert(array->data != NULL);

    if (index < array->length)
    {
      // free element
      if (array->arrayFreeFunction != NULL)
      {
        array->arrayFreeFunction(array->data+index*array->elementSize,array->arrayFreeUserData);
      }

      // remove element
      if (index < array->length-1L)
      {
        memmove(array->data+index*array->elementSize,
                array->data+(index+1)*array->elementSize,
                array->elementSize*(array->length-index)
               );
      }
      array->length--;
    }
  }
}

void Array_removeAll(Array                *array,
                     const void           *data,
                     ArrayCompareFunction arrayCompareFunction,
                     void                 *arrayCompareUserData
                    )
{
  ulong index;

  index = 0;
  if (arrayCompareFunction != NULL)
  {
    while (index < array->length)
    {
      if (arrayCompareFunction(array->data+index*array->elementSize,data,arrayCompareUserData) == 0)
      {
        Array_remove(array,index);
      }
      else
      {
        index++;
      }
    }
  }
  else
  {
    while (index < array->length)
    {
      if (memcmp(array->data+index*array->elementSize,data,array->elementSize) == 0)
      {
        Array_remove(array,index);
      }
      else
      {
        index++;
      }
    }
  }
}

bool Array_contains(const Array          *array,
                    const void           *data,
                    ArrayCompareFunction arrayCompareFunction,
                    void                 *arrayCompareUserData
                   )
{
  ulong index;

  if (arrayCompareFunction != NULL)
  {
    for (index = 0; index < array->length; index++)
    {
      if (arrayCompareFunction(array->data+index*array->elementSize,data,arrayCompareUserData) == 0)
      {
        return TRUE;
      }
    }
  }
  else
  {
    for (index = 0; index < array->length; index++)
    {
      if (memcmp(array->data+index*array->elementSize,data,array->elementSize) == 0)
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

long Array_find(const Array          *array,
                ArrayFindModes       arrayFindMode,
                void                 *data,
                ArrayCompareFunction arrayCompareFunction,
                void                 *arrayCompareUserData
               )
{
  long i;

  switch (arrayFindMode)
  {
    case ARRAY_FIND_FORWARD:
      i = 0;
      if (arrayCompareFunction != NULL)
      {
        while (i < (long)array->length)
        {
          if (arrayCompareFunction(array->data+i*array->elementSize,data,arrayCompareUserData) == 0)
          {
            return i;
          }
          i++;
        }
      }
      else
      {
        while (i < (long)array->length)
        {
          if (memcmp(array->data+i*array->elementSize,data,array->elementSize) == 0)
          {
            return i;
          }
          i++;
        }
      }
      break;
    case ARRAY_FIND_BACKWARD:
      i = array->length-1;
      if (arrayCompareFunction != NULL)
      {
        while (i >= 0)
        {
          if (arrayCompareFunction(array->data+i*array->elementSize,data,arrayCompareUserData) == 0)
          {
            return i;
          }
          i--;
        }
      }
      else
      {
        while (i >= 0)
        {
          if (memcmp(array->data+i*array->elementSize,data,array->elementSize) == 0)
          {
            return i;
          }
          i--;
        }
      }
      break;
  }

  return -1;
}

long Array_findNext(const Array          *array,
                    ArrayFindModes       arrayFindMode,
                    void                 *data,
                    ulong                index,
                    ArrayCompareFunction arrayCompareFunction,
                    void                 *arrayCompareUserData
                   )
{
  long i;

  switch (arrayFindMode)
  {
    case ARRAY_FIND_FORWARD:
      if (index < array->length)
      {
        i = (long)index+1;
        if (arrayCompareFunction != NULL)
        {
          while (i < (long)array->length)
          {
            if (arrayCompareFunction(array->data+i*array->elementSize,data,arrayCompareUserData) == 0)
            {
              return i;
            }
            i++;
          }
        }
        else
        {
          while (i < (long)array->length)
          {
            if (memcmp(array->data+i*array->elementSize,data,array->elementSize) == 0)
            {
              return i;
            }
            i++;
          }
        }
      }
      break;
    case ARRAY_FIND_BACKWARD:
      if (index > 0)
      {
        i = (long)index-1;
        if (arrayCompareFunction != NULL)
        {
          while (i >= 0)
          {
            if (arrayCompareFunction(array->data+i*array->elementSize,data,arrayCompareUserData) == 0)
            {
              return i;
            }
            i--;
          }
        }
        else
        {
          while (i >= 0)
          {
            if (memcmp(array->data+i*array->elementSize,data,array->elementSize) == 0)
            {
              return i;
            }
            i--;
          }
        }
      }
      break;
  }

  return -1;
}

void Array_sort(Array                *array,
                ArrayCompareFunction arrayCompareFunction,
                void                 *arrayCompareUserData
               )
{
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
UNUSED_VARIABLE(array);
UNUSED_VARIABLE(arrayCompareFunction);
UNUSED_VARIABLE(arrayCompareUserData);
}

#ifndef NDEBUG
void Array_debugDone(void)
{
  pthread_once(&debugArrayInitFlag,debugArrayInit);

  Array_debugCheck();

  pthread_mutex_lock(&debugArrayLock);
  {
    List_done(&debugArrayList,NULL,NULL);
  }
  pthread_mutex_unlock(&debugArrayLock);
}

void Array_debugDumpInfo(FILE                  *handle,
                         ArrayDumpInfoFunction arrayDumpInfoFunction,
                         void                  *arrayDumpInfoUserData
                        )
{
  ulong                n;
  const DebugArrayNode *debugArrayNode;

  pthread_once(&debugArrayInitFlag,debugArrayInit);

  pthread_mutex_lock(&debugArrayLock);
  {
    n = 0L;
    LIST_ITERATE(&debugArrayList,debugArrayNode)
    {
      fprintf(handle,"DEBUG: array %p[%ld] allocated at %s, line %ld\n",
              debugArrayNode->array->data,
              debugArrayNode->array->maxLength,
              debugArrayNode->fileName,
              debugArrayNode->lineNb
             );

      if (arrayDumpInfoFunction != NULL)
      {
        if (!arrayDumpInfoFunction(debugArrayNode->array,
                                   debugArrayNode->fileName,
                                   debugArrayNode->lineNb,
                                   n,
                                   List_count(&debugArrayList),
                                   arrayDumpInfoUserData
                                  )
           )
        {
          break;
        }
      }

      n++;
    }
  }
  pthread_mutex_unlock(&debugArrayLock);
}

void Array_debugPrintInfo(ArrayDumpInfoFunction srrayDumpInfoFunction,
                          void                  *arrayDumpInfoUserData
                         )
{
  Array_debugDumpInfo(stderr,srrayDumpInfoFunction,arrayDumpInfoUserData);
}

void Array_debugPrintStatistics(void)
{
  pthread_once(&debugArrayInitFlag,debugArrayInit);

  pthread_mutex_lock(&debugArrayLock);
  {
    fprintf(stderr,"DEBUG: %lu array(s) allocated, total %lu bytes\n",
            List_count(&debugArrayList),
            debugArrayList.allocatedMemory
           );
  }
  pthread_mutex_unlock(&debugArrayLock);
}

void Array_debugCheck(void)
{
  const DebugArrayNode *debugArrayNode;

  pthread_once(&debugArrayInitFlag,debugArrayInit);

  Array_debugPrintInfo(CALLBACK_(NULL,NULL));
  Array_debugPrintStatistics();

  pthread_mutex_lock(&debugArrayLock);
  {
    if (!List_isEmpty(&debugArrayList))
    {
      LIST_ITERATE(&debugArrayList,debugArrayNode)
      {
        fprintf(stderr,"DEBUG: array %p[%ld] allocated at %s, line %ld\n",
                debugArrayNode->array->data,
                debugArrayNode->array->maxLength,
                debugArrayNode->fileName,
                debugArrayNode->lineNb
               );
      }
      HALT_INTERNAL_ERROR_LOST_RESOURCE();
    }
  }
  pthread_mutex_unlock(&debugArrayLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
