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
#define THREAD_LOCAL_STORAGE_HASHTABLE_SIZE      15
#define THREAD_LOCAL_STORAGE_HASHTABLE_INCREMENT 16

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
    ThreadId   id;
    const char *name;
  } StackTraceThreadInfo;


  LOCAL struct sigaction     debugThreadSignalSegVPrevHandler;
  LOCAL struct sigaction     debugThreadSignalAbortPrevHandler;
  LOCAL struct sigaction     debugThreadSignalQuitPrevHandler;

  LOCAL pthread_once_t       debugThreadInitFlag                = PTHREAD_ONCE_INIT;

  LOCAL pthread_mutex_t      debugThreadSignalLock              = PTHREAD_MUTEX_INITIALIZER;

  LOCAL pthread_mutex_t      debugThreadStackTraceThreadLock    = PTHREAD_MUTEX_INITIALIZER;
  LOCAL StackTraceThreadInfo debugThreadStackTraceThreads[256];
  LOCAL uint                 debugThreadStackTraceThreadCount   = 0;

  LOCAL pthread_mutex_t      debugThreadStackTraceLock          = PTHREAD_MUTEX_INITIALIZER;
  LOCAL pthread_cond_t       debugThreadStackTraceDone          = PTHREAD_COND_INITIALIZER;
  LOCAL bool                 debugThreadStackTraceRun           = FALSE;
  LOCAL uint                 debugThreadStackTraceThreadIndex;
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
* Name   : debugThreadStackTraceAddThread
* Purpose: add thread for stack trace
* Input  : threadId - thread id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadStackTraceAddThread(ThreadId threadId)
{
  pthread_mutex_lock(&debugThreadStackTraceThreadLock);
  {
    assert(debugThreadStackTraceThreadCount < SIZE_OF_ARRAY(debugThreadStackTraceThreads));

    debugThreadStackTraceThreads[debugThreadStackTraceThreadCount].id   = threadId;
    debugThreadStackTraceThreads[debugThreadStackTraceThreadCount].name = NULL;
    debugThreadStackTraceThreadCount++;
  }
  pthread_mutex_unlock(&debugThreadStackTraceThreadLock);
}

/***********************************************************************\
* Name   : debugThreadStackTraceRemoveThread
* Purpose: remove thread from stack trace
* Input  : threadId - thread id
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadStackTraceRemoveThread(ThreadId threadId)
{
  uint i;

  pthread_mutex_lock(&debugThreadStackTraceThreadLock);
  {
    assert(debugThreadStackTraceThreadCount > 0);

    i = 0;
    while ((i < debugThreadStackTraceThreadCount) && pthread_equal(debugThreadStackTraceThreads[i].id,threadId) == 0)
    {
      i++;
    }
    if (i < debugThreadStackTraceThreadCount)
    {
      memmove(&debugThreadStackTraceThreads[i+0],
              &debugThreadStackTraceThreads[i+1],
              (debugThreadStackTraceThreadCount-i-1)*sizeof(StackTraceThreadInfo)
             );
      debugThreadStackTraceThreadCount--;
    }
  }
  pthread_mutex_unlock(&debugThreadStackTraceThreadLock);
}

/***********************************************************************\
* Name   : debugThreadStackTraceSetThreadName
* Purpose: set thread name
* Input  : threadId - thread id
*          name     - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadStackTraceSetThreadName(ThreadId threadId, const char *name)
{
  uint i;

  pthread_mutex_lock(&debugThreadStackTraceThreadLock);
  {
    assert(debugThreadStackTraceThreadCount > 0);

    i = 0;
    while ((i < debugThreadStackTraceThreadCount) && pthread_equal(debugThreadStackTraceThreads[i].id,threadId) == 0)
    {
      i++;
    }
    if (i < debugThreadStackTraceThreadCount)
    {
      debugThreadStackTraceThreads[i].name = name;
    }
  }
  pthread_mutex_unlock(&debugThreadStackTraceThreadLock);
}

/***********************************************************************\
* Name   : debugStackTraceGetThreadName
* Purpose: get thread name
* Input  : threadId - thread id
* Output : -
* Return : thread name or NULL
* Notes  : -
\***********************************************************************/

LOCAL const char *debugThreadStackTraceGetThreadName(ThreadId threadId)
{
  uint       i;
  const char *name;

  assert(debugThreadStackTraceThreadCount > 0);

  i = 0;
  while ((i < debugThreadStackTraceThreadCount) && pthread_equal(debugThreadStackTraceThreads[i].id,threadId) == 0)
  {
    i++;
  }
  if (i < debugThreadStackTraceThreadCount)
  {
    name = debugThreadStackTraceThreads[i].name;
  }
  else
  {
    name = NULL;
  }

  return name;
}

/***********************************************************************\
* Name   : debugThreadStackTraceWrapStartCode
* Purpose: debug stack trace start code wrapper
* Input  : userData - stacktrace thread start info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *debugThreadStackTraceWrapStartCode(void *userData)
{
  StackTraceThreadStartInfo *stackTraceThreadStartInfo = (StackTraceThreadStartInfo*)userData;
  void                      *(*startCode)(void*);
  void                      *argument;
  ThreadId                  threadId;
  void                      *result;

  assert(stackTraceThreadStartInfo != NULL);
  assert(stackTraceThreadStartInfo->startCode != NULL);

  // get copy of start data, pthread id
  startCode = stackTraceThreadStartInfo->startCode;
  argument  = stackTraceThreadStartInfo->argument;
  threadId  = Thread_getCurrentId();

  // add thread
  debugThreadStackTraceAddThread(threadId);

  // signal thread start done
  sem_post(&stackTraceThreadStartInfo->lock);

  // run thread code
  result = startCode(argument);

  // remove thread
  debugThreadStackTraceRemoveThread(threadId);

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
  result = __real_pthread_create(thread,attr,debugThreadStackTraceWrapStartCode,&stackTraceThreadStartInfo);
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
* Name   : debugThreadDumpStackTrace
* Purpose: dump stacktraces of current thread
* Input  : pthreadId - pthread id
*          reason    - reason text or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadDumpStackTrace(ThreadId threadId, const char *reason)
{
  const char *name;

  name = debugThreadStackTraceGetThreadName(threadId);

  pthread_mutex_lock(&debugConsoleLock);
  {
    name = debugThreadStackTraceGetThreadName(threadId);
    fprintf(stderr,
            "Thread stack trace: '%s' (%p)%s\n",
            (name != NULL) ? name : "<none>",
            (void*)threadId,
            (reason != NULL) ? reason : ""
           );
    #ifndef NDEBUG
      debugDumpCurrentStackTrace(stderr,0,1);
    #else
      fprintf(stderr,"  not available");
    #endif
    fprintf(stderr,"\n");
  }
  pthread_mutex_unlock(&debugConsoleLock);
}

/***********************************************************************\
* Name   : debugThreadDumpAllStackTraces
* Purpose: dump stacktraces of all threads
* Input  : reason - reason text or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadDumpAllStackTraces(const char *reason)
{
  const char      *name;
  struct timespec timeout;

  pthread_mutex_lock(&debugThreadStackTraceLock);
  {
    if (debugThreadStackTraceRun)
    {
      name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

      pthread_mutex_lock(&debugConsoleLock);
      {
        fprintf(stderr,
                "Thread stack trace %02d/%02d: '%s' (%p)\n",
                debugThreadStackTraceThreadIndex+1,
                debugThreadStackTraceThreadCount,
                (name != NULL) ? name : "<none>",
                (void*)debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id
               );
        #ifndef NDEBUG
          debugDumpCurrentStackTrace(stderr,0,1);
        #else
          fprintf(stderr,"  not available");
        #endif
        fprintf(stderr,"\n");
      }
      pthread_mutex_unlock(&debugConsoleLock);

      pthread_cond_signal(&debugThreadStackTraceDone);
//fprintf(stderr,"%s, %d: signal done %p\n",__FILE__,__LINE__,pthread_self());
    }
    else
    {
      // mark running
      debugThreadStackTraceRun = TRUE;

      pthread_mutex_lock(&debugThreadStackTraceThreadLock);
      {
        // print stack trace of all threads
        for (debugThreadStackTraceThreadIndex = 0; debugThreadStackTraceThreadIndex < debugThreadStackTraceThreadCount; debugThreadStackTraceThreadIndex++)
        {
          if (pthread_equal(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id,pthread_self()) == 0)
          {
            // trigger print stack trace in thread
            #ifndef NDEBUG
//fprintf(stderr,"%s, %d: send %s %p\n",__FILE__,__LINE__,debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id),debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);
              if (pthread_kill(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id,SIGQUIT) == 0)
              {
                // wait for done signal
//fprintf(stderr,"%s, %d: wait %p: %s %p \n",__FILE__,__LINE__,pthread_self(),debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id),debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);
                clock_gettime(CLOCK_REALTIME,&timeout);
                timeout.tv_sec += 5;
                if (pthread_cond_timedwait(&debugThreadStackTraceDone,&debugThreadStackTraceLock,&timeout) != 0)
                {
                  // wait for done fail
                  name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

                  pthread_mutex_lock(&debugConsoleLock);
                  {
                    fprintf(stderr,
                            "Thread stack trace %02d/%02d: '%s' (%p)\n",
                            debugThreadStackTraceThreadIndex+1,
                            debugThreadStackTraceThreadCount,
                            (name != NULL) ? name : "<none>",
                            (void*)debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id
                           );
                    fprintf(stderr,"  not availble (terminate fail)\n");
                    fprintf(stderr,"\n");
                  }
                  pthread_mutex_unlock(&debugConsoleLock);
                }
              }
              else
              {
                // send SIQQUIT fail
                name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

                pthread_mutex_lock(&debugConsoleLock);
                {
                  fprintf(stderr,
                          "Thread stack trace %02d/%02d: '%s' (%p)\n",
                          debugThreadStackTraceThreadIndex+1,
                          debugThreadStackTraceThreadCount,
                          (name != NULL) ? name : "<none>",
                          (void*)debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id
                         );
                  fprintf(stderr,"  not availble (trigger fail)\n");
                  fprintf(stderr,"\n");
                }
                pthread_mutex_unlock(&debugConsoleLock);
              }
            #else /* NDEBUG */
              fprintf(stderr,"  not available");
            #endif /* not NDEBUG */
          }
          else
          {
            // print stack trace of this thread (probably crashed thread)
            name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

            pthread_mutex_lock(&debugConsoleLock);
            {
              fprintf(stderr,
                    "Thread stack trace %02d/%02d: '%s' (%p)%s\n",
                    debugThreadStackTraceThreadIndex+1,
                    debugThreadStackTraceThreadCount,
                    (name != NULL) ? name : "<none>",
                    (void*)debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id,
                    (reason != NULL) ? reason : ""
                   );
              #ifndef NDEBUG
                debugDumpCurrentStackTrace(stderr,0,1);
              #else /* NDEBUG */
                fprintf(stderr,"  not available");
              #endif /* not NDEBUG */
              fprintf(stderr,"\n");
            }
            pthread_mutex_unlock(&debugConsoleLock);
          }
        }
      }
      pthread_mutex_unlock(&debugThreadStackTraceThreadLock);

      // mark not running
      debugThreadStackTraceRun = FALSE;
    }
  }
  pthread_mutex_unlock(&debugThreadStackTraceLock);
}

/***********************************************************************\
* Name   : debugThreadSignalSegVHandler
* Purpose: signal-segmantation vault handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadSignalSegVHandler(int signalNumber, siginfo_t *siginfo, void *context)
{

  if (signalNumber == SIGSEGV)
  {
    pthread_mutex_lock(&debugThreadSignalLock);
    {
      debugThreadDumpAllStackTraces(" *** CRASHED ***");
    }
    pthread_mutex_unlock(&debugThreadSignalLock);
  }

  if (debugThreadSignalSegVPrevHandler.sa_sigaction != NULL)
  {
    debugThreadSignalSegVPrevHandler.sa_sigaction(signalNumber,siginfo,context);
  }
}

/***********************************************************************\
* Name   : debugThreadSignalAbortHandler
* Purpose: signal-segmantation vault handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadSignalAbortHandler(int signalNumber, siginfo_t *siginfo, void *context)
{
  if (signalNumber == SIGABRT)
  {
    pthread_mutex_lock(&debugThreadSignalLock);
    {
#ifndef NDEBUG
      // Note: in debug mode only dump current stack trace
      debugThreadDumpStackTrace(pthread_self()," *** ABORTED ***");
#else /* not NDEBUG */
      debugThreadDumpAllStackTraces(" *** ABORTED ***");
#endif /* NDEBUG */
    }
    pthread_mutex_unlock(&debugThreadSignalLock);
  }

  if (debugThreadSignalAbortPrevHandler.sa_sigaction != NULL)
  {
    debugThreadSignalAbortPrevHandler.sa_sigaction(signalNumber,siginfo,context);
  }
}

/***********************************************************************\
* Name   : debugThreadSignalQuitHandler
* Purpose: signal-quit handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugThreadSignalQuitHandler(int signalNumber, siginfo_t *siginfo, void *context)
{
  UNUSED_VARIABLE(siginfo);
  UNUSED_VARIABLE(context);

  if (signalNumber == SIGQUIT)
  {
    debugThreadDumpAllStackTraces(NULL);
  }

  // Note: do not call previouos handler
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
  debugThreadStackTraceAddThread(pthread_self());

  // install signal handlers for printing stack traces
  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugThreadSignalSegVHandler;
  sigaction(SIGSEGV,&sa,&debugThreadSignalSegVPrevHandler);

  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugThreadSignalAbortHandler;
  sigaction(SIGABRT,&sa,&debugThreadSignalAbortPrevHandler);

  sigfillset(&sa.sa_mask);
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = debugThreadSignalQuitHandler;
  sigaction(SIGQUIT,&sa,&debugThreadSignalQuitPrevHandler);
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
    debugThreadStackTraceSetThreadName(pthread_self(),startInfo->name);
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
  int             result;

  assert(thread != NULL);
  assert(name != NULL);
  assert(entryFunction != NULL);

  #ifndef NDEBUG
    pthread_once(&debugThreadInitFlag,debugThreadInit);
  #endif /* NDEBUG */

  // init thread info
  result = sem_init(&startInfo.lock,0,0);
  assert(result == 0);
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
  do
  {
    result = sem_wait(&startInfo.lock);
  }
  while ((result != 0) && (errno == EINTR));
  assert(result == 0);

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

const char *Thread_getName(ThreadId threadId)
{
  #ifndef NDEBUG
    return debugThreadStackTraceGetThreadName(threadId);
  #else /* NDEBUG */
    UNUSED_VARIABLE(threadId);

    return NULL;
  #endif /* not NDEBUG */
}

const char *Thread_getCurrentName(void)
{
  return Thread_getName(pthread_self());
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
