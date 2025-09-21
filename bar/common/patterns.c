/***********************************************************************\
*
* Contents: pattern functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include "common/strings.h"
#include "common/cstrings.h"

#include "errors.h"

#include "patterns.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char   *name;
  PatternTypes patternType;
} PATTERN_TYPES[] =
{
  { "glob",           PATTERN_TYPE_GLOB           },
  { "regex",          PATTERN_TYPE_REGEX          },
  { "extended_regex", PATTERN_TYPE_EXTENDED_REGEX }
};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getRegularExpression
* Purpose: convert string to regular expression string
* Input  : pattern      - pattern to compile
*          patternType  - pattern type
*          patternFlags - pattern flags
* Output : regexString - regular expression string
*          regexFlags  - regular expression flags
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getRegularExpression(String       regexString,
                                int          *regexFlags,
                                const char   *string,
                                PatternTypes patternType,
                                uint         patternFlags
                               )
{
  assert(regexString != NULL);
  assert(regexFlags != NULL);
  assert(string != NULL);

  (*regexFlags) = 0;
  if ((patternFlags & PATTERN_FLAG_IGNORE_CASE) == PATTERN_FLAG_IGNORE_CASE) (*regexFlags) |= REG_ICASE;
  switch (patternType)
  {
    case PATTERN_TYPE_GLOB:
      {
        ulong i = 0;
        while (string[i] != NUL)
        {
          switch (string[i])
          {
            case '*':
              String_appendCString(regexString,".*");
              i++;
              break;
            case '?':
              String_appendChar(regexString,'.');
              i++;
              break;
            case '.':
              String_appendCString(regexString,"\\.");
              i++;
              break;
            case '\\':
              String_appendCString(regexString,"\\\\");
              i++;
              break;
            case '[':
            case ']':
            case '^':
            case '$':
            case '(':
            case ')':
            case '{':
            case '}':
            case '+':
            case '|':
              String_appendChar(regexString,'\\');
              String_appendChar(regexString,string[i]);
              i++;
              break;
            default:
              String_appendChar(regexString,string[i]);
              i++;
              break;
          }
        }
      }
      break;
    case PATTERN_TYPE_REGEX:
      String_setCString(regexString,string);
      break;
    case PATTERN_TYPE_EXTENDED_REGEX:
      String_setCString(regexString,string);
      (*regexFlags) |= REG_EXTENDED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

/***********************************************************************\
* Name   : compilePattern
* Purpose: compile pattern
* Input  : pattern      - pattern to compile
*          patternType  - pattern type
*          patternFlags - pattern flags
* Output : regexBegin - regular expression for matching begin
*          regexEnd   - regular expression for matching end
*          regexExact - regular expression for exact matching
*          regexAny   - regular expression for matching anywhere
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compilePattern(ConstString regexString,
                            int         regexFlags,
                            regex_t     *regexBegin,
                            regex_t     *regexEnd,
                            regex_t     *regexExact,
                            regex_t     *regexAny
                           )
{
  assert(regexString != NULL);
  assert(regexBegin != NULL);
  assert(regexEnd != NULL);
  assert(regexExact != NULL);
  assert(regexAny != NULL);

  // init variables
  String string = String_new();

  // compile regular expression
  String_set(string,regexString);
  if (String_index(string,STRING_BEGIN) != '^') String_insertChar(string,STRING_BEGIN,'^');
  int error = regcomp(regexBegin,String_cString(string),regexFlags);
  if (error != 0)
  {
    char buffer[256];
    regerror(error,regexBegin,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = NUL;
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,"%s",buffer);
  }

  String_set(string,regexString);
  if (String_index(string,STRING_END) != '$') String_insertChar(string,STRING_BEGIN,'$');
  if (regcomp(regexEnd,String_cString(string),regexFlags) != 0)
  {
    char buffer[256];
    regerror(error,regexEnd,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = NUL;
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,"%s",buffer);
  }

  String_set(string,regexString);
  if (String_index(string,STRING_BEGIN) != '^') String_insertChar(string,STRING_BEGIN,'^');
  if (String_index(string,STRING_END) != '$') String_insertChar(string,STRING_END,'$');
  if (regcomp(regexExact,String_cString(string),regexFlags) != 0)
  {
    char buffer[256];
    regerror(error,regexExact,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = NUL;
    regfree(regexEnd);
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,"%s",buffer);
  }

  String_set(string,regexString);
  if (regcomp(regexAny,String_cString(string),regexFlags) != 0)
  {
    char buffer[256];
    regerror(error,regexAny,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = NUL;
    regfree(regexExact);
    regfree(regexEnd);
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,"%s",buffer);
  }

  // free resources
  String_delete(string);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Pattern_initAll(void)
{
  return ERROR_NONE;
}

void Pattern_doneAll(void)
{
}

const char *Pattern_patternTypeToString(PatternTypes patternType, const char *defaultValue)
{
  size_t i = 0;
  while (   (i < SIZE_OF_ARRAY(PATTERN_TYPES))
         && (PATTERN_TYPES[i].patternType != patternType)
        )
  {
    i++;
  }
  const char *name;
  if (i < SIZE_OF_ARRAY(PATTERN_TYPES))
  {
    name = PATTERN_TYPES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Pattern_parsePatternType(const char *name, PatternTypes *patternType, void *userData)
{
  assert(name != NULL);
  assert(patternType != NULL);

  UNUSED_VARIABLE(userData);

  size_t i = 0;
  while (   (i < SIZE_OF_ARRAY(PATTERN_TYPES))
         && !stringEqualsIgnoreCase(PATTERN_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(PATTERN_TYPES))
  {
    (*patternType) = PATTERN_TYPES[i].patternType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

#ifdef NDEBUG
  Errors Pattern_init(Pattern      *pattern,
                      ConstString  string,
                      PatternTypes patternType,
                      uint         patternFlags
                     )
#else /* not NDEBUG */
  Errors __Pattern_init(const char   *__fileName__,
                        ulong        __lineNb__,
                        Pattern      *pattern,
                        ConstString  string,
                        PatternTypes patternType,
                        uint         patternFlags
                       )
#endif /* NDEBUG */
{
  #ifdef NDEBUG
    return Pattern_initCString(pattern,String_cString(string),patternType,patternFlags);
  #else /* not NDEBUG */
    return __Pattern_initCString(__fileName__,__lineNb__,pattern,String_cString(string),patternType,patternFlags);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
  Errors Pattern_initCString(Pattern      *pattern,
                             const char   *string,
                             PatternTypes patternType,
                             uint         patternFlags
                            )
#else /* not NDEBUG */
  Errors __Pattern_initCString(const char   *__fileName__,
                               ulong        __lineNb__,
                               Pattern      *pattern,
                               const char   *string,
                               PatternTypes patternType,
                               uint         patternFlags
                              )
#endif /* NDEBUG */
{
  Errors error;

  assert(pattern != NULL);

  // initialize variables
  pattern->type        = patternType;
  pattern->regexString = String_new();
  pattern->regexFlags  = 0;

  // get regular expression
  getRegularExpression(pattern->regexString,
                       &pattern->regexFlags,
                       string,
                       patternType,
                       patternFlags
                      );

  // compile pattern
  error = compilePattern(pattern->regexString,
                         pattern->regexFlags,
                         &pattern->regexBegin,
                         &pattern->regexEnd,
                         &pattern->regexExact,
                         &pattern->regexAny
                        );
  if (error != ERROR_NONE)
  {
    String_delete(pattern->regexString);
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(pattern,Pattern);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,pattern,Pattern);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Pattern_done(Pattern *pattern)
#else /* not NDEBUG */
  void __Pattern_done(const char *__fileName__,
                      ulong      __lineNb__,
                      Pattern    *pattern
                     )
#endif /* NDEBUG */
{
  assert(pattern != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(pattern,Pattern);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,pattern,Pattern);
  #endif /* NDEBUG */

  regfree(&pattern->regexAny);
  regfree(&pattern->regexExact);
  regfree(&pattern->regexEnd);
  regfree(&pattern->regexBegin);
  String_delete(pattern->regexString);
}

Pattern *Pattern_new(ConstString string, PatternTypes patternType, uint patternFlags)
{
  Errors  error;

  assert(string != NULL);

  Pattern *pattern = (Pattern*)malloc(sizeof(Pattern));
  if (pattern == NULL)
  {
    return NULL;
  }

  error = Pattern_init(pattern,string,patternType,patternFlags);
  if (error != ERROR_NONE)
  {
    free(pattern);
    return NULL;
  }

  return pattern;
}

void Pattern_delete(Pattern *pattern)
{
  assert(pattern != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(pattern);

  Pattern_done(pattern);
  free(pattern);
}

Pattern *Pattern_duplicate(const Pattern *fromPattern)
{
  assert(fromPattern != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromPattern);

  // allocate pattern
  Pattern *pattern = (Pattern*)malloc(sizeof(Pattern));
  if (pattern == NULL)
  {
    return NULL;
  }

  // copy pattern
  if (Pattern_copy(pattern,fromPattern) != ERROR_NONE)
  {
    free(pattern);
    return NULL;
  }

  return pattern;
}

Errors Pattern_copy(Pattern *pattern, const Pattern *fromPattern)
{
  Errors error;

  assert(pattern != NULL);
  assert(fromPattern != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromPattern);

  // initialize variables
  pattern->type        = fromPattern->type;
  pattern->regexString = String_duplicate(fromPattern->regexString);
  pattern->regexFlags  = fromPattern->regexFlags;

  // compile pattern
  error = compilePattern(pattern->regexString,
                         pattern->regexFlags,
                         &pattern->regexBegin,
                         &pattern->regexEnd,
                         &pattern->regexExact,
                         &pattern->regexAny
                        );
  if (error != ERROR_NONE)
  {
    String_delete(pattern->regexString);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(pattern,Pattern);

  return ERROR_NONE;
}

Errors Pattern_move(Pattern *pattern, const Pattern *fromPattern)
{
  assert(pattern != NULL);
  assert(fromPattern != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromPattern);

  DEBUG_REMOVE_RESOURCE_TRACE(fromPattern,Pattern);

  pattern->type        = fromPattern->type;
  pattern->regexString = fromPattern->regexString;
  pattern->regexFlags  = fromPattern->regexFlags;
  pattern->regexBegin  = fromPattern->regexBegin;
  pattern->regexEnd    = fromPattern->regexEnd;
  pattern->regexExact  = fromPattern->regexExact;
  pattern->regexAny    = fromPattern->regexAny;

  DEBUG_ADD_RESOURCE_TRACE(pattern,Pattern);

  return ERROR_NONE;
}

bool Pattern_match(const Pattern     *pattern,
                   ConstString       string,
                   ulong             index,
                   PatternMatchModes patternMatchMode,
                   ulong             *matchIndex,
                   ulong             *matchLength
                  )
{
  assert(pattern != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(pattern);
  assert(string != NULL);

  bool       matchFlag = FALSE;
  regmatch_t matches[1];
  switch (patternMatchMode)
  {
    case PATTERN_MATCH_MODE_BEGIN:
      matchFlag = (regexec(&pattern->regexBegin,String_cString(string)+index,1,matches,0) == 0);
      break;
    case PATTERN_MATCH_MODE_END:
      matchFlag = (regexec(&pattern->regexEnd,String_cString(string)+index,1,matches,0) == 0);
      break;
    case PATTERN_MATCH_MODE_EXACT:
      matchFlag = (regexec(&pattern->regexExact,String_cString(string)+index,1,matches,0) == 0);
      break;
    case PATTERN_MATCH_MODE_ANY:
      matchFlag = (regexec(&pattern->regexAny,String_cString(string)+index,1,matches,0) == 0);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  if (matchFlag)
  {
    if (matchIndex != NULL)
    {
      (*matchIndex) = index+matches[0].rm_so;
    }
    if (matchLength != NULL)
    {
      (*matchLength) = matches[0].rm_eo-matches[0].rm_so;
    }
  }

  return matchFlag;
}

bool Pattern_checkIsPattern(const ConstString string)
{
  const char *PATTERNS_CHARS = "*?[{";

  assert(string != NULL);

  ulong i           = 0L;
  bool  patternFlag = FALSE;
  while ((i < String_length(string)) && !patternFlag)
  {
    if (String_index(string,i) != '\\')
    {
      patternFlag = (strchr(PATTERNS_CHARS,String_index(string,i)) != NULL);
    }
    else
    {
      i++;
    }
    i++;
  }

  return patternFlag;
}

bool Pattern_isValid(const ConstString string, PatternTypes patternType)
{
  bool    isValid;
  String  regexString;
  int     regexFlags;
  regex_t regex;

  assert(string != NULL);

  // init variables
  regexString = String_new();

  // get regular expression
  getRegularExpression(regexString,
                       &regexFlags,
                       String_cString(string),
                       patternType,
                       PATTERN_FLAG_NONE
                      );

  // compile pattern
  if (regcomp(&regex,String_cString(string),regexFlags) == 0)
  {
    regfree(&regex);
    isValid = TRUE;
  }
  else
  {
    isValid = FALSE;
  }

  // free resources
  String_delete(regexString);

  return isValid;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
