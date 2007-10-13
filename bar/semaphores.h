/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/semaphores.h,v $
* $Revision: 1.2 $
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

typedef struct
{
  pthread_mutex_t     lock;
  pthread_mutexattr_t lockAttributes;
  uint                lockCount;
  pthread_cond_t      modified;
  bool                endFlag;
} Semaphore;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

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

void Semaphore_lock(Semaphore *semaphore);

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
* Name   : Semaphore_waitModified
* Purpose: wait until semaphore modified
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_waitModified(Semaphore *semaphore);

/***********************************************************************\
* Name   : Semaphore_setEndOfMail
* Purpose: set end-of-mail flag for semaphore
* Input  : semaphore - semaphore
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Semaphore_setEnd(Semaphore *semaphore);

#ifdef __cplusplus
  }
#endif

#endif /* __SEMAPHORES__ */

/* end of file */
