/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/strings.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: dynamic string functions
* Systems : all
*
\***********************************************************************/

#ifndef __STRINGS__
#define __STRINGS__

/****************************** Includes *******************************/
#include <stdlib.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define STRING_BEGIN 0
#define STRING_END   -1

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/* string */
typedef struct __String* String;

/* internal tokenizer data */
typedef struct
{
  String     string;
  long       index;
  const char *separatorChars;
  const char *stringChars;
  String     token;
} StringTokenizer;

/* comparison, iteration functions */
typedef int(*StringCompareFunction)(void *userData, char ch1, char ch2);
typedef char(*StringIterateFunction)(void *userData, char ch);

/****************************** Macros *********************************/

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

String String_new(void);
String String_newCString(const char *s);
String String_newChar(char ch);
String String_newBuffer(char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_delete
* Purpose: delete string
* Input  : string - string to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_delete(String string);

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

String String_set(String string, String sourceString);
String String_setCString(String string, const char *s);
String String_setChar(String string, char ch);
String String_setBuffer(String string, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : String_copy
* Purpose: duplicate string
* Input  : string - string to copy
* Output : -
* Return : new string (copy)
* Notes  : -
\***********************************************************************/

String String_copy(const String fromString);

/***********************************************************************\
* Name   : String_sub
* Purpose: get sub-string from string
* Input  : string     - string to set
*          fromString - string to get sub-string from
*          index      - start index (0..n-1)
*          length     - length of sub-string (0..n)
* Output : -
* Return : new sub-string
* Notes  : -
\***********************************************************************/

String String_sub(String string, const String fromString, unsigned long index, long length);

/***********************************************************************\
* Name   : String_append, String_appendCString, String_appendChar
* Purpose: append to string
* Input  : string                - string
*          appendString/s/buffer - string to append
*          ch                    - character to append
*          bufferLength          - buffer length
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_append(String string, String appendString);
String String_appendBuffer(String string, const char *buffer, ulong bufferLength);
String String_appendCString(String string, const char *s);
String String_appendChar(String string, char ch);

/***********************************************************************\
* Name   : String_insert, String_insertCString, String_insertChar
* Purpose: insert into string
* Input  : string                - string
*          index                 - index where to insert
*          insertString/s/buffer - string to insert
*          ch                    - character to insert
*          bufferLength          - buffer length
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_insert(String string, unsigned long index, String insertString);
String String_insertBuffer(String string, unsigned long index, const char *buffer, ulong bufferLength);
String String_insertCString(String string, unsigned long index, const char *s);
String String_insertChar(String string, unsigned long index, char ch);

/***********************************************************************\
* Name   : String_remove
* Purpose: remove part of string
* Input  : string - string
*          index  - index of first character to remove
*          length - length to remove
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_remove(String string, unsigned long index, unsigned long length);

/***********************************************************************\
* Name   : String_replace, String_replaceCString, String_replaceChar
* Purpose: replace part of string with other string
* Input  : string                - string                
*          index                 - index where to insert 
*          length                - length to replace     
*          insertString/s/buffer - string to insert
*          ch                    - character to insert
*          bufferLength          - buffer length
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_replace(String string, unsigned long index, unsigned long length, String insertString);
String String_replaceBuffer(String string, unsigned long index, unsigned long length, const char *buffer, ulong bufferLength);
String String_replaceCString(String string, unsigned long index, unsigned long length, const char *s);
String String_replaceChar(String string, unsigned long index, unsigned long length, char ch);

/***********************************************************************\
* Name   : String_length
* Purpose: get string length
* Input  : string - string
* Output : -
* Return : length of string (0..n)
* Notes  : -
\***********************************************************************/

unsigned long String_length(String string);

/***********************************************************************\
* Name   : String_index
* Purpose: get char at index
* Input  : string - string
*          index  - index (0..n-1)
* Output : -
* Return : character position position "index"
* Notes  : -
\***********************************************************************/

char String_index(String string, unsigned long index);

/***********************************************************************\
* Name   : String_cString
* Purpose: get C-string from string
* Input  : string - string
* Output : -
* Return : C-string
* Notes  : -
\***********************************************************************/

const char *String_cString(String string);

/***********************************************************************\
* Name   : String_compare
* Purpose: compare two strings
* Input  : string1,string2 - strings to compare
* Output : -
* Return : -1 if string1 <  string2
*           0 if string1 == string2
*          +1 if string1 >  string2
* Notes  : -
\***********************************************************************/

int String_compare(String string1, String string2, StringCompareFunction stringCompareFunction, void *userData);

/***********************************************************************\
* Name   : String_equals
* Purpose: check if strings are equal
* Input  : string1,string2/string,s/string,ch - strings/character to
*                                               compare
* Output : -
* Return : TRUE if string equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool String_equals(String string1, String string2);
bool String_equalsCString(String string, const char *s);
bool String_equalsChar(String string, char ch);

/***********************************************************************\
* Name   : String_find, String_findCString, String_findChar
* Purpose: find string in string
* Input  : string - string
*          index - index to start search (0..n-1)
*          findString - string to find/s - C-string to find/
*          ch - character to find
* Output : -
* Return : index in string or -1 if not found
* Notes  : -
\***********************************************************************/

long String_find(String string, unsigned long index, String findString);
long String_findCString(String string, unsigned long index, const char *s);
long String_findChar(String string, unsigned long index, char ch);
long String_findLast(String string, long index, String findString);
long String_findLastCString(String string, long index, const char *s);
long String_findLastChar(String string, long index, char ch);

/***********************************************************************\
* Name   : String_iterate
* Purpose: iterate over string
* Input  : string                - string
*          stringIterateFunction - iterator function
*          userData              - user data for iterator function
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_iterate(String string, StringIterateFunction stringIterateFunction, void *userData);

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
* Name   : String_rightPad, String_rightPad
* Purpose: pad string right/left
* Input  : string - string
*          length - length to pad
*          char   - padding char
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

String String_rightPad(String string, unsigned long length, char ch);
String String_leftPad(String string, unsigned long length, char ch);

/***********************************************************************\
* Name   : String_format
* Purpose: format string
* Input  : string - string
*          format - printf-like format string
*          ...    - arguments
* Output : -
* Return : format string
* Notes  : additional format characters
*           %S  String
\***********************************************************************/

String String_format(String string, const char *format, ...);

/***********************************************************************\
* Name   : String_initTokenizer, String_doneTokenizer
* Purpose: initialise/deinitialise string tokenizer
* Input  : stringTokenizer - string tokenizer
*          string          - string
*          separatorChars  - token seperator characters, e. g. " "
*          stringChars     - token string escape characters, e. g. ",'
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void String_initTokenizer(StringTokenizer *stringTokenizer, const String string, const char *separatorChars, const char *stringChars);
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

bool String_getNextToken(StringTokenizer *stringTokenizer, String *const token, long *tokenIndex);

/***********************************************************************\
* Name   : String_parse
* Purpose: parse string
* Input  : String - string
*          format - format (like scanf)
*          ...    - optional variables
* Output : -
* Return : TRUE is parsed, FALSE on error
* Notes  : -
\***********************************************************************/

bool String_parse(String string, const char *format, ...);

#ifdef __cplusplus
  }
#endif

#endif /* __STRINGS__ */

/* end of file */
