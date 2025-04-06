/***********************************************************************\
*
* Contents: C string definitions
* Systems: Linux
*
\***********************************************************************/

#ifndef __CSTRINGS__
#define __CSTRINGS__

#if (defined DEBUG)
 #warning DEBUG option set - no LOCAL and no -O2 (optimizer) will be used!
#endif
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdint.h>
#ifdef HAVE_STDBOOL_H
  #include <stdbool.h>
#endif
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_LIBINTL_H
  #include <libintl.h>
#endif
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// definition of some character names
#define NUL '\000'
#define CODE_POINT_NUL 0x00000000

#define STRING_NO_ASSIGN (void*)(-1)

/**************************** Datatypes ********************************/
// Unicode codepoint (4 bytes)
typedef uint32_t Codepoint;

// string tokenizer
typedef struct
{
  char       *string;
  const char *delimiters;
  const char *nextToken;
  char       *p;
} CStringTokenizer;

// string iterator
typedef struct
{
  const char *s;
  size_t     nextIndex;
  Codepoint  codepoint;
} CStringIterator;

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : CSTRING_CHAR_ITERATE
* Purpose: iterated over characters of string and execute block
* Input  : string           - string
*          iteratorVariable - iterator variable (type long)
*          variable         - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all characters in string
*          usage:
*            CStringIterator iteratorVariable;
*            Codepoint       variable;
*            CSTRING_CHAR_ITERATE(string,iteratorVariable,variable)
*            {
*              ... = variable
*            }
\***********************************************************************/

#define CSTRING_CHAR_ITERATE(string,iteratorVariable,variable) \
  for (stringIteratorInit(&iteratorVariable,string), variable = stringIteratorGet(&iteratorVariable); \
       !stringIteratorEnd(&iteratorVariable); \
       stringIteratorNext(&iteratorVariable), variable = stringIteratorGet(&iteratorVariable) \
      )

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************\
* Name   : stringClear
* Purpose: clear string
* Input  : s - string
* Output : -
* Return : string
* Notes  : string is always NUL-terminated
\***********************************************************************/

static inline char *stringClear(char *s)
{
  if (s != NULL)
  {
    (*s) = NUL;
  }

  return s;
}

/***********************************************************************\
* Name   : stringLength
* Purpose: get string length
* Input  : s - string
* Output : -
* Return : string length or 0 [bytes]
* Notes  : -
\***********************************************************************/

static inline size_t stringLength(const char *s)
{
  return (s != NULL) ? strlen(s) : 0;
}

/***********************************************************************\
* Name   : stringCompare
* Purpose: compare strings
* Input  : s1, s2 - strings (can be NULL)
* Output : -
* Return : <0/=0/>0 if s1 </=/> s2
* Notes  : -
\***********************************************************************/

static inline int stringCompare(const char *s1, const char *s2)
{
  if      ((s1 != NULL) && (s2 != NULL)) return strcmp(s1,s2);
  else if ((s1 != NULL) && (s2 == NULL)) return -1;
  else if ((s1 == NULL) && (s2 != NULL)) return  1;
  else                                   return  0;
}

/***********************************************************************\
* Name   : stringEquals
* Purpose: compare strings for equal
* Input  : s1, s2 - strings (can be NULL)
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

static inline bool stringEquals(const char *s1, const char *s2)
{
  return (s1 == s2) || ((s1 != NULL) && (s2 != NULL) && (strcmp(s1,s2) == 0));
}

/***********************************************************************\
* Name   : stringEqualsPrefix
* Purpose: compare string prefixes for equal
* Input  : s1, s2 - strings
*          n      - prefix length
* Output : -
* Return : TRUE iff prefix equals
* Notes  : -
\***********************************************************************/

static inline bool stringEqualsPrefix(const char *s1, const char *s2, size_t n)
{
  return strncmp(s1,s2,n) == 0;
}

/***********************************************************************\
* Name   : stringEqualsIgnoreCase
* Purpose: compare strings for equal and ignore case
* Input  : s1, s2 - strings
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

static inline bool stringEqualsIgnoreCase(const char *s1, const char *s2)
{
  return (s1 == s2) || ((s1 != NULL) && (s2 != NULL) && (strcasecmp(s1,s2) == 0));
}

/***********************************************************************\
* Name   : stringEqualsPrefixIgnoreCase
* Purpose: compare string prefixes for equal and ignore case
* Input  : s1, s2 - strings
*          n      - prefix length
* Output : -
* Return : TRUE iff prefix equals
* Notes  : -
\***********************************************************************/

static inline bool stringEqualsPrefixIgnoreCase(const char *s1, const char *s2, size_t n)
{
  return strncasecmp(s1,s2,n) == 0;
}

/***********************************************************************\
* Name   : stringStartsWith
* Purpose: check if string starts with prefix
* Input  : string - string
*          prefix - prefix
* Output : -
* Return : TRUE iff string start with prefix
* Notes  : -
\***********************************************************************/

static inline bool stringStartsWith(const char *string, const char *prefix)
{
  return strncmp(string,prefix,strlen(prefix)) == 0;
}

/***********************************************************************\
* Name   : stringStartsWithIgnoreCase
* Purpose: check if string starts with prefix and ignore case
* Input  : string - string
*          prefix - prefix
* Output : -
* Return : TRUE iff string start with prefix
* Notes  : -
\***********************************************************************/

static inline bool stringStartsWithIgnoreCase(const char *string, const char *prefix)
{
  return strncasecmp(string,prefix,strlen(prefix)) == 0;
}

/***********************************************************************\
* Name   : stringEndsWith
* Purpose: check if string ends with suffix
* Input  : string - string
*          suffix - suffix
* Output : -
* Return : TRUE iff string start with suffix
* Notes  : -
\***********************************************************************/

static inline bool stringEndsWith(const char *string, const char *suffix)
{
  size_t n = strlen(string);
  size_t m = strlen(suffix);

  return    (n >= m)
         && strncmp(&string[n-m],suffix,m) == 0;
}

/***********************************************************************\
* Name   : stringEndsWithIgnoreCase
* Purpose: check if string ends with suffix and ignore case
* Input  : string - string
*          suffix - suffix
* Output : -
* Return : TRUE iff string start with suffix
* Notes  : -
\***********************************************************************/

static inline bool stringEndsWithIgnoreCase(const char *string, const char *suffix)
{
  size_t n = strlen(string);
  size_t m = strlen(suffix);

  return    (n >= m)
         && strncasecmp(&string[n-m],suffix,m) == 0;
}

/***********************************************************************\
* Name   : stringIsEmpty
* Purpose: check if string is NULL or empty
* Input  : string - string
* Output : -
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

static inline bool stringIsEmpty(const char *string)
{
  return (string == NULL) || (string[0] == NUL);
}

/***********************************************************************\
* Name   : stringSet
* Purpose: set string
* Input  : string     - destination string
*          stringSize - size of string (including terminating NUL)
*          source     - source string
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringSet(char *string, size_t stringSize, const char *source)
{
  assert(stringSize > 0);

  if (string != NULL)
  {
    if (source != NULL)
    {
      /* Note: gcc 9.x generate a false positive warning "specified bound depends
               on the length of the source argument" if stringSize depend on string
               even a code like this is correct:

               n = strlen(s)+1;
               t = malloc(n);
               strncpy(t,s,n-1); t[n-1] = '\0';
      */
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wstringop-overflow"
      strncpy(string,source,stringSize-1);
      string[stringSize-1] = NUL;
      #pragma GCC diagnostic pop
    }
    else
    {
      string[0] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringSetBuffer
* Purpose: set string
* Input  : string     - destination string
*          stringSize - size of string (including terminating NUL)
*          buffer     - buffer
*          bufferSize - buffer size [bytes]
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringSetBuffer(char *string, size_t stringSize, const char *buffer, size_t bufferSize)
{
  assert(stringSize > 0);

  if (string != NULL)
  {
    if (buffer != NULL)
    {
      strncpy(string,buffer,MIN(stringSize-1,bufferSize)); string[MIN(stringSize-1,bufferSize)] = NUL;
    }
    else
    {
      string[0] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringReplace
* Purpose: replace string
* Input  : string        - destination string
*          stringSize    - size of string (including terminating NUL)
*          index         - replace start index
*          replaceLength - number of characters to replace
*          replaceString - replace string
* Output : -
* Return : modified string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

char* stringReplace(char *string, size_t stringSize, size_t index, size_t replaceLength, const char *replaceString);

/***********************************************************************\
* Name   : stringIntLength
* Purpose: get string length of integer (number of digits)
* Input  : n - integer
* Output : -
* Return : string integer length
* Notes  : -
\***********************************************************************/

static inline size_t stringIntLength(int n)
{
  return snprintf(NULL,0,"%d",n);
}

/***********************************************************************\
* Name   : stringInt64Length
* Purpose: get string length of integer (number of digits)
* Input  : n - integer
* Output : -
* Return : string integer length
* Notes  : -
\***********************************************************************/

static inline size_t stringInt64Length(int64 n)
{
  return snprintf(NULL,0,"%" PRIi64,n);
}

/***********************************************************************\
* Name   : stringVFormat, stringFormat
* Purpose: format string
* Input  : string     - string
*          stringSize - size of string (including terminating NUL)
*          format     - printf-like format string
*          ...        - optional arguments
*          arguments  - arguments
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/


static inline char* stringVFormat(char *string, size_t stringSize, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(stringSize > 0);
  assert(format != NULL);

  vsnprintf(string,stringSize,format,arguments);

  return string;
}

static inline char* stringFormat(char *string, size_t stringSize, const char *format, ...)
{
  assert(string != NULL);
  assert(stringSize > 0);
  assert(format != NULL);

  va_list arguments;
  va_start(arguments,format);
  string = stringVFormat(string,stringSize,format,arguments);
  va_end(arguments);

  return string;
}

/***********************************************************************\
* Name   : stringVFormatLength, stringFormatLength
* Purpose: get length of formated string
* Input  : format    - printf-like format string
*          ...       - optional arguments
*          arguments - arguments
* Output : -
* Return : length [bytes]
* Notes  : -
\***********************************************************************/

static inline size_t stringVFormatLength(const char *format, va_list arguments)
{
  assert(format != NULL);

  return vsnprintf(NULL,0,format,arguments);
}
static inline size_t stringFormatLength(const char *format, ...)
{
  assert(format != NULL);

  va_list arguments;
  va_start(arguments,format);
  size_t n = stringVFormatLength(format,arguments);
  va_end(arguments);

  return n;
}

/***********************************************************************\
* Name   : stringAppend
* Purpose: append string to string
* Input  : string     - destination string
*          stringSize - size of destination string (including
*                       terminating NUL)
*          source     - string to append
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringAppend(char *string, size_t stringSize, const char *source)
{
  assert(stringSize > 0);

  if ((string != NULL) && (source != NULL))
  {
    size_t n = strlen(string);
    if (stringSize > (n+1))
    {
      strncat(string,source,stringSize-(n+1));
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringAppendChar
* Purpose: append chararacter to string
* Input  : string     - destination string
*          stringSize - size of destination string (including
*                       terminating NUL)
*          ch         - character to append
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringAppendChar(char *string, size_t stringSize, char ch)
{
  assert(stringSize > 0);

  if (string != NULL)
  {
    size_t n = strlen(string);
    if (stringSize > (n+1))
    {
      string[n]   = ch;
      string[n+1] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringAppendBuffer
* Purpose: append buffer to string
* Input  : string       - destination string
*          stringSize   - size of destination string (including
*                         terminating NUL)
*          buffer       - buffer
*          bufferLength - buffer length
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringAppendBuffer(char *string, size_t stringSize, const char *buffer, size_t bufferLength)
{
  assert(stringSize > 0);

  if ((string != NULL) && (buffer != NULL))
  {
    size_t n = strlen(string);
    if (stringSize >= (n+bufferLength+1))
    {
      memcpy(&string[n],buffer,stringSize-(bufferLength+1));
      string[n+bufferLength] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringAppendVFormat, stringAppendFormat
* Purpose: format string and append to string
* Input  : string     - string
*          stringSize - size of destination string (including terminating
*                       NUL)
*          format     - printf-like format string
*          ...        - optional arguments
*          arguments  - arguments
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringAppendVFormat(char *string, size_t stringSize, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(stringSize > 0);
  assert(format != NULL);

  size_t n = strlen(string);
  if (n < stringSize)
  {
    vsnprintf(&string[n],stringSize-n,format,arguments);
  }

  return string;
}

static inline char* stringAppendFormat(char *string, size_t stringSize, const char *format, ...)
{
  va_list arguments;

  assert(string != NULL);
  assert(stringSize > 0);
  assert(format != NULL);

  va_start(arguments,format);
  stringAppendVFormat(string,stringSize,format,arguments);
  va_end(arguments);

  return string;
}

/***********************************************************************\
* Name   : stringFill
* Purpose: fill string
* Input  : string     - destination string
*          stringSize - size of destination string (including
*                       terminating NUL)
*          length     - length [characters]
*          ch         - character to fill string with
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringFill(char *string, size_t stringSize, size_t length, char ch)
{
  size_t n;

  assert(stringSize > 0);

  if (string != NULL)
  {
    n = MIN(stringSize-1,length);
    memset(string,ch,n);
    string[n] = NUL;
  }

  return string;
}

/***********************************************************************\
* Name   : stringFillAppend
* Purpose: append to string
* Input  : string     - destination string
*          stringSize - size of destination string (including
*                       terminating NUL)
*          length     - total length of string
*          ch         - character to append to string
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringFillAppend(char *string, size_t stringSize, size_t length, char ch)
{
  size_t n,m;

  assert(stringSize > 0);

  if (string != NULL)
  {
    n = strlen(string);
    if (n < stringSize)
    {
      m = MIN(stringSize-n-1,length);
      memset(&string[n],ch,m);
      string[n+m] = NUL;
    }
  }

  return string;
}


/***********************************************************************\
* Name   : stringTrimBegin
* Purpose: trim spaces at beginning of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline const char* stringTrimBegin(const char *string)
{
  while (isspace(*string))
  {
    string++;
  }

  return string;
}

/***********************************************************************\
* Name   : stringTrimEnd
* Purpose: trim spaces at end of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline char* stringTrimEnd(char *string)
{
  char *s;

  s = &string[strlen(string)-1];
  while ((s >= string) && isspace(*s))
  {
    s--;
  }
  s[1] = NUL;

  return string;
}

/***********************************************************************\
* Name   : stringTrim
* Purpose: trim spaces at beginning and end of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline char* stringTrim(char *string)
{
  char *s;

  while (isspace(*string))
  {
    string++;
  }

  s = &string[strlen(string)-1];
  while ((s >= string) && isspace(*s))
  {
    s--;
  }
  s[1] = NUL;

  return string;
}

/***********************************************************************\
* Name   : stringNew
* Purpose: allocate new (emtpy) string
* Input  : n - string size (including terminating NUL)
* Output : -
* Return : new string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringNew(size_t n)
{
  char *string;

  assert(n > 0);

  string = (char*)malloc(n*sizeof(char));
  if (string != NULL)
  {
    string[0] = NUL;
  }

  return string;
}

/***********************************************************************\
* Name   : stringNewBuffer
* Purpose: allocate new string from buffer
* Input  : buffer     - buffer (can be NULL)
*          bufferSize - buffer size
* Output : -
* Return : new string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringNewBuffer(const char *buffer, size_t bufferSize)
{
  char *string;

  string = (char*)malloc(bufferSize*sizeof(char)+1);
  if (string != NULL)
  {
    if (buffer != NULL)
    {
      strncpy(string,buffer,bufferSize); string[bufferSize] = NUL;
    }
    else
    {
      string[0] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringDuplicate
* Purpose: duplicate string
* Input  : source - source string
* Output : -
* Return : duplicate string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringDuplicate(const char *source)
{
  char *duplicate;

  if (source != NULL)
  {
    duplicate = strdup(source);
  }
  else
  {
    duplicate = NULL;
  }

  return duplicate;
}

/***********************************************************************\
* Name   : stringDelete
* Purpose: delete string
* Input  : string - string to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringDelete(char *string)
{
  if (string != NULL)
  {
    free(string);
  }
}

/***********************************************************************\
* Name   : stringAt
* Purpose: get character in string
* Input  : string    - string
*          index     - index (0..n-1)
*          nextIndex - next index variable or NULL
* Output : nextIndex - next index
* Return : character/codepoint
* Notes  : -
\***********************************************************************/

static inline char stringAt(const char *string, size_t index)
{
  assert(string != NULL);

  return string[index];
}

/***********************************************************************\
* Name   : stringIsValidUTF8CodepointN
* Purpose: check if valid UTF8 codepoint
* Input  : string    - string
*          length    - string length
*          index     - index [0..n-1]
*          nextIndex - next index variable (can be NULL)
* Output : nextIndex - next index [0..n-1]
* Return : TRUE iff valid codepoint
* Notes  : -
\***********************************************************************/

static inline bool stringIsValidUTF8CodepointN(const char *string, size_t length, size_t index, size_t *nextIndex)
{
  if ((string != NULL) && (index < length))
  {
    if      (   ((index+4) <= length)
             && ((string[index] & 0xF8) == 0xF0)
             && ((string[index+1] & 0xC0) == 0x80)
             && (string[index+2] != 0x00)
             && (string[index+3] != 0x00)
            )
    {
      // 4 byte UTF8 codepoint
      if (nextIndex != NULL) (*nextIndex) = index+4;
      return TRUE;
    }
    else if (   ((index+3) <= length)
             && ((string[index] & 0xF0) == 0xE0)
             && ((string[index+1] & 0xC0) == 0x80)
             && (string[index+2] != 0x00)
            )
    {
      // 3 byte UTF8 codepoint
      if (nextIndex != NULL) (*nextIndex) = index+3;
      return TRUE;
    }
    else if (   ((index+2) <= length)
             && ((string[index] & 0xE0) == 0xC0)
             && ((string[index+1] & 0xC0) == 0x80)
            )
    {
      // 2 byte UTF8 codepoint
      if (nextIndex != NULL) (*nextIndex) = index+2;
      return TRUE;
    }
    else if (   ((index+1) <= length)
             && ((uchar)string[index] <= 0x7F)
            )
    {
      // 1 byte UTF8 codepoint
      if (nextIndex != NULL) (*nextIndex) = index+1;
      return TRUE;
    }
    else
    {
      // invalid
      return FALSE;
    }
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringIsValidUTF8Codepoint
* Purpose: check if valid UTF8 codepoint
* Input  : string    - string
*          index     - index [0..n-1]
*          nextIndex - next index variable (can be NULL)
* Output : nextIndex - next index [0..n-1]
* Return : TRUE iff valid codepoint
* Notes  : -
\***********************************************************************/

static inline bool stringIsValidUTF8Codepoint(const char *string, size_t index, size_t *nextIndex)
{
  if (string != NULL)
  {
    return stringIsValidUTF8CodepointN(string,stringLength(string),index,nextIndex);
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringIsValidUTF8
* Purpose: check if string has valid UTF8 encoding
* Input  : s         - string
*          index     - index [0..n-1]
* Output : -
* Return : TRUE iff valid encoding
* Notes  : -
\***********************************************************************/

static inline bool stringIsValidUTF8(const char *string, size_t index)
{
  size_t nextIndex;

  if (string != NULL)
  {
    assert(index <= stringLength(string));
    while (string[index] != NUL)
    {
      if (stringIsValidUTF8Codepoint(string,index,&nextIndex))
      {
        index = nextIndex;
      }
      else
      {
        // error
        break;
      }
    }

    return string[index] == NUL;
  }
  else
  {
    return TRUE;
  }
}

/***********************************************************************\
* Name   : stringMakeValidUTF8
* Purpose: make valid UTF8 encoded string; discard non UTF8 encodings
* Input  : string - string
*          index  - start index [0..n-1] or STRING_BEGIN
* Output : string - valid UTF8 encoded string
* Return : string
* Notes  : -
\***********************************************************************/

static inline char *stringMakeValidUTF8(char *string, size_t index)
{
  size_t nextIndex;
  size_t toIndex;

  if (string != NULL)
  {
    toIndex = index;
    while (string[index] != NUL)
    {
      if (stringIsValidUTF8Codepoint(string,index,&nextIndex))
      {
        while (index < nextIndex)
        {
          string[toIndex] = string[index];
          index++;
          toIndex++;
        }
      }
      else
      {
        index++;
      }
    }
    string[toIndex] = NUL;
  }

  return string;
}

/***********************************************************************\
* Name   : stringNextUTF8N
* Purpose: get next UTF8 character index
* Input  : string - string
*          length - string length
*          index  - index (0..n-1)
* Output : -
* Return : next index
* Notes  : -
\***********************************************************************/

static inline size_t stringNextUTF8N(const char *string, size_t length, size_t index)
{
  assert(string != NULL);
  assert(index <= length);

  if      (((index+4) <= length) && ((string[index] & 0xF8) == 0xF0))
  {
    // 4 byte UTF8 codepoint
    index += 4;
  }
  else if (((index+3) <= length) && ((string[index] & 0xF0) == 0xE0))
  {
    // 3 byte UTF8 codepoint
    index += 3;
  }
  else if (((index+2) <= length) && ((string[index] & 0xE0) == 0xC0))
  {
    // 2 byte UTF8 codepoint
    index += 2;
  }
  else if ((index+1) <= length)
  {
    // 1 byte UTF8 codepoint
    index += 1;
  }

  return index;
}

/***********************************************************************\
* Name   : stringNextUTF8
* Purpose: get next UTF8 character index
* Input  : string - string
*          index  - index (0..n-1)
* Output : -
* Return : next index
* Notes  : -
\***********************************************************************/

static inline size_t stringNextUTF8(const char *string, size_t index)
{
  assert(string != NULL);

  return stringNextUTF8N(string,stringLength(string),index);
}

/***********************************************************************\
* Name   : charUTF8Length
* Purpose: get length of UTF8 character from codepoint
* Input  : codepoint - codepoint
* Output : -
* Return : length of UTF8 character [bytes]
* Notes  : -
\***********************************************************************/

static inline size_t charUTF8Length(Codepoint codepoint)
{
  size_t length;

  if      ((codepoint & 0xFFFFFF80) == 0)
  {
    // 7bit ASCII -> 1 byte
    length = 1;
  }
  else if ((codepoint & 0xFFFFF800) == 0)
  {
    // 11bit UTF8 codepoint -> 2 byte
    length = 2;
  }
  else if ((codepoint & 0xFFFF0000) == 0)
  {
    // 16bit UTF8 codepoint -> 3 byte
    length = 3;
  }
  else // ((codepoint & 0xFFE00000) == 0)
  {
    // 21bit UTF8 codepoint -> 4 byte
    length = 4;
  }

  return length;
}

/***********************************************************************\
* Name   : isCharUTF8
* Purpose: check if UTF8 character
* Input  : codepoint - codepoint
* Output : -
* Return : TRUE iff UTF8 character
* Notes  : -
\***********************************************************************/

static inline bool isCharUTF8(Codepoint codepoint)
{
  return codepoint >= 128;
}

/***********************************************************************\
* Name   : charUTF8
* Purpose: convert codepoint to UTF8 character as string
* Input  : codepoint - codepoint
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

static inline const char *charUTF8(Codepoint codepoint)
{
  static char s[4+1];

  if      ((codepoint & 0xFFFFFF80) == 0)
  {
    // 7bit ASCII; 0b1xxxxxxx
    s[0] = (char)(codepoint & 0x0000007F);
    s[1] = NUL;
  }
  else if ((codepoint & 0xFFFFF800) == 0)
  {
    // 11bit UTF8 codepoint: 0b110xxxxx 0b10xxxxxx
    s[0] = 0xC0 | (char)((codepoint & 0x000007C0) >> 6);
    s[1] = 0x80 | (char)((codepoint & 0x0000003F) >> 0);
    s[2] = NUL;
  }
  else if ((codepoint & 0xFFFF0000) == 0)
  {
    // 16bit UTF8 codepoint: 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
    s[0] = 0xE0 | (char)((codepoint & 0x0000F000) >> 12);
    s[1] = 0x80 | (char)((codepoint & 0x00000FC0) >>  6);
    s[2] = 0x80 | (char)((codepoint & 0x0000003F) >>  0);
    s[3] = NUL;
  }
  else // ((codepoint & 0xFFE00000) == 0)
  {
    // 21bit UTF8 codepoint: 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
    s[0] = 0xF0 | (char)((codepoint & 0x001C0000) >> 18);
    s[1] = 0x80 | (char)((codepoint & 0x0003F000) >> 12);
    s[2] = 0x80 | (char)((codepoint & 0x00000FC0) >>  6);
    s[3] = 0x80 | (char)((codepoint & 0x0000003F) >>  0);
    s[4] = NUL;
  }

  return s;
}

/***********************************************************************\
* Name   : stringLengthCodepointsUTF8
* Purpose: get number of UTF8 codepoints in string
* Input  : string - string
* Output : -
* Return : number of codepoints
* Notes  : -
\***********************************************************************/

static inline size_t stringLengthCodepointsUTF8(const char *string)
{
  size_t n;
  size_t index;

  n = 0;
  if (string != NULL)
  {
    index = 0;
    while (string[index] != NUL)
    {
      n++;
      index = stringNextUTF8(string,index);
    }
  }

  return n;
}

/***********************************************************************\
* Name   : stringAtUTF8N
* Purpose: get codepoint
* Input  : string    - string
*          length    - string length
*          index     - index (0..n-1)
*          nextIndex - next index variable (can be NULL)
* Output : nextIndex - next index [0..n-1]
* Return : codepoint
* Notes  : -
\***********************************************************************/

static inline Codepoint stringAtUTF8N(const char *string, size_t length, size_t index, size_t *nextIndex)
{
  Codepoint codepoint;

  assert(string != NULL);
  assert(index <= length);

  /* Note: gcc 11.x generate a false positive warning "array out of depends"
           even the if-statement checks the bounds?
   */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  if      (((index+4) <= length) && ((string[index] & 0xF8) == 0xF0))
  {
    // 4 byte UTF8 codepoint
    codepoint =   (Codepoint)((string[index+0] & 0x07) << 18)
                | (Codepoint)((string[index+1] & 0x3F) << 12)
                | (Codepoint)((string[index+2] & 0x3F) <<  6)
                | (Codepoint)((string[index+3] & 0x3F) <<  0);
    if (nextIndex != NULL) (*nextIndex) = index+4;
  }
  else if (((index+3) <= length) && ((string[index] & 0xF0) == 0xE0))
  {
    // 3 byte UTF8 codepoint
    codepoint =   (Codepoint)((string[index+0] & 0x0F) << 12)
                | (Codepoint)((string[index+1] & 0x3F) <<  6)
                | (Codepoint)((string[index+2] & 0x3F) <<  0);
    if (nextIndex != NULL) (*nextIndex) = index+3;
  }
  else if (((index+2) <= length) && ((string[index] & 0xE0) == 0xC0))
  {
    // 2 byte UTF8 codepoint
    codepoint =   (Codepoint)((string[index+0] & 0x1F) << 6)
                | (Codepoint)((string[index+1] & 0x3F) << 0);
    if (nextIndex != NULL) (*nextIndex) = index+2;
  }
  else if ((index+1) <= length)
  {
    // 1 byte UTF8 codepoint
    codepoint = (Codepoint)string[index];
    if (nextIndex != NULL) (*nextIndex) = index+1;
  }
  else
  {
    codepoint = CODE_POINT_NUL;
  }
#pragma GCC diagnostic pop

  return codepoint;
}

/***********************************************************************\
* Name   : stringAtUTF8
* Purpose: get codepoint
* Input  : s         - string
*          index     - index (0..n-1)
*          nextIndex - next index variable (can be NULL)
* Output : nextIndex - next index [0..n-1]
* Return : codepoint
* Notes  : -
\***********************************************************************/

static inline Codepoint stringAtUTF8(const char *string, size_t index, size_t *nextIndex)
{
  assert(string != NULL);

  return stringAtUTF8N(string,stringLength(string),index,nextIndex);
}

/***********************************************************************\
* Name   : stringVFormatLengthCodepointsUTF8, stringFormatLengthCodepointsUTF8
* Purpose: get number of codepoints of formated UTF8 string
* Input  : format    - format string
*          ...       - optional arguments
*          arguments - arguments
* Output : -
* Return : length [codepoints]
* Notes  : -
\***********************************************************************/

static inline size_t stringVFormatLengthCodepointsUTF8(const char *format, va_list arguments)
{
  size_t n;
  char   *s;

  if ((format != NULL) && (vasprintf(&s,format,arguments) != -1))
  {
    n = stringLengthCodepointsUTF8(s);
    free(s);
  }
  else
  {
    n = 0;
  }

  return n;
}
static inline size_t stringFormatLengthCodepointsUTF8(const char *format, ...)
{
  va_list arguments;
  size_t  n;

  if (format != NULL)
  {
    va_start(arguments,format);
    n = stringVFormatLengthCodepointsUTF8(format,arguments);
    va_end(arguments);
  }
  else
  {
    n = 0;
  }

  return n;
}

/***********************************************************************\
* Name   : stringFind, stringFindChar stringFindReverseChar
* Purpose: find string/character in string
* Input  : string              - string
*          findString,findChar - string/character to find
* Output : -
* Return : index or -1
* Notes  : -
\***********************************************************************/

static inline long stringFind(const char *string, const char *findString)
{
  const char *t;

  assert(string != NULL);
  assert(findString != NULL);

  t = strstr(string,findString);
  return (t != NULL) ? (long)(t-string) : -1L;
}

static inline long stringFindChar(const char *string, char findChar)
{
  const char *t;

  assert(string != NULL);

  t = strchr(string,findChar);
  return (t != NULL) ? (long)(t-string) : -1L;
}

static inline long stringFindReverseChar(const char *string, char findChar)
{
  const char *t;

  assert(string != NULL);

  t = strrchr(string,findChar);
  return (t != NULL) ? (long)(t-string) : -1L;
}

/***********************************************************************\
* Name   : stringSub
* Purpose: get sub-string
* Input  : string     - destination string
*          stringSize - size of destination string
*          source     - source string
*          index      - sub-string start index
*          length     - sub-string length or -1
* Output : -
* Return : string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringSub(char *string, size_t stringSize, const char *source, size_t index, long length)
{
  long n;

  assert(stringSize > 0);

  if (string != NULL)
  {
    if (source != NULL)
    {
      n = (length >= 0)
            ? MIN((long)stringSize-1,length)
            : MIN((long)stringSize-1,(long)strlen(source)-(long)index);
      if (n < 0) n = 0;
      strncpy(string,source+index,n); string[n] = NUL;
    }
  }

  return string;
}

/***********************************************************************\
* Name   : stringIteratorInit
* Purpose: init string iterator
* Input  : cStringIterator - string iterator variable
*          string          - string
* Output : stringIterator - string iterator
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringIteratorInit(CStringIterator *cStringIterator, const char *s)
{
  assert(cStringIterator != NULL);

  cStringIterator->s         = s;
  cStringIterator->nextIndex = 0;

  if (cStringIterator->s[cStringIterator->nextIndex] != NUL)
  {
    cStringIterator->codepoint = stringAtUTF8(cStringIterator->s,0,&cStringIterator->nextIndex);
  }
  else
  {
    cStringIterator->nextIndex = 0;
    cStringIterator->codepoint = CODE_POINT_NUL;
  }
}

/***********************************************************************\
* Name   : stringIteratorDone
* Purpose: done string iterator
* Input  : cStringIterator - string iterator
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringIteratorDone(CStringIterator *cStringIterator)
{
  assert(cStringIterator != NULL);

  UNUSED_VARIABLE(cStringIterator);
}

/***********************************************************************\
* Name   : stringIteratorGet
* Purpose: get character (codepoint) from string iterator
* Input  : cStringIterator - string iterator
* Output : -
* Return : character
* Notes  : -
\***********************************************************************/

static inline Codepoint stringIteratorGet(CStringIterator *cStringIterator)
{
  assert(cStringIterator != NULL);

  return cStringIterator->codepoint;
}

/***********************************************************************\
* Name   : stringIteratorAtX
* Purpose: get character (codepoint) from string iterator
* Input  : cStringIterator - string iterator
*          index           - index (0..n-1)
* Output : -
* Return : character
* Notes  : -
\***********************************************************************/

static inline Codepoint stringIteratorAtX(CStringIterator *cStringIterator, size_t index)
{
  Codepoint codepoint;
  size_t     nextIndex;

  assert(cStringIterator != NULL);

  codepoint = cStringIterator->codepoint;
  nextIndex = cStringIterator->nextIndex;
  while ((index > 0) && (cStringIterator->s[nextIndex] != NUL))
  {
    codepoint = stringAtUTF8(cStringIterator->s,nextIndex,&nextIndex);
    index--;
  }

  return codepoint;
}

/***********************************************************************\
* Name   : stringIteratorEnd
* Purpose: check if string iterator end
* Input  : cStringIterator - string iterator
* Output : -
* Return : TRUE iff string iterator end (no more characters)
* Notes  : -
\***********************************************************************/

static inline bool stringIteratorEnd(const CStringIterator *cStringIterator)
{
  assert(cStringIterator != NULL);

  return cStringIterator->codepoint == CODE_POINT_NUL;
}

/***********************************************************************\
* Name   : stringIteratorNext
* Purpose: increment string iterator
* Input  : cStringIterator - string iterator
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringIteratorNext(CStringIterator *cStringIterator)
{
  assert(cStringIterator != NULL);

  if (cStringIterator->codepoint != CODE_POINT_NUL)
  {
    cStringIterator->codepoint = stringAtUTF8(cStringIterator->s,cStringIterator->nextIndex,&cStringIterator->nextIndex);
  }
}

/***********************************************************************\
* Name   : stringIteratorNextX
* Purpose: increment string iterator
* Input  : cStringIterator - string iterator
*          n               - number of chracters to increment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringIteratorNextX(CStringIterator *cStringIterator, size_t n)
{
  assert(cStringIterator != NULL);

  while ((n > 0) && (cStringIterator->codepoint != CODE_POINT_NUL))
  {
    cStringIterator->codepoint = stringAtUTF8(cStringIterator->s,cStringIterator->nextIndex,&cStringIterator->nextIndex);
    n--;
  }
}

/***********************************************************************\
* Name   : stringIteratorGetNext
* Purpose: get next character (codepoint) from string iterator
* Input  : cStringIterator - string iterator
* Output : cStringIterator - incremented string iterator
* Return : character
* Notes  : -
\***********************************************************************/

static inline Codepoint stringIteratorGetNext(CStringIterator *cStringIterator)
{
  Codepoint codepoint;

  assert(cStringIterator != NULL);

  codepoint = cStringIterator->codepoint;
  stringIteratorNext(cStringIterator);

  return codepoint;
}

/***********************************************************************\
* Name   : stringTokenizerInit
* Purpose: init string tokenizer
* Input  : cStringTokenizer - string tokenizer
*          s                - string
*          delimiters       - token delimiters
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringTokenizerInit(CStringTokenizer *cStringTokenizer, const char *string, const char *delimiters)
{
  assert(cStringTokenizer != NULL);
  assert(string != NULL);

  cStringTokenizer->string     = strdup(string);
  cStringTokenizer->nextToken  = strtok_r(cStringTokenizer->string,delimiters,&cStringTokenizer->p);
  cStringTokenizer->delimiters = delimiters;
}

/***********************************************************************\
* Name   : stringTokenizerDone
* Purpose: done string tokenizer
* Input  : cStringTokenizer - string tokenizer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void stringTokenizerDone(CStringTokenizer *cStringTokenizer)
{
  assert(cStringTokenizer != NULL);

  free(cStringTokenizer->string);
}

/***********************************************************************\
* Name   : stringGetNextToken
* Purpose: get next string token
* Input  : cStringTokenizer - string tokenizer
* Output : token - next token
* Return : TRUE iff next token
* Notes  : -
\***********************************************************************/

static inline bool stringGetNextToken(CStringTokenizer *cStringTokenizer, const char **token)
{
  assert(cStringTokenizer != NULL);
  assert(token != NULL);

  (*token) = cStringTokenizer->nextToken;
  if (cStringTokenizer->nextToken != NULL)
  {
    cStringTokenizer->nextToken = strtok_r(NULL,cStringTokenizer->delimiters,&cStringTokenizer->p);
  }

  return (*token) != NULL;
}

/***********************************************************************\
* Name   : stringToBool
* Purpose: convert string to bool-value
* Input  : string - string (1/true/on/no + 0/false/off/no)
*          i      - value variable
* Output : i - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToBool(const char *string, bool *b)
{
  assert(string != NULL);
  assert(b != NULL);

  if      (   stringEquals(string,"1")
           || stringEqualsIgnoreCase(string,"true")
           || stringEqualsIgnoreCase(string,"on")
           || stringEqualsIgnoreCase(string,"yes")
          )
  {
    (*b) = TRUE;
    return TRUE;
  }
  else if (   stringEquals(string,"0")
           || stringEqualsIgnoreCase(string,"false")
           || stringEqualsIgnoreCase(string,"off")
           || stringEqualsIgnoreCase(string,"no")
          )
  {
    (*b) = FALSE;
    return TRUE;
  }
  else
  {
    (*b) = FALSE;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToInt
* Purpose: convert string to int-value
* Input  : string - string
*          i      - value variable
*          tail   - tail variable (can be NULL)
* Output : i    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToInt(const char *string, int *i, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*i) = (int)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToUInt
* Purpose: convert string to uint-value
* Input  : string - string
*          i      - value variable
*          tail   - tail variable (can be NULL)
* Output : i    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToUInt(const char *string, uint *i, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*i) = (uint)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToLong
* Purpose: convert string to long-value
* Input  : string - string
*          i      - value variable
*          tail   - tail variable (can be NULL)
* Output : i    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToLong(const char *string, long *i, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*i) = (long)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToULong
* Purpose: convert string to ulong-value
* Input  : string - string
*          i      - value variable
*          tail   - tail variable (can be NULL)
* Output : i    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToULong(const char *string, ulong *i, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*i) = (ulong)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToInt64
* Purpose: convert string to int64-value
* Input  : string - string
*          l      - value variable
*          tail   - tail variable (can be NULL)
* Output : l    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToInt64(const char *string, int64 *l, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(l != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*l) = (int64)n;
    return TRUE;
  }
  else
  {
    (*l) = 0LL;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToUInt64
* Purpose: convert string to uint64-value
* Input  : string - string
*          l      - value variable
*          tail   - tail variable (can be NULL)
* Output : l    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToUInt64(const char *string, uint64 *l, const char **tail)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(l != NULL);

  n = strtoll(string,&s,0);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*l) = (uint64)n;
    return TRUE;
  }
  else
  {
    (*l) = 0LL;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToDouble
* Purpose: convert string to double value
* Input  : string - string
*          d      - value variable
*          tail   - tail variable (can be NULL)
* Output : d    - value
*          tail - not parsed tail part of string
* Return : TRUE iff no error
* Notes  : -
\***********************************************************************/

static inline bool stringToDouble(const char *string, double *d, const char **tail)
{
  double n;
  char   *s;

  assert(string != NULL);
  assert(d != NULL);

  n = strtod(string,&s);
  if (tail != NULL) (*tail) = s;
  if (((*s) == NUL) || (tail != NULL))
  {
    (*d) = n;
    return TRUE;
  }
  else
  {
    (*d) = 0.0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringVScan, stringScan
* Purpose: scan string
* Input  : string    - string
*          format    - format
*          arguments - arguments
*          ...       - optional variables, last value have to be NULL!
* Output : -
* Return : TRUE iff string scanned with format
* Notes  : -
\***********************************************************************/

bool stringVScan(const char *string, const char *format, va_list arguments);
static inline int stringScan(const char *string, const char *format, ...)
{
  va_list arguments;
  bool    result;

  assert(string != NULL);
  assert(format != NULL);

  va_start(arguments,format);
  result = stringVScan(string,format,arguments);
  va_end(arguments);

  return result;
}

/***********************************************************************\
* Name   : stringVMatch, stringMatch
* Purpose: match string
* Input  : string            - string
*          pattern           - pattern
*          matchedString     - string matching regular expression (can be
*                              NULL)
*          matchedStringSize - size of string matching regular expression
*          arguments         - arguments
*          ...               - optional matching strings of sub-patterns
*                              (const char**,size_t*), last value have
*                              to be NULL!
* Output : -
* Return : TRUE iff pattern match with string
* Notes  : sub-match strings are _not_ copied!
\***********************************************************************/

bool stringVMatch(const char *string, const char *pattern, const char **matchedString, size_t *matchedStringSize, va_list arguments);
static inline bool stringMatch(const char *string, const char *pattern, const char **matchedString, size_t *matchedStringSize, ...)
{
  va_list arguments;
  bool    result;

  assert(string != NULL);
  assert(pattern != NULL);

  va_start(arguments,matchedStringSize);
  result = stringVMatch(string,pattern,matchedString,matchedStringSize,arguments);
  va_end(arguments);

  return result;
}

/***********************************************************************\
* Name   : stringSimpleHash
* Purpose: calculate 32bit simple hash from string
* Input  : string - string
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

uint32 stringSimpleHash(const char *string);

#ifdef __cplusplus
}
#endif

#endif /* __CSTRINGS__ */

/* end of file */
