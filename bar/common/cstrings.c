/***********************************************************************\
*
* Contents: C string definitions
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
  assert(string != NULL);
  assert(format != NULL);

  // count variables
  va_list arguments;
  va_copy(arguments,variables);
  int variableCount = 0;
  const void *variable;
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
*          matchedSubStrings - matched sub-string variables (char*,size_t);
*                              last value have to be NULL
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
  assert(string != NULL);
  assert(pattern != NULL);

  bool matchFlag;

  #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
    // compile pattern
    regex_t regex;
    if (regcomp(&regex,pattern,REG_ICASE|REG_EXTENDED) != 0)
    {
      return FALSE;
    }

    // count sub-patterns (=1 for total matched string + number of matched-sub-strings)
    va_list    arguments;
    va_copy(arguments,matchedSubStrings);
    uint       subMatchCount = 1;
    const char **matchedSubString;
    do
    {
      matchedSubString = va_arg(arguments,const char**);
      if (matchedSubString != NULL)
      {
        size_t *matchedSubStringSize = va_arg(arguments,size_t*);
        assert(matchedSubStringSize != NULL);
        UNUSED_VARIABLE(matchedSubStringSize);
        subMatchCount++;
      }
    }
    while (matchedSubString != NULL);
    va_end(arguments);

    // allocate sub-patterns array
    regmatch_t *subMatches = (regmatch_t*)malloc(subMatchCount*sizeof(regmatch_t));
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
      for (uint i = 1; i < subMatchCount; i++)
      {
        const char **matchedSubString    = va_arg(arguments,const char**);
        size_t     *matchedSubStringSize = va_arg(arguments,size_t*);
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

char* stringReplace(char *string, size_t stringSize, size_t index, size_t replaceLength, const char *replaceString)
{
  assert(stringSize > 0);

  if ((string != NULL) && (replaceLength > 0))
  {
    size_t n = strlen(string);
    if (index < n)
    {
      if (replaceString != NULL)
      {
        // replace sub-string
        size_t insertLength = strlen(replaceString);
        if (insertLength > (stringSize - index - 1)) { insertLength = stringSize - index - 1; }
        if ((index + replaceLength) > n) n = replaceLength - index;
        if ((index + replaceLength) < n)
        {
          // some characters after replacement index have to be moved
          size_t m = MIN(stringSize - (index + insertLength),
                         stringSize - (index + replaceLength)
                        );
          memmove(&string[index + insertLength],
                  &string[index + replaceLength],
                  MIN(m,
                      stringSize - (index + insertLength - replaceLength) - 1
                     )
                 );
        }
        memcpy(&string[index], replaceString, insertLength);
        string[MIN(n + insertLength - replaceLength, stringSize - 1)] = NUL;
      }
      else
      {
        // just remove sub-string
        if ((index+replaceLength) < n)
        {
          memmove(&string[index],&string[index+replaceLength],n-(index+replaceLength));
        }
        string[n-replaceLength] = NUL;
      }
    }
  }

  return string;
}

bool stringVScan(const char *string, const char *format, va_list arguments)
{
  assert(format != NULL);

  bool scannedFlag = FALSE;

  if (string != NULL)
  {
    scannedFlag = vscanString(string,format,arguments);
  }

  return scannedFlag;
}

bool stringVMatch(const char *string, const char *pattern, const char **matchedString, size_t *matchedStringSize, va_list arguments)
{
  assert(pattern != NULL);

  bool matchFlag = FALSE;

  if (string != NULL)
  {
    matchFlag = vmatchString(string,pattern,matchedString,matchedStringSize,arguments);
  }

  return matchFlag;
}

uint32 stringSimpleHash(const char *string)
{

  uint32          hash;
  initSimpleHash(&hash);
  if (string != NULL)
  {
    CStringIterator cstringIterator;
    char            ch;
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
