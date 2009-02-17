/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/stringlists.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

#define __STRINGLISTS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
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
  LIST_DELETE_NODE(stringNode);
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

StringNode *StringList_remove(StringList *stringList, StringNode *stringNode)
{
  assert(stringList != NULL);
  assert(stringNode != NULL);

  return (StringNode*)List_remove(stringList,stringNode);
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

StringNode *StringList_find(StringList *stringList, const String string)
{
  return StringList_findCString(stringList,String_cString(string));
}

StringNode *StringList_findCString(StringList *stringList, const char *s)
{
  StringNode *stringNode;

  assert(stringList != NULL);

  stringNode = stringList->head;
  while (   (stringNode != NULL)
         && !String_equalsCString(stringNode->string,s)
        )
  {
    stringNode = stringNode->next;
  }

  return stringNode;
}

StringNode *StringList_match(StringList *stringList, const String pattern)
{
  return StringList_matchCString(stringList,String_cString(pattern));
}

StringNode *StringList_matchCString(StringList *stringList, const char *pattern)
{
  regex_t    regex;
  StringNode *stringNode;

  assert(stringList != NULL);
  assert(pattern != NULL);

  /* compile pattern */
  if (regcomp(&regex,pattern,REG_ICASE|REG_EXTENDED) != 0)
  {
    return NULL;
  }

  /* search in list */
  stringNode = stringList->head;
  while (   (stringNode != NULL)
         && (regexec(&regex,String_cString(stringNode->string),0,NULL,0) != 0)
        )
  {
    stringNode = stringNode->next;
  }

  /* free resources */
  regfree(&regex);

  return stringNode;
}

const char* const *StringList_toCStringArray(const StringList *stringList)
{
  char const **cStringArray;
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

#ifndef NDEBUG
void StringList_print(const StringList *stringList)
{
  StringNode *stringNode;
  uint       z;

  assert(stringList != NULL);

  stringNode = stringList->head;
  z = 1;
  while (stringNode != NULL)
  {
    printf("%d: %s\n",z,String_cString(stringNode->string));
    stringNode = stringNode->next;
    z++;
  }
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
