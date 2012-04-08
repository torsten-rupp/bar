/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic array functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "arrays.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY

/***************************** Constants *******************************/

#define DEFAULT_LENGTH 8
#define DELTA_LENGTH   8

/***************************** Datatypes *******************************/

struct __Array
{
  ulong elementSize;                 // size of element
  ulong length;                      // current length of array
  ulong maxLength;                   // current maximal length of array
  byte  *data;                       // array data
};

#ifndef NDEBUG
  typedef struct DebugArrayNode
  {
    LIST_NODE_HEADER(struct DebugArrayNode);

    const char           *fileName;
    ulong                lineNb;
    const struct __Array *array;
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
Array Array_new(ulong elementSize, ulong length)
#else /* not NDEBUG */
Array __Array_new(const char *fileName, ulong lineNb, ulong elementSize, ulong length)
#endif /* NDEBUG */
{
  struct __Array *array;
  #ifndef NDEBUG
    DebugArrayNode *debugArrayNode;
  #endif /* not NDEBUG */

  assert(elementSize > 0);

  array = (struct __Array*)malloc(sizeof(struct __Array));
  if (array == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }
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

  array->elementSize = elementSize;
  array->length      = 0;
  array->maxLength   = length;

  #ifndef NDEBUG
    pthread_once(&debugArrayInitFlag,debugArrayInit);

    pthread_mutex_lock(&debugArrayLock);
    {
      debugArrayNode = LIST_NEW_NODE(DebugArrayNode);
      if (debugArrayNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      debugArrayNode->fileName = fileName;
      debugArrayNode->lineNb   = lineNb;
      debugArrayNode->array    = array;
      List_append(&debugArrayList,debugArrayNode);
      debugArrayList.allocatedMemory += sizeof(DebugArrayNode)+sizeof(struct __Array)+array->elementSize*array->maxLength;
    }
    pthread_mutex_unlock(&debugArrayLock);
  #endif /* not NDEBUG */

  return array;
}

void Array_delete(Array array, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData)
{
  ulong z;
  #ifndef NDEBUG
    DebugArrayNode *debugArrayNode;
  #endif /* not NDEBUG */

  if (array != NULL)
  {
    assert(array->data != NULL);

    if (arrayElementFreeFunction != NULL)
    {
      for (z = 0; z < array->length; z++)
      {
        arrayElementFreeFunction(array->data+z*array->elementSize,arrayElementFreeUserData);
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
          assert(debugArrayList.allocatedMemory >= sizeof(DebugArrayNode)+sizeof(struct __Array)+array->maxLength*array->elementSize);
          debugArrayList.allocatedMemory -= sizeof(DebugArrayNode)+sizeof(struct __Array)+array->maxLength*array->elementSize;
          LIST_DELETE_NODE(debugArrayNode);
        }
        else
        {
          fprintf(stderr,"DEBUG WARNING: array %p not found in debug list!\n",
                  array
                 );
        }
      }
      pthread_mutex_unlock(&debugArrayLock);
    #endif /* not NDEBUG */

    free(array->data);
    free(array);
  }
}

void Array_clear(Array array, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData)
{
  ulong z;

  if (array != NULL)
  {
    assert(array->data != NULL);

    if (arrayElementFreeFunction != NULL)
    {
      for (z = 0; z < array->length; z++)
      {
        arrayElementFreeFunction(array->data+z*array->elementSize,arrayElementFreeUserData);
      }
    }
    array->length = 0;
  }
}

ulong Array_length(const Array array)
{
  return (array != NULL) ? array->length : 0;
}

bool Array_put(Array array, ulong index, const void *data)
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

void *Array_get(const Array array, ulong index, void *data)
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

bool Array_insert(Array array, long nextIndex, const void *data)
{
  byte *newData;

  if (array != NULL)
  {
    assert(array->data != NULL);

    // extend array if needed
    if      (nextIndex == ARRAY_END)
    {
    }
    else if ((ulong)nextIndex+1 >= array->maxLength)
    {
      newData = realloc(array->data,(nextIndex+1)*array->elementSize);
      if (newData == NULL)
      {
        return FALSE;
      }
      array->maxLength = nextIndex+1;
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
    return FALSE;
  }
}

bool Array_append(Array array, const void *data)
{
  byte *newData;

  if (array != NULL)
  {
    assert(array->data != NULL);

    // extend array if needed
    if (array->length >= array->maxLength)
    {
      newData = realloc(array->data,(array->maxLength+DELTA_LENGTH)*array->elementSize);
      if (newData == NULL)
      {
        return FALSE;
      }
      array->maxLength = array->maxLength+DELTA_LENGTH;
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

void Array_remove(Array array, ulong index, ArrayElementFreeFunction arrayElementFreeFunction, void *arrayElementFreeUserData)
{
  if (array != NULL)
  {
    assert(array->data != NULL);

    // remove element
    if (index < array->length)
    {
      if (arrayElementFreeFunction != NULL)
      {
        arrayElementFreeFunction(array->data+index*array->elementSize,arrayElementFreeUserData);
      }

      if (index < array->length-1)
      {
        memmove(array->data+index*array->elementSize,
                array->data+(index+1)*array->elementSize,
                array->elementSize*(array->length-index)
               );
      }
      array->length--;
    }
    else
    {
    }
  }
}

const void *Array_cArray(const Array array)
{
  return (array != NULL) ? array->data : NULL;
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

void Array_debugDumpInfo(FILE *handle)
{
  DebugArrayNode *debugArrayNode;

  pthread_once(&debugArrayInitFlag,debugArrayInit);

  pthread_mutex_lock(&debugArrayLock);
  {
    LIST_ITERATE(&debugArrayList,debugArrayNode)
    {
      fprintf(handle,"DEBUG: array %p[%ld] allocated at %s, line %ld\n",
              debugArrayNode->array->data,
              debugArrayNode->array->maxLength,
              debugArrayNode->fileName,
              debugArrayNode->lineNb
             );
    }
  }
  pthread_mutex_unlock(&debugArrayLock);
}

void Array_debugPrintInfo(void)
{
  Array_debugDumpInfo(stderr);
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
  pthread_once(&debugArrayInitFlag,debugArrayInit);

  Array_debugPrintInfo();
  Array_debugPrintStatistics();

  pthread_mutex_lock(&debugArrayLock);
  {
    if (!List_isEmpty(&debugArrayList))
    {
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
