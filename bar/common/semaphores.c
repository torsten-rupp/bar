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

//TODO: use Windows WaitForSingleObject?
//#if   defined(PLATFORM_LINUX)
#if 1
  #include <pthread.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
  #include <pthread.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/lists.h"
#include "common/threads.h"
#include "common/misc.h"

#include "semaphores.h"

/****************** Conditional compilation switches *******************/
#define _USE_ATOMIC_INCREMENT
#define _CHECK_FOR_DEADLOCK
#define _DEBUG_SHOW_LAST_INFO

/***************************** Constants *******************************/

#ifndef NDEBUG
  #define DEBUG_MAX_THREADS     64
  #define DEBUG_MAX_SEMAPHORES  256

  #define DEBUG_FLAG_READ       FALSE
  #define DEBUG_FLAG_READ_WRITE FALSE
  #define DEBUG_FLAG_MODIFIED   FALSE

  const char *SEMAPHORE_LOCK_TYPE_NAMES[] =
  {
    [SEMAPHORE_LOCK_TYPE_NONE]       = "-",
    [SEMAPHORE_LOCK_TYPE_READ]       = "R",
    [SEMAPHORE_LOCK_TYPE_READ_WRITE] = "RW"
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
//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    LOCAL pthread_mutexattr_t debugSemaphoreLockAttribute;
    LOCAL pthread_mutex_t     debugSemaphoreLock;
  #elif defined(PLATFORM_WINDOWS)
    LOCAL HANDLE              debugSemaphoreLock;
  #endif /* PLATFORM_... */
  LOCAL ThreadId            debugSemaphoreThreadId;
  LOCAL DebugSemaphoreList  debugSemaphoreList;
  #ifdef HAVE_SIGQUIT
    LOCAL void                (*debugSignalQuitPrevHandler)(int);
  #endif /* HAVE_SIGQUIT */

  LOCAL uint64              startCycleCount;


  LOCAL const Semaphore     *debugTraceSemaphore = NULL;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

//TODO: use Windows WaitForSingleObject?
//#if   defined(PLATFORM_LINUX)
#if 1
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
  #define VERIFY_COUNTERS(semaphore) do { assert((semaphore)->debug.lockedByCount == ((semaphore)->readLockCount+(semaphore)->readWriteLockCount+(semaphore)->waitModifiedCount)); } while (0)

//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text,timeout,...) \
      do \
      { \
        struct timespec __timespec; \
        bool            __locked; \
        \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s (timeout %ldms)\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text,timeout); \
        pthread_mutex_lock(&debugSemaphoreLock); \
        { \
          debugCheckForDeadLock(semaphore,lockType,__FILE__,__LINE__); \
        } \
        pthread_mutex_unlock(&debugSemaphoreLock); \
        if (timeout != WAIT_FOREVER) \
        { \
          getTimeSpec(&__timespec,timeout); \
          __locked = (pthread_mutex_timedlock(&semaphore->lock,&__timespec) == 0); \
        } \
        else \
        { \
          __locked = (pthread_mutex_lock(&semaphore->lock) == 0); \
        } \
        if (__locked) \
        { \
          __VA_ARGS__; \
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
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (pthread_mutex_trylock(&semaphore->lock) == 0) \
        { \
          lockedFlag = TRUE; \
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
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_mutex_unlock(&semaphore->lock); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex,timeout,...) \
      do \
      { \
        struct timespec __timespec; \
        \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (timeout != WAIT_FOREVER) \
        { \
          getTimeSpec(&__timespec,timeout); \
          if (pthread_cond_timedwait(condition,mutex,&__timespec) == 0) \
          { \
            __VA_ARGS__; \
          } \
        } \
        else \
        { \
          if (pthread_cond_wait(condition,mutex) == 0) \
          { \
            __VA_ARGS__; \
          } \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(semaphore,debugFlag,text,condition,type) \
      do \
      { \
        int __result; \
        \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        switch (type) \
        { \
          case SEMAPHORE_SIGNAL_MODIFY_SINGLE: __result = pthread_cond_signal(condition);    break; \
          case SEMAPHORE_SIGNAL_MODIFY_ALL:    __result = pthread_cond_broadcast(condition); break; \
        } \
        assert(__result == 0); \
        UNUSED_VARIABLE(__result); \
      } \
      while (0)
  #elif defined(PLATFORM_WINDOWS)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        WaitForSingleObject(semaphore->lock,INFINITE); \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) locked %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (WaitForSingleObject(semaphore->lock,timeout) != WAIT_OBJECT_0) \
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
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) wait lock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        if (WaitForSingleObject(semaphore->lock,0) != WAIT_OBJECT_0) \
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
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        ReleaseMutex(semaphore); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex) \
      do \
      { \
        assert(semaphore != NULL); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        pthread_cond_wait(condition,mutex); \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        __int64         __windowsTime; \
        struct timespec __timespec; \
        \
        assert(semaphore != NULL); \
        assert(timeout != WAIT_FOREVER); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) unlock+wait %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        getTimeSpec(&__timespec,timeout); \
        if (pthread_cond_timedwait(condition,mutex,&__timespec) != 0) \
        { \
          lockedFlag = FALSE; \
        } \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) waited+locked %s done\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(semaphore,debugFlag,text,condition,type) \
      do \
      { \
        int __result; \
        \
        assert(semaphore != NULL); \
        \
        UNUSED_VARIABLE(type); \
        \
        if (debugFlag) fprintf(stderr,"%s, %4d: '%s' (%s) signal %s\n",__FILE__,__LINE__,Thread_getCurrentName(),Thread_getCurrentIdString(),text); \
        switch (type) \
        { \
          case SEMAPHORE_SIGNAL_MODIFY_SINGLE: __result = pthread_cond_signal(condition);    break; \
          case SEMAPHORE_SIGNAL_MODIFY_ALL:    __result = pthread_cond_broadcast(condition); break; \
        } \
        assert(__result == 0);
        UNUSED_VARIABLE(__result); \
      } \
      while (0)
  #endif /* PLATFORM_... */

#else /* NDEBUG */
  #define VERIFY_COUNTERS(semaphore) do { } while (0)

//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text,timeout,...) \
      do \
      { \
        struct timespec __timespec; \
        bool            __locked; \
        \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        if (timeout != WAIT_FOREVER) \
        { \
          getTimeSpec(&__timespec,timeout); \
          __locked = (pthread_mutex_timedlock(&semaphore->lock,&__timespec) == 0); \
        } \
        else \
        { \
          __locked = (pthread_mutex_lock(&semaphore->lock) == 0); \
        } \
        if (__locked) \
        { \
          __VA_ARGS__; \
        } \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
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

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex,timeout,...) \
      do \
      { \
        struct timespec __timespec; \
        \
        assert(semaphore != NULL); \
        assert(timeout != WAIT_FOREVER); \
        \
        if (timeout != WAIT_FOREVER) \
        { \
          getTimeSpec(&__timespec,timeout); \
          if (pthread_cond_timedwait(condition,mutex,&__timespec) == 0) \
          { \
            __VA_ARGS__; \
          } \
        } \
        else \
        { \
          if (pthread_cond_wait(condition,mutex) == 0) \
          { \
            __VA_ARGS__; \
          } \
        } \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(semaphore,debugFlag,text,condition,type) \
      do \
      { \
        int __result; \
        \
        UNUSED_VARIABLE(semaphore); \
        UNUSED_VARIABLE(text); \
        \
        switch (type) \
        { \
          case SEMAPHORE_SIGNAL_MODIFY_SINGLE: __result = pthread_cond_signal(condition);    break; \
          case SEMAPHORE_SIGNAL_MODIFY_ALL:    __result = pthread_cond_broadcast(condition); break; \
        } \
        assert(__result == 0); \
        UNUSED_VARIABLE(__result); \
      } \
      while (0)
  #elif defined(PLATFORM_WINDOWS)
    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        (void)WaitForSingleObject(semaphore->lock,INFINITE); \
      } \
      while (0)

    #define __SEMAPHORE_LOCK(semaphore,lockType,debugFlag,text,timeout,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        if (WaitForSingleObject(semaphore->lock,timeout) != WAIT_OBJECT_0) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_TRYLOCK(semaphore,lockType,debugFlag,text,lockedFlag) \
      do \
      { \
        UNUSED_VARIABLE(lockType); \
        UNUSED_VARIABLE(text); \
        \
        if (WaitForSingleObject(semaphore->lock,0) != WAIT_OBJECT_0) lockedFlag = FALSE; \
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

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex) \
      do \
      { \
        UNUSED_VARIABLE(semaphore); \
        UNUSED_VARIABLE(text); \
        \
        pthread_cond_wait(condition,mutex); \
      } \
      while (0)

    #define __SEMAPHORE_WAIT(semaphore,debugFlag,text,condition,mutex,timeout,lockedFlag) \
      do \
      { \
        struct timespec __timespec; \
        \
        assert(semaphore != NULL); \
        assert(timeout != WAIT_FOREVER); \
        \
        UNUSED_VARIABLE(semaphore); \
        UNUSED_VARIABLE(text); \
        \
        getTimeSpec(&__timespec,timeout); \
        if (pthread_cond_timedwait(condition,mutex,&__timespec) == ETIMEDOUT) lockedFlag = FALSE; \
      } \
      while (0)

    #define __SEMAPHORE_SIGNAL(semaphore,debugFlag,text,condition,type) \
      do \
      { \
        int __result; \
        \
        UNUSED_VARIABLE(semaphore); \
        UNUSED_VARIABLE(text); \
        \
        switch (type) \
        { \
          case SEMAPHORE_SIGNAL_MODIFY_SINGLE: __result = pthread_cond_signal(condition);    break; \
          case SEMAPHORE_SIGNAL_MODIFY_ALL:    __result = pthread_cond_broadcast(condition); break; \
        } \
        assert(__result == 0); \
        UNUSED_VARIABLE(__result); \
      } \
      while (0)
  #endif /* PLATFORM_... */

#endif /* not NDEBUG */

/***************************** Forwards ********************************/
#if !defined(NDEBUG) && defined(HAVE_SIGQUIT)
  LOCAL void debugSemaphoreSignalHandler(int signalNumber);
#endif /* !defined(NDEBUG) && defined(HAVE_SIGQUIT) */

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
  List_init(&debugSemaphoreList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  // init lock
//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_mutexattr_init(&debugSemaphoreLockAttribute);
    pthread_mutexattr_settype(&debugSemaphoreLockAttribute,PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&debugSemaphoreLock,&debugSemaphoreLockAttribute) != 0)
    {
      HALT_INTERNAL_ERROR("Cannot initialize semaphore debug lock!");
    }
  #elif defined(PLATFORM_WINDOWS)
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    debugSemaphoreLock = CreateMutex(NULL,FALSE,NULL);
    if (debugSemaphoreLock == NULL)
    {
      HALT_INTERNAL_ERROR("Cannot initialize semaphore debug lock!");
    }
  #endif /* PLATFORM_... */

  #ifdef HAVE_SIGQUIT
    // install signal handler for Ctrl-\ (SIGQUIT) for printing debug information
    debugSignalQuitPrevHandler = signal(SIGQUIT,debugSemaphoreSignalHandler);
  #endif /* HAVE_SIGQUIT */

  startCycleCount = getCycleCounter();
}

/***********************************************************************\
* Name   : debugSemaphoreSignalHandler
* Purpose: signal handler
* Input  : signalNumber - signal number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGQUIT
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
#endif /* HAVE_SIGQUIT */

/***********************************************************************\
* Name   : debugSemaphoreIsOwned
* Purpose: check if semaphore is owned by current thread
* Input  : semaphore - semaphore
* Return : TRUE iff semaphore is owned by current thread
* Notes  : -
\***********************************************************************/

LOCAL bool debugSemaphoreIsOwned(const Semaphore *semaphore)
{
  bool     isOwned;
  ThreadId currentThreadId;
  uint     i;

  assert(semaphore != NULL);

  isOwned = FALSE;

  currentThreadId = Thread_getCurrentId();

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    for (i = 0; i < semaphore->debug.lockedByCount; i++)
    {
      if (Thread_equalThreads(semaphore->debug.lockedBy[i].threadId,currentThreadId))
      {
        isOwned = TRUE;
        break;
      }
    }
  }
  pthread_mutex_unlock(&debugSemaphoreLock);

  return isOwned;
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

  threadInfos[(*threadInfoCount)].threadId     = Thread_getCurrentId();
  threadInfos[(*threadInfoCount)].lockType     = lockType;
  threadInfos[(*threadInfoCount)].fileName     = fileName;
  threadInfos[(*threadInfoCount)].lineNb       = lineNb;
  threadInfos[(*threadInfoCount)].cycleCounter = getCycleCounter()-startCycleCount;
// TODO: stacktrace
  (*threadInfoCount)++;

#if 0
      fprintf(stderr,
              "DEBUG WARNING: too many thread locks for semaphore '%s' at %s, line %lu (max. %lu)!\n",
              semaphore->name,
              fileName,
              lineNb,
              (ulong)SIZE_OF_ARRAY(semaphore->debug.lockedBy)
             );
#endif

  for (uint i = 0; i < (*threadInfoCount); i++)
  {
    assert(!Thread_isNone(threadInfos[i].threadId));
    assert(threadInfos[i].lockType != SEMAPHORE_LOCK_TYPE_NONE);
    assert(threadInfos[i].fileName != NULL);
    assert(threadInfos[i].lineNb != 0);
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

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugAddThreadInfo(semaphore->debug.lockedBy,&semaphore->debug.lockedByCount,lockType,fileName,lineNb);
    VERIFY_COUNTERS(semaphore);
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

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugAddThreadInfo(semaphore->debug.pendingBy,&semaphore->debug.pendingByCount,lockType,fileName,lineNb);
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
  threadInfos[(*threadInfoCount)-1].threadId     = THREAD_ID_NONE;
  threadInfos[(*threadInfoCount)-1].lockType     = SEMAPHORE_LOCK_TYPE_NONE;
  threadInfos[(*threadInfoCount)-1].fileName     = NULL;
  threadInfos[(*threadInfoCount)-1].lineNb       = 0;
  threadInfos[(*threadInfoCount)-1].cycleCounter = 0LL;
  (*threadInfoCount)--;

  for (uint i = 0; i < (*threadInfoCount); i++)
  {
    assert(!Thread_isNone(threadInfos[i].threadId));
    assert(threadInfos[i].lockType != SEMAPHORE_LOCK_TYPE_NONE);
    assert(threadInfos[i].fileName != NULL);
    assert(threadInfos[i].lineNb != 0);
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

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugRemoveThreadInfo(semaphore->debug.lockedBy,&semaphore->debug.lockedByCount,"locked",semaphore->debug.name,fileName,lineNb);
    VERIFY_COUNTERS(semaphore);
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

  pthread_mutex_lock(&debugSemaphoreLock);
  {
    debugRemoveThreadInfo(semaphore->debug.pendingBy,&semaphore->debug.pendingByCount,"pending",semaphore->debug.name,fileName,lineNb);
  }
  pthread_mutex_unlock(&debugSemaphoreLock);
}

/***********************************************************************\
* Name   : debugStoreThreadInfoHistory
* Purpose: add thread to thread info array
* Input  : semaphore - semaphore
*          infoType  - info type; see SemaphoreThreadInfoTypes
*          lockType  - lock type; see SemaphoreLockTypes
*          fileName  - file name
*          lineNb    - line number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void debugStoreThreadInfoHistory(Semaphore               *semaphore,
                                              __SemaphoreHistoryTypes type,
                                              const char              *fileName,
                                              ulong                   lineNb
                                             )
{
  assert(semaphore != NULL);

  semaphore->debug.history[semaphore->debug.historyIndex].type         = type;
  semaphore->debug.history[semaphore->debug.historyIndex].threadId     = Thread_getCurrentId();
  semaphore->debug.history[semaphore->debug.historyIndex].fileName     = fileName;
  semaphore->debug.history[semaphore->debug.historyIndex].lineNb       = lineNb;
  semaphore->debug.history[semaphore->debug.historyIndex].cycleCounter = getCycleCounter()-startCycleCount;
// TODO: stacktrace
  semaphore->debug.historyIndex = (semaphore->debug.historyIndex+1)%__SEMAPHORE_MAX_THREAD_INFO;
  if (semaphore->debug.historyCount < __SEMAPHORE_MAX_THREAD_INFO) semaphore->debug.historyCount++;
}

/***********************************************************************\
* Name   : debugSetSemaphoreState
* Purpose: set debug semaphore state data
* Input  : semaphore      - semaphore
*          semaphoreState - semaphore state
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugSetSemaphoreState(Semaphore *semaphore, __SemaphoreState *semaphoreState)
{
  assert(semaphore != NULL);
  assert(semaphoreState != NULL);

  semaphoreState->timestamp             = getCycleCounter();
  semaphoreState->readRequestCount      = semaphore->readRequestCount;
  semaphoreState->readLockCount         = semaphore->readLockCount;
  semaphoreState->readWriteRequestCount = semaphore->readWriteRequestCount;
  semaphoreState->readWriteLockCount    = semaphore->readWriteLockCount;
}

/***********************************************************************\
* Name   : debugClearSemaphoreState
* Purpose: clear debug semaphore state data
* Input  : semaphoreState - semaphore state
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugClearSemaphoreState(__SemaphoreState *semaphoreState)
{
  assert(semaphoreState != NULL);

  semaphoreState->timestamp             = 0LL;
  semaphoreState->readRequestCount      = 0;
  semaphoreState->readLockCount         = 0;
  semaphoreState->readWriteRequestCount = 0;
  semaphoreState->readWriteLockCount    = 0;
}

/***********************************************************************\
* Name   : debugPrintSemaphoreState
* Purpose: print debug semaphore state data
* Input  : text           - text
*          indent         - indent
*          semaphoreState - semaphore state
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef DEBUG_SHOW_LAST_INFO
LOCAL void debugPrintSemaphoreState(const char *text, const char *indent, const __SemaphoreState *semaphoreState)
{
  assert(text != NULL);
  assert(semaphoreState != NULL);

  fprintf(stderr,"%s%s\n",indent,text);
  fprintf(stderr,"%s  timestamp            =%"PRIu64"\n",indent,semaphoreState->timestamp);
  fprintf(stderr,"%s  readRequestCount     =%u\n",indent,semaphoreState->readRequestCount);
  fprintf(stderr,"%s  readLockCount        =%u\n",indent,semaphoreState->readLockCount);
  fprintf(stderr,"%s  readWriteRequestCount=%u\n",indent,semaphoreState->readWriteRequestCount);
  fprintf(stderr,"%s  readWriteLockCount   =%u\n",indent,semaphoreState->readWriteLockCount);
}
#endif /* DEBUG_SHOW_LAST_INFO */

#endif /* not NDEBUG */

/***********************************************************************\
* Name   : getTimeSpec
* Purpose: get POSIX compatible timespec with offset
* Input  : timeOffset - time offset [ms]
* Output : timespec - time
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void getTimeSpec(struct timespec *timespec, ulong timeOffset)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    __int64 windowsTime;
  #endif /* PLATFORM_... */

  assert(timespec != NULL);

  #if   defined(PLATFORM_LINUX)
    clock_gettime(CLOCK_REALTIME,timespec);
  #elif defined(PLATFORM_WINDOWS)
    GetSystemTimeAsFileTime((FILETIME*)&windowsTime);
    windowsTime -= 116444736000000000LL;  // Jan 1 1601 -> Jan 1 1970
    timespec->tv_sec  = (windowsTime/10000000LL);
    timespec->tv_nsec = (windowsTime%10000000LL)*100LL;
  #endif /* PLATFORM_... */
  timespec->tv_nsec = timespec->tv_nsec+((timeOffset)%1000L)*1000000L; \
  timespec->tv_sec  = timespec->tv_sec+((timespec->tv_nsec/1000000L)+(timeOffset))/1000L; \
  timespec->tv_nsec %= 1000000L; \
}

#ifdef NDEBUG
LOCAL_INLINE void incrementReadRequest(Semaphore  *semaphore)
#else /* not NDEBUG */
LOCAL_INLINE void incrementReadRequest(Semaphore          *semaphore,
                                       SemaphoreLockTypes semaphoreLockType,
                                       const char         *__fileName__,
                                       ulong              __lineNb__
                                      )
#endif /* NDEBUG */
{
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
    debugAddPendingThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
    debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadRequest);
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
LOCAL_INLINE void decrementReadRequest(Semaphore  *semaphore)
#else /* not NDEBUG */
LOCAL_INLINE void decrementReadRequest(Semaphore  *semaphore,
                                       const char *__fileName__,
                                       ulong      __lineNb__
                                      )
#endif /* NDEBUG */
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
    debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
LOCAL_INLINE void incrementReadWriteRequest(Semaphore  *semaphore)
#else /* not NDEBUG */
LOCAL_INLINE void incrementReadWriteRequest(Semaphore          *semaphore,
                                            SemaphoreLockTypes semaphoreLockType,
                                            const char         *__fileName__,
                                            ulong              __lineNb__
                                           )
#endif /* NDEBUG */
{
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
    debugAddPendingThreadInfo(semaphore,semaphoreLockType,__fileName__,__lineNb__);
    debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadWriteRequest);
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
LOCAL_INLINE void decrementReadWriteRequest(Semaphore  *semaphore)
#else /* not NDEBUG */
LOCAL_INLINE void decrementReadWriteRequest(Semaphore  *semaphore,
                                            const char *__fileName__,
                                            ulong      __lineNb__
                                           )
#endif /* NDEBUG */
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
    debugRemovePendingThreadInfo(semaphore,__fileName__,__lineNb__);
  #endif /* not NDEBUG */
}

#ifdef CHECK_FOR_DEADLOCK

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

#ifndef NDEBUG
LOCAL void debugCheckForDeadLock(Semaphore          *semaphore,
                                 SemaphoreLockTypes lockType,
                                 const char         *fileName,
                                 ulong              lineNb
                                )
{
#ifdef CHECK_FOR_DEADLOCK
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

  UNUSED_VARIABLE(lockType);

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
  bool        lockedFlag;
  TimeoutInfo timeoutInfo;

  assert(semaphore != NULL);
  assert((semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ) || (semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

  lockedFlag = FALSE;

  Misc_initTimeout(&timeoutInfo,timeout);
  switch (semaphoreLockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      break;

    case SEMAPHORE_LOCK_TYPE_READ:
      /* request read lock
         Note: for a read lock the semaphore is locked temporary and a read-lock is stored
      */

      // increment read request counter atomically
      #ifndef NDEBUG
        incrementReadRequest(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #else /* NDEBUG */
        incrementReadRequest(semaphore);
      #endif /* not NDEBUG */

      // read: aquire temporary lock
      __SEMAPHORE_LOCK(semaphore,semaphoreLockType,DEBUG_FLAG_READ,"R",timeout,lockedFlag = TRUE);
      if (!lockedFlag)
      {
        #ifndef NDEBUG
          decrementReadRequest(semaphore,__fileName__,__lineNb__);
        #else /* NDEBUG */
          decrementReadRequest(semaphore);
        #endif /* not NDEBUG */
        break;
      }

      VERIFY_COUNTERS(semaphore);
      {
        // wait until no other read/write locks
        while (   (semaphore->readWriteLockCount > 0)
               && !Thread_isCurrentThread(semaphore->readWriteLockOwnedBy)
               && !Misc_isTimeout(&timeoutInfo)
              )
        {
          __SEMAPHORE_WAIT(semaphore,
                           DEBUG_FLAG_READ_WRITE,
                           "R",
                           &semaphore->modified,
                           &semaphore->lock,
                           Misc_getRestTimeout(&timeoutInfo,500)
                          );
        }
        if (   (semaphore->readWriteLockCount > 0)
            && !Thread_isCurrentThread(semaphore->readWriteLockOwnedBy)
           )
        {
          __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
          lockedFlag = FALSE;

          #ifndef NDEBUG
            decrementReadRequest(semaphore,__fileName__,__lineNb__);
          #else /* NDEBUG */
            decrementReadRequest(semaphore);
          #endif /* not NDEBUG */
          break;
        }

#if 0
// Note: allow weaker access -> if already aquired read/write-lock handle read-lock like a read/write-lock
        // check if re-lock with weaker access -> error
        if (semaphore->readWriteLockCount > 0)
        {
          #ifndef NDEBUG
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
#endif // 0

#if 0
//Note: read-lock requests will not wait for running read/write-locks, because aquiring a read/write lock is waiting for running read-locks (see below)
        /* wait until no more other read/write-locks
           Note: do 'polling in a loop, because signal/broadcast may not always wake-up waiting threads
        */
        while (   (semaphore->readWriteLockCount > 0)
               && !Misc_isTimeout(&timeoutInfo)
              )
        {
          __SEMAPHORE_WAIT(semaphore,
                           semaphoreLockType,
                           DEBUG_FLAG_READ_WRITE,
                           "R",
                           &semaphore->modified,
                           &semaphore->lock,
                           Misc_getRestTimeout(&timeoutInfo,500)
                          );
        }
        if (semaphore->readWriteLockCount > 0)
        {
          __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
          lockedFlag = FALSE;

          #ifndef NDEBUG
            decrementReadRequest(semaphore,__fileName__,__lineNb__);
          #else /* NDEBUG */
            decrementReadRequest(semaphore);
          #endif /* not NDEBUG */
          break;
        }
        assert(semaphore->readWriteLockCount == 0);
#endif // 0

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
        #ifndef NDEBUG
          debugAddLockedThreadInfo(semaphore,SEMAPHORE_LOCK_TYPE_READ,__fileName__,__lineNb__);
          debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadLock);
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_LOCK_READ,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        // decrement read request counter
        #ifndef NDEBUG
          decrementReadRequest(semaphore,__fileName__,__lineNb__);
        #else /* NDEBUG */
          decrementReadRequest(semaphore);
        #endif /* not NDEBUG */
      }
      VERIFY_COUNTERS(semaphore);

      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
      break;

    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      /* request write lock
         Note: for a read/write lock the semaphore is locked permanent
      */

      // increment read/write request counter atomically
      #ifndef NDEBUG
        incrementReadWriteRequest(semaphore,semaphoreLockType,__fileName__,__lineNb__);
      #else /* NDEBUG */
        incrementReadWriteRequest(semaphore);
      #endif /* not NDEBUG */

      // write: aquire permanent lock
      __SEMAPHORE_LOCK(semaphore,semaphoreLockType,DEBUG_FLAG_READ_WRITE,"RW",timeout,lockedFlag = TRUE);
      if (!lockedFlag)
      {
        #ifndef NDEBUG
          decrementReadWriteRequest(semaphore,__fileName__,__lineNb__);
        #else /* NDEBUG */
          decrementReadWriteRequest(semaphore);
        #endif /* not NDEBUG */
        break;
      }

      VERIFY_COUNTERS(semaphore);
      {
        /* wait until no more read-locks
           Note: do 'polling in a loop, because signal/broadcast may not always wake-up waiting threads
        */
        while (   (semaphore->readLockCount > 0)
               && !Misc_isTimeout(&timeoutInfo)
              )
        {
          __SEMAPHORE_WAIT(semaphore,
                           DEBUG_FLAG_READ_WRITE,
                           "R",
                           &semaphore->readLockZero,
                           &semaphore->lock,
                           Misc_getRestTimeout(&timeoutInfo,500)
                          );
        }
        if (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ_WRITE,"RW");
          lockedFlag = FALSE;

          #ifndef NDEBUG
            decrementReadWriteRequest(semaphore,__fileName__,__lineNb__);
          #else /* NDEBUG */
            decrementReadWriteRequest(semaphore);
          #endif /* not NDEBUG */
          break;
        }

        #ifndef NDEBUG
          debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadWriteWakeup);
        #endif /* not NDEBUG */
        assert(semaphore->readLockCount == 0);
        assert((semaphore->lockType == SEMAPHORE_LOCK_TYPE_NONE) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));
        assert(Thread_isNone(semaphore->readWriteLockOwnedBy) || Thread_isCurrentThread(semaphore->readWriteLockOwnedBy));

        // set/increment read/write-lock
        semaphore->lockType             = SEMAPHORE_LOCK_TYPE_READ_WRITE;
        semaphore->readWriteLockCount++;
        semaphore->readWriteLockOwnedBy = Thread_getCurrentId();
        #ifndef NDEBUG
          debugAddLockedThreadInfo(semaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE,__fileName__,__lineNb__);
          debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadWriteLock);
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_LOCK_READ_WRITE,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        // decrement read/write request counter atomically
        #ifndef NDEBUG
          decrementReadWriteRequest(semaphore,__fileName__,__lineNb__);
        #else /* NDEBUG */
          decrementReadWriteRequest(semaphore);
        #endif /* not NDEBUG */

        assert(debugSemaphoreIsOwned(semaphore));
        assertx(Thread_isCurrentThread(semaphore->readWriteLockOwnedBy),
                "%p: current thread %s, owner thread %s",
                semaphore,
                Thread_getCurrentIdString(),
                Thread_getIdString(semaphore->readWriteLockOwnedBy)
               );
      }
      VERIFY_COUNTERS(semaphore);
      break;

    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  Misc_doneTimeout(&timeoutInfo);

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

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      // nothing to do
      break;

    case SEMAPHORE_LOCK_TYPE_READ:
      /* unlock read
      */
      __SEMAPHORE_LOCK(semaphore,semaphore->lockType,DEBUG_FLAG_READ,"R",WAIT_FOREVER);
      {
        VERIFY_COUNTERS(semaphore);
        {
          assert(semaphore->readLockCount > 0);
          assert(semaphore->readWriteLockCount == 0);
          assert(Thread_isNone(semaphore->readWriteLockOwnedBy));

          // do one read-unlock
          semaphore->readLockCount--;
          #ifndef NDEBUG
            debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
            debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadUnlock);
            debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_UNLOCK,__fileName__,__lineNb__);
          #endif /* not NDEBUG */

          if (semaphore->readLockCount == 0)
          {
            // semaphore is free
            semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

            // signal modification
            __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"READ0 (unlock)",&semaphore->readLockZero,SEMAPHORE_SIGNAL_MODIFY_ALL);
          }
        }
        VERIFY_COUNTERS(semaphore);
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
      break;

    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      /* unlock read/write
         Note: for a read/write lock semaphore is locked permanent
      */

      VERIFY_COUNTERS(semaphore);
      {
        assert(semaphore->readLockCount == 0);
        assert(semaphore->readWriteLockCount > 0);
        assertx(Thread_isCurrentThread(semaphore->readWriteLockOwnedBy),
                "%p: current thread %s, owner thread %s",
                semaphore,
                Thread_getCurrentIdString(),
                Thread_getIdString(semaphore->readWriteLockOwnedBy)
               );
//fprintf(stderr,"%s, %d: thread=%s sem=%p count=%d owner=%d\n",__FILE__,__LINE__,Thread_getCurrentIdString(),semaphore,semaphore->lock.__data.__count,semaphore->lock.__data.__owner);
//fprintf(stderr,"%s, %4d: thread=%s sem=%p count=%d owner=%d: %d %d %d\n",__FILE__,__LINE__,Thread_getCurrentIdString(),semaphore,semaphore->lock.__data.__count,semaphore->lock.__data.__owner,semaphore->lockedByCount,semaphore->readLockCount,semaphore->readWriteLockCount);

        // do one read/write-unlock
        semaphore->readWriteLockCount--;
        #ifndef NDEBUG
          // debug lock code: remove lock information
          debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
          debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadWriteUnlock);
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_UNLOCK,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        if (semaphore->readWriteLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType             = SEMAPHORE_LOCK_TYPE_NONE;
          semaphore->readWriteLockOwnedBy = THREAD_ID_NONE;

          // signal modification
          __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_SINGLE);
        }
      }
      VERIFY_COUNTERS(semaphore);

      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ_WRITE,"RW");
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
  bool modifiedFlag;

  assert(semaphore != NULL);
  assert(semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE);

  modifiedFlag = FALSE;

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      // nothing to do
      break;

    case SEMAPHORE_LOCK_TYPE_READ:
      /* wait modified read -> temporary revert own read-lock and wait for modification signal
      */
      __SEMAPHORE_LOCK(semaphore,semaphore->lockType,DEBUG_FLAG_READ,"R",WAIT_FOREVER);
      {
        VERIFY_COUNTERS(semaphore);

        assert(semaphore->readLockCount > 0);
        assert(semaphore->readWriteLockCount == 0);
        assert(Thread_isNone(semaphore->readWriteLockOwnedBy));

        #ifndef NDEBUG
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_WAIT,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        // temporary revert read-lock
        semaphore->readLockCount--;
        #ifndef NDEBUG
          debugRemoveLockedThreadInfo(semaphore,__fileName__,__lineNb__);
        #endif /* not NDEBUG */
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;
          #ifndef NDEBUG
            debugSetSemaphoreState(semaphore,&semaphore->debug.lastReadUnlock);
          #endif /* not NDEBUG */

          // signal modification
          __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"READ0 (wait)",&semaphore->readLockZero,SEMAPHORE_SIGNAL_MODIFY_ALL);
        }

        /* wait for modification with timeout
           Note: do 'polling in a loop, because signal/broadcast may not always wake-up waiting threads
        */
        __SEMAPHORE_WAIT(semaphore,
                         DEBUG_FLAG_READ,
                         "MODIFIED",
                         &semaphore->modified,
                         &semaphore->lock,
                         timeout,
                         modifiedFlag = TRUE
                        );

        /* wait until there are no more write-locks
           Note: do 'polling in a loop, because signal/broadcast may not always wake-up waiting threads
        */
        while (semaphore->readWriteLockCount > 0)
        {
          __SEMAPHORE_WAIT(semaphore,
                           DEBUG_FLAG_READ,
                           "W",
                           &semaphore->modified,
                           &semaphore->lock,
                           500
                          );
        }

        // Note: semaphore must now be free/read locked
        assert((semaphore->lockType == SEMAPHORE_LOCK_TYPE_NONE) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ));
        assert(semaphore->readWriteLockCount == 0);
        assert(Thread_isNone(semaphore->readWriteLockOwnedBy));

        // restore temporary reverted read-lock
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;
        semaphore->readLockCount++;
        #ifndef NDEBUG
          debugAddLockedThreadInfo(semaphore,SEMAPHORE_LOCK_TYPE_READ,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        VERIFY_COUNTERS(semaphore);

        #ifndef NDEBUG
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_WAIT_DONE,__fileName__,__lineNb__);
        #endif /* not NDEBUG */
      }
      __SEMAPHORE_UNLOCK(semaphore,DEBUG_FLAG_READ,"R");
      break;

    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      /* wait modified read/write lock -> temporary revert own read/write-lock and wait for modification signal
         Note: for a read/write lock semaphore is locked permanent
      */

      {
        VERIFY_COUNTERS(semaphore);

        assert(semaphore->readLockCount == 0);
        assert(semaphore->readWriteLockCount > 0);
//TODO: remove
#if 0
if (!Thread_isCurrentThread(semaphore->readWriteLockOwnedBy)) {
fprintf(stderr,"%s, %d: owner=%s current=%s\n",__FILE__,__LINE__,Thread_getIdString(semaphore->readWriteLockOwnedBy),Thread_getCurrentIdString());
Semaphore_debugPrintInfo();
}
#endif
        assertx(Thread_isCurrentThread(semaphore->readWriteLockOwnedBy),
                "%p: current thread %s, owner thread %s",
                semaphore,
                Thread_getCurrentIdString(),
                Thread_getIdString(semaphore->readWriteLockOwnedBy)
               );

        savedReadWriteLockCount = semaphore->readWriteLockCount;

        // temporary revert write-lock
        semaphore->lockType             = SEMAPHORE_LOCK_TYPE_NONE;
        semaphore->readWriteLockCount   = 0;
        semaphore->readWriteLockOwnedBy = THREAD_ID_NONE;
        semaphore->waitModifiedCount    += savedReadWriteLockCount;

        #ifndef NDEBUG
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_WAIT,__fileName__,__lineNb__);
        #endif /* not NDEBUG */

        // signal modification
        __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_SINGLE);

        // wait for modification
        __SEMAPHORE_WAIT(semaphore,
                         DEBUG_FLAG_READ_WRITE,
                         "MODIFIED",
                         &semaphore->modified,
                         &semaphore->lock,
                         timeout,
                         modifiedFlag = TRUE
                        );

        // request read/write-lock
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

        /* wait until no more read-locks
           Note: do 'polling' in a loop, because signal/broadcast may not always wake-up waiting threads
        */
        while (semaphore->readLockCount > 0)
        {
          __SEMAPHORE_WAIT(semaphore,
                           DEBUG_FLAG_READ_WRITE,
                           "R",
                           &semaphore->readLockZero,
                           &semaphore->lock,
                           500
                          );
        }
        assert(semaphore->readLockCount == 0);

        // Note: semaphore must now be free
        assert(semaphore->lockType == SEMAPHORE_LOCK_TYPE_NONE);
        assert(semaphore->readLockCount == 0);
        assert(semaphore->readWriteLockCount == 0);
        assert(Thread_isNone(semaphore->readWriteLockOwnedBy));

        // restore temporary reverted read/write-lock
        assert(semaphore->waitModifiedCount >= savedReadWriteLockCount);
        semaphore->lockType             = SEMAPHORE_LOCK_TYPE_READ_WRITE;
        semaphore->readWriteLockCount   = savedReadWriteLockCount;
        semaphore->readWriteLockOwnedBy = Thread_getCurrentId();
        semaphore->waitModifiedCount    -= savedReadWriteLockCount;

        // done request read/write-lock
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

        VERIFY_COUNTERS(semaphore);

        #ifndef NDEBUG
          debugStoreThreadInfoHistory(semaphore,SEMAPHORE_HISTORY_TYPE_WAIT_DONE,__fileName__,__lineNb__);
        #endif /* not NDEBUG */
      }
      break;

    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return modifiedFlag;
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
//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    if (pthread_mutex_init(&semaphore->requestLock,NULL) != 0)
    {
      return FALSE;
    }
  #elif defined(PLATFORM_WINDOWS)
    semaphore->requestLock = CreateMutex(NULL,FALSE,NULL);
    if (semaphore->requestLock == NULL)
    {
      return FALSE;
    }
  #endif /* PLATFORM_... */
  semaphore->readRequestCount      = 0;
  semaphore->readWriteRequestCount = 0;

//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_mutexattr_init(&semaphore->lockAttributes);
    pthread_mutexattr_settype(&semaphore->lockAttributes,PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&semaphore->lock,&semaphore->lockAttributes) != 0)
    {
      pthread_mutexattr_destroy(&semaphore->lockAttributes);
      pthread_mutex_destroy(&semaphore->requestLock);
      return FALSE;
    }
  #elif defined(PLATFORM_WINDOWS)
    semaphore->lock = CreateMutex(NULL,FALSE,NULL);
    if (semaphore->lock == NULL)
    {
      CloseHandle(semaphore->requestLock);
      return FALSE;
    }
  #endif /* PLATFORM_... */
  if (pthread_cond_init(&semaphore->readLockZero,NULL) != 0)
  {
//TODO: use Windows WaitForSingleObject?
//    #if   defined(PLATFORM_LINUX)
#if 1
      pthread_mutex_destroy(&semaphore->lock);
      pthread_mutexattr_destroy(&semaphore->lockAttributes);
      pthread_mutex_destroy(&semaphore->requestLock);
    #elif defined(PLATFORM_WINDOWS)
      CloseHandle(semaphore->lock);
      CloseHandle(semaphore->requestLock);
    #endif /* PLATFORM_... */
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->modified,NULL) != 0)
  {
    pthread_cond_destroy(&semaphore->readLockZero);
//TODO: use Windows WaitForSingleObject?
//    #if   defined(PLATFORM_LINUX)
#if 1
      pthread_mutex_destroy(&semaphore->lock);
      pthread_mutexattr_destroy(&semaphore->lockAttributes);
      pthread_mutex_destroy(&semaphore->requestLock);
    #elif defined(PLATFORM_WINDOWS)
      CloseHandle(semaphore->lock);
      CloseHandle(semaphore->requestLock);
    #endif /* PLATFORM_... */
    return FALSE;
  }
  semaphore->lockType             = SEMAPHORE_LOCK_TYPE_NONE;
  semaphore->readLockCount        = 0;
  semaphore->readWriteLockCount   = 0;
  semaphore->readWriteLockOwnedBy = THREAD_ID_NONE;
  semaphore->waitModifiedCount    = 0;
  semaphore->endFlag              = FALSE;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(semaphore,Semaphore);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,semaphore,Semaphore);
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    // add to semaphore list
    pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
      if (List_contains(&debugSemaphoreList,semaphore,CALLBACK_(NULL,NULL)))
      {
        HALT_INTERNAL_ERROR_AT(__fileName__,__lineNb__,"Semaphore '%s' was already initialized at %s, line %lu!",
                               semaphore->debug.name,
                               semaphore->debug.fileName,
                               semaphore->debug.lineNb
                              );
      }

      semaphore->debug.fileName       = __fileName__;
      semaphore->debug.lineNb         = __lineNb__;
      semaphore->debug.name           = name;
      memClear(semaphore->debug.pendingBy,sizeof(semaphore->debug.pendingBy));
      semaphore->debug.pendingByCount = 0;
      memClear(semaphore->debug.lockedBy,sizeof(semaphore->debug.lockedBy));
      semaphore->debug.lockedByCount  = 0;

      List_append(&debugSemaphoreList,semaphore);
    }
    pthread_mutex_unlock(&debugSemaphoreLock);

    debugClearSemaphoreState(&semaphore->debug.lastReadRequest     );
    debugClearSemaphoreState(&semaphore->debug.lastReadWakeup      );
    debugClearSemaphoreState(&semaphore->debug.lastReadLock        );
    debugClearSemaphoreState(&semaphore->debug.lastReadUnlock      );
    debugClearSemaphoreState(&semaphore->debug.lastReadWriteRequest);
    debugClearSemaphoreState(&semaphore->debug.lastReadWriteWakeup );
    debugClearSemaphoreState(&semaphore->debug.lastReadWriteLock   );
    debugClearSemaphoreState(&semaphore->debug.lastReadWriteUnlock );

    semaphore->debug.historyIndex = 0;
    semaphore->debug.historyCount = 0;
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

  #ifndef NDEBUG
    // check if still locked
    pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

    pthread_mutex_lock(&debugSemaphoreLock);
    {
      if (semaphore->debug.lockedByCount > 0)
      {
        HALT_INTERNAL_ERROR("Thread '%s' (%s) did not unlock semaphore '%s' which was locked at %s, line %lu!",
                            Thread_getName(semaphore->debug.lockedBy[0].threadId),
                            Thread_getIdString(semaphore->debug.lockedBy[0].threadId),
                            semaphore->debug.name,
                            semaphore->debug.lockedBy[0].fileName,
                            semaphore->debug.lockedBy[0].lineNb
                           );
      }
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
  #endif /* not NDEBUG */

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(semaphore,Semaphore);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,semaphore,Semaphore);
  #endif /* NDEBUG */

  // free resources
  pthread_cond_destroy(&semaphore->modified);
  pthread_cond_destroy(&semaphore->readLockZero);
//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_mutex_destroy(&semaphore->lock);
    pthread_mutexattr_destroy(&semaphore->lockAttributes);
    pthread_mutex_destroy(&semaphore->requestLock);
  #elif defined(PLATFORM_WINDOWS)
    CloseHandle(semaphore->lock);
    CloseHandle(semaphore->requestLock);
  #endif /* PLATFORM_... */
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
    for (i = 0; i < semaphore->debug.lockedByCount; i++)
    {
      if (Thread_equalThreads(semaphore->debug.lockedBy[i].threadId,currentThreadId))
      {
        isOwned = TRUE;
        break;
      }
    }
  }
  pthread_mutex_unlock(&debugSemaphoreLock);

  return isOwned;
}
#endif /* not NDEBUG */

void Semaphore_signalModified(Semaphore *semaphore, SemaphoreSignalModifyTypes type)
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  #ifdef NDEBUG
    (void)lock(semaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
  #else /* not NDEBUG */
    (void)lock(__FILE__,__LINE__,semaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
  #endif /* NDEBUG */
  {
    __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,type);
  }
  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__FILE__,__LINE__,semaphore);
  #endif /* NDEBUG */
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
  {
    // set end flag
    semaphore->endFlag = TRUE;

    // signal modification
    __SEMAPHORE_SIGNAL(semaphore,DEBUG_FLAG_MODIFIED,"MODIFIED",&semaphore->modified,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }
  // unlock
  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__FILE__,__LINE__,semaphore);
  #endif /* NDEBUG */
}

#ifndef NDEBUG
void Semaphore_debugTrace(const Semaphore *semaphore)
{
  assert(semaphore != NULL);

  debugTraceSemaphore = semaphore;
}

void Semaphore_debugTraceClear(void)
{
  debugTraceSemaphore = NULL;
}

void Semaphore_debugDump(const Semaphore *semaphore, FILE *handle)
{
  #ifndef NDEBUG
    const char *HISTORY_TYPE_STRINGS[] =
    {
      "locked read",
      "locked read/write",
      "unlocked",
      "wait",
      "wait done"
    };
  #endif

  char s[64+1];
  uint i,index;

  assert(semaphore != NULL);
  assert(handle != NULL);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugConsoleLock);
  {
    stringFormat(s,sizeof(s),"'%s'",semaphore->debug.name);
    fprintf(handle,"  %-64s 0x%016"PRIxPTR" (%s, line %lu): pending R %3u/RW %3u",s,(uintptr_t)semaphore,semaphore->debug.fileName,semaphore->debug.lineNb,semaphore->readRequestCount,semaphore->readWriteRequestCount);
    #if !defined(NDEBUG) && defined(DEBUG_SHOW_LAST_INFO)
      fprintf(handle,"\n");
      debugPrintSemaphoreState("last readRequest",         "    ",&semaphore->debug.lastReadRequest     );
      debugPrintSemaphoreState("last lastReadLock",        "    ",&semaphore->debug.lastReadLock        );
      debugPrintSemaphoreState("last lastReadUnlock",      "    ",&semaphore->debug.lastReadUnlock      );
      debugPrintSemaphoreState("last lastReadWriteRequest","    ",&semaphore->debug.lastReadWriteRequest);
      debugPrintSemaphoreState("last lastReadWriteWakeup", "    ",&semaphore->debug.lastReadWriteWakeup );
      debugPrintSemaphoreState("last lastReadWriteLock",   "    ",&semaphore->debug.lastReadWriteLock   );
      debugPrintSemaphoreState("last lastReadWriteUnlock", "    ",&semaphore->debug.lastReadWriteUnlock );
    #endif /* !DEBUG && DEBUG_SHOW_LAST_INFO */
    switch (semaphore->lockType)
    {
      case SEMAPHORE_LOCK_TYPE_NONE:
        fprintf(handle,", not locked\n");
        break;
      case SEMAPHORE_LOCK_TYPE_READ:
        fprintf(handle,", LOCKED %s %d\n", SEMAPHORE_LOCK_TYPE_NAMES[SEMAPHORE_LOCK_TYPE_READ],semaphore->readLockCount);
        for (i = 0; i < semaphore->debug.lockedByCount; i++)
        {
          fprintf(handle,
                  "    by thread '%s' (%s) at %s, line %lu\n",
                  Thread_getName(semaphore->debug.lockedBy[i].threadId),
                  Thread_getIdString(semaphore->debug.lockedBy[i].threadId),
                  semaphore->debug.lockedBy[i].fileName,
                  semaphore->debug.lockedBy[i].lineNb
                 );
        }
        for (i = 0; i < semaphore->debug.pendingByCount; i++)
        {
          fprintf(handle,
                  "    pending thread '%s' (%s) %s at %s, line %lu\n",
                  Thread_getName(semaphore->debug.lockedBy[i].threadId),
                  Thread_getIdString(semaphore->debug.pendingBy[i].threadId),
                  SEMAPHORE_LOCK_TYPE_NAMES[semaphore->debug.pendingBy[i].lockType],
                  semaphore->debug.pendingBy[i].fileName,
                  semaphore->debug.pendingBy[i].lineNb
                 );
        }
        break;
      case SEMAPHORE_LOCK_TYPE_READ_WRITE:
        fprintf(handle,", LOCKED %s %d\n", SEMAPHORE_LOCK_TYPE_NAMES[SEMAPHORE_LOCK_TYPE_READ_WRITE],semaphore->readWriteLockCount);
        for (i = 0; i < semaphore->debug.lockedByCount; i++)
        {
          fprintf(handle,
                  "    by thread '%s' (%s) at %s, line %lu\n",
                  Thread_getName(semaphore->debug.lockedBy[i].threadId),
                  Thread_getIdString(semaphore->debug.lockedBy[i].threadId),
                  semaphore->debug.lockedBy[i].fileName,
                  semaphore->debug.lockedBy[i].lineNb
                 );
        }
        for (i = 0; i < semaphore->debug.pendingByCount; i++)
        {
          fprintf(handle,
                  "    pending thread '%s' (%s) %s at %s, line %lu\n",
                  Thread_getName(semaphore->debug.lockedBy[i].threadId),
                  Thread_getIdString(semaphore->debug.pendingBy[i].threadId),
                  SEMAPHORE_LOCK_TYPE_NAMES[semaphore->debug.pendingBy[i].lockType],
                  semaphore->debug.pendingBy[i].fileName,
                  semaphore->debug.pendingBy[i].lineNb
                 );
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    #ifndef NDEBUG
      fprintf(handle,"  History:\n");
      for (i = 0; i < semaphore->debug.historyCount; i++)
      {
        index = (semaphore->debug.historyIndex+semaphore->debug.historyCount-i-1)%semaphore->debug.historyCount;
        stringFormat(s,sizeof(s),"'%s'",Thread_getName(semaphore->debug.history[index].threadId));
        fprintf(stderr,
                "    %-20s %16"PRIu64" thread %-20s (%s) at %s, %lu\n",
                HISTORY_TYPE_STRINGS[semaphore->debug.history[index].type],
                semaphore->debug.history[index].cycleCounter,
                s,
                Thread_getIdString(semaphore->debug.history[index].threadId),
                semaphore->debug.history[index].fileName,
                semaphore->debug.history[index].lineNb
               );
      }
    #endif /* NDEBUG */
    fflush(handle);
  }
  pthread_mutex_unlock(&debugConsoleLock);
}

void Semaphore_debugDumpInfo(FILE *handle)
{
  const Semaphore *semaphore;

  assert(handle != NULL);

  pthread_once(&debugSemaphoreInitFlag,debugSemaphoreInit);

  pthread_mutex_lock(&debugConsoleLock);
  {
    fprintf(handle,"Semaphore debug info:\n");
    pthread_mutex_lock(&debugSemaphoreLock);
    {
      LIST_ITERATE(&debugSemaphoreList,semaphore)
      {
        Semaphore_debugDump(semaphore,handle);
      }
    }
    pthread_mutex_unlock(&debugSemaphoreLock);
    fprintf(handle,"\n");
    fflush(handle);
  }
  pthread_mutex_unlock(&debugConsoleLock);
}

void Semaphore_debugPrintInfo(void)
{
  Semaphore_debugDumpInfo(stderr);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
