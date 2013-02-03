/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver pattern functions
* Systems: all
*
\***********************************************************************/

#ifndef __PATTERNS__
#define __PATTERNS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory 

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #error No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define PATTERN_CHAR_SET_GLOB           "?*"
#define PATTERN_CHAR_SET_REGEX          "*+?{}():[].^$|"
#define PATTERN_CHAR_SET_EXTENDED_REGEX "*+?{}():[].^$|"

// pattern types
#define PATTERN_TYPE_GLOB           0x00  // * and ?
#define PATTERN_TYPE_REGEX          0x01  // regular expressions
#define PATTERN_TYPE_EXTENDED_REGEX 0x02  // extended regular expressions

#define PATTERN_TYPE_MASK 0x0F

// pattern options
#define PATTERN_OPTION_IGNORE_CASE  0x10  // ignore upper/lower case

#define PATTERN_OPTIONS_MASK 0xF0

// match modes
typedef enum
{
  PATTERN_MATCH_MODE_BEGIN,
  PATTERN_MATCH_MODE_END,
  PATTERN_MATCH_MODE_EXACT,
} PatternMatchModes;

/***************************** Datatypes *******************************/

// pattern types
#warning remove and rename PatternTypes -> patternFlags
typedef uint PatternTypes;

// pattern
typedef struct
{
  PatternTypes type;
  regex_t      regexBegin;              // regular expression for matching begin
  regex_t      regexEnd;                // regular expression for matching end
  regex_t      regexExact;              // regular expression for matching exact
} Pattern;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Pattern_initAll
* Purpose: init patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Pattern_initAll(void);

/***********************************************************************\
* Name   : Pattern_doneAll
* Purpose: deinitialize patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void Pattern_doneAll(void);

/***********************************************************************\
* Name   : Pattern_init
* Purpose: init pattern
* Input  : pattern     - pattern variable
*          string      - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : pattern - initialzied variable
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Pattern_init(Pattern *pattern, const String string, PatternTypes patternType);
Errors Pattern_initCString(Pattern *pattern, const char *string, PatternTypes patternType);

/***********************************************************************\
* Name   : Pattern_done
* Purpose: done pattern
* Input  : pattern - pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_done(Pattern *pattern);

/***********************************************************************\
* Name   : Pattern_new
* Purpose: create new pattern
* Input  : string      - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : -
* Return : pattern or NULL
* Notes  : -
\***********************************************************************/

Pattern *Pattern_new(const String string, PatternTypes patternType);

/***********************************************************************\
* Name   : Pattern_delete
* Purpose: delete pattern
* Input  : pattern - pattern to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_delete(Pattern *pattern);

/***********************************************************************\
* Name   : Pattern_match
* Purpose: patch string with single pattern
* Input  : pattern          - pattern
*          string           - string
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_match(const Pattern     *pattern,
                   const String      string,
                   PatternMatchModes patternMatchMode
                  );

/***********************************************************************\
* Name   : Pattern_checkIsPattern
* Purpose: check if string is a pattern
* Input  : string - string
* Output : -
* Return : TRUE is string is a pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_checkIsPattern(const String string);

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNS__ */

/* end of file */
