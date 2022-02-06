/***********************************************************************\
*
* $Revision: 3438 $
* $Date: 2015-01-02 10:45:04 +0100 (Fri, 02 Jan 2015) $
* $Author: torsten $
* Contents: Progress info functions
* Systems: all
*
\***********************************************************************/

#define __PROGRESS_INFO_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "common/global.h"

#include "progressinfo.h"

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
                      )
{
  va_list arguments;
  String  text;

  assert(progressInfo != NULL);

  progressInfo->parent           = parentProgressInfo;
  if (filterWindowSize > 0)
  {
    progressInfo->filterTimes      = (uint64*)malloc(filterWindowSize*sizeof(uint64));;
    if (progressInfo->filterTimes == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    progressInfo->filterTimeCount = 0;
  }
  progressInfo->filterWindowSize = filterWindowSize;
  progressInfo->initFunction     = progressInitFunction;
  progressInfo->initUserData     = progressInitUserData;
  progressInfo->doneFunction     = progressDoneFunction;
  progressInfo->doneUserData     = progressDoneUserData;
  progressInfo->infoFunction     = progressInfoFunction;
  progressInfo->infoUserData     = progressInfoUserData;
  progressInfo->reportTime       = reportTime;
  progressInfo->text             = NULL;

  if (format != NULL)
  {
    va_start(arguments,format);
    progressInfo->text = String_vformat(String_new(),format,arguments);
    va_end(arguments);
  }

  ProgressInfo_reset(progressInfo,stepCount);
}

void ProgressInfo_done(ProgressInfo *progressInfo)
{
  assert(progressInfo != NULL);

  if (progressInfo->doneFunction != NULL)
  {
    progressInfo->doneFunction((ulong)((Misc_getTimestamp()-progressInfo->startTimestamp)/US_PER_SECOND),
                               progressInfo->doneUserData
                              );
  }

  String_delete(progressInfo->text);
}

void ProgressInfo_reset(ProgressInfo *progressInfo, uint64 stepCount)
{
  String text;

  assert(progressInfo != NULL);

  progressInfo->stepCount              = stepCount;

  progressInfo->startTimestamp         = Misc_getTimestamp();
  progressInfo->step                   = 0LL;

  progressInfo->filterTimeIndex        = 0;
  progressInfo->filterTimeCount        = 0;
  progressInfo->filterTimeSum          = 0;

  progressInfo->lastTimestamp          = Misc_getTimestamp();

  progressInfo->lastProgressSum        = 0L;
  progressInfo->lastProgressCount      = 0;
  progressInfo->lastProgressTimestamp  = 0LL;

  if (   (progressInfo->text != NULL)
      && (progressInfo->initFunction != NULL)
     )
  {
    progressInfo->initFunction(String_cString(progressInfo->text),
                               stepCount,
                               progressInfo->initUserData
                              );
  }
}

void ProgressInfo_step(void *userData)
{
  ProgressInfo *progressInfo = (ProgressInfo*)userData;
  uint64       now;
  uint64       stepTime;
  uint         progress;
  uint         lastProgress;
  ulong        stepTimeAverage;
  uint64       elapsedTime;  // [us]
  uint64       estimatedTotalTime;  // [us]
  uint64       estimatedRestTime;  // [us]

extern ProgressInfo *ppp;

  assert(progressInfo != NULL);

  progressInfo->step++;

  if (progressInfo->stepCount > 0)
  {
    now         = Misc_getTimestamp();
    elapsedTime = now-progressInfo->startTimestamp;

    stepTime = elapsedTime/progressInfo->step;
    progressInfo->lastTimestamp = now;
//fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__); asm("int3");

    estimatedTotalTime = stepTime*progressInfo->stepCount;
//if (progressInfo != ppp) fprintf(stderr,"%s:%d: %llu\n",__FILE__,__LINE__,estimatedTotalTime/1000000);

    if (progressInfo->filterWindowSize > 0)
    {
      // average filter of last N values
      progressInfo->filterTimeSum =  progressInfo->filterTimeSum
                                  +estimatedTotalTime
                                  -progressInfo->filterTimes[(progressInfo->filterTimeIndex+progressInfo->filterTimeCount-1)%progressInfo->filterTimeCount];

      progressInfo->filterTimes[progressInfo->filterTimeIndex] = estimatedTotalTime;
      progressInfo->filterTimeIndex = (progressInfo->filterTimeIndex+1) % progressInfo->filterWindowSize;
      if (progressInfo->filterTimeCount < progressInfo->filterWindowSize) progressInfo->filterTimeCount++;
    }
    else
    {
      // average filter all values
      progressInfo->filterTimeSum += estimatedTotalTime;
    }
//if (progressInfo != ppp)fprintf(stderr,"%s:%d: %p i0=%u i1=%u estimatedTotalTime=%llu estimatedTotalTimeSum=%llu average=%llu\n",__FILE__,__LINE__,progressInfo,i0,i1,stepTime,progressInfo->estimatedTotalTimeSum,progressInfo->estimatedTotalTimeSum/SIZE_OF_ARRAY(progressInfo->estimatedTotalTimes));

    progress     = (progressInfo->step*1000)/progressInfo->stepCount;
    lastProgress = (progressInfo->lastProgressCount > 0)
                     ? (uint)(progressInfo->lastProgressSum/(ulong)progressInfo->lastProgressCount)
                     : 0;
    if (progress >= (lastProgress+1))
    {
      progressInfo->lastProgressSum   += progress;
      progressInfo->lastProgressCount += 1;
    }

    if (   (progressInfo->infoFunction != NULL)
        && (progressInfo->step > 0LL)
        && (   ((progressInfo->reportTime == 0) && (progress >= (lastProgress+1)))
            || (now > (progressInfo->lastProgressTimestamp+progressInfo->reportTime*US_PER_MS))
           )
       )
    {
      if (progressInfo->filterWindowSize > 0)
      {
        estimatedTotalTime = progressInfo->filterTimeSum/progressInfo->filterWindowSize;
      }
      else
      {
        estimatedTotalTime = progressInfo->filterTimeSum/progressInfo->step;
      }
      estimatedRestTime  = (elapsedTime < estimatedTotalTime) ? (ulong)(estimatedTotalTime-elapsedTime) : 0LL;
#if 0
if (progressInfo != ppp) fprintf(stderr,"%s:%d: %p i=%u p=%u elapsedTime=%lf estimatedTotalTime=%lf estimatedRestTime=%lf s=%lf\n",__FILE__,__LINE__,progressInfo,
progressInfo->filterTimeIndex,
progress,
(double)elapsedTime/US_PER_SECOND,
(double)estimatedTotalTime/US_PER_SECOND,
(double)estimatedRestTime/US_PER_SECOND,
(double)((progress> 0)?(elapsedTime*1000)/progress:0)/US_PER_SECOND
);
#endif

      progressInfo->infoFunction(progress,
                                 (ulong)(estimatedTotalTime/US_PER_SECOND),
                                 (ulong)(estimatedRestTime/US_PER_SECOND),
                                 progressInfo->infoUserData
                                );

      progressInfo->lastProgressTimestamp = now;
    }
  }

  if (progressInfo->parent != NULL)
  {
    ProgressInfo_step(progressInfo->parent);
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
