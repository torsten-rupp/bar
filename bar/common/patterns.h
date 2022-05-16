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

#include "common/global.h"
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
  PATTERN_TYPE_EXTENDED_REGEX,          // extended regular expressions
  PATTERN_TYPE_UNKNOWN
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
  String       regexString;             // regular expression string
  int          regexFlags;              // regular expression flags
  regex_t      regexBegin;              // regular expression for matching begin
  regex_t      regexEnd;                // regular expression for matching end
  regex_t      regexExact;              // regular expression for matching exact
  regex_t      regexAny;                // regular expression for matching anywhere
} Pattern;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Pattern_init(...)        __Pattern_init       (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Pattern_initCString(...) __Pattern_initCString(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Pattern_done(...)        __Pattern_done       (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
* Input  : name     - name of pattern type
*          userData - user data (not used)
* Output : patternType - pattern type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Pattern_parsePatternType(const char *name, PatternTypes *patternType, void *userData);

/***********************************************************************\
* Name   : Pattern_init
* Purpose: init pattern
* Input  : pattern      - pattern variable
*          string       - pattern
*          patternType  - pattern type; see PATTERN_TYPE_*
*          patternFlags - pattern flags; see PATTERN_FLAG_*
* Output : pattern - initialized variable
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Pattern_init(Pattern      *pattern,
                      ConstString  string,
                      PatternTypes patternType,
                      uint         patternFlags
                     );
  Errors Pattern_initCString(Pattern      *pattern,
                             const char   *string,
                             PatternTypes patternType,
                             uint         patternFlags
                            );
#else /* not NDEBUG */
  Errors __Pattern_init(const char   *__fileName__,
                        ulong        __lineNb__,
                        Pattern      *pattern,
                        ConstString  string,
                        PatternTypes patternType,
                        uint         patternFlags
                       );
  Errors __Pattern_initCString(const char   *__fileName__,
                               ulong        __lineNb__,
                               Pattern      *pattern,
                               const char   *string,
                               PatternTypes patternType,
                               uint         patternFlags
                              );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Pattern_done
* Purpose: done pattern
* Input  : pattern - pattern
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Pattern_done(Pattern *pattern);
#else /* not NDEBUG */
  void __Pattern_done(const char *__fileName__,
                      ulong      __lineNb__,
                      Pattern    *pattern
                     );
#endif /* NDEBUG */

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

Pattern *Pattern_new(ConstString string, PatternTypes patternType, uint patternFlags);

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
* Name   : Pattern_duplicate
* Purpose: duplicate pattern
* Input  : fromPattern - from pattern
* Output : -
* Return : pattern
* Notes  : -
\***********************************************************************/

Pattern *Pattern_duplicate(const Pattern *fromPattern);

/***********************************************************************\
* Name   : Pattern_copy
* Purpose: copy pattern
* Input  : pattern     - pattern variable
*          fromPattern - from pattern
* Output : pattern - pattern
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Pattern_copy(Pattern *pattern, const Pattern *fromPattern);

/***********************************************************************\
* Name   : Pattern_move
* Purpose: move pattern
* Input  : pattern     - pattern variable
*          fromPattern - from pattern
* Output : pattern - pattern
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Pattern_move(Pattern *pattern, const Pattern *fromPattern);

/***********************************************************************\
* Name   : Pattern_match
* Purpose: patch string with single pattern
* Input  : pattern          - pattern
*          string           - string
*          index            - start index in string
*          patternMatchMode - pattern match mode; see PatternMatchModes
*          matchIndex       - match index variable (can be NULL)
*          matchLength      - match length variable (can be NULL)
* Output : matchIndex       - match index
*          matchLength      - match length
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_match(const Pattern     *pattern,
                   ConstString       string,
                   ulong             index,
                   PatternMatchModes patternMatchMode,
                   ulong             *matchIndex,
                   ulong             *matchLength
                  );

/***********************************************************************\
* Name   : Pattern_checkIsPattern
* Purpose: check if string is a pattern
* Input  : string - string
* Output : -
* Return : TRUE is string is a pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_checkIsPattern(const ConstString string);

/***********************************************************************\
* Name   : Pattern_isValid
* Purpose: check if string is a valid pattern
* Input  : string       - string
*          patternType  - pattern type
* Output : -
* Return : TRUE is string is a valid pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_isValid(const ConstString string, PatternTypes patternType);

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNS__ */

/* end of file */
