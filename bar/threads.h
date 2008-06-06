/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/threads.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: thread functions
* Systems: all
*
\***********************************************************************/

#ifndef __THREADS__
#define __TRHEADS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  pthread_t handle;
} Thread;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Thread_init
* Purpose: init thread
* Input  : thread        - thread variable
*          niceLevel     - nice level or 0 for default level
*          entryFunction - thread entry function
*          userData      - thread user data
* Output : -
* Return : TRUE if thread started, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Thread_init(Thread     *thread,
                 int        niceLevel,
                 const void *entryFunction,
                 void       *userData
                );

/***********************************************************************\
* Name   : Thread_done
* Purpose: deinitialize thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_done(Thread *thread);

/***********************************************************************\
* Name   : Thread_join
* Purpose: wait for termination of thread
* Input  : thread - thread
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_join(Thread *thread);

/***********************************************************************\
* Name   : Thread_yield
* Purpose: reschedule thread execution
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Thread_yield(void);

#ifdef __cplusplus
  }
#endif

#endif /* __TRHEADS__ */

/* end of file */
