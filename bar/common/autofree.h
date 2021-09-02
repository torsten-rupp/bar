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
#include <stdint.h>
#include <pthread.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
#endif /* PLATFORM_... */

#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/

// free functions
typedef void(*AutoFreeFunction)(const void *resource);

typedef struct AutoFreeNode
{
  LIST_NODE_HEADER(struct AutoFreeNode);

  const void       *resource;
  AutoFreeFunction autoFreeFunction;
  void             *autoFreeUserData;

  #ifndef NDEBUG
    const char   *fileName;
    ulong        lineNb;
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
  #define AutoFree_add(...)    __AutoFree_add(__FILE__,__LINE__, ## __VA_ARGS__)
  #define AutoFree_remove(...) __AutoFree_remove(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/* Note: for these macros a GNU C compiler is required, because
         anonymous functions are used. See
         http://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
*/
#ifdef __GNUC__
  #define AUTOFREE_ADD(autoFreeList,resource_,freeFunctionBody) \
    AutoFree_add(autoFreeList, \
                 resource_, \
                 (AutoFreeFunction)({ \
                                     auto void __closure__(void); \
                                     void __closure__(void)freeFunctionBody __closure__; \
                                   }) \
                )

  #define AUTOFREE_ADDX(autoFreeList,resource_,freeFunctionSignature,freeFunctionBody) \
    AutoFree_add(autoFreeList, \
                 resource_, \
                 (AutoFreeFunction)({ \
                                     auto void __closure__ freeFunctionSignature; \
                                     void __closure__ freeFunctionSignature freeFunctionBody __closure__; \
                                   }) \
                )

  #define AUTOFREE_REMOVE(autoFreeList,resource_) \
    do \
    { \
      AutoFree_remove(autoFreeList, \
                      resource_ \
                     ); \
    } \
    while (0)

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
* Notes  : Resources are not freed!
\***********************************************************************/

void AutoFree_done(AutoFreeList *autoFreeList);

/***********************************************************************\
* Name   : AutoFree_save
* Purpose: save end of autofree list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : store point
* Notes  : -
\***********************************************************************/

void *AutoFree_save(AutoFreeList *autoFreeList);

/***********************************************************************\
* Name   : AutoFree_restore
* Purpose: restore saved auto-free list
* Input  : autoFreeList - auto-free list
*          savePoint    - save point
*          freeFlag     - TRUE to free resources
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_restore(AutoFreeList *autoFreeList,
                      void         *savePoint,
                      bool         freeFlag
                     );

/***********************************************************************\
* Name   : AutoFree_cleanup
* Purpose: cleanup all resources and free auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_cleanup(AutoFreeList *autoFreeList);

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
                  const void       *resource,
                  AutoFreeFunction autoFreeFunction
                 );
#else /* not NDEBUG */
bool __AutoFree_add(const char       *__fileName__,
                    ulong            __lineNb__,
                    AutoFreeList     *autoFreeList,
                    const void       *resource,
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
                     const void   *resource
                    );
#else /* not NDEBUG */
void __AutoFree_remove(const char   *__fileName__,
                       ulong        __lineNb__,
                       AutoFreeList *autoFreeList,
                       const void   *resource
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

void AutoFree_free(AutoFreeList *autoFreeList,
                   const void   *resource
                  );

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
* Name   : AutoFree_debugDumpInfo
* Purpose: dump auto-free list
* Input  : autoFreeList - auto-free list
*          handle       - output file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_debugDumpInfo(AutoFreeList *autoFreeList, FILE *handle);

/***********************************************************************\
* Name   : AutoFree_debugPrintInfo
* Purpose: print info for auto-free list
* Input  : autoFreeList - auto-free list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void AutoFree_debugPrintInfo(AutoFreeList *autoFreeList);
#endif /* not NDEBUG */

#ifdef __cplusplus
}
#endif

#endif /* __AUTOFREE__ */

/* end of file */
