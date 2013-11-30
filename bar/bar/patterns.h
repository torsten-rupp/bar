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
typedef enum
{
  PATTERN_TYPE_GLOB,                    // * and ?
  PATTERN_TYPE_REGEX,                   // regular expressions
  PATTERN_TYPE_EXTENDED_REGEX           // extended regular expressions
} PatternTypes;

// pattern flags
#define PATTERN_FLAG_NONE        0x00   // no flags
#define PATTERN_FLAG_IGNORE_CASE 0x01   // ignore upper/lower case

// match modes
typedef enum
{
  PATTERN_MATCH_MODE_BEGIN,             // match begin of string
  PATTERN_MATCH_MODE_END,               // match end of string
  PATTERN_MATCH_MODE_EXACT,             // match exact
  PATTERN_MATCH_MODE_ANY                // match anywhere
} PatternMatchModes;

/***************************** Datatypes *******************************/

// pattern
typedef struct
{
  PatternTypes type;
  uint         flags;
  regex_t      regexBegin;              // regular expression for matching begin
  regex_t      regexEnd;                // regular expression for matching end
  regex_t      regexExact;              // regular expression for matching exact
  regex_t      regexAny;                // regular expression for matching anywhere
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
* Name   : Pattern_patternTypeToString
* Purpose: get name of pattern type
* Input  : patternType  - pattern type
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *Pattern_patternTypeToString(PatternTypes patternType, const char *defaultValue);

/***********************************************************************\
* Name   : Pattern_parsePatternType
* Purpose: pattern pattern type
* Input  : name - name of pattern type
* Output : patternType - pattern type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Pattern_parsePatternType(const char *name, PatternTypes *patternType);

/***********************************************************************\
* Name   : Pattern_init
* Purpose: init pattern
* Input  : pattern      - pattern variable
*          string       - pattern
*          patternType  - pattern type; see PATTERN_TYPE_*
*          patternFlags - pattern flags; see PATTERN_FLAG_*
* Output : pattern - initialzied variable
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Pattern_init(Pattern *pattern, const String string, PatternTypes patternType, uint patternFlags);
Errors Pattern_initCString(Pattern *pattern, const char *string, PatternTypes patternType, uint patternFlags);

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
* Input  : string       - pattern
*          patternType  - pattern type; see PATTERN_TYPE_*
*          patternFlags - pattern flags; see PATTERN_FLAG_*
* Output : -
* Return : pattern or NULL
* Notes  : -
\***********************************************************************/

Pattern *Pattern_new(const String string, PatternTypes patternType, uint patternFlags);

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
