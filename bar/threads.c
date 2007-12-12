/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/threads.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "global.h"

#include "threads.h"

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

bool Thread_init(Thread     *thread,
                 int        priority,
                 const void *entryFunction,
                 void       *userData
                )
{
  struct sched_param param;

  assert(thread != NULL);

  /* start thread */
  if (pthread_create(&thread->handle,NULL,(void*(*)(void*))entryFunction,userData) != 0)
  {
    return FALSE;
  }

  /* set priority */
  if (priority != 0)
  {
    param.sched_priority = priority;
    if (pthread_setschedparam(thread->handle,
                              SCHED_OTHER,
                              &param
                             ) != 0)
    {
      return FALSE;
    }
  }

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

#ifdef __cplusplus
  }
#endif

/* end of file */
