/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/lists.c,v $
* $Revision: 1.1.1.1 $
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

/***********************************************************************\
* Name   : List_init
* Purpose: initialise list
* Input  : -
* Output : -
* Return : list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

void List_init(void *list)
{
  assert(list != NULL);

  ((List*)list)->head  = NULL;
  ((List*)list)->tail  = NULL;
  ((List*)list)->count = 0;
}

/***********************************************************************\
* Name   : List_done
* Purpose: deinitialize list
* Input  : list             - list to free
*          nodeFreeFunction - free function for single node or NULL
*          userData         - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

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
      free(node);
    }
  }
  ((List*)list)->tail  = NULL;
  ((List*)list)->count = 0;
}

/***********************************************************************\
* Name   : List_new
* Purpose: allocate new list
* Input  : -
* Output : -
* Return : list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

List *List_new(void)
{
  List *list;

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  List_init(list);
//fprintf(stderr,"%s,%d: new list %p\n",__FILE__,__LINE__,list);

  return list;
}

/***********************************************************************\
* Name   : List_delete
* Purpose: free list
* Input  : list             - list to free
*          nodeFreeFunction - free function for single node or NULL
*          userData         - user data for free function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_delete(void *list, NodeFreeFunction nodeFreeFunction, void *userData)
{
  assert(list != NULL);

  List_done(list,nodeFreeFunction,userData);\
  free(list);
}

/***********************************************************************\
* Name   : List_move
* Purpose: move contents of list
* Input  : fromList                        - from list
*          toList                          - to list
*          fromListFromNode,fromListToNode - from/to node (could be
*                                            NULL)
*          toListNextNode                  - insert node before nextNode
*                                            (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

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
    List_rem(fromList,node);
    List_ins(toList,node,toListNextNode);
    node = nextNode;
  }
  if (node != NULL)
  {
    List_rem(fromList,node);
    List_ins(toList,node,toListNextNode);
  }
}

/***********************************************************************\
* Name   : List_empty
* Purpose: check if list is empty
* Input  : list - list
* Output : -
* Return : TRUE if list is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

unsigned long List_empty(void *list)
{
  assert(list != NULL);
  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  return (((List*)list)->count == 0);
}

/***********************************************************************\
* Name   : List_count
* Purpose: get number of elements in list
* Input  : list - list to free
* Output : -
* Return : number of elements
* Notes  : -
\***********************************************************************/

unsigned long List_count(void *list)
{
  assert(list != NULL);
  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  return ((List*)list)->count;
}

/***********************************************************************\
* Name   : List_ins
* Purpose: insert node into list
* Input  : list     - list
*          node     - node to insert
*          nextNode - insert node before nextNode (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_ins(void *list, void *node, void *nextNode)
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

/***********************************************************************\
* Name   : List_add
* Purpose: add node to end of list
* Input  : list - list
*          node - node to add
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_add(void *list, void *node)
{
  assert(list != NULL);
  assert(node != NULL);

  List_ins(list,node,NULL);
}

/***********************************************************************\
* Name   : List_rem
* Purpose: remove node from list
* Input  : list - list
*          node - node to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_rem(void *list, void *node)
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

/***********************************************************************\
* Name   : List_getFirst
* Purpose: remove first node from list
* Input  : list - list
* Output : -
* Return : removed node or NULL if list is empty
* Notes  : -
\***********************************************************************/

Node *List_getFirst(void *list)
{
  Node *node;

  assert(list != NULL);

  node = ((List*)list)->head;
  if (node != NULL) List_rem(list,node);

  return node;
}

/***********************************************************************\
* Name   : List_getLast
* Purpose: remove last node from list
* Input  : list - list
* Output : -
* Return : removed node or NULL if list is empty
* Notes  : -
\***********************************************************************/

Node *List_getLast(void *list)
{
  Node *node;

  assert(list != NULL);

  node = ((List*)list)->tail;
  if (node != NULL) List_rem(list,node);

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

/***********************************************************************\
* Name   : List_findNext
* Purpose: find next node in list
* Input  : list                - list
*          node                - previous found node
*          nodeCompareFunction - compare function
*          userData            - user data for compare function
* Output : -
* Return : next node or NULL if no next node found
* Notes  : -
\***********************************************************************/

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
