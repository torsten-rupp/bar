/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/semaphores.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: functions for inter-process semaphores
* Systems: all POSIX
*
\***********************************************************************/

#ifndef __SEMAPHORES__
#define __SEMAPHORES__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  SEMAPHORE_LOCK_TYPE_NONE,
  SEMAPHORE_LOCK_TYPE_READ,
  SEMAPHORE_LOCK_TYPE_READ_WRITE,
} SemaphoreLockTypes;

typedef struct
{
  pthread_mutex_t     requestLock;       // lock to update request counters
  pthread_mutex_t     lock;              // lock
//  pthread_mutexattr_t lockAttributes;
  SemaphoreLockTypes  lockType;          // current lock type
  uint                readRequestCount;  // number of pending read locks
  uint                readLockCount;     // number of read locks
  uint                writeRequestCount; // number of pending read/write locks
  uint                writeLockCount;    // number of read/write locks
  pthread_cond_t      readLockZero;      // signal read-lock is 0
  pthread_cond_t      modified;          // signal modified
  bool                endFlag;
} Semaphore;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : SEMAPHORE_LOCKED_DO
* Purpose: execute block with semaphore locked
* Input  : semaphore         - semaphore
*          semaphoreLockType - lock type
* Output : -
* Return : -
* Notes  : usage:
*            SEMAPHORE_LOCKED_DO(semaphore,semaphoreLockType)
*            {
*              ...
*            }
\***********************************************************************/

#define SEMAPHORE_LOCKED_DO(semaphore,semaphoreLockType) \
  for (Semaphore_lock(semaphore,semaphoreLockType); Semaphore_isLocked(semaphore); Semaphore_unlock(semaphore))

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
* Return : -
* Notes  : -
\***********************************************************************/

bool Semaphore_init(Semaphore *semaphore);

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

Semaphore *Semaphore_new(void);

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
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_lock(Semaphore          *semaphore,
                    SemaphoreLockTypes semaphoreLockType
                   );

/***********************************************************************\
* Name   : Semaphore_unlock
* Purpose: unlock semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_unlock(Semaphore *semaphore);

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
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_waitModified(Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_checkPending
* Purpose: check if thread is pending for semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : TRUE iff other thread is pending for semaphore, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool Semaphore_checkPending(Semaphore *semaphore);

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
