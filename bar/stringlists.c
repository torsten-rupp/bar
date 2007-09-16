/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/stringlists.c,v $
* $Revision: 1.1 $
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

void StringLists_init(StringList *stringList)
{
  assert(stringList != NULL);

  Lists_init(stringList);
}

void StringLists_done(StringList *stringList, void *userData)
{
  assert(stringList != NULL);

  Lists_done(stringList,(NodeFreeFunction)freeStringNode,userData);
}

StringList *StringLists_new(void)
{
  return (StringList*)Lists_new();
}

void StringLists_delete(StringList *stringList, void *userData)
{
  assert(stringList != NULL);

  Lists_delete(stringList,(NodeFreeFunction)freeStringNode,userData);
}

unsigned long StringLists_empty(StringList *stringList)
{
  assert(stringList != NULL);

  return Lists_empty(stringList);
}

unsigned long StringLists_count(StringList *stringList)
{
  assert(stringList != NULL);

  return Lists_count(stringList);
}

void StringLists_insert(StringList *stringList, String string, void *nextNode)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = (StringNode*)malloc(sizeof(StringNode));
  if (stringNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringNode->string = String_copy(string);
  Lists_ins(stringList,stringNode,nextNode);
}

void StringLists_append(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = (StringNode*)malloc(sizeof(StringNode));
  if (stringNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringNode->string = String_copy(string);
  Lists_add(stringList,stringNode);
}

void StringLists_remove(StringList *stringList, StringNode *stringNode)
{
  assert(stringList != NULL);
  assert(stringNode != NULL);
}

String StringLists_getFirst(StringList *stringList, String string)
{
  StringNode *stringNode;
  assert(string != NULL);

  assert(stringList != NULL);

  stringNode = (StringNode*)Lists_getFirst(stringList);
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

String StringLists_getLast(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);
  assert(string != NULL);

  stringNode = (StringNode*)Lists_getLast(stringList);
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
