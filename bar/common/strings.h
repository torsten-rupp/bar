/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic string functions
* Systems: all
*
\***********************************************************************/

#ifndef __STRINGS__
#define __STRINGS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <wctype.h>
#include <assert.h>

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define STRING_BEGIN 0L
#define STRING_END   MAX_LONG

#define STRING_WHITE_SPACES " \t\f\v\n\r"
#define STRING_QUOTE        '\''
#define STRING_QUOTES       "\"'"

// string escape character
#define STRING_ESCAPE_CHARACTER  '\\'
#define STRING_ESCAPE_CHARACTERS "\\"

// default character mapping with escape
#define STRING_ESCAPE_CHARACTER_MAP_LENGTH 9
extern const char STRING_ESCAPE_CHARACTERS_MAP_FROM[STRING_ESCAPE_CHARACTER_MAP_LENGTH];
extern const char STRING_ESCAPE_CHARACTERS_MAP_TO[STRING_ESCAPE_CHARACTER_MAP_LENGTH];

#define STRING_NO_ASSIGN (void*)(-1)

// empty string
extern const struct __String* STRING_EMPTY;

/***************************** Datatypes *******************************/

// string
typedef struct __String* String;
typedef struct __String const* ConstString;

#ifndef SIZEOF_UNSIGNED_LONG
  #define SIZEOF_UNSIGNED_LONG 4
#endif
struct __String
{
  ulong length;                         // current length
  ulong maxLength : SIZEOF_UNSIGNED_LONG*8-2;  // max. length
  enum                                  // type
  {
    STRING_TYPE_DYNAMIC,
    STRING_TYPE_STATIC,
    STRING_TYPE_CONST
  } type : 2;
  char  *data;                          // string data
  #ifndef NDEBUG
    ulong checkSum;                     // checksum of data
  #endif /* not NDEBUG */
};

typedef ulong StringIterator;

// internal tokenizer data
typedef struct
{
  const char *data;                     // string data
  ulong      length;                    // string length
  long       index;                     // index in string
  const char *separatorChars;           // token separator characters
  const char *stringQuotes;             // string quote characters
  bool       skipEmptyTokens;           // TRUE for skipping empty tokens
  String     token;                     // next token
} StringTokenizer;

// comparison, iteration functions
typedef int(*StringCompareFunction)(char ch1, char ch2, void *userData);
typedef const char*(*StringIterateFunction)(char ch, void *userData);

// number unit
typedef struct
{
  const char *name;
  uint64     factor;
} StringUnit;

// debug info function

#ifndef NDEBUG

/***********************************************************************\
* Name   : StringDumpInfoFunction
* Purpose: string dump info call-back function
* Input  : string        - string
*          allocFileName - allocation file name
*          allocLineNb   - allocation line number
*          n             - string number [0..count-1]
*          count         - total string count
*          userData      - user data
* Output : -
* Return : TRUE for continue, FALSE for abort
* Notes  : -
\***********************************************************************/

typedef bool(*StringDumpInfoFunction)(ConstString string,
                                      const char  *allocFileName,
                                      ulong       allocLineNb,
                                      ulong       n,
                                      ulong       count,
                                      void        *userData
                                     );

#endif /* not NDEBUG */

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// static string
#define __STATIC_STRING_IDENTIFIER1(name,suffix) __STATIC_STRING_IDENTIFIER2(name,suffix)
#define __STATIC_STRING_IDENTIFIER2(name,suffix) __##name##suffix
#ifndef NDEBUG
  #define StaticString(name,length) \
    char __STATIC_STRING_IDENTIFIER1(name,_data)[(length)+1] = { [0] = NUL }; \
    struct __String __STATIC_STRING_IDENTIFIER1(name,_string) = \
    { \
      0, \
      (length)+1, \
      STRING_TYPE_STATIC, \
      __STATIC_STRING_IDENTIFIER1(name,_data), \
      STRING_CHECKSUM(0,(length)+1,__STATIC_STRING_IDENTIFIER1(name,_data)) \
    }; \
    String const name = &(__STATIC_STRING_IDENTIFIER1(name,_string))
#else /* NDEBUG */
  #define StaticString(name,length) \
    char __STATIC_STRING_IDENTIFIER1(name,_data)[(length)+1] = { [0] = NUL }; \
    struct __String __STATIC_STRING_IDENTIFIER1(name,_string) = \
    { \
      0, \
      (length)+1, \
      STRING_TYPE_STATIC, \
      __STATIC_STRING_IDENTIFIER1(name,_data) \
    }; \
    String const name = &(__STATIC_STRING_IDENTIFIER1(name,_string))
#endif /* not NDEBUG */

// debugging
#ifndef NDEBUG
  #define String_new()           __String_new(__FILE__,__LINE__)
  #define String_newCString(...) __String_newCString(__FILE__,__LINE__, ## __VA_ARGS__)
  #define String_newChar(...)    __String_newChar(__FILE__,__LINE__, ## __VA_ARGS__)
  #define String_newBuffer(...)  __String_newBuffer(__FILE__,__LINE__, ## __VA_ARGS__)
  #define String_duplicate(...)  __String_duplicate(__FILE__,__LINE__, ## __VA_ARGS__)
  #define String_copy(...)       __String_copy(__FILE__,__LINE__, ## __VA_ARGS__)
  #define String_delete(...)     __String_delete(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

#ifndef NDEBUG
  #define STRING_CHECKSUM(length,maxLength,data) \
    ((ulong)((length)^(ulong)(maxLength)^(ulong)(intptr_t)(data)))

  /***********************************************************************\
  * Name   : String_debugCheckValid
  * Purpose: check if string is valod
  * Input  : __fileName__ - file name
  *          __lineNb__   - line number
  *          string       - string
  * Output : -
  * Return : -
  * Notes  : HALT if string is invalid
  \***********************************************************************/

  void String_debugCheckValid(const char *__fileName__, ulong __lineNb__, ConstString string);

  #define STRING_CHECK_VALID(string) \
    do \
    { \
      String_debugCheckValid(__FILE__,__LINE__,string); \
    } \
    while (0)

  #define STRING_CHECK_VALID_AT(fileName,lineNb,string) \
    do \
    { \
      String_debugCheckValid(fileName,lineNb,string); \
    } \
    while (0)
#else /* NDEBUG */
  #define STRING_CHECK_VALID(string) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : STRING_CHAR_ITERATE
* Purpose: iterated over characters of string and execute block
* Input  : string           - string
*          iteratorVariable - iterator variable (type long)
*          variable         - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all characters in string
*          usage:
*            StringIterator iteratorVariable;
*            char           variable;
*            STRING_CHAR_ITERATE(string,iteratorVariable,variable)
*            {
*              ... = variable
*            }
\***********************************************************************/

#define STRING_CHAR_ITERATE(string,iteratorVariable,variable) \
  for (iteratorVariable = 0, variable = String_index(string,0L); \
       (iteratorVariable) < String_length(string); \
       iteratorVariable++, variable = String_index(string,iteratorVariable) \
      )

/***********************************************************************\
* Name   : STRING_CHAR_ITERATEX
* Purpose: iterated over characters of string and execute block
* Input  : string           - string
*          iteratorVariable - iterator variable (type long)
*          variable         - iteration variable
*          condition        - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all characters in string
*          usage:
*            ulong iteratorVariable;
*            char  variable;
*            STRING_CHAR_ITERATEX(string,iteratorVariable,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define STRING_CHAR_ITERATEX(string,iteratorVariable,variable,condition) \
  for (iteratorVariable = 0, variable = String_index(string,0L); \
       ((iteratorVariable) < String_length(string)) && (condition); \
       iteratorVariable++, variable = String_index(string,iteratorVariable) \
      )

/***********************************************************************\
* Name   : STRING_CHAR_ITERATE_UTF8
* Purpose: iterated over characters of string and execute block
* Input  : string           - string
*          iteratorVariable - iterator variable (type long)
*          variable         - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all characters in string
*          usage:
*            ulong     iteratorVariable;
*            Codepoint variable;
*            STRING_CHAR_ITERATE_UTF8(string,iteratorVariable,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define STRING_CHAR_ITERATE_UTF8(string,iteratorVariable,variable) \
  for (iteratorVariable = 0, variable = stringAtUTF8(string->data,0,NULL); \
       (iteratorVariable) < String_length(string); \
       (iteratorVariable) = stringNextUTF8(string->data,iteratorVariable), variable = stringAtUTF8(string->data,iteratorVariable,NULL) \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : String_new, String_newCString, String_newChar,
*          String_newBuffer
* Purpose: create new string
* Input  : s            - C-string
*          ch           - character
*          buffer       - buffer
*          bufferLength - length of buffer
* Output : -
* Return : string or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
String String_new(void);
String String_newCString(const char *s);
String String_newChar(char ch);
String String_newBuffer(const void *buffer, ulong bufferLength);
#else /* not NDEBUG */
String __String_new(const char *__fileName__, ulong __lineNb__);
String __String_newCString(const char *__fileName__, ulong __lineNb__, const char *s);
String __String_newChar(const char *__fileName__, ulong __lineNb__, char ch);
String __String_newBuffer(const char *__fileName__, ulong __lineNb__, const void *buffer, ulong bufferLength);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : String_duplicate
* Purpose: duplicate string
* Input  : fromString - string to duplicate
* Output : -
* Return : new string (copy)
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
String String_duplicate(ConstString fromString);
#else /* not NDEBUG */
String __String_duplicate(const char *__fileName__, ulong __lineNb__, ConstString fromString);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : String_copy
* Purpose: copy string
* Input  : string     - reference to string variable (if referenced
*                       string is NULL, a new string is allocated)
*          fromString - string to copy
* Output : -
* Return : new string (copy)
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
String String_copy(String *string, ConstString fromString);
#else /* not NDEBUG */
String __String_copy(const char *__fileName__, ulong __lineNb__, String *string, ConstString fromString);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : String_delete
* Purpose: delete string
* Input  : string - string to delete
* Output : -
* Return : -
* Notes  : use "ConstString" to be able to delete strings which are
*          allocated and then marked "const"
\***********************************************************************/

#ifdef NDEBUG
void String_delete(ConstString string);
#else /* not NDEBUG */
void __String_delete(const char *__fileName__, ulong __lineNb__, ConstString string);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : String_clear
* Purpose: clear string
* Input  : string - string to clear
* Output : -
* Return : cleared string (empty)
* Notes  : -
\***********************************************************************/

String String_clear(String string);

/***********************************************************************\
* Name   : String_erase
* Purpose: erase string content (clear content and string)
* Input  : string - string to erase
* Output : -
* Return : erased string (empty)
* Notes  : -
\***********************************************************************/

String String_erase(String string);

/***********************************************************************\
* Name   : String_set, String_setCString, String_setChar,
*          Stirng_setBuffer
* Purpose: set string (copy string)
* Input  : string       - string to set
*          sourceString - source string
*          s            - C-string
*          ch           - character
*          buffer       - buffer
*          bufferLength - length of buffer
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_set(String string, ConstString sourceString);
String String_setCString(String string, const char *s);
String String_setChar(String string, char ch);
String String_setBuffer(String string, const void *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_format, String String_vformat, String_formatAppend,
*          String_vformatAppend
* Purpose: format string/format string and append
* Input  : string - string
*          format - printf-like format string
*          ...    - arguments
* Output : -
* Return : format string
* Notes  : additional format characters
*           %S   String
*           %cS  String with quoting char c
*           %b   binary value
*           %y   bool value
*           %nC  repeat char n times (n can be 0)
*           %*C  repeat char * times (* is uint value preceding char
*                argument, can be 0)
\***********************************************************************/

String String_format(String string, const char *format, ...);
String String_vformat(String string, const char *format, va_list arguments);
//TODO: remove, use String_appendFormat
String String_formatAppend(String string, const char *format, ...);
String String_vformatAppend(String string, const char *format, va_list arguments);

/***********************************************************************\
* Name   : String_append, String_appendSub, String_appendCString,
*          String_appendChar, String_appendCharUTF, String_appendBuffer
* Purpose: append to string
* Input  : string         - string
*          appendString/s - string to append
*          ch             - character to append
*          buffer         - buffer to append
*          bufferLength   - length of buffer
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_append(String string, ConstString appendString);
String String_appendSub(String string, ConstString fromString, ulong fromIndex, long fromLength);
String String_appendCString(String string, const char *s);
String String_appendChar(String string, char ch);
String String_appendCharUTF8(String string, Codepoint codepoint);
String String_appendBuffer(String string, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_appendFormat, String String_appendVformat
* Purpose: format string and append
* Input  : string - string
*          format - printf-like format string
*          ...    - arguments
* Output : -
* Return : format string
* Notes  : additional format characters
*           %S   String
*           %cS  String with quoting char c
*           %b   binary value
*           %y   bool value
\***********************************************************************/

String String_appendFormat(String string, const char *format, ...);
String String_appendVFormat(String string, const char *format, va_list arguments);

/***********************************************************************\
* Name   : String_insert, String_insertSub, String_insertCString,
*          String_insertChar, String_insertBuffer
* Purpose: insert into string
* Input  : string         - string
*          index          - index where to insert
*          insertString/s - string to insert
*          fromIndex      - sub-string from index
*          fromLength     - sub-string from length
*          ch             - character to insert
*          buffer         - bufer to insert
*          bufferLength   - length of buffer
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_insert(String string, ulong index, ConstString insertString);
String String_insertSub(String string, ulong index, ConstString fromString, ulong fromIndex, long fromLength);
String String_insertCString(String string, ulong index, const char *s);
String String_insertChar(String string, ulong index, char ch);
String String_insertBuffer(String string, ulong index, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_remove
* Purpose: remove part of string
* Input  : string - string
*          index  - index of first character to remove or STRING_END to
*                   remove characters from end
*          length - length to remove
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_remove(String string, ulong index, ulong length);

/***********************************************************************\
* Name   : String_truncate
* Purpose: truncate string
* Input  : string - string
*          index  - index of first character
*          length - length of string
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_truncate(String string, ulong index, ulong length);

/***********************************************************************\
* Name   : String_replace, String_replaceCString, String_replaceChar,
*          String_replaceBuffer
* Purpose: replace part of string with other string
* Input  : string         - string
*          index          - index where to replace
*          length         - length to replace
*          insertString/s - string to insert
*          ch             - character to insert
*          buffer         - buffer to insert
*          bufferLength   - length of buffers
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_replace(String string, ulong index, ulong length, ConstString insertString);
String String_replaceCString(String string, ulong index, ulong length, const char *s);
String String_replaceChar(String string, ulong index, ulong length, char ch);
String String_replaceBuffer(String string, ulong index, ulong length, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_replaceAll, String_replaceAllCString, String_replaceAllChar
* Purpose: replace all strings with other string
* Input  : string                 - string
*          index                  - index where to replace
*          fromString/from/fromCh - string/character to replace
*          toString/to/toCh       - new string/character
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_replaceAll(String string, ulong index, ConstString fromString, ConstString toString);
String String_replaceAllCString(String string, ulong index, const char *from, const char *to);
String String_replaceAllChar(String string, ulong index, char fromCh, char toCh);

/***********************************************************************\
* Name   : String_map, String_mapCString, String_mapChar,
* Purpose: map string/char to other string/char
* Input  : string         - string
*          index          - index where to start mapping
*          from,to        - from/to strings/chars
*          count          - number of strings/chars
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_map(String string, ulong index, ConstString from[], ConstString to[], uint count);
String String_mapCString(String string, ulong index, const char* from[], const char* to[], uint count);
String String_mapChar(String string, ulong index, const char from[], const char to[], uint count);

/***********************************************************************\
* Name   : String_sub, String_subCString, String_subBuffer
* Purpose: get sub-string from string
* Input  : string/buffer - string/buffer to set
*          fromString    - string to get sub-string from
*          index         - start index (0..n-1)
*          length        - length of sub-string (0..n) or STRING_END
* Output : -
* Return : new sub-string/buffer
* Notes  : -
\***********************************************************************/

String String_sub(String string, ConstString fromString, ulong index, long length);
char *String_subCString(char *s, ConstString fromString, ulong index, long length);
char *String_subBuffer(char *buffer, ConstString fromString, ulong index, long length);

/***********************************************************************\
* Name   : String_join, String_joinCString, String_joinChar,
*          String_joinBuffer
* Purpose: join strings with separator char
* Input  : string       - string
*          joinString/s - string to join
*          ch           - character to join
*          buffer       - buffer to join
*          bufferLength - length of buffer
*          joinChar     - separator character
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_join(String string, ConstString joinString, char joinChar);
String String_joinCString(String string, const char *s, char joinChar);
String String_joinChar(String string, char ch, char joinChar);
String String_joinBuffer(String string, const char *buffer, ulong bufferLength, char joinChar);

/***********************************************************************\
* Name   : String_length
* Purpose: get string length
* Input  : string - string
* Output : -
* Return : length of string (0..n)
* Notes  : -
\***********************************************************************/

INLINE ulong String_length(ConstString string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE ulong String_length(ConstString string)
{
  STRING_CHECK_VALID(string);

  return (string != NULL) ? string->length : 0L;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_isSet
* Purpose: check if string is set (not NULL)
* Input  : string - string
* Output : -
* Return : TRUE iff string is set, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool String_isSet(ConstString string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE bool String_isSet(ConstString string)
{
  STRING_CHECK_VALID(string);

  return (string != NULL);
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_isEmpty
* Purpose: check if string is NULL or empty
* Input  : string - string
* Output : -
* Return : TRUE iff string is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool String_isEmpty(ConstString string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE bool String_isEmpty(ConstString string)
{
  STRING_CHECK_VALID(string);

  return (string != NULL) ? (string->length == 0) : TRUE;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_index
* Purpose: get char at index
* Input  : string - string
*          index  - index (0..n-1) or STRING_END to get last
*                   character
* Output : -
* Return : character at position "index"
* Notes  : -
\***********************************************************************/

INLINE char String_index(ConstString string, ulong index);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE char String_index(ConstString string, ulong index)
{
  char ch;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    if      (index == STRING_END)
    {
      ch = (string->length > 0L) ? string->data[string->length-1] : NUL;
    }
    else if (index < string->length)
    {
      ch = string->data[index];
    }
    else
    {
      ch = NUL;
    }
  }
  else
  {
    ch = NUL;
  }

  return ch;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

INLINE Codepoint String_atUTF8(ConstString string, ulong index, ulong *nextIndex);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE Codepoint String_atUTF8(ConstString string, ulong index, ulong *nextIndex)
{
  Codepoint codepoint;


  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    codepoint = stringAtUTF8(string->data,index,nextIndex);
  }
  else
  {
    codepoint = 0x00000000;
  }

  return codepoint;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_cString
* Purpose: get C-string from string
* Input  : string - string
* Output : -
* Return : C-string
* Notes  : -
\***********************************************************************/

INLINE const char *String_cString(const ConstString string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE const char *String_cString(const ConstString string)
{
  STRING_CHECK_VALID(string);

  return (string != NULL) ? &string->data[0] : NULL;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_compare
* Purpose: compare two strings
* Input  : string1,string2       - strings to compare
*          stringCompareFunction - string compare function (can be NULL)
*          stringCompareUserData - user data for compare function
* Output : -
* Return : -1 if string1 <  string2
*           0 if string1 == string2
*          +1 if string1 >  string2
* Notes  : -
\***********************************************************************/

int String_compare(ConstString           string1,
                   ConstString           string2,
                   StringCompareFunction stringCompareFunction,
                   void                  *stringCompareUserData
                  );

/***********************************************************************\
* Name   : String_equals, String_equalsCString, String_equalsChar
*          String_equalsBuffer
* Purpose: check if strings are equal
* Input  : string1,string2 - strings to compare
*          string/s        - string/C-string to compare
*          string,ch       - strings/character to compare
*          buffer          - buffer to compare
*          bufferLength    - length of buffer
* Output : -
* Return : TRUE if strings equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool String_equals(ConstString string1, ConstString string2);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE bool String_equals(ConstString string1, ConstString string2)
{
  bool equalFlag;

  if ((string1 != NULL) && (string2 != NULL))
  {
    STRING_CHECK_VALID(string1);
    STRING_CHECK_VALID(string2);

    if (string1->length == string2->length)
    {
      equalFlag = (memcmp(string1->data,string2->data,string1->length) == 0);
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */
bool String_equalsCString(ConstString string, const char *s);
bool String_equalsChar(ConstString string, char ch);
bool String_equalsBuffer(ConstString string, const char *buffer, ulong bufferLength);

bool String_equalsIgnoreCase(ConstString string1, ConstString string2);
bool String_equalsIgnoreCaseCString(ConstString string, const char *s);
bool String_equalsIgnoreCaseChar(ConstString string, char ch);
bool String_equalsIgnoreCaseBuffer(ConstString string, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_subEquals, String_subEqualsCString,
*          String_subEqualsChar, String_subEqualsBuffer
* Purpose: check if string2 is equal to string1 at some position
* Input  : string1,string2 - strings to compare
*          string/s        - string/C-string to compare
*          ch              - character to compare
*          buffer          - buffer to compare
*          bufferLength    - length of buffer
*          index           - position in string1
*          length          - length to compare
* Output : -
* Return : TRUE if strings equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool String_subEquals(ConstString string1, ConstString string2, long index, ulong length);
bool String_subEqualsCString(ConstString string, const char *s, long index, ulong length);
bool String_subEqualsChar(ConstString string, char ch, long index);
bool String_subEqualsBuffer(ConstString string, const char *buffer, ulong bufferLength, long index, ulong length);

bool String_subEqualsIgnoreCase(ConstString string1, ConstString string2, long index, ulong length);
bool String_subEqualsIgnoreCaseCString(ConstString string, const char *s, long index, ulong length);
bool String_subEqualsIgnoreCaseChar(ConstString string, char ch, long index);
bool String_subEqualsIgnoreCaseBuffer(ConstString string, const char *buffer, ulong bufferLength, long index, ulong length);

/***********************************************************************\
* Name   : String_startsWith, String_startsWithCString,
*          String_startsWithChar, String_startsWithBuffer
* Purpose: check if string start with string/character/buffer
* Input  : string1/string - string to check
*          string2/s      - string/C-string to compare
*          ch             - character to compare
*          buffer         - buffer to compare
*          bufferLength   - length of buffer
* Output : -
* Return : TRUE iff string1/string start with string2/s/ch/buffer
* Notes  : -
\***********************************************************************/

bool String_startsWith(ConstString string1, ConstString string2);
bool String_startsWithCString(ConstString string, const char *s);
bool String_startsWithChar(ConstString string, char ch);
bool String_startsWithBuffer(ConstString string, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_endsWith, String_endsWithCString,
*          String_endsWithChar, String_endsWithBuffer
* Purpose: check if string ends with string/character/buffer
* Input  : string1/string - string to check
*          string2/s      - string/C-string to compare
*          ch             - character to compare
*          buffer         - buffer to compare
*          bufferLength   - length of buffer
* Output : -
* Return : TRUE iff string1/string ends with string2/s/ch/buffer
* Notes  : -
\***********************************************************************/

bool String_endsWith(ConstString string1, ConstString string2);
bool String_endsWithCString(ConstString string, const char *s);
bool String_endsWithChar(ConstString string, char ch);
bool String_endsWithBuffer(ConstString string, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_find, String_findCString, String_findChar,
*          String_findLast, String_findLastCString, String_findLastChar
* Purpose: find string in string
* Input  : string - string
*          index - index to start search (0..n-1)
*          findString - string to find/s - C-string to find/
*          ch - character to find
* Output : -
* Return : index in string or -1 if not found
* Notes  : -
\***********************************************************************/

long String_find(ConstString string, ulong index, ConstString findString);
long String_findCString(ConstString string, ulong index, const char *s);
long String_findChar(ConstString string, ulong index, char ch);
long String_findLast(ConstString string, long index, ConstString findString);
long String_findLastCString(ConstString string, long index, const char *s);
long String_findLastChar(ConstString string, long index, char ch);

/***********************************************************************\
* Name   : String_iterate
* Purpose: iterate over string
* Input  : string                - string
*          stringIterateFunction - iterator function
*          stringIterateUserData - user data for iterator function
* Output : -
* Return : string
* Notes  : Note: returned string of iterate function replaces character
*          in string
\***********************************************************************/

String String_iterate(String                string,
                      StringIterateFunction stringIterateFunction,
                      void                  *stringIterateUserData
                     );

/***********************************************************************\
* Name   : String_iterateBegin,String_iterateEnd
* Purpose: iterate over string
* Input  : string                - string
*          stringIterateFunction - iterator function
*          stringIterateUserData - user data for iterator function
* Output : -
* Return : string iterator
* Notes  : Note: returned string of iterate function replaces character
*          in string
\***********************************************************************/

INLINE StringIterator String_iterateBegin(String string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE StringIterator String_iterateBegin(String string)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    return 0;
  }
  else
  {
    return 0;
  }
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

INLINE StringIterator String_iterateEnd(String string);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE StringIterator String_iterateEnd(String string)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    return string->length;
  }
  else
  {
    return 0;
  }
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_iterateNext, String_iterateNextUTF8
* Purpose: get next character from string iterator
* Input  : string         - string
*          stringIterator - string iterator
* Output : stringIterator - string iterator
* Return : character
* Notes  : -
\***********************************************************************/

INLINE char String_iterateNext(String string, StringIterator *stringIterator);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE char String_iterateNext(String string, StringIterator *stringIterator)
{
  char ch;

  STRING_CHECK_VALID(string);
  assert(stringIterator != NULL);

  if ((string != NULL) && ((*stringIterator) < String_iterateEnd(string)))
  {
    ch = string->data[*stringIterator];
    (*stringIterator)++;
  }
  else
  {
    ch = NUL;
  }

  return ch;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

INLINE Codepoint String_iterateNextUTF8(String string, StringIterator *stringIterator);
#if defined(NDEBUG) || defined(__STRINGS_IMPLEMENTATION__)
INLINE Codepoint String_iterateNextUTF8(String string, StringIterator *stringIterator)
{
  ulong     nextIndex;
  Codepoint codepoint;

  STRING_CHECK_VALID(string);
  assert(stringIterator != NULL);

  if ((string != NULL) && ((*stringIterator) < String_iterateEnd(string)))
  {
    codepoint = stringAtUTF8(string->data,*stringIterator,&nextIndex);
    (*stringIterator) = nextIndex;
  }
  else
  {
    codepoint = 0x00000000;
  }

  return codepoint;
}
#endif /* NDEBUG || __STRINGS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : String_toLower, String_toUpper
* Purpose: convert string to lower/upper case
* Input  : string - string
* Output : -
* Return : converted string
* Notes  : -
\***********************************************************************/

String String_toLower(String string);
String String_toUpper(String string);

/***********************************************************************\
* Name   : String_trim, String_trimBegin, String_trimEnd
* Purpose: trim string begin/end
* Input  : string - string
*          chars  - chars to trim
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_trim(String string, const char *chars);
String String_trimBegin(String string, const char *chars);
String String_trimEnd(String string, const char *chars);

/***********************************************************************\
* Name   : String_escape
* Purpose: escape string
* Input  : string     - string variable
*          escapeChar - escape char
*          chars      - characters to escape
*          from       - from map characters or NULL
*          to         - to map characters or NULL
*          count      - number of from/to map characters or 0
* Output : -
* Return : escaped string
* Notes  : -
\***********************************************************************/

String String_escape(String     string,
                     char       escapeChar,
                     const char *chars,
                     const char from[],
                     const char to[],
                     uint       count
                    );

/***********************************************************************\
* Name   : String_unescape
* Purpose: unescape string
* Input  : string     - string variable
*          escapeChar - escape char
*          from       - from map characters or NULL
*          to         - to map characters or NULL
*          count      - number of from/to map characters or 0
* Output : -
* Return : unescaped string
* Notes  : -
\***********************************************************************/

String String_unescape(String     string,
                       char       escapeChar,
                       const char from[],
                       const char to[],
                       uint       count
                      );

/***********************************************************************\
* Name   : String_quote, String_unquote
* Purpose: quote/unquote string
* Input  : string          - string
*          quoteChar       - quote character to add
*          forceQuoteChars - characters to force quote or NULL
*          quoteChars      - quote characters
* Output : -
* Return : quoted/unquoted string
* Notes  : add quote character and escape enclosed quote characters if
*             forceQuoteChars == NULL
*          or string contain some characters from forceQuoteChars
\***********************************************************************/

String String_quote(String string, char quoteChar, const char *forceQuoteChars);
String String_unquote(String string, const char *quoteChars);

/***********************************************************************\
* Name   : String_rightPad, String_rightPad
* Purpose: pad string right/left
* Input  : string - string
*          length - length to pad
*          ch     - padding char
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_padRight(String string, ulong length, char ch);
String String_padLeft(String string, ulong length, char ch);

/***********************************************************************\
* Name   : String_fill
* Purpose: fill string with character
* Input  : string - string
*          length - length to fill
*          ch     - fill char
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_fillChar(String string, ulong length, char ch);

/***********************************************************************\
* Name   : String_initTokenizer, String_initTokenizerCString,
*          String_doneTokenizer
* Purpose: initialise/deinitialise string tokenizer
* Input  : stringTokenizer - string tokenizer
*          string          - string
*          separatorChars  - token seperator characters, e. g. " "
*          stringQuotes    - token string quote characters, e. g. ",'
*          skipEmptyTokens - TRUE to skip empty tokens, FALSE to get
*                            also empty tokens
* Output : -
* Return : -
* Notes  : string must be unchanged until String_doneTokenizer() is
*          called!
\***********************************************************************/

void String_initTokenizer(StringTokenizer *stringTokenizer,
                          ConstString     string,
                          ulong           index,
                          const char      *separatorChars,
                          const char      *stringQuotes,
                          bool            skipEmptyTokens
                         );
void String_initTokenizerCString(StringTokenizer *stringTokenizer,
                                 const char      *string,
                                 const char      *separatorChars,
                                 const char      *stringQuotes,
                                 bool            skipEmptyTokens
                                );
void String_doneTokenizer(StringTokenizer *stringTokenizer);

/***********************************************************************\
* Name   : String_getNextToken
* Purpose: find next token
* Input  : stringTokenizer - string tokenizer
* Output : token      - token (internal reference; do not delete!)
*          tokenIndex - token index (could be NULL)
* Return : TRUE if token found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool String_getNextToken(StringTokenizer *stringTokenizer,
                         ConstString     *token,
                         long            *tokenIndex
                        );

/***********************************************************************\
* Name   : String_scan
* Purpose: scan string
* Input  : string,s - string
*          index    - start index or STRING_BEGIN
*          format   - format (like scanf)
*          ...      - optional variables
* Output : -
* Return : TRUE is scanned, FALSE on error
* Notes  : for C-strings the max. length have to be specified with %<n>s
\***********************************************************************/

bool String_scan(ConstString string, ulong index, const char *format, ...);
bool String_scanCString(const char *s, const char *format, ...);

/***********************************************************************\
* Name   : String_parse
* Purpose: parse string
* Input  : string,s - string
*          index    - start index or STRING_BEGIN
*          format   - format (like scanf)
*          ...      - optional variables
* Output : nextIndex - index of next character in string not parsed or
*                      STRING_END if string completely parsed (can be
*                      NULL)
* Return : TRUE is fully parsed or nextIndex != NULL , FALSE on error
* Notes  : extended scan-function:
*            - match also specified text
*            - %<n>s will return max. <n-1> characters and always add
*              a \0 at the end of the string
*            - %[<c>]s and %[<c>]S are parsed as strings which could
*              be enclosed in "..." or '...'
*            - % s and % S parse rest of string (including spaces)
*            - % [<c>]s and % [<c>]S are parse rest of string as
*              string which could be enclosed in "..." or '...'
*            - %y boolean
*            - if a value is NULL, skip value
\***********************************************************************/

bool String_parse(ConstString string, ulong index, const char *format, long *nextIndex, ...);
bool String_parseCString(const char *s, const char *format, long *nextIndex, ...);

/***********************************************************************\
* Name   : String_match, String_matchCString
* Purpose: match string pattern
* Input  : string        - string
*          index         - start index in string
*          pattern       - pattern
* Output : nextIndex     - index of next character in string not matched
*                          (can be NULL)
*          matchedString - string matching regular expression (can be
*                          STRING_NO_ASSIGN)
*          ...           - optional matching strings of sub-patterns
*                          (can be STRING_NO_ASSIGN), last value have to
*                          be NULL!
* Return : TRUE if pattern is matching, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool String_match(ConstString string, ulong index, ConstString pattern, long *nextIndex, String matchedString, ...);
bool String_matchCString(ConstString string, ulong index, const char *pattern, long *nextIndex, String matchedString, ...);

/***********************************************************************\
* Name   : String_toInteger, String_toInteger64, String_toDouble,
*          String_toBoolean
* Purpose: convert string into integer, integer64, double, boolean or
*          string (string without enclosing quotes ' or ") and add
* Input  : string                   - string variable (for string)
*          convertString            - string to convert
*          index                    - start index in convertString
*          stringUnits              - string units (for integer, double)
*          stringUnitCount          - number of string units (for
*                                     integer, double)
*          trueStrings,falseStrings - string false texts (for boolean)
*                                     or NULL for default values
*          stringQuotes             - string quotes (for string)
* Output : nextIndex - index of next character in string not parsed or
*                      STRING_END if string completely parsed (can be
*                      NULL)
*
* Return : integer/integer64/double/boolean/string value
* Notes  : -
\***********************************************************************/

int String_toInteger(ConstString convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount);
int64 String_toInteger64(ConstString string, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount);
double String_toDouble(ConstString convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount);
bool String_toBoolean(ConstString convertString, ulong index, long *nextIndex, const char *trueStrings[], uint trueStringCount, const char *falseStrings[], uint falseStringCount);
String String_toString(String string, ConstString convertString, ulong index, long *nextIndex, const char *stringQuotes);

/***********************************************************************\
* Name   : String_getMatchingUnit
* Purpose: get matching unit for number
* Input  : n               - number
*          stringUnits     - string units (for integer, double)
*          stringUnitCount - number of string units (for
* Output : -
* Return : string unit or NULL
* Notes  : -
\***********************************************************************/

StringUnit String_getMatchingUnit(int n, const StringUnit units[], uint unitCount);
StringUnit String_getMatchingUnit64(int64 n, const StringUnit units[], uint unitCount);
StringUnit String_getMatchingUnitDouble(double n, const StringUnit units[], uint unitCount);

/***********************************************************************\
* Name   : String_toCString
* Purpose: allocate memory and convert to C-string
* Input  : string - string
* Output : -
* Return : C-string or NULL on insufficient memory
* Notes  : memory have to be deallocated by free()!
\***********************************************************************/

char* String_toCString(ConstString string);

#ifndef NDEBUG

/***********************************************************************\
* Name   : String_debugInit
* Purpose: init string debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : called automatically
\***********************************************************************/

void String_debugInit(void);

/***********************************************************************\
* Name   : String_debugDone
* Purpose: done string debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_debugDone(void);

/***********************************************************************\
* Name   : String_debugDumpInfo, String_debugPrintInfo
* Purpose: string debug function: output allocated strings
* Input  : handle                 - output channel
*          stringDumpInfoFunction - string dump info call-back or NULL
*          stringDumpInfoUserData - string dump info user data
*          stringDumpInfoTypes    - string dump info types; see
*                                   DUMP_INFO_TYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_debugDumpInfo(FILE                   *handle,
                          StringDumpInfoFunction stringDumpInfoFunction,
                          void                   *stringDumpInfoUserData,
                          uint                   stringDumpInfoTypes

                         );
void String_debugPrintInfo(StringDumpInfoFunction stringDumpInfoFunction,
                           void                   *stringDumpInfoUserData,
                           uint                   stringDumpInfoTypes
                          );

/***********************************************************************\
* Name   : String_debugPrintStatistics
* Purpose: string debug function: output strings statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_debugPrintStatistics(void);

/***********************************************************************\
* Name   : String_debugCheck
* Purpose: string debug function: output allocated strings and
*          statistics, check lost resources
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_debugCheck(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __STRINGS__ */

/* end of file */
