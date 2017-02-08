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

// length of UUID string (xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx)
#define MISC_UUID_STRING_LENGTH 36

// text macro patterns
#define TEXT_MACRO_PATTERN_INTEGER   "[+-]{0,1}\\d+"
#define TEXT_MACRO_PATTERN_INTEGER64 "[+-]{0,1}\\d+"
#define TEXT_MACRO_PATTERN_DOUBLE    "[+-]{0,1}(\\d+|\\d+\.\\d*|\\d*\.\\d+)"
#define TEXT_MACRO_PATTERN_CSTRING   "\\S+"
#define TEXT_MACRO_PATTERN_STRING    "\\S+"

/***************************** Datatypes *******************************/

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

// expand types
typedef enum
{
  EXPAND_MACRO_MODE_STRING,
  EXPAND_MACRO_MODE_PATTERN
} ExpandMacroModes;

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
* Name   : TEXT_MACRO_*
* Purpose: init text macro
* Input  : macro    - macro variable
*          _name    - macro name (including %)
*          _value   - value
*          _pattern - regular expression pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define TEXT_MACRO_N_INTEGER(macro,_name,_value,_pattern) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_INTEGER; \
    macro.name    = _name; \
    macro.value.i = _value; \
    macro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_INTEGER64(macro,_name,_value,_pattern) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_INTEGER64; \
    macro.name    = _name; \
    macro.value.l = _value; \
    macro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_DOUBLE(macro,_name,_value,_pattern) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_DOUBLE; \
    macro.name    = _name; \
    macro.value.d = _value; \
    macro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_CSTRING(macro,_name,_value,_pattern) \
  do { \
    macro.type    = TEXT_MACRO_TYPE_CSTRING; \
    macro.name    = _name; \
    macro.value.s = _value; \
    macro.pattern = _pattern; \
  } while (0)
#define TEXT_MACRO_N_STRING(macro,_name,_value,_pattern) \
  do { \
    macro.type         = TEXT_MACRO_TYPE_STRING; \
    macro.name         = _name; \
    macro.value.string = (String)_value; \
    macro.pattern      = _pattern; \
  } while (0)

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
* Name   : Misc_getCurrentDateTime
* Purpose: get current date/time
* Input  : -
* Output : -
* Return : date/time (seconds since 1970-01-01 00:00:00)
* Notes  : -
\***********************************************************************/

uint64 Misc_getCurrentDateTime(void);

/***********************************************************************\
* Name   : Misc_getCurrentDateTime
* Purpose: get current date/time
* Input  : -
* Output : -
* Return : date (seconds since 1970-01-01 00:00:00)
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
*          stdoutExecuteIOFunction - stdout callback or NULL
*          stdoutExecuteIOUserData - user data for stdoout callback
*          stderrExecuteIOFunction - stderr callback or NULL
*          stderrExecuteIOUserData - user data for stderr callback
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Misc_executeCommand(const char        *commandTemplate,
                           const TextMacro   macros[],
                           uint              macroCount,
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

String Misc_base64Encode(String string, const byte *data, ulong dataLength);

/***********************************************************************\
* Name   : Misc_base64Decode
* Purpose: decode base64
* Input  : data          - data variable
*          maxDataLength - max. length of data
*          string,s      - base64 string
*          index         - start index or STRING_BEGIN
* Output : -
* Return : length of decoded data or -1 on error
* Notes  : -
\***********************************************************************/

bool Misc_base64Decode(byte *data, ulong dataLength, ConstString string, ulong index);
bool Misc_base64DecodeCString(byte *data, uint dataLength, const char *s);

/***********************************************************************\
* Name   : Misc_base64DecodeLength
* Purpose: get base64 decode length
* Input  : string,s - base64 string
*          index    - start index or STRING_BEGIN
* Output : -
* Return : decoded length
* Notes  : -
\***********************************************************************/

ulong Misc_base64DecodeLength(ConstString string, ulong index);
ulong Misc_base64DecodeLengthCString(const char *s);

#ifdef __cplusplus
  }
#endif

#endif /* __MISC__ */

/* end of file */
