/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/lists.c,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: dynamic list functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "lists.h"

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

void List_init(void *list)
{
  assert(list != NULL);

  ((List*)list)->head  = NULL;
  ((List*)list)->tail  = NULL;
  ((List*)list)->count = 0;
}

void List_done(void *list, NodeFreeFunction nodeFreeFunction, void *userData)
{
  Node *node;

  assert(list != NULL);

  if (nodeFreeFunction != NULL)
  {
    while (((List*)list)->head != NULL)
    {
      node = ((List*)list)->head;
      ((List*)list)->head = ((List*)list)->head->next;
      nodeFreeFunction(node,userData);
    }
  }
  else
  {
    while (((List*)list)->head != NULL)
    {
      node = ((List*)list)->head;
      ((List*)list)->head = ((List*)list)->head->next;
      LIST_DELETE_NODE(node);
    }
  }
  ((List*)list)->tail  = NULL;
  ((List*)list)->count = 0;
}

List *List_new(void)
{
  List *list;

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  List_init(list);

  return list;
}

void List_delete(void *list, NodeFreeFunction nodeFreeFunction, void *userData)
{
  assert(list != NULL);

  List_done(list,nodeFreeFunction,userData);\
  free(list);
}

void List_move(void *fromList, void *toList, void *fromListFromNode, void *fromListToNode, void *toListNextNode)
{
  Node *node;
  Node *nextNode;
  
  assert(fromList != NULL);
  assert(toList != NULL);

  if (fromListFromNode == LIST_START) fromListFromNode = ((List*)fromList)->head;

  node = (Node*)fromListFromNode;
  while (node != fromListToNode)
  {
    nextNode = node->next;
    List_remove(fromList,node);
    List_insert(toList,node,toListNextNode);
    node = nextNode;
  }
  if (node != NULL)
  {
    List_remove(fromList,node);
    List_insert(toList,node,toListNextNode);
  }
}

unsigned long List_empty(void *list)
{
  assert(list != NULL);
  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  return (((List*)list)->count == 0);
}

unsigned long List_count(void *list)
{
  assert(list != NULL);
  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  return ((List*)list)->count;
}

void List_insert(void *list, void *node, void *nextNode)
{
  assert(list != NULL);

  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );
  assert(node != NULL);

  if      (nextNode != NULL)
  {
    ((Node*)node)->prev = ((Node*)nextNode)->prev;
    ((Node*)node)->next = ((Node*)nextNode);
    if (((Node*)nextNode)->prev != NULL) ((Node*)nextNode)->prev->next = node;
    if (((Node*)nextNode)->next != NULL) ((Node*)nextNode)->next->prev = node;

    if (((List*)list)->head == nextNode) ((List*)list)->head = node;
    ((List*)list)->count++;
  }
  else if (((List*)list)->head != NULL)
  {
    ((Node*)node)->prev = ((List*)list)->tail;
    ((Node*)node)->next = NULL;

    ((List*)list)->tail->next = node;
    ((List*)list)->tail = node;
    ((List*)list)->count++;
  }
  else
  {
    ((Node*)node)->prev = NULL;
    ((Node*)node)->next = NULL;

    ((List*)list)->head  = node;
    ((List*)list)->tail  = node;
    ((List*)list)->count = 1;
  }

  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );
}

void List_append(void *list, void *node)
{
  assert(list != NULL);
  assert(node != NULL);

  List_insert(list,node,NULL);
}

void List_remove(void *list, void *node)
{
  assert(list != NULL);
  assert(((List*)list)->head != NULL);
  assert(((List*)list)->tail != NULL);
  assert(((List*)list)->count > 0);
  assert((Node*)node != NULL);

  if (((Node*)node)->prev != NULL) ((Node*)node)->prev->next = ((Node*)node)->next;
  if (((Node*)node)->next != NULL) ((Node*)node)->next->prev = ((Node*)node)->prev;
  if ((Node*)node == ((List*)list)->head) ((List*)list)->head = ((Node*)node)->next;
  if ((Node*)node == ((List*)list)->tail) ((List*)list)->tail = ((Node*)node)->prev;
  ((List*)list)->count--;

  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );
}

Node *List_getFirst(void *list)
{
  Node *node;

  assert(list != NULL);

  node = ((List*)list)->head;
  if (node != NULL) List_remove(list,node);

  return node;
}

Node *List_getLast(void *list)
{
  Node *node;

  assert(list != NULL);

  node = ((List*)list)->tail;
  if (node != NULL) List_remove(list,node);

  return node;
}

/***********************************************************************\
* Name   : List_findFirst
* Purpose: find node in list
* Input  : list                - list
*          nodeCompareFunction - compare function
*          userData            - user data for compare function
* Output : -
* Return : node or NULL if not found
* Notes  : -
\***********************************************************************/

Node *List_findFirst(void *list, NodeCompareFunction nodeCompareFunction, void *userData)
{
  Node *node;

  assert(list != NULL);
  assert(nodeCompareFunction != NULL);

  node = ((List*)list)->head;
  while ((node != NULL) && (nodeCompareFunction(node,userData) != 0))
  {
    node = node->next;
  }

  return node;
}

Node *List_findNext(void *list, void *node, NodeCompareFunction nodeCompareFunction, void *userData)
{
  assert(list != NULL);
  assert(nodeCompareFunction != NULL);

  if (node != NULL)
  {
    node = (((Node*)node))->next;
    while ((node != NULL) && (nodeCompareFunction(node,userData) != 0))
    {
      node = (((Node*)node))->next;
    }
  }

  return node;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
