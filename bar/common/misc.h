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
#include <unistd.h>
#ifdef HAVE_SYSTEMD_SD_ID128_H
  #include <systemd/sd-id128.h>
#endif
#include <assert.h>

// file/socket handle events
#if   defined(PLATFORM_LINUX)
  #include <poll.h>
#elif defined(PLATFORM_WINDOWS)
  #include <winsock2.h>
  #include <windows.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/strings.h"

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

// length of UUID string (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
#define MISC_UUID_STRING_LENGTH 36
#define MISC_UUID_NONE "00000000-0000-0000-0000-000000000000"

// length of machine id
#define MISC_MACHINE_ID_LENGTH (128/8)

// text macro patterns
#define TEXT_MACRO_PATTERN_INTEGER   "[+-]{0,1}\\d+"
#define TEXT_MACRO_PATTERN_INTEGER64 "[+-]{0,1}\\d+"
#define TEXT_MACRO_PATTERN_DOUBLE    "[+-]{0,1}(\\d+|\\d+\.\\d*|\\d*\.\\d+)"
#define TEXT_MACRO_PATTERN_CSTRING   "\\S+"
#define TEXT_MACRO_PATTERN_STRING    "\\S+"

// file/socket handle events
#if   defined(PLATFORM_LINUX)
  #define HANDLE_EVENT_INPUT   POLLIN
  #define HANDLE_EVENT_OUTPUT  POLLOUT
  #define HANDLE_EVENT_ERROR   POLLERR
  #define HANDLE_EVENT_HANGUP  POLLHUP
  #define HANDLE_EVENT_INVALID POLLNVAL
#elif defined(PLATFORM_WINDOWS)
  #ifdef HAVE_WSAPOLL
    #define HANDLE_EVENT_INPUT   POLLIN
    #define HANDLE_EVENT_OUTPUT  POLLOUT
    #define HANDLE_EVENT_ERROR   POLLHUP  // POLLERR: not supported
    #define HANDLE_EVENT_HANGUP  POLLHUP
    #define HANDLE_EVENT_INVALID 0  // POLLNVAL: not supported
  #else /* not HAVE_WSAPOLL */
    #define HANDLE_EVENT_INPUT   (1 << 0)
    #define HANDLE_EVENT_OUTPUT  (1 << 1)
    #define HANDLE_EVENT_ERROR   (1 << 2)
    #define HANDLE_EVENT_HANGUP  (1 << 3)
    #define HANDLE_EVENT_INVALID (1 << 4)
  #endif /* HAVE_WSAPOLL */
#endif /* PLATFORM_... */
#define HANDLE_EVENT_ANY (  HANDLE_EVENT_INPUT \
                          | HANDLE_EVENT_OUTPUT \
                          | HANDLE_EVENT_ERROR \
                          | HANDLE_EVENT_INPUT \
                          | HANDLE_EVENT_INVALID \
                         )

/***************************** Datatypes *******************************/

#if 0
//TODO: useful? remove? 64/32 bit?
typedef struct
{
  ulong value:61;
  enum unit
  {
    S,
    MS,
    US
  }:3;
} Time;
#define TIME(value,unit) { value, unit }
#endif

// timeout info
typedef struct
{
  long   timeout;
  uint64 endTimestamp;
} TimeoutInfo;

// UUID (Note; name clash with Windows)
typedef char UUID_[MISC_UUID_STRING_LENGTH+1];

// machine/application id
typedef const byte* MachineId;

// text macros
typedef enum
{
  TEXT_MACRO_TYPE_INTEGER,
  TEXT_MACRO_TYPE_INTEGER64,
  TEXT_MACRO_TYPE_DOUBLE,
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
    double         d;
    const char     *s;
    String         string;
  } value;
  const char *pattern;
} TextMacro;

// internal text macros type
typedef struct
{
  uint            count;
  const uint      maxCount;
  const TextMacro *data;
} __TextMacros;

// expand types
typedef enum
{
  EXPAND_MACRO_MODE_STRING,
  EXPAND_MACRO_MODE_PATTERN
} ExpandMacroModes;

// signal mask
#ifdef HAVE_SIGSET_T
  typedef sigset_t SignalMask;
#else /* not HAVE_SIGSET_T */
  typedef uint SignalMask;
#endif /* HAVE_SIGSET_T */

// file/socket wait handle
typedef struct
{
  #if   defined(PLATFORM_LINUX)
    struct pollfd *pollfds;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      WSAPOLLFD   *pollfds;
    #else /* not HAVE_WSAPOLL */
      int         handles[FD_SETSIZE];
      fd_set      readfds;
      fd_set      writefds;
      fd_set      exceptionfds;
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
  uint          handleCount;
  uint          maxHandleCount;
} WaitHandle;

/***********************************************************************\
* Name   : ExecuteIOFunction
* Purpose: call back for read line
* Input  : line     - line
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ExecuteIOFunction)(ConstString line,
                                 void        *userData
                                );

// performance values
typedef struct
{
  uint64 timeStamp;
  double value;
} PerformanceValue;

// performance filter (average of time range)
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

// define text macros variable
#define __TEXT_MACROS_IDENTIFIER1(name,suffix) __TEXT_MACROS_IDENTIFIER2(name,suffix)
#define __TEXT_MACROS_IDENTIFIER2(name,suffix) __##name##suffix
#ifndef NDEBUG
  #define TextMacros(name,count) \
    TextMacro __TEXT_MACROS_IDENTIFIER1(name,_data)[count]; \
    __TextMacros name = \
    { \
      0, \
      count, \
      __TEXT_MACROS_IDENTIFIER1(name,_data) \
    }
#else /* NDEBUG */
  #define TextMacros(name,count) \
    TextMacro __TEXT_MACROS_IDENTIFIER1(name,_data)[count]; \
    __TextMacros name = \
    { \
      0, \
      count, \
      __TEXT_MACROS_IDENTIFIER1(name,_data) \
    }
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : TEXT_MACROS_INIT
* Purpose: init text macros
* Input  : variable - variable
* Output : -
* Return : -
* Notes  : usage:
*          TextMacros (textMacros, 3);
*
*          TEXT_MACROS_INIT(textMacros)
*          {
*            TEXT_MACRO_INTEGER(name,value,pattern);
*            TEXT_MACRO_CSTRING(name,value,pattern);
*            TEXT_MACRO_STRING (name,value,pattern);
*          }
*          Misc_expandMacros(...,
*                            textMacros.data,
*                            textMacros.count,
*                            ...
*                           );
\***********************************************************************/

#ifndef NDEBUG
  #define TEXT_MACROS_INIT(variable) \
    for (TextMacro *__textMacro = (TextMacro*)&(variable).data[0], *__textMacroEnd = (TextMacro*)&(variable).data[variable.maxCount]; \
         __textMacro <= &(variable).data[0]; \
         (variable).count = __textMacro-&(variable).data[0] \
        )
#else /* NDEBUG */
  #define TEXT_MACROS_INIT(variable) \
    for (TextMacro *__textMacro = (TextMacro*)&(variable).data[0]; \
         __textMacro <= &(variable).data[0]; \
         (variable).count = __textMacro-&(variable).data[0] \
        )
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : TEXT_MACRO_*
* Purpose: init text macro
* Input  : name    - macro name (including %)
*          value   - value
*          pattern - regular expression pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define TEXT_MACRO_INTEGER(name,value,pattern) \
  { \
    TEXT_MACRO_TYPE_INTEGER, \
    name, \
    {value,0LL,0.0,NULL,NULL}, \
    pattern \
  }
#define TEXT_MACRO_INTEGER64(name,value,pattern) \
  { \
    TEXT_MACRO_TYPE_INTEGER64, \
    name, \
    {0,value,0.0,NULL,NULL}, \
    pattern \
  }
#define TEXT_MACRO_DOUBLE(name,value,pattern) \
  { \
    TEXT_MACRO_TYPE_DOUBLE, \
    name, \
    {0,0LL,value,NULL,NULL}, \
    pattern \
  }
#define TEXT_MACRO_CSTRING(name,value,pattern) \
  { \
    TEXT_MACRO_TYPE_CSTRING, \
    name, \
    {0,0LL,0.0,value,NULL}, \
    pattern \
  }
#define TEXT_MACRO_STRING(name,value,pattern) \
  { \
    TEXT_MACRO_TYPE_STRING, \
    name, \
    {0,0LL,0.0,NULL,value}, \
    pattern \ \
  }

/***********************************************************************\
* Name   : TEXT_MACRO_N_*
* Purpose: init text macro
* Input  : textMacro - text macro variable
*          _name     - macro name (including %)
*          _value    - value
*          _pattern  - regular expression pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define TEXT_MACRO_N_INTEGER(textMacro,_name,_value,_pattern) \
  do { \
    textMacro.type    = TEXT_MACRO_TYPE_INTEGER; \
    textMacro.name    = _name; \
    textMacro.value.i = _value; \
    textMacro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_INTEGER64(textMacro,_name,_value,_pattern) \
  do { \
    textMacro.type    = TEXT_MACRO_TYPE_INTEGER64; \
    textMacro.name    = _name; \
    textMacro.value.l = _value; \
    textMacro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_DOUBLE(textMacro,_name,_value,_pattern) \
  do { \
    textMacro.type    = TEXT_MACRO_TYPE_DOUBLE; \
    textMacro.name    = _name; \
    textMacro.value.d = _value; \
    textMacro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_CSTRING(textMacro,_name,_value,_pattern) \
  do { \
    textMacro.type    = TEXT_MACRO_TYPE_CSTRING; \
    textMacro.name    = _name; \
    textMacro.value.s = _value; \
    textMacro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_STRING(textMacro,_name,_value,_pattern) \
  do { \
    textMacro.type         = TEXT_MACRO_TYPE_STRING; \
    textMacro.name         = _name; \
    textMacro.value.string = (String)_value; \
    textMacro.pattern      = _pattern; \
  } while (0)

/***********************************************************************\
* Name   : TEXT_MACRO_X_*
* Purpose: init text macro
* Input  : name    - macro name (including %)
*          value   - value
*          pattern - regular expression pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define TEXT_MACRO_X_INTEGER(_name,_value,_pattern) \
  do { \
    assert(__textMacro < __textMacroEnd); \
    __textMacro->type    = TEXT_MACRO_TYPE_INTEGER; \
    __textMacro->name    = _name; \
    __textMacro->value.i = _value; \
    __textMacro->pattern = _pattern; \
    __textMacro++; \
  } while (0)
#define TEXT_MACRO_X_INTEGER64(_name,_value,_pattern) \
  do { \
    assert(__textMacro < __textMacroEnd); \
    __textMacro->type    = TEXT_MACRO_TYPE_INTEGER64; \
    __textMacro->name    = _name; \
    __textMacro->value.l = _value; \
    __textMacro->pattern = _pattern; \
    __textMacro++; \
  } while (0)
#define TEXT_MACRO_X_DOUBLE(_name,_value,_pattern) \
  do { \
    assert(__textMacro < __textMacroEnd); \
    __textMacro->type    = TEXT_MACRO_TYPE_DOUBLE; \
    __textMacro->name    = _name; \
    __textMacro->value.d = _value; \
    __textMacro->pattern = _pattern; \
    __textMacro++; \
  } while (0)
#define TEXT_MACRO_X_CSTRING(_name,_value,_pattern) \
  do { \
    assert(__textMacro < __textMacroEnd); \
    __textMacro->type    = TEXT_MACRO_TYPE_CSTRING; \
    __textMacro->name    = _name; \
    __textMacro->value.s = _value; \
    __textMacro->pattern = _pattern; \
    __textMacro++; \
  } while (0)
#define TEXT_MACRO_X_STRING(_name,_value,_pattern) \
  do { \
    assert(__textMacro < __textMacroEnd); \
    __textMacro->type         = TEXT_MACRO_TYPE_STRING; \
    __textMacro->name         = _name; \
    __textMacro->value.string = (String)_value; \
    __textMacro->pattern      = _pattern; \
    __textMacro++; \
  } while (0)

/***********************************************************************\
* Name   : MISC_SIGNAL_MASK_CLEAR
* Purpose: clear signal mask
* Input  : signalMask - signal mask
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGSET_T
  #define MISC_SIGNAL_MASK_CLEAR(signalMaks) sigemptyset(&signalMask)
#else /* not HAVE_SIGSET_T */
  #define MISC_SIGNAL_MASK_CLEAR(signalMaks) memClear(&signalMask,sizeof(signalMask))
#endif  /* HAVE_SIGSET_T */

/***********************************************************************\
* Name   : MISC_SIGNAL_MASK_SET
* Purpose: add signal mask
* Input  : signalMask - signal mask
*          signal     - signal to add
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGSET_T
  #define MISC_SIGNAL_MASK_SET(signalMaks,signal) sigaddset(&signalMask,signal)
#else /* not HAVE_SIGSET_T */
  #define MISC_SIGNAL_MASK_SET(signalMaks,signal) do {} while (0)
#endif  /* HAVE_SIGSET_T */

/***********************************************************************\
* Name   : MISC_HANDLES_ITERATE
* Purpose: iterated over handles and execute block
* Input  : waitHandle - wait handle
*          handle     - iteration handle
*          events     - events
* Output : handle - handle
*          events - events
* Return : -
* Notes  : variable will contain all active handles
*          usage:
*            int  handle;
*            uint events;
*            MISC_HANDLES_ITERATE(&waitHandle,handle,event)
*            {
*              ...
*            }
\***********************************************************************/

#define MISC_HANDLES_ITERATE(waitHandle,handle,events) \
  for (uint __i ## COUNTER = Misc_handleIterate(waitHandle,0,&handle,&events); \
       __i ## COUNTER < Misc_handlesIterateCount(waitHandle); \
       __i ## COUNTER = Misc_handleIterate(waitHandle,__i ## COUNTER +1,&handle,&events) \
      ) \
  if (events != 0)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Misc_getRandom
* Purpose: get random value
* Input  : -
* Output : -
* Return : random value
* Notes  : -
\***********************************************************************/

uint64 Misc_getRandom(uint64 min, uint64 max);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_getTimestamp
* Purpose: get timestamp
* Input  : -
* Output : -
* Return : timestamp [us]
* Notes  : -
\***********************************************************************/

uint64 Misc_getTimestamp(void);

/***********************************************************************\
* Name   : Misc_initTimeout
* Purpose: init timeout
* Input  : timeoutInfo - timeout info
*          timeout     - timeout [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Misc_initTimeout(TimeoutInfo *timeoutInfo, long timeout);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE void Misc_initTimeout(TimeoutInfo *timeoutInfo, long timeout)
{
  assert(timeoutInfo != NULL);

  timeoutInfo->timeout      = timeout;
  timeoutInfo->endTimestamp = (timeout != WAIT_FOREVER) ? Misc_getTimestamp()+(uint64)timeout*US_PER_MS : 0LL;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_doneTimeout
* Purpose: done timeout
* Input  : timeoutInfo - timeout info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Misc_doneTimeout(TimeoutInfo *timeoutInfo);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE void Misc_doneTimeout(TimeoutInfo *timeoutInfo)
{
  assert(timeoutInfo != NULL);

  UNUSED_VARIABLE(timeoutInfo);
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_restartTimeout
* Purpose: restart timeout
* Input  : timeoutInfo - timeout info
*          timeout     - timeout [ms] (can be 0)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Misc_restartTimeout(TimeoutInfo *timeoutInfo, long timeout);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE void Misc_restartTimeout(TimeoutInfo *timeoutInfo, long timeout)
{
  assert(timeoutInfo != NULL);

  if (timeout != 0L)
  {
    timeoutInfo->timeout = timeout;
  }
  timeoutInfo->endTimestamp = (timeoutInfo->timeout != WAIT_FOREVER) ? Misc_getTimestamp()+(uint64)timeoutInfo->timeout*US_PER_MS : 0LL;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_stopTimeout
* Purpose: stop timeout
* Input  : timeoutInfo - timeout info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Misc_stopTimeout(TimeoutInfo *timeoutInfo);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE void Misc_stopTimeout(TimeoutInfo *timeoutInfo)
{
  assert(timeoutInfo != NULL);

  timeoutInfo->endTimestamp = 0LL;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_getRestTimeout
* Purpose: get rest timeout
* Input  : timeoutInfo - timeout info
*          maxTimeout  - max. timeout [ms]
* Output : -
* Return : rest timeout [ms]
* Notes  : -
\***********************************************************************/

INLINE long Misc_getRestTimeout(const TimeoutInfo *timeoutInfo, long maxTimeout);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE long Misc_getRestTimeout(const TimeoutInfo *timeoutInfo, long maxTimeout)
{
  uint64 timestamp;

  assert(timeoutInfo != NULL);

  if (timeoutInfo->timeout != WAIT_FOREVER)
  {
    timestamp = Misc_getTimestamp();
    return (timestamp <= timeoutInfo->endTimestamp)
             ? MIN((long)((timeoutInfo->endTimestamp-timestamp)/US_PER_MS),maxTimeout)
             : 0L;
  }
  else
  {
    return maxTimeout;
  }
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_getTotalTimeout
* Purpose: get total timeout
* Input  : timeoutInfo - timeout info
* Output : -
* Return : total timeout [ms]
* Notes  : -
\***********************************************************************/

INLINE long Misc_getTotalTimeout(const TimeoutInfo *timeoutInfo);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE long Misc_getTotalTimeout(const TimeoutInfo *timeoutInfo)
{
  assert(timeoutInfo != NULL);

  return timeoutInfo->timeout;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_isTimeout
* Purpose: check if timeout
* Input  : timeoutInfo - timeout info
* Output : -
* Return : TRUE iff timeout
* Notes  : -
\***********************************************************************/

INLINE bool Misc_isTimeout(const TimeoutInfo *timeoutInfo);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE bool Misc_isTimeout(const TimeoutInfo *timeoutInfo)
{
  assert(timeoutInfo != NULL);

  return (   (timeoutInfo->timeout != WAIT_FOREVER)
          && Misc_getTimestamp() >= timeoutInfo->endTimestamp
         );
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_getCurrentDateTime
* Purpose: get current date/time
* Input  : -
* Output : -
* Return : date/time (seconds since 1970-01-01 00:00:00)
* Notes  : -
\***********************************************************************/

uint64 Misc_getCurrentDateTime(void);

/***********************************************************************\
* Name   : Misc_getCurrentDate
* Purpose: get current date
* Input  : -
* Output : -
* Return : date (seconds since 1970-01-01 00:00:00 without time)
* Notes  : -
\***********************************************************************/

uint64 Misc_getCurrentDate(void);

/***********************************************************************\
* Name   : Misc_getCurrentTime
* Purpose: get current time
* Input  : -
* Output : -
* Return : time (seconds since 00:00:00)
* Notes  : -
\***********************************************************************/

uint32 Misc_getCurrentTime(void);

/***********************************************************************\
* Name   : Misc_splitDateTime
* Purpose: split date/time into parts
* Input  : dateTime - date/time (seconds since 1970-1-1 00:00:00)
* Output : year             - year, YYYY (could be NULL)
*          month            - month, 1..12 (could be NULL)
*          day              - day, 1..31 (could be NULL)
*          hour             - hour, 0..23 (could be NULL)
*          minute           - minute, 0..59 (could be NULL)
*          second           - second, 0..59 (could be NULL)
*          weekDay          - week day, DAY_* (could be NULL)
*          isDayLightSaving - TRUE iff day light saving active (can be
*                             NULL)
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
                        WeekDays *weekDay,
                        bool     *isDayLightSaving
                       );

/***********************************************************************\
* Name   : Misc_getWeekDay
* Purpose: get week day
* Input  : year  - year
*          month - month
*          day   - day
* Output : -
* Return : week day
* Notes  : -
\***********************************************************************/

WeekDays Misc_getWeekDay(uint year, uint month, uint day);

/***********************************************************************\
* Name   : Misc_getLastDayOfMonth
* Purpose: get last day of month
* Input  : year  - year
*          month - month
* Output : -
* Return : last day in month [1..31]
* Notes  : -
\***********************************************************************/

uint Misc_getLastDayOfMonth(uint year, uint month);

/***********************************************************************\
* Name   : Misc_isLeapYear
* Purpose: check if year is a leap year
* Input  : year - year
* Output : -
* Return : TRUE iff year is a leap year
* Notes  : -
\***********************************************************************/

INLINE bool Misc_isLeapYear(uint year);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE bool Misc_isLeapYear(uint year)
{
  return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_makeDateTime
* Purpose: create date/time from parts
* Input  : year             - year, YYYY
*          month            - month, 1..12
*          day              - day, 1..31
*          hour             - hour, 0..23
*          minute           - minute, 0..59
*          second           - second, 0..59
*          isDayLightSaving - TRUE iff day light saving is active
* Return : date/time (seconds since 1970-1-1 00:00:00)
* Return : -
* Notes  : -
\***********************************************************************/

uint64 Misc_makeDateTime(uint year,
                         uint month,
                         uint day,
                         uint hour,
                         uint minute,
                         uint second,
                         bool isDayLightSaving
                        );

/***********************************************************************\
* Name   : Misc_parseDateTime
* Purpose: parse known date/time string
* Input  : string - string to parse
* Output : -
* Return : date/time (seconds since 1970-1-1 00:00:00)
* Notes  : -
\***********************************************************************/

uint64 Misc_parseDateTime(const char *string);

/***********************************************************************\
* Name   : Misc_formatDateTime, Misc_formatDateTimeCString
* Purpose: format date/time and append
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

/***********************************************************************\
* Name   : Misc_mdelay
* Purpose: delay program execution
* Input  : time - delay time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Misc_mdelay(uint64 time);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE void Misc_mdelay(uint64 time)
{
  Misc_udelay(time*US_PER_MS);
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_userNameToUserId
* Purpose: convert user name to user id
* Input  : name - user name
* Output : -
* Return : user id or FILE_DEFAULT_USER_ID if user not found
* Notes  : -
\***********************************************************************/

uint32 Misc_userNameToUserId(const char *name);

/***********************************************************************\
* Name   : Misc_userNameToUserId
* Purpose: convert user name to user id
* Input  : name     - name variable
*          nameSize - max. size of name
*          userId   - user id
* Output : name - user name
* Return : user name or "NONE" if user not found
* Notes  : -
\***********************************************************************/

const char *Misc_userIdToUserName(char *name, uint nameSize, uint32 userId);

/***********************************************************************\
* Name   : Misc_groupNameToGroupId
* Purpose: convert group name to group id
* Input  : name - group name
* Output : -
* Return : user id or FILE_DEFAULT_GROUP_ID if group not found
* Notes  : -
\***********************************************************************/

uint32 Misc_groupNameToGroupId(const char *name);

/***********************************************************************\
* Name   : Misc_groupNameToGroupId
* Purpose: convert group name to group id
* Input  : name     - name variable
*          nameSize - max. size of name
*          groupId  - group id
* Output : name - group name
* Return : group name or "NONE" if user not found
* Notes  : -
\***********************************************************************/

const char *Misc_groupIdToGroupName(char *name, uint nameSize, uint32 groupId);

/***********************************************************************\
* Name   : Misc_getCurrentUserName
* Purpose: get current user name
* Input  : string - string variable
* Output : -
* Return : string with current user name
* Notes  : -
\***********************************************************************/

String Misc_getCurrentUserName(String string);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_getId
* Purpose: get unique identifier
* Input  : -
* Output : -
* Return : unique identifier (> 0)
* Notes  : -
\***********************************************************************/

uint Misc_getId(void);

/***********************************************************************\
* Name   : Misc_getUUID, Misc_getUUIDCString
* Purpose: get new universally unique identifier (DCE 1.1)
* Input  : string     - string variable
*          buffer     - buffer
*          bufferSize - buffer size
* Output : -
* Return : new universally unique identifier
* Notes  : -
\***********************************************************************/

String Misc_getUUID(String string);
const char *Misc_getUUIDCString(char *buffer, uint bufferSize);

/***********************************************************************\
* Name   : Misc_setApplicationId, Misc_setApplicationIdCString
* Purpose: set application id
* Input  : data   - application id
*          length - length of application id data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_setApplicationId(const byte data[], uint length);
void Misc_setApplicationIdCString(const char *data);

/***********************************************************************\
* Name   : Misc_getMachineId
* Purpose: get unique machine id
* Input  : -
* Output : -
* Return : machine id
* Notes  : -
\***********************************************************************/

MachineId Misc_getMachineId(void);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_expandMacros
* Purpose: expand macros %<name>:<format> in string
* Input  : string               - string variable
*          templateStruing      - template string with macros
*          macros               - array with macro definitions
*          macroCount           - number of macro definitions
*          expandMacroCharacter - TRUE to expand %% -> %
* Output : s - string with expanded macros
* Return : expanded string
* Notes  : -
\***********************************************************************/

String Misc_expandMacros(String           string,
                         const char       *templateString,
                         ExpandMacroModes expandMacroMode,
                         const TextMacro  macros[],
                         uint             macroCount,
                         bool             expandMacroCharacter
                        );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_waitHandle
* Purpose: wait for single handle
* Input  : handle     - handle
*          signalMask - signal mask (can be NULL)
*          events     - events to wait for
*          timeout    - timeout [ms[
* Output : -
* Return : events; see HANDLE_EVENT_...
* Notes  : -
\***********************************************************************/

uint Misc_waitHandle(int        handle,
                     SignalMask *signalMask,
                     uint       events,
                     long       timeout
                    );

/***********************************************************************\
* Name   : Misc_initWait
* Purpose: init handle wait
* Input  : waitHandle     - wait handle
*          maxHandleCount - inital max. handle count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_initWait(WaitHandle *waitHandle, uint maxHandleCount);

/***********************************************************************\
* Name   : Misc_doneWait
* Purpose: done handle wait
* Input  : waitHandle - wait handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_doneWait(WaitHandle *waitHandle);

/***********************************************************************\
* Name   : Misc_waitReset
* Purpose: reset handles
* Input  : waitHandle - wait handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_waitReset(WaitHandle *waitHandle);

/***********************************************************************\
* Name   : Misc_waitAdd
* Purpose: add handle
* Input  : waitHandle - wait handle
*          handle     - handle
*          events     - events to wait for
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_waitAdd(WaitHandle *waitHandle, int handle, uint events);

/***********************************************************************\
* Name   : Misc_waitHandles
* Purpose: wait for handles
* Input  : waitHandle - wait handle
*          signalMask - signal mask (can be NULL)
*          timeout    - timeout [ms[
* Output : -
* Return : number of active handles or -1 on error
* Notes  : -
\***********************************************************************/

int Misc_waitHandles(WaitHandle *waitHandle,
                     SignalMask *signalMask,
                     long       timeout
                    );

/***********************************************************************\
* Name   : Misc_handlesIterateCount
* Purpose: get handles iterator couont
* Input  : waitHandle - wait handle
* Output : -
* Return : handles iterator count
* Notes  : -
\***********************************************************************/

INLINE uint Misc_handlesIterateCount(const WaitHandle *waitHandle);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE uint Misc_handlesIterateCount(const WaitHandle *waitHandle)
{
  assert(waitHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    return waitHandle->handleCount;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      return waitHandle->handleCount;
    #else /* not HAVE_WSAPOLL */
      return waitHandle->handleCount*3;
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_handleIterate
* Purpose: handles iterator
* Input  : waitHandle - wait handle
*          i          - iterator counter
*          handle     - handle variable
*          events     - events variable
* Output : handle - handle
*          events - events
* Return : iterator counter
* Notes  : -
\***********************************************************************/

INLINE uint Misc_handleIterate(const WaitHandle *waitHandle, uint i, int *handle, uint *events);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE uint Misc_handleIterate(const WaitHandle *waitHandle, uint i, int *handle, uint *events)
{
  assert(waitHandle != NULL);
  assert(handle != NULL);
  assert(events != NULL);

  #if   defined(PLATFORM_LINUX)
    (*handle) = waitHandle->pollfds[i].fd;
    (*events) =  waitHandle->pollfds[i].revents;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      (*handle) = waitHandle->pollfds[i].fd;
      (*events) =  waitHandle->pollfds[i].revents;
    #else /* not HAVE_WSAPOLL */
      switch (i%3)
      {
        case 0: (*handle) = waitHandle->handles[i/3]; (*events) = FD_ISSET(waitHandle->handles[i/3],&waitHandle->readfds     ) ? HANDLE_EVENT_INPUT  : 0; break;
        case 1: (*handle) = waitHandle->handles[i/3]; (*events) = FD_ISSET(waitHandle->handles[i/3],&waitHandle->writefds    ) ? HANDLE_EVENT_OUTPUT : 0; break;
        case 2: (*handle) = waitHandle->handles[i/3]; (*events) = FD_ISSET(waitHandle->handles[i/3],&waitHandle->exceptionfds) ? HANDLE_EVENT_ERROR  : 0; break;
      }
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */

  return i;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_isAnyEvent
* Purpose: check if any event occured
* Input  : events - events
* Output : -
* Return : TRUE iff event occured
* Notes  : -
\***********************************************************************/

INLINE bool Misc_isAnyEvent(uint events);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE bool Misc_isAnyEvent(uint events)
{
  return events != 0;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_isHandleEvent
* Purpose: check if event occured
* Input  : events - events
*          event  - event to check
* Output : -
* Return : TRUE iff event occured
* Notes  : -
\***********************************************************************/

INLINE bool Misc_isHandleEvent(uint events, uint event);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE bool Misc_isHandleEvent(uint events, uint event)
{
  return (events & event) != 0;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_findCommandInPath
* Purpose: find command in PATH
* Input  : command - command variable
*          name    - command name
* Output : command - absolute command file name
* Return : TRUE iff found
* Notes  : -
\***********************************************************************/

bool Misc_findCommandInPath(String     command,
                            const char *name
                           );

/***********************************************************************\
* Name   : Misc_executeCommand
* Purpose: execute external command
* Input  : commandTemplate         - command template string
*          macros                  - macros array
*          macroCount              - number of macros in array
*          commandLine             - command line variable or NULL
*          stdoutExecuteIOFunction - stdout callback or NULL
*          stdoutExecuteIOUserData - user data for stdoout callback
*          stderrExecuteIOFunction - stderr callback or NULL
*          stderrExecuteIOUserData - user data for stderr callback
* Output : commandLine - command line
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Misc_executeCommand(const char        *commandTemplate,
                           const TextMacro   macros[],
                           uint              macroCount,
                           String            commandLine,
                           ExecuteIOFunction stdoutExecuteIOFunction,
                           void              *stdoutExecuteIOUserData,
                           ExecuteIOFunction stderrExecuteIOFunction,
                           void              *stderrExecuteIOUserData
                          );

/***********************************************************************\
* Name   : Misc_executeScript
* Purpose: execute external script with shell
* Input  : script                  - script
*          stdoutExecuteIOFunction - stdout callback or NULL
*          stdoutExecuteIOUserData - user data for stdoout callback
*          stderrExecuteIOFunction - stderr callback or NULL
*          stderrExecuteIOUserData - user data for stderr callback
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Misc_executeScript(const char       *script,
                          ExecuteIOFunction stdoutExecuteIOFunction,
                          void              *stdoutExecuteIOUserData,
                          ExecuteIOFunction stderrExecuteIOFunction,
                          void              *stderrExecuteIOUserData
                         );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_isTerminal
* Purpose: check if handle is a terminal
* Input  : handle - handle
* Output : -
* Return : TRUE iff terminal
* Notes  : -
\***********************************************************************/

bool Misc_isTerminal(int handle);

/***********************************************************************\
* Name   : Misc_isStdoutTerminal
* Purpose: check if stdout is a terminal
* Input  : -
* Output : -
* Return : TRUE iff stdout is terminal
* Notes  : -
\***********************************************************************/

INLINE bool Misc_isStdoutTerminal(void);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE bool Misc_isStdoutTerminal(void)
{
  return Misc_isTerminal(STDOUT_FILENO);
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

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

/***********************************************************************\
* Name   : Misc_getConsoleSize
* Purpose: get size of console window
* Input  : -
* Output : rows    - number of rows (can be NULL)
*          columns - number of columns (can be NULL)
* Return : -
* Notes  : default is 25x80
\***********************************************************************/

void Misc_getConsoleSize(uint *rows, uint *columns);

/***********************************************************************\
* Name   : Misc_getConsoleRows
* Purpose: get number of rows of console window
* Input  : -
* Output : -
* Return : number of rows
* Notes  : default is 25
\***********************************************************************/

INLINE uint Misc_getConsoleRows(void);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE uint Misc_getConsoleRows(void)
{
  uint n;

  Misc_getConsoleSize(&n,NULL);

  return n;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_getConsoleColumns
* Purpose: get number of columns of console window
* Input  : -
* Output : -
* Return : number of columns
* Notes  : default is 80
\***********************************************************************/

INLINE uint Misc_getConsoleColumns(void);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE uint Misc_getConsoleColumns(void)
{
  uint n;

  Misc_getConsoleSize(NULL,&n);

  return n;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_performanceFilterInit
* Purpose: initialize performance filter
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
* Purpose: deinitialize performance filter
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

/***********************************************************************\
* Name   : Misc_base64Encode
* Purpose: encode base64
* Input  : string     - string variable to append to
*          data       - data to encode
*          dataLength - length of data to encode
* Output : -
* Return : encoded string
* Notes  : -
\***********************************************************************/

String Misc_base64Encode(String string, const void *data, uint dataLength);
void *Misc_base64EncodeBuffer(void *buffer, uint bufferLength, const void *data, uint dataLength);

/***********************************************************************\
* Name   : Misc_base64EncodeLength
* Purpose: get base64 encode length
* Input  : data       - data to encode
*          dataLength - length of data to encode
* Output : -
* Return : encoded length
* Notes  : -
\***********************************************************************/

INLINE uint Misc_base64EncodeLength(const void *data, uint dataLength);
#if defined(NDEBUG) || defined(__MISC_IMPLEMENTATION__)
INLINE uint Misc_base64EncodeLength(const void *data, uint dataLength)
{
  assert(data != NULL);

  UNUSED_VARIABLE(data);

  return ((dataLength+3-1)/3)*4;
}
#endif /* NDEBUG || __MISC_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Misc_base64Decode, Misc_base64DecodeCString
* Purpose: decode base64
* Input  : data          - data variable
*          maxDataLength - max. length of data
*          dataLength    - data length variable (can be NULL)
*          string,s      - base64 string
*          index         - start index or STRING_BEGIN
*          buffer        - buffer with base64 data
*          bufferLength  - length of buffer with base64 data
* Output : data       - data
*          dataLength - data length
* Return : TRUE iff data decoded
* Notes  : -
\***********************************************************************/

bool Misc_base64Decode(void *data, uint maxDataLength, uint *dataLength, ConstString string, ulong index);
bool Misc_base64DecodeCString(void *data, uint maxDataLength, uint *dataLength, const char *s);
bool Misc_base64DecodeBuffer(void *data, uint maxDataLength, uint *dataLength, const void *buffer, uint bufferLength);

/***********************************************************************\
* Name   : Misc_base64DecodeLength, Misc_base64DecodeLengthCString
* Purpose: get base64 decode length
* Input  : string,s - base64 string
*          index    - start index or STRING_BEGIN
* Output : -
* Return : decoded length or 0
* Notes  : -
\***********************************************************************/

uint Misc_base64DecodeLength(ConstString string, ulong index);
uint Misc_base64DecodeLengthCString(const char *s);
uint Misc_base64DecodeLengthBuffer(const void *buffer, uint bufferLength);

/***********************************************************************\
* Name   : Misc_hexEncode
* Purpose: encoded data as hex-string
* Input  : string     - string variable to append to
*          data       - data to encode
*          dataLength - length of data to encode
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String Misc_hexEncode(String string, const void *data, uint dataLength);

/***********************************************************************\
* Name   : Misc_hexDecode, Misc_hexDecodeCString
* Purpose: decode hex-string into data
* Input  : data          - data variable
*          maxDataLength - max. length of data
*          string,s      - base64 string
*          index         - start index or STRING_BEGIN
* Output : data - data
* Return : TRUE iff data decoded
* Notes  : -
\***********************************************************************/

bool Misc_hexDecode(void *data, uint *dataLength, ConstString string, ulong index, uint maxDataLength);
bool Misc_hexDecodeCString(void *data, uint *dataLength, const char *s, uint maxDataLength);

/***********************************************************************\
* Name   : Misc_hexDecodeLength, Misc_hexDecodeLengthCString
* Purpose: get hex decode length
* Input  : string,s - hex string
*          index    - start index or STRING_BEGIN
* Output : -
* Return : decoded length
* Notes  : -
\***********************************************************************/

uint Misc_hexDecodeLength(ConstString string, ulong index);
uint Misc_hexDecodeLengthCString(const char *s);

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
/***********************************************************************\
* Name   : Misc_getRegistryString
* Purpose: get string from Windows registry
* Input  : string - string varibale
*          name   - registry value name
* Output : -
* Return : TRUE iff read
* Notes  : -
\***********************************************************************/

bool Misc_getRegistryString(String string, HKEY parentKey, const char *subKey, const char *name);
#endif /* PLATFORM_... */

#ifdef __cplusplus
  }
#endif

#endif /* __MISC__ */

/* end of file */
