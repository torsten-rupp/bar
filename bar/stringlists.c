/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/stringlists.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

#ifndef __XYZ__
#define __XYZ__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "lists.h"
#include "strings.h"

#include "stringlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeStringNode
* Purpose: free allocated file name node
* Input  : fileNameNode - file name node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeStringNode(StringNode *stringNode, void *userData)
{
  assert(stringNode != NULL);

  String_delete(stringNode->string);
}

/*---------------------------------------------------------------------*/

void StringList_init(StringList *stringList)
{
  assert(stringList != NULL);

  List_init(stringList);
}

void StringList_done(StringList *stringList, void *userData)
{
  assert(stringList != NULL);

  List_done(stringList,(NodeFreeFunction)freeStringNode,userData);
}

StringList *StringList_new(void)
{
  return (StringList*)List_new();
}

void StringList_delete(StringList *stringList, void *userData)
{
  assert(stringList != NULL);

  List_delete(stringList,(NodeFreeFunction)freeStringNode,userData);
}

unsigned long StringList_empty(StringList *stringList)
{
  assert(stringList != NULL);

  return List_empty(stringList);
}

unsigned long StringList_count(StringList *stringList)
{
  assert(stringList != NULL);

  return List_count(stringList);
}

void StringList_insert(StringList *stringList, String string, void *nextNode)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = LIST_NEW_NODE(StringNode);
  if (stringNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringNode->string = String_copy(string);
  List_insert(stringList,stringNode,nextNode);
}

void StringList_append(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = LIST_NEW_NODE(StringNode);
  if (stringNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringNode->string = String_copy(string);
  List_append(stringList,stringNode);
}

void StringList_remove(StringList *stringList, StringNode *stringNode)
{
  assert(stringList != NULL);
  assert(stringNode != NULL);
}

String StringList_getFirst(StringList *stringList, String string)
{
  StringNode *stringNode;
  assert(string != NULL);

  assert(stringList != NULL);

  stringNode = (StringNode*)List_getFirst(stringList);
  if (stringNode != NULL)
  {
    String_set(string,stringNode->string);
    String_delete(stringNode->string);
    free(stringNode);
  }
  else
  {
    String_clear(string);
  }

  return string;
}

String StringList_getLast(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);
  assert(string != NULL);

  stringNode = (StringNode*)List_getLast(stringList);
  if (stringNode != NULL)
  {
    String_set(string,stringNode->string);
    String_delete(stringNode->string);
    free(stringNode);
  }
  else
  {
    String_clear(string);
  }

  return string;
}

#ifdef __cplusplus
  }
#endif

#endif /* __XYZ__ */

/* end of file */
