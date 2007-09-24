/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/msgqueues.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: functions for inter-process message queues
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

#include "msgqueues.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct MsgNode
{
  NODE_HEADER(struct MsgNode);

  ulong size;
  byte  data[0];
} MsgNode;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool MsgQueue_init(MsgQueue *msgQueue, ulong maxMsgs)
{
  assert(msgQueue != NULL);

  msgQueue->maxMsgs = maxMsgs;
  if (pthread_mutex_init(&msgQueue->lock,NULL) != 0)
  {
    return FALSE;
  }
  if (pthread_cond_init(&msgQueue->modified,NULL) != 0)
  {
    pthread_mutex_destroy(&msgQueue->lock);
    return FALSE;
  }
  msgQueue->endOfMsgFlag = FALSE;
  List_init(&msgQueue->list);

  return TRUE;
}

void MsgQueue_done(MsgQueue *msgQueue, MsgFreeFunction msgFreeFunction, void *userData)
{
  MsgNode *msgNode;

  assert(msgQueue != NULL);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  /* discard all remaining messages */
  while (!List_empty(&msgQueue->list))
  {
    msgNode = (MsgNode*)List_getFirst(&msgQueue->list);

    if (msgFreeFunction != NULL)
    {
      msgFreeFunction(msgNode->data,userData);
    }
    free(msgNode);
  }

  /* free resources */
  pthread_cond_destroy(&msgQueue->modified);
  pthread_mutex_destroy(&msgQueue->lock);
}

MsgQueue *MsgQueue_new(ulong maxMsgs)
{
  MsgQueue *msgQueue;

  msgQueue = (MsgQueue*)malloc(sizeof(MsgQueue));
  if (msgQueue != NULL)
  {
    if (!MsgQueue_init(msgQueue,maxMsgs))
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

void MsgQueue_delete(MsgQueue *msgQueue, MsgFreeFunction msgFreeFunction, void *userData)
{
  assert(msgQueue != NULL);

  MsgQueue_done(msgQueue,msgFreeFunction,userData);
  free(msgQueue);
}

void MsgQueue_clear(MsgQueue *msgQueue, MsgFreeFunction msgFreeFunction, void *userData)
{
  MsgNode *msgNode;

  assert(msgQueue != NULL);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  /* discard all remaining messages */
  while (!List_empty(&msgQueue->list))
  {
    msgNode = (MsgNode*)List_getFirst(&msgQueue->list);

    if (msgFreeFunction != NULL)
    {
      msgFreeFunction(msgNode->data,userData);
    }
    free(msgNode);
  }

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);
}

bool MsgQueue_get(MsgQueue *msgQueue, void *msg, ulong *size, ulong maxSize)
{
  MsgNode *msgNode;
  ulong   n;

  assert(msgQueue != NULL);

fprintf(stderr,"%s,%d: get \n",__FILE__,__LINE__);
  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  /* wait for message */
  while (!msgQueue->endOfMsgFlag && (List_count(&msgQueue->list) <= 0))
  {
    pthread_cond_wait(&msgQueue->modified,&msgQueue->lock); 
  }

  /* get message */
  msgNode = (MsgNode*)List_getFirst(&msgQueue->list);

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);

  /* copy data, free message */
  if (msgNode == NULL)
  {
    return FALSE;
  }
  n = MIN(msgNode->size,maxSize);
  memcpy(msg,msgNode->data,n);
  if (size != NULL) (*size) = n;
  free(msgNode);

  /* signal modify */
  pthread_cond_broadcast(&msgQueue->modified);

  return TRUE;
}

bool MsgQueue_put(MsgQueue *msgQueue, const void *msg, ulong size)
{
  MsgNode *msgNode;

  assert(msgQueue != NULL);

fprintf(stderr,"%s,%d: put \n",__FILE__,__LINE__);
  /* allocate message */
  msgNode = (MsgNode*)malloc(sizeof(MsgNode)+size);
  if (msgNode == NULL)
  {
    return FALSE;
  }
  msgNode->size = size;
  memcpy(msgNode->data,msg,size);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  /* check if end of message */
  if (msgQueue->endOfMsgFlag)
  {
    free(msgNode);
    pthread_mutex_unlock(&msgQueue->lock);
    return FALSE;
  }

  /* check number of messages */
  if (msgQueue->maxMsgs > 0)
  {
    while (!msgQueue->endOfMsgFlag && (List_count(&msgQueue->list) >= msgQueue->maxMsgs))
    {
      pthread_cond_wait(&msgQueue->modified,&msgQueue->lock); 
    }
    if (List_count(&msgQueue->list) >= msgQueue->maxMsgs)
    {
      free(msgNode);
      pthread_mutex_unlock(&msgQueue->lock);
      return FALSE;
    }
  }

  /* put message */
  List_append(&msgQueue->list,msgNode);

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);

  /* signal modify */
  pthread_cond_broadcast(&msgQueue->modified);

  return TRUE;
}

ulong MsgQueue_count(MsgQueue *msgQueue)
{
  ulong count;

  assert(msgQueue != NULL);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  /* get count */
  count = List_count(&msgQueue->list);

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);

  return count;
}

void MsgQueue_wait(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  if (!msgQueue->endOfMsgFlag)
  {
    pthread_cond_wait(&msgQueue->modified,&msgQueue->lock); 
  }

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);
}

void MsgQueue_setEndOfMsg(MsgQueue *msgQueue)
{
  assert(msgQueue != NULL);

  /* lock */
  pthread_mutex_lock(&msgQueue->lock);

  msgQueue->endOfMsgFlag = TRUE;

  /* unlock */
  pthread_mutex_unlock(&msgQueue->lock);
}


#ifdef __cplusplus
  }
#endif

/* end of file */
