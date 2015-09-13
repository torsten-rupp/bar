/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

#define __THREADS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "threads.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define THREAD_LOCAL_STOAGE_HASHTABLE_SIZE      15
#define THREAD_LOCAL_STOAGE_HASHTABLE_INCREMENT 16

#ifndef NDEBUG
#endif /* NDEBUG */

/***************************** Datatypes *******************************/
typedef struct
{
  sem_t      lock;
  const char *name;
  int        niceLevel;
  void       (*entryFunction)(void*);
  void       *userData;
} ThreadStartInfo;

/***************************** Variables *******************************/
#ifndef NDEBUG
  typedef struct
  {
    sem_t lock;
    void  *(*startCode)(void*);
    void  *argument;
  } StackTraceThreadStartInfo;


  LOCAL struct sigaction debugSignalQuitPrevHandler;
  LOCAL pthread_once_t   debugStackTraceInitFlag    = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutex_t  debugStackTraceLock        = PTHREAD_MUTEX_INITIALIZER;
  LOCAL pthread_cond_t   debugStackTraceDone        = PTHREAD_COND_INITIALIZER;
  LOCAL bool             debugStackTraceRun = false;
  LOCAL pthread_t        debugStackTraceThreadIds[256];
  LOCAL uint             debugStackTraceThreadIdCount = 0;
  LOCAL uint             debugStackTraceThreadIndex;
#endif /* NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ----------------------------------------------------------------------

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugStackTraceAddThread
* Purpose: add thread for stack trace
* Input  : threadId - thread id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugStackTraceAddThread(pthread_t threadId)
{
  assert(debugStackTraceThreadIdCount < SIZE_OF_ARRAY(debugStackTraceThreadIds));

  debugStackTraceThreadIds[debugStackTraceThreadIdCount] = threadId;
  debugStackTraceThreadIdCount++;
}

/***********************************************************************\
* Name   : debugStackTraceRemoveThread
* Purpose: remove thread from stack trace
* Input  : threadId - thread id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugStackTraceRemoveThread(pthread_t threadId)
{
  uint i;

  assert(debugStackTraceThreadIdCount > 0);

  i = 0;
  while ((i < debugStackTraceThreadIdCount) && pthread_equal(debugStackTraceThreadIds[i],threadId) == 0)
  {
    i++;
  }
  if (i < debugStackTraceThreadIdCount)
  {
    memmove(&debugStackTraceThreadIds[i+0],
            &debugStackTraceThreadIds[i+1],
            (debugStackTraceThreadIdCount-i-1)*sizeof(pthread_t)
           );
    debugStackTraceThreadIdCount--;
  }
}

LOCAL void debugStackTraceWrapStartCode(void *userData)
{
  StackTraceThreadStartInfo *stackTraceThreadStartInfo = (StackTraceThreadStartInfo*)userData;
  void                      *(*startCode)(void*);
  void                      *argument;
  pthread_t                 threadId;

  assert(stackTraceThreadStartInfo != NULL);
  assert(stackTraceThreadStartInfo->startCode != NULL);

  // get copy of start data, pthread id
  startCode = stackTraceThreadStartInfo->startCode;
  argument  = stackTraceThreadStartInfo->argument;
  threadId  = pthread_self();

  // add thread
  pthread_mutex_lock(&debugStackTraceLock);
  {
    debugStackTraceAddThread(threadId);
  }
  pthread_mutex_unlock(&debugStackTraceLock);

  // signal thread start done
  sem_post(&stackTraceThreadStartInfo->lock);

  // run thread code
  startCode(argument);

  // remove thread
  pthread_mutex_lock(&debugStackTraceLock);
  {
    debugStackTraceRemoveThread(threadId);
  }
  pthread_mutex_unlock(&debugStackTraceLock);
}

/***********************************************************************\
* Name   : __wrap_pthread_create
* Purpose: wrapper function for pthread_create
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

int __wrap_pthread_create(pthread_t *thread,
                          const     pthread_attr_t *attr,
                          void      *(*startCode)(void*),
                          void      *argument
                         );
int __wrap_pthread_create(pthread_t *thread,
                                const     pthread_attr_t *attr,
                                void      *(*startCode)(void*),
                                void      *argument
                               )
{
  // real pthread_create()
  extern int __real_pthread_create(pthread_t *thread,
                                   const     pthread_attr_t *attr,
                                   void      *(*startCode)(void*),
                                   void      *argument
                                  ) __attribute((weak));
  StackTraceThreadStartInfo stackTraceThreadStartInfo;
  int                       result;

  assert(__real_pthread_create != NULL);
  assert(thread != NULL);
  assert(startCode != NULL);

  // init thread
  sem_init(&stackTraceThreadStartInfo.lock,0,0);
  stackTraceThreadStartInfo.startCode = startCode;
  stackTraceThreadStartInfo.argument  = argument;
  result = __real_pthread_create(thread,attr,debugStackTraceWrapStartCode,&stackTraceThreadStartInfo);
  if (result == 0)
  {
    // wait until thread started
    sem_wait(&stackTraceThreadStartInfo.lock);
  }

  // free resources
  sem_destroy(&stackTraceThreadStartInfo.lock);

  return result;
}

/***********************************************************************\
* Name   : debugSignalQuitHandler
* Purpose: signal-quit handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugSignalQuitHandler(int signalNumber, siginfo_t *siginfo, void *context)
{
  char     title[64];
  sigset_t mask;

  if (signalNumber == SIGQUIT)
  {
    pthread_mutex_lock(&debugStackTraceLock);
    {
      if (debugStackTraceRun)
      {
        snprintf(title,sizeof(title),"Thread stack trace %02d/%02d: 0x%x",debugStackTraceThreadIndex+1,debugStackTraceThreadIdCount,pthread_self());
        #ifndef NDEBUG
          debugDumpCurrentStackTrace(stderr,title,0);
        #else
          fprintf(stderr,"  not available");
        #endif
        fprintf(stderr,"\n");

        pthread_cond_signal(&debugStackTraceDone);
//fprintf(stderr,"%s, %d: sent back %x\n",__FILE__,__LINE__,debugStackTraceThreadId);
      }
      else
      {
        // mark running
        debugStackTraceRun = true;

        // print stack trace of main thread
        snprintf(title,sizeof(title),"Thread stack trace %02d/%02d: 0x%x",0,debugStackTraceThreadIdCount,pthread_self());
        #ifndef NDEBUG
          debugDumpCurrentStackTrace(stderr,title,0);
        #else
          fprintf(stderr,"  not available");
        #endif
        fprintf(stderr,"\n");

        // trigger print stack trace in all threads
        for (debugStackTraceThreadIndex = 0; debugStackTraceThreadIndex < debugStackTraceThreadIdCount; debugStackTraceThreadIndex++)
        {
          if (pthread_equal(debugStackTraceThreadIds[debugStackTraceThreadIndex],pthread_self()) == 0)
          {
           // trigger print stack trace in thread
//fprintf(stderr,"%s, %d: send %x\n",__FILE__,__LINE__,debugStackTraceThreads[debugStackTraceThreadIndex]);
            pthread_kill(debugStackTraceThreadIds[debugStackTraceThreadIndex],SIGQUIT);

           // wait for done signal
           pthread_cond_wait(&debugStackTraceDone,&debugStackTraceLock);
          }
        }

        // mark not running
        debugStackTraceRun = false;
      }
    }
    pthread_mutex_unlock(&debugStackTraceLock);
  }

  if (debugSignalQuitPrevHandler.sa_sigaction != NULL)
  {
    debugSignalQuitPrevHandler.sa_sigaction(signalNumber,siginfo,context);
  }
}

/***********************************************************************\
* Name   : debugStackTraceInit
* Purpose: init debug stack trace
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugStackTraceInit(void)
{
  struct sigaction sa;

  // install signal handler for Ctrl-\ (SIGQUIT) for printing debug information
  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugSignalQuitHandler;
  sigaction(SIGQUIT,&sa,&debugSignalQuitPrevHandler);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : threadStart
* Purpose: thread start function
* Input  : startInfo - start info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void threadStart(ThreadStartInfo *startInfo)
{
  int  niceLevel;
  void (*entryFunction)(void*);
  void *userData;

  assert(startInfo != NULL);

  // get local copy of start data
  niceLevel     = startInfo->niceLevel;
  entryFunction = startInfo->entryFunction;
  userData      = startInfo->userData;

  // signal thread started
  sem_post(&startInfo->lock);

  #if   defined(PLATFORM_LINUX)
    if (nice(niceLevel) == -1)
    {
      // ignore error
    }
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // run thread code
  assert(entryFunction != NULL);
  entryFunction(userData);
}

/*---------------------------------------------------------------------*/

uint Thread_getNumberOfCores(void)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    SYSTEM_INFO info;
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_SYSCONF) && defined(HAVE__SC_NPROCESSORS_CONF)
      return (uint)sysconf(_SC_NPROCESSORS_CONF);
    #else
      return 1;
    #endif
  #elif defined(PLATFORM_WINDOWS)
    GetSystemInfo(&info);
    return (uint)info.dwNumberOfProcessors;
  #endif /* PLATFORM_... */
}

bool Thread_init(Thread     *thread,
                 const char *name,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *userData
                )
{
  ThreadStartInfo startInfo;
  pthread_attr_t  threadAttributes;

  assert(thread != NULL);
  assert(name != NULL);
  assert(entryFunction != NULL);

  #ifndef NDEBUG
    pthread_once(&debugStackTraceInitFlag,debugStackTraceInit);
  #endif /* NDEBUG */

  // init thread info
  sem_init(&startInfo.lock,0,0);
  startInfo.name          = name;
  startInfo.niceLevel     = niceLevel;
  startInfo.entryFunction = entryFunction;
  startInfo.userData      = userData;

  // init thread attributes
  pthread_attr_init(&threadAttributes);
  #ifdef HAVE_PTHREAD_ATTR_SETNAME
    if (name != NULL)
    {
      pthread_attr_setname(&threadAttributes,(char*)name);
    }
  #else /* HAVE_PTHREAD_ATTR_SETNAME */
    UNUSED_VARIABLE(name);
  #endif /* HAVE_PTHREAD_ATTR_SETNAME */

  // start thread
  if (pthread_create(&thread->handle,
                     &threadAttributes,
                     (void*(*)(void*))threadStart,
                     &startInfo                   ) != 0)
  {
    sem_destroy(&startInfo.lock);
    return FALSE;
  }
  pthread_attr_destroy(&threadAttributes);

  // wait until thread started
  sem_wait(&startInfo.lock);

  // free resources
  sem_destroy(&startInfo.lock);

  return TRUE;
}

void Thread_done(Thread *thread)
{
  assert(thread != NULL);

  UNUSED_VARIABLE(thread);
}

bool Thread_join(Thread *thread)
{
  assert(thread != NULL);

  return pthread_join(thread->handle,NULL) == 0;
}

void Thread_delay(uint time)
{
  #ifdef HAVE_NANOSLEEP
    struct timespec ts;
  #endif /* HAVE_NANOSLEEP */

  #if defined(PLATFORM_LINUX)
    #if   defined(HAVE_NANOSLEEP)
      ts.tv_sec  = (ulong)(time/1000LL);
      ts.tv_nsec = (ulong)((time%1000LL)*1000000);
      while (   (nanosleep(&ts,&ts) == -1)
             && (errno == EINTR)
            )
     {
        // nothing to do
      }
    #else
      sleep(time/1000LL);
    #endif
  #elif defined(PLATFORM_WINDOWS)
    Sleep(time);
  #endif
}

void Thread_initLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageAllocFunction threadLocalStorageAllocFunction, void *threadLocalStorageAllocUserData)
{
  assert(threadLocalStorage != NULL);

  threadLocalStorage->allocFunction = threadLocalStorageAllocFunction;
  threadLocalStorage->allocUserData = threadLocalStorageAllocUserData;
  pthread_mutex_init(&threadLocalStorage->lock,NULL);
  List_init(&threadLocalStorage->instanceList);
}

void Thread_doneLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageFreeFunction threadLocalStorageFreeFunction, void *threadLocalStorageFreeUserData)
{
  ThreadLocalStorageInstanceNode *threadLocalStorageInstanceNode;

  assert(threadLocalStorage != NULL);

  pthread_mutex_lock(&threadLocalStorage->lock);
  {
    while (!List_isEmpty(&threadLocalStorage->instanceList))
    {
      threadLocalStorageInstanceNode = (ThreadLocalStorageInstanceNode*)List_getFirst(&threadLocalStorage->instanceList);
      if (threadLocalStorageFreeFunction != NULL) threadLocalStorageFreeFunction(threadLocalStorageInstanceNode->p,threadLocalStorageFreeUserData);
      LIST_DELETE_NODE(threadLocalStorageInstanceNode);
    }
  }
  pthread_mutex_unlock(&threadLocalStorage->lock);
}

void *Thread_getLocalVariable(ThreadLocalStorage *threadLocalStorage)
{
  ThreadId                       currentThreadId;
  void                           *p;
  ThreadLocalStorageInstanceNode *threadLocalStorageInstanceNode;

  assert(threadLocalStorage != NULL);

  pthread_mutex_lock(&threadLocalStorage->lock);
  {
    // find instance
    threadLocalStorageInstanceNode = (ThreadLocalStorageInstanceNode*)List_first(&threadLocalStorage->instanceList);
    currentThreadId = Thread_getCurrentId();
    while (   (threadLocalStorageInstanceNode != NULL)
           && (pthread_equal(threadLocalStorageInstanceNode->threadId,currentThreadId) == 0)
          )
    {
      threadLocalStorageInstanceNode = threadLocalStorageInstanceNode->next;
    }

    if (threadLocalStorageInstanceNode == NULL)
    {
      // allocate new instance
      threadLocalStorageInstanceNode = LIST_NEW_NODE(ThreadLocalStorageInstanceNode);
      if (threadLocalStorageInstanceNode == NULL)
      {
        pthread_mutex_unlock(&threadLocalStorage->lock);
        return NULL;
      }
      threadLocalStorageInstanceNode->threadId = currentThreadId;

      threadLocalStorageInstanceNode->p = threadLocalStorage->allocFunction(threadLocalStorage->allocUserData);
      if (threadLocalStorageInstanceNode->p == NULL)
      {
        pthread_mutex_unlock(&threadLocalStorage->lock);
        LIST_DELETE_NODE(threadLocalStorageInstanceNode);
        return NULL;
      }

      // add instance
      List_append(&threadLocalStorage->instanceList,threadLocalStorageInstanceNode);
    }

    p = threadLocalStorageInstanceNode->p;
  }
  pthread_mutex_unlock(&threadLocalStorage->lock);

  return p;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
