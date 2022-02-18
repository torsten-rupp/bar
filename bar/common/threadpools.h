/***********************************************************************\
*
* $Revision: 11078 $
* $Date: 2020-08-04 10:41:15 +0200 (Tue, 04 Aug 2020) $
* $Author: torsten $
* Contents: thread pool functions
* Systems: all
*
\***********************************************************************/

#ifndef __THREADPOOLS__
#define __THREADPOOLS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include <common/threads.h>

#if   defined(PLATFORM_LINUX)
  #include <unistd.h>
  #include <sys/syscall.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/lists.h"
#include "common/msgqueues.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  THREADPOOL_THREAD_STATE_IDLE,
  THREADPOOL_THREAD_STATE_RUNNING,
  THREADPOOL_THREAD_STATE_QUIT
} ThreadPoolThreadStates;

typedef struct ThreadPoolNode
{
  LIST_NODE_HEADER(struct ThreadPoolNode);
  ThreadPoolThreadStates state;
  pthread_t              thread;
  pthread_t              usedBy;
  const void             *entryFunction;
  void                   *argument;
  pthread_cond_t         trigger;
} ThreadPoolNode;

typedef struct
{
  LIST_HEADER(ThreadPoolNode);
} ThreadPoolList;

typedef struct
{
  char            namePrefix[32];
  int             niceLevel;
  uint            maxSize;

  pthread_mutex_t lock;
  pthread_cond_t  modified;

  ThreadPoolList  idle;
  ThreadPoolList  running;
  uint            size;

  bool            quitFlag;
} ThreadPool;

typedef struct
{
} ThreadPoolSet;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : ThreadPool_initAll
* Purpose: initialize thread pool functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ThreadPool_initAll(void);

/***********************************************************************\
* Name   : ThreadPool_doneAll
* Purpose: deinitialize thread pool functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ThreadPool_doneAll(void);

/***********************************************************************\
* Name   : ThreadPool_init
* Purpose: init thread pool
* Input  : threadPool - thread pool variable
*          namePrefix - thread name prefix
*          niceLevel  - nice level or 0 for default level
*          size       - number of threads in pool
*          maxSize    - max. number of threads in pool
* Output : threadPool - thread pool
* Return : TURE iff no error
* Notes  : -
\***********************************************************************/

bool ThreadPool_init(ThreadPool *threadPool,
                     const char *namePrefix,
                     int        niceLevel,
                     uint       size,
                     uint       maxSize
                    );

/***********************************************************************\
* Name   : ThreadPool_done
* Purpose: done thread pool
* Input  : threadPool - thread pool
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
void ThreadPool_done(ThreadPool *threadPool);

/***********************************************************************\
* Name   : ThreadPool_initSet
* Purpose: init thread pool set
* Input  : threadPoolSet - thread pool set variable
* Output : threadPoolSet - thread pool set
* Return : -
* Notes  : -
\***********************************************************************/

void ThreadPool_initSet(ThreadPoolSet *threadPoolSet);

/***********************************************************************\
* Name   : ThreadPool_doneSet
* Purpose: done thread pool set
* Input  : threadPoolSet - thread pool set
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ThreadPool_doneSet(ThreadPoolSet *threadPoolSet);

/***********************************************************************\
* Name   : ThreadPool_run
* Purpose: run function with thread from thread pool
* Input  : threadPool    - thread pool
*          entryFunction - thread entry function
*          argument      - thread argument
* Output : -
* Return : thread pool node
* Notes  : -
\***********************************************************************/

ThreadPoolNode *ThreadPool_run(ThreadPool *threadPool,
                               const void *entryFunction,
                               void       *argument
                              );

/***********************************************************************\
* Name   : ThreadPool_join
* Purpose: wait for termination of thread
* Input  : threadPool     - thread pool
*          threadPoolNode - thread pool node
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool ThreadPool_join(ThreadPool *threadPool, ThreadPoolNode *threadPoolNode);

/***********************************************************************\
* Name   : ThreadPool_joinAll
* Purpose: wait for termination of all threads in pool allocated by
*          calling thread
* Input  : threadPool - thread pool
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool ThreadPool_joinAll(ThreadPool *threadPool);

/***********************************************************************\
* Name   : ThreadPool_idleCount
* Purpose: get number of idle threads in pool
* Input  : threadPool - thread pool
* Output : -
* Return : number of idle threads
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
INLINE uint ThreadPool_idleCount(ThreadPool *threadPool);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE uint ThreadPool_idleCount(ThreadPool *threadPool)
{
  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);

  return List_count(&threadPool->idle);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ThreadPool_runningCount
* Purpose: get number of running threads in pool
* Input  : threadPool - thread pool
* Output : -
* Return : number of running threads
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
INLINE uint ThreadPool_runningCount(ThreadPool *threadPool);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE uint ThreadPool_runningCount(ThreadPool *threadPool)
{
  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);

  return List_count(&threadPool->running);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */
#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __THREADPOOLS__ */

/* end of file */
