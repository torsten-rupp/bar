/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for inter-process message queues
* Systems: all POSIX
*
\***********************************************************************/

#define __MSGQUEUES_IMPLEMENATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <assert.h>

#include "common/global.h"
#include "common/lists.h"

#include "msgqueues.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct MsgNode
{
  LIST_NODE_HEADER(struct MsgNode);

  ulong size;
  byte  data[0];
} MsgNode;

typedef bool MsgQueueLock;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : MSGQUEUE_LOCKED_DO
* Purpose: execute block with message queue locked
* Input  : msgQueueLock - lock flag variable (MsgQueueLock)
*          msgQueue     - message queue
* Output : -
* Return : -
* Notes  : usage:
*            MsgQueueLock msgQueueLock;
*            MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
*            {
*              ...
*            }
*
*          message queue must be unlocked manually if break is used!
\***********************************************************************/

#define MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue) \
  for (lock(msgQueue), msgQueueLock = TRUE; \
       msgQueueLock; \
       unlock(msgQueue), msgQueueLock = FALSE \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeMsgNode
* Purpose: free message node
* Input  : msgNode  - message node
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeMsgNode(MsgNode *msgNode, void *userData)
{
  UNUSED_VARIABLE(msgNode);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : clear
* Purpose: discard all remaining messages in queue
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clear(MsgQueue *msgQueue)
{
  MsgNode *msgNode;

  assert(msgQueue != NULL);

  while (!List_isEmpty(&msgQueue->list))
  {
    msgNode = (MsgNode*)List_removeFirst(&msgQueue->list);

    if (msgQueue->msgQueueMsgFreeFunction != NULL)
    {
      msgQueue->msgQueueMsgFreeFunction(msgNode->data,msgQueue->msgQueueMsgFreeUserData);
    }
    freeMsgNode(msgNode,NULL);
    free(msgNode);
  }
}

/***********************************************************************\
* Name   : lock
* Purpose: lock message queue
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void lock(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  pthread_mutex_lock(&msgQueue->lock);
  msgQueue->lockCount++;
}

/***********************************************************************\
* Name   : unlock
* Purpose: message queue unlock
* Input  : msgQueue - message queue
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void unlock(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);
  assert(msgQueue->lockCount > 0);

  if (msgQueue->modifiedFlag && (msgQueue->lockCount == 1))
  {
    pthread_cond_broadcast(&msgQueue->modified);
  }

  msgQueue->lockCount--;
  pthread_mutex_unlock(&msgQueue->lock);
}

/***********************************************************************\
* Name   : initTimespec
* Purpose: initialize timespec structure
* Input  : timespec - timespec variable
*          timeout  - timeout [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initTimespec(struct timespec *timespec, long timeout)
{
  struct timeval timeval;
  long           s,ns;

  assert(timespec != NULL);

  gettimeofday(&timeval,NULL);
  s  = timeval.tv_sec       +timeout/1000L;
  ns = timeval.tv_usec*1000L+(timeout%1000L)*1000000L;
  if (ns > 1000000000L)
  {
    s  += ns/1000000000L;
    ns = ns%1000000000L;
  }
  timespec->tv_sec  = s;
  timespec->tv_nsec = ns;
}

/***********************************************************************\
* Name   : waitModified
* Purpose: wait until message is modified
* Input  : msgQueue - message queue
* Output : -
* Return : TRUE iff modified, timeout otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool waitModified(MsgQueue *msgQueue, const struct timespec *timeout)
{
  uint lockCount;
  uint i;
  int  result;

  assert(msgQueue != NULL);
  assert(msgQueue->lockCount > 0);

  lockCount = msgQueue->lockCount;

  for (i = 1; i < lockCount; i++)
  {
    pthread_mutex_unlock(&msgQueue->lock);
  }
  msgQueue->lockCount  = 0;
  if (timeout != NULL)
  {
    result = pthread_cond_timedwait(&msgQueue->modified,&msgQueue->lock,timeout);
  }
  else
  {
    result = pthread_cond_wait(&msgQueue->modified,&msgQueue->lock);
  }
  msgQueue->lockCount  = lockCount;
  for (i = 1; i < lockCount; i++)
  {
    pthread_mutex_lock(&msgQueue->lock);
  }

  return result == 0;
}

/*---------------------------------------------------------------------*/

bool MsgQueue_init(MsgQueue                *msgQueue,
                   ulong                   maxMsgs,
                   MsgQueueMsgFreeFunction msgQueueMsgFreeFunction,
                   void                    *msgQueueMsgFreeUserData
                  )
{
  assert(msgQueue != NULL);

  msgQueue->maxMsgs                 = maxMsgs;
  msgQueue->msgQueueMsgFreeFunction = msgQueueMsgFreeFunction;
  msgQueue->msgQueueMsgFreeUserData = msgQueueMsgFreeUserData;
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_mutexattr_init(&msgQueue->lockAttributes);
    pthread_mutexattr_settype(&msgQueue->lockAttributes,PTHREAD_MUTEX_RECURSIVE_NP);
    if (pthread_mutex_init(&msgQueue->lock,&msgQueue->lockAttributes) != 0)
    {
      return FALSE;
    }
  #elif defined(PLATFORM_WINDOWS)
xxx
    msgQueue->lock = CreateMutex(NULL,FALSE,NULL);
    if (msgQueue->lock == NULL)
    {
      return FALSE;
    }
  #endif /* PLATFORM_... */
  if (pthread_cond_init(&msgQueue->modified,NULL) != 0)
  {
    pthread_mutex_destroy(&msgQueue->lock);
    return FALSE;
  }
  msgQueue->modifiedFlag = FALSE;
  msgQueue->lockCount    = 0;
  msgQueue->endOfMsgFlag = FALSE;
  List_init(&msgQueue->list,CALLBACK_(NULL,NULL),CALLBACK_(freeMsgNode,NULL));

  return TRUE;
}

void MsgQueue_done(MsgQueue *msgQueue)
{
  MsgNode *msgNode;

  assert(msgQueue != NULL);

  // lock
  lock(msgQueue);

  // discard all remaining messages
  while (!List_isEmpty(&msgQueue->list))
  {
    msgNode = (MsgNode*)List_removeFirst(&msgQueue->list);

    if (msgQueue->msgQueueMsgFreeFunction != NULL)
    {
      msgQueue->msgQueueMsgFreeFunction(msgNode->data,msgQueue->msgQueueMsgFreeUserData);
    }
    free(msgNode);
  }

  // free resources
  pthread_cond_destroy(&msgQueue->modified);
//  #if   defined(PLATFORM_LINUX)
#if 1
    pthread_mutex_destroy(&msgQueue->lock);
    pthread_mutexattr_destroy(&msgQueue->lockAttributes);
  #elif defined(PLATFORM_WINDOWS)
xxx
  #endif /* PLATFORM_... */
}

MsgQueue *MsgQueue_new(ulong                   maxMsgs,
                       MsgQueueMsgFreeFunction msgQueueMsgFreeFunction,
                       void                    *msgQueueMsgFreeUserData
                      )
{
  MsgQueue *msgQueue;

  msgQueue = (MsgQueue*)malloc(sizeof(MsgQueue));
  if (msgQueue != NULL)
  {
    if (!MsgQueue_init(msgQueue,maxMsgs,CALLBACK_(msgQueueMsgFreeFunction,msgQueueMsgFreeUserData)))
    {
      free(msgQueue);
      return NULL;
    }
  }
  else
  {
    return NULL;
  }

  return msgQueue;
}

void MsgQueue_delete(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  MsgQueue_done(msgQueue);
  free(msgQueue);
}

void MsgQueue_clear(MsgQueue *msgQueue)
{
  MsgQueueLock msgQueueLock;

  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    clear(msgQueue);
  }
}

void MsgQueue_lock(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  lock(msgQueue);
}

void MsgQueue_unlock(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  unlock(msgQueue);
}


bool MsgQueue_get(MsgQueue *msgQueue, void *msg, ulong *size, ulong maxSize, long timeout)
{
  MsgQueueLock    msgQueueLock;
  struct timespec timespec;
  MsgNode         *msgNode;
  ulong           n;

  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    // wait for message
    if (timeout != WAIT_FOREVER)
    {
      initTimespec(&timespec,timeout);

      while (!msgQueue->endOfMsgFlag && (List_count(&msgQueue->list) <= 0))
      {
        if (!waitModified(msgQueue,&timespec))
        {
          unlock(msgQueue);
          return FALSE;
        }
      }
    }
    else
    {
      while (!msgQueue->endOfMsgFlag && (List_count(&msgQueue->list) <= 0))
      {
        (void)waitModified(msgQueue,NULL);
      }
    }

    // get message
    msgNode = (MsgNode*)List_removeFirst(&msgQueue->list);

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }

  // copy data, free message
  if (msgNode == NULL)
  {
    return FALSE;
  }
  n = MIN(msgNode->size,maxSize);
  memcpy(msg,msgNode->data,n);
  if (size != NULL) (*size) = n;
  free(msgNode);

  return TRUE;
}

bool MsgQueue_put(MsgQueue *msgQueue, const void *msg, ulong size)
{
  MsgQueueLock msgQueueLock;
  MsgNode      *msgNode;

  assert(msgQueue != NULL);

  // allocate message
  msgNode = (MsgNode*)malloc(sizeof(MsgNode)+size);
  if (msgNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  msgNode->size = size;
  memcpy(msgNode->data,msg,size);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    // check if end of message
    if (msgQueue->endOfMsgFlag)
    {
      free(msgNode);
      unlock(msgQueue);
      return FALSE;
    }

    if (msgQueue->maxMsgs > 0)
    {
      // wait for space in queue
      while (!msgQueue->endOfMsgFlag && (List_count(&msgQueue->list) >= msgQueue->maxMsgs))
      {
        waitModified(msgQueue,NULL);
      }
      assert(msgQueue->endOfMsgFlag || List_count(&msgQueue->list) < msgQueue->maxMsgs);
    }

    // put message
    List_append(&msgQueue->list,msgNode);

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }

  return TRUE;
}

void MsgQueue_wait(MsgQueue *msgQueue)
{
  MsgQueueLock msgQueueLock;

  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    if (!msgQueue->endOfMsgFlag)
    {
      waitModified(msgQueue,NULL);
    }
  }
}

void MsgQueue_setEndOfMsg(MsgQueue *msgQueue)
{
  MsgQueueLock msgQueueLock;

  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    msgQueue->endOfMsgFlag = TRUE;

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }
}

void MsgQueue_reset(MsgQueue *msgQueue)
{
  MsgQueueLock msgQueueLock;

  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueueLock,msgQueue)
  {
    clear(msgQueue);
    msgQueue->endOfMsgFlag = FALSE;

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }
}
#ifdef __cplusplus
  }
#endif

/* end of file */
