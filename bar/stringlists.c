/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/stringlists.c,v $
* $Revision: 1.12 $
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
* Name   : insertString
* Purpose: insert string in string list
* Input  : stringList - string list
*          string     - string to insert
*          nextNode   - next string list node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void insertString(StringList *stringList, const String string, StringNode *nextStringNode)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = LIST_NEW_NODE(StringNode);
  if (stringNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringNode->string = string;
  List_insert(stringList,stringNode,nextStringNode);
}

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

  UNUSED_VARIABLE(userData);

  String_delete(stringNode->string);
}

/*---------------------------------------------------------------------*/

void StringList_init(StringList *stringList)
{
  assert(stringList != NULL);

  List_init(stringList);
}

void StringList_done(StringList *stringList)
{
  assert(stringList != NULL);

  List_done(stringList,(ListNodeFreeFunction)freeStringNode,NULL);
}

StringList *StringList_new(void)
{
  return (StringList*)List_new();
}

void StringList_delete(StringList *stringList)
{
  assert(stringList != NULL);

  List_delete(stringList,(ListNodeFreeFunction)freeStringNode,NULL);
}

void StringList_clear(StringList *stringList)
{
  assert(stringList != NULL);

  List_clear(stringList,(ListNodeFreeFunction)freeStringNode,NULL);
}

void StringList_move(StringList *fromStringList, StringList *toStringList)
{
  assert(fromStringList != NULL);
  assert(toStringList != NULL);

  List_move(fromStringList,toStringList,NULL,NULL,NULL);
}

bool StringList_empty(const StringList *stringList)
{
  assert(stringList != NULL);

  return List_empty(stringList);
}

unsigned long StringList_count(const StringList *stringList)
{
  assert(stringList != NULL);

  return List_count(stringList);
}

void StringList_insert(StringList *stringList, const String string, StringNode *nextStringNode)
{
  insertString(stringList,String_duplicate(string),nextStringNode);
}

void StringList_insertCString(StringList *stringList, const char *s, StringNode *nextStringNode)
{
  insertString(stringList,String_newCString(s),nextStringNode);
}

void StringList_insertChar(StringList *stringList, char ch, StringNode *nextStringNode)
{
  insertString(stringList,String_newChar(ch),nextStringNode);
}

void StringList_insertBuffer(StringList *stringList, char *buffer, ulong bufferLength, StringNode *nextStringNode)
{
  insertString(stringList,String_newBuffer(buffer,bufferLength),nextStringNode);
}

void StringList_append(StringList *stringList, const String string)
{
  insertString(stringList,String_duplicate(string),NULL);
}

void StringList_appendCString(StringList *stringList, const char *s)
{
  insertString(stringList,String_newCString(s),NULL);
}

void StringList_appendChar(StringList *stringList, char ch)
{
  insertString(stringList,String_newChar(ch),NULL);
}

void StringList_appendBuffer(StringList *stringList, char *buffer, ulong bufferLength)
{
  insertString(stringList,String_newBuffer(buffer,bufferLength),NULL);
}

void StringList_remove(StringList *stringList, StringNode *stringNode)
{
  assert(stringList != NULL);
  assert(stringNode != NULL);

  List_remove(stringList,stringNode);
}

String StringList_getFirst(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = (StringNode*)List_getFirst(stringList);
  if (stringNode != NULL)
  {
    if (string != NULL)
    {
      String_set(string,stringNode->string);
      String_delete(stringNode->string);
    }
    else
    {
      string = stringNode->string;
    }
    free(stringNode);

    return string;
  }
  else
  {
    if (string != NULL)
    {
      String_clear(string);
    }

    return NULL;
  }
}

String StringList_getLast(StringList *stringList, String string)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = (StringNode*)List_getLast(stringList);
  if (stringNode != NULL)
  {
    if (string != NULL)
    {
      String_set(string,stringNode->string);
      String_delete(stringNode->string);
    }
    else
    {
      string = stringNode->string;
    }
    free(stringNode);

    return string;
  }
  else
  {
    if (string != NULL)
    {
      String_clear(string);
    }

    return NULL;
  }
}

const char* const *StringList_toCStringArray(const StringList *stringList)
{
  char const  **cStringArray;
  StringNode *stringNode;
  uint       z;

  assert(stringList != NULL);

  cStringArray = (const char**)malloc(stringList->count*sizeof(char*));
  if (cStringArray != NULL)
  {
    stringNode = stringList->head;
    z = 0;
    while (stringNode != NULL)
    {
      cStringArray[z] = String_cString(stringNode->string);
      stringNode = stringNode->next;
      z++;
    }
  }

  return cStringArray;
}

#ifdef __cplusplus
  }
#endif

#endif /* __XYZ__ */

/* end of file */
