/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/mailboxes.c,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: functions for inter-process mailboxes
* Systems: all POSIX
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "mailboxes.h"

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

LOCAL void lock(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  pthread_mutex_lock(&mailbox->lock);
  mailbox->lockCount++;
}

LOCAL void unlock(Mailbox *mailbox)
{
  assert(mailbox != NULL);
  assert(mailbox->lockCount > 0);

  if (mailbox->lockCount == 1)
  {
    pthread_cond_broadcast(&mailbox->modified);
  }

  mailbox->lockCount--;
  pthread_mutex_unlock(&mailbox->lock);
}

LOCAL void waitModified(Mailbox *mailbox)
{
  uint lockCount;
  uint z;

  assert(mailbox != NULL);
  assert(mailbox->lockCount > 0);

  lockCount = mailbox->lockCount;

  for (z = 1; z < lockCount; z++) pthread_mutex_unlock(&mailbox->lock);
  mailbox->lockCount = 0;
  pthread_cond_wait(&mailbox->modified,&mailbox->lock); 
  mailbox->lockCount = lockCount;
  for (z = 1; z < lockCount; z++) pthread_mutex_lock(&mailbox->lock);
}

/*---------------------------------------------------------------------*/

bool Mailbox_init(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  pthread_mutexattr_init(&mailbox->lockAttributes);
  pthread_mutexattr_settype(&mailbox->lockAttributes,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&mailbox->lock,&mailbox->lockAttributes) != 0)
  {
    return FALSE;
  }
  if (pthread_cond_init(&mailbox->modified,NULL) != 0)
  {
    pthread_mutex_destroy(&mailbox->lock);
    return FALSE;
  }
  mailbox->lockCount     = 0;
  mailbox->endOfMailFlag = FALSE;

  return TRUE;
}

void Mailbox_done(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  /* lock */
  lock(mailbox);

  /* free resources */
  pthread_cond_destroy(&mailbox->modified);
  pthread_mutex_destroy(&mailbox->lock);
  pthread_mutexattr_destroy(&mailbox->lockAttributes);
}

Mailbox *Mailbox_new(void)
{
  Mailbox *mailbox;

  mailbox = (Mailbox*)malloc(sizeof(Mailbox));
  if (mailbox != NULL)
  {
    if (!Mailbox_init(mailbox))
    {
      free(mailbox);
      return NULL;
    }
  }
  else
  {
    return NULL;
  }

  return mailbox;
}

void Mailbox_delete(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  Mailbox_done(mailbox);
  free(mailbox);
}

void Mailbox_lock(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  lock(mailbox);
}

void Mailbox_unlock(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  unlock(mailbox);
}

void Mailbox_wait(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  /* lock */
  lock(mailbox);

  if (!mailbox->endOfMailFlag)
  {
    waitModified(mailbox); 
  }

  /* unlock */
  unlock(mailbox);
}

void Mailbox_setEndOfMail(Mailbox *mailbox)
{
  assert(mailbox != NULL);

  /* lock */
  lock(mailbox);

  mailbox->endOfMailFlag = TRUE;

  /* unlock */
  unlock(mailbox);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
