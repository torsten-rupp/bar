/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/stringlists.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: string list functions
* Systems : all
*
\***********************************************************************/

#ifndef __STRINGLISTS__
#define __STRINGLISTS__

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
* Name   : StringLists_init
* Purpose: initialise string list
* Input  : stringList - string list to initialize
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_init(StringList *stringList);

/***********************************************************************\
* Name   : StringLists_done
* Purpose: free a strings in list
* Input  : list     - list to free
*          userData - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_done(StringList *stringList, void *userData);

/***********************************************************************\
* Name   : StringLists_new
* Purpose: allocate new string list
* Input  : -
* Output : -
* Return : string list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

StringList *StringLists_new(void);

/***********************************************************************\
* Name   : StringLists_delete
* Purpose: free all strings and delete string list
* Input  : stringList - list to free
*          userData   - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_delete(StringList *stringList, void *userData);

/***********************************************************************\
* Name   : StringLists_empty
* Purpose: check if list is empty
* Input  : stringList - string list
* Output : -
* Return : TRUE if list is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

unsigned long StringLists_empty(StringList *stringList);

/***********************************************************************\
* Name   : StringLists_count
* Purpose: get number of elements in list
* Input  : stringList - string list
* Output : -
* Return : number of elements
* Notes  : -
\***********************************************************************/

unsigned long StringLists_count(StringList *stringList);

/***********************************************************************\
* Name   : StringLists_insert
* Purpose: insert string into list
* Input  : stringList - string list
*          string     - string to insert
*          nextNode   - insert node before nextNode (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_insert(StringList *stringList, String string, void *nextNode);

/***********************************************************************\
* Name   : StringLists_append
* Purpose: add string to end of list
* Input  : stringList - string list
*          string     - string to append to list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_append(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringLists_remove
* Purpose: remove string node from list
* Input  : stringList - string list
*          stringNode - string node to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringLists_remove(StringList *stringList, StringNode *stringNode);

/***********************************************************************\
* Name   : StringLists_getFirst
* Purpose: remove first string from list
* Input  : stringList - string list
*          string     - string variable
* Output : -
* Return : removed string
* Notes  : -
\***********************************************************************/

String StringLists_getFirst(StringList *stringList, String string);

/***********************************************************************\
* Name   : StringLists_getLast
* Purpose: remove last string from list
* Input  : stringList - string list
*          string     - string variable
* Output : -
* Return : removed string
* Notes  : -
\***********************************************************************/

String StringLists_getLast(StringList *stringList, String string);

#ifdef __cplusplus
  }
#endif

#endif /* __STRINGLISTS__ */

/* end of file */
