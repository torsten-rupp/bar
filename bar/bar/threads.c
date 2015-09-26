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
#include <time.h>
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
  void       *argument;
} ThreadStartInfo;

/***************************** Variables *******************************/
#ifndef NDEBUG
  typedef struct
  {
    sem_t lock;
    void  *(*startCode)(void*);
    void  *argument;
  } StackTraceThreadStartInfo;

  typedef struct
  {
    pthread_t  id;
    const char *name;
  } StackTraceThreadInfo;


  LOCAL struct sigaction     debugSignalSegVPrevHandler;
  LOCAL struct sigaction     debugSignalQuitPrevHandler;

  LOCAL pthread_once_t       debugThreadInitFlag          = PTHREAD_ONCE_INIT;

  LOCAL pthread_mutex_t      debugSignalSegVLock          = PTHREAD_MUTEX_INITIALIZER;

  LOCAL pthread_mutex_t      debugStackTraceThreadLock    = PTHREAD_MUTEX_INITIALIZER;
  LOCAL StackTraceThreadInfo debugStackTraceThreads[256];
  LOCAL uint                 debugStackTraceThreadCount = 0;

  LOCAL pthread_mutex_t      debugStackTraceLock          = PTHREAD_MUTEX_INITIALIZER;
  LOCAL pthread_cond_t       debugStackTraceDone          = PTHREAD_COND_INITIALIZER;
  LOCAL bool                 debugStackTraceRun           = FALSE;
  LOCAL uint                 debugStackTraceThreadIndex;
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
  assert(debugStackTraceThreadCount < SIZE_OF_ARRAY(debugStackTraceThreads));

  pthread_mutex_lock(&debugStackTraceThreadLock);
  {
    debugStackTraceThreads[debugStackTraceThreadCount].id   = threadId;
    debugStackTraceThreads[debugStackTraceThreadCount].name = NULL;
    debugStackTraceThreadCount++;
  }
  pthread_mutex_unlock(&debugStackTraceThreadLock);
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

  assert(debugStackTraceThreadCount > 0);

  pthread_mutex_lock(&debugStackTraceThreadLock);
  {
    i = 0;
    while ((i < debugStackTraceThreadCount) && pthread_equal(debugStackTraceThreads[i].id,threadId) == 0)
    {
      i++;
    }
    if (i < debugStackTraceThreadCount)
    {
      memmove(&debugStackTraceThreads[i+0],
              &debugStackTraceThreads[i+1],
              (debugStackTraceThreadCount-i-1)*sizeof(StackTraceThreadInfo)
             );
      debugStackTraceThreadCount--;
    }
  }
  pthread_mutex_unlock(&debugStackTraceThreadLock);
}

/***********************************************************************\
* Name   : debugStackTraceSetThreadName
* Purpose: set thread name
* Input  : threadId - thread id
*          name     - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugStackTraceSetThreadName(pthread_t threadId, const char *name)
{
  uint i;

  assert(debugStackTraceThreadCount > 0);

  pthread_mutex_lock(&debugStackTraceThreadLock);
  {
    i = 0;
    while ((i < debugStackTraceThreadCount) && pthread_equal(debugStackTraceThreads[i].id,threadId) == 0)
    {
      i++;
    }
    if (i < debugStackTraceThreadCount)
    {
      debugStackTraceThreads[i].name = name;
    }
  }
  pthread_mutex_unlock(&debugStackTraceThreadLock);
}

/***********************************************************************\
* Name   : debugStackTraceGetThreadName
* Purpose: get thread name
* Input  : threadId - thread id
* Output : -
* Return : thread name or NULL
* Notes  : -
\***********************************************************************/

LOCAL const char *debugStackTraceGetThreadName(pthread_t threadId)
{
  uint       i;
  const char *name;

  assert(debugStackTraceThreadCount > 0);

  i = 0;
  while ((i < debugStackTraceThreadCount) && pthread_equal(debugStackTraceThreads[i].id,threadId) == 0)
  {
    i++;
  }
  if (i < debugStackTraceThreadCount)
  {
    name = debugStackTraceThreads[i].name;
  }
  else
  {
    name = NULL;
  }

  return name;
}

/***********************************************************************\
* Name   : debugStackTraceWrapStartCode
* Purpose: debug stack trace start code wrapper
* Input  : userData - stacktrace thread start info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *debugStackTraceWrapStartCode(void *userData)
{
  StackTraceThreadStartInfo *stackTraceThreadStartInfo = (StackTraceThreadStartInfo*)userData;
  void                      *(*startCode)(void*);
  void                      *argument;
  pthread_t                 threadId;
  void                      *result;

  assert(stackTraceThreadStartInfo != NULL);
  assert(stackTraceThreadStartInfo->startCode != NULL);

  // get copy of start data, pthread id
  startCode = stackTraceThreadStartInfo->startCode;
  argument  = stackTraceThreadStartInfo->argument;
  threadId  = pthread_self();

  // add thread
  debugStackTraceAddThread(threadId);

  // signal thread start done
  sem_post(&stackTraceThreadStartInfo->lock);

  // run thread code
  result = startCode(argument);

  // remove thread
  debugStackTraceRemoveThread(threadId);

  return result;
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
* Name   : debugDumpAllStackTraces
* Purpose: dump stacktraces of all threads
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugDumpAllStackTraces(void)
{
  const char      *name;
  struct timespec timeout;

  pthread_mutex_lock(&debugStackTraceLock);
  {
    if (debugStackTraceRun)
    {
      pthread_mutex_lock(&debugConsoleLock);
      {
        name = debugStackTraceGetThreadName(debugStackTraceThreads[debugStackTraceThreadIndex].id);
        fprintf(stderr,
              "Thread stack trace %02d/%02d: '%s' (0x%lx)\n",
              debugStackTraceThreadIndex+1,
              debugStackTraceThreadCount,
              (name != NULL) ? name : "<none>",
              debugStackTraceThreads[debugStackTraceThreadIndex].id
             );
        #ifndef NDEBUG
          debugDumpCurrentStackTrace(stderr,0,1);
        #else
          fprintf(stderr,"  not available");
        #endif

        pthread_cond_signal(&debugStackTraceDone);
  //fprintf(stderr,"%s, %d: singal done %p\n",__FILE__,__LINE__,pthread_self());
      }
      pthread_mutex_unlock(&debugConsoleLock);
    }
    else
    {
      // mark running
      debugStackTraceRun = TRUE;

      pthread_mutex_lock(&debugStackTraceThreadLock);
      {
        // print stack trace of all threads
        for (debugStackTraceThreadIndex = 0; debugStackTraceThreadIndex < debugStackTraceThreadCount; debugStackTraceThreadIndex++)
        {
          if (pthread_equal(debugStackTraceThreads[debugStackTraceThreadIndex].id,pthread_self()) == 0)
          {
            // trigger print stack trace in thread
            #ifndef NDEBUG
//fprintf(stderr,"%s, %d: send %s %p\n",__FILE__,__LINE__,debugStackTraceGetThreadName(debugStackTraceThreads[debugStackTraceThreadIndex].id),debugStackTraceThreads[debugStackTraceThreadIndex].id);
              if (pthread_kill(debugStackTraceThreads[debugStackTraceThreadIndex].id,SIGQUIT) == 0)
              {
                // wait for done signal
//fprintf(stderr,"%s, %d: wait %p: %s %p \n",__FILE__,__LINE__,pthread_self(),debugStackTraceGetThreadName(debugStackTraceThreads[debugStackTraceThreadIndex].id),debugStackTraceThreads[debugStackTraceThreadIndex].id);
                clock_gettime(CLOCK_REALTIME,&timeout);
                timeout.tv_sec += 2;
                if (pthread_cond_timedwait(&debugStackTraceDone,&debugStackTraceLock,&timeout) != 0)
                {
                  fprintf(stderr,"  not availble (terminate fail)\n");
                }
              }
              else
              {
                fprintf(stderr,"  not availble (trigger fail)\n");
              }
            #else /* NDEBUG */
              fprintf(stderr,"  not available");
            #endif /* not NDEBUG */
          }
          else
          {
            // print stack trace of this thread (probably crashed thread)
            pthread_mutex_lock(&debugConsoleLock);
            {
              name = debugStackTraceGetThreadName(debugStackTraceThreads[debugStackTraceThreadIndex].id);
              fprintf(stderr,
                    "Thread stack trace %02d/%02d: '%s' (0x%lx) *** CRASHED ***\n",
                    debugStackTraceThreadIndex+1,
                    debugStackTraceThreadCount,
                    (name != NULL) ? name : "<none>",
                    debugStackTraceThreads[debugStackTraceThreadIndex].id
                   );
              #ifndef NDEBUG
                debugDumpCurrentStackTrace(stderr,0,1);
              #else /* NDEBUG */
                fprintf(stderr,"  not available");
              #endif /* not NDEBUG */
            }
            pthread_mutex_unlock(&debugConsoleLock);
          }
          fprintf(stderr,"\n");
        }
      }
      pthread_mutex_unlock(&debugStackTraceThreadLock);

      // mark not running
      debugStackTraceRun = FALSE;
    }
  }
  pthread_mutex_unlock(&debugStackTraceLock);
}

/***********************************************************************\
* Name   : debugSignalSegVHandler
* Purpose: signal-segmantation vault handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugSignalSegVHandler(int signalNumber, siginfo_t *siginfo, void *context)
{

  if (signalNumber == SIGSEGV)
  {
    pthread_mutex_lock(&debugSignalSegVLock);
    {
      debugDumpAllStackTraces();
    }
    pthread_mutex_unlock(&debugSignalSegVLock);
  }

  if (debugSignalSegVPrevHandler.sa_sigaction != NULL)
  {
    debugSignalSegVPrevHandler.sa_sigaction(signalNumber,siginfo,context);
  }
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
  if (signalNumber == SIGQUIT)
  {
    debugDumpAllStackTraces();
  }

  if (debugSignalQuitPrevHandler.sa_sigaction != NULL)
  {
    debugSignalQuitPrevHandler.sa_sigaction(signalNumber,siginfo,context);
  }
}

/***********************************************************************\
* Name   : debugThreadInit
* Purpose: init debug stack trace
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadInit(void)
{
  struct sigaction sa;

  // add main thread
  debugStackTraceAddThread(pthread_self());

  // install signal handler for segmentation vault (SIGSEGV) for printing stack traces
  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugSignalSegVHandler;
  sigaction(SIGSEGV,&sa,&debugSignalSegVPrevHandler);

  // install signal handler for Ctrl-\ (SIGQUIT) for printing stack traces
  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugSignalQuitHandler;
  sigaction(SIGQUIT,&sa,&debugSignalQuitPrevHandler);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : threadStartCode
* Purpose: thread start code
* Input  : startInfo - start info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *threadStartCode(void *userData)
{
  ThreadStartInfo *startInfo = (ThreadStartInfo*)userData;
  void            (*entryFunction)(void*);
  void            *argument;

  assert(startInfo != NULL);

  #ifndef NDEBUG
    debugStackTraceSetThreadName(pthread_self(),startInfo->name);
  #endif /* NDEBUG */

  #if   defined(PLATFORM_LINUX)
    if (nice(startInfo->niceLevel) == -1)
    {
      // ignore error
    }
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // get local copy of start data
  entryFunction = startInfo->entryFunction;
  argument      = startInfo->argument;

  // signal thread started
  sem_post(&startInfo->lock);

  // run thread code
  assert(entryFunction != NULL);
  entryFunction(argument);

  return NULL;
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
                 void       *argument
                )
{
  ThreadStartInfo startInfo;
  pthread_attr_t  threadAttributes;

  assert(thread != NULL);
  assert(name != NULL);
  assert(entryFunction != NULL);

  #ifndef NDEBUG
    pthread_once(&debugThreadInitFlag,debugThreadInit);
  #endif /* NDEBUG */

  // init thread info
  sem_init(&startInfo.lock,0,0);
  startInfo.name          = name;
  startInfo.niceLevel     = niceLevel;
  startInfo.entryFunction = entryFunction;
  startInfo.argument      = argument;

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
                     threadStartCode,
                     &startInfo
                    ) != 0
     )
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
