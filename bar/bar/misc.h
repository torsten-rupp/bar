/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: miscellaneous functions
* Systems: all
*
\***********************************************************************/

#ifndef __MISC__
#define __MISC__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DATE_TIME_FORMAT_DEFAULT "%Y-%m-%d %H:%M:%S %Z"
#define DATE_TIME_FORMAT_ISO     "%F %T %Z"
#define DATE_TIME_FORMAT_LOCALE  "%c"

// month, day names
typedef enum
{
  MONTH_JAN =  1,
  MONTH_FEB =  2,
  MONTH_MAR =  3,
  MONTH_APR =  4,
  MONTH_MAY =  5,
  MONTH_JUN =  6,
  MONTH_JUL =  7,
  MONTH_AUG =  8,
  MONTH_SEP =  9,
  MONTH_OCT = 10,
  MONTH_NOV = 11,
  MONTH_DEC = 12,
} Months;

typedef enum
{
  WEEKDAY_MON = 0,
  WEEKDAY_TUE = 1,
  WEEKDAY_WED = 2,
  WEEKDAY_THU = 3,
  WEEKDAY_FRI = 4,
  WEEKDAY_SAT = 5,
  WEEKDAY_SUN = 6,
} WeekDays;

#define MISC_US_PER_SECOND (1000LL*1000LL)

/***************************** Datatypes *******************************/
// text macro definitions
typedef enum
{
  TEXT_MACRO_TYPE_INTEGER,
  TEXT_MACRO_TYPE_INTEGER64,
  TEXT_MACRO_TYPE_CSTRING,
  TEXT_MACRO_TYPE_STRING,
} TextMacroTypes;

typedef struct
{
  TextMacroTypes type;
  const char     *name;
  struct
  {
    int            i;
    int64          l;
    const char     *s;
    String         string;
  } value;
} TextMacro;

typedef void(*ExecuteIOFunction)(void         *userData,
                                 const String line
                                );

typedef struct
{
  uint64 timeStamp;
  double value;
} PerformanceValue;

typedef struct
{
  uint             maxSeconds;
  uint             seconds;
  uint             index;
  PerformanceValue *performanceValues;
  double           average;
  ulong            n;
} PerformanceFilter;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define TEXT_MACRO_INTEGER(name,value) \
  { \
    TEXT_MACRO_TYPE_INTEGER, \
    name, \
    {value,0LL,NULL,NULL} \
  }
#define TEXT_MACRO_INTEGER64(name,value) \
  { \
    TEXT_MACRO_TYPE_INTEGER64, \
    name, \
    {0,value,NULL,NULL} \
  }
#define TEXT_MACRO_CSTRING(name,value) \
  { \
    TEXT_MACRO_TYPE_CSTRING, \
    name, \
    {0,0LL,value,NULL} \
  }
#define TEXT_MACRO_STRING(name,value) \
  { \
    TEXT_MACRO_TYPE_STRING, \
    name, \
    {0,0LL,NULL,value} \
  }

#define TEXT_MACRO_N_INTEGER(macro,_name,_value) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_INTEGER; \
    macro.name    = _name; \
    macro.value.i = _value; \
  } while (0)
#define TEXT_MACRO_N_INTEGER64(macro,_name,_value) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_INTEGER64; \
    macro.name    = _name; \
    macro.value.l = _value; \
  } while (0)
#define TEXT_MACRO_N_CSTRING(macro,_name,_value) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_CSTRING; \
    macro.name    = _name; \
    macro.value.s = _value; \
  } while (0)
#define TEXT_MACRO_N_STRING(macro,_name,_value) \
  do { \
    macro.type         = TEXT_MACRO_TYPE_STRING; \
    macro.name         = _name; \
    macro.value.string = _value; \
  } while (0)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getTimestamp
* Purpose: get timestamp
* Input  : -
* Output : -
* Return : timestamp [us]
* Notes  : -
\***********************************************************************/

uint64 Misc_getTimestamp(void);

/***********************************************************************\
* Name   : Misc_getCurrentDateTime
* Purpose: get current date/time
* Input  : -
* Output : -
* Return : date/time (seconds since 1970-1-1 00:00:00)
* Notes  : -
\***********************************************************************/

uint64 Misc_getCurrentDateTime(void);

/***********************************************************************\
* Name   : Misc_splitDateTime
* Purpose: split date/time into parts
* Input  : dateTime - date/time (seconds since 1970-1-1 00:00:00)
* Output : year    - year, YYYY (could be NULL)
*          month   - month, 1..12 (could be NULL)
*          day     - day, 1..31 (could be NULL)
*          hour    - hour, 0..23 (could be NULL)
*          minute  - minute, 0..59 (could be NULL)
*          second  - second, 0..59 (could be NULL)
*          weekDay - week day, DAY_* (could be NULL)
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_splitDateTime(uint64   dateTime,
                        uint     *year,
                        uint     *month,
                        uint     *day,
                        uint     *hour,
                        uint     *minute,
                        uint     *second,
                        WeekDays *weekDay
                       );

/***********************************************************************\
* Name   : Misc_makeDateTime
* Purpose: create date/time from parts
* Input  : year    - year, YYYY
*          month   - month, 1..12
*          day     - day, 1..31
*          hour    - hour, 0..23
*          minute  - minute, 0..59
*          second  - second, 0..59
* Return : date/time (seconds since 1970-1-1 00:00:00)
* Return : -
* Notes  : -
\***********************************************************************/

uint64 Misc_makeDateTime(uint year,
                         uint month,
                         uint day,
                         uint hour,
                         uint minute,
                         uint second
                        );

uint64 Misc_parseDateTime(const char *string);

/***********************************************************************\
* Name   : Misc_formatDateTime, Misc_formatDateTimeCString
* Purpose: format date/time
* Input  : string     - string variable
*          buffer     - buffer
*          bufferSize - buffer size
*          dateTime   - date/time (seconds since 1970-1-1 00:00:00)
*          format     - format string (see strftime) or NULL for default
* Output : -
* Return : date/time string
* Notes  : -
\***********************************************************************/

String Misc_formatDateTime(String string, uint64 dateTime, const char *format);
const char* Misc_formatDateTimeCString(char *buffer, uint bufferSize, uint64 dateTime, const char *format);

/***********************************************************************\
* Name   : Misc_udelay
* Purpose: delay program execution
* Input  : time - delay time [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_udelay(uint64 time);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_expandMacrosCString
* Purpose: expand macros %<name>:<format> in string
* Input  : string        - string variable
*          macroTemplate - string with macros
*          macros        - array with macro definitions
*          macroCount    - number of macro definitions
* Output : s - string with expanded macros
* Return : expanded string
* Notes  : -
\***********************************************************************/

String Misc_expandMacros(String          string,
                         const char      *macroTemplate,
                         const TextMacro macros[],
                         uint            macroCount
                        );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_executeCommand
* Purpose: execute external command
* Input  : commandTemplate         - command template string
*          macros                  - macros array
*          macroCount              - number of macros in array
*          stdoutExecuteIOFunction - stdout callback or NULL
*          stderrExecuteIOFunction - stderr callback or NULL
*          executeIOUserData       - user data for callbacks
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Misc_executeCommand(const char        *commandTemplate,
                           const TextMacro   macros[],
                           uint              macroCount,
                           ExecuteIOFunction stdoutExecuteIOFunction,
                           ExecuteIOFunction stderrExecuteIOFunction,
                           void              *executeIOUserData
                          );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_waitEnter
* Purpose: wait until user press ENTER
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_waitEnter(void);

/***********************************************************************\
* Name   : Misc_waitYesNo
* Purpose: wait for user yes/no input
* Input  : message - message to print
* Output : -
* Return : TRUE for yes, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Misc_getYesNo(const char *message);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_performanceFilterInit
* Purpose: initialise performance filter
* Input  : performanceFilter - performance filter variable
*          seconds           - filter time window size in seconds
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_performanceFilterInit(PerformanceFilter *performanceFilter,
                                uint              maxSeconds
                               );

/***********************************************************************\
* Name   : Misc_performanceFilterDone
* Purpose: deinitialise performance filter
* Input  : performanceFilter - performance filter variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_performanceFilterDone(PerformanceFilter *performanceFilter);

/***********************************************************************\
* Name   : Misc_performanceFilterClear
* Purpose: clear performance filter
* Input  : performanceFilter - performance filter variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_performanceFilterClear(PerformanceFilter *performanceFilter);

/***********************************************************************\
* Name   : Misc_performanceFilterAdd
* Purpose: add filter value
* Input  : performanceFilter - performance filter variable
*          value             - value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_performanceFilterAdd(PerformanceFilter *performanceFilter,
                               double            value
                              );

/***********************************************************************\
* Name   : Misc_performanceFilterGetValue
* Purpose: get performance value
* Input  : performanceFilter - performance filter variable
*          seconds           - history seconds
* Output : -
* Return : performance value in x/s or 0
* Notes  : -
\***********************************************************************/

double Misc_performanceFilterGetValue(const PerformanceFilter *performanceFilter,
                                      uint                    seconds
                                     );

/***********************************************************************\
* Name   : Misc_performanceFilterGetAverageValue
* Purpose: get average performance value
* Input  : performanceFilter - performance filter variable
* Output : -
* Return : average performance value in x/s or 0
* Notes  : -
\***********************************************************************/

double Misc_performanceFilterGetAverageValue(PerformanceFilter *performanceFilter);

#ifdef __cplusplus
  }
#endif

#endif /* __MISC__ */

/* end of file */
