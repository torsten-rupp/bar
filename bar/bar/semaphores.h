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
#endif /* PLATFORM_... */

#include "global.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define SEMAPHORE_NO_WAIT      0L
#define SEMAPHORE_WAIT_FOREVER -1L

/***************************** Datatypes *******************************/

// lock types
typedef enum
{
  SEMAPHORE_LOCK_TYPE_NONE,
  SEMAPHORE_LOCK_TYPE_READ,
  SEMAPHORE_LOCK_TYPE_READ_WRITE,
} SemaphoreLockTypes;

typedef struct Semaphore
{
  #ifndef NDEBUG
    LIST_NODE_HEADER(struct Semaphore);
  #endif /* not NDEBUG */


  #if   defined(PLATFORM_LINUX)
    pthread_mutex_t     requestLock;         // lock to update request counters
  #elif defined(PLATFORM_WINDOWS)
    HANDLE              requestLock;
  #endif /* PLATFORM_... */
  uint                readRequestCount;      // number of pending read locks
  uint                readWriteRequestCount; // number of pending read/write locks

  #if   defined(PLATFORM_LINUX)
    pthread_mutex_t     lock;                /* lock (thread who own lock is allowed
                                                to change the following semaphore
                                                variables)
                                             */
  #elif defined(PLATFORM_WINDOWS)
    HANDLE              lock;
  #endif /* PLATFORM_... */
//  pthread_mutexattr_t lockAttributes;

  SemaphoreLockTypes  lockType;              // current lock type
  uint                readLockCount;         // number of read locks
  uint                readWriteLockCount;    // number of read/write locks
  #if   defined(PLATFORM_LINUX)
    pthread_cond_t      readLockZero;        // signal read-lock became 0
    pthread_cond_t      modified;            // signal values are modified
  #elif defined(PLATFORM_WINDOWS)
    pthread_cond_t      readLockZero;        // signal read-lock beaome 0
    pthread_cond_t      modified;            // signal values are modified
  #endif /* PLATFORM_... */
  bool                endFlag;

  // debug data
  #ifndef NDEBUG
    const char *name;
    struct
    {
      pthread_t  thread;
      const char *fileName;
      ulong      lineNb;
    } lockedBy[16];                          // threads who locked semaphore
    uint       lockedByCount;                // number of threadds who locked semaphore
  #endif /* not NDEBUG */
} Semaphore;

// semaphore lock flag variable
typedef bool SemaphoreLock;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : SEMAPHORE_LOCKED_DO
* Purpose: execute block with semaphore locked
* Input  : semaphoreLock     - lock flag variable (SemaphoreLock)
*          semaphore         - semaphore
*          semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or SEMAPORE_WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            SemaphoreLock semaphoreLock;
*            SEMAPHORE_LOCKED_DO(semaphoreLock,semaphore,semaphoreLockType,timeout)
*            {
*              ...
*            }
\***********************************************************************/

#define SEMAPHORE_LOCKED_DO(semaphoreLock,semaphore,semaphoreLockType,timeout) \
  for (semaphoreLock = Semaphore_lock(semaphore,semaphoreLockType,timeout); \
       semaphoreLock; \
       Semaphore_unlock(semaphore), semaphoreLock = FALSE \
      )

#ifndef NDEBUG
  // 2 macros necessary, because of "string"-construction
  #define _SEMAPHORE_NAME(variable) _SEMAPHORE_NAME_INTERN(variable)
  #define _SEMAPHORE_NAME_INTERN(variable) #variable

  #define Semaphore_init(semaphore) __Semaphore_init(_SEMAPHORE_NAME(semaphore),semaphore)
  #define Semaphore_new(semaphore) __Semaphore_new(_SEMAPHORE_NAME(semaphore),semaphore)
  #define Semaphore_lock(semaphore,semaphoreLockType,timeout) __Semaphore_lock(__FILE__,__LINE__,semaphore,semaphoreLockType,timeout)
  #define Semaphore_forceLock(semaphore,semaphoreLockType) __Semaphore_forceLock(__FILE__,__LINE__,semaphore,semaphoreLockType)
  #define Semaphore_unlock(semaphore) __Semaphore_unlock(__FILE__,__LINE__,semaphore)
  #define Semaphore_waitModified(semaphore,timeout) __Semaphore_waitModified(__FILE__,__LINE__,semaphore,timeout)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : SEMAPHORE_ASSERT_OWNERSHIP
* Purpose: check ownership of semaphore
* Input  : semaphore - semaphore to check
* Output : -
* Return : -
* Notes  : in debug mode stop if calling thread does not own semaphore
\***********************************************************************/

#ifndef NDEBUG
  #define SEMAPHORE_ASSERT_OWNERSHIP(semaphore) \
    do \
    { \
      assert((semaphore)->lockedByCount > 0); \
      assert(pthread_equal((semaphore)->lockedBy[(semaphore)->lockedByCount-1].thread,pthread_self()) != 0); \
    } \
    while (0)
#else /* NDEBUG */
  #define SEMAPHORE_ASSERT_OWNERSHIP(sempahore) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Semaphore_init
* Purpose: initialize semaphore
* Input  : -
* Output : semaphore - initialized semaphore
* Return : TRUE if semaphore initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_init(Semaphore *semaphore);
#else /* not NDEBUG */
bool __Semaphore_init(const char *name, Semaphore *semaphore);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_done
* Purpose: free semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_done(Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_new
* Purpose: create new semaphore
* Input  : -
* Output : -
* Return : semaphore or NULL if insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Semaphore *Semaphore_new(void);
#else /* not NDEBUG */
Semaphore *__Semaphore_new(const char *name);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_delete
* Purpose: delete semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_delete(Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_lock
* Purpose: lock semaphore
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type: READ, READ/WRITE
*          timeout           - timeout [ms] or SEMAPHORE_WAIT_FOREVER
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
bool __Semaphore_lock(const char         *fileName,
                      ulong              lineNb,
                      Semaphore          *semaphore,
                      SemaphoreLockTypes semaphoreLockType,
                      long               timeout
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_forceLock
* Purpose: foce locking semaphore
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

  if (!Semaphore_lock(semaphore,semaphoreLockType,SEMAPHORE_WAIT_FOREVER))
  {
    HALT_INTERNAL_ERROR("Cannot lock semaphore at %s, %lu",__FILE__,__LINE__);
  }
}
#endif /* NDEBUG || __SEMAPHORES_IMPLEMENATION__ */
#else /* not NDEBUG */
INLINE void __Semaphore_forceLock(const char         *fileName,
                                  ulong              lineNb,
                                  Semaphore          *semaphore,
                                  SemaphoreLockTypes semaphoreLockType
                                 );
#if defined(NDEBUG) || defined(__SEMAPHORES_IMPLEMENATION__)
INLINE void __Semaphore_forceLock(const char         *fileName,
                                  ulong              lineNb,
                                  Semaphore          *semaphore,
                                  SemaphoreLockTypes semaphoreLockType
                                 )
{
  assert(semaphore != NULL);

  if (!__Semaphore_lock(fileName,lineNb,semaphore,semaphoreLockType,SEMAPHORE_WAIT_FOREVER))
  {
    HALT_INTERNAL_ERROR("Cannot lock semaphore at %s, %lu",fileName,lineNb);
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
void __Semaphore_unlock(const char *fileName, ulong lineNb, Semaphore *semaphore);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Semaphore_isLocked
* Purpose: check if semaphore is currently locked
* Input  : semaphore - semaphore
* Output : -
* Return : TRUE iff currently locked
* Notes  : -
\***********************************************************************/

bool Semaphore_isLocked(Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_waitModified
* Purpose: wait until semaphore is modified
* Input  : semaphore - semaphore
*          timeout   - timeout [ms] or SEMAPHORE_WAIT_FOREVER
* Output : -
* Return : TRUE if modified, FALSE on timeout
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
bool Semaphore_waitModified(Semaphore *semaphore,
                            long      timeout
                           );
#else /* not NDEBUG */
bool __Semaphore_waitModified(const char *fileName,
                              ulong      lineNb,
                              Semaphore  *semaphore,
                              long       timeout
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

#ifdef __cplusplus
  }
#endif

#endif /* __SEMAPHORES__ */

/* end of file */
