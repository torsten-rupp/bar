/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic list functions
* Systems: all
*
\***********************************************************************/

#ifndef __LISTS__
#define __LISTS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdbool.h>

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define LIST_START NULL
#define LIST_END   NULL

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : ListNodeDuplicateFunction
* Purpose: duplicate list node function
* Input  : fromNode - copy from node
*          userData - user data
* Output : -
* Return : duplicated list node
* Notes  : -
\***********************************************************************/

typedef void*(*ListNodeDuplicateFunction)(const void *fromNode, void *userData);

/***********************************************************************\
* Name   : ListNodeFreeFunction
* Purpose: delete list node function
* Input  : node     - node
*          userData - user data
* Output : -
* Return : duplicated list node
* Notes  : -
\***********************************************************************/

typedef void(*ListNodeFreeFunction)(void *node, void *userData);

/***********************************************************************\
* Name   : ListNodeEqualsFunction
* Purpose: list node equals function
* Input  : node     - node to check
*          userData - user data
* Output : -
* Return : TRUE iff node equals
* Notes  : -
\***********************************************************************/

typedef bool(*ListNodeEqualsFunction)(const void *node, void *userData);

/***********************************************************************\
* Name   : ListNodeCompareFunction
* Purpose: compare list nodes function
* Input  : node1,node2 - nodes to compare
*          userData - user data
* Output : -
* Return : -1/0/-1 iff </=/>
* Notes  : -
\***********************************************************************/

typedef int(*ListNodeCompareFunction)(const void *node1, const void *node2, void *userData);

// list find modes
typedef enum
{
  LIST_FIND_FORWARD,
  LIST_FIND_BACKWARD
} ListFindModes;

#define LIST_NODE_HEADER(type) \
  type *prev; \
  type *next

#ifndef NDEBUG
  #define LIST_HEADER(type) \
    type                      *head; \
    type                      *tail; \
    ulong                     count; \
    ListNodeDuplicateFunction duplicateFunction; \
    void                      *duplicateUserData; \
    ListNodeFreeFunction      freeFunction; \
    void                      *freeUserData; \
    \
    const char                *fileName; \
    ulong                     lineNb
#else /* NDEBUG */
  #define LIST_HEADER(type) \
    type                      *head; \
    type                      *tail; \
    ulong                     count; \
    ListNodeDuplicateFunction duplicateFunction; \
    void                      *duplicateUserData; \
    ListNodeFreeFunction      freeFunction; \
    void                      *freeUserData
#endif /* not NDEBUG */

// list node
typedef struct Node
{
  LIST_NODE_HEADER(struct Node);
} Node;

// list
typedef struct
{
  LIST_HEADER(Node);
} List;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define LIST_STATIC_INIT {NULL,NULL,0L,NULL,0}
#else /* NDEBUG */
  #define LIST_STATIC_INIT {NULL,NULL,0L}
#endif /* not NDEBUG */

#define LIST_NEW_NODE(type) (type*)List_newNode(sizeof(type))
#define LIST_DELETE_NODE(node) List_deleteNode((Node*)node)
#ifndef NDEBUG
  #define LIST_NEW_NODEX(fileName,lineNb,type) (type*)__List_newNode(fileName,lineNb,sizeof(type))
  #define LIST_DELETE_NODEX(fileName,lineNb,node) __List_deleteNode(fileName,lineNb,(Node*)node)
#endif /* not NDEBUG */

#define LIST_DEFINE(type,define) \
  typedef struct { define; } type; \
  typedef struct type ## Node\
  { \
    LIST_NODE_HEADER(struct type ## Node); \
    define; \
  } type ## Node; \
  typedef struct \
  { \
    LIST_HEADER(type ## Node); \
  } type ## List

/***********************************************************************\
* Name   : LIST_DONE
* Purpose: iterated over list and execute block then delete node
* Input  : list     - list
*          variable - iterator variable
* Output : -
* Return : -
* Notes  : usage:
*            ListNode *variable;
*
*            LIST_DONE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define LIST_DONE(list,variable) \
  for ((variable) = (list)->head; \
       (variable) != NULL; \
       (variable) = (typeof(variable))List_deleteNode((Node*)variable) \
      )

/***********************************************************************\
* Name   : LIST_HEAD
* Purpose: get list head (first node)
* Input  : list - list
* Output : -
* Return : list head
* Notes  : -
\***********************************************************************/

#define LIST_HEAD(list) (list)->head

/***********************************************************************\
* Name   : LIST_TAIL
* Purpose: get list tail (last node)
* Input  : list - list
* Output : -
* Return : list tail
* Notes  : -
\***********************************************************************/

#define LIST_TAIL(list) (list)->tail

/***********************************************************************\
* Name   : LIST_ITERATE
* Purpose: iterated over list and execute block
* Input  : list     - list
*          variable - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            LIST_ITERATE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define LIST_ITERATE(list,variable) \
  for ((variable) = (list)->head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : LIST_ITERATEX
* Purpose: iterated over list and execute block
* Input  : list      - list
*          variable  - iteration variable
*          condition - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            LIST_ITERATEX(list,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define LIST_ITERATEX(list,variable,condition) \
  for ((variable) = (list)->head; \
       ((variable) != NULL) && (condition); \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : LIST_FIND_FIRST, LIST_FIND_LAST, LIST_FIND
* Purpose: find first/last entry in list
* Input  : list      - list
*          variable  - variable name
*          condition - condition code
* Output : -
* Return : node or NULL if not found
* Notes  : usage:
*          node = LIST_FIND_FIRST(list,variable,variable->...)
*          node = LIST_FIND_LAST(list,variable,variable->...)
*          node = LIST_FIND(list,variable,variable->...)
\***********************************************************************/

#define LIST_FIND_FIRST(list,variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      assert((void*)(list) != NULL); \
      \
      variable = (typeof(variable))(list)->head; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->next; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })
#define LIST_FIND_LAST(list,variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      assert((void*)(list) != NULL); \
      \
      variable = (typeof(variable))(list)->tail; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->prev; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })
#define LIST_FIND(list,variable,condition) LIST_FIND_FIRST(list,variable,condition)

/***********************************************************************\
* Name   : LIST_FIND_PREV, LIST_FIND_NEXT
* Purpose: find previous/next entry in list
* Input  : node      - node
*          variable  - variable name
*          condition - condition code
* Output : -
* Return : node or NULL if not found
* Notes  : usage:
*          node = LIST_FIND_PREV(node,variable,variable->...)
*          node = LIST_FIND_NEXT(node,variable,variable->...)
\***********************************************************************/

#define LIST_FIND_PREV(node,variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      assert((void*)(node) != NULL); \
      \
      variable = (typeof(variable))node->prev; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->prev; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })
#define LIST_FIND_NEXT(node,variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      assert((void*)(node) != NULL); \
      \
      variable = (typeof(variable))node->next; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->next; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })

/***********************************************************************\
* Name   : LIST_CONTAINS
* Purpose: check if entry is in list
* Input  : list      - list
*          variable  - variable name
*          condition - condition code
* Output : -
* Return : TRUE iff in list
* Notes  : usage:
*          boolean = LIST_CONTAINS(list,variable,variable->... == ...)
\***********************************************************************/

#define LIST_CONTAINS(list,variable,condition) (LIST_FIND(list,variable,condition) != NULL)

/***********************************************************************\
* Name   : LIST_REMOVE
* Purpose: find and remove entry in list
* Input  : list      - list
*          variable  - iteration variable
*          condition - additional condition
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define LIST_REMOVE(list,variable,condition) \
  do \
  { \
    variable = list->head; \
    while ((variable) != NULL) \
    { \
      if (condition) \
      { \
        (variable) = List_remove(list,variable); \
        break; \
      } \
      else \
      { \
        (variable) = (variable)->next; \
      } \
    } \
  } \
  while (0)

#ifndef NDEBUG
  #define List_newNode(...)       __List_newNode      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_deleteNode(...)    __List_deleteNode   (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_init(...)          __List_init         (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_initDuplicate(...) __List_initDuplicate(__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_new(...)           __List_new          (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_duplicate(...)     __List_duplicate    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_insert(...)        __List_insert       (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_append(...)        __List_append       (__FILE__,__LINE__, ## __VA_ARGS__)
  #define List_appendUniq(...)    __List_appendUniq   (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : List_newNode
* Purpose: allocate new list node
* Input  : size - size of node
* Output : -
* Return : node or NULL if insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Node *List_newNode(ulong size);
#else /* not NDEBUG */
Node *__List_newNode(const char *__fileName__, ulong __lineNb__, ulong size);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_deleteNode
* Purpose: delete list node
* Input  : node - list node
* Output : -
* Return : next node in list or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Node *List_deleteNode(Node *node);
#else /* not NDEBUG */
Node *__List_deleteNode(const char *__fileName__, ulong __lineNb__, Node *node);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_init
* Purpose: initialize list
* Input  : list              - list to initialize
*          duplicateFunction - node duplicate function
*          duplicateUserData - node duplicate user data
*          freeFunction      - node free function
*          freeUserData      - node free user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void List_init(void                      *list,
               ListNodeDuplicateFunction duplicateFunction,
               void                      *duplicateUserData,
               ListNodeFreeFunction      freeFunction,
               void                      *freeUserData
              );
#else /* not NDEBUG */
void __List_init(const char                *__fileName__,
                 ulong                     __lineNb__,
                 void                      *list,
                 ListNodeDuplicateFunction duplicateFunction,
                 void                      *duplicateUserData,
                 ListNodeFreeFunction      freeFunction,
                 void                      *freeUserData
                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_initDuplicate
* Purpose: initialize a duplicated list
* Input  : list                            - list to initialize
*          fromList                        - from list
*          fromListFromNode,fromListToNode - from/to node (could be
*                                            NULL)
*          duplicateFunction               - node duplicate function
*          duplicateUserData               - node duplicate user data
*          freeFunction                    - node free function
*          freeUserData                    - node free user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void List_initDuplicate(void                      *list,
                        const void                *fromList,
                        const void                *fromListFromNode,
                        const void                *fromListToNode,
                        ListNodeDuplicateFunction duplicateFunction,
                        void                      *duplicateUserData,
                        ListNodeFreeFunction      freeFunction,
                        void                      *freeUserData
                       );
#else /* not NDEBUG */
void __List_initDuplicate(const char                *__fileName__,
                          ulong                     __lineNb__,
                          void                      *list,
                          const void                *fromList,
                          const void                *fromListFromNode,
                          const void                *fromListToNode,
                          ListNodeDuplicateFunction duplicateFunction,
                          void                      *duplicateUserData,
                          ListNodeFreeFunction      freeFunction,
                          void                      *freeUserData
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_done
* Purpose: free all nodes
* Input  : list - list to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_done(void *list);

/***********************************************************************\
* Name   : List_new
* Purpose: allocate new list
* Input  : duplicateFunction - node duplicate function
*          duplicateUserData - node duplicate user data
*          freeFunction      - node free function
*          freeUserData      - node free user data
* Output : -
* Return : list or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
List *List_new(ListNodeDuplicateFunction duplicateFunction,
               void                      *duplicateUserData,
               ListNodeFreeFunction      freeFunction,
               void                      *freeUserData
              );
#else /* not NDEBUG */
List *__List_new(const char                *__fileName__,
                 ulong                     __lineNb__,
                 ListNodeDuplicateFunction duplicateFunction,
                 void                      *duplicateUserData,
                 ListNodeFreeFunction      freeFunction,
                 void                      *freeUserData
                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_duplicate
* Purpose: duplicate list
* Input  : fromList                        - from list
*          fromListFromNode,fromListToNode - from/to node (could be
*                                            NULL)
*          duplicateFunction               - node duplicate function
*          duplicateUserData               - node duplicate user data
*          freeFunction                    - node free function
*          freeUserData                    - node free user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
List *List_duplicate(const void                *fromList,
                     const void                *fromListFromNode,
                     const void                *fromListToNode,
                     ListNodeDuplicateFunction duplicateFunction,
                     void                      *duplicateUserData,
                     ListNodeFreeFunction      freeFunction,
                     void                      *freeUserData
                    );
#else /* not NDEBUG */
List *__List_duplicate(const char                *__fileName__,
                       ulong                     __lineNb__,
                       const void                *fromList,
                       const void                *fromListFromNode,
                       const void                *fromListToNode,
                       ListNodeDuplicateFunction duplicateFunction,
                       void                      *duplicateUserData,
                       ListNodeFreeFunction      freeFunction,
                       void                      *freeUserData
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_delete
* Purpose: free all nodes and delete list
* Input  : list - list to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_delete(void *list);

/***********************************************************************\
* Name   : List_clear
* Purpose: free all nodes in list
* Input  : list - list
* Output : -
* Return : list
* Notes  : -
\***********************************************************************/

void *List_clear(void *list);

/***********************************************************************\
* Name   : List_copy
* Purpose: copy contents of list
* Input  : toList                          - to list
*          toListNextNode                  - insert node before nextNode
*                                            (could be NULL)
*          fromList                        - from list
*          fromListFromNode,fromListToNode - from/to node (could be
*                                            NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_copy(void       *toList,
               void       *toListNextNode,
               const void *fromList,
               const void *fromListFromNode,
               const void *fromListToNode
              );

/***********************************************************************\
* Name   : List_move
* Purpose: move contents of list
* Input  : toList                          - to list
*          toListNextNode                  - insert node before nextNode
*                                            (could be NULL)
*          fromList                        - from list
*          fromListFromNode,fromListToNode - from/to node (could be
*                                            NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_move(void       *toList,
               void       *toListNextNode,
               void       *fromList,
               const void *fromListFromNode,
               const void *fromListToNode
              );

/***********************************************************************\
* Name   : List_exchange
* Purpose: exchange list content
* Input  : list1,list2 - lists
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_exchange(void *list1,
                   void *list2
                  );

/***********************************************************************\
* Name   : List_isEmpty
* Purpose: check if list is empty
* Input  : list - list
* Output : -
* Return : TRUE if list is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool List_isEmpty(const void *list);
#if defined(NDEBUG) || defined(__LISTS_IMPLEMENTATION__)
INLINE bool List_isEmpty(const void *list)
{
  assert(list != NULL);
  assert(((((const List*)list)->count == 0) && (((const List*)list)->head == NULL) && (((const List*)list)->tail == NULL)) ||
         ((((const List*)list)->count > 0) && (((const List*)list)->head != NULL) && (((const List*)list)->tail != NULL))
        );

  return (((const List*)list)->count == 0);
}
#endif /* NDEBUG || __LISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : List_count
* Purpose: get number of elements in list
* Input  : list - list
* Output : -
* Return : number of elements
* Notes  : -
\***********************************************************************/

INLINE ulong List_count(const void *list);
#if defined(NDEBUG) || defined(__LISTS_IMPLEMENTATION__)
INLINE ulong List_count(const void *list)
{
  assert(list != NULL);
  assert(((((const List*)list)->count == 0) && (((const List*)list)->head == NULL) && (((const List*)list)->tail == NULL)) ||
         ((((const List*)list)->count > 0) && (((const List*)list)->head != NULL) && (((const List*)list)->tail != NULL))
        );

  return ((const List*)list)->count;
}
#endif /* NDEBUG || __LISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : List_insert
* Purpose: insert node into list
* Input  : list     - list
*          node     - node to insert
*          nextNode - insert node before nextNode (could be NULL to
*                     append)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void List_insert(void *list,
                 void *node,
                 void *nextNode
                );
#else /* not NDEBUG */
void __List_insert(const char *fileName,
                   ulong      lineNb,
                   void       *list,
                   void       *node,
                   void       *nextNode
                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_append
* Purpose: append node to end of list
* Input  : list - list
*          node - node to add
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void List_append(void *list,
                 void *node
                );
#else /* not NDEBUG */
void __List_append(const char *fileName,
                   ulong      lineNb,
                   void       *list,
                   void       *node
                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : List_remove
* Purpose: remove node from list
* Input  : list - list
*          node - node to remove
* Output : -
* Return : next node in list or NULL
* Notes  : -
\***********************************************************************/

void *List_remove(void *list,
                  void *node
                 );

/***********************************************************************\
* Name   : List_removeAndFree
* Purpose: remove node from list and free
* Input  : list - list
*          node - node to remove
* Output : -
* Return : next node in list or NULL
* Notes  : -
\***********************************************************************/

void *List_removeAndFree(void *list,
                         void *node
                        );

/***********************************************************************\
* Name   : List_first
* Purpose: first node from list
* Input  : list - list
* Output : -
* Return : node or NULL if list is empty
* Notes  : -
\***********************************************************************/

INLINE Node *List_first(const void *list);
#if defined(NDEBUG) || defined(__LISTS_IMPLEMENTATION__)
INLINE Node *List_first(const void *list)
{
  assert(list != NULL);
  assert(((((const List*)list)->count == 0) && (((const List*)list)->head == NULL) && (((const List*)list)->tail == NULL)) ||
         ((((const List*)list)->count > 0) && (((const List*)list)->head != NULL) && (((const List*)list)->tail != NULL))
        );

  return ((const List*)list)->head;
}
#endif /* NDEBUG || __LISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : List_last
* Purpose: last node from list
* Input  : list - list
* Output : -
* Return : node or NULL if list is empty
* Notes  : -
\***********************************************************************/

INLINE Node *List_last(const void *list);
#if defined(NDEBUG) || defined(__LISTS_IMPLEMENTATION__)
INLINE Node *List_last(const void *list)
{
  assert(list != NULL);
  assert(((((const List*)list)->count == 0) && (((const List*)list)->head == NULL) && (((const List*)list)->tail == NULL)) ||
         ((((const List*)list)->count > 0) && (((const List*)list)->head != NULL) && (((const List*)list)->tail != NULL))
        );

  return ((const List*)list)->tail;
}
#endif /* NDEBUG || __LISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : List_removeFirst
* Purpose: remove and return first node from list
* Input  : list - list
* Output : -
* Return : removed node or NULL if list is empty
* Notes  : -
\***********************************************************************/

Node *List_removeFirst(void *list);

/***********************************************************************\
* Name   : List_removeLast
* Purpose: remove and return last node from list
* Input  : list - list
* Output : -
* Return : removed node or NULL if list is empty
* Notes  : -
\***********************************************************************/

Node *List_removeLast(void *list);

/***********************************************************************\
* Name   : List_contains
* Purpose: check if list contains node
* Input  : list                   - list
*          node                   - node
*          listNodeEqualsFunction - equals function or NULL
*          listNodeEqualsUserData - user data for equals function
* Output : -
* Return : TRUE if list contain node, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool List_contains(const void             *list,
                   const void             *node,
                   ListNodeEqualsFunction listNodeEqualsFunction,
                   void                   *listNodeEqualsUserData
                  );

/***********************************************************************\
* Name   : List_find
* Purpose: find node in list
* Input  : list                   - list
*          listNodeEqualsFunction - equals function
*          listNodeEqualsUserData - user data for equals function
* Output : -
* Return : node or NULL if not found
* Notes  : -
\***********************************************************************/

INLINE void *List_find(const void             *list,
                       ListNodeEqualsFunction listNodeEqualsFunction,
                       void                   *listNodeEqualsUserData
                      );
#if defined(NDEBUG) || defined(__LISTS_IMPLEMENTATION__)
INLINE void *List_find(const void             *list,
                       ListNodeEqualsFunction listNodeEqualsFunction,
                       void                   *listNodeEqualsUserData
                      )
{
  Node *node;

  assert(list != NULL);
  assert(listNodeEqualsFunction != NULL);

  node = ((const List*)list)->head;
  while ((node != NULL) && !listNodeEqualsFunction(node,listNodeEqualsUserData))
  {
    node = node->next;
  }

  return node;
}
#endif /* NDEBUG || __LISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : List_findFirst
* Purpose: find node in list
* Input  : list                   - list
*          listFindMode           - list find mode
*          listNodeEqualsFunction - equals function
*          listNodeEqualsUserData - user data for equals function
* Output : -
* Return : node or NULL if not found
* Notes  : -
\***********************************************************************/

void *List_findFirst(const void             *list,
                     ListFindModes          listFindMode,
                     ListNodeEqualsFunction listNodeEqualsFunction,
                     void                   *listNodeEqualsUserData
                    );

/***********************************************************************\
* Name   : List_findNext
* Purpose: find next node in list
* Input  : list                   - list
*          listFindMode           - list find mode
*          node                   - previous found node
*          listNodeEqualsFunction - equals function
*          listNodeEqualsUserData - user data for equals function
* Output : -
* Return : next node or NULL if no next node found
* Notes  : -
\***********************************************************************/

void *List_findNext(const void             *list,
                    ListFindModes          listFindMode,
                    void                   *node,
                    ListNodeEqualsFunction listNodeEqualsFunction,
                    void                   *listNodeEqualsUserData
                   );

/***********************************************************************\
* Name   : List_findAndRemove
* Purpose: find and remove node from list
* Input  : list                   - list
*          listFindMode           - list find mode
*          listNodeEqualsFunction - equals function or NULL
*          listNodeEqualsUserData - user data for equals function
* Output : -
* Return : node or NULL if list is empty
* Notes  : -
\***********************************************************************/

Node *List_findAndRemove(void                   *list,
                         ListFindModes          listFindMode,
                         ListNodeEqualsFunction listNodeEqualsFunction,
                         void                   *listNodeEqualsUserData
                        );


/***********************************************************************\
* Name   : List_sort
* Purpose: sort list
* Input  : list                    - list
*          listNodeCompareFunction - compare function
*          listNodeCompareUserData - user data for compare function
* Output : -
* Return : -
* Notes  : use temporary O(n) memory
\***********************************************************************/

void List_sort(void                    *list,
               ListNodeCompareFunction listNodeCompareFunction,
               void                    *listNodeCompareUserData
              );

#ifndef NDEBUG
/***********************************************************************\
* Name   : List_debugInit
* Purpose: init list debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : called automatically
\***********************************************************************/

void List_debugInit(void);

/***********************************************************************\
* Name   : List_debugDone
* Purpose: done list debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_debugDone(void);

/***********************************************************************\
* Name   : List_debugDumpInfo, List_debugPrintInfo
* Purpose: list debug function: output allocated list nodes
* Input  : handle - output channel
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_debugDumpInfo(FILE *handle);
void List_debugPrintInfo(void);

/***********************************************************************\
* Name   : List_debugPrintStatistics
* Purpose: list debug function: output list statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_debugPrintStatistics(void);

/***********************************************************************\
* Name   : List_debugCheck
* Purpose: list debug function: output allocated list nodes and
*          statistics, check lost resources
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void List_debugCheck(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __LISTS__ */

/* end of file */
