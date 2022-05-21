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
#include <sys/time.h>
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
  char           name[64];
  sem_t          started;
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
  struct timeval  timeval;
  struct timespec timespec;
  void           (*entryFunction)(void*);
  void           *argument;

  assert(startInfo != NULL);
  assert(startInfo->threadPool != NULL);
  assert(startInfo->threadPoolNode != NULL);

  pthread_cleanup_push(threadPoolTerminated,&startInfo->threadPoolNode->thread);
  {
    // try to set thread name
    #ifdef HAVE_PTHREAD_SETNAME_NP
      if (!stringIsEmpty(startInfo->name))
      {
        (void)pthread_setname_np(pthread_self(),startInfo->name);
      }
    #endif /* HAVE_PTHREAD_SETNAME_NP */

    #if   defined(PLATFORM_LINUX)
      if (nice(startInfo->threadPool->niceLevel) == -1)
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
        // wait for function to execute
//fprintf(stderr,"%s:%d: wait=%p %p\n",__FILE__,__LINE__,threadPoolNode,&threadPoolNode->trigger);
// TODO: work-around for missed signal; replace by semaphore per thread
        gettimeofday(&timeval,NULL);
        timespec.tv_sec  = timeval.tv_sec+1;
        timespec.tv_nsec = timeval.tv_usec*1000;
        (void)pthread_cond_timedwait(&threadPoolNode->trigger,&threadPool->lock,&timespec);

//fprintf(stderr,"%s:%d: got %p qiot=%d\n",__FILE__,__LINE__,threadPoolNode,threadPool->quitFlag);
        if (!threadPool->quitFlag && (threadPoolNode->state == THREADPOOL_THREAD_STATE_RUNNING))
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
      }
    }
    pthread_mutex_unlock(&threadPool->lock);
  }
  pthread_cleanup_pop(1);

  // quit
  threadPoolNode->state = THREADPOOL_THREAD_STATE_QUIT;
  pthread_cond_broadcast(&threadPool->modified);

  return NULL;
}

/***********************************************************************\
* Name   : newThread
* Purpose: add new thread to thread pool
* Input  : threadPool - thread pool
*          niceLevel  - nice level
* Output : -
* Return : thread pool node or nULL
* Notes  : -
\***********************************************************************/

LOCAL ThreadPoolNode *newThread(ThreadPool *threadPool)
{
  ThreadPoolNode *threadPoolNode;
  StartInfo      startInfo;
  pthread_attr_t threadAttributes;
  int            result;

  assert(threadPool != NULL);

  // new thread pool node
  threadPoolNode = LIST_NEW_NODE(ThreadPoolNode);
  if (threadPoolNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  threadPoolNode->state = THREADPOOL_THREAD_STATE_IDLE;
  pthread_cond_init(&threadPoolNode->trigger,NULL);

  // init start info
  if (sem_init(&startInfo.started,0,0) != 0)
  {
    HALT_INTERNAL_ERROR("cannot initialise start trigger");
  }
  startInfo.threadPool     = threadPool;
  startInfo.threadPoolNode = threadPoolNode;
  stringFormat(startInfo.name,sizeof(startInfo.name),"%s%d",threadPool->namePrefix,threadPool->size);

  // init thread attributes
  pthread_attr_init(&threadAttributes);
  #ifdef HAVE_PTHREAD_ATTR_SETNAME
    if (name != NULL)
    {
      pthread_attr_setname(&threadAttributes,startInfo.name);
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
    sem_destroy(&startInfo.started);
    pthread_attr_destroy(&threadAttributes);
    LIST_DELETE_NODE(threadPoolNode);

    return NULL;
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

  // add to idle list
  List_append(&threadPool->idle,threadPoolNode);
  threadPool->size++;

  // free resources
  sem_destroy(&startInfo.started);

  return threadPoolNode;
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
  ThreadPoolNode *threadPoolNode;

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
      pthread_cond_broadcast(&threadPoolNode->trigger);
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
    // find not terminated thread pool thread
    threadPoolNode = LIST_FIND(&threadPool->idle,threadPoolNode,threadPoolNode->state != THREADPOOL_THREAD_STATE_QUIT);

    // wait for thread termination
    if (threadPoolNode != NULL)
    {
      pthread_cond_broadcast(&threadPoolNode->trigger);
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
                     uint       size,
                     uint       maxSize
                    )
{
  uint           i;
  ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);

  // init
  stringSet(threadPool->namePrefix,sizeof(threadPool->namePrefix),namePrefix);
  threadPool->niceLevel = niceLevel;
  threadPool->maxSize   = maxSize;
  pthread_mutex_init(&threadPool->lock,NULL);
  pthread_cond_init(&threadPool->modified,NULL);
  List_init(&threadPool->idle,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  List_init(&threadPool->running,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  threadPool->size      = 0;
  threadPool->quitFlag  = FALSE;

  for (i = 0; i < size; i++)
  {
    threadPoolNode = newThread(threadPool);
    if (threadPoolNode == NULL)
    {
      // stop threads
      threadPool->quitFlag = TRUE;
      pthread_mutex_lock(&threadPool->lock);
      {
        waitIdleThreads(threadPool);
      }
      pthread_mutex_unlock(&threadPool->lock);

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
      List_done(&threadPool->running);
      List_done(&threadPool->idle);
      pthread_cond_destroy(&threadPool->modified);
      pthread_mutex_destroy(&threadPool->lock);

      return FALSE;
    }
  }
  assert(List_count(&threadPool->idle) == size);

  DEBUG_ADD_RESOURCE_TRACE(threadPool,ThreadPool);

  return TRUE;
}

void ThreadPool_done(ThreadPool *threadPool)
{
  ThreadPoolNode *threadPoolNode;

  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);

  ThreadPool_joinAll(threadPool);

  DEBUG_REMOVE_RESOURCE_TRACE(threadPool,ThreadPool);

  // lock
  pthread_mutex_lock(&threadPool->lock);

  // signal quit
  threadPool->quitFlag = TRUE;

  // wait running thrads
  waitRunningThreads(threadPool);
  assert(List_isEmpty(&threadPool->running));

  // quit idle thread
  waitIdleThreads(threadPool);

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
  List_done(&threadPool->running);
  List_done(&threadPool->idle);
  pthread_cond_destroy(&threadPool->modified);
  pthread_mutex_destroy(&threadPool->lock);
}

void ThreadPool_initSet(ThreadPoolSet *threadPoolSet,
                        ThreadPool    *threadPool
                       )
{
  assert(threadPoolSet != NULL);

  threadPoolSet->threadPool = threadPool;

  Array_init(&threadPoolSet->threadPoolNodes,
             sizeof(ThreadPoolNode*),
             64,
             CALLBACK_(NULL,NULL),
             CALLBACK_(NULL,NULL)
            );

  DEBUG_ADD_RESOURCE_TRACE(threadPoolSet,ThreadPoolSet);
}

void ThreadPool_doneSet(ThreadPoolSet *threadPoolSet)
{
  assert(threadPoolSet != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(threadPoolSet,ThreadPoolSet);

  Array_done(&threadPoolSet->threadPoolNodes);
}

void ThreadPool_setAdd(ThreadPoolSet *threadPoolSet, ThreadPoolNode *threadPoolNode)
{
  assert(threadPoolSet != NULL);
  assert(threadPoolNode != NULL);

  Array_append(&threadPoolSet->threadPoolNodes,&threadPoolNode);
}

ThreadPoolNode *ThreadPool_run(ThreadPool *threadPool,
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
    if (   List_isEmpty(&threadPool->idle)
        && (threadPool->size < threadPool->maxSize)
       )
    {
      // create new thread
      threadPoolNode = newThread(threadPool);
    }
    else
    {
      // wait for idle thread
      while (List_isEmpty(&threadPool->idle))
      {
        pthread_cond_wait(&threadPool->modified,&threadPool->lock);
      }
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

  return threadPoolNode;
}

bool ThreadPool_join(ThreadPool *threadPool, ThreadPoolNode *threadPoolNode)
{
  assert(threadPool != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPool);
  assert(threadPoolNode != NULL);

  if (pthread_equal(threadPoolNode->usedBy,pthread_self()) != 0)
  {
    pthread_mutex_lock(&threadPool->lock);
    {
      while (threadPoolNode->state == THREADPOOL_THREAD_STATE_RUNNING)
      {
        pthread_cond_wait(&threadPool->modified,&threadPool->lock);
      }
    }
    pthread_mutex_unlock(&threadPool->lock);
  }

  return TRUE;
}

bool ThreadPool_joinSet(ThreadPoolSet *threadPoolSet)
{
  ArrayIterator  arrayIterator;
  ThreadPoolNode *threadPoolNode;

  assert(threadPoolSet != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(threadPoolSet);

  ARRAY_ITERATE(&threadPoolSet->threadPoolNodes,arrayIterator,threadPoolNode)
  {
    ThreadPool_join(threadPoolSet->threadPool,threadPoolNode);
  }

  return TRUE;
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
