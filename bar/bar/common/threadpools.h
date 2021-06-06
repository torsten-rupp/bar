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
  pthread_mutex_t lock;
  pthread_cond_t  modified;

  ThreadPoolList  idle;
  ThreadPoolList  running;

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
* Purpose: initialize thread functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ThreadPool_initAll(void);

/***********************************************************************\
* Name   : ThreadPool_doneAll
* Purpose: deinitialize thread functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ThreadPool_doneAll(void);

bool ThreadPool_init(ThreadPool *threadPool,
                     const char *name,
                     int        niceLevel,
                     uint       size
                    );
void ThreadPool_done(ThreadPool *threadPool);

void ThreadPool_initSet(ThreadPoolSet *threadPoolSet);
void ThreadPool_doneSet(ThreadPoolSet *threadPoolSet);

void ThreadPool_run(ThreadPool *threadPool,
                    const void *entryFunction,
                    void       *argument
                   );

/***********************************************************************\
* Name   : ThreadPool_joinAll
* Purpose: wait for termination of thread
* Input  : thread - thread
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool ThreadPool_joinAll(ThreadPool *threadPool);

#ifdef __cplusplus
  }
#endif

#endif /* __THREADPOOLS__ */

/* end of file */
