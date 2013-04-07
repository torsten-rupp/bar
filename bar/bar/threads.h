/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

#ifndef __THREADS__
#define __TRHEADS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define THREAD_ID_NONE -1

/***************************** Datatypes *******************************/

typedef struct
{
  pthread_t handle;
} Thread;

typedef pthread_t ThreadId;

// thread local storage
typedef struct ThreadLocalStorageInstanceNode
{
  LIST_NODE_HEADER(struct ThreadLocalStorageInstanceNode);

  #ifndef HAVE__THREAD
    ThreadId threadId;
  #endif /* not HAVE__THREAD */
  void *p;
} ThreadLocalStorageInstanceNode;

typedef struct
{
  LIST_HEADER(ThreadLocalStorageInstanceNode);
} ThreadLocalStorageInstanceList;

typedef void*(*ThreadLocalStorageAllocFunction)(void *userData);
typedef void(*ThreadLocalStorageFreeFunction)(void *variable, void *userData);

typedef struct
{
  ThreadLocalStorageAllocFunction allocFunction;
  void                            *allocUserData;
  pthread_mutex_t                 lock;
  ThreadLocalStorageInstanceList  instanceList;
} ThreadLocalStorage;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Thread_getNumberOfCores
* Purpose: get number of cpu cores
* Input  : -
* Output : -
* Return : number of cpu cores
* Notes  : -
\***********************************************************************/

uint Thread_getNumberOfCores(void);

/***********************************************************************\
* Name   : Thread_init
* Purpose: init thread
* Input  : thread        - thread variable
*          name          - name
*          niceLevel     - nice level or 0 for default level
*          entryFunction - thread entry function
*          userData      - thread user data
* Output : -
* Return : TRUE if thread started, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Thread_init(Thread     *thread,
                 const char *name,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *userData
                );

/***********************************************************************\
* Name   : Thread_done
* Purpose: deinitialize thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_done(Thread *thread);

/***********************************************************************\
* Name   : Thread_join
* Purpose: wait for termination of thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_join(Thread *thread);

/***********************************************************************\
* Name   : Thread_yield
* Purpose: reschedule thread execution
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Thread_yield(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENATION__)
INLINE void Thread_yield(void)
{
  sched_yield();
}
#endif /* NDEBUG || __THREADS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Thread_getCurrentId
* Purpose: get current thread id
* Input  : -
* Output : -
* Return : current thread id
* Notes  : -
\***********************************************************************/

INLINE ThreadId Thread_getCurrentId(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENATION__)
INLINE ThreadId Thread_getCurrentId(void)
{
  return pthread_self();
}
#endif /* NDEBUG || __THREADS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Thread_equalThreads
* Purpose: compare thread ids
* Input  : threadId0,threadId1 - thread ids
* Output : -
* Return : TRUE iff thread ids are equal
* Notes  : -
\***********************************************************************/

INLINE bool Thread_equalThreads(ThreadId threadId0, ThreadId threadId1);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENATION__)
INLINE bool Thread_equalThreads(ThreadId threadId0, ThreadId threadId1)
{
  return pthread_equal(threadId0,threadId1) != 0;
}
#endif /* NDEBUG || __THREADS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Thread_isCurrentThread
* Purpose: check if thread is current thread
* Input  : threadId thread Id
* Output : -
* Return : TRUE if thread id is current thread
* Notes  : -
\***********************************************************************/

INLINE bool Thread_isCurrentThread(ThreadId threadId);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENATION__)
INLINE bool Thread_isCurrentThread(ThreadId threadId)
{
  return pthread_equal(threadId,pthread_self()) != 0;
}
#endif /* NDEBUG || __THREADS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Thread_initLocalVariable
* Purpose: init thread local variable
* Input  : threadLocalStorageAllocFunction - allocate new variable
*                                            instance callback code
*          threadLocalStorageAllocUserData - user data for callback
* Output : threadLocalStorage - initialized thread local storage handle
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_initLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageAllocFunction threadLocalStorageAllocFunction, void *threadLocalStorageAllocUserData);

/***********************************************************************\
* Name   : Thread_doneLocalVariable
* Purpose: done thread local variable
* Input  : threadLocalStorage - thread local storage handle
*          threadLocalStorageFreeFunction - free variable instance
*                                           callback code
*          threadLocalStorageFreeUserData - user data for callback
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_doneLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageFreeFunction threadLocalStorageFreeFunction, void *threadLocalStorageFreeUserData);

/***********************************************************************\
* Name   : Thread_getLocalVariable
* Purpose: get thread local variable
* Input  : threadLocalStorage - thread local storage handle
* Output : -
* Return : thread local variable or NULL
* Notes  : -
\***********************************************************************/

void *Thread_getLocalVariable(ThreadLocalStorage *threadLocalStorage);

#ifdef __cplusplus
  }
#endif

#endif /* __TRHEADS__ */

/* end of file */
