/***********************************************************************\
*
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
#include "common/misc.h"

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
*            MSGQUEUE_LOCKED_DO(msgQueue)
*            {
*              ...
*            }
*
*          message queue must be unlocked manually if break is used!
\***********************************************************************/

#define MSGQUEUE_LOCKED_DO(msgQueue) \
  for (MsgQueueLock __lock ## __COUNTER__ = lock(msgQueue); \
       __lock ## __COUNTER__; \
       unlock(msgQueue), __lock ## __COUNTER__ = FALSE \
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
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

LOCAL bool lock(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  pthread_mutex_lock(&msgQueue->lock);
  msgQueue->lockCount++;

  return TRUE;
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

  if (msgQueue->modifiedFlag && (msgQueue->lockCount <= 1))
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

LOCAL void initTimespec(struct timespec *timespec, ulong timeout)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    __int64 windowsTime;
  #endif /* PLATFORM_... */

  assert(timespec != NULL);

  #if   defined(PLATFORM_LINUX)
    clock_gettime(CLOCK_REALTIME,timespec);
  #elif defined(PLATFORM_WINDOWS)
    GetSystemTimeAsFileTime((FILETIME*)&windowsTime);
    windowsTime -= 116444736000000000LL;  // Jan 1 1601 -> Jan 1 1970
    timespec->tv_sec  = (windowsTime/10000000LL);
    timespec->tv_nsec = (windowsTime%10000000LL)*100LL;
  #endif /* PLATFORM_... */
  timespec->tv_nsec = timespec->tv_nsec+((timeout)%1000L)*1000000L; \
  timespec->tv_sec  = timespec->tv_sec+((timespec->tv_nsec/1000000000L)+(timeout))/1000L; \
  timespec->tv_nsec %= 1000000000L; \
}

/***********************************************************************\
* Name   : waitModified
* Purpose: wait until message is modified
* Input  : msgQueue - message queue
*          timeout  - timeout [ms]
* Output : -
* Return : TRUE iff modified, timeout otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool waitModified(MsgQueue *msgQueue, ulong timeout)
{
  uint            lockCount;
  uint            i;
  struct timespec timespec;
  int             result;

  assert(msgQueue != NULL);
  assert(msgQueue->lockCount > 0);

  // temporary revert lock count > 1
  lockCount = msgQueue->lockCount;
  for (i = 1; i < lockCount; i++)
  {
    pthread_mutex_unlock(&msgQueue->lock);
  }

  // wait modified or timeout
  msgQueue->lockCount  = 0;
  initTimespec(&timespec,timeout);
  result = pthread_cond_timedwait(&msgQueue->modified,&msgQueue->lock,&timespec);

  // restore lock count > 1
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
  msgQueue->modifiedFlag   = FALSE;
  msgQueue->lockCount      = 0;
  msgQueue->endOfMsgFlag   = FALSE;
  msgQueue->terminatedFlag = FALSE;
  List_init(&msgQueue->list,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeMsgNode,NULL));

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

void MsgQueue_terminate(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueue)
  {
    msgQueue->terminatedFlag = TRUE;

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }
}

void MsgQueue_clear(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueue)
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
  TimeoutInfo timeoutInfo;
  MsgNode     *msgNode;
  ulong       n;

  assert(msgQueue != NULL);

  Misc_initTimeout(&timeoutInfo,timeout);
  MSGQUEUE_LOCKED_DO(msgQueue)
  {
    // wait for message
    while (   !msgQueue->endOfMsgFlag
           && !msgQueue->terminatedFlag
           && List_isEmpty(&msgQueue->list)
           && !Misc_isTimeout(&timeoutInfo)
          )
    {
      // work-around: wait with timeout to handle lost wake-ups
      (void)waitModified(msgQueue,(ulong)Misc_getRestTimeout(&timeoutInfo,5000));
    }
    if (   msgQueue->terminatedFlag
        || List_isEmpty(&msgQueue->list)
       )
    {
      unlock(msgQueue);
      Misc_doneTimeout(&timeoutInfo);
      return FALSE;
    }

    // get message
    msgNode = (MsgNode*)List_removeFirst(&msgQueue->list);

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }
  Misc_doneTimeout(&timeoutInfo);

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
  MsgNode *msgNode;

  assert(msgQueue != NULL);

  // allocate message
  msgNode = (MsgNode*)malloc(sizeof(MsgNode)+size);
  if (msgNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  msgNode->size = size;
  memcpy(msgNode->data,msg,size);

  MSGQUEUE_LOCKED_DO(msgQueue)
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
      while (   !msgQueue->endOfMsgFlag
             && (List_count(&msgQueue->list) >= msgQueue->maxMsgs)
            )
      {
        waitModified(msgQueue,WAIT_FOREVER);
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
  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueue)
  {
    if (!msgQueue->endOfMsgFlag)
    {
      waitModified(msgQueue,WAIT_FOREVER);
    }
  }
}

void MsgQueue_setEndOfMsg(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueue)
  {
    msgQueue->endOfMsgFlag = TRUE;

    // signal modify
    msgQueue->modifiedFlag = TRUE;
  }
}

void MsgQueue_reset(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  MSGQUEUE_LOCKED_DO(msgQueue)
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
