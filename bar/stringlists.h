/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/stringlists.h,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: string list functions
* Systems : all
*
\***********************************************************************/

#ifndef __STRING_LISTS__
#define __STRING_LISTS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "strings.h"
#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct StringNode
{
  NODE_HEADER(struct StringNode);

  String string;
} StringNode;

typedef struct
{
  LIST_HEADER(StringNode);
} StringList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

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
* Return : -
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

bool StringList_empty(const StringList *stringList);

/***********************************************************************\
* Name   : StringList_count
* Purpose: get number of elements in list
* Input  : stringList - string list
* Output : -
* Return : number of elements
* Notes  : -
\***********************************************************************/

unsigned long StringList_count(const StringList *stringList);

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

void StringList_insert(StringList *stringList, String string, StringNode *nextStringNode);
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

void StringList_append(StringList *stringList, String string);
void StringList_appendCString(StringList *stringList, const char *s);
void StringList_appendChar(StringList *stringList, char ch);
void StringList_appendBuffer(StringList *stringList, char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : StringList_remove
* Purpose: remove string node from list
* Input  : stringList - string list
*          stringNode - string node to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringList_remove(StringList *stringList, StringNode *stringNode);

/***********************************************************************\
* Name   : StringList_getFirst
* Purpose: remove first string from list
* Input  : stringList - string list
*          string     - string variable
* Output : -
* Return : removed string
* Notes  : -
\***********************************************************************/

String StringList_getFirst(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringList_getLast
* Purpose: remove last string from list
* Input  : stringList - string list
*          string     - string variable
* Output : -
* Return : removed string
* Notes  : -
\***********************************************************************/

String StringList_getLast(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringList_toCStringArray
* Purpose: allocate array with C strings from string list
* Input  : stringList - string list
* Output : -
* Return : C string array or NULL if insufficient memory
* Notes  : free memory after usage!
\***********************************************************************/

const char* const *StringList_toCStringArray(const StringList *stringList);

#ifdef __cplusplus
  }
#endif

#endif /* __STRING_LISTS__ */

/* end of file */
