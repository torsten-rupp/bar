/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for inter-process mutex semaphores
* Systems: all POSIX
*
\***********************************************************************/

#ifndef __SEMAPHORES__
#define __SEMAPHORES__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <pthread.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
  #include <pthread.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/threads.h"
#ifndef NDEBUG
  #include "common/lists.h"
#endif /* not NDEBUG */

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#ifndef NDEBUG
  #define __SEMAPHORE_MAX_THREAD_INFO 64
#endif /* not NDEBUG */

/***************************** Datatypes *******************************/

typedef enum
{
  SEMAPHORE_TYPE_BINARY,
  SEMAPHORE_TYPE_COUNTING,
  SEMAPHORE_TYPE_MUTEX
} SemaphoreTypes;

// lock types
typedef enum
{
  SEMAPHORE_LOCK_TYPE_NONE,
  SEMAPHORE_LOCK_TYPE_READ,
  SEMAPHORE_LOCK_TYPE_READ_WRITE,
} SemaphoreLockTypes;

#ifndef NDEBUG
  typedef struct
  {
    ThreadId           threadId;             // id of thread who locked semaphore
    SemaphoreLockTypes lockType;
    const char         *fileName;            // file+line number of lock call
    ulong              lineNb;
    uint64             cycleCounter;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  } __SemaphoreThreadInfo;

  typedef struct
  {
    uint64 timestamp;
    uint   readRequestCount;
    uint   readLockCount;
    uint   readWriteRequestCount;
    uint   readWriteLockCount;
  } __SemaphoreState;

  typedef enum
  {
    SEMAPHORE_HISTORY_TYPE_LOCK_READ,
    SEMAPHORE_HISTORY_TYPE_LOCK_READ_WRITE,
    SEMAPHORE_HISTORY_TYPE_UNLOCK,
    SEMAPHORE_HISTORY_TYPE_WAIT,
    SEMAPHORE_HISTORY_TYPE_WAIT_DONE,
  } __SemaphoreHistoryTypes;

  typedef struct
  {
    __SemaphoreHistoryTypes type;
    ThreadId                threadId;      // id of thread who locked semaphore
    const char              *fileName;     // file+line number of lock call
    ulong                   lineNb;
    uint64                  cycleCounter;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* HAVE_BACKTRACE */
  } __SemaphoreHistoryInfo;
#endif /* not NDEBUG */

typedef struct Semaphore
{
  #ifndef NDEBUG
    LIST_NODE_HEADER(struct Semaphore);
  #endif /* not NDEBUG */

  SemaphoreTypes      type;
//TODO: use Windows WaitForSingleObject?
//  #if   defined(PLATFORM_LINUX)              // lock to update request counters, thread info
#if 1
    pthread_mutex_t     requestLock;
  #elif defined(PLATFORM_WINDOWS)
    HANDLE              requestLock;
  #endif /* PLATFORM_... */
  uint                readRequestCount;      // number of pending read locks
  uint                readWriteRequestCount; // number of pending read/write locks

// TODO:
//  #if   defined(PLATFORM_LINUX)              // lock (thread who own lock is allowed to change the following semaphore variables)
#if 1
    pthread_mutex_t     lock;
    pthread_mutexattr_t lockAttributes;
  #elif defined(PLATFORM_WINDOWS)
    HANDLE              lock;
  #endif /* PLATFORM_... */

  SemaphoreLockTypes  lockType;              // current lock type
  uint                readLockCount;         // current number of read locks
  uint                readWriteLockCount;    // current number of read/write locks
  ThreadId            readWriteLockOwnedBy;  // current read/write lock owner thread
  uint                waitModifiedCount;     // current number of wait modified calls
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_cond_t      readLockZero;        // signal read-lock became 0
    pthread_cond_t      modified;            // signal values are modified
  #elif defined(PLATFORM_WINDOWS)
    pthread_cond_t      readLockZero;        // signal read-lock became 0
    pthread_cond_t      modified;            // signal values are modified
  #endif /* PLATFORM_... */
  bool                endFlag;

  #ifndef NDEBUG
    struct
    {
      const char            *fileName;       // file+line number of creation
      ulong                 lineNb;
      const char            *name;           // semaphore name (variable)
      __SemaphoreThreadInfo pendingBy[__SEMAPHORE_MAX_THREAD_INFO];  // threads who wait for semaphore
      uint                  pendingByCount;  // number of threads who wait for semaphore
      __SemaphoreThreadInfo lockedBy[__SEMAPHORE_MAX_THREAD_INFO];  // threads who locked semaphore
      uint                  lockedByCount;   // number of threads who locked semaphore

      __SemaphoreState      lastReadRequest;
      __SemaphoreState      lastReadWakeup;
      __SemaphoreState      lastReadLock;
      __SemaphoreState      lastReadUnlock;
      __SemaphoreState      lastReadWriteRequest;
      __SemaphoreState      lastReadWriteWakeup;
      __SemaphoreState      lastReadWriteLock;
      __SemaphoreState      lastReadWriteUnlock;

      __SemaphoreHistoryInfo history[__SEMAPHORE_MAX_THREAD_INFO];
      uint                   historyCount;
      uint                   historyIndex;
    } debug;
  #endif /* not NDEBUG */
} Semaphore;

typedef struct
{
  pthread_cond_t      condition;
} SemaphoreCondition;

// semaphore lock flag variable
typedef bool SemaphoreLock;

// semaphore modify types
typedef enum
{
  SEMAPHORE_SIGNAL_MODIFY_SINGLE,
  SEMAPHORE_SIGNAL_MODIFY_ALL
} SemaphoreSignalModifyTypes;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : SEMAPHORE_LOCKED_DO
* Purpose: execute block with semaphore locked
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            SEMAPHORE_LOCKED_DO(semaphore,semaphoreLockType,timeout)
*            {
*              ...
*            }
*
*          semaphore must be unlocked manually if 'break'  or
*          'return' is used!
\***********************************************************************/

#define SEMAPHORE_LOCKED_DO(semaphore,semaphoreLockType,timeout) \
  for (SemaphoreLock __semaphoreLock ## __COUNTER__ = Semaphore_lock(semaphore,semaphoreLockType,timeout); \
       __semaphoreLock ## __COUNTER__; \
       Semaphore_unlock(semaphore), __semaphoreLock ## __COUNTER__ = FALSE \
      )

#ifndef NDEBUG
  // 2 macros necessary, because of "string"-construction
  #define _SEMAPHORE_NAME(variable) _SEMAPHORE_NAME_INTERN(variable)
  #define _SEMAPHORE_NAME_INTERN(variable) #variable

  #define Semaphore_init(semaphore,type) __Semaphore_init(__FILE__,__LINE__,_SEMAPHORE_NAME(semaphore),semaphore,type)
  #define Semaphore_done(semaphore)      __Semaphore_done(__FILE__,__LINE__,semaphore)
  #define Semaphore_new(semaphore,type)  __Semaphore_new(__FILE__,__LINE__,_SEMAPHORE_NAME(semaphore),semaphore,type)
  #define Semaphore_delete(semaphore)    __Semaphore_delete(__FILE__,__LINE__,semaphore)
  #define Semaphore_lock(...)            __Semaphore_lock(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Semaphore_forceLock(...)       __Semaphore_forceLock(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Semaphore_unlock(...)          __Semaphore_unlock(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Semaphore_waitModified(...)    __Semaphore_waitModified(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Semaphore_init
* Purpose: initialize semaphore
* Input  : semaphore     - semaphore variable
*          semaphoreType - semaphore type (still not used!)
* Output : semaphore - semaphore
* Return : TRUE if semaphore initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_init(Semaphore *semaphore, SemaphoreTypes semaphoreType);
#else /* not NDEBUG */
bool __Semaphore_init(const char     *__fileName__,
                      ulong          __lineNb__,
                      const char     *name,
                      Semaphore      *semaphore,
                      SemaphoreTypes semaphoreType
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_done
* Purpose: done semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Semaphore_done(Semaphore *semaphore);
#else /* not NDEBUG */
void __Semaphore_done(const char *__fileName__,
                      ulong      __lineNb__,
                      Semaphore  *semaphore
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_new
* Purpose: create new semaphore
* Input  :
* Output : -
* Return : semaphore or NULL if insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Semaphore *Semaphore_new(SemaphoreTypes semaphoreType);
#else /* not NDEBUG */
Semaphore *__Semaphore_new(const char     *__fileName__,
                           ulong          __lineNb__,
                           const char     *name,
                           SemaphoreTypes semaphoreType
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_delete
* Purpose: delete semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Semaphore_delete(Semaphore *semaphore);
#else /* not NDEBUG */
void __Semaphore_delete(const char *__fileName__,
                        ulong      __lineNb__,
                        Semaphore  *semaphore
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_lock
* Purpose: lock semaphore
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type: READ, READ/WRITE
*          timeout           - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if locked, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType,
                    long               timeout
                   );
#else /* not NDEBUG */
bool __Semaphore_lock(const char         *__fileName__,
                      ulong              __lineNb__,
                      Semaphore          *semaphore,
                      SemaphoreLockTypes semaphoreLockType,
                      long               timeout
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_forceLock
* Purpose: force locking semaphore
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type: READ, READ/WRITE
* Output : -
* Return : -
* Notes  : if semaphore cannot be locked halt program with internal
*          error
\***********************************************************************/

#ifdef NDEBUG
INLINE void Semaphore_forceLock(Semaphore          *semaphore,
                                SemaphoreLockTypes semaphoreLockType
                               );
#if defined(NDEBUG) || defined(__SEMAPHORES_IMPLEMENATION__)
INLINE void Semaphore_forceLock(Semaphore          *semaphore,
                                SemaphoreLockTypes semaphoreLockType
                               )
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  if (!Semaphore_lock(semaphore,semaphoreLockType,WAIT_FOREVER))
  {
    HALT_INTERNAL_ERROR("Cannot lock semaphore at %s, %u",__FILE__,__LINE__);
  }
}
#endif /* NDEBUG || __SEMAPHORES_IMPLEMENATION__ */
#else /* not NDEBUG */
INLINE void __Semaphore_forceLock(const char         *__fileName__,
                                  ulong              __lineNb__,
                                  Semaphore          *semaphore,
                                  SemaphoreLockTypes semaphoreLockType
                                 );
#if defined(NDEBUG) || defined(__SEMAPHORES_IMPLEMENATION__)
INLINE void __Semaphore_forceLock(const char         *__fileName__,
                                  ulong              __lineNb__,
                                  Semaphore          *semaphore,
                                  SemaphoreLockTypes semaphoreLockType
                                 )
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  if (!__Semaphore_lock(__fileName__,__lineNb__,semaphore,semaphoreLockType,WAIT_FOREVER))
  {
    HALT_INTERNAL_ERROR("Cannot lock semaphore at %s, %lu",__fileName__,__lineNb__);
  }
}
#endif /* NDEBUG || __SEMAPHORES_IMPLEMENATION__ */
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_unlock
* Purpose: unlock semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Semaphore_unlock(Semaphore *semaphore);
#else /* not NDEBUG */
void __Semaphore_unlock(const char *__fileName__,
                        ulong      __lineNb__,
                        Semaphore  *semaphore
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_lockCount
* Purpose: get number locks (READ or READ/WRITE)
* Input  : semaphore - semaphore
* Output : -
* Return : number of locks
* Notes  : -
\***********************************************************************/

INLINE uint Semaphore_lockCount(Semaphore *semaphore);
#if defined(NDEBUG) || defined(__SEMAPHORES_IMPLEMENATION__)
INLINE uint Semaphore_lockCount(Semaphore *semaphore)
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  return semaphore->readLockCount+semaphore->readWriteLockCount;
}
#endif /* NDEBUG || __SEMAPHORES_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Semaphore_isLocked
* Purpose: check if semaphore is currently locked
* Input  : semaphore - semaphore
* Output : -
* Return : TRUE iff currently locked
* Notes  : -
\***********************************************************************/

INLINE bool Semaphore_isLocked(Semaphore *semaphore);
#if defined(NDEBUG) || defined(__SEMAPHORES_IMPLEMENATION__)
INLINE bool Semaphore_isLocked(Semaphore *semaphore)
{
  assert(semaphore != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(semaphore);

  return semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE;
}
#endif /* NDEBUG || __SEMAPHORES_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Semaphore_isOwned
* Purpose: check if semaphore is owned by calling thread
* Input  : semaphore - semaphore
*          threadId  - thread id
* Output : -
* Return : TRUE iff semaphore is owned by calling thread
* Notes  : for debugging only!
\***********************************************************************/

#ifndef NDEBUG
bool Semaphore_isOwned(const Semaphore *semaphore);
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : Semaphore_signalModified
* Purpose: signal semaphore is modified
* Input  : semaphore - semaphore
*          type      - signal modify type; see
*                      SEMAPHORE_SIGNAL_MODIFY_...
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_signalModified(Semaphore *semaphore, SemaphoreSignalModifyTypes type);

/***********************************************************************\
* Name   : Semaphore_waitModified
* Purpose: wait until semaphore is modified
* Input  : semaphore - semaphore
*          timeout   - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if modified, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_waitModified(Semaphore *semaphore,
                            long      timeout
                           );
#else /* not NDEBUG */
bool __Semaphore_waitModified(const char *__fileName__,
                              ulong      __lineNb__,
                              Semaphore  *semaphore,
                              long       timeout
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_waitCondition
* Purpose: wait until semaphore is modified
* Input  : semaphore - semaphore
*          timeout   - timeout [ms] or WAIT_FOREVER
* Output : -
* Return : TRUE if modified, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_waitCondition(SemaphoreCondition *condition,
                             Semaphore          *semaphore,
                             long               timeout
                            );
#else /* not NDEBUG */
bool __Semaphore_waitCondition(const char         *__fileName__,
                               ulong              __lineNb__,
                               SemaphoreCondition *condition,
                               Semaphore          *semaphore,
                               long               timeout
                              );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_isLockPending
* Purpose: check if another thread is pending for semaphore lock
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type: READ, READ/WRITE
* Output : -
* Return : TRUE iff another thread is pending for semaphore lock, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool Semaphore_isLockPending(Semaphore *semaphore, SemaphoreLockTypes semaphoreLockType);

/***********************************************************************\
* Name   : Semaphore_setEnd
* Purpose: set end flag for semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : trigger all waiting threads
\***********************************************************************/

void Semaphore_setEnd(Semaphore *semaphore);

#ifndef NDEBUG

/***********************************************************************\
* Name   : Semaphore_debugTrace
* Purpose: debug trace semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugTrace(const Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_debugTraceClear
* Purpose: debug trace semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugTraceClear(void);

/***********************************************************************\
* Name   : Semaphore_debugDump
* Purpose: print debug info
* Input  : semaphore - semaphore
*          handle    - output file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugDump(const Semaphore *semaphore, FILE *handle);

/***********************************************************************\
* Name   : Semaphore_debugDumpInfo
* Purpose: print debug info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugDumpInfo(FILE *handle);

/***********************************************************************\
* Name   : Semaphore_debugPrintInfo
* Purpose: print debug info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_debugPrintInfo(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __SEMAPHORES__ */

/* end of file */
