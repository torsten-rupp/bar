/***********************************************************************\
*
* $Revision: 11036 $
* $Date: 2020-07-28 08:02:25 +0200 (Tue, 28 Jul 2020) $
* $Author: torsten $
* Contents: thread pool functions
* Systems: all
*
\***********************************************************************/

#define __THREADPOOLS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  #include <sysinfoapi.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/lists.h"
#include "common/msgqueues.h"

#include "threadpools.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
const uint MAX_POOL_RUN_MSG_QUEUE = 8;

#ifndef NDEBUG
#endif /* NDEBUG */

/***************************** Datatypes *******************************/
typedef struct
{
  sem_t          lock;
  ThreadPool     *threadPool;
  ThreadPoolNode *threadPoolNode;
  const char     *namePrefix;
  int            niceLevel;
  sem_t          started;
//  void       (*entryFunction)(void*);
//  void       *argument;
} StartInfo;


/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : threadPoolTerminated
* Purpose: request terminate thread pool thread callback
* Input  : userData - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void threadPoolTerminated(void *userData)
{
  Thread *thread = (Thread*)userData;

  assert(thread != NULL);

  thread->terminatedFlag = TRUE;
}

/***********************************************************************\
* Name   : threadPoolStartCode
* Purpose: thread pool thread start code
* Input  : userData - start info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *threadPoolStartCode(void *userData)
{
  StartInfo      *startInfo = (StartInfo*)userData;
  ThreadPool     *threadPool;
  ThreadPoolNode *threadPoolNode;
  void           (*entryFunction)(void*);
  void           *argument;

  assert(startInfo != NULL);

  pthread_cleanup_push(threadPoolTerminated,&startInfo->threadPoolNode->thread);
  {
    // try to set thread name
    #ifdef HAVE_PTHREAD_SETNAME_NP
      if (startInfo->namePrefix != NULL)
      {
        (void)pthread_setname_np(pthread_self(),startInfo->namePrefix);
      }
    #endif /* HAVE_PTHREAD_SETNAME_NP */

    #ifndef NDEBUG
//      debugThreadStackTraceSetThreadName(pthread_self(),startInfo->namePrefix);
    #endif /* NDEBUG */

    #if   defined(PLATFORM_LINUX)
      if (nice(startInfo->niceLevel) == -1)
      {
        // ignore error
      }
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    // get local copy of start data
    threadPool     = startInfo->threadPool;
    threadPoolNode = startInfo->threadPoolNode;

    // signal thread started
    sem_post(&startInfo->started);

    pthread_mutex_lock(&threadPool->lock);
    {
      while (!threadPool->quitFlag)
      {
        // wait for function to exdecute or quit
//fprintf(stderr,"%s:%d: wait=%p %p\n",__FILE__,__LINE__,threadPoolNode,&threadPoolNode->trigger);
        while (   !threadPool->quitFlag
               && (pthread_cond_wait(&threadPoolNode->trigger,&threadPool->lock) != 0)
              )
        {
          // nothing to do
        }

//fprintf(stderr,"%s:%d: got %p qiot=%d\n",__FILE__,__LINE__,threadPoolNode,threadPool->quitFlag);
        if (!threadPool->quitFlag)
        {
          // get local copy of function
          entryFunction = threadPoolNode->entryFunction;
          argument      = threadPoolNode->argument;

          if (entryFunction != NULL)
          {
            // run function
            pthread_mutex_unlock(&threadPool->lock);
            {
              entryFunction(argument);
            }
            pthread_mutex_lock(&threadPool->lock);

            // re-add to idle list
            threadPoolNode->state = THREADPOOL_THREAD_STATE_IDLE;
            List_remove(&threadPool->running,threadPoolNode);
            List_append(&threadPool->idle,threadPoolNode);
            pthread_cond_broadcast(&threadPool->modified);
          }
        }
//fprintf(stderr,"%s:%d: threadPoolNode=%p _quitFlag=%d\n",__FILE__,__LINE__,threadPoolNode,threadPool->quitFlag);
      }
    }
    pthread_mutex_unlock(&threadPool->lock);
  }
  pthread_cleanup_pop(1);

  // quit
//fprintf(stderr,"%s:%d: quit %p\n",__FILE__,__LINE__,threadPoolNode);
  threadPoolNode->state = THREADPOOL_THREAD_STATE_QUIT;
  pthread_cond_broadcast(&threadPool->modified);

  return NULL;
}

/***********************************************************************\
* Name   : waitRunningThreads
* Purpose: wait for running threads
* Input  : threadPool - thread pool
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void waitRunningThreads(ThreadPool *threadPool)
{
  const ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);

  do
  {
    // find running thread pool thread started by this thread
    threadPoolNode = LIST_FIND(&threadPool->running,
                               threadPoolNode,
                               pthread_equal(threadPoolNode->usedBy,pthread_self()) != 0
                              );
//fprintf(stderr,"%s:%d: runningThreadPoolNode=%p\n",__FILE__,__LINE__,runningThreadPoolNode);

    // wait for thread termination
    if (threadPoolNode != NULL)
    {
      pthread_cond_wait(&threadPool->modified,&threadPool->lock);
    }
  }
  while (threadPoolNode != NULL);
}

/***********************************************************************\
* Name   : waitIdleThreads
* Purpose: wait for idle threads to quit
* Input  : threadPool - thread pool
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void waitIdleThreads(ThreadPool *threadPool)
{
  ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);

  do
  {
    threadPoolNode = LIST_FIND(&threadPool->idle,threadPoolNode,threadPoolNode->state != THREADPOOL_THREAD_STATE_QUIT);
    if (threadPoolNode != NULL)
    {
      pthread_cond_signal(&threadPoolNode->trigger);
      pthread_cond_wait(&threadPool->modified,&threadPool->lock);
    }
  }
  while (threadPoolNode != NULL);
}

/*---------------------------------------------------------------------*/

Errors ThreadPool_initAll(void)
{
  #ifndef NDEBUG
  #endif /* NDEBUG */

  return ERROR_NONE;
}

void ThreadPool_doneAll(void)
{
}

bool ThreadPool_init(ThreadPool *threadPool,
                     const char *namePrefix,
                     int        niceLevel,
                     uint       size
                    )
{
  StartInfo      startInfo;
  uint           i;
  ThreadPoolNode *threadPoolNode;
  pthread_attr_t threadAttributes;
  int            result;

  assert(threadPool != NULL);

  // init
  pthread_mutex_init(&threadPool->lock,NULL);
  pthread_cond_init(&threadPool->modified,NULL);
  List_init(&threadPool->idle);
  List_init(&threadPool->running);
  threadPool->quitFlag = FALSE;
  if (sem_init(&startInfo.started,0,0) != 0)
  {
    HALT_INTERNAL_ERROR("cannot initialise start trigger");
  }
  startInfo.threadPool = threadPool;
  startInfo.niceLevel  = niceLevel;

  for (i = 0; i < size; i++)
  {
    threadPoolNode = LIST_NEW_NODE(ThreadPoolNode);
    if (threadPoolNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    threadPoolNode->state = THREADPOOL_THREAD_STATE_IDLE;
    pthread_cond_init(&threadPoolNode->trigger,NULL);

    // init start info
    startInfo.threadPoolNode = threadPoolNode;
    startInfo.namePrefix     = namePrefix;

    // init thread attributes
    pthread_attr_init(&threadAttributes);
    #ifdef HAVE_PTHREAD_ATTR_SETNAME
      if (name != NULL)
      {
        pthread_attr_setname(&threadAttributes,(char*)name);
      }
    #endif /* HAVE_PTHREAD_ATTR_SETNAME */

    // start thread
    if (pthread_create(&threadPoolNode->thread,
                       &threadAttributes,
                       threadPoolStartCode,
                       &startInfo
                      ) != 0
       )
    {
      pthread_attr_destroy(&threadAttributes);

      // stop threads
      threadPool->quitFlag = TRUE;
      waitIdleThreads(threadPool);

      // free threads
      while (!List_isEmpty(&threadPool->idle))
      {
        threadPoolNode = (ThreadPoolNode*)List_removeFirst(&threadPool->idle);

        pthread_join(threadPoolNode->thread,NULL);
        pthread_cond_destroy(&threadPoolNode->trigger);
        LIST_DELETE_NODE(threadPoolNode);
      }
      assert(List_isEmpty(&threadPool->idle));

      // free resources
      List_done(&threadPool->running,CALLBACK_(NULL,NULL));
      List_done(&threadPool->idle,CALLBACK_(NULL,NULL));
      pthread_cond_destroy(&threadPool->modified);
      pthread_mutex_destroy(&threadPool->lock);

      return FALSE;
    }
    pthread_attr_destroy(&threadAttributes);

    // wait until thread started
    do
    {
      result = sem_wait(&startInfo.started);
    }
    while ((result != 0) && (errno == EINTR));
    if (result != 0)
    {
      HALT_INTERNAL_ERROR("wait for start lock failed");
    }

    List_append(&threadPool->idle,threadPoolNode);
  }
  assert(List_count(&threadPool->idle) == size);
  sem_destroy(&startInfo.started);

//  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(threadPool,ThreadPool);
//  #else /* not NDEBUG */
//    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,threadPool,ThreadPool);
//  #endif /* not NDEBUG */

  return TRUE;
}

void ThreadPool_done(ThreadPool *threadPool)
{
  ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);

  ThreadPool_joinAll(threadPool);

  DEBUG_REMOVE_RESOURCE_TRACE(threadPool,ThreadPool);

  // signal quit
  pthread_mutex_lock(&threadPool->lock);
  {
    // signal quit
    threadPool->quitFlag = TRUE;

    // wait running thrads
    waitRunningThreads(threadPool);
    assert(List_isEmpty(&threadPool->running));

    // quit idle thread
    waitIdleThreads(threadPool);
  }
  pthread_mutex_unlock(&threadPool->lock);

  // lock
  pthread_mutex_lock(&threadPool->lock);

  // free thread pool threads
  while (!List_isEmpty(&threadPool->idle))
  {
    threadPoolNode = (ThreadPoolNode*)List_removeFirst(&threadPool->idle);
    pthread_join(threadPoolNode->thread,NULL);
    pthread_cond_destroy(&threadPoolNode->trigger);
    LIST_DELETE_NODE(threadPoolNode);
  }
  assert(List_isEmpty(&threadPool->idle));

  // free resources
  List_done(&threadPool->running,CALLBACK_(NULL,NULL));
  List_done(&threadPool->idle,CALLBACK_(NULL,NULL));
  pthread_cond_destroy(&threadPool->modified);
  pthread_mutex_destroy(&threadPool->lock);
}

void ThreadPool_run(ThreadPool *threadPool,
                    const void *entryFunction,
                    void       *argument
                   )
{
  ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);
  assert(entryFunction != NULL);

  pthread_mutex_lock(&threadPool->lock);
  {
    // wait for idle thread
    while (List_isEmpty(&threadPool->idle))
    {
      pthread_cond_wait(&threadPool->modified,&threadPool->lock);
    }

    // remove from idle list
    threadPoolNode = (ThreadPoolNode*)List_removeFirst(&threadPool->idle);

    // add to running list
    threadPoolNode->state         = THREADPOOL_THREAD_STATE_RUNNING;
    threadPoolNode->usedBy        = pthread_self();
    threadPoolNode->entryFunction = entryFunction;
    threadPoolNode->argument      = argument;
    List_append(&threadPool->running,threadPoolNode);

    // run thread with funciton
    pthread_cond_signal(&threadPoolNode->trigger);
  }
  pthread_mutex_unlock(&threadPool->lock);
}

bool ThreadPool_joinAll(ThreadPool *threadPool)
{
  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);

  pthread_mutex_lock(&threadPool->lock);
  {
    waitRunningThreads(threadPool);
  }
  pthread_mutex_unlock(&threadPool->lock);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
