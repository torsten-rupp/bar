/***********************************************************************\
*
* $Revision: 2615 $
* $Date: 2013-09-02 20:57:12 +0200 (Mon, 02 Sep 2013) $
* $Author: trupp $
* Contents: auto-free resource functions
* Systems: Linux
*
\***********************************************************************/

#ifndef __AUTOFREE__
#define __AUTOFREE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <pthread.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
#endif /* PLATFORM_... */

#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/

// free functions
typedef void(*AutoFreeFunction)(void *resource);

typedef struct AutoFreeNode
{
  LIST_NODE_HEADER(struct AutoFreeNode);

  void             *resource;
  AutoFreeFunction autoFreeFunction;
  void             *autoFreeUserData;

  #ifndef NDEBUG
    const char     *fileName;
    ulong          lineNb;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  #endif /* not NDEBUG */
} AutoFreeNode;

typedef struct
{
  LIST_HEADER(AutoFreeNode);
  pthread_mutex_t lock;
} AutoFreeList;

/**************************** Variables ********************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define AutoFree_add(autoFreeList,resource,autoFreeFunction) __AutoFree_add(__FILE__,__LINE__,autoFreeList,resource,autoFreeFunction)
  #define AutoFree_remove(autoFreeList,resource)               __AutoFree_remove(__FILE__,__LINE__,autoFreeList,resource)
#endif /* not NDEBUG */

/* Note: for these macros a GNU C compiler is required, because
         anonymous functions are used. See
         http://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
*/
#ifdef __GNUC__
  #define AUTOFREE_ADD(autoFreeList,resource_,type,freeFunctionBody) \
    AutoFree_add(autoFreeList,\
                 (void*)(unsigned long long)resource_, \
                 (AutoFreeFunction)({ \
                                     auto void __closure__(type); \
                                     void __closure__(type resource)freeFunctionBody __closure__; \
                                   }) \
                )

  #define AUTOFREE_ADDX(autoFreeList,resource_,freeFunctionSignature,freeFunctionBody) \
    AutoFree_add(autoFreeList,\
                 (void*)(unsigned long long)resource_, \
                 (AutoFreeFunction)({ \
                                     auto void __closure__ freeFunctionSignature; \
                                     void __closure__ freeFunctionSignature freeFunctionBody __closure__; \
                                   }) \
                )

  #define AUTOFREE_FREE(functionBody) \
  ({ \
      void __fn__ functionBody __fn__; \
  })
#endif /* __GNUC__ */

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************\
* Name   : AutoFree_init
* Purpose: initialize auto-free list
* Input  : autoFreeList - auto-free list variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_init(AutoFreeList *autoFreeList);

/***********************************************************************\
* Name   : AutoFree_done
* Purpose: done auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_done(AutoFreeList *autoFreeList);

/***********************************************************************\
* Name   : AutoFree_done
* Purpose: free all resources and done auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_freeDone(AutoFreeList *autoFreeList);

/***********************************************************************\
* Name   : AutoFree_add
* Purpose: add resource to auto-free list
* Input  : autoFreeList     - auto-free list
*          resource         - resource
*          autoFreeFunction - free function
* Output : -
* Return : TRUE iff resourced added
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool AutoFree_add(AutoFreeList     *autoFreeList,
                  void             *resource,
                  AutoFreeFunction autoFreeFunction
                 );
#else /* not NDEBUG */
bool __AutoFree_add(const char       *__fileName__,
                    ulong            __lineNb__,
                    AutoFreeList     *autoFreeList,
                    void             *resource,
                    AutoFreeFunction autoFreeFunction
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : AutoFree_remove
* Purpose: remove resource from auto-free list
* Input  : autoFreeList - auto-free list
*          resource     - resource
* Output : -
* Return : -
* Notes  : resource is not freed!
\***********************************************************************/

#ifdef NDEBUG
void AutoFree_remove(AutoFreeList *autoFreeList,
                     void         *resource
                    );
#else /* not NDEBUG */
void __AutoFree_remove(const char   *__fileName__,
                       ulong        __lineNb__,
                       AutoFreeList *autoFreeList,
                       void         *resource
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : AutoFree_free
* Purpose: remove resource from auto-free list and free resource
* Input  : autoFreeList - auto-free list
*          resource     - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_free(AutoFreeList *autoFreeList, void *resource);

/***********************************************************************\
* Name   : AutoFree_freeAll
* Purpose: free all resources
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_freeAll(AutoFreeList *autoFreeList);

#ifndef NDEBUG

/***********************************************************************\
* Name   : AutoFree_dumpInfo
* Purpose: dump auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_dumpInfo(AutoFreeList *autoFreeList, FILE *handle);

/***********************************************************************\
* Name   : AutoFree_printInfo
* Purpose: print info for auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_printInfo(AutoFreeList *autoFreeList);

#endif /* not NDEBUG */

#ifdef __cplusplus
}
#endif

#endif /* __AUTOFREE__ */

/* end of file */
