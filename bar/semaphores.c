/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/semaphores.c,v $
* $Revision: 1.3 $
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
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

#define DEBUG_READ       FALSE
#define DEBUG_READ_WRITE FALSE

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

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL void lock(Semaphore          *semaphore,
                SemaphoreLockTypes semaphoreLockType
               )
{
  assert(semaphore != NULL);

  switch (semaphoreLockType)
  {
    case SEMAPHORE_LOCK_TYPE_READ:
      // request read lock
      pthread_mutex_lock(&semaphore->requestLock);
      semaphore->readRequestCount++;
      pthread_mutex_unlock(&semaphore->requestLock);

      // lock
      LOCK(DEBUG_READ,"R",&semaphore->lock);

      // wait until no more write-requests/write-locks
      while ((semaphore->writeRequestCount > 0) || (semaphore->writeLockCount > 0))
      {
        WAIT(DEBUG_READ,"RW",&semaphore->modified,&semaphore->lock); 
      }

      // set read-lock
      assert(semaphore->readRequestCount > 0);
      semaphore->readRequestCount--;
      semaphore->readLockCount++;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;

      // unlock
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      // request write lock
      pthread_mutex_lock(&semaphore->requestLock);
      semaphore->writeRequestCount++;
      pthread_mutex_unlock(&semaphore->requestLock);

      // lock
      LOCK(DEBUG_READ_WRITE,"RW",&semaphore->lock);

      // wait until no more read-locks
      while (semaphore->readLockCount > 0)
      {
        WAIT(DEBUG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock); 
      }
      assert(semaphore->readLockCount == 0);

      // set read/write-lock
      assert(semaphore->writeRequestCount > 0);
      semaphore->writeRequestCount--;
      semaphore->writeLockCount++;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL void unlock(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_READ:
      // lock
      LOCK(DEBUG_READ,"R",&semaphore->lock);

      assert(semaphore->readLockCount > 0);
      semaphore->readLockCount--;
      if (semaphore->readLockCount == 0)
      {
        // semaphore is free
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

        // signal that read-lock count become 0
        SIGNAL(DEBUG_READ,"READ0 (unlock)",&semaphore->readLockZero);
      }

      // unlock
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);

      assert(semaphore->writeLockCount > 0);
      semaphore->writeLockCount--;

      // semaphore is free
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

      // send modified signal
      SIGNAL(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified);

      // unlock
      UNLOCK(DEBUG_READ_WRITE,"RW",&semaphore->lock,semaphore->readLockCount);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL void waitModified(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  assert(semaphore->lockType != SEMAPHORE_LOCK_TYPE_NONE);
  assert((semaphore->readLockCount > 0) || (semaphore->lockType == SEMAPHORE_LOCK_TYPE_READ_WRITE));

  switch (semaphore->lockType)
  {
    case SEMAPHORE_LOCK_TYPE_READ:
      // lock
      LOCK(DEBUG_READ,"R",&semaphore->lock);

      // temporary revert read-lock
      assert(semaphore->readLockCount > 0);
      semaphore->readLockCount--;
      if (semaphore->readLockCount == 0)
      {
        // semaphore is free
        semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

        // signal that read-lock count become 0
        SIGNAL(DEBUG_READ,"READ0 (wait)",&semaphore->readLockZero);
      }

      // wait for modification
      WAIT(DEBUG_READ,"MODIFIED",&semaphore->modified,&semaphore->lock); 

      // wait until no more write-requests/write-locks
      while ((semaphore->writeRequestCount > 0) || (semaphore->writeLockCount > 0))
      {
        WAIT(DEBUG_READ,"W",&semaphore->modified,&semaphore->lock); 
      }

      // restore read-lock
      semaphore->readLockCount++;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ;

      // unlock
      UNLOCK(DEBUG_READ,"R",&semaphore->lock,semaphore->readLockCount);
      break;
    case SEMAPHORE_LOCK_TYPE_READ_WRITE:
      assert(semaphore->readLockCount == 0);

      // temporary revert read/write-lock
      assert(semaphore->writeLockCount > 0);
      semaphore->writeLockCount--;
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_NONE;

      // wait for modification
      WAIT(DEBUG_READ_WRITE,"MODIFIED",&semaphore->modified,&semaphore->lock);

      // request write lock
      semaphore->writeLockCount++;

      // wait until no more read-locks
      while (semaphore->readLockCount > 0)
      {
        WAIT(DEBUG_READ_WRITE,"R",&semaphore->readLockZero,&semaphore->lock); 
      }
      assert(semaphore->readLockCount == 0);

      // restore read-lock
      semaphore->lockType = SEMAPHORE_LOCK_TYPE_READ_WRITE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

/*---------------------------------------------------------------------*/

bool Semaphore_init(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  if (pthread_mutex_init(&semaphore->requestLock,NULL) != 0)
  {
    return FALSE;
  }
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
  semaphore->lockType          = SEMAPHORE_LOCK_TYPE_NONE;
  semaphore->readRequestCount  = 0;
  semaphore->readLockCount     = 0;
  semaphore->writeRequestCount = 0;
  semaphore->writeLockCount    = 0;
  semaphore->endFlag           = FALSE;

  return TRUE;
}

void Semaphore_done(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  /* lock */
  lock(semaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE);

  /* free resources */
  pthread_cond_destroy(&semaphore->modified);
  pthread_cond_destroy(&semaphore->readLockZero);
  pthread_mutex_destroy(&semaphore->lock);
//  pthread_mutexattr_destroy(&semaphore->lockAttributes);
  pthread_mutex_destroy(&semaphore->requestLock);
}

Semaphore *Semaphore_new(void)
{
  Semaphore *semaphore;

  semaphore = (Semaphore*)malloc(sizeof(Semaphore));
  if (semaphore != NULL)
  {
    if (!Semaphore_init(semaphore))
    {
      free(semaphore);
      return NULL;
    }
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

void Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType
                   )
{
  assert(semaphore != NULL);

  lock(semaphore,semaphoreLockType);
}

void Semaphore_unlock(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  unlock(semaphore);
}

void Semaphore_waitModified(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  if (!semaphore->endFlag)
  {
    waitModified(semaphore); 
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
        pendingFlag = (semaphore->writeRequestCount > 0);
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

  /* lock */
  lock(semaphore,SEMAPHORE_LOCK_TYPE_READ);

  semaphore->endFlag = TRUE;

  /* unlock */
  unlock(semaphore);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
