/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/semaphores.c,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: functions for inter-process semaphores
* Systems: all POSIX
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

#ifndef NDEBUG
typedef struct
{
  LIST_HEADER(Semaphore);
} DebugSemaphoreList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/

#define DEBUG_READ       FALSE
#define DEBUG_READ_WRITE FALSE

#ifndef NDEBUG
  LOCAL pthread_once_t     debugSemaphoreInitFlag = PTHREAD_ONCE_INIT;
  LOCAL DebugSemaphoreList debugSemaphoreList;
  LOCAL void(*debugPrevSignalHandler)(int)       ;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define LOCK(debugFlag,type,semaphore) \
    do { \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p wait lock %s\n",__FILE__,__LINE__,(void*)pthread_self(),type); \
      pthread_mutex_lock(semaphore); \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p locked %s\n",__FILE__,__LINE__,(void*)pthread_self(),type); \
    } while (0)

  #define UNLOCK(debugFlag,type,semaphore,n) \
    do { \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p unlock %s n=%d\n",__FILE__,__LINE__,(void*)pthread_self(),type,n); \
      pthread_mutex_unlock(semaphore); \
    } while (0)

  #define WAIT(debugFlag,type,condition,semaphore) \
    do { \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p unlock+wait %s\n",__FILE__,__LINE__,(void*)pthread_self(),type); \
      pthread_cond_wait(condition,semaphore); \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p waited+locked %s done\n",__FILE__,__LINE__,(void*)pthread_self(),type); \
    } while (0)

  #define SIGNAL(debugFlag,type,condition) \
    do { \
      if (debugFlag) fprintf(stderr,"%s,%4d: %p signal %s\n",__FILE__,__LINE__,(void*)pthread_self(),type); \
      pthread_cond_signal(condition); \
    } while (0)

#else /* NDEBUG */

  #define LOCK(debugFlag,type,semaphore) \
    do { \
      pthread_mutex_lock(semaphore); \
    } while (0)

  #define UNLOCK(debugFlag,type,semaphore,n) \
    do { \
      pthread_mutex_unlock(semaphore); \
    } while (0)

  #define WAIT(debugFlag,type,condition,semaphore) \
    do { \
      pthread_cond_wait(condition,semaphore); \
    } while (0)

  #define SIGNAL(debugFlag,type,condition) \
    do { \
      pthread_cond_broadcast(condition); \
    } while (0)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/
LOCAL void signalHandler(int signalNumber);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : Semaphore_debugInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void Semaphore_debugInit(void)
{
  // init variables
  List_init(&debugSemaphoreList);

  // init signal handler for Ctrl-\ (SIGQUIT) for printing debug information
  debugPrevSignalHandler = signal(SIGQUIT,signalHandler);
}

/***********************************************************************\
* Name   : Semaphore_debugPrintInfo
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void Semaphore_debugPrintInfo(void)
{
  Semaphore  *semaphore;
  const char *semaphoreState;
  uint       z;

  fprintf(stderr,"Semaphore debug info:\n");
  LIST_ITERATE(&debugSemaphoreList,semaphore)
  {
    switch (semaphore->lockType)
    {
      case SEMAPHORE_LOCK_TYPE_NONE:
        assert(semaphore->readLockCount == 0);
        assert(semaphore->readWriteLockCount == 0);
        semaphoreState = "none";
        break;
      case SEMAPHORE_LOCK_TYPE_READ:
        assert(semaphore->readWriteLockCount == 0);
        semaphoreState = "read";
        break;
      case SEMAPHORE_LOCK_TYPE_READ_WRITE:
        assert(semaphore->readLockCount == 0);
        semaphoreState = "read/write";
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    fprintf(stderr,"  '%s':\n",semaphore->name);
    fprintf(stderr,"    locked '%s'\n",semaphoreState);
    for (z = 0; z < semaphore->lockedByCount; z++)
    {
      fprintf(stderr,
              "    by thread 0x%lx at %s, line %lu\n",
              semaphore->lockedBy[z].thread,
              semaphore->lockedBy[z].fileName,
              semaphore->lockedBy[z].lineNb
             );
    }
  }
}

/***********************************************************************\
* Name   : signalHandler
* Purpose: signal handler
* Input  : signalNumber - signal number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void signalHandler(int signalNumber)
{
  if (signalNumber == SIGQUIT)
  {
    Semaphore_debugPrintInfo();
  }
  if (debugPrevSignalHandler != NULL)
  {
    debugPrevSignalHandler(signalNumber);
  }
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : lock
* Purpose: lock semaphore
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type; see SemaphoreLockTypes
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL void lock(Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType
               )
#else /* not NDEBUG */
LOCAL void lock(const char         *fileName,
                ulong              lineNb,
                Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType
               )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);
  assert((semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ) || (semaphoreLockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

  switch (semaphoreLockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
      // request read lock
      pthread_mutex_lock(&semaphore->requestLock);
      {
        semaphore->readRequestCount++;
      }
      pthread_mutex_unlock(&semaphore->requestLock);

      LOCK(DEBUG_READ,"R",&semaphore->lock);
      {
        assert(semaphore->readWriteLockCount == 0);

        #ifndef NDEBUG
          // debug lock trace code
          if (semaphore->lockedByCount < SIZE_OF_ARRAY(semaphore->lockedBy))
          {
            semaphore->lockedBy[semaphore->lockedByCount].thread   = pthread_self();
            semaphore->lockedBy[semaphore->lockedByCount].fileName = fileName;
            semaphore->lockedBy[semaphore->lockedByCount].lineNb   = lineNb;
            semaphore->lockedByCount++;
          }
          else
          {
            fprintf(stderr,
                    "DEBUG WARNING: too many thread locks for semaphore %p at %s, line %lu (max. %d)!\n",
                    semaphore,
                    fileName,
                    lineNb,
                    SIZE_OF_ARRAY(semaphore->lockedBy)
                   );
          }
        #endif /* not NDEBUG */

        // wait until no more read/write-locks
        while (semaphore->readWriteLockCount > 0)
        {
          WAIT(DEBUG_READ_WRITE,"R",&semaphore->modified,&semaphore->lock); 
        }
        assert(semaphore->readWriteLockCount == 0);

        // set/increment read-lock if there is no read/write-lock
        semaphore->readLockCount++;
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;

        // decrement read request counter
        pthread_mutex_lock(&semaphore->requestLock);
        {
          assert(semaphore->readRequestCount > 0);
          semaphore->readRequestCount--;
        }
        pthread_mutex_unlock(&semaphore->requestLock);
      }
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      // request write lock
      pthread_mutex_lock(&semaphore->requestLock);
      {
        semaphore->readWriteRequestCount++;
      }
      pthread_mutex_unlock(&semaphore->requestLock);

      // lock
      LOCK(DEBUG_READ_WRITE,"RW",&semaphore->lock);

      #ifndef NDEBUG
        // debug lock trace code
        if (semaphore->lockedByCount < SIZE_OF_ARRAY(semaphore->lockedBy))
        {
          semaphore->lockedBy[semaphore->lockedByCount].thread   = pthread_self();
          semaphore->lockedBy[semaphore->lockedByCount].fileName = fileName;
          semaphore->lockedBy[semaphore->lockedByCount].lineNb   = lineNb;
          semaphore->lockedByCount++;
        }
        else
        {
          fprintf(stderr,
                  "DEBUG WARNING: too many thread locks for semaphore '%s' at %s, line %lu (max. %d)!\n",
                  semaphore->name,
                  fileName,
                  lineNb,
                  SIZE_OF_ARRAY(semaphore->lockedBy)
                 );
        }
      #endif /* not NDEBUG */

      // wait until no more read-locks
      while (semaphore->readLockCount > 0)
      {
        WAIT(DEBUG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock); 
      }
      assert(semaphore->readLockCount == 0);

      // set/increment read/write-lock
      semaphore->readWriteLockCount++;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;

      // decrement write request counter
      pthread_mutex_lock(&semaphore->requestLock);
      {
        assert(semaphore->readWriteRequestCount > 0);
        semaphore->readWriteRequestCount--;
      }
      pthread_mutex_unlock(&semaphore->requestLock);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
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
  #ifndef NDEBUG
    pthread_t threadSelf;
    uint      z;
  #endif /* not NDEBUG */

  assert(semaphore != NULL);

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
pthread_yield();
      LOCK(DEBUG_READ,"R",&semaphore->lock);
      {
        assert(semaphore->readLockCount > 0);
        assert(semaphore->readWriteLockCount == 0);

        #ifndef NDEBUG
          // debug lock trace code
          threadSelf = pthread_self();
          z = 0;
          while (   (z < semaphore->lockedByCount)
                 && (pthread_equal(threadSelf,semaphore->lockedBy[z].thread) == 0)
                )
          {
            z++;
          }
          if (z < semaphore->lockedByCount)
          {
            memset(&semaphore->lockedBy[z],0,sizeof(semaphore->lockedBy[z]));
            if (semaphore->lockedByCount > 1)
            {
              semaphore->lockedBy[z] = semaphore->lockedBy[semaphore->lockedByCount-1];
              memset(&semaphore->lockedBy[semaphore->lockedByCount-1],0,sizeof(semaphore->lockedBy[semaphore->lockedByCount-1]));
            }
            semaphore->lockedByCount--;
          }
          else
          {
            Semaphore_debugPrintInfo();
            HALT_INTERNAL_ERROR("Thread 0x%lx try to unlock not locked semaphore '%s' at %s, line %lu!",
                                threadSelf,
                                semaphore->name,
                                fileName,
                                lineNb
                               );
          }
        #endif /* not NDEBUG */

pthread_yield();
        // do one read-unlock
        semaphore->readLockCount--;
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

pthread_yield();
          // signal that read-lock count become 0
          SIGNAL(DEBUG_READ,"READ0 (unlock)",&semaphore->readLockZero);
        }
      }
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
pthread_yield();
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);
      assert(semaphore->readWriteLockCount > 0);

pthread_yield();
      #ifndef NDEBUG
        // debug lock trace code
        threadSelf = pthread_self();
        z = 0;
        while (   (z < semaphore->lockedByCount)
               && (pthread_equal(threadSelf,semaphore->lockedBy[z].thread) == 0)
              )
        {
          z++;
        }
        if (z < semaphore->lockedByCount)
        {
          memset(&semaphore->lockedBy[z],0,sizeof(semaphore->lockedBy[z]));
          if (semaphore->lockedByCount > 1)
          {
            semaphore->lockedBy[z] = semaphore->lockedBy[semaphore->lockedByCount-1];
            memset(&semaphore->lockedBy[semaphore->lockedByCount-1],0,sizeof(semaphore->lockedBy[semaphore->lockedByCount-1]));
          }
          semaphore->lockedByCount--;
        }
        else
        {
          Semaphore_debugPrintInfo();
          HALT_INTERNAL_ERROR("Thread 0x%lx try to unlock not locked semaphore '%s' at %s, line %lu!",
                              threadSelf,
                              semaphore->name,
                              fileName,
                              lineNb
                             );
        }
      #endif /* not NDEBUG */

pthread_yield();
      // do one read/write-unlock
      semaphore->readWriteLockCount--;
      if (semaphore->readWriteLockCount == 0)
      {
        // semaphore is free
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

pthread_yield();
        // send modified signal
        SIGNAL(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified);

        // unlock
        UNLOCK(DEBUG_READ_WRITE,"RW",&semaphore->lock,semaphore->readLockCount);
      }
pthread_yield();
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

/***********************************************************************\
* Name   : isLocked
* Purpose: check if semaphore is locked
* Input  : semaphore - semaphore
* Output : -
* Return : TRUE if semaphore is locked, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool isLocked(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  return semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE;
}

/***********************************************************************\
* Name   : waitModified
* Purpose: wait until semaphore is modified
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL void waitModified(Semaphore *semaphore)
#else /* not NDEBUG */
LOCAL void waitModified(const char *fileName, ulong lineNb, Semaphore *semaphore)
#endif /* NDEBUG */
{
  uint savedReadWriteLockCount;

  assert(semaphore != NULL);
  assert(semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE);
  assert((semaphore->readLockCount > 0) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

  #ifndef NDEBUG
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(lineNb);
  #endif /* not NDEBUG */

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_NONE:
      break;
    case SEMAPHORE_LOCK_TYPE_READ:
      LOCK(DEBUG_READ,"R",&semaphore->lock);
      {
        assert(semaphore->readLockCount > 0);
        assert(semaphore->readWriteLockCount == 0);

pthread_yield();
        // temporary revert read-lock
        semaphore->readLockCount--;
        if (semaphore->readLockCount == 0)
        {
          // semaphore is free
          semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

          // signal that read-lock count become 0
          SIGNAL(DEBUG_READ,"READ0 (wait)",&semaphore->readLockZero);
        }
        SIGNAL(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified);

pthread_yield();
        // wait for modification
        WAIT(DEBUG_READ,"MODIFIED",&semaphore->modified,&semaphore->lock); 

pthread_yield();
        // wait until there are no more write-locks
        while (semaphore->readWriteLockCount > 0)
        {
          WAIT(DEBUG_READ,"W",&semaphore->modified,&semaphore->lock); 
        }

pthread_yield();
        // restore temporary reverted read-lock
        semaphore->readLockCount++;
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;
      }
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
pthread_yield();
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);
      assert(semaphore->readWriteLockCount > 0);

pthread_yield();
      // temporary revert write-lock
      savedReadWriteLockCount = semaphore->readWriteLockCount;
      semaphore->readWriteLockCount = 0;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;
      SIGNAL(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified);

pthread_yield();
      // wait for modification
      WAIT(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified,&semaphore->lock);

pthread_yield();
      // request write-lock
      pthread_mutex_lock(&semaphore->requestLock);
      {
        semaphore->readWriteRequestCount++;
      }
      pthread_mutex_unlock(&semaphore->requestLock);

pthread_yield();
      // wait until no more read-locks
      while (semaphore->readLockCount > 0)
      {
        WAIT(DEBUG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock); 
      }
      assert(semaphore->readLockCount == 0);

pthread_yield();
      // decrement write request counter
      pthread_mutex_lock(&semaphore->requestLock);
      {
        assert(semaphore->readWriteRequestCount > 0);
        semaphore->readWriteRequestCount--;
      }
      pthread_mutex_unlock(&semaphore->requestLock);

pthread_yield();
      // restore temporary reverted write-lock
      assert(semaphore->readWriteLockCount == 0);
      semaphore->readWriteLockCount = savedReadWriteLockCount;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
pthread_yield();
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
bool Semaphore_init(Semaphore *semaphore)
#else /* not NDEBUG */
bool __Semaphore_init(const char *name, Semaphore *semaphore)
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  if (pthread_mutex_init(&semaphore->requestLock,NULL) != 0)
  {
    return FALSE;
  }
  semaphore->readRequestCount      = 0;
  semaphore->readWriteRequestCount = 0;

#if 0
  pthread_mutexattr_init(&semaphore->lockAttributes);
  pthread_mutexattr_settype(&semaphore->lockAttributes,PTHREAD_MUTEX_RECURSIVE);
#endif /* 0 */
  if (pthread_mutex_init(&semaphore->lock,NULL) != 0)
  {
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->readLockZero,NULL) != 0)
  {
    pthread_mutex_destroy(&semaphore->lock);
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->modified,NULL) != 0)
  {
    pthread_cond_destroy(&semaphore->readLockZero);
    pthread_mutex_destroy(&semaphore->lock);
//    pthread_mutexattr_destroy(&semaphore->lockAttributes);
    pthread_mutex_destroy(&semaphore->requestLock);
    return FALSE;
  }
  semaphore->lockType           = SEMAPHORE_LOCK_TYPE_NONE;
  semaphore->readLockCount      = 0;
  semaphore->readWriteLockCount = 0;
  semaphore->endFlag            = FALSE;

  #ifndef NDEBUG
    pthread_once(&debugSemaphoreInitFlag,Semaphore_debugInit);

    semaphore->name          = name;
    memset(semaphore->lockedBy,0,sizeof(semaphore->lockedBy));
    semaphore->lockedByCount = 0;

    List_append(&debugSemaphoreList,semaphore);
  #endif /* not NDEBUG */

  return TRUE;
}

void Semaphore_done(Semaphore *semaphore)
{
  #ifndef NDEBUG
    int z;
  #endif /* not NDEBUG */

  assert(semaphore != NULL);

  /* lock to avoid further usage */
  LOCK(DEBUG_READ_WRITE,"D",&semaphore->lock);

  #ifndef NDEBUG
    pthread_once(&debugSemaphoreInitFlag,Semaphore_debugInit);

    for (z = 0; z < semaphore->lockedByCount; z++)
    {
      fprintf(stderr,
              "DEBUG WARNING: thread 0x%lx did not unlocked semaphore '%s' locked at %s, line %lu!\n",
              semaphore->lockedBy[z].thread,
              semaphore->name,
              semaphore->lockedBy[z].fileName,
              semaphore->lockedBy[z].lineNb
             );
    }

    List_remove(&debugSemaphoreList,semaphore);
  #endif /* not NDEBUG */

  /* free resources */
  pthread_cond_destroy(&semaphore->modified);
  pthread_cond_destroy(&semaphore->readLockZero);
  pthread_mutex_destroy(&semaphore->lock);
//  pthread_mutexattr_destroy(&semaphore->lockAttributes);
  pthread_mutex_destroy(&semaphore->requestLock);
}

#ifdef NDEBUG
Semaphore *Semaphore_new(void)
#else /* not NDEBUG */
Semaphore *__Semaphore_new(const char *name)
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
      if (!__Semaphore_init(name,semaphore))
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
void Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType
                   )
#else /* not NDEBUG */
void __Semaphore_lock(const char         *fileName,
                      ulong              lineNb,
                      Semaphore          *semaphore,
                      SemaphoreLockTypes semaphoreLockType
                     )
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  #ifdef NDEBUG
    lock(semaphore,semaphoreLockType);
  #else /* not NDEBUG */
    lock(fileName,lineNb,semaphore,semaphoreLockType);
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

bool Semaphore_isLocked(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  return isLocked(semaphore);
}

#ifdef NDEBUG
void Semaphore_waitModified(Semaphore *semaphore)
#else /* not NDEBUG */
void __Semaphore_waitModified(const char *fileName, ulong lineNb, Semaphore *semaphore)
#endif /* NDEBUG */
{
  assert(semaphore != NULL);

  if (!semaphore->endFlag)
  {
    #ifdef NDEBUG
      waitModified(semaphore); 
    #else /* not NDEBUG */
      waitModified(fileName,lineNb,semaphore); 
    #endif /* NDEBUG */
  }
}

bool Semaphore_checkPending(Semaphore *semaphore)
{
  bool pendingFlag;

  assert(semaphore != NULL);

  pendingFlag = FALSE;
  if (!semaphore->endFlag)
  {
    switch (semaphore->lockType)
    {
      case SEMAPHORE_LOCK_TYPE_NONE:
        pendingFlag = FALSE;
        break;
      case SEMAPHORE_LOCK_TYPE_READ:
        pendingFlag = (semaphore->readWriteRequestCount > 0);
        break;
      case SEMAPHORE_LOCK_TYPE_READ_WRITE:
        pendingFlag = (semaphore->readRequestCount > 0);
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
    lock(semaphore,SEMAPHORE_LOCK_TYPE_READ);
  #else /* not NDEBUG */
    lock(__FILE__,__LINE__,semaphore,SEMAPHORE_LOCK_TYPE_READ);
  #endif /* NDEBUG */

  // set end flag
  semaphore->endFlag = TRUE;

  // send modified signal
  SIGNAL(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified);

  // unlock
  #ifdef NDEBUG
    unlock(semaphore);
  #else /* not NDEBUG */
    unlock(__FILE__,__LINE__,semaphore);
  #endif /* NDEBUG */
}

#ifdef __cplusplus
  }
#endif

/* end of file */
