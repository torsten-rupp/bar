/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/misc.h,v $
* $Revision: 1.6 $
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

/***************************** Datatypes *******************************/
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
* Name   : Misc_getDateTime
* Purpose: get current date/time
* Input  : buffer     - buffer for date/time stirng
*          bufferSize - buffer size
* Output : -
* Return : date/time string
* Notes  : -
\***********************************************************************/

const char *Misc_getDateTime(char *buffer, uint bufferSize);

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
