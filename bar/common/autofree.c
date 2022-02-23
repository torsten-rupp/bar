/***********************************************************************\
*
* $Revision: 2636 $
* $Date: 2013-09-02 21:22:01 +0200 (Mon, 02 Sep 2013) $
* $Author: trupp $
* Contents: auto resource functions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <pthread.h>
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#include <assert.h>

#include "common/lists.h"

#include "autofree.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_MAX_FREE_LIST 4000

/**************************** Datatypes ********************************/

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------

void AutoFree_init(AutoFreeList *autoFreeList)
{
  assert(autoFreeList != NULL);

  pthread_mutex_init(&autoFreeList->lock,NULL);
  List_init(autoFreeList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
}

void AutoFree_done(AutoFreeList *autoFreeList)
{
  assert(autoFreeList != NULL);

  List_done(autoFreeList);
  pthread_mutex_destroy(&autoFreeList->lock);
}

void *AutoFree_save(AutoFreeList *autoFreeList)
{
  assert(autoFreeList != NULL);

  return autoFreeList->tail;
}

void AutoFree_restore(AutoFreeList *autoFreeList, void *savePoint, bool freeFlag)
{
  AutoFreeNode *autoFreeNode;

  assert(autoFreeList != NULL);

  while (autoFreeList->tail != savePoint)
  {
    // remove last from list
    autoFreeNode = (AutoFreeNode*)List_removeLast(autoFreeList);

    // free resource
    if (freeFlag && (autoFreeNode->autoFreeFunction != NULL))
    {
#if 0
      #ifndef NDEBUG
        fprintf(stderr,
                "DEBUG: call auto free %p at %s, line %lu with auto resource 0x%llu\n",
                autoFreeNode->autoFreeFunction,
                autoFreeNode->fileName,
                autoFreeNode->lineNb,
                autoFreeNode->resource
               );
      #endif /* not NDEBUG */
#endif /* 0 */
      autoFreeNode->autoFreeFunction(autoFreeNode->resource);
    }

    // free node
    LIST_DELETE_NODE(autoFreeNode);
  }
}

void AutoFree_cleanup(AutoFreeList *autoFreeList)
{
  assert(autoFreeList != NULL);

  AutoFree_freeAll(autoFreeList);
  List_done(autoFreeList);
  pthread_mutex_destroy(&autoFreeList->lock);
}

#ifdef NDEBUG
bool AutoFree_add(AutoFreeList     *autoFreeList,
                  const void       *resource,
                  AutoFreeFunction autoFreeFunction
                 )
#else /* not NDEBUG */
bool __AutoFree_add(const char       *__fileName__,
                    ulong            __lineNb__,
                    AutoFreeList     *autoFreeList,
                    const void       *resource,
                    AutoFreeFunction autoFreeFunction
                   )
#endif /* NDEBUG */
{
  AutoFreeNode *autoFreeNode;

  assert(autoFreeList != NULL);

  pthread_mutex_lock(&autoFreeList->lock);
  {
    // allocate new node
    #ifdef NDEBUG
      autoFreeNode = LIST_NEW_NODE(AutoFreeNode);
    #else /* not NDEBUG */
      autoFreeNode = LIST_NEW_NODEX(__fileName__,__lineNb__,AutoFreeNode);
    #endif /* NDEBUG */
    if (autoFreeNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // init resource node
    autoFreeNode->resource         = resource;
    autoFreeNode->autoFreeFunction = autoFreeFunction;
    #ifndef NDEBUG
      autoFreeNode->fileName = __fileName__;
      autoFreeNode->lineNb   = __lineNb__;
      #ifdef HAVE_BACKTRACE
        autoFreeNode->stackTraceSize = backtrace((void*)autoFreeNode->stackTrace,SIZE_OF_ARRAY(autoFreeNode->stackTrace));
      #endif /* HAVE_BACKTRACE */
    #endif /* NDEBUG */

    // add resource to allocated-list
    List_append(autoFreeList,autoFreeNode);
  }
  pthread_mutex_unlock(&autoFreeList->lock);

  return TRUE;
}

#ifdef NDEBUG
void AutoFree_remove(AutoFreeList *autoFreeList,
                     const void   *resource
                    )
#else /* not NDEBUG */
void __AutoFree_remove(const char   *__fileName__,
                       ulong        __lineNb__,
                       AutoFreeList *autoFreeList,
                       const void   *resource
                      )
#endif /* NDEBUG */
{
  bool         foundFlag;
  AutoFreeNode *autoFreeNode;

  assert(autoFreeList != NULL);

  pthread_mutex_lock(&autoFreeList->lock);
  {
    // remove resources from list
    foundFlag = FALSE;
    autoFreeNode = autoFreeList->tail;
    while (autoFreeNode != NULL)
    {
      if (autoFreeNode->resource == resource)
      {
        // remove from list
        autoFreeNode = List_removeAndFree(autoFreeList,autoFreeNode,CALLBACK_(NULL,NULL));
        foundFlag = TRUE;
      }
      else
      {
        // previous node
        autoFreeNode = autoFreeNode->prev;
      }
    }
    if (!foundFlag)
    {
      #ifndef NDEBUG
        fprintf(stderr,"DEBUG WARNING: auto resource %p not found in auto resource list at %s, line %lu\n",
                resource,
                __fileName__,
                __lineNb__
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
        #endif /* HAVE_BACKTRACE */
      #else /* not NDEBUG */
        fprintf(stderr,"DEBUG WARNING: auto resource %p not found in auto resource list\n",
                resource
               );
      #endif /* not NDEBUG */
      HALT_INTERNAL_ERROR("auto-free remove fail");
    }
  }
  pthread_mutex_unlock(&autoFreeList->lock);
}

void AutoFree_free(AutoFreeList *autoFreeList,
                   const void   *resource
                  )
{
  AutoFreeNode *autoFreeNode;

  assert(autoFreeList != NULL);

  pthread_mutex_lock(&autoFreeList->lock);
  {
    // remove resource from list
    autoFreeNode = autoFreeList->tail;
    while ((autoFreeNode != NULL) && (autoFreeNode->resource != resource))
    {
      autoFreeNode = autoFreeNode->prev;
    }
    if (autoFreeNode != NULL)
    {
      // remove from list
      List_remove(autoFreeList,autoFreeNode);

      // free resource
      if (autoFreeNode->autoFreeFunction != NULL)
      {
        autoFreeNode->autoFreeFunction(autoFreeNode->resource);
      }

      // free node
      LIST_DELETE_NODE(autoFreeNode);
    }
  }
  pthread_mutex_unlock(&autoFreeList->lock);
}

void AutoFree_freeAll(AutoFreeList *autoFreeList)
{
  AutoFreeNode *autoFreeNode;

  assert(autoFreeList != NULL);

  pthread_mutex_lock(&autoFreeList->lock);
  {
    while (!List_isEmpty(autoFreeList))
    {
      // remove from list
      autoFreeNode = (AutoFreeNode*)List_removeLast(autoFreeList);

      // free resource
      if (autoFreeNode->autoFreeFunction != NULL)
      {
#if 0
        #ifndef NDEBUG
          fprintf(stderr,
                  "DEBUG: call auto free %p at %s, line %lu with auto resource 0x%llu\n",
                  autoFreeNode->autoFreeFunction,
                  autoFreeNode->fileName,
                  autoFreeNode->lineNb,
                  autoFreeNode->resource
                 );
        #endif /* not NDEBUG */
#endif /* 0 */
        autoFreeNode->autoFreeFunction(autoFreeNode->resource);
      }

      // free node
      LIST_DELETE_NODE(autoFreeNode);
    }
  }
  pthread_mutex_unlock(&autoFreeList->lock);
}

#ifndef NDEBUG
void AutoFree_debugDumpInfo(AutoFreeList *autoFreeList, FILE *handle)
{
  AutoFreeNode *autoFreeNode;

  pthread_mutex_lock(&autoFreeList->lock);
  {
    LIST_ITERATE(autoFreeList,autoFreeNode)
    {
      fprintf(handle,"DEBUG: auto resource %p added at %s, line %lu\n",
              autoFreeNode->resource,
              autoFreeNode->fileName,
              autoFreeNode->lineNb
             );
    }
  }
  pthread_mutex_unlock(&autoFreeList->lock);
}

void AutoFree_debugPrintInfo(AutoFreeList *autoFreeList)
{
  AutoFree_debugDumpInfo(autoFreeList,stderr);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
}
#endif

/* end of file */
