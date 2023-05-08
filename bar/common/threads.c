/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

#define __THREADS_IMPLEMENTATION__

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

#include "threads.h"

/****************** Conditional compilation switches *******************/
#define _STACKTRACE_ON_SIGNAL

/***************************** Constants *******************************/
const uint THREAD_LOCAL_STORAGE_HASHTABLE_SIZE      = 15;
const uint THREAD_LOCAL_STORAGE_HASHTABLE_INCREMENT = 16;

#ifndef NDEBUG
#endif /* NDEBUG */

/***************************** Datatypes *******************************/
typedef struct
{
  Thread     *thread;
  const char *name;
  int        niceLevel;
  void       (*entryFunction)(void*);
  void       *argument;
  sem_t      started;
} StartInfo;

/***************************** Variables *******************************/
#if   defined(PLATFORM_LINUX)
  const ThreadId THREAD_ID_NONE = -1;
#elif defined(PLATFORM_WINDOWS)
  const ThreadId THREAD_ID_NONE = {NULL,0};
#endif /* PLATFORM_... */

#ifndef NDEBUG
  typedef struct
  {
    sem_t lock;
    void  *(*startCode)(void*);
    void  *argument;
  } StackTraceStartInfo;

  typedef struct
  {
    ThreadId   id;
    const char *name;
  } StackTraceThreadInfo;


  #ifdef HAVE_SIGACTION
    LOCAL struct sigaction   debugThreadSignalSegVPrevHandler;
    LOCAL struct sigaction   debugThreadSignalAbortPrevHandler;
    LOCAL struct sigaction   debugThreadSignalQuitPrevHandler;
  #endif /* HAVE_SIGACTION */

  LOCAL pthread_once_t       debugThreadInitFlag                = PTHREAD_ONCE_INIT;

  #ifdef STACKTRACE_ON_SIGNAL
    LOCAL pthread_mutex_t      debugThreadSignalLock              = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  #endif

  LOCAL pthread_mutex_t      debugThreadStackTraceThreadLock    = PTHREAD_MUTEX_INITIALIZER;
  LOCAL StackTraceThreadInfo debugThreadStackTraceThreads[256];
  LOCAL uint                 debugThreadStackTraceThreadCount   = 0;

  #ifdef HAVE_SIGQUIT
    LOCAL pthread_mutex_t      debugThreadStackTraceLock          = PTHREAD_MUTEX_INITIALIZER;
    LOCAL pthread_cond_t       debugThreadStackTraceDone          = PTHREAD_COND_INITIALIZER;
    LOCAL bool                 debugThreadStackTraceRun           = FALSE;
    LOCAL uint                 debugThreadStackTraceThreadIndex;
  #endif /* HAVE_SIGQUIT */
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
    if (debugThreadStackTraceThreadCount < SIZE_OF_ARRAY(debugThreadStackTraceThreads))
    {
      debugThreadStackTraceThreads[debugThreadStackTraceThreadCount].id   = threadId;
      debugThreadStackTraceThreads[debugThreadStackTraceThreadCount].name = NULL;
      debugThreadStackTraceThreadCount++;
    }
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
* Name   : debugThreadStackTraceGetThreadName
* Purpose: get thread name
* Input  : threadId - thread id
* Output : -
* Return : thread name or "unknown"
* Notes  : -
\***********************************************************************/

LOCAL const char *debugThreadStackTraceGetThreadName(ThreadId threadId)
{
  uint       i;
  const char *name;

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
    name = "unknown";
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
  StackTraceStartInfo *stackTraceStartInfo = (StackTraceStartInfo*)userData;
  void                *(*startCode)(void*);
  void                *argument;
  ThreadId            threadId;
  void                *result;

  assert(stackTraceStartInfo != NULL);
  assert(stackTraceStartInfo->startCode != NULL);

  // get copy of start data, pthread id
  startCode = stackTraceStartInfo->startCode;
  argument  = stackTraceStartInfo->argument;
  threadId  = Thread_getCurrentId();

  // add thread
  debugThreadStackTraceAddThread(threadId);

  // signal thread start done
  sem_post(&stackTraceStartInfo->lock);

  // run thread code
  result = startCode(argument);

  // remove thread
  debugThreadStackTraceRemoveThread(threadId);

  return result;
}

/***********************************************************************\
* Name   : __wrap_pthread_create
* Purpose: wrapper function for pthread_create()
* Input  : thread    - thread variable
*          attr      - thread attributes
*          startCode - thread entry code
*          argument  - thread argument
* Output : -
* Return : 0 or error code
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
  StackTraceStartInfo stackTraceStartInfo;
  int                       result;

  assert(__real_pthread_create != NULL);
  assert(thread != NULL);
  assert(startCode != NULL);

  // init thread
  sem_init(&stackTraceStartInfo.lock,0,0);
  stackTraceStartInfo.startCode = startCode;
  stackTraceStartInfo.argument  = argument;
  result = __real_pthread_create(thread,attr,debugThreadStackTraceWrapStartCode,&stackTraceStartInfo);
  if (result == 0)
  {
    // wait until thread started
    sem_wait(&stackTraceStartInfo.lock);
  }

  // free resources
  sem_destroy(&stackTraceStartInfo.lock);

  return result;
}

/***********************************************************************\
* Name   : debugThreadDumpStackTrace
* Purpose: dump stacktraces of current thread
* Input  : threadId       - thread id
*          type           - output type; see DebugDumpStackTraceOutputTypes
*          skipFrameCount - skip frame count
*          reason         - reason text or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#if STACKTRACE_ON_SIGNAL
LOCAL void debugThreadDumpStackTrace(ThreadId                       threadId,
                                     DebugDumpStackTraceOutputTypes type,
                                     uint                           skipFrameCount,
                                     const char                     *reason
                                    )
{
  const char *name;

  name = debugThreadStackTraceGetThreadName(threadId);

  pthread_mutex_lock(&debugConsoleLock);
  {
    debugDumpStackTraceOutput(stderr,
                              0,
                              type,
                              "Thread stack trace: '%s' (%s)%s\n",
                              (name != NULL) ? name : "<none>",
                              Thread_getIdString(threadId),
                              (reason != NULL) ? reason : ""
                             );
    #ifndef NDEBUG
      debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,1+skipFrameCount);
    #else
      debugDumpStackTraceOutput(stderr,0,type,"  not available");
    #endif
  }
  pthread_mutex_unlock(&debugConsoleLock);
}
#endif // STACKTRACE_ON_SIGNAL

/***********************************************************************\
* Name   : debugThreadDumpAllStackTraces
* Purpose: dump stacktraces of all threads
* Input  : type           - output type; see DebugDumpStackTraceOutputTypes
*          skipFrameCount - skip frame count
*          reason         - reason text or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGQUIT
LOCAL void debugThreadDumpAllStackTraces(DebugDumpStackTraceOutputTypes type,
                                         uint                           skipFrameCount,
                                         const char                     *reason
                                        )
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
        debugDumpStackTraceOutput(stderr,
                                  0,
                                  type,
                                  "Thread stack trace %02d/%02d: '%s' (%s)\n",
                                  debugThreadStackTraceThreadIndex+1,
                                  debugThreadStackTraceThreadCount,
                                  (name != NULL) ? name : "<none>",
                                  Thread_getIdString(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id)
                                 );
        #ifndef NDEBUG
          debugDumpCurrentStackTrace(stderr,0,type,1+skipFrameCount);
        #else
          debugDumpStackTraceOutput(stderr,0,type,"  not available");
        #endif
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
                timeout.tv_sec += 30;
                if (pthread_cond_timedwait(&debugThreadStackTraceDone,&debugThreadStackTraceLock,&timeout) == 0)
                {
                  // wait for done fail
                  name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

                  pthread_mutex_lock(&debugConsoleLock);
                  {
                    debugDumpStackTraceOutput(stderr,
                                              0,
                                              type,
                                              "Thread stack trace %02d/%02d: '%s' (%s)\n",
                                              debugThreadStackTraceThreadIndex+1,
                                              debugThreadStackTraceThreadCount,
                                              (name != NULL) ? name : "<none>",
                                              Thread_getIdString(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id)
                                             );
                    debugDumpStackTraceOutput(stderr,0,type,"  not available (terminate failed)\n");
                  }
                  pthread_mutex_unlock(&debugConsoleLock);
                }
                else
                {
                  fprintf(stderr,
                          "Warning: process signal QUIT by thread %s failed (error %d: %s)",
                          Thread_getIdString(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id),
                          errno,
                          strerror(errno)
                         );
                }
              }
              else
              {
                // send SIQQUIT fail
                name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

                pthread_mutex_lock(&debugConsoleLock);
                {
                  debugDumpStackTraceOutput(stderr,
                                            0,
                                            type,
                                            "Thread stack trace %02d/%02d: '%s' (%s)\n",
                                            debugThreadStackTraceThreadIndex+1,
                                            debugThreadStackTraceThreadCount,
                                            (name != NULL) ? name : "<none>",
                                            Thread_getIdString(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id)
                                           );
                  debugDumpStackTraceOutput(stderr,0,type,"  not available (trigger failed)\n");
                }
                pthread_mutex_unlock(&debugConsoleLock);
              }
            #else /* NDEBUG */
              debugDumpStackTraceOutput(stderr,0,type,"  not available");
            #endif /* not NDEBUG */
          }
          else
          {
            // print stack trace of this thread (probably crashed thread)
            name = debugThreadStackTraceGetThreadName(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id);

            pthread_mutex_lock(&debugConsoleLock);
            {
              debugDumpStackTraceOutput(stderr,
                                        0,
                                        type,
                                        "Thread stack trace %02d/%02d: '%s' (%s)%s\n",
                                        debugThreadStackTraceThreadIndex+1,
                                        debugThreadStackTraceThreadCount,
                                        (name != NULL) ? name : "<none>",
                                        Thread_getIdString(debugThreadStackTraceThreads[debugThreadStackTraceThreadIndex].id),
                                        (reason != NULL) ? reason : ""
                                       );
              #ifndef NDEBUG
                debugDumpCurrentStackTrace(stderr,0,type,1+skipFrameCount);
              #else /* NDEBUG */
                debugDumpStackTraceOutput(stderr,0,type,"  not available");
              #endif /* not NDEBUG */
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
#endif /* HAVE_SIGQUIT */

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

#ifdef HAVE_SIGACTION
LOCAL void debugThreadSignalSegVHandler(int signalNumber, siginfo_t *siginfo, void *context)
#else /* not HAVE_SIGACTION */
LOCAL void debugThreadSignalSegVHandler(int signalNumber)
#endif /* HAVE_SIGACTION */
{
  if (signalNumber == SIGSEGV)
  {
    #ifdef STACKTRACE_ON_SIGNAL
      #ifdef HAVE_SIGQUIT
        pthread_mutex_lock(&debugThreadSignalLock);
        {
          debugThreadDumpAllStackTraces(DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL,1," *** CRASHED ***");
        }
        pthread_mutex_unlock(&debugThreadSignalLock);
      #endif /* HAVE_SIGQUIT */
    #endif
  }

  #ifdef HAVE_SIGACTION
    // call previous handler
    if (debugThreadSignalSegVPrevHandler.sa_sigaction != NULL)
    {
      debugThreadSignalSegVPrevHandler.sa_sigaction(signalNumber,siginfo,context);
    }
  #endif /* HAVE_SIGACTION */
}

/***********************************************************************\
* Name   : debugThreadSignalAbortHandler
* Purpose: signal-abort handler to print stack trace
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGACTION
LOCAL void debugThreadSignalAbortHandler(int signalNumber, siginfo_t *siginfo, void *context)
#else /* not HAVE_SIGACTION */
LOCAL void debugThreadSignalAbortHandler(int signalNumber)
#endif /* HAVE_SIGACTION */
{
  if (signalNumber == SIGABRT)
  {
    #ifdef STACKTRACE_ON_SIGNAL
      pthread_mutex_lock(&debugThreadSignalLock);
      {
        #ifndef NDEBUG
          // Note: in debug mode only dump current stack trace
          debugThreadDumpStackTrace(pthread_self(),DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL,1," *** ABORTED ***");
        #else /* not NDEBUG */
          debugThreadDumpAllStackTraces(DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL," *** ABORTED ***");
        #endif /* NDEBUG */
      }
      pthread_mutex_unlock(&debugThreadSignalLock);
    #endif
  }

  #ifdef HAVE_SIGACTION
    // call previous handler
    if (debugThreadSignalAbortPrevHandler.sa_sigaction != NULL)
    {
      debugThreadSignalAbortPrevHandler.sa_sigaction(signalNumber,siginfo,context);
    }
  #endif /* HAVE_SIGACTION */
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

#if defined(ENABLE_DEBUG_THREAD_CRASH_HANDLERS) && defined(HAVE_SIGQUIT) && defined(HAVE_SIGACTION)
#if defined(HAVE_SIGQUIT) && defined(HAVE_SIGACTION)
LOCAL void debugThreadSignalQuitHandler(int signalNumber, siginfo_t *siginfo, void *context)
#else /* not defined(HAVE_SIGQUIT) && defined(HAVE_SIGACTION) */
LOCAL void debugThreadSignalQuitHandler(int signalNumber)
#endif /* defined(HAVE_SIGQUIT) && defined(HAVE_SIGACTION) */
{
  UNUSED_VARIABLE(siginfo);
  UNUSED_VARIABLE(context);

  if (signalNumber == SIGQUIT)
  {
    #ifdef STACKTRACE_ON_SIGNAL
      // Note: do not lock; signal handler is called for every thread
      debugThreadDumpAllStackTraces(DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,1,NULL);
    #endif
  }

  #ifdef HAVE_SIGACTION
    // call previous handler
    if (debugThreadSignalQuitPrevHandler.sa_sigaction != NULL)
    {
      debugThreadSignalQuitPrevHandler.sa_sigaction(signalNumber,siginfo,context);
    }
  #endif /* HAVE_SIGACTION */
}
#endif /* defined(ENABLE_DEBUG_THREAD_CRASH_HANDLERS) && defined(HAVE_SIGQUIT) && defined(HAVE_SIGACTION) */

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
  #ifdef ENABLE_DEBUG_THREAD_CRASH_HANDLERS
    #ifdef HAVE_SIGACTION
      struct sigaction signalAction;
    #endif /* HAVE_SIGACTION */

    // add main thread
    debugThreadStackTraceAddThread(pthread_self());

    // install signal handlers for printing stack traces
    #ifdef HAVE_SIGACTION
      sigfillset(&signalAction.sa_mask);
      signalAction.sa_flags     = SA_SIGINFO;
      signalAction.sa_sigaction = debugThreadSignalSegVHandler;
      sigaction(SIGSEGV,&signalAction,&debugThreadSignalSegVPrevHandler);

      sigfillset(&signalAction.sa_mask);
      signalAction.sa_flags     = SA_SIGINFO;
      signalAction.sa_sigaction = debugThreadSignalAbortHandler;
      sigaction(SIGABRT,&signalAction,&debugThreadSignalAbortPrevHandler);

      #ifdef HAVE_SIGQUIT
        sigfillset(&signalAction.sa_mask);
        signalAction.sa_flags     = SA_SIGINFO;
        signalAction.sa_sigaction = debugThreadSignalQuitHandler;
        sigaction(SIGQUIT,&signalAction,&debugThreadSignalQuitPrevHandler);
      #endif /* HAVE_SIGQUIT */
    #else /* not HAVE_SIGACTION */
      signal(SIGSEGV,debugThreadSignalSegVHandler);
      signal(SIGABRT,debugThreadSignalAbortHandler);
      #ifdef HAVE_SIGQUIT
        signal(SIGQUIT,debugThreadSignalQuitHandler);
      #endif /* HAVE_SIGQUIT */
    #endif /* HAVE_SIGACTION */
  #endif /* not ENABLE_DEBUG_THREAD_CRASH_HANDLERS */
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : threadTerminated
* Purpose: request terminate thread callback
* Input  : userData - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void threadTerminated(void *userData)
{
  Thread *thread = (Thread*)userData;

  assert(thread != NULL);

  thread->terminatedFlag = TRUE;
}

/***********************************************************************\
* Name   : threadStartCode
* Purpose: thread start code
* Input  : userData - start info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void *threadStartCode(void *userData)
{
  StartInfo *startInfo = (StartInfo*)userData;
  void      (*entryFunction)(void*);
  void      *argument;

  assert(startInfo != NULL);

  pthread_cleanup_push(threadTerminated,startInfo->thread);
  {
    // try to set thread name
    #ifdef HAVE_PTHREAD_SETNAME_NP
      if (startInfo->name != NULL)
      {
        (void)pthread_setname_np(pthread_self(),startInfo->name);
      }
    #endif /* HAVE_PTHREAD_SETNAME_NP */

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
    sem_post(&startInfo->started);

    // run thread code
    assert(entryFunction != NULL);
    entryFunction(argument);
  }
  pthread_cleanup_pop(1);

  return NULL;
}

/*---------------------------------------------------------------------*/

Errors Thread_initAll(void)
{
  #ifndef NDEBUG
    pthread_once(&debugThreadInitFlag,debugThreadInit);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

void Thread_doneAll(void)
{
}

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

#ifdef NDEBUG
bool Thread_init(Thread     *thread,
                 const char *name,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *argument
                )
#else /* not NDEBUG */
bool __Thread_init(const char *__fileName__,
                   ulong      __lineNb__,
                   Thread     *thread,
                   const char *name,
                   int        niceLevel,
                   const void *entryFunction,
                   void       *argument
                  )
#endif /* NDEBUG */
{
  StartInfo      startInfo;
  pthread_attr_t threadAttributes;
  int            result;

  assert(thread != NULL);
  assert(name != NULL);
  assert(entryFunction != NULL);

  #ifndef NDEBUG
    pthread_once(&debugThreadInitFlag,debugThreadInit);
  #endif /* NDEBUG */

  // init thread info
  startInfo.thread        = thread;
  startInfo.name          = name;
  startInfo.niceLevel     = niceLevel;
  startInfo.entryFunction = entryFunction;
  startInfo.argument      = argument;
  result = sem_init(&startInfo.started,0,0);
  if (result != 0)
  {
    HALT_INTERNAL_ERROR("cannot initialise thread start lock");
  }

  // init thread attributes
  pthread_attr_init(&threadAttributes);
  #ifdef HAVE_PTHREAD_ATTR_SETNAME
    if (name != NULL)
    {
      pthread_attr_setname(&threadAttributes,(char*)name);
    }
  #endif /* HAVE_PTHREAD_ATTR_SETNAME */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(thread,Thread);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,thread,Thread);
  #endif /* not NDEBUG */

  // start thread
  thread->quitFlag   = FALSE;
  thread->joinedFlag = FALSE;
  if (pthread_create(&thread->handle,
                     &threadAttributes,
                     threadStartCode,
                     &startInfo
                    ) != 0
     )
  {
    #ifdef NDEBUG
      DEBUG_REMOVE_RESOURCE_TRACE(thread,Thread);
    #else /* not NDEBUG */
      DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,thread,Thread);
    #endif /* NDEBUG */
    pthread_attr_destroy(&threadAttributes);
    sem_destroy(&startInfo.started);
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
    HALT_INTERNAL_ERROR("wait for thread start lock failed");
  }

  // free resources
  sem_destroy(&startInfo.started);

  return TRUE;
}

#ifdef NDEBUG
void Thread_done(Thread *thread)
#else /* not NDEBUG */
void __Thread_done(const char *__fileName__,
                   ulong      __lineNb__,
                   Thread     *thread
                  )
#endif /* NDEBUG */
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);
  assert(Thread_isTerminated(thread));

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(thread,Thread);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,thread,Thread);
  #endif /* NDEBUG */

  UNUSED_VARIABLE(thread);
}

int Thread_getPriority(Thread *thread)
{
  int                policy;
  struct sched_param scheduleParameter;

  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  pthread_getschedparam(thread->handle,&policy,&scheduleParameter);

  return scheduleParameter.sched_priority;
}

void Thread_setPriority(Thread *thread, int priority)
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  #ifdef HAVE_PTHREAD_SETSCHEDPRIO
    pthread_setschedprio(thread->handle,priority);
  #else
    UNUSED_VARIABLE(thread);
    UNUSED_VARIABLE(priority);
  #endif /* HAVE_PTHREAD_SETSCHEDPRIO */
}

bool Thread_join(Thread *thread)
{
  assert(thread != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(thread);

  thread->quitFlag = TRUE;

  if (!thread->joinedFlag)
  {
    // Note: pthread_join() can only be called once with success!
    if (pthread_join(thread->handle,NULL) != 0)
    {
      return FALSE;
    }
    thread->joinedFlag = TRUE;
  }

  return TRUE;
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

const char *Thread_getName(const ThreadId threadId)
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

const char *Thread_getIdString(const ThreadId threadId)
{
  // Note: use ringbuffer with string ids to avoid using same string in consequtive calls!
  static char idStrings[16][64+1];
  static uint idStringIndex = 0;

  uint  i;
  int   j;
  uint8 *p;

  assert((2+sizeof(ThreadId)*2) < (sizeof(idStrings[0])-1));

  i = ATOMIC_INCREMENT(idStringIndex);

  // Note: reverse to be compatible with gdb output
  p = (uint8*)(void*)(&threadId);
  stringSet(idStrings[i%16],sizeof(idStrings[i%16]),"0x");
  for (j = (int)sizeof(ThreadId)-1; j >= 0; j--)
  {
    stringFormatAppend(idStrings[i%16],sizeof(idStrings[i%16]),"%02x",p[j]);
  }

  return idStrings[i%16];
}

const char *Thread_getCurrentIdString(void)
{
  return Thread_getIdString(pthread_self());
}

void Thread_initLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageAllocFunction threadLocalStorageAllocFunction, void *threadLocalStorageAllocUserData)
{
  assert(threadLocalStorage != NULL);

  threadLocalStorage->allocFunction = threadLocalStorageAllocFunction;
  threadLocalStorage->allocUserData = threadLocalStorageAllocUserData;
  pthread_mutex_init(&threadLocalStorage->lock,NULL);
  List_init(&threadLocalStorage->instanceList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
}

void Thread_doneLocalVariable(ThreadLocalStorage *threadLocalStorage, ThreadLocalStorageFreeFunction threadLocalStorageFreeFunction, void *threadLocalStorageFreeUserData)
{
  ThreadLocalStorageInstanceNode *threadLocalStorageInstanceNode;

  assert(threadLocalStorage != NULL);

  pthread_mutex_lock(&threadLocalStorage->lock);
  {
    while (!List_isEmpty(&threadLocalStorage->instanceList))
    {
      threadLocalStorageInstanceNode = (ThreadLocalStorageInstanceNode*)List_removeFirst(&threadLocalStorage->instanceList);
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
