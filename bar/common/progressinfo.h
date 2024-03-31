/***********************************************************************\
*
* Contents: Progress info functions
* Systems: all
*
\***********************************************************************/

#ifndef __PROGRESS_INFO__
#define __PROGRESS_INFO__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/misc.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : ProgressInitFunction
* Purpose: progress init function call back
* Input  : text      - progress text
*          stepCount - step count
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ProgressInitFunction)(const char *text,
                                    uint64     stepCount,
                                    void       *userData
                                   );

/***********************************************************************\
* Name   : ProgressDoneFunction
* Purpose: progress done function call back
* Input  : totalTime - total time [s]
*          userData  - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ProgressDoneFunction)(ulong totalTime,
                                    void  *userData
                                   );

/***********************************************************************\
* Name   : ProgressInfoFunction
* Purpose: progress info function call back
* Input  : progress           - progress [%%]
*          estimatedTotalTime - estimated total time [s]
*          estimatedRestTime  - estimated rest time [s]
*          userData           - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ProgressInfoFunction)(uint  progress,
                                    ulong estimatedTotalTime,
                                    ulong estimatedRestTime,
                                    void  *userData
                                   );

// progress info data
typedef struct ProgressInfo
{
  struct ProgressInfo  *parent;
  uint64               *filterTimes;
  uint                 filterWindowSize;
  ProgressInitFunction initFunction;
  void                 *initUserData;
  ProgressDoneFunction doneFunction;
  void                 *doneUserData;
  ProgressInfoFunction infoFunction;
  void                 *infoUserData;
  uint                 reportTime;
  uint64               stepCount;
  String               text;

  uint64               startTimestamp;
  uint64               step;

  uint                 filterTimeIndex;
  uint                 filterTimeCount;
  uint64               filterTimeSum;

  uint64               lastTimestamp;

  ulong                lastProgressSum;  // last progress sum [1/1000]
  uint                 lastProgressCount;
  uint64               lastProgressTimestamp;
} ProgressInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

/***********************************************************************\
* Name   : ProgressInfo_init
* Purpose: init progress processing
* Input  : progressInfo         - progress info variable
*          parentProgressInfo   - parent progress info (can be NULL)
*          filterWindowSize     - filter window size or 0
*          reportTime           - report time [ms] or 0
*          stepCount            - step count
*          progressInitFunction - init function
*          progressInitUserData - init function user data
*          progressDoneFunction - done function
*          progressDoneUserData - done function user data
*          progressInfoFunction - info function
*          progressInfoUserData - info function user data
*          format               - text format string
*          ...                  - optional arguments for text
* Output : progressInfo - progress info
* Return : -
* Notes  : -
\***********************************************************************/

void ProgressInfo_init(ProgressInfo         *progressInfo,
                       ProgressInfo         *parentProgressInfo,
                       uint                 filterWindowSize,
                       uint                 reportTime,
                       uint64               stepCount,
                       ProgressInitFunction progressInitFunction,
                       void                 *progressInitUserData,
                       ProgressDoneFunction progressDoneFunction,
                       void                 *progressDoneUserData,
                       ProgressInfoFunction progressInfoFunction,
                       void                 *progressInfoUserData,
                       const char           *format,
                       ...
                      );

/***********************************************************************\
* Name   : ProgressInfo_done
* Purpose: done progress processing
* Input  : progressInfo - progress info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ProgressInfo_done(ProgressInfo *progressInfo);

/***********************************************************************\
* Name   : ProgressInfo_reset
* Purpose: reset progress processing
* Input  : progressInfo - progress info variable
*          stepCount    - step count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ProgressInfo_reset(ProgressInfo *progressInfo, uint64 stepCount);

/***********************************************************************\
* Name   : ProgressInfo_step
* Purpose: do progress step call back
* Input  : userData - progress info
* Output : -
* Return : -
* Notes  : parent progress info step functions are executed, too
\***********************************************************************/

void ProgressInfo_step(void *userData);

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __PROGRESS_INFO__ */

/* end of file */
