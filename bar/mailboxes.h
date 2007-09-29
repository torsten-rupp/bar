/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/mailboxes.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: functions for inter-process mailboxes
* Systems: all POSIX
*
\***********************************************************************/

#ifndef __MAILBOX__
#define __MAILBOX__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  pthread_mutex_t lock;
  pthread_cond_t  modified;
  uint            lockCount;
  pthread_t       lockThread;
  bool            endOfMailFlag;
} Mailbox;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Mailbox_init
* Purpose: initialize mailbox
* Input  : -
* Output : mailbox - initialized mailbox
* Return : -
* Notes  : -
\***********************************************************************/

bool Mailbox_init(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_done
* Purpose: free mailbox
* Input  : mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_done(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_new
* Purpose: create new mailbox
* Input  : -
* Output : -
* Return : mailbox or NULL if insufficient memory
* Notes  : -
\***********************************************************************/

Mailbox *Mailbox_new(void);

/***********************************************************************\
* Name   : Mailbox_delete
* Purpose: delete mailbox
* Input  : mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_delete(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_lock
* Purpose: lock mailbox
* Input  : Mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_lock(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_unlock
* Purpose: unlock mailbox
* Input  : Mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_unlock(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_wait
* Purpose: wait until mailbox modified
* Input  : Mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_wait(Mailbox *mailbox);

/***********************************************************************\
* Name   : Mailbox_setEndOfMail
* Purpose: set end-of-mail flag for mailbox
* Input  : mailbox - mailbox
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Mailbox_setEndOfMail(Mailbox *mailbox);

#ifdef __cplusplus
  }
#endif

#endif /* __MAILBOX__ */

/* end of file */
