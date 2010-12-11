/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/stringlists.h,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: string list functions
* Systems: all
*
\***********************************************************************/

#ifndef __STRING_LISTS__
#define __STRING_LISTS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define STRINGLIST_BEGIN NULL

/***************************** Datatypes *******************************/

typedef struct StringNode
{
  LIST_NODE_HEADER(struct StringNode);

  String string;
} StringNode;

typedef struct
{
  LIST_HEADER(StringNode);
} StringList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : STRINGLIST_ITERATE
* Purpose: iterate over string list
* Input  : stringList - string list
*          variable   - iteration variable (type StringNode)
*          value      - iteration value (must not be initalised!)
* Output : -
* Return : -
* Notes  : value will contain all strings in list
\***********************************************************************/

#define STRINGLIST_ITERATE(stringList,variable,value) \
  for ((variable) = (stringList)->head, value = ((variable) != NULL) ? (stringList)->head->string : NULL; \
       (variable) != NULL; \
       (variable) = (variable)->next, value = ((variable) != NULL) ? (variable)->string : NULL \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : StringList_init
* Purpose: initialise string list
* Input  : stringList - string list to initialize
* Output : -
* Return : initialized string list
* Notes  : -
\***********************************************************************/

void StringList_init(StringList *stringList);

/***********************************************************************\
* Name   : StringList_done
* Purpose: free a strings in list
* Input  : stringList - string list to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_done(StringList *stringList);

/***********************************************************************\
* Name   : StringList_new
* Purpose: allocate new string list
* Input  : -
* Output : -
* Return : string list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

StringList *StringList_new(void);

/***********************************************************************\
* Name   : StringList_duplicate
* Purpose: duplicate string list
* Input  : stringList - string list to duplicate
* Output : -
* Return : string list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

StringList *StringList_duplicate(const StringList *stringList);

/***********************************************************************\
* Name   : StringList_copy
* Purpose: copy sting list
* Input  : stringList - string list
*          fromStringList - string list to copy
* Output : -
* Return : string list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

void StringList_copy(StringList *stringList, const StringList *fromStringList);

/***********************************************************************\
* Name   : StringList_delete
* Purpose: free all strings and delete string list
* Input  : stringList - list to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_delete(StringList *stringList);

/***********************************************************************\
* Name   : StringList_clear
* Purpose: remove all entry in list
* Input  : stringList - string list
* Output : -
* Return : -
* Notes  : 
\***********************************************************************/

void StringList_clear(StringList *stringList);

/***********************************************************************\
* Name   : StringList_move
* Purpose: move strings from soruce list to destination list
* Input  : fromStringList - from string list
*          toStringList   - to string list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_move(StringList *fromStringList, StringList *toStringList);

/***********************************************************************\
* Name   : StringList_empty
* Purpose: check if list is empty
* Input  : stringList - string list
* Output : -
* Return : TRUE if list is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool StringList_empty(const StringList *stringList);
#if defined(NDEBUG) || defined(__STRINGLISTS_IMPLEMENATION__)
INLINE bool StringList_empty(const StringList *stringList)
{
  assert(stringList != NULL);

  return List_empty(stringList);
}
#endif /* NDEBUG || __STRINGLISTS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : StringList_count
* Purpose: get number of elements in list
* Input  : stringList - string list
* Output : -
* Return : number of elements
* Notes  : -
\***********************************************************************/

INLINE ulong StringList_count(const StringList *stringList);
#if defined(NDEBUG) || defined(__STRINGLISTS_IMPLEMENATION__)
INLINE ulong StringList_count(const StringList *stringList)
{
  assert(stringList != NULL);

  return List_count(stringList);
}
#endif /* NDEBUG || __STRINGLISTS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : StringList_insert/StringList_insertCString/
*          StringList_insertChar/StringList_insertBuffer
* Purpose: insert string into list
* Input  : stringList - string list
*          string     - string to insert
*          nextNode   - insert node before nextNode (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_insert(StringList *stringList, const String string, StringNode *nextStringNode);
void StringList_insertCString(StringList *stringList, const char *s, StringNode *nextStringNode);
void StringList_insertChar(StringList *stringList, char ch, StringNode *nextStringNode);
void StringList_insertBuffer(StringList *stringList, char *buffer, ulong bufferLength, StringNode *nextStringNode);

/***********************************************************************\
* Name   : StringList_append/StringList_appendCString
*          StringList_appendChar/StringList_appendBuffer
* Purpose: add string to end of list
* Input  : stringList - string list
*          string     - string to append to list (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_append(StringList *stringList, const String string);
void StringList_appendCString(StringList *stringList, const char *s);
void StringList_appendChar(StringList *stringList, char ch);
void StringList_appendBuffer(StringList *stringList, char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : StringList_remove
* Purpose: remove string node from list
* Input  : stringList - string list
*          stringNode - string node to remove
* Output : -
* Return : next node in list or NULL
* Notes  : -
\***********************************************************************/

StringNode *StringList_remove(StringList *stringList, StringNode *stringNode);

/***********************************************************************\
* Name   : StringList_first
* Purpose: first string from list
* Input  : stringList - string list
*          string     - string variable (can be NULL)
* Output : -
* Return : string or NULL if no more strings
* Notes  : if no string variable is supplied, the string from the list
*          is returned directly and must not be freed!
\***********************************************************************/

INLINE String StringList_first(const StringList *stringList, String string);
#if defined(NDEBUG) || defined(__STRINGLISTS_IMPLEMENATION__)
INLINE String StringList_first(const StringList *stringList, String string)
{
  assert(stringList != NULL);

  if (string != NULL)
  {
    String_set(string,stringList->head->string);
    return string;
  }
  else
  {
    return stringList->head->string;
  }
}
#endif /* NDEBUG || __STRINGLISTS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : StringList_last
* Purpose: last string from list
* Input  : stringList - string list
*          string     - string variable (can be NULL)
* Output : -
* Return : string or NULL if no more strings
* Notes  : if no string variable is supplied, the string from the list
*          is returned directly and must not be freed!
\***********************************************************************/

INLINE String StringList_last(const StringList *stringList, String string);
#if defined(NDEBUG) || defined(__STRINGLISTS_IMPLEMENATION__)
INLINE String StringList_last(const StringList *stringList, String string)
{
  assert(stringList != NULL);

  if (string != NULL)
  {
    String_set(string,stringList->tail->string);
    return string;
  }
  else
  {
    return stringList->tail->string;
  }
}
#endif /* NDEBUG || __STRINGLISTS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : StringList_getFirst
* Purpose: remove first string from list
* Input  : stringList - string list
*          string     - string variable (can be NULL)
* Output : -
* Return : string or NULL if no more strings
* Notes  : if no string variable is supplied, the string from the list
*          is returned directly and have to be freed
\***********************************************************************/

String StringList_getFirst(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringList_getLast
* Purpose: remove last string from list
* Input  : stringList - string list
*          string     - string variable (can be NULL)
* Output : -
* Return : string or NULL if no more strings
* Notes  : if no string variable is supplied, the string from the list
*          is returned directly and have to be freed
\***********************************************************************/

String StringList_getLast(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringList_find, StringList_findCString
* Purpose: find string in string list
* Input  : stringList - string list
*          string,s   - string to find
* Output : -
* Return : string node or NULL of string not found
* Notes  : -
\***********************************************************************/

StringNode *StringList_find(StringList *stringList, const String string);
StringNode *StringList_findCString(StringList *stringList, const char *s);

/***********************************************************************\
* Name   : StringList_contain, StringList_containCString
* Purpose: check if string list contain string
* Input  : stringList - string list
*          string,s   - string to find
* Output : -
* Return : TRUE if string found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool StringList_contain(StringList *stringList, const String string);
bool StringList_containCString(StringList *stringList, const char *s);

/***********************************************************************\
* Name   : StringList_match
* Purpose: match string with string list
* Input  : stringList - string list
*          patternm   - pattern string
* Output : -
* Return : string node or NULL of string do not match
* Notes  : -
\***********************************************************************/

StringNode *StringList_match(StringList *stringList, const String pattern);
StringNode *StringList_matchCString(StringList *stringList, const char *pattern);

/***********************************************************************\
* Name   : StringList_toCStringArray
* Purpose: allocate array with C strings from string list
* Input  : stringList - string list
* Output : -
* Return : C string array or NULL if insufficient memory
* Notes  : free memory after usage!
\***********************************************************************/

const char* const *StringList_toCStringArray(const StringList *stringList);

#ifndef NDEBUG
void StringList_print(const StringList *stringList);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __STRING_LISTS__ */

/* end of file */
