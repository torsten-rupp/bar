/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/lists.c,v $
* $Revision: 1.10 $
* $Author: torsten $
* Contents: dynamic list functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

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

void List_done(void                 *list,
               ListNodeFreeFunction listNodeFreeFunction,
               void                 *listNodeFreeUserData
              )
{
  assert(list != NULL);

  List_clear(list,listNodeFreeFunction,listNodeFreeUserData);
}

List *List_new(void)
{
  List *list;

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  List_init(list);

  return list;
}

List *List_duplicate(const void           *fromList,
                     const void           *fromListFromNode,
                     const void           *fromListToNode,
                     ListNodeCopyFunction listNodeCopyFunction,
                     void                 *listNodeCopyUserData
                    )
{
  List *list;

  assert(fromList != NULL);
  assert(listNodeCopyFunction != NULL);

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  List_init(list);
  List_copy(fromList,
            list,
            fromListFromNode,
            fromListToNode,
            NULL,
            listNodeCopyFunction,
            listNodeCopyUserData
           );

  return list;
}

void List_delete(void                 *list,
                 ListNodeFreeFunction listNodeFreeFunction,
                 void                 *listNodeFreeUserData
                )
{
  assert(list != NULL);

  List_done(list,listNodeFreeFunction,listNodeFreeUserData);\
  free(list);
}

void List_clear(void                 *list,
                ListNodeFreeFunction listNodeFreeFunction,
                void                 *listNodeFreeUserData
               )
{
  Node *node;

  assert(list != NULL);

  if (listNodeFreeFunction != NULL)
  {
    while (((List*)list)->head != NULL)
    {
      node = ((List*)list)->head;
      ((List*)list)->head = ((List*)list)->head->next;
      listNodeFreeFunction(node,listNodeFreeUserData);
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

void List_copy(const void           *fromList,
               void                 *toList,
               const void           *fromListFromNode,
               const void           *fromListToNode,
               void                 *toListNextNode,
               ListNodeCopyFunction listNodeCopyFunction,
               void                 *listNodeCopyUserData
              )
{
  Node *node;
  Node *newNode;
  
  assert(fromList != NULL);
  assert(toList != NULL);
  assert(listNodeCopyFunction != NULL);

  if (fromListFromNode == LIST_START) fromListFromNode = ((List*)fromList)->head;

  node = (Node*)fromListFromNode;
  while (node != fromListToNode)
  {
    newNode = listNodeCopyFunction(node,listNodeCopyUserData);
    List_insert(toList,newNode,toListNextNode);
    node = node->next;
  }
  if (node != NULL)
  {
    newNode = listNodeCopyFunction(node,listNodeCopyUserData);
    List_insert(toList,newNode,toListNextNode);
  }
}

void List_move(void *fromList,
               void *toList,
               void *fromListFromNode,
               void *fromListToNode,
               void *toListNextNode
              )
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

void List_insert(void *list,
                 void *node,
                 void *nextNode
                )
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

void List_append(void *list,
                 void *node
                )
{
  assert(list != NULL);
  assert(node != NULL);

  List_insert(list,node,NULL);
}

void *List_remove(void *list,
                  void *node
                 )
{
  void *nextNode;

  assert(list != NULL);
  assert(((List*)list)->head != NULL);
  assert(((List*)list)->tail != NULL);
  assert(((List*)list)->count > 0);
  assert((Node*)node != NULL);

  nextNode = ((Node*)node)->next;
  if (((Node*)node)->prev != NULL) ((Node*)node)->prev->next = ((Node*)node)->next;
  if (((Node*)node)->next != NULL) ((Node*)node)->next->prev = ((Node*)node)->prev;
  if ((Node*)node == ((List*)list)->head) ((List*)list)->head = ((Node*)node)->next;
  if ((Node*)node == ((List*)list)->tail) ((List*)list)->tail = ((Node*)node)->prev;
  ((List*)list)->count--;

  assert(((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL)) ||
         ((((List*)list)->count > 0) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  return nextNode;
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

Node *List_findFirst(void                    *list,
                     ListNodeCompareFunction listNodeCompareFunction,
                     void                    *listNodeCompareUserData
                    )
{
  Node *node;

  assert(list != NULL);
  assert(listNodeCompareFunction != NULL);

  node = ((List*)list)->head;
  while ((node != NULL) && (listNodeCompareFunction(node,listNodeCompareUserData) != 0))
  {
    node = node->next;
  }

  return node;
}

Node *List_findNext(void                    *list,
                    void                    *node,
                    ListNodeCompareFunction listNodeCompareFunction,
                    void                    *listNodeCompareUserData
                   )
{
  assert(list != NULL);
  assert(listNodeCompareFunction != NULL);

  UNUSED_VARIABLE(list);

  if (node != NULL)
  {
    node = (((Node*)node))->next;
    while ((node != NULL) && (listNodeCompareFunction(node,listNodeCompareUserData) != 0))
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
