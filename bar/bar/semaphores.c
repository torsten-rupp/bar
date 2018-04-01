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
#include <inttypes.h>
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
#define USE_ATOMIC_INCREMENT
#define _CHECK_FOR_DEADLOCK

/***************************** Constants *******************************/

#ifndef NDEBUG
  #define DEBUG_MAX_THREADS     64
  #define DEBUG_MAX_SEMAPHORES  256

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
        pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        \
        pthread_mutex_lock(&debugSemaphoreLock); \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
          __locked = (pthread_mutex_trylock(&semaphore->lock) == 0); \
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
        pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s (timeout %ldms)\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,timeout); \
        \
        pthread_mutex_lock(&debugSemaphoreLock); \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
        } \
        pthread_mutex_unlock(&debugSemaphoreLock); \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_nsec %= 1000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s (timeout %ldms)\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,timeout); \
        if (pthread_mutex_timedlock(&semaphore->lock,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
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
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
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
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_nsec %= 1000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition,type) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        switch (type) \
        { \
          case SEMAPHORE_SIGNAL_MODIFY_SINGLE: pthread_cond_signal(condition);    break; \
          case SEMAPHORE_SIGNAL_MODIFY_ALL:    pthread_cond_broadcast(condition); break; \
        } \
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
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_nsec %= 1000000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (WaitForSingleObject(semaphore,&__tp) != WAIT_OBJECT_0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
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
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text) \
      do \
      { \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
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
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/10000000L)+(timeout))/1000L; \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        else \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition,type) \
      do \
      { \
        UNUSED_VARIABLE(type); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_broadcast(condition); \
      } \
      while (0)
  #endif /* PLATFORM_... */

#else /* NDEBUG */

  #if   defined(PLATFORM_LINUX)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_mutex_lock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        UNUSED_VARIABLE(text); \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_nsec %= 1000000L; \
        \
        if (pthread_mutex_timedlock(&semaphore->lock,&__tp) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        if (pthread_mutex_trylock(&semaphore->lock) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_mutex_unlock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(debugFlag,text,condition,mutex) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_cond_wait(condition,mutex); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        struct timespec __tp; \
        \
        UNUSED_VARIABLE(text); \
        \
        clock_gettime(CLOCK_REALTIME,&__tp); \
        __tp.tv_nsec = __tp.tv_nsec+((timeout)%1000L)*1000000L; \
        __tp.tv_sec  = __tp.tv_sec+((__tp.tv_nsec/1000000L)+(timeout))/1000L; \
        __tp.tv_nsec %= 1000000L; \
        \
        if (pthread_cond_timedwait(condition,mutex,&__tp) != 0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition,type) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        UNUSED_VARIABLE(type); \
        \
        pthread_cond_broadcast(condition); \
      } \
      while (0)
  #elif defined(PLATFORM_WINDOWS)
    #define __SEMAPHORE_LOCK(semaphore,debugFlag,text) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_mutex_lock(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK_TIMEOUT(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        if (WaitForSingleObject(semaphore,timeout) != WAIT_OBJECT_0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        if (WaitForSingleObject(semaphore,0) != WAIT_OBJECT_0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_UNLOCK(semaphore,debugFlag,text) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_mutex_unlock(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        pthread_cond_wait(condition,semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT_TIMEOUT(semaphore,debugFlag,text,condition,timeout,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        \
        if (pthread_cond_timedwait(condition,semaphore,timeout) == ETIMEDOUT) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(debugFlag,text,condition,type) \
      do \
      { \
        UNUSED_VARIABLE(text); \
        UNUSED_VARIABLE(type); \
        \
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
* Name   : debugSemaphoreInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugSemaphoreInit(void)
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
* Name   : debugSemaphoreSignalHandler
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
* Name   : debugAddThreadInfo
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

LOCAL_INLINE void debugAddThreadInfo(__SemaphoreThreadInfo threadInfos[],
                                     uint                  *threadInfoCount,
                                     SemaphoreLockTypes    lockType,
                                     const char            *fileName,
                                     ulong                 lineNb
                                    )
{
  assert(threadInfos != NULL);
  assert(threadInfoCount != NULL);
  assert((*threadInfoCount) < __SEMAPHORE_MAX_THREAD_INFO);

  threadInfos[(*threadInfoCount)].threadId = Thread_getCurrentId();
  threadInfos[(*threadInfoCount)].lockType = lockType;
  threadInfos[(*threadInfoCount)].fileName = fileName;
  threadInfos[(*threadInfoCount)].lineNb   = lineNb;
  (*threadInfoCount)++;

#if 0
      fprintf(stderr,
              "DEBUG WARNING: too many thread locks for semaphore '%s' at %s, line %lu (max. %lu)!\n",
              semaphore->name,
              fileName,
              lineNb,
              (ulong)SIZE_OF_ARRAY(semaphore->lockedBy)
             );
#endif
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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugAddThreadInfo(semaphore->lockedBy,&semaphore->lockedByCount,lockType,fileName,lineNb);
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugAddThreadInfo(semaphore->pendingBy,&semaphore->pendingByCount,lockType,fileName,lineNb);
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
}

/***********************************************************************\
* Name   : debugRemoveThreadInfo
* Purpose: remove thread from thread info array
* Input  : threadInfos     - thread info array
*          threadInfoCount - thread info count
*          type            - type text
*          semaphoreName   - semaphore name
*          fileName        - file name
*          lineNb          - line number
* Output : threadInfoCount - new thread info count
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugRemoveThreadInfo(__SemaphoreThreadInfo threadInfos[],
                                        uint                  *threadInfoCount,
                                        const char            *type,
                                        const char            *semaphoreName,
                                        const char            *fileName,
                                        ulong                 lineNb
                                       )
{
  int i;

  assert(threadInfos != NULL);
  assert(threadInfoCount != NULL);
  assert((*threadInfoCount) > 0);

  i = (int)(*threadInfoCount)-1;
  while (   (i >= 0)
         && !Thread_isCurrentThread(threadInfos[i].threadId)
        )
  {
    i--;
  }

  if (i < 0)
  {
    Semaphore_debugPrintInfo();
    HALT_INTERNAL_ERROR("Thread '%s' (%s) try to unlock not %s semaphore '%s' at %s, line %lu!",
                        Thread_getCurrentName(),
                        Thread_getCurrentIdString(),
                        type,
                        semaphoreName,
                        fileName,
                        lineNb
                       );
  }

  threadInfos[i] = threadInfos[(*threadInfoCount)-1];
  memset(&threadInfos[(*threadInfoCount)-1],0,sizeof(__SemaphoreThreadInfo));
  (*threadInfoCount)--;
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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugRemoveThreadInfo(semaphore->lockedBy,&semaphore->lockedByCount,"locked",semaphore->name,fileName,lineNb);
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugRemoveThreadInfo(semaphore->pendingBy,&semaphore->pendingByCount,"pending",semaphore->name,fileName,lineNb);
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
}

#if CHECK_FOR_DEADLOCK

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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  for (i = 0; i < semaphore->pendingByCount; i++)
  {
    if (Thread_equalThreads(semaphore->pendingBy[i].threadId,threadId)) return &semaphore->pendingBy[i];
  }

  return NULL;
}

/***********************************************************************\
* Name   : debugSemaphoreSetInit
* Purpose: init semaphore set
* Input  : semaphore         - semaphore set
*          semaphoreCount    - number of semaphores variable
*          addSemaphore      - initial semaphore
* Output : semaphoreCount - new number of semaphores
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugSemaphoreSetInit(const Semaphore *semaphores[], uint *semaphoreCount, const Semaphore *semaphore)
{
  semaphores[0]     = semaphore;
  (*semaphoreCount) = 1;
}

/***********************************************************************\
* Name   : debugSemaphoreSetContains
* Purpose: check if semaphore is contained in semaphore set
* Input  : semaphore         - semaphore set
*          semaphoreCount    - number of semaphores
*          semaphore         - semaphore to check
* Output : -
* Return : TRUE iff semaphore is contained in set
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool debugSemaphoreSetContains(const Semaphore *semaphores[], uint semaphoreCount, const Semaphore *semaphore)
{
  uint i;

  i = 0;
  while ((i < semaphoreCount) && (semaphores[i] != semaphore))
  {
    i++;
  }
  
  return i < semaphoreCount;
}

/***********************************************************************\
* Name   : debugSemaphoreSetAdd
* Purpose: add semaphore to semaphore set (avoid duplicates)
* Input  : semaphore         - semaphore set
*          semaphoreCount    - number of semaphores variable
*          addSemaphore      - semaphore to add
* Output : semaphoreCount - new number of semaphores
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugSemaphoreSetAdd(const Semaphore *semaphores[], uint *semaphoreCount, const Semaphore *semaphore)
{
  uint n;

  n = (*semaphoreCount);
  if (!debugSemaphoreSetContains(semaphores,n,semaphore))
  {
    assert(n < DEBUG_MAX_SEMAPHORES);
    semaphores[n] = semaphore;
    (*semaphoreCount) = n+1;
  }
}

#endif /* CHECK_FOR_DEADLOCK */

/***********************************************************************\
* Name   : debugCheckForDeadLock
* Purpose: check for dead lock
* Input  : semaphore - semaphore
*          lockType  - lock types; see SemaphoreLockTypes
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugCheckForDeadLock(Semaphore          *semaphore,
                                 SemaphoreLockTypes lockType,
                                 const char         *fileName,
                                 ulong              lineNb
                                )
{
#if CHECK_FOR_DEADLOCK
  uint                        i,j,k;
  const Semaphore             *otherSemaphore;
  const __SemaphoreThreadInfo *pendingInfo;
  const Semaphore             *lockedSemaphores[DEBUG_MAX_SEMAPHORES];
  uint                        lockedSemaphoreCount;
  const Semaphore             *checkSemaphores[DEBUG_MAX_SEMAPHORES];
  uint                        checkSemaphoreCount;
  const Semaphore             *lockedSemaphore;
  const Semaphore             *checkSemaphore;

  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  UNUSED_VARIABLE(lockType);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    // get all locked semaphores of current thread
    lockedSemaphoreCount = 0;
    LIST_ITERATE(&debugSemaphoreList,otherSemaphore)
    {
      if (getLockedByThreadInfo(otherSemaphore,Thread_getCurrentId()) != NULL)
      {
        assert(lockedSemaphoreCount < DEBUG_MAX_SEMAPHORES);
        lockedSemaphores[lockedSemaphoreCount] = otherSemaphore; lockedSemaphoreCount++;
      }
    }

    if (!debugSemaphoreSetContains(lockedSemaphores,lockedSemaphoreCount,semaphore))
    {
      // initial check requested semaphore
      debugSemaphoreSetInit(checkSemaphores,&checkSemaphoreCount,semaphore);

      /* check
         - if some thread which locked check semaphore is pending for a semaphore locked by current thread -> dead lock
         - all semaphores, too, some thread which locked current check semaphore is pending
      */
      i = 0;
      while (i < checkSemaphoreCount)
      {
        assert(i < DEBUG_MAX_SEMAPHORES);
        checkSemaphore = checkSemaphores[i];

        for (j = 0; j < lockedSemaphoreCount; j++)
        {
          lockedSemaphore = lockedSemaphores[j];

          // check if some thread which locked check semaphore is pending for a semaphore locked by current thread
          for (k = 0; k < checkSemaphore->lockedByCount; k++)
          {
            pendingInfo = getPendingByThreadInfo(lockedSemaphore,checkSemaphore->lockedBy[k].threadId);
            if (pendingInfo != NULL)
            {
              // DEAD LOCK!
              Semaphore_debugPrintInfo();
              HALT_INTERNAL_ERROR_AT(fileName,lineNb,"DEAD LOCK!");
            }

            // check all semaphores, too, some thread which locked current check semaphore is pending
            LIST_ITERATE(&debugSemaphoreList,otherSemaphore)
            {
              pendingInfo = getPendingByThreadInfo(otherSemaphore,checkSemaphore->lockedBy[k].threadId);
              if (pendingInfo != NULL)
              {
                debugSemaphoreSetAdd(checkSemaphores,&checkSemaphoreCount,otherSemaphore);
              }
            }
          }
        }

        i++;
      }
    }
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
#else
  UNUSED_VARIABLE(semaphore);
  UNUSED_VARIABLE(lockType);
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(lineNb);
#endif /* CHECK_FOR_DEADLOCK */
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
LOCAL bool lock(const char         *__fileName__,
                ulong              __lineNb__,
                Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType,
                long               timeout
               )
#endif /* NDEBUG */
{
  bool lockedFlag;

  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);
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
      #ifdef USE_ATOMIC_INCREMENT
        ATOMIC_INCREMENT(semaphore->readRequestCount);
      #else /* not USE_ATOMIC_INCREMENT */
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          semaphore->readRequestCount++;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* USE_ATOMIC_INCREMENT */
      assert(semaphore->readRequestCount > 0);

      #ifndef NDEBUG
        // debug trace code: store pending information
        debugAddPendingThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #endif /* not NDEBUG */

      // read: aquire lock temporary and increment read-lock counter
      if (timeout != WAIT_FOREVER)
      {
        __SEMAPHORE_LOCK_TIMEOUT(semaphore,semaphoreLockType,DEBUG_FLAG_READ,"R",timeout,lockedFlag);
        if (!lockedFlag)
        {
          #ifdef USE_ATOMIC_INCREMENT
            assert(semaphore->readRequestCount > 0);
            ATOMIC_DECREMENT(semaphore->readRequestCount);
          #else /* not USE_ATOMIC_INCREMENT */
            __SEMAPHORE_REQUEST_LOCK(semaphore);
            {
              assert(semaphore->readRequestCount > 0);
              semaphore->readRequestCount--;
            }
            __SEMAPHORE_REQUEST_UNLOCK(semaphore);
          #endif /* USE_ATOMIC_INCREMENT */

          #ifndef NDEBUG
            // debug trace code: remove pending information
            debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
          #endif /* not NDEBUG */

          return FALSE;
        }
      }
      else
      {
        __SEMAPHORE_LOCK(semaphore,semaphoreLockType,DEBUG_FLAG_READ,"R");
      }
      {
#if 0
// Note: allow weaker access -> if already aquired read/write-lock handle read-lock like a read/write-lock
        // check if re-lock with weaker access -> error
        if (semaphore->readWriteLockCount > 0)
        {
          #ifndef NDEBUG
            pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

            pthread_mutex_lock(&debugSemaphoreLock);
            {
              assert(semaphore->lockedByCount > 0);

              HALT_INTERNAL_ERROR("Thread '%s' (%s) try to lock semaphore '%s' with weaker access 'read' at %s, line %lu which was previously locked 'read/write' at %s, line %lu !",
                                  Thread_getCurrentName(),
                                  Thread_getCurrentIdString(),
                                  semaphore->name,
                                  __fileName__,
                                  __lineNb__,
                                  semaphore->lockedBy[semaphore->lockedByCount-1].fileName,
                                  semaphore->lockedBy[semaphore->lockedByCount-1].lineNb
                                 );
            }
            pthread_mutex_unlock(&debugSemaphoreLock);
          #else /* NDEBUG */
            HALT_INTERNAL_ERROR("Thread '%s' (%s) try to lock semaphore with weaker 'read' access!",
                                Thread_getCurrentName(),
                                Thread_getCurrentIdString()
                               );
          #endif /* not NDEBUG */
        }
#endif

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
              __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");

              #ifdef USE_ATOMIC_INCREMENT
                assert(semaphore->readWriteRequestCount > 0);
                ATOMIC_DECREMENT(semaphore->readWriteRequestCount);
              #else /* not USE_ATOMIC_INCREMENT */
                __SEMAPHORE_REQUEST_LOCK(semaphore);
                {
                  assert(semaphore->readWriteRequestCount > 0);
                  semaphore->readWriteRequestCount--;
                }
                __SEMAPHORE_REQUEST_UNLOCK(semaphore);
              #endif /* USE_ATOMIC_INCREMENT */

              #ifndef NDEBUG
                // debug trace code: remove pending information
                debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
              #endif /* not NDEBUG */

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

        // increment lock count and set lock type
        if (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          // if already has aquired read/write-lock increment read/write-lock count
          semaphore->readWriteLockCount++;
        }
        else
        {
          // increment read-lock count and set lock type
          semaphore->readLockCount++;
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;
        }

        // decrement read request counter atomically, store lock information
        #ifdef USE_ATOMIC_INCREMENT
          assert(semaphore->readRequestCount > 0);
          ATOMIC_DECREMENT(semaphore->readRequestCount);
        #else /* not USE_ATOMIC_INCREMENT */
          __SEMAPHORE_REQUEST_LOCK(semaphore);
          {
            assert(semaphore->readRequestCount > 0);
            semaphore->readRequestCount--;
          }
          __SEMAPHORE_REQUEST_UNLOCK(semaphore);
        #endif /* USE_ATOMIC_INCREMENT */
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");

      #ifndef NDEBUG
        // debug trace code: remove pending information, store locked information
        debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
        debugAddLockedThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #endif /* not NDEBUG */
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      /* request write lock
         Note: for a read/write lock semaphore is locked permanent
      */

      // increment read/write request counter atomically
      #ifdef USE_ATOMIC_INCREMENT
        ATOMIC_INCREMENT(semaphore->readWriteRequestCount);
      #else /* not USE_ATOMIC_INCREMENT */
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          semaphore->readWriteRequestCount++;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* USE_ATOMIC_INCREMENT */
      assert(semaphore->readWriteRequestCount > 0);

      #ifndef NDEBUG
        // debug trace code: store pending information
        debugAddPendingThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #endif /* not NDEBUG */

      // write: aquire permanent lock
      if (timeout != WAIT_FOREVER)
      {
        __SEMAPHORE_LOCK_TIMEOUT(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"RW",timeout,lockedFlag);
        if (!lockedFlag)
        {
          #ifdef USE_ATOMIC_INCREMENT
            assert(semaphore->readWriteRequestCount > 0);
            ATOMIC_DECREMENT(semaphore->readWriteRequestCount);
          #else /* not USE_ATOMIC_INCREMENT */
            __SEMAPHORE_REQUEST_LOCK(semaphore);
            {
              assert(semaphore->readWriteRequestCount > 0);
              semaphore->readWriteRequestCount--;
            }
            __SEMAPHORE_REQUEST_UNLOCK(semaphore);
          #endif /* USE_ATOMIC_INCREMENT */

          #ifndef NDEBUG
            // debug trace code: remove pending information
            debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
          #endif /* not NDEBUG */

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
            __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ_WRITE,"RW");

            #ifdef USE_ATOMIC_INCREMENT
              assert(semaphore->readWriteRequestCount > 0);
              ATOMIC_DECREMENT(semaphore->readWriteRequestCount);
            #else /* not USE_ATOMIC_INCREMENT */
              __SEMAPHORE_REQUEST_LOCK(semaphore);
              {
                assert(semaphore->readWriteRequestCount > 0);
                semaphore->readWriteRequestCount--;
              }
              __SEMAPHORE_REQUEST_UNLOCK(semaphore);
            #endif /* USE_ATOMIC_INCREMENT */

            #ifndef NDEBUG
              // debug trace code: remove pending information
              debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
            #endif /* not NDEBUG */

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
      #ifdef USE_ATOMIC_INCREMENT
        assert(semaphore->readWriteRequestCount > 0);
        ATOMIC_DECREMENT(semaphore->readWriteRequestCount);
      #else /* not USE_ATOMIC_INCREMENT */
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          assert(semaphore->readWriteRequestCount > 0);
          semaphore->readWriteRequestCount--;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* USE_ATOMIC_INCREMENT */

      #ifndef NDEBUG
        // debug trace code: remove pending information, store locked information
        debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
        debugAddLockedThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #endif /* not NDEBUG */
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
LOCAL void unlock(const char *__fileName__,
                  ulong      __lineNb__,
                  Semaphore  *semaphore
                 )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

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
        #ifdef USE_ATOMIC_INCREMENT
        #else /* not USE_ATOMIC_INCREMENT */
        #endif /* USE_ATOMIC_INCREMENT */
        semaphore->readLockCount--;
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

          // signal that read-lock count become 0
          __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"READ0 (unlock)",&semaphore->readLockZero,SEMAPHORE_SIGNAL_MODIFY_SINGLE);
        }
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
      #ifndef NDEBUG
        // debug lock code: remove lock information
        debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
      #endif /* not NDEBUG */
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);
      assert(semaphore->readWriteLockCount > 0);

      // do one read/write-unlock
      #ifdef USE_ATOMIC_INCREMENT
      #else /* not USE_ATOMIC_INCREMENT */
      #endif /* USE_ATOMIC_INCREMENT */
      semaphore->readWriteLockCount--;
      if (semaphore->readWriteLockCount == 0)
      {
        // semaphore is free
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

        // signal modification
        __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_SINGLE);
      }

      // unlock
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ_WRITE,"RW");

      #ifndef NDEBUG
        // debug lock code: remove lock information
        debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
      #endif /* not NDEBUG */
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
* Return : TRUE if modified
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL bool waitModified(Semaphore *semaphore,
                        long      timeout
                       )
#else /* not NDEBUG */
LOCAL bool waitModified(const char *__fileName__,
                        ulong      __lineNb__,
                        Semaphore  *semaphore,
                        long       timeout
                       )
#endif /* NDEBUG */
{
  uint savedReadWriteLockCount;
  bool lockedFlag;

  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);
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
          __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"READ0 (wait)",&semaphore->readLockZero,SEMAPHORE_SIGNAL_MODIFY_SINGLE);
        }
//TODO
//        __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified);

        #ifndef NDEBUG
          // debug trace code: temporary remove locked information
          debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
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
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");

      #ifndef NDEBUG
        // debug trace code: revert temporary remove locked information
        debugAddLockedThreadInfo(semaphore,semaphore->lockType,__fileName__,__lineNb__);
      #endif /* not NDEBUG */
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
//???? richtig?
__SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_SINGLE);

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
      #ifdef USE_ATOMIC_INCREMENT
        ATOMIC_INCREMENT(semaphore->readWriteRequestCount);
      #else /* not USE_ATOMIC_INCREMENT */
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          semaphore->readWriteRequestCount++;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* USE_ATOMIC_INCREMENT */
      assert(semaphore->readWriteRequestCount > 0);

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
      #ifdef USE_ATOMIC_INCREMENT
        assert(semaphore->readWriteRequestCount > 0);
        ATOMIC_DECREMENT(semaphore->readWriteRequestCount);
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          assert(semaphore->readWriteLockCount == 0);
          semaphore->readWriteLockCount += savedReadWriteLockCount;
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #else /* not USE_ATOMIC_INCREMENT */
        __SEMAPHORE_REQUEST_LOCK(semaphore);
        {
          assert(semaphore->readWriteRequestCount > 0);
          semaphore->readWriteRequestCount--;
          assert(semaphore->readWriteLockCount == 0);
          semaphore->readWriteLockCount += savedReadWriteLockCount;
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
        }
        __SEMAPHORE_REQUEST_UNLOCK(semaphore);
      #endif /* USE_ATOMIC_INCREMENT */
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
* Name   : waitCondition
* Purpose: wait for contition
* Input  : semaphore - semaphore
*          timeout   - timeout [ms]
* Output : -
* Return : TRUE on condition, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL bool waitCondition(SemaphoreCondition *condition,
                         Semaphore          *semaphore,
                         long               timeout
                        )
#else /* not NDEBUG */
LOCAL bool waitCondition(const char         *__fileName__,
                         ulong              __lineNb__,
                         SemaphoreCondition *condition,
                         Semaphore          *semaphore,
                         long               timeout
                        )
#endif /* NDEBUG */
{
//TODO: still not implemented
#ifndef NDEBUG
UNUSED_VARIABLE(__fileName__);
UNUSED_VARIABLE(__lineNb__);
#endif
UNUSED_VARIABLE(condition);
UNUSED_VARIABLE(semaphore);
UNUSED_VARIABLE(timeout);
  return FALSE;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
bool Semaphore_init(Semaphore *semaphore, SemaphoreTypes semaphoreType)
#else /* not NDEBUG */
bool __Semaphore_init(const char     *__fileName__,
                      ulong          __lineNb__,
                      const char     *name,
                      Semaphore      *semaphore,
                      SemaphoreTypes semaphoreType
                     )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  semaphore->type                  = semaphoreType;
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

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(semaphore,sizeof(Semaphore));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,semaphore,sizeof(Semaphore));
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    // add to semaphore list
    pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
      if (List_contains(&debugSemaphoreList,semaphore,CALLBACK(NULL,NULL)))
      {
        HALT_INTERNAL_ERROR_AT(__fileName__,__lineNb__,"Semaphore '%s' was already initialized at %s, line %lu!",
                               semaphore->name,
                               semaphore->fileName,
                               semaphore->lineNb
                              );
      }

      semaphore->fileName       = __fileName__;
      semaphore->lineNb         = __lineNb__;
      semaphore->name           = name;
      memset(semaphore->pendingBy,0,sizeof(semaphore->pendingBy));
      semaphore->pendingByCount = 0;
      memset(semaphore->lockedBy,0,sizeof(semaphore->lockedBy));
      semaphore->lockedByCount  = 0;

      List_append(&debugSemaphoreList,semaphore);
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
  #endif /* not NDEBUG */

  return TRUE;
}

#ifdef NDEBUG
void Semaphore_done(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_done(const char *__fileName__,
                      ulong      __lineNb__,
                      Semaphore  *semaphore
                     )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  #ifndef NDEBUG
    // removed from semaphore list
    pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
      List_remove(&debugSemaphoreList,semaphore);
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
  #endif /* not NDEBUG */

  // try to lock to avoid further usage
//TODO: useful to lock before destroy?
//  pthread_mutex_trylock(&semaphore->lock);

  // check if still locked
  #ifndef NDEBUG
    pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
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
    pthread_mutex_unlock(&debugSemaphoreLock);
  #endif /* not NDEBUG */

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(semaphore,sizeof(Semaphore));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,semaphore,sizeof(Semaphore));
  #endif /* NDEBUG */

  // free resources
  pthread_cond_destroy(&semaphore->modified);
  pthread_cond_destroy(&semaphore->readLockZero);
  pthread_mutex_destroy(&semaphore->lock);
  pthread_mutexattr_destroy(&semaphore->lockAttributes);
  pthread_mutex_destroy(&semaphore->requestLock);
}

#ifdef NDEBUG
Semaphore *Semaphore_new(SemaphoreTypes semaphoreType)
#else /* not NDEBUG */
Semaphore *__Semaphore_new(const char     *__fileName__,
                           ulong          __lineNb__,
                           const char     *name,
                           SemaphoreTypes semaphoreType
                          )
#endif /* NDEBUG */
{
  Semaphore *semaphore;

  semaphore = (Semaphore*)malloc(sizeof(Semaphore));
  if (semaphore != NULL)
  {
    #ifdef NDEBUG
      if (!Semaphore_init(semaphore,semaphoreType))
      {
        free(semaphore);
        return NULL;
      }
    #else /* not NDEBUG */
      if (!__Semaphore_init(__fileName__,__lineNb__,name,semaphore,semaphoreType))
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

#ifdef NDEBUG
void Semaphore_delete(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_delete(const char *__fileName__,
                        ulong      __lineNb__,
                        Semaphore  *semaphore
                       )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  #ifdef NDEBUG
    Semaphore_done(semaphore);
  #else /* not NDEBUG */
    __Semaphore_done(__fileName__,__lineNb__,semaphore);
  #endif /* NDEBUG */
  free(semaphore);
}

#ifdef NDEBUG
bool Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType,
                    long               timeout
                   )
#else /* not NDEBUG */
bool __Semaphore_lock(const char         *__fileName__,
                      ulong              __lineNb__,
                      Semaphore          *semaphore,
                      SemaphoreLockTypes semaphoreLockType,
                      long               timeout
                     )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  #ifdef NDEBUG
    return lock(semaphore,semaphoreLockType,timeout);
  #else /* not NDEBUG */
    return lock(__fileName__,__lineNb__,semaphore,semaphoreLockType,timeout);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
void Semaphore_unlock(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_unlock(const char *__fileName__,
                        ulong      __lineNb__,
                        Semaphore  *semaphore
                       )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__fileName__,__lineNb__,semaphore);
  #endif /* NDEBUG */
}

#ifndef NDEBUG
bool Semaphore_isOwned(const Semaphore *semaphore)
{
  bool     isOwned;
  ThreadId currentThreadId;
  uint     i;

  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  isOwned = FALSE;

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  currentThreadId = Thread_getCurrentId();

  pthread_mutex_lock(&debugSemaphoreLock);
  {
  for (i = 0; i < semaphore->lockedByCount; i++)
  {
    if (Thread_equalThreads(semaphore->lockedBy[i].threadId,currentThreadId))
    {
      isOwned = TRUE;
      break;
    }
  }
if (!isOwned)
{
fprintf(stderr,"%s, %d: current=%s\n",__FILE__,__LINE__,Thread_getIdString(currentThreadId));
  for (i = 0; i < semaphore->lockedByCount; i++)
  {
  fprintf(stderr,"%s, %d:  %d: %s => %d\n",__FILE__,__LINE__,i,
  Thread_getIdString(semaphore->lockedBy[i].threadId),
  Thread_equalThreads(semaphore->lockedBy[i].threadId,currentThreadId)
  );
  }
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
}

  return isOwned;
}
#endif /* not NDEBUG */

void Semaphore_signalModified(Semaphore *semaphore, SemaphoreSignalModifyTypes type)
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,type);
}

#ifdef NDEBUG
bool Semaphore_waitModified(Semaphore *semaphore,
                            long      timeout
                           )
#else /* not NDEBUG */
bool __Semaphore_waitModified(const char *__fileName__,
                              ulong      __lineNb__,
                              Semaphore  *semaphore,
                              long       timeout
                             )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  if (!semaphore->endFlag)
  {
    #ifdef NDEBUG
      return waitModified(semaphore,timeout);
    #else /* not NDEBUG */
      return waitModified(__fileName__,__lineNb__,semaphore,timeout);
    #endif /* NDEBUG */
  }
  else
  {
    return TRUE;
  }
}

#ifdef NDEBUG
bool Semaphore_waitCondition(SemaphoreCondition *condition,
                             Semaphore          *semaphore,
                             long               timeout
                            )
#else /* not NDEBUG */
bool __Semaphore_waitCondition(const char         *__fileName__,
                               ulong              __lineNb__,
                               SemaphoreCondition *condition,
                               Semaphore          *semaphore,
                               long               timeout
                              )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(condition);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  if (!semaphore->endFlag)
  {
    #ifdef NDEBUG
      return waitCondition(condition,semaphore,timeout);
    #else /* not NDEBUG */
      return waitCondition(__fileName__,__lineNb__,condition,semaphore,timeout);
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
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  pendingFlag = FALSE;
  if (!semaphore->endFlag)
  {
//TODO: lock is not really required, because check for pending may return false
    __SEMAPHORE_REQUEST_LOCK(semaphore);
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
    __SEMAPHORE_REQUEST_UNLOCK(semaphore);
  }

  return pendingFlag;
}

void Semaphore_setEnd(Semaphore *semaphore)
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  // lock
  #ifdef NDEBUG
    lock(semaphore,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER);
  #else /* not NDEBUG */
    lock(__FILE__,__LINE__,semaphore,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER);
  #endif /* NDEBUG */

  // set end flag
  semaphore->endFlag = TRUE;

  // signal modification
  __SEMAPHORE_SIGNAL(DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // unlock
  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__FILE__,__LINE__,semaphore);
  #endif /* NDEBUG */
}

#ifndef NDEBUG
void Semaphore_debugPrintInfo(void)
{
  const Semaphore *semaphore;
  char            s[64+1];
  uint            i;

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugConsoleLock);
  {
    fprintf(stderr,"Semaphore debug info:\n");
    pthread_mutex_lock(&debugSemaphoreLock);
    {
      LIST_ITERATE(&debugSemaphoreList,semaphore)
      {
        snprintf(s,sizeof(s),"'%s'",semaphore->name);
        fprintf(stderr,"  %-64s 0x%016"PRIxPTR" (%s, line %lu):",s,(uintptr_t)semaphore,semaphore->fileName,semaphore->lineNb);
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
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
    fprintf(stderr,"\n");
  }
  pthread_mutex_unlock(&debugConsoleLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
