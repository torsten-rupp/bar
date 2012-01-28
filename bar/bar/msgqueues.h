/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for inter-process message queues
* Systems: all POSIX
*
\***********************************************************************/

#ifndef __MSG_QUEUE__
#define __MSG_QUEUE__

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
  ulong               maxMsgs;
  pthread_mutex_t     lock;
  pthread_mutexattr_t lockAttributes;
  uint                lockCount;
  pthread_cond_t      modified;
  bool                modifiedFlag;
  bool                endOfMsgFlag;
  List                list;
} MsgQueue;

typedef void(*MsgQueueMsgFreeFunction)(void *msg, void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : MsgQueue_init
* Purpose: initialize message queue
* Input  : maxMsg - max. number of message in queue (0 for unlimited)
* Output : msgQueue - initialized message queue
* Return : -
* Notes  : -
\***********************************************************************/

bool MsgQueue_init(MsgQueue *msgQueue, ulong maxMsgs);

/***********************************************************************\
* Name   : MsgQueue_done
* Purpose: free all remaining message in queue and deinitialize
* Input  : msgQueue                - message queue
*          msgQueueMsgFreeFunction - message free function or NULL
*          msgQueueMsgFreeUserData - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_done(MsgQueue *msgQueue, MsgQueueMsgFreeFunction msgQueueMsgFreeFunction, void *msgQueueMsgFreeUserData);

/***********************************************************************\
* Name   : MsgQueue_new
* Purpose: create new message queue
* Input  : maxMsg - max. number of message in queue (0 for unlimited)
* Output : -
* Return : messages queue or NULL if insufficient memory
* Notes  : -
\***********************************************************************/

MsgQueue *MsgQueue_new(ulong maxMsgs);

/***********************************************************************\
* Name   : MsgQueue_delete
* Purpose: delete message queue
* Input  : msgQueue                - message queue
*          msgQueueMsgFreeFunction - message free function or NULL
*          msgQueueMsgFreeUserData - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_delete(MsgQueue *msgQueue, MsgQueueMsgFreeFunction msgQueueMsgFreeFunction, void *msgQueueMsgFreeUserData);

/***********************************************************************\
* Name   : MsgQueue_clear
* Purpose: discard all message in queue
* Input  : msgQueue                - message queue
*          msgQueueMsgFreeFunction - message free function or NULL
*          msgQueueMsgFreeUserData - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_clear(MsgQueue *msgQueue, MsgQueueMsgFreeFunction msgQueueMsgFreeFunction, void *msgQueueMsgFreeUserData);

/***********************************************************************\
* Name   : MsgQueue_lock
* Purpose: lock message queue
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_lock(MsgQueue *msgQueue);

/***********************************************************************\
* Name   : MsgQueue_unlock
* Purpose: unlock message queue
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_unlock(MsgQueue *msgQueue);

/***********************************************************************\
* Name   : MsgQueue_get
* Purpose: get next message in queue
* Input  : msgQueue - message queue
*          maxSize  - max. size of message
* Output : msg  - message
*          size - size of message (can be NULL)
* Return : TRUE if message received, FALSE if end-of-messages
* Notes  : -
\***********************************************************************/

bool MsgQueue_get(MsgQueue *msgQueue, void *msg, ulong *size, ulong maxSize);

/***********************************************************************\
* Name   : MsgQueue_put
* Purpose: put message into queue
* Input  : msgQueue - message queue
*          msg      - message data
*          size     - size of message
* Output : -
* Return : TRUE if message stored in queue, FALSE if end-of-messages
* Notes  : -
\***********************************************************************/

bool MsgQueue_put(MsgQueue *msgQueue, const void *msg, ulong size);

/***********************************************************************\
* Name   : MsgQueue_count
* Purpose: get number of messages in queue
* Input  : msgQueue - message queue
* Output : -
* Return : number of messsages in queue
* Notes  : -
\***********************************************************************/

ulong MsgQueue_count(MsgQueue *msgQueue);

/***********************************************************************\
* Name   : MsgQueue_wait
* Purpose: wait until message queue modified (stored new message,
*          removed message)
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_wait(MsgQueue *msgQueue);

/***********************************************************************\
* Name   : MsgQueue_setEndOfMsg
* Purpose: set end-of-message flag for queue
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void MsgQueue_setEndOfMsg(MsgQueue *msgQueue);

#ifdef __cplusplus
  }
#endif

#endif /* __MSG_QUEUE__ */

/* end of file */
