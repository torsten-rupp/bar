/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/misc.h,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: miscellaneous functions
* Systems: all
*
\***********************************************************************/

#ifndef __MISC__
#define __MISC__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEFAULT_DATE_TIME_FORMAT "%Y-%m-%d %H:%M:%S %Z"

/***************************** Datatypes *******************************/
/* text macro definitions */
typedef enum
{
  TEXT_MACRO_TYPE_INT,
  TEXT_MACRO_TYPE_INT64,
  TEXT_MACRO_TYPE_CSTRING,
  TEXT_MACRO_TYPE_STRING,
} TextMacroTypes;

typedef struct
{
  TextMacroTypes type;
  const char     *name;
  int            i;
  int64          l;
  const char     *s;
  String         string;
} TextMacro;

typedef void(*ExecuteIOFunction)(void         *userData,
                                 const String line
                                );

/* month, day names */
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

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define TEXT_MACRO_INT(macro,_name,value) \
  do { \
    macro.type   = TEXT_MACRO_TYPE_INT; \
    macro.name   = _name; \
    macro.i      = value; \
  } while (0)
#define TEXT_MACRO_INT64(macro,_name,value) \
  do { \
    macro.type   = TEXT_MACRO_TYPE_INT64; \
    macro.name   = _name; \
    macro.l      = value; \
  } while (0)
#define TEXT_MACRO_CSTRING(macro,_name,value) \
  do { \
    macro.type   = TEXT_MACRO_TYPE_CSTRING; \
    macro.name   = _name; \
    macro.s      = value; \
  } while (0)
#define TEXT_MACRO_STRING(macro,_name,value) \
  do { \
    macro.type   = TEXT_MACRO_TYPE_STRING; \
    macro.name   = _name; \
    macro.string = value; \
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
* Input  : dateTime - date/time
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
* Name   : Misc_formatDateTime
* Purpose: format date/time
* Input  : string     - string variable
*          dateTime   - date/time (seconds since 1970-1-1 00:00:00)
*          format     - format string (strftime) or NULL
* Output : -
* Return : date/time string
* Notes  : -
\***********************************************************************/

String Misc_formatDateTime(String string, uint64 dateTime, const char *format);

/***********************************************************************\
* Name   : udelay
* Purpose: delay program execution
* Input  : time - delay time [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_udelay(uint64 time);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_expandMacros
* Purpose: expand macros %... in string
* Input  : string     - string variable
*          template   - string with macros
*          macros     - array with macro definitions
*          macroCount - number of macro definitions
* Output : s - string with expanded macros
* Return : -
* Notes  : -
\***********************************************************************/

void Misc_expandMacros(String          string,
                       const String    template,
                       const TextMacro macros[],
                       uint            macroCount
                      );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Misc_executeCommand
* Purpose: execute external command
* Input  : commandTemplate - command template string
*          macros          - macros array
*          macroCount      - number of macros in array
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

#ifdef __cplusplus
  }
#endif

#endif /* __MISC__ */

/* end of file */
