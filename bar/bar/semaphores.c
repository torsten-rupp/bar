/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for inter-process semaphores
* Systems: all POSIX
*
\***********************************************************************/

#define __SEMAPHORES_IMPLEMENATION__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <pthread.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
#endif /* PLATFORM_... */

#include "global.h"
#include "lists.h"
#include "threads.h"

#include "semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#ifndef NDEBUG
  #define DEBUG_FLAG_READ       FALSE
  #define DEBUG_FLAG_READ_WRITE FALSE
  #define DEBUG_FLAG_MODIFIED   FALSE

  const char *SEMAPHORE_LOCK_TYPE_NAMES[] =
  {
    [SEMAPHORE_LOCK_TYPE_NONE]       = "NONE",
    [SEMAPHORE_LOCK_TYPE_READ]       = "READ",
    [SEMAPHORE_LOCK_TYPE_READ_WRITE] = "READ/WRITE"
  };
#endif /* not NDEBUG */

/***************************** Datatypes *******************************/

#ifndef NDEBUG
  typedef struct
  {
    LIST_HEADER(Semaphore);
  } DebugSemaphoreList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/

#ifndef NDEBUG
  LOCAL pthread_once_t      debugSemaphoreInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugSemaphoreLockAttribute;
  LOCAL pthread_mutex_t     debugSemaphoreLock;
  LOCAL ThreadId            debugSemaphoreThreadId;
  LOCAL DebugSemaphoreList  debugSemaphoreList;
  LOCAL void                (*debugSignalQuitPrevHandler)(int);
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#if   defined(PLATFORM_LINUX)
  #define __SEMAPHORE_REQUEST_LOCK(semaphore) \
    do \
    { \
      pthread_mutex_lock(&semaphore->requestLock); \
    } \
    while (0)

  #define __SEMAPHORE_REQUEST_UNLOCK(semaphore) \
    do \
    { \
      pthread_mutex_unlock(&semaphore->requestLock); \
    } \
    while (0)
#elif defined(PLATFORM_WINDOWS)
  #define __SEMAPHORE_REQUEST_LOCK(semaphore) \
    do \
    { \
      WaitForSingleObject(semaphore->requestLock,INFINITE); \
    } \
    while (0)

  #define __SEMAPHORE_REQUEST_UNLOCK(semaphore) \
    do \
    { \
      ReleaseMutex(semaphore->requestLock); \
    } \
    while (0)
#endif /* PLATFORM_... */

#ifndef NDEBUG

  #if   defined(PLATFORM_LINUX)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        bool __locked; \
        \
        pthread_once(&debugSemaphoreInitFlag,debugInit); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        \
        pthread_mutex_lock(&debugSemaphoreLock); \
        { \
debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); \
          __locked = (pthread_mutex_trylock(&semaphore->lock) == 0); \
          if (0&& !__locked) \
          { \
            /* debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); */\
          } \
        } \
        pthread_mutex_unlock(&debugSemaphoreLock); \
        \
        if (!__locked) pthread_mutex_lock(&semaphore->lock); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        assert(timeout != WAIT_FOREVER); \
        \
        pthread_once(&debugSemaphoreInitFlag,debugInit); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s (timeout %ldms)\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,timeout); \
        \
        pthread_mutex_lock(&debugSemaphoreLock); \
        { \
          debugCheckForDeadLock(NULL,0/*fileName,lineNb*/,semaphore,lockType); \
        } \
        pthread_mutex_unlock(&debugSemaphoreLock); \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s (timeout %ldms)\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,timeout); \
        if (pthread_mutex_timedlock(&semaphore->lock,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(NULL,0/*fileName,lineNb*/,semaphore,lockType); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_mutex_trylock(&semaphore->lock) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text,n) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s n=%d\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,n); \
        pthread_mutex_unlock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(debugFlag,text,condition,mutex) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_wait(condition,mutex); \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        assert(timeout != WAIT_FOREVER); \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_signal(condition); \
      } \
      while (0)
  #elif defined(PLATFORM_WINDOWS)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        WaitForSingleObject(semaphore,INFINITE); \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (WaitForSingleObject(semaphore,&__tp) != WAIT_OBJECT_0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (WaitForSingleObject(semaphore,0) != WAIT_OBJECT_0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text,n) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s n=%d\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,n); \
        ReleaseMutext(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(debugFlag,text,condition,mutex) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_wait(condition,mutex); \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(fileName,lineNb,semaphore,lockType); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_signal(condition); \
      } \
      while (0)
  #endif /* PLATFORM_... */

#else /* NDEBUG */

  #if   defined(PLATFORM_LINUX)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        pthread_mutex_lock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (pthread_mutex_timedlock(&semaphore->lock,&__tp) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        if (pthread_mutex_trylock(&semaphore->lock) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text,n) \
      do \
      { \
        pthread_mutex_unlock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(debugFlag,text,condition,mutex) \
      do \
      { \
        pthread_cond_wait(condition,mutex); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*10000000L; \
        \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition) \
      do \
      { \
        pthread_cond_broadcast(condition); \
      } \
      while (0)
  #elif defined(PLATFORM_WINDOWS)
    #define __SEMAPHORE_LOCK(semaphore,debugFlag,text) \
      do \
      { \
        pthread_mutex_lock(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        if (WaitForSingleObject(semaphore,timeout) != WAIT_OBJECT_0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        if (WaitForSingleObject(semaphore,0) != WAIT_OBJECT_0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text,n) \
      do \
      { \
        pthread_mutex_unlock(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition) \
      do \
      { \
        pthread_cond_wait(condition,semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(semaphore,debugFlag,text,condition,timeout,lockedFlag) \
      do \
      { \
        if (pthread_cond_timedwait(condition,semaphore,timeout) == ETIMEDOUT) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition) \
      do \
      { \
        pthread_cond_broadcast(condition); \
      } \
      while (0)
  #endif /* PLATFORM_... */

#endif /* not NDEBUG */

/***************************** Forwards ********************************/
#ifndef NDEBUG
  LOCAL void debugSemaphoreSignalHandler(int signalNumber);
#endif /* not NDEBUG */

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugInit(void)
{
  // init variables
  debugSemaphoreThreadId = Thread_getCurrentId();
  List_init(&debugSemaphoreList);

  // init lock
  pthread_mutexattr_init(&debugSemaphoreLockAttribute);
  pthread_mutexattr_settype(&debugSemaphoreLockAttribute,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&debugSemaphoreLock,&debugSemaphoreLockAttribute) != 0)
  {
    HALT_INTERNAL_ERROR("Cannot initialize semaphore debug lock!");
  }

  // install signal handler for Ctrl-\ (SIGQUIT) for printing debug information
  debugSignalQuitPrevHandler = signal(SIGQUIT,debugSemaphoreSignalHandler);
}

/***********************************************************************\
* Name   : signalHandler
* Purpose: signal handler
* Input  : signalNumber - signal number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugSemaphoreSignalHandler(int signalNumber)
{
  if ((signalNumber == SIGQUIT) && Thread_isCurrentThread(debugSemaphoreThreadId))
  {
    Semaphore_debugPrintInfo();
  }

  if (debugSignalQuitPrevHandler != NULL)
  {
    debugSignalQuitPrevHandler(signalNumber);
  }
}

/***********************************************************************\
* Name   : debugAddThread
* Purpose: add thread to thread info array
* Input  : threadInfos     - thread info array
*          threadInfoCount - thread info count
*          lockType        - lock type; see SemaphoreLockTypes
*          fileName        - file name
*          lineNb          - line number
* Output : threadInfoCount - new thread info count
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool debugAddThreadInfo(__SemaphoreThreadInfo threadInfos[],
                                     uint                  *threadInfoCount,
                                     SemaphoreLockTypes    lockType,
                                     const char            *fileName,
                                     ulong                 lineNb
                                    )
{
  assert(threadInfos != NULL);
  assert(threadInfoCount != NULL);
  assert((*threadInfoCount) <= __SEMAPHORE_MAX_THREAD_INFO);

  if ((*threadInfoCount) < __SEMAPHORE_MAX_THREAD_INFO)
  {
    threadInfos[(*threadInfoCount)].threadId = Thread_getCurrentId();
    threadInfos[(*threadInfoCount)].lockType = lockType;
    threadInfos[(*threadInfoCount)].fileName = fileName;
    threadInfos[(*threadInfoCount)].lineNb   = lineNb;
    (*threadInfoCount)++;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : debugAddLockedThreadInfo
* Purpose: add thread to locked thread info array
* Input  : semaphore - semaphore
*          lockType  - lock type; see SemaphoreLockTypes
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugAddLockedThreadInfo(Semaphore          *semaphore,
                                           SemaphoreLockTypes lockType,
                                           const char         *fileName,
                                           ulong              lineNb
                                          )
{
  assert(semaphore != NULL);

  if (!debugAddThreadInfo(semaphore->lockedBy,&semaphore->lockedByCount,lockType,fileName,lineNb))
  {
    fprintf(stderr,
            "DEBUG WARNING: too many thread locks for semaphore '%s' at %s, line %lu (max. %lu)!\n",
            semaphore->name,
            fileName,
            lineNb,
            (ulong)SIZE_OF_ARRAY(semaphore->lockedBy)
           );
  }
}

/***********************************************************************\
* Name   : debugAddPendingThreadInfo
* Purpose: add thread to pending thread info array
* Input  : semaphore - semaphore
*          lockType  - lock type; see SemaphoreLockTypes
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugAddPendingThreadInfo(Semaphore          *semaphore,
                                            SemaphoreLockTypes lockType,
                                            const char         *fileName,
                                            ulong              lineNb
                                           )
{
  assert(semaphore != NULL);

  if (!debugAddThreadInfo(semaphore->pendingBy,&semaphore->pendingByCount,lockType,fileName,lineNb))
  {
    fprintf(stderr,
            "DEBUG WARNING: too many pending thread locks for semaphore '%s' at %s, line %lu (max. %lu)!\n",
            semaphore->name,
            fileName,
            lineNb,
            (ulong)SIZE_OF_ARRAY(semaphore->pendingBy)
           );
  }
}

/***********************************************************************\
* Name   : debugRemoveThreadInfo
* Purpose: remove thread from thread info array
* Input  : threadInfos     - thread info array
*          threadInfoCount - thread info count
* Output : threadInfoCount - new thread info count
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool debugRemoveThreadInfo(__SemaphoreThreadInfo threadInfos[],
                                        uint                  *threadInfoCount
                                       )
{
  int i;

  assert(threadInfos != NULL);
  assert(threadInfoCount != NULL);
  assert((*threadInfoCount) <= __SEMAPHORE_MAX_THREAD_INFO);

  i = (int)(*threadInfoCount)-1;
  while (   (i >= 0)
         && !Thread_isCurrentThread(threadInfos[i].threadId)
        )
  {
    i--;
  }
  if (i >= 0)
  {
    threadInfos[i] = threadInfos[(*threadInfoCount)-1];
    (*threadInfoCount)--;

    return TRUE;
  }
  else
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    return FALSE;
  }
}

/***********************************************************************\
* Name   : debugRemoveLockedThreadInfo
* Purpose: remove thread from locked thread info array
* Input  : semaphore - semaphore
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugRemoveLockedThreadInfo(Semaphore  *semaphore,
                                              const char *fileName,
                                              ulong      lineNb
                                             )
{
  assert(semaphore != NULL);

  if (!debugRemoveThreadInfo(semaphore->lockedBy,&semaphore->lockedByCount))
  {
    Semaphore_debugPrintInfo();
    HALT_INTERNAL_ERROR("Thread '%s' (%s) try to unlock not locked semaphore '%s' at %s, line %lu!",
                        Thread_getCurrentName(),
                        Thread_getCurrentIdString(),
                        semaphore->name,
                        fileName,
                        lineNb
                       );
  }
}

/***********************************************************************\
* Name   : debugRemovePendingThreadInfo
* Purpose: remove thread from pending thread info array
* Input  : semaphore - semaphore
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugRemovePendingThreadInfo(Semaphore  *semaphore,
                                               const char *fileName,
                                               ulong      lineNb
                                              )
{
  assert(semaphore != NULL);

  if (!debugRemoveThreadInfo(semaphore->pendingBy,&semaphore->pendingByCount))
  {
    Semaphore_debugPrintInfo();
    HALT_INTERNAL_ERROR("Thread '%s' (%s) try to remove not pending semaphore '%s' at %s, line %lu!",
                        Thread_getCurrentName(),
                        Thread_getCurrentIdString(),
                        semaphore->name,
                        fileName,
                        lineNb
                       );
  }
}

/***********************************************************************\
* Name   : getLockedByThreadInfo
* Purpose: get locked thread info
* Input  : semaphore - semaphore
*          threadId  - thread id
* Output : -
* Return : thread info or NULL if not locked by thread
* Notes  : -
\***********************************************************************/

LOCAL const __SemaphoreThreadInfo *getLockedByThreadInfo(const Semaphore *semaphore, const ThreadId threadId)
{
  uint i;

  assert(semaphore != NULL);

  for (i = 0; i < semaphore->lockedByCount; i++)
  {
    if (Thread_equalThreads(semaphore->lockedBy[i].threadId,threadId)) return &semaphore->lockedBy[i];
  }

  return NULL;
}

/***********************************************************************\
* Name   : getPendingByThreadInfo
* Purpose: get pending thread info
* Input  : semaphore - semaphore
*          threadId  - thread id
* Output : -
* Return : thread info or NULL if thread is not pending
* Notes  : -
\***********************************************************************/

LOCAL const __SemaphoreThreadInfo *getPendingByThreadInfo(const Semaphore *semaphore, const ThreadId threadId)
{
  uint i;

  assert(semaphore != NULL);

  for (i = 0; i < semaphore->pendingByCount; i++)
  {
    if (Thread_equalThreads(semaphore->pendingBy[i].threadId,threadId)) return &semaphore->pendingBy[i];
  }

  return NULL;
}

/***********************************************************************\
* Name   : debugCheckForDeadLock
* Purpose: check for dead lock
* Input  : fileName      - file name
*          lineNb        - line number
*          semaphore     - semaphore
*          lockType      - lock types; see SemaphoreLockTypes
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugCheckForDeadLock(const char         *fileName,
                                 ulong              lineNb,
                                 Semaphore          *semaphore,
                                 SemaphoreLockTypes lockType
                                )
{
  uint                        i;
  const Semaphore             *otherSemaphore;
  const __SemaphoreThreadInfo *lockedInfo,*pendingInfo;

  assert(semaphore != NULL);

//  __SEMAPHORE_REQUEST_LOCK(semaphore);
  {
    // check all threads who currently own semaphore
    for (i = 0; i < semaphore->lockedByCount; i++)
    {
      // check if thread is pending for a semaphore which is owned by current thread
      LIST_ITERATE(&debugSemaphoreList,otherSemaphore)
      {
        lockedInfo  = getLockedByThreadInfo(otherSemaphore,Thread_getCurrentId());
        pendingInfo = getPendingByThreadInfo(otherSemaphore,semaphore->lockedBy[i].threadId);
        if (   (lockedInfo != NULL)
            && (lockedInfo->lockType != lockType)
            && (pendingInfo != NULL)
           )
        {
          fprintf(stderr,"Warning: DEAD LOCK at %s, line %lu\n",fileName,lineNb);
          fprintf(stderr,"  Thread '%s' (%s)\n    locked '%s' %s (%s, line %lu) at %s, line %lu and\n    wait for '%s' %s (%s, line %lu) at %s, line %lu\n",
                  Thread_getCurrentName(),
                  Thread_getCurrentIdString(),
                  otherSemaphore->name,
                  SEMAPHORE_LOCK_TYPE_NAMES[lockedInfo->lockType],
                  otherSemaphore->fileName,
                  otherSemaphore->lineNb,
                  lockedInfo->fileName,
                  lockedInfo->lineNb,
                  semaphore->name,
                  SEMAPHORE_LOCK_TYPE_NAMES[pendingInfo->lockType],
                  semaphore->fileName,
                  semaphore->lineNb,
                  fileName,
                  lineNb
                 );
          if (!Thread_isCurrentThread(semaphore->lockedBy[i].threadId))
          {
            fprintf(stderr,"  Thread '%s' (%s)\n    locked '%s' %s (%s, line %lu) at %s, line %lu and\n    wait for '%s' %s (%s, line %lu) at %s, line %lu\n",
                    Thread_getName(semaphore->lockedBy[i].threadId),
                    Thread_getIdString(semaphore->lockedBy[i].threadId),
                    semaphore->name,
                    SEMAPHORE_LOCK_TYPE_NAMES[semaphore->lockedBy[i].lockType],
                    semaphore->fileName,
                    semaphore->lineNb,
                    semaphore->lockedBy[i].fileName,
                    semaphore->lockedBy[i].lineNb,
                    otherSemaphore->name,
                    SEMAPHORE_LOCK_TYPE_NAMES[pendingInfo->lockType],
                    otherSemaphore->fileName,
                    otherSemaphore->lineNb,
                    pendingInfo->fileName,
                    pendingInfo->lineNb
                   );
          }
          fprintf(stderr,"  Stack trace:\n");
          debugDumpCurrentStackTrace(stderr,2,1);
          Semaphore_debugPrintInfo();
HALT_INTERNAL_ERROR_AT(fileName,lineNb,"DEAD LOCK!");
          break;
        }
      }
    }
  }
//  __SEMAPHORE_REQUEST_UNLOCK(semaphore);
}

/***********************************************************************\
* Name   : debugCheckUnlocked
* Purpose: add thread to thread info array
* Input  : threadInfo      - thread info array
*          threadInfoCount - thread info count
*          fileName        - file name
*          lineNb          - line number
* Output : threadInfoCount - new thread info count
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugCheckUnlocked(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  if (semaphore->lockedByCount > 0)
  {
    HALT_INTERNAL_ERROR("Thread '%s' (%s) did not unlock semaphore '%s' which was locked at %s, line %lu!",
                        Thread_getName(semaphore->lockedBy[0].threadId),
                        Thread_getIdString(semaphore->lockedBy[0].threadId),
                        semaphore->name,
                        semaphore->lockedBy[0].fileName,
                        semaphore->lockedBy[0].lineNb
                       );
  }
}

#endif /* not NDEBUG */

/***********************************************************************\
* Name   : lock
* Purpose: lock semaphore
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL bool lock(Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType,
                long               timeout
               )
#else /* not NDEBUG */
LOCAL bool lock(const char         *fileName,
                ulong              lineNb,
                Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType,
                long               timeout
               )
#endif /* NDEBUG */
{
  bool lockedFlag;

  assert(semaphore != NULL);
  assert((semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ) || (semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

  lockedFlag = TRUE;

  switch (semaphoreLockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
      /* request read lock
         Note: for a read lock semaphore is locked temporary and read-lock is stored
      */

      // increment read request counter atomically
      __SEMAPHORE_REQUEST_LOCK(semaphore);
      {
        semaphore->readRequestCount++;

        #ifndef NDEBUG
          // debug trace code: store pending information
          debugAddPendingThreadInfo(semaphore,semaphoreLockType,fileName,lineNb);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_REQUEST_UNLOCK(semaphore);

      // read: aquire lock temporary and increment read-lock counter
      if (timeout != WAIT_FOREVER)
      {
        __SEMAPHORE_LOCK_TIMEOUT(semaphore,semaphoreLockType,DEBUG_FLAG_READ,"R",timeout,lockedFlag);
        if (!lockedFlag)
        {
          __SEMAPHORE_REQUEST_LOCK(semaphore);
          {
            assert(semaphore->readRequestCount > 0);
            semaphore->readRequestCount--;

            #ifndef NDEBUG
              // debug trace code: remove pending information
              debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
            #endif /* not NDEBUG */
          }
          __SEMAPHORE_REQUEST_UNLOCK(semaphore);
          return FALSE;
        }
      }
      else
      {
        __SEMAPHORE_LOCK(semaphore,semaphoreLockType,DEBUG_FLAG_READ,"R");
      }
      {
        // check if re-lock with weaker access -> error
        if (semaphore->readWriteLockCount > 0)
        {
          #ifndef NDEBUG
            assert(semaphore->lockedByCount > 0);

            HALT_INTERNAL_ERROR("Thread '%s' (%s) try to lock semaphore '%s' with weaker access 'read' at %s, line %lu which was previously locked 'read/write' at %s, line %lu !",
                                Thread_getCurrentName(),
                                Thread_getCurrentIdString(),
                                semaphore->name,
                                fileName,
                                lineNb,
                                semaphore->lockedBy[semaphore->lockedByCount-1].fileName,
                                semaphore->lockedBy[semaphore->lockedByCount-1].lineNb
                               );
          #else /* NDEBUG */
            HALT_INTERNAL_ERROR("Thread '%s' (%s) try to lock semaphore with weaker 'read' access!",
                                Thread_getCurrentName(),
                                Thread_getCurrentIdString()
                               );
          #endif /* not NDEBUG */
        }

#if 0
//Note: read-lock requests will not wait for running read/write-locks, because aquiring a read/write lock is waiting for running read-locks (see below)
        // wait until no more other read/write-locks
        if      (timeout != WAIT_FOREVER)
        {
          while (semaphore->readWriteLockCount > 0)
          {
            __SEMAPHORE_WAIT_TIMEOUT(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"R",&semaphore->modified,&semaphore->lock,timeout,lockedFlag);
            if (!lockedFlag)
            {
              #ifndef NDEBUG
                assert(semaphore->lockedByCount > 0);
                semaphore->lockedByCount--;
              #endif /* not NDEBUG */
              __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R",semaphore->readLockCount);
              __SEMAPHORE_REQUEST_LOCK(semaphore);
              {
                assert(semaphore->readWriteRequestCount > 0);

                #ifndef NDEBUG
                  // debug trace code: remove pending information
                  debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
                #endif /* not NDEBUG */

                semaphore->readWriteRequestCount--;
              }
              __SEMAPHORE_REQUEST_UNLOCK(semaphore);
              return FALSE;
            }
          }
        }
        else
        {
          while (semaphore->readWriteLockCount > 0)
          {
            __SEMAPHORE_WAIT(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"R");
          }
        }
        assert(semaphore->readWriteLockCount == 0);
#endif /* 0 */
        assert((semaphore->lockType == SEMAPHORE_LOCK_TYPE_NONE) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ));

        // increment read-lock count and set lock type
        semaphore->readLockCount++;
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;

        // decrement read request counter atomically, store lock information
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          assert(semaphore->readRequestCount > 0);

          #ifndef NDEBUG
            // debug trace code: remove pending information, store locked information
            debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
            debugAddLockedThreadInfo(semaphore,semaphoreLockType,fileName,lineNb);
          #endif /* not NDEBUG */

          semaphore->readRequestCount--;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R",semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      /* request write lock
         Note: for a read/write lock semaphore is locked permanent
      */

      // increment read/write request counter atomically
      __SEMAPHORE_REQUEST_LOCK(semaphore);
      {
        semaphore->readWriteRequestCount++;

        #ifndef NDEBUG
          // debug trace code: store pending information
          debugAddPendingThreadInfo(semaphore,semaphoreLockType,fileName,lineNb);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_REQUEST_UNLOCK(semaphore);

      // write: aquire lock permanent
      if (timeout != WAIT_FOREVER)
      {
        __SEMAPHORE_LOCK_TIMEOUT(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"RW",timeout,lockedFlag);
        if (!lockedFlag)
        {
          __SEMAPHORE_REQUEST_LOCK(semaphore);
          {
            assert(semaphore->readWriteRequestCount > 0);
            semaphore->readWriteRequestCount--;

            #ifndef NDEBUG
              // debug trace code: remove pending information
              debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
            #endif /* not NDEBUG */

          }
          __SEMAPHORE_REQUEST_UNLOCK(semaphore);
          return FALSE;
        }
      }
      else
      {
        __SEMAPHORE_LOCK(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"RW");
      }

      // wait until no more read-locks
      if (timeout != WAIT_FOREVER)
      {
        while (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_WAIT_TIMEOUT(DEBUG_FLAG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock,timeout,lockedFlag);
          if (!lockedFlag)
          {
            #ifndef NDEBUG
              assert(semaphore->lockedByCount > 0);
              semaphore->lockedByCount--;
            #endif /* not NDEBUG */
            __SEMAPHORE_REQUEST_LOCK(semaphore);
            {
              assert(semaphore->readWriteRequestCount > 0);
              semaphore->readWriteRequestCount--;

              #ifndef NDEBUG
                // debug trace code: remove pending information
                debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
              #endif /* not NDEBUG */
            }
            __SEMAPHORE_REQUEST_UNLOCK(semaphore);
            return FALSE;
          }
        }
      }
      else
      {
        while (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_WAIT(DEBUG_FLAG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock);
        }
      }
      assert(semaphore->readLockCount == 0);
      assert((semaphore->lockType == SEMAPHORE_LOCK_TYPE_NONE) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

      // set/increment read/write-lock
      semaphore->readWriteLockCount++;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;

      // decrement read/write request counter atomically, store lock information
      __SEMAPHORE_REQUEST_LOCK(semaphore);
      {
        assert(semaphore->readWriteRequestCount > 0);
        semaphore->readWriteRequestCount--;

        #ifndef NDEBUG
          // debug trace code: remove pending information, store locked information
          debugRemovePendingThreadInfo(semaphore,fileName,lineNb);
          debugAddLockedThreadInfo(semaphore,semaphoreLockType,fileName,lineNb);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return lockedFlag;
}

/***********************************************************************\
* Name   : unlock
* Purpose: unlock semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL void unlock(Semaphore *semaphore)
#else /* not NDEBUG */
LOCAL void unlock(const char *fileName, ulong lineNb, Semaphore *semaphore)
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      // nothing to do
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
      __SEMAPHORE_LOCK(semaphore,semaphore->lockType,DEBUG_FLAG_READ,"R");
      {
        assert(semaphore->readLockCount > 0);
        assert(semaphore->readWriteLockCount == 0);

        // do one read-unlock
        semaphore->readLockCount--;
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

          // signal that read-lock count become 0
          __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"READ0 (unlock)",&semaphore->readLockZero);
        }

        #ifndef NDEBUG
          // debug lock code: remove lock information
          __SEMAPHORE_REQUEST_LOCK(semaphore);
          {
            debugRemoveLockedThreadInfo(semaphore,fileName,lineNb);
          }
          __SEMAPHORE_REQUEST_UNLOCK(semaphore);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R",semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);
      assert(semaphore->readWriteLockCount > 0);

      // do one read/write-unlock
      semaphore->readWriteLockCount--;
      if (semaphore->readWriteLockCount == 0)
      {
        // semaphore is free
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

        // signal modification
        __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);
      }

      #ifndef NDEBUG
        // debug lock code: remove lock information
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          debugRemoveLockedThreadInfo(semaphore,fileName,lineNb);
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* not NDEBUG */

      // unlock
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ_WRITE,"RW",semaphore->readWriteLockCount);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

/***********************************************************************\
* Name   : waitModified
* Purpose: wait until semaphore is modified
* Input  : semaphore - semaphore
*          timeout   - timeout [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL bool waitModified(Semaphore *semaphore,
                        long      timeout
                       )
#else /* not NDEBUG */
LOCAL bool waitModified(const char *fileName,
                        ulong      lineNb,
                        Semaphore  *semaphore,
                        long       timeout
                       )
#endif /* NDEBUG */
{
  uint savedReadWriteLockCount;
  bool lockedFlag;

  assert(semaphore != NULL);
  assert(semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE);

  lockedFlag = TRUE;

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      // nothing to do
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
      // semaphore is read-locked -> temporary revert own read-lock and wait for modification signal
      __SEMAPHORE_LOCK(semaphore,semaphore->lockType,DEBUG_FLAG_READ,"R");
      {
        assert(semaphore->readLockCount > 0);
        assert(semaphore->readWriteLockCount == 0);

        // temporary revert read-lock
        semaphore->readLockCount--;
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

          // signal that read-lock count become 0
          __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"READ0 (wait)",&semaphore->readLockZero);
        }
//TODO
//        __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);

        #ifndef NDEBUG
          // debug trace code: temporary remove locked information
          debugRemoveLockedThreadInfo(semaphore,fileName,lineNb);
        #endif /* not NDEBUG */

        if (timeout != WAIT_FOREVER)
        {
          // wait for modification with timeout
          __SEMAPHORE_WAIT_TIMEOUT(DEBUG_FLAG_READ,"MODIFIED",&semaphore->modified,&semaphore->lock,timeout,lockedFlag);

          // wait until there are no more write-locks
          while (semaphore->readWriteLockCount > 0)
          {
            __SEMAPHORE_WAIT_TIMEOUT(DEBUG_FLAG_READ,"W",&semaphore->modified,&semaphore->lock,timeout,lockedFlag);
          }
        }
        else
        {
          // wait for modification
          __SEMAPHORE_WAIT(DEBUG_FLAG_READ,"MODIFIED",&semaphore->modified,&semaphore->lock);

          // wait until there are no more write-locks
          while (semaphore->readWriteLockCount > 0)
          {
            __SEMAPHORE_WAIT(DEBUG_FLAG_READ,"W",&semaphore->modified,&semaphore->lock);
          }
        }

        // restore temporary reverted read-lock
        semaphore->readLockCount++;
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;

        #ifndef NDEBUG
          // debug trace code: revert temporary remove locked information
          debugAddLockedThreadInfo(semaphore,semaphore->lockType,fileName,lineNb);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R",semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      // semaphore is read/write-locked -> temporary revert own read/write-lock and wait for modification signal
      assert(semaphore->readLockCount == 0);
      assert(semaphore->readWriteLockCount > 0);

      // temporary revert write-lock (Note: no locking is required, because read/write-lock is already exclusive)
      savedReadWriteLockCount = semaphore->readWriteLockCount;
      semaphore->readWriteLockCount = 0;

      // semaphore is now free
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

      // signal modification
//TODO
//      __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);

      // wait for modification
      if (timeout != WAIT_FOREVER)
      {
        __SEMAPHORE_WAIT_TIMEOUT(DEBUG_FLAG_READ_WRITE,"MODIFIED",&semaphore->modified,&semaphore->lock,timeout,lockedFlag);
      }
      else
      {
        __SEMAPHORE_WAIT(DEBUG_FLAG_READ_WRITE,"MODIFIED",&semaphore->modified,&semaphore->lock);
      }

      // request write-lock
      __SEMAPHORE_REQUEST_LOCK(semaphore);
      {
        semaphore->readWriteRequestCount++;
      }
      __SEMAPHORE_REQUEST_UNLOCK(semaphore);

      // wait until no more read-locks
      if (timeout != WAIT_FOREVER)
      {
        while (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_WAIT_TIMEOUT(DEBUG_FLAG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock,timeout,lockedFlag);
        }
      }
      else
      {
        while (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_WAIT(DEBUG_FLAG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock);
        }
      }
      assert(semaphore->readLockCount == 0);

      // restore temporary reverted write-lock
      __SEMAPHORE_REQUEST_LOCK(semaphore);
      {
        assert(semaphore->readWriteRequestCount > 0);
        semaphore->readWriteRequestCount--;
        assert(semaphore->readWriteLockCount == 0);
        semaphore->readWriteLockCount = savedReadWriteLockCount;
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
      }
      __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return lockedFlag;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
bool Semaphore_init(Semaphore *semaphore)
#else /* not NDEBUG */
bool __Semaphore_init(const char *fileName,
                      ulong      lineNb,
                      const char *name,
                      Semaphore  *semaphore)
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  if (pthread_mutex_init(&semaphore->requestLock,NULL) != 0)
  {
    return FALSE;
  }
  semaphore->readRequestCount      = 0;
  semaphore->readWriteRequestCount = 0;

  pthread_mutexattr_init(&semaphore->lockAttributes);
  pthread_mutexattr_settype(&semaphore->lockAttributes,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&semaphore->lock,&semaphore->lockAttributes) != 0)
  {
    pthread_mutexattr_destroy(&semaphore->lockAttributes);
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->readLockZero,NULL) != 0)
  {
    pthread_mutex_destroy(&semaphore->lock);
    pthread_mutexattr_destroy(&semaphore->lockAttributes);
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->modified,NULL) != 0)
  {
    pthread_cond_destroy(&semaphore->readLockZero);
    pthread_mutex_destroy(&semaphore->lock);
    pthread_mutexattr_destroy(&semaphore->lockAttributes);
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  semaphore->lockType           = SEMAPHORE_LOCK_TYPE_NONE;
  semaphore->readLockCount      = 0;
  semaphore->readWriteLockCount = 0;
  semaphore->endFlag            = FALSE;

  #ifndef NDEBUG
    pthread_once(&debugSemaphoreInitFlag,debugInit);

    if (List_contains(&debugSemaphoreList,semaphore,CALLBACK(NULL,NULL)))
    {
      HALT_INTERNAL_ERROR_AT(fileName,lineNb,"Semaphore '%s' was already initialized at %s, line %lu!",
                             semaphore->name,
                             semaphore->fileName,
                             semaphore->lineNb
                            );
    }

    semaphore->fileName       = fileName;
    semaphore->lineNb         = lineNb;
    semaphore->name           = name;
//    memset(semaphore->lockedBy,0,sizeof(semaphore->lockedBy));
    semaphore->lockedByCount  = 0;
    semaphore->pendingByCount = 0;

    List_append(&debugSemaphoreList,semaphore);
  #endif /* not NDEBUG */

  return TRUE;
}

void Semaphore_done(Semaphore *semaphore)
{
  bool lockedFlag;

  assert(semaphore != NULL);

  #ifndef NDEBUG
    pthread_once(&debugSemaphoreInitFlag,debugInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
      debugCheckUnlocked(semaphore);
      List_remove(&debugSemaphoreList,semaphore);
  #endif /* not NDEBUG */

  // try to lock to avoid further usage
  __SEMAPHORE_LOCK_TIMEOUT(semaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE,DEBUG_FLAG_READ_WRITE,"D",NO_WAIT,lockedFlag);
  UNUSED_VARIABLE(lockedFlag);

  // free resources
  pthread_cond_destroy(&semaphore->modified);
  pthread_cond_destroy(&semaphore->readLockZero);
  pthread_mutex_destroy(&semaphore->lock);
  pthread_mutexattr_destroy(&semaphore->lockAttributes);
  pthread_mutex_destroy(&semaphore->requestLock);

  #ifndef NDEBUG
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
Semaphore *Semaphore_new(void)
#else /* not NDEBUG */
Semaphore *__Semaphore_new(const char *fileName,
                           ulong      lineNb,
                           const char *name
                          )
#endif /* NDEBUG */
{
  Semaphore *semaphore;

  semaphore = (Semaphore*)malloc(sizeof(Semaphore));
  if (semaphore != NULL)
  {
    #ifdef NDEBUG
      if (!Semaphore_init(semaphore))
      {
        free(semaphore);
        return NULL;
      }
    #else /* not NDEBUG */
      if (!__Semaphore_init(fileName,lineNb,name,semaphore))
      {
        free(semaphore);
        return NULL;
      }
    #endif /* NDEBUG */
  }
  else
  {
    return NULL;
  }

  return semaphore;
}

void Semaphore_delete(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  Semaphore_done(semaphore);
  free(semaphore);
}

#ifdef NDEBUG
bool Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType,
                    long               timeout
                   )
#else /* not NDEBUG */
bool __Semaphore_lock(const char         *fileName,
                      ulong              lineNb,
                      Semaphore          *semaphore,
                      SemaphoreLockTypes semaphoreLockType,
                      long               timeout
                     )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  #ifdef NDEBUG
    return lock(semaphore,semaphoreLockType,timeout);
  #else /* not NDEBUG */
    return lock(fileName,lineNb,semaphore,semaphoreLockType,timeout);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
void Semaphore_unlock(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_unlock(const char *fileName, ulong lineNb, Semaphore *semaphore)
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(fileName,lineNb,semaphore);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
void Semaphore_signalModified(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_signalModified(const char *fileName,
                                ulong      lineNb,
                                Semaphore  *semaphore
                               )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  assert(semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE);

  __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);
}

#ifdef NDEBUG
bool Semaphore_waitModified(Semaphore *semaphore,
                            long      timeout
                           )
#else /* not NDEBUG */
bool __Semaphore_waitModified(const char *fileName,
                              ulong      lineNb,
                              Semaphore  *semaphore,
                              long       timeout
                             )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  if (!semaphore->endFlag)
  {
    #ifdef NDEBUG
      return waitModified(semaphore,timeout);
    #else /* not NDEBUG */
      return waitModified(fileName,lineNb,semaphore,timeout);
    #endif /* NDEBUG */
  }
  else
  {
    return TRUE;
  }
}

bool Semaphore_isLockPending(Semaphore *semaphore, SemaphoreLockTypes semaphoreLockType)
{
  bool pendingFlag;

  assert(semaphore != NULL);

  pendingFlag = FALSE;
  if (!semaphore->endFlag)
  {
    switch (semaphoreLockType)
    {
      case SEMAPHORE_LOCK_TYPE_NONE:
        break;
      case SEMAPHORE_LOCK_TYPE_READ:
        pendingFlag = (semaphore->readRequestCount > 0) || (semaphore->readWriteRequestCount > 0);
        break;
      case SEMAPHORE_LOCK_TYPE_READ_WRITE:
        pendingFlag = (semaphore->readWriteRequestCount > 0);
        break;
    }
  }

  return pendingFlag;
}

void Semaphore_setEnd(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  // lock
  #ifdef NDEBUG
    lock(semaphore,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER);
  #else /* not NDEBUG */
    lock(__FILE__,__LINE__,semaphore,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER);
  #endif /* NDEBUG */

  // set end flag
  semaphore->endFlag = TRUE;

  // signal modification
  __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);

  // unlock
  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__FILE__,__LINE__,semaphore);
  #endif /* NDEBUG */
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : Semaphore_debugPrintInfo
* Purpose: print debug info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugPrintInfo(void)
{
  const Semaphore *semaphore;
  uint            i;

  pthread_mutex_lock(&debugConsoleLock);
  {
    fprintf(stderr,"Semaphore debug info:\n");
    LIST_ITERATE(&debugSemaphoreList,semaphore)
    {
      fprintf(stderr,"  '%s' (%s, line %lu):",semaphore->name,semaphore->fileName,semaphore->lineNb);
      switch (semaphore->lockType)
      {
        case SEMAPHORE_LOCK_TYPE_NONE:
          assert(semaphore->readLockCount == 0);
          assert(semaphore->readWriteLockCount == 0);
          fprintf(stderr,"\n");
          break;
        case SEMAPHORE_LOCK_TYPE_READ:
          assert(semaphore->readLockCount > 0);
          assert(semaphore->readWriteLockCount == 0);
          fprintf(stderr," locked %s (%d)\n", SEMAPHORE_LOCK_TYPE_NAMES[SEMAPHORE_LOCK_TYPE_READ],semaphore->readLockCount);
          for (i = 0; i < semaphore->lockedByCount; i++)
          {
            fprintf(stderr,
                    "    by thread '%s' (%s) at %s, line %lu\n",
                    Thread_getName(semaphore->lockedBy[i].threadId),
                    Thread_getIdString(semaphore->lockedBy[i].threadId),
                    semaphore->lockedBy[i].fileName,
                    semaphore->lockedBy[i].lineNb
                   );
          }
          for (i = 0; i < semaphore->pendingByCount; i++)
          {
            fprintf(stderr,
                    "    pending thread '%s' (%s) %s at %s, line %lu\n",
                    Thread_getName(semaphore->lockedBy[i].threadId),
                    Thread_getIdString(semaphore->pendingBy[i].threadId),
                    SEMAPHORE_LOCK_TYPE_NAMES[semaphore->pendingBy[i].lockType],
                    semaphore->pendingBy[i].fileName,
                    semaphore->pendingBy[i].lineNb
                   );
          }
          break;
        case SEMAPHORE_LOCK_TYPE_READ_WRITE:
          assert(semaphore->readLockCount == 0);
          assert(semaphore->readWriteLockCount > 0);
          fprintf(stderr," locked %s (%d)\n", SEMAPHORE_LOCK_TYPE_NAMES[SEMAPHORE_LOCK_TYPE_READ_WRITE],semaphore->readWriteLockCount);
          for (i = 0; i < semaphore->lockedByCount; i++)
          {
            fprintf(stderr,
                    "    by thread '%s' (%s) at %s, line %lu\n",
                    Thread_getName(semaphore->lockedBy[i].threadId),
                    Thread_getIdString(semaphore->lockedBy[i].threadId),
                    semaphore->lockedBy[i].fileName,
                    semaphore->lockedBy[i].lineNb
                   );
          }
          for (i = 0; i < semaphore->pendingByCount; i++)
          {
            fprintf(stderr,
                    "    pending thread '%s' (%s) %s at %s, line %lu\n",
                    Thread_getName(semaphore->lockedBy[i].threadId),
                    Thread_getIdString(semaphore->pendingBy[i].threadId),
                    SEMAPHORE_LOCK_TYPE_NAMES[semaphore->pendingBy[i].lockType],
                    semaphore->pendingBy[i].fileName,
                    semaphore->pendingBy[i].lineNb
                   );
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
    fprintf(stderr,"\n");
  }
  pthread_mutex_unlock(&debugConsoleLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
