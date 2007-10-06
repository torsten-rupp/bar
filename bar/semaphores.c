/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/semaphores.c,v $
* $Revision: 1.1 $
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

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL void lock(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  pthread_mutex_lock(&semaphore->lock);
  semaphore->lockCount++;
}

LOCAL void unlock(Semaphore *semaphore)
{
  assert(semaphore != NULL);
  assert(semaphore->lockCount > 0);

  if (semaphore->lockCount == 1)
  {
    pthread_cond_broadcast(&semaphore->modified);
  }

  semaphore->lockCount--;
  pthread_mutex_unlock(&semaphore->lock);
}

LOCAL void waitModified(Semaphore *semaphore)
{
  uint lockCount;
  uint z;

  assert(semaphore != NULL);
  assert(semaphore->lockCount > 0);

  lockCount = semaphore->lockCount;

  for (z = 1; z < lockCount; z++) pthread_mutex_unlock(&semaphore->lock);
  semaphore->lockCount = 0;
  pthread_cond_wait(&semaphore->modified,&semaphore->lock); 
  semaphore->lockCount = lockCount;
  for (z = 1; z < lockCount; z++) pthread_mutex_lock(&semaphore->lock);
}

/*---------------------------------------------------------------------*/

bool Semaphore_init(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  pthread_mutexattr_init(&semaphore->lockAttributes);
  pthread_mutexattr_settype(&semaphore->lockAttributes,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&semaphore->lock,&semaphore->lockAttributes) != 0)
  {
    return FALSE;
  }
  if (pthread_cond_init(&semaphore->modified,NULL) != 0)
  {
    pthread_mutex_destroy(&semaphore->lock);
    return FALSE;
  }
  semaphore->lockCount = 0;
  semaphore->endFlag   = FALSE;

  return TRUE;
}

void Semaphore_done(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  /* lock */
  lock(semaphore);

  /* free resources */
  pthread_cond_destroy(&semaphore->modified);
  pthread_mutex_destroy(&semaphore->lock);
  pthread_mutexattr_destroy(&semaphore->lockAttributes);
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

void Semaphore_lock(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  lock(semaphore);
}

void Semaphore_unlock(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  unlock(semaphore);
}

void Semaphore_wait(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  if (!semaphore->endFlag)
  {
    waitModified(semaphore); 
  }
}

void Semaphore_setEnd(Semaphore *semaphore)
{
  assert(semaphore != NULL);

  /* lock */
  lock(semaphore);

  semaphore->endFlag = TRUE;

  /* unlock */
  unlock(semaphore);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
