/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: global definitions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "global.h"

#include "cstrings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************\
* Name   : vscanString
* Purpose: scan string with arguments
* Input  : string    - string to patch
*          format    - format string; like for scanf()
*          variables - variables
* Output : -
* Return : TRUE if string scanned, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool vscanString(const char *string,
                       const char *format,
                       va_list    variables
                      )
{
  int        variableCount;
  va_list    arguments;
  const void *variable;

  assert(string != NULL);
  assert(format != NULL);

  // count variables
  va_copy(arguments,variables);
  variableCount = 0;
  do
  {
    variable = va_arg(arguments,void*);
    if (variable != NULL) variableCount++;
  }
  while (variable != NULL);
  va_end(arguments);

  // scan
  return vsscanf(string,format,variables) == variableCount;
}

/***********************************************************************\
* Name   : vmatchString
* Purpose: match string with arguments
* Input  : string            - string to patch
*          pattern           - regualar expression pattern
*          matchedString     - matched string variable (can be NULL)
*          matchedStringSize - size of matched string
*          matchedSubStrings - matched sub-string variables (char*,ulong)
* Output : nextIndex         - index of next not matched character
*          matchedString     - matched string
*          matchedSubStrings - matched sub-strings
* Return : TRUE if string matched, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool vmatchString(const char *string,
                        const char *pattern,
                        const char **matchedString,
                        size_t     *matchedStringSize,
                        va_list    matchedSubStrings
                       )
{
  bool       matchFlag;
  #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
    regex_t    regex;
    va_list    arguments;
    const char **matchedSubString;
    size_t     *matchedSubStringSize;
    regmatch_t *subMatches;
    uint       subMatchCount;
    uint       i;
  #endif /* HAVE_PCRE || HAVE_REGEX_H */

  assert(string != NULL);
  assert(pattern != NULL);

  #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
    // compile pattern
    if (regcomp(&regex,pattern,REG_ICASE|REG_EXTENDED) != 0)
    {
      return FALSE;
    }

    // count sub-patterns (=1 for total matched string + number of matched-sub-strings)
    va_copy(arguments,matchedSubStrings);
    subMatchCount = 1;
    do
    {
      matchedSubString = va_arg(arguments,const char**);
      if (matchedSubString != NULL)
      {
        matchedSubStringSize = va_arg(arguments,size_t*);
        assert((matchedSubString != NULL) && (matchedSubStringSize != NULL));
        subMatchCount++;
      }
    }
    while (matchedSubString != NULL);
    va_end(arguments);

    // allocate sub-patterns array
    subMatches = (regmatch_t*)malloc(subMatchCount*sizeof(regmatch_t));
    if (subMatches == NULL)
    {
      regfree(&regex);
      return FALSE;
    }

    // match
    matchFlag = (regexec(&regex,
                         string,
                         subMatchCount,
                         subMatches,
                         0  // eflags
                        ) == 0
                );

    // get next index, sub-matches
    if (matchFlag)
    {
      if ((matchedString     != STRING_NO_ASSIGN) && (matchedString     != NULL)) (*matchedString    ) = &string[subMatches[0].rm_so];
      if ((matchedStringSize != STRING_NO_ASSIGN) && (matchedStringSize != NULL)) (*matchedStringSize) = subMatches[0].rm_eo-subMatches[0].rm_so;

      va_copy(arguments,matchedSubStrings);
      for (i = 1; i < subMatchCount; i++)
      {
        matchedSubString     = va_arg(arguments,const char**);
        matchedSubStringSize = va_arg(arguments,size_t*);
        if (subMatches[i].rm_so != -1)
        {
          assert(subMatches[i].rm_eo >= subMatches[i].rm_so);
          if (matchedSubString     != STRING_NO_ASSIGN) (*matchedSubString    ) = &string[subMatches[i].rm_so];
          if (matchedSubStringSize != STRING_NO_ASSIGN) (*matchedSubStringSize) = subMatches[i].rm_eo-subMatches[i].rm_so;
        }
      }
      va_end(arguments);
    }

    // free resources
    free(subMatches);
    regfree(&regex);
  #else /* not HAVE_PCRE || HAVE_REGEX_H */
    UNUSED_VARIABLE(string);
    UNUSED_VARIABLE(pattern);
    UNUSED_VARIABLE(matchedString);
    UNUSED_VARIABLE(matchedStringSize);
    UNUSED_VARIABLE(matchedSubStrings);

    matchFlag = FALSE;
  #endif /* HAVE_PCRE || HAVE_REGEX_H */

  return matchFlag;
}

// ----------------------------------------------------------------------

bool stringVScan(const char *string, const char *format, va_list arguments)
{
  bool scannedFlag;

  assert(format != NULL);

  scannedFlag = FALSE;

  if (string != NULL)
  {
    scannedFlag = vscanString(string,format,arguments);
  }

  return scannedFlag;
}

bool stringVMatch(const char *string, const char *pattern, const char **matchedString, size_t *matchedStringSize, va_list arguments)
{
  bool matchFlag;

  assert(pattern != NULL);

  matchFlag = FALSE;

  if (string != NULL)
  {
    matchFlag = vmatchString(string,pattern,matchedString,matchedStringSize,arguments);
  }

  return matchFlag;
}

uint32 stringSimpleHash(const char *string)
{
  uint32          hash;
  CStringIterator cstringIterator;
  char            ch;

  initSimpleHash(&hash);
  if (string != NULL)
  {
    CSTRING_CHAR_ITERATE(string,cstringIterator,ch)
    {
      updateSimpleHash(&hash,ch);
    }
  }

  return doneSimpleHash(hash);
}

#ifdef __cplusplus
}
#endif

/* end of file */
