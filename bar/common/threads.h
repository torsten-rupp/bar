/***********************************************************************\
*
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

#ifndef __THREADS__
#define __THREADS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <unistd.h>
  #include <sys/syscall.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define THREAD_LPW_ID_NONE 0

/***************************** Datatypes *******************************/

typedef pthread_t ThreadId;
typedef pid_t ThreadLWPId;

typedef struct
{
  pthread_t handle;
  bool      quitFlag;             // TRUE to request thread quit
  bool      terminatedFlag;       // TRUE iff terminated
  bool      joinedFlag;           // TRUE iff thread joined
} Thread;

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
extern const ThreadId THREAD_ID_NONE;

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Thread_init(...) __Thread_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Thread_done(...) __Thread_done(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Thread_initAll
* Purpose: initialize thread functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Thread_initAll(void);

/***********************************************************************\
* Name   : Thread_doneAll
* Purpose: deinitialize thread functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_doneAll(void);

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
*          argument      - thread argument
* Output : -
* Return : TRUE if thread started, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Thread_init(Thread     *thread,
                 const char *name,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *argument
                );
#else /* not NDEBUG */
bool __Thread_init(const char *__fileName__,
                   size_t     __lineNb__,
                   Thread     *thread,
                   const char *name,
                   int        niceLevel,
                   const void *entryFunction,
                   void       *argument
                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Thread_done
* Purpose: deinitialize thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Thread_done(Thread *thread);
#else /* not NDEBUG */
void __Thread_done(const char *__fileName__,
                   size_t     __lineNb__,
                   Thread     *thread
                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Thread_getPriority
* Purpose: get thread priority
* Input  : thread - thread
* Output : -
* Return : thread priority
* Notes  : -
\***********************************************************************/

int Thread_getPriority(Thread *thread);

/***********************************************************************\
* Name   : Thread_setPriority
* Purpose: set thread priority
* Input  : thread   - thread
*          priority - priority
* Output : -
* Return : TRUE iff priority set
* Notes  : -
\***********************************************************************/

bool Thread_setPriority(Thread *thread, int priority);

/***********************************************************************\
* Name   : Thread_quit
* Purpose: request quit thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Thread_quit(Thread *thread);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE void Thread_quit(Thread *thread)
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  thread->quitFlag = TRUE;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_isQuit
* Purpose: check if thread should quit
* Input  : thread - thread
* Output : -
* Return : TRUE iff quit thread requested
* Notes  : -
\***********************************************************************/

INLINE bool Thread_isQuit(const Thread *thread);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE bool Thread_isQuit(const Thread *thread)
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  return thread->quitFlag;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_isTerminated
* Purpose: check if thread terminated
* Input  : thread - thread
* Output : -
* Return : TRUE iff quit thread requested
* Notes  : -
\***********************************************************************/

INLINE bool Thread_isTerminated(const Thread *thread);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE bool Thread_isTerminated(const Thread *thread)
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  return thread->terminatedFlag;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_join
* Purpose: wait for termination of thread
* Input  : thread - thread
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Thread_join(Thread *thread);

/***********************************************************************\
* Name   : Thread_yieldThread_init
* Purpose: reschedule thread execution
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Thread_yield(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE void Thread_yield(void)
{
  sched_yield();
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_delay
* Purpose: delay thread execution
* Input  : time - delay time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_delay(size_t time);

/***********************************************************************\
* Name   : Thread_getId
* Purpose: get thread id
* Input  : thread - thread
* Output : -
* Return : thread id
* Notes  : -
\***********************************************************************/

INLINE ThreadId Thread_getId(const Thread *thread);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE ThreadId Thread_getId(const Thread *thread)
{
  assert(thread != NULL);

  return thread->handle;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_getCurrentId
* Purpose: get current thread id
* Input  : -
* Output : -
* Return : current thread id
* Notes  : -
\***********************************************************************/

INLINE ThreadId Thread_getCurrentId(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE ThreadId Thread_getCurrentId(void)
{
  return pthread_self();
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_getCurrentLWPId
* Purpose: get current thread LWP id
* Input  : -
* Output : -
* Return : current thread LWP id or THREAD_LPW_ID_NONE
* Notes  : -
\***********************************************************************/

INLINE ThreadLWPId Thread_getCurrentLWPId(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE ThreadLWPId Thread_getCurrentLWPId(void)
{
  #if   defined(PLATFORM_LINUX)
    #ifdef SYS_gettid
      return (ThreadLWPId)syscall(SYS_gettid);
    #else
      return THREAD_LPW_ID_NONE;
    #endif
  #elif defined(PLATFORM_WINDOWS)
    return THREAD_LPW_ID_NONE;
  #endif /* PLATFORM_... */
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_equalThreads
* Purpose: compare thread ids
* Input  : threadId0,threadId1 - thread ids
* Output : -
* Return : TRUE iff thread ids are equal
* Notes  : -
\***********************************************************************/

INLINE bool Thread_equalThreads(const ThreadId threadId0, const ThreadId threadId1);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE bool Thread_equalThreads(const ThreadId threadId0, const ThreadId threadId1)
{
  return pthread_equal(threadId0,threadId1) != 0;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_isCurrentThread
* Purpose: check if thread id is current thread
* Input  : threadId thread Id
* Output : -
* Return : TRUE if thread id is current thread
* Notes  : -
\***********************************************************************/

INLINE bool Thread_isCurrentThread(const ThreadId threadId);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE bool Thread_isCurrentThread(const ThreadId threadId)
{
  return pthread_equal(threadId,pthread_self()) != 0;
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_isNone
* Purpose: check if thread id is none
* Input  : threadId thread Id
* Output : -
* Return : TRUE if thread id is none
* Notes  : -
\***********************************************************************/

INLINE bool Thread_isNone(const ThreadId threadId);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE bool Thread_isNone(const ThreadId threadId)
{
  return memEquals(&threadId,sizeof(threadId),&THREAD_ID_NONE,sizeof(THREAD_ID_NONE));
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Thread_getName
* Purpose: get name of thread
* Input  : threadId - thread id
* Output : -
* Return : thread name or "unknown"
* Notes  : -
\***********************************************************************/

const char *Thread_getName(const ThreadId threadId);

/***********************************************************************\
* Name   : Thread_getCurrentName
* Purpose: get name of current thread
* Input  : -
* Output : -
* Return : thread name or NULL
* Notes  : -
\***********************************************************************/

const char *Thread_getCurrentName(void);

/***********************************************************************\
* Name   : Thread_getIdString
* Purpose: get id string of thread
* Input  : threadId - thread id
* Output : -
* Return : thread id string or NULL
* Notes  : -
\***********************************************************************/

const char *Thread_getIdString(const ThreadId threadId);

/***********************************************************************\
* Name   : Thread_getCurrentIdString
* Purpose: get id string of current thread
* Input  : -
* Output : -
* Return : thread id string or NULL
* Notes  : -
\***********************************************************************/

const char *Thread_getCurrentIdString(void);

/***********************************************************************\
* Name   : Thread_getCurrentNumber
* Purpose: get current thread number (integer)
* Input  : -
* Output : -
* Return : current thread number (integer)
* Notes  : -
\***********************************************************************/

INLINE uint64 Thread_getCurrentNumber(void);
#if defined(NDEBUG) || defined(__THREADS_IMPLEMENTATION__)
INLINE uint64 Thread_getCurrentNumber(void)
{
  #ifdef SYS_gettid
    return (uint64)syscall(SYS_gettid);
  #else
    return 0LL;
  #endif
}
#endif /* NDEBUG || __THREADS_IMPLEMENTATION__ */

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
