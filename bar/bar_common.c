/***********************************************************************\
*
* $Revision: 3438 $
* $Date: 2015-01-02 10:45:04 +0100 (Fri, 02 Jan 2015) $
* $Author: torsten $
* Contents: Backup ARchiver common functions
* Systems: all
*
\***********************************************************************/

#define __BAR_COMMON_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "forward.h"         // required for JobOptions

#include "common/global.h"
#include "common/cstrings.h"

#include "configuration.h"
#include "archive.h"

#include "bar_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
String tmpDirectory;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Common_initAll(void)
{
  tmpDirectory = String_new();

  return ERROR_NONE;
}

void Common_doneAll(void)
{
  String_delete(tmpDirectory);
}

void initRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  runningInfo->progress.done.count          = 0L;
  runningInfo->progress.done.size           = 0LL;
  runningInfo->progress.total.count         = 0L;
  runningInfo->progress.total.size          = 0LL;
  runningInfo->progress.collectTotalSumDone = FALSE;
  runningInfo->progress.skipped.count       = 0L;
  runningInfo->progress.skipped.size        = 0LL;
  runningInfo->progress.error.count         = 0L;
  runningInfo->progress.error.size          = 0LL;
  runningInfo->progress.archiveSize         = 0LL;
  runningInfo->progress.compressionRatio    = 0.0;
  runningInfo->progress.entry.name          = String_new();
  runningInfo->progress.entry.doneSize      = 0LL;
  runningInfo->progress.entry.totalSize     = 0LL;
  runningInfo->progress.storage.name        = String_new();
  runningInfo->progress.storage.doneSize    = 0LL;
  runningInfo->progress.storage.totalSize   = 0LL;
  runningInfo->progress.volume.number       = 0;
  runningInfo->progress.volume.done         = 0.0;
  runningInfo->message.code                 = MESSAGE_CODE_NONE;
  runningInfo->message.data                 = String_new();
}

void doneRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  String_delete(runningInfo->message.data);
  String_delete(runningInfo->progress.storage.name);
  String_delete(runningInfo->progress.entry.name);
}

void setRunningInfo(RunningInfo *runningInfo, const RunningInfo *fromRunningInfo)
{
  double  entriesPerSecondAverage,bytesPerSecondAverage,storageBytesPerSecondAverage;
  ulong   restFiles;
  uint64  restBytes;
  uint64  restStorageBytes;
  ulong   estimatedRestTime;

  assert(runningInfo != NULL);
  assert(runningInfo->progress.entry.name != NULL);
  assert(runningInfo->progress.storage.name != NULL);
  assert(fromRunningInfo != NULL);
  assert(fromRunningInfo->progress.entry.name != NULL);
  assert(fromRunningInfo->progress.storage.name != NULL);

  runningInfo->progress.done.count          = fromRunningInfo->progress.done.count;
  runningInfo->progress.done.size           = fromRunningInfo->progress.done.size;
  runningInfo->progress.total.count         = fromRunningInfo->progress.total.count;
  runningInfo->progress.total.size          = fromRunningInfo->progress.total.size;
  runningInfo->progress.collectTotalSumDone = fromRunningInfo->progress.collectTotalSumDone;
  runningInfo->progress.skipped.count       = fromRunningInfo->progress.skipped.count;
  runningInfo->progress.skipped.size        = fromRunningInfo->progress.skipped.size;
  runningInfo->progress.error.count         = fromRunningInfo->progress.error.count;
  runningInfo->progress.error.size          = fromRunningInfo->progress.error.size;
  runningInfo->progress.archiveSize         = fromRunningInfo->progress.archiveSize;
  runningInfo->progress.compressionRatio    = fromRunningInfo->progress.compressionRatio;
  String_set(runningInfo->progress.entry.name,fromRunningInfo->progress.entry.name);
  runningInfo->progress.entry.doneSize      = fromRunningInfo->progress.entry.doneSize;
  runningInfo->progress.entry.totalSize     = fromRunningInfo->progress.entry.totalSize;
  String_set(runningInfo->progress.storage.name,fromRunningInfo->progress.storage.name);
  runningInfo->progress.storage.doneSize    = fromRunningInfo->progress.storage.doneSize;
  runningInfo->progress.storage.totalSize   = fromRunningInfo->progress.storage.totalSize;
  runningInfo->progress.volume.number       = fromRunningInfo->progress.volume.number;
  runningInfo->progress.volume.done         = fromRunningInfo->progress.volume.done;

  runningInfo->message.code        = fromRunningInfo->message.code;
  String_set(runningInfo->message.data,fromRunningInfo->message.data);

  // calculate statics values
  Misc_performanceFilterAdd(&runningInfo->entriesPerSecondFilter,     runningInfo->progress.done.count);
  Misc_performanceFilterAdd(&runningInfo->bytesPerSecondFilter,       runningInfo->progress.done.size);
  Misc_performanceFilterAdd(&runningInfo->storageBytesPerSecondFilter,runningInfo->progress.storage.doneSize);
  entriesPerSecondAverage      = Misc_performanceFilterGetAverageValue(&runningInfo->entriesPerSecondFilter     );
  bytesPerSecondAverage        = Misc_performanceFilterGetAverageValue(&runningInfo->bytesPerSecondFilter       );
  storageBytesPerSecondAverage = Misc_performanceFilterGetAverageValue(&runningInfo->storageBytesPerSecondFilter);

  // calculate rest values
  restFiles         = (runningInfo->progress.total.count       > runningInfo->progress.done.count      ) ? runningInfo->progress.total.count      -runningInfo->progress.done.count       : 0L;
  restBytes         = (runningInfo->progress.total.size        > runningInfo->progress.done.size       ) ? runningInfo->progress.total.size       -runningInfo->progress.done.size        : 0LL;
  restStorageBytes  = (runningInfo->progress.storage.totalSize > runningInfo->progress.storage.doneSize) ? runningInfo->progress.storage.totalSize-runningInfo->progress.storage.doneSize : 0LL;

  // calculate estimated rest time
  estimatedRestTime = 0L;
  if (entriesPerSecondAverage      > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restFiles       /entriesPerSecondAverage     )); }
  if (bytesPerSecondAverage        > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restBytes       /bytesPerSecondAverage       )); }
  if (storageBytesPerSecondAverage > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restStorageBytes/storageBytesPerSecondAverage)); }

  // calulcate performance values
  runningInfo->entriesPerSecond      = Misc_performanceFilterGetValue(&runningInfo->entriesPerSecondFilter     ,10);
  runningInfo->bytesPerSecond        = Misc_performanceFilterGetValue(&runningInfo->bytesPerSecondFilter       ,10);
  runningInfo->storageBytesPerSecond = Misc_performanceFilterGetValue(&runningInfo->storageBytesPerSecondFilter,10);
  runningInfo->estimatedRestTime     = estimatedRestTime;
}

void resetRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  runningInfo->error                        = ERROR_NONE;

  runningInfo->progress.done.count          = 0L;
  runningInfo->progress.done.size           = 0LL;
  runningInfo->progress.total.count         = 0L;
  runningInfo->progress.total.size          = 0LL;
  runningInfo->progress.collectTotalSumDone = FALSE;
  runningInfo->progress.skipped.count       = 0L;
  runningInfo->progress.skipped.size        = 0LL;
  runningInfo->progress.error.count         = 0L;
  runningInfo->progress.error.size          = 0LL;
  runningInfo->progress.archiveSize         = 0LL;
  runningInfo->progress.compressionRatio    = 0.0;
  String_clear(runningInfo->progress.entry.name);
  runningInfo->progress.entry.doneSize      = 0LL;
  runningInfo->progress.entry.totalSize     = 0LL;
  String_clear(runningInfo->progress.storage.name);
  runningInfo->progress.storage.doneSize    = 0LL;
  runningInfo->progress.storage.totalSize   = 0LL;
  runningInfo->progress.volume.number       = 0;
  runningInfo->progress.volume.done         = 0.0;
  runningInfo->message.code                 = MESSAGE_CODE_NONE;
  String_clear(runningInfo->message.data);

  runningInfo->lastErrorCode                = ERROR_CODE_NONE;
  runningInfo->lastErrorNumber              = 0;
  String_clear(runningInfo->lastErrorData);

  runningInfo->lastExecutedDateTime         = 0LL;

  Misc_performanceFilterClear(&runningInfo->entriesPerSecondFilter     );
  Misc_performanceFilterClear(&runningInfo->bytesPerSecondFilter       );
  Misc_performanceFilterClear(&runningInfo->storageBytesPerSecondFilter);

  runningInfo->entriesPerSecond             = 0.0;
  runningInfo->bytesPerSecond               = 0.0;
  runningInfo->storageBytesPerSecond        = 0.0;
  runningInfo->estimatedRestTime            = 0L;
}

const char *messageCodeToString(MessageCodes messageCode)
{
  const char *MESSAGE_CODE_TEXT[] =
  {
    "NONE",
    "WAIT_FOR_TEMPORARY_SPACE",
    "WAIT_FOR_VOLUME",
    "ADD_ERROR_CORRECTION_CODES",
    "BLANK_VOLUME",
    "WRITE_VOLUME"
  };

  assert(messageCode >= MESSAGE_CODE_MIN);
  assert(messageCode <= MESSAGE_CODE_MAX);

  return MESSAGE_CODE_TEXT[(uint)messageCode];
}

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  uint64           dateTime
                 )
{
  assert(templateHandle != NULL);

  // init variables
  templateHandle->templateString  = templateString;
  templateHandle->expandMacroMode = expandMacroMode;
  templateHandle->dateTime        = dateTime;
  templateHandle->textMacros      = NULL;
  templateHandle->textMacroCount  = 0;
}

void templateMacros(TemplateHandle   *templateHandle,
                    const TextMacro  textMacros[],
                    uint             textMacroCount
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;

  assert(templateHandle != NULL);

  // add macros
  newTextMacroCount = templateHandle->textMacroCount+textMacroCount;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  memcpy(&newTextMacros[templateHandle->textMacroCount],textMacros,textMacroCount*sizeof(TextMacro));
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;
}

String templateDone(TemplateHandle *templateHandle,
                    String         string
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;
  char      buffer[256];
  uint      weekNumberU,weekNumberW;
  ulong     i;
  char      format[4];
  size_t    length;
  uint      z;

  assert(templateHandle != NULL);

  // init variables
  if (string == NULL) string = String_new();

  // get local time
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r((const time_t*)&templateHandle->dateTime,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime((const time_t*)&templateHandle->dateTime);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  // get week numbers
  strftime(buffer,sizeof(buffer)-1,"%U",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberU = (uint)atoi(buffer);
  strftime(buffer,sizeof(buffer)-1,"%W",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberW = (uint)atoi(buffer);

  // add week macros
  newTextMacroCount = templateHandle->textMacroCount+4;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+0],"%U2",(weekNumberU%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+1],"%U4",(weekNumberU%4)+1,"[1234]");
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+2],"%W2",(weekNumberW%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+3],"%W4",(weekNumberW%4)+1,"[1234]");
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;

  // expand macros
  Misc_expandMacros(string,
                    templateHandle->templateString,
                    templateHandle->expandMacroMode,
                    templateHandle->textMacros,
                    templateHandle->textMacroCount,
                    FALSE
                   );

  // expand date/time macros, replace %% -> %
  i = 0L;
  while (i < String_length(string))
  {
    switch (String_index(string,i))
    {
      case '%':
        if ((i+1) < String_length(string))
        {
          switch (String_index(string,i+1))
          {
            case '%':
              // %% -> %
              String_remove(string,i,1);
              i += 1L;
              break;
            case 'a':
            case 'A':
            case 'b':
            case 'B':
            case 'c':
            case 'C':
            case 'd':
            case 'D':
            case 'e':
            case 'E':
            case 'F':
            case 'g':
            case 'G':
            case 'h':
            case 'H':
            case 'I':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'M':
            case 'n':
            case 'O':
            case 'p':
            case 'P':
            case 'r':
            case 'R':
            case 's':
            case 'S':
            case 't':
            case 'T':
            case 'u':
            case 'U':
            case 'V':
            case 'w':
            case 'W':
            case 'x':
            case 'X':
            case 'y':
            case 'Y':
            case 'z':
            case 'Z':
            case '+':
              // format date/time part
              switch (String_index(string,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox: extended date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = String_index(string,i+2);
                  format[3] = '\0';

                  String_remove(string,i,3);
                  break;
                default:
                  // %x: date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = '\0';

                  String_remove(string,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,tm); buffer[sizeof(buffer)-1] = '\0';

              // insert into string
              switch (templateHandle->expandMacroMode)
              {
                case EXPAND_MACRO_MODE_STRING:
                  String_insertBuffer(string,i,buffer,length);
                  i += length;
                  break;
                case EXPAND_MACRO_MODE_PATTERN:
                  for (z = 0 ; z < length; z++)
                  {
                    if (strchr("*+?{}():[].^$|",buffer[z]) != NULL)
                    {
                      String_insertChar(string,i,'\\');
                      i += 1L;
                    }
                    String_insertChar(string,i,buffer[z]);
                    i += 1L;
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                  #endif /* NDEBUG */
              }
              break;
            default:
              // keep %x
              i += 2L;
              break;
          }
        }
        else
        {
          // keep % at end of string
          i += 1L;
        }
        break;
      default:
        i += 1L;
        break;
    }
  }

  // free resources
  free((void*)templateHandle->textMacros);

  return string;
}

String expandTemplate(const char       *templateString,
                      ExpandMacroModes expandMacroMode,
                      time_t           timestamp,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     )
{
  TemplateHandle templateHandle;

  templateInit(&templateHandle,
               templateString,
               expandMacroMode,
               timestamp
              );
  templateMacros(&templateHandle,
                 textMacros,
                 textMacroCount
                );

  return templateDone(&templateHandle,
                      NULL  // string
                     );
}

Errors executeTemplate(const char        *templateString,
                       time_t            timestamp,
                       const TextMacro   textMacros[],
                       uint              textMacroCount,
                       ExecuteIOFunction executeIOFunction,
                       void              *executeIOUserData
                      )
{
  String script;
  Errors error;

  if (!stringIsEmpty(templateString))
  {
    script = expandTemplate(templateString,
                            EXPAND_MACRO_MODE_STRING,
                            timestamp,
                            textMacros,
                            textMacroCount
                           );
    if (!String_isEmpty(script))
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK_(executeIOFunction,executeIOUserData),
                                 CALLBACK_(executeIOFunction,executeIOUserData)
                                );
      String_delete(script);
    }
    else
    {
      error = ERROR_EXPAND_TEMPLATE;
    }
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

// ----------------------------------------------------------------------

bool parseBandWidthNumber(ConstString s, ulong *n)
{
  const StringUnit UNITS[] =
  {
    {"T",1024LL*1024LL*1024LL*1024LL},
    {"G",1024LL*1024LL*1024LL},
    {"M",1024LL*1024LL},
    {"K",1024LL},
    {NULL,0LL}
  };

  assert(s != NULL);
  assert(n != NULL);

  (*n) = (ulong)String_toInteger64(s,STRING_BEGIN,NULL,UNITS,SIZE_OF_ARRAY(UNITS));

  return TRUE;
}

ulong getBandWidth(BandWidthList *bandWidthList)
{
  uint64        currentDateTime;
  uint          currentYear,currentMonth,currentDay;
  WeekDays      currentWeekDay;
  uint          currentHour,currentMinute;
  uint          matchingDateTime;
  BandWidthNode *matchingBandWidthNode;
  BandWidthNode *bandWidthNode;
  int           year,month,day;
  uint          hour,minute;
  uint64        dateTime;
  ulong         n;
  uint64        timestamp;
  FileHandle    fileHandle;
  String        line;

  assert(bandWidthList != NULL);

  n = 0L;

  // get current date/time values
  currentDateTime = Misc_getCurrentDateTime();
  Misc_splitDateTime(currentDateTime,
                     TIME_TYPE_LOCAL,
                     &currentYear,
                     &currentMonth,
                     &currentDay,
                     &currentHour,
                     &currentMinute,
                     NULL,  // second
                     &currentWeekDay,
                     NULL  // currentIsDayLightSaving
                    );

  // find best matching band width node
  matchingDateTime      = 0LL;
  matchingBandWidthNode = NULL;
  LIST_ITERATE(bandWidthList,bandWidthNode)
  {
    year   = (bandWidthNode->year   != DATE_ANY) ? (uint)bandWidthNode->year   : currentYear;
    month  = (bandWidthNode->month  != DATE_ANY) ? (uint)bandWidthNode->month  : currentMonth;
    day    = (bandWidthNode->day    != DATE_ANY) ? (uint)bandWidthNode->day    : currentDay;
    hour   = (bandWidthNode->hour   != TIME_ANY) ? (uint)bandWidthNode->hour   : currentHour;
    minute = (bandWidthNode->minute != TIME_ANY) ? (uint)bandWidthNode->minute : currentMinute;

    dateTime = Misc_makeDateTime(TIME_TYPE_LOCAL,year,month,day,hour,minute,0,DAY_LIGHT_SAVING_MODE_AUTO);

    if (   (currentDateTime >= dateTime)
        && (   (matchingBandWidthNode == NULL)
            || (dateTime > matchingDateTime)
           )
       )
    {
      matchingDateTime      = dateTime;
      matchingBandWidthNode = bandWidthNode;
    }
  }

  if (matchingBandWidthNode != NULL)
  {
    // read band width
    if (matchingBandWidthNode->fileName != NULL)
    {
      // read from external file
      timestamp = Misc_getTimestamp();
      if (timestamp > (bandWidthList->lastReadTimestamp+5*US_PER_SECOND))
      {
        bandWidthList->n = 0LL;

        // open file
        if (File_open(&fileHandle,matchingBandWidthNode->fileName,FILE_OPEN_READ) == ERROR_NONE)
        {
          line = String_new();
          while (File_getLine(&fileHandle,line,NULL,"#;"))
          {
            // parse band width
            if (!parseBandWidthNumber(line,&bandWidthList->n))
            {
              continue;
            }
          }
          String_delete(line);

          // close file
          (void)File_close(&fileHandle);

          // store timestamp of last read
          bandWidthList->lastReadTimestamp = timestamp;
        }
      }

      n = bandWidthList->n;
    }
    else
    {
      // use value
      n = matchingBandWidthNode->n;
    }
  }

  return n;
}

bool isInTimeRange(uint hour, uint minute, int beginHour, int beginMinute, int endHour, int endMinute)
{
  if (TIME_BEGIN(beginHour,beginMinute) <= TIME_END(endHour,endMinute))
  {
    return    (TIME(hour,minute) >= TIME_BEGIN(beginHour,beginMinute))
           && (TIME(hour,minute) <= TIME_END  (endHour,  endMinute  ));
  }
  else
  {
    return    (TIME(hour,minute) >= TIME_BEGIN(beginHour,beginMinute))
           || (TIME(hour,minute) <= TIME_END  (endHour,  endMinute  ));
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
