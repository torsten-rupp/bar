/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/threads.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <assert.h>

#include "global.h"

#include "threads.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  sem_t lock;
  const char *name;
  int   niceLevel;
  void  (*entryFunction)(void*);
  void  *userData;
} ThreadStartInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : threadStart
* Purpose: thread start function
* Input  : startInfo - start info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void threadStart(ThreadStartInfo *startInfo)
{
  int  niceLevel;
  void (*entryFunction)(void*);
  void *userData;

  assert(startInfo != NULL);

  niceLevel     = startInfo->niceLevel;
  entryFunction = startInfo->entryFunction;
  userData      = startInfo->userData;
  sem_post(&startInfo->lock);

  nice(niceLevel);
//fprintf(stderr,"%s,%d: %s: %d %d\n",__FILE__,__LINE__,startInfo->name,pthread_self(),getpid());

  assert(entryFunction != NULL);
  entryFunction(userData);
}

/*---------------------------------------------------------------------*/

bool Thread_init(Thread     *thread,
                 const char *name,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *userData
                )
{
  ThreadStartInfo startInfo;
  pthread_attr_t  threadAttributes;

  assert(thread != NULL);
  assert(name != NULL);
  assert(entryFunction != NULL);

  /* init thread info */
  sem_init(&startInfo.lock,0,0);
startInfo.name = name;
  startInfo.niceLevel     = niceLevel;
  startInfo.entryFunction = entryFunction;
  startInfo.userData      = userData;

  /* start thread */
  pthread_attr_init(&threadAttributes);
  #ifdef HAVE_PTHREAD_ATTR_SETNAME
    if (name != NULL)
    {
      pthread_attr_setname(&threadAttributes,(char*)name);
    }
  #else /* HAVE_PTHREAD_ATTR_SETNAME */
    UNUSED_VARIABLE(name);
  #endif /* HAVE_PTHREAD_ATTR_SETNAME */

  /* start thread */
  if (pthread_create(&thread->handle,
                     &threadAttributes,
                     (void*(*)(void*))threadStart,
                     &startInfo                   ) != 0)
  {
    sem_destroy(&startInfo.lock);
    return FALSE;
  }
  pthread_attr_destroy(&threadAttributes);

  /* wait until thread started */
  sem_wait(&startInfo.lock);

  /* free resources */
  sem_destroy(&startInfo.lock);

  return TRUE;
}

void Thread_done(Thread *thread)
{
  assert(thread != NULL);

  UNUSED_VARIABLE(thread);
}

void Thread_join(Thread *thread)
{
  assert(thread != NULL);

  pthread_join(thread->handle,NULL);
}

void Thread_yield(void)
{
  sched_yield();
}

#ifdef __cplusplus
  }
#endif

/* end of file */
