/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver pattern functions
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

#include "global.h"
#include "strings.h"

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
* Name   : toRegularExpression
* Purpose: convert pattern to regular expression
* Input  : pattern      - pattern to compile
*          patternType  - pattern type
*          patternFlags - pattern flags
* Output : regexString - match string
*          regexFlags  - regular expression flags
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getRegularExpression(String       regexString,
                                int          *regexFlags,
                                const char   *pattern,
                                PatternTypes patternType,
                                uint         patternFlags
                               )
{
  ulong i;

  assert(regexString != NULL);
  assert(regexFlags != NULL);
  assert(pattern != NULL);

  (*regexFlags) = REG_NOSUB;
  if ((patternFlags & PATTERN_FLAG_IGNORE_CASE) == PATTERN_FLAG_IGNORE_CASE) (*regexFlags) |= REG_ICASE;
  switch (patternType)
  {
    case PATTERN_TYPE_GLOB:
      i = 0;
      while (pattern[i] != '\0')
      {
        switch (pattern[i])
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
            String_appendChar(regexString,pattern[i]);
            i++;
            break;
          default:
            String_appendChar(regexString,pattern[i]);
            i++;
            break;
        }
      }
      break;
    case PATTERN_TYPE_REGEX:
      String_setCString(regexString,pattern);
      break;
    case PATTERN_TYPE_EXTENDED_REGEX:
      String_setCString(regexString,pattern);
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
  String string;
  int    error;
  char   buffer[256];

  assert(regexString != NULL);
  assert(regexBegin != NULL);
  assert(regexEnd != NULL);
  assert(regexExact != NULL);
  assert(regexAny != NULL);

  // init variables
  string = String_new();

  // compile regular expression
  String_set(string,regexString);
  if (String_index(string,STRING_BEGIN) != '^') String_insertChar(string,STRING_BEGIN,'^');
  error = regcomp(regexBegin,String_cString(string),regexFlags);
  if (error != 0)
  {
    regerror(error,regexBegin,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = '\0';
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,buffer);
  }

  String_set(string,regexString);
  if (String_index(string,STRING_END) != '$') String_insertChar(string,STRING_BEGIN,'$');
  if (regcomp(regexEnd,String_cString(string),regexFlags) != 0)
  {
    regerror(error,regexEnd,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = '\0';
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,buffer);
  }

  String_set(string,regexString);
  if (String_index(string,STRING_BEGIN) != '^') String_insertChar(string,STRING_BEGIN,'^');
  if (String_index(string,STRING_END) != '$') String_insertChar(string,STRING_END,'$');
  if (regcomp(regexExact,String_cString(string),regexFlags) != 0)
  {
    regerror(error,regexExact,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = '\0';
    regfree(regexEnd);
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,buffer);
  }

  String_set(string,regexString);
  if (regcomp(regexAny,String_cString(string),regexFlags) != 0)
  {
    regerror(error,regexAny,buffer,sizeof(buffer)-1); buffer[sizeof(buffer)-1] = '\0';
    regfree(regexExact);
    regfree(regexEnd);
    regfree(regexBegin);
    String_delete(string);
    return ERRORX_(INVALID_PATTERN,0,buffer);
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
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(PATTERN_TYPES))
         && (PATTERN_TYPES[z].patternType != patternType)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(PATTERN_TYPES))
  {
    name = PATTERN_TYPES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Pattern_parsePatternType(const char *name, PatternTypes *patternType)
{
  uint z;

  assert(name != NULL);
  assert(patternType != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(PATTERN_TYPES))
         && !stringEqualsIgnoreCase(PATTERN_TYPES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(PATTERN_TYPES))
  {
    (*patternType) = PATTERN_TYPES[z].patternType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Pattern_init(Pattern *pattern, ConstString string, PatternTypes patternType, uint patternFlags)
{
  return Pattern_initCString(pattern,String_cString(string),patternType,patternFlags);
}

Errors Pattern_initCString(Pattern *pattern, const char *string, PatternTypes patternType, uint patternFlags)
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
    return error;
  }

  return ERROR_NONE;
}

void Pattern_done(Pattern *pattern)
{
  assert(pattern != NULL);

  regfree(&pattern->regexAny);
  regfree(&pattern->regexExact);
  regfree(&pattern->regexEnd);
  regfree(&pattern->regexBegin);
  String_delete(pattern->regexString);
}

Pattern *Pattern_new(ConstString string, PatternTypes patternType, uint patternFlags)
{
  Pattern *pattern;
  Errors  error;

  assert(string != NULL);

  pattern = (Pattern*)malloc(sizeof(Pattern));
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

  Pattern_done(pattern);
  free(pattern);
}

Pattern *Pattern_duplicate(const Pattern *fromPattern)
{
  Pattern *pattern;

  assert(fromPattern != NULL);

  // allocate pattern
  pattern = (Pattern*)malloc(sizeof(Pattern));
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

  return ERROR_NONE;
}

bool Pattern_match(const Pattern     *pattern,
                   ConstString       string,
                   PatternMatchModes patternMatchMode
                  )
{
  bool matchFlag;

  assert(pattern != NULL);
  assert(string != NULL);

  matchFlag = FALSE;
  switch (patternMatchMode)
  {
    case PATTERN_MATCH_MODE_BEGIN:
      matchFlag = (regexec(&pattern->regexBegin,String_cString(string),0,NULL,0) == 0);
      break;
    case PATTERN_MATCH_MODE_END:
      matchFlag = (regexec(&pattern->regexEnd,String_cString(string),0,NULL,0) == 0);
      break;
    case PATTERN_MATCH_MODE_EXACT:
      matchFlag = (regexec(&pattern->regexExact,String_cString(string),0,NULL,0) == 0);
      break;
    case PATTERN_MATCH_MODE_ANY:
      matchFlag = (regexec(&pattern->regexAny,String_cString(string),0,NULL,0) == 0);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return matchFlag;
}

bool Pattern_checkIsPattern(const ConstString string)
{
  const char *PATTERNS_CHARS = "*?[{";

  ulong i;
  bool  patternFlag;

  assert(string != NULL);

  i = 0L;
  patternFlag = FALSE;
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

#ifdef __cplusplus
  }
#endif

/* end of file */
