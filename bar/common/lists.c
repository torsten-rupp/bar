/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic list functions
* Systems: all
*
\***********************************************************************/

#define __LISTS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifndef NDEBUG
  #include <pthread.h>
#endif /* not NDEBUG */
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#include <assert.h>

#include "common/global.h"

#include "lists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_LIST_HASH_SIZE 4093
#define DEBUG_MAX_FREE_LIST  4000

/***************************** Datatypes *******************************/
#ifndef NDEBUG
  // list of nodes
  typedef struct DebugListNode
  {
    LIST_NODE_HEADER(struct DebugListNode);

    const char      *fileName;
    ulong           lineNb;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */

    const char      *deleteFileName;
    ulong           deleteLineNb;
    #ifdef HAVE_BACKTRACE
      void const *deleteStackTrace[16];
      int        deleteStackTraceSize;
    #endif /* HAVE_BACKTRACE */

    const List *list;
    const Node *node;
  } DebugListNode;

  typedef struct
  {
    LIST_HEADER(DebugListNode);

    struct
    {
      DebugListNode *first;
      ulong          count;
    } hash[DEBUG_LIST_HASH_SIZE];
  } DebugListNodeList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  LOCAL pthread_once_t      debugListInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugListLockAttributes;
  LOCAL pthread_mutex_t     debugListLock;
  LOCAL DebugListNodeList   debugListAllocNodeList;
  LOCAL DebugListNodeList   debugListFreeNodeList;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

LOCAL_INLINE void listInsert(void *list,
                             void *node,
                             void *nextNode
                            );

LOCAL_INLINE void listRemove(void *list,
                             void *node
                            );

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugNodeHashIndex
* Purpose: get string hash index
* Input  : string - string
* Output : -
* Return : hash index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint debugNodeHashIndex(const Node *node)
{
  assert(node != NULL);

  return ((uintptr_t)node >> 2) % DEBUG_LIST_HASH_SIZE;
}

/***********************************************************************\
* Name   : debugFindNode
* Purpose: find string in list
* Input  : debugListNodeList - node list
*          string            - string
* Output : -
* Return : string node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DebugListNode *debugFindNode(const DebugListNodeList *debugListNodeList, const Node *node)
{
  uint          index;
  DebugListNode *debugListNode;
  ulong         n;

  assert(debugListNodeList != NULL);
  assert(node != NULL);

  index = debugNodeHashIndex(node);

  debugListNode = debugListNodeList->hash[index].first;
  n             = debugListNodeList->hash[index].count;
  while ((debugListNode != NULL) && (n > 0) && (debugListNode->node != node))
  {
    debugListNode = debugListNode->next;
    n--;
  }

  return (n > 0) ? debugListNode : NULL;
}

/***********************************************************************\
* Name   : debugAddNode
* Purpose: add node to list
* Input  : debugListNodeList - list
*          debugListNode     - node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugAddNode(DebugListNodeList *debugListNodeList, DebugListNode *debugListNode)
{
  uint index;

  assert(debugListNodeList != NULL);
  assert(debugListNode != NULL);

  index = debugNodeHashIndex(debugListNode->node);

  listInsert(debugListNodeList,debugListNode,debugListNodeList->hash[index].first);
  debugListNodeList->hash[index].first = debugListNode;
  debugListNodeList->hash[index].count++;
}

/***********************************************************************\
* Name   : debugRemoveNode
* Purpose: remove node from list
* Input  : debugListNodeList - list
*          debugListNode     - node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugRemoveNode(DebugListNodeList *debugListNodeList, DebugListNode *debugListNode)
{
  uint index;

  assert(debugListNodeList != NULL);
  assert(debugListNode != NULL);

  index = debugNodeHashIndex(debugListNode->node);
  assert(debugListNodeList->hash[index].count > 0);

  listRemove(debugListNodeList,debugListNode);
  debugListNodeList->hash[index].count--;
  if      (debugListNodeList->hash[index].count == 0)
  {
    debugListNodeList->hash[index].first = NULL;
  }
  else if (debugListNodeList->hash[index].first == debugListNode)
  {
    debugListNodeList->hash[index].first = debugListNode->next;
  }
}

/***********************************************************************\
* Name   : debugListInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
LOCAL void debugListInit(void)
{
  pthread_mutexattr_init(&debugListLockAttributes);
  pthread_mutexattr_settype(&debugListLockAttributes,PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&debugListLock,&debugListLockAttributes);
  List_init(&debugListAllocNodeList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  memClear(debugListAllocNodeList.hash,sizeof(debugListAllocNodeList.hash));
  List_init(&debugListFreeNodeList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  memClear(debugListFreeNodeList.hash,sizeof(debugListFreeNodeList.hash));
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : debugCheckDuplicateNode
* Purpose: check if node is already in list
* Input  : fileName - code file name
*          lineNb   - code line number
*          list     - list where node should be inserted
*          newNode  - new node to insert
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugCheckDuplicateNode(const char *fileName,
                                   ulong      lineNb,
                                   const List *list,
                                   const Node *node
                                  )
{
#if 1
  uint          index;
  DebugListNode *debugListNode;
  ulong         n;

  assert(list != NULL);

  index = debugNodeHashIndex(node);

  pthread_once(&debugListInitFlag,debugListInit);

  pthread_mutex_lock(&debugListLock);
  {
    debugListNode = debugListAllocNodeList.hash[index].first;
    n             = debugListAllocNodeList.hash[index].count;
    while ((debugListNode != NULL) && (n > 0))
    {
      if (debugListNode->node == node)
      {
        if      (debugListNode->list == list)
        {
           HALT_INTERNAL_ERROR_AT(fileName,lineNb,"node %p is already in list %p initialized at %s, %lu!",node,list,list->fileName,list->lineNb);
        }
        else if (debugListNode->list != NULL)
        {
           HALT_INTERNAL_ERROR_AT(fileName,lineNb,"node %p is still in other list %p initialized at %s, %lu!",node,debugListNode->list,debugListNode->list->fileName,debugListNode->list->lineNb);
        }
      }
      debugListNode = debugListNode->next;
      n--;
    }
  }
  pthread_mutex_unlock(&debugListLock);
#endif
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : listInsert
* Purpose: insert node into list
* Input  : list     - list
*          node     - node to insert
*          nextNode - next node in list or NULL to append
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void listInsert(void *list,
                             void *node,
                             void *nextNode
                            )
{
  #ifndef NDEBUG
    DebugListNode *debugListNode;
  #endif // NDEBUG

  assert(list != NULL);
  assert(node != NULL);

  assert(   ((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL))
         || ((((List*)list)->count == 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL) && (((List*)list)->head == ((List*)list)->tail))
         || ((((List*)list)->count > 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );
  assert(node != NULL);

  // insert into list
  if      (nextNode != NULL)
  {
    // insert in middle of list
    ((Node*)node)->prev = ((Node*)nextNode)->prev;
    ((Node*)node)->next = ((Node*)nextNode);
    if (((Node*)nextNode)->prev != NULL) ((Node*)nextNode)->prev->next = node;
    ((Node*)nextNode)->prev = node;

    if (((List*)list)->head == nextNode) ((List*)list)->head = node;
    ((List*)list)->count++;
  }
  else if (((List*)list)->head != NULL)
  {
    // append to end of list
    ((Node*)node)->prev = ((List*)list)->tail;
    ((Node*)node)->next = NULL;

    ((List*)list)->tail->next = node;
    ((List*)list)->tail = node;
    ((List*)list)->count++;
  }
  else
  {
    // insert as first node
    ((Node*)node)->prev = NULL;
    ((Node*)node)->next = NULL;

    ((List*)list)->head  = node;
    ((List*)list)->tail  = node;
    ((List*)list)->count = 1;
  }
  assert(   ((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL))
         || ((((List*)list)->count == 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL) && (((List*)list)->head == ((List*)list)->tail))
         || ((((List*)list)->count > 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  // add reference to list
  #ifndef NDEBUG
    pthread_once(&debugListInitFlag,debugListInit);

    pthread_mutex_lock(&debugListLock);
    {
      // Node: may be NULL in case debug node is inserted
      debugListNode = debugFindNode(&debugListAllocNodeList,node);
      if (debugListNode != NULL)
      {
        debugListNode->list = list;
      }
    }
    pthread_mutex_unlock(&debugListLock);
  #endif
}

/***********************************************************************\
* Name   : listRemove
* Purpose: remove node from list
* Input  : list - list
*          node - node to remove
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void listRemove(void *list,
                             void *node
                            )
{
  #ifndef NDEBUG
    DebugListNode *debugListNode;
  #endif // NDEBUG

  assert(list != NULL);
  assert(((List*)list)->head != NULL);
  assert(((List*)list)->tail != NULL);
  assert(((List*)list)->count > 0);
  assert(node != NULL);

  // remove from list
  if (((Node*)node)->prev != NULL) ((Node*)node)->prev->next = ((Node*)node)->next;
  if (((Node*)node)->next != NULL) ((Node*)node)->next->prev = ((Node*)node)->prev;
  if ((Node*)node == ((List*)list)->head) ((List*)list)->head = ((Node*)node)->next;
  if ((Node*)node == ((List*)list)->tail) ((List*)list)->tail = ((Node*)node)->prev;
  ((List*)list)->count--;
  assert(   ((((List*)list)->count == 0) && (((List*)list)->head == NULL) && (((List*)list)->tail == NULL))
         || ((((List*)list)->count == 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL) && (((List*)list)->head == ((List*)list)->tail))
         || ((((List*)list)->count > 1) && (((List*)list)->head != NULL) && (((List*)list)->tail != NULL))
        );

  // remove reference to list
  #ifndef NDEBUG
    pthread_once(&debugListInitFlag,debugListInit);

    pthread_mutex_lock(&debugListLock);
    {
      // Node: may be NULL in case debug node is inserted
      debugListNode = debugFindNode(&debugListAllocNodeList,node);
      if (debugListNode != NULL)
      {
        debugListNode->list = NULL;
      }
    }
    pthread_mutex_unlock(&debugListLock);
  #endif
}

#if 0
// NYI: obsolete?
/***********************************************************************\
* Name   : listContains
* Purpose: check if node is in list
* Input  : list - list
*          node - node to search for
* Output : -
* Return : TRUE if node is in list, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool listContains(void *list,
                               void *node
                              )
{
  Node *listNode;

  assert(list != NULL);
  assert(node != NULL);

  listNode = ((List*)list)->head;
  while (listNode != node)
  {
    listNode = listNode->next;
  }

  return listNode != NULL;
}
#endif /* 0 */

// ----------------------------------------------------------------------


#ifdef NDEBUG
Node * List_newNode(ulong size)
#else /* not NDEBUG */
Node * __List_newNode(const char *__fileName__, ulong __lineNb__, ulong size)
#endif /* NDEBUG */
{
  Node *node;
  #ifndef NDEBUG
    DebugListNode *debugListNode;
  #endif /* not NDEBUG */

  // allocate node
  node = (Node*)malloc(size);
  if (node == NULL)
  {
    return NULL;
  }

  // add to allocated node list
  #ifndef NDEBUG
    pthread_once(&debugListInitFlag,debugListInit);

    pthread_mutex_lock(&debugListLock);
    {
      // find node in free-list; reuse or allocate new debug node
      debugListNode = debugListFreeNodeList.head;
      while ((debugListNode != NULL) && (debugListNode->node != node))
      {
        debugListNode = debugListNode->next;
      }
      if (debugListNode != NULL)
      {
        debugRemoveNode(&debugListFreeNodeList,debugListNode);
      }
      else
      {
        debugListNode = (DebugListNode*)malloc(sizeof(DebugListNode));
        if (debugListNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }

      // init list node
      debugListNode->fileName = __fileName__;
      debugListNode->lineNb   = __lineNb__;
      debugListNode->list     = NULL;
      debugListNode->node     = node;
      #ifdef HAVE_BACKTRACE
        debugListNode->stackTraceSize       = getStackTrace(debugListNode->stackTrace,SIZE_OF_ARRAY(debugListNode->stackTrace));
        debugListNode->deleteStackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */
      debugAddNode(&debugListAllocNodeList,debugListNode);
    }
    pthread_mutex_unlock(&debugListLock);
  #endif /* not NDEBUG */

  return node;
}

#ifdef NDEBUG
Node *List_deleteNode(Node *node)
#else /* not NDEBUG */
Node *__List_deleteNode(const char *__fileName__, ulong __lineNb__, Node *node)
#endif /* NDEBUG */
{
  Node *nextNode;
  #ifndef NDEBUG
    DebugListNode *debugListNode;
  #endif /* not NDEBUG */

  assert(node != NULL);

  // remove from allocated node list, add to node free list, shorten list
  #ifndef NDEBUG
    pthread_once(&debugListInitFlag,debugListInit);

    pthread_mutex_lock(&debugListLock);
    {
      // find node in free-list to check for duplicate free
      debugListNode = debugFindNode(&debugListFreeNodeList,node);
      if (debugListNode != NULL)
      {
        fprintf(stderr,"DEBUG WARNING: multiple free of node %p at %s, %lu and previously at %s, %lu which was allocated at %s, %ld!\n",
                node,
                __fileName__,
                __lineNb__,
                debugListNode->deleteFileName,
                debugListNode->deleteLineNb,
                debugListNode->fileName,
                debugListNode->lineNb
               );
        #ifdef HAVE_BACKTRACE
          fprintf(stderr,"  allocated at\n");
          debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugListNode->stackTrace,debugListNode->stackTraceSize,0);
          fprintf(stderr,"  deleted at\n");
          debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugListNode->deleteStackTrace,debugListNode->deleteStackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("delete node");
      }

      // remove node from allocated list, add node to free-list, shorten list
      debugListNode = debugFindNode(&debugListAllocNodeList,node);
      if (debugListNode != NULL)
      {
        // check if node still in some list
        if (debugListNode->list != NULL)
        {
          fprintf(stderr,"DEBUG WARNING: node %p allocated at %s, %lu is still in list %p at %s, %lu!\n",
                  node,
                  debugListNode->fileName,
                  debugListNode->lineNb,
                  debugListNode->list,
                  __fileName__,
                  __lineNb__
                 );
          #ifdef HAVE_BACKTRACE
            fprintf(stderr,"  allocated at\n");
            debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugListNode->stackTrace,debugListNode->stackTraceSize,0);
            fprintf(stderr,"  deleted at\n");
            debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugListNode->deleteStackTrace,debugListNode->deleteStackTraceSize,0);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR("delete node");
        }
        // remove from allocated list
        debugRemoveNode(&debugListAllocNodeList,debugListNode);

        // add to free list
        debugListNode->deleteFileName = __fileName__;
        debugListNode->deleteLineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          debugListNode->deleteStackTraceSize = getStackTrace(debugListNode->deleteStackTrace,SIZE_OF_ARRAY(debugListNode->deleteStackTrace));
        #endif /* HAVE_BACKTRACE */
        debugAddNode(&debugListFreeNodeList,debugListNode);

        // shorten free list
        while (debugListFreeNodeList.count > DEBUG_MAX_FREE_LIST)
        {
          debugListNode = debugListFreeNodeList.head;
          debugRemoveNode(&debugListFreeNodeList,debugListNode);
          free(debugListNode);
        }
      }
      else
      {
        fprintf(stderr,"DEBUG WARNING: node %p not found in debug list at %s, line %lu\n",
                node,
                __fileName__,
                __lineNb__
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("delete node");
      }
    }
    pthread_mutex_unlock(&debugListLock);
  #endif /* not NDEBUG */

  // get next node, free node
  nextNode = node->next;
  free(node);

  return nextNode;
}

#ifdef NDEBUG
void List_init(void                      *list,
               ListNodeDuplicateFunction duplicateFunction,
               void                      *duplicateUserData,
               ListNodeFreeFunction      freeFunction,
               void                      *freeUserData
              )
#else /* not NDEBUG */
void __List_init(const char                *__fileName__,
                 ulong                     __lineNb__,
                 void                      *list,
                 ListNodeDuplicateFunction duplicateFunction,
                 void                      *duplicateUserData,
                 ListNodeFreeFunction      freeFunction,
                 void                      *freeUserData
                )
#endif /* NDEBUG */
{
  assert(list != NULL);

  ((List*)list)->head              = NULL;
  ((List*)list)->tail              = NULL;
  ((List*)list)->count             = 0;
  ((List*)list)->duplicateFunction = duplicateFunction;
  ((List*)list)->duplicateUserData = duplicateUserData;
  ((List*)list)->freeFunction      = freeFunction;
  ((List*)list)->freeUserData      = freeUserData;
  #ifndef NDEBUG
    ((List*)list)->fileName = __fileName__;
    ((List*)list)->lineNb   = __lineNb__;
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
void List_initDuplicate(void                      *list,
                        const void                *fromList,
                        const void                *fromListFromNode,
                        const void                *fromListToNode,
                        ListNodeDuplicateFunction duplicateFunction,
                        void                      *duplicateUserData,
                        ListNodeFreeFunction      freeFunction,
                        void                      *freeUserData
                       )
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
                         )
#endif /* NDEBUG */
{
  assert(list != NULL);
  assert(fromList != NULL);

// TODO: free callback
  #ifdef NDEBUG
    List_init(list,CALLBACK_(duplicateFunction,duplicateUserData),CALLBACK_(freeFunction,freeUserData));
  #else /* not NDEBUG */
    __List_init(__fileName__,__lineNb__,list,CALLBACK_(duplicateFunction,duplicateUserData),CALLBACK_(freeFunction,freeUserData));
  #endif /* NDEBUG */
  List_copy(list,
            NULL,
            fromList,
            fromListFromNode,
            fromListToNode
           );
}

void List_done(void *list)
{
  assert(list != NULL);

  List_clear(list);
}

// TODO: callbacks
#ifdef NDEBUG
List *List_new(ListNodeDuplicateFunction duplicateFunction,
               void                      *duplicateUserData,
               ListNodeFreeFunction      freeFunction,
               void                      *freeUserData
              )
#else /* not NDEBUG */
List *__List_new(const char                *__fileName__,
                 ulong                     __lineNb__,
                 ListNodeDuplicateFunction duplicateFunction,
                 void                      *duplicateUserData,
                 ListNodeFreeFunction      freeFunction,
                 void                      *freeUserData
                )
#endif /* NDEBUG */
{
  List *list;

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  #ifdef NDEBUG
    List_init(list,CALLBACK_(duplicateFunction,duplicateUserData),CALLBACK_(freeFunction,freeUserData));
  #else /* not NDEBUG */
    __List_init(__fileName__,__lineNb__,list,CALLBACK_(duplicateFunction,duplicateUserData),CALLBACK_(freeFunction,freeUserData));
  #endif /* NDEBUG */

  return list;
}

#ifdef NDEBUG
List *List_duplicate(const void                *fromList,
                     const void                *fromListFromNode,
                     const void                *fromListToNode,
                     ListNodeDuplicateFunction duplicateFunction,
                     void                      *duplicateUserData,
                     ListNodeFreeFunction      freeFunction,
                     void                      *freeUserData
                    )
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
                      )
#endif /* NDEBUG */
{
  List *list;

  assert(fromList != NULL);
  assert(duplicateFunction != NULL);

  list = (List*)malloc(sizeof(List));
  if (list == NULL) return NULL;

  #ifdef NDEBUG
    List_initDuplicate(list,
                       fromList,
                       fromListFromNode,
                       fromListToNode,
                       duplicateFunction,
                       duplicateUserData,
                       freeFunction,
                       freeUserData
                      );
  #else /* not NDEBUG */
    __List_initDuplicate(__fileName__,
                         __lineNb__,
                         list,
                         fromList,
                         fromListFromNode,
                         fromListToNode,
                         duplicateFunction,
                         duplicateUserData,
                         freeFunction,
                         freeUserData
                        );
  #endif /* NDEBUG */

  return list;
}

void List_delete(void *list)
{
  assert(list != NULL);

  List_done(list);\
  free(list);
}

void *List_clear(void *list)
{
  Node *node;

  assert(list != NULL);

  if (((List*)list)->freeFunction != NULL)
  {
    while (!List_isEmpty(list))
    {
      node = ((List*)list)->tail;
      listRemove(list,node);
      ((List*)list)->freeFunction(node,((List*)list)->freeUserData);
      LIST_DELETE_NODE(node);
    }
  }
  else
  {
    while (!List_isEmpty(list))
    {
      node = ((List*)list)->tail;
      listRemove(list,node);
      LIST_DELETE_NODE(node);
    }
  }
  assert(((List*)list)->head == NULL);
  assert(((List*)list)->count == 0);

  return list;
}

void List_copy(void       *toList,
               void       *toListNextNode,
               const void *fromList,
               const void *fromListFromNode,
               const void *fromListToNode
              )
{
  Node *node;
  Node *newNode;

  assert(toList != NULL);
  assert(fromList != NULL);
  assert(((List*)toList)->duplicateFunction != NULL);

  if (fromListFromNode == LIST_START) fromListFromNode = ((List*)fromList)->head;

  node = (Node*)fromListFromNode;
  while (node != fromListToNode)
  {
    newNode = ((List*)toList)->duplicateFunction(node,((List*)toList)->duplicateUserData);
    List_insert(toList,newNode,toListNextNode);
    node = node->next;
  }
  if (node != NULL)
  {
    newNode = ((List*)toList)->duplicateFunction(node,((List*)toList)->duplicateUserData);
    List_insert(toList,newNode,toListNextNode);
  }
}

void List_move(void       *toList,
               void       *toListNextNode,
               void       *fromList,
               const void *fromListFromNode,
               const void *fromListToNode
              )
{
  Node *node;
  Node *nextNode;

  assert(toList != NULL);
  assert(fromList != NULL);

  if (fromListFromNode == LIST_START) fromListFromNode = ((List*)fromList)->head;

  node = (Node*)fromListFromNode;
  while (node != fromListToNode)
  {
    nextNode = node->next;
    listRemove(fromList,node);
    listInsert(toList,node,toListNextNode);
    node = nextNode;
  }
  if (node != NULL)
  {
    listRemove(fromList,node);
    listInsert(toList,node,toListNextNode);
  }
}

void List_exchange(void *list1,
                   void *list2
                  )
{
  Node  *node;
  ulong count;

  assert(list1 != NULL);
  assert(list2 != NULL);

  node  = ((List*)list1)->head;  ((List*)list1)->head  = ((List*)list2)->head;  ((List*)list2)->head  = node;
  node  = ((List*)list1)->tail;  ((List*)list1)->tail  = ((List*)list2)->tail;  ((List*)list2)->tail  = node;
  count = ((List*)list1)->count; ((List*)list1)->count = ((List*)list2)->count; ((List*)list2)->count = count;
}

#ifdef NDEBUG
void List_insert(void *list,
                 void *node,
                 void *nextNode
                )
#else /* not NDEBUG */
void __List_insert(const char *fileName,
                   ulong      lineNb,
                   void       *list,
                   void       *node,
                   void       *nextNode
                  )
#endif /* NDEBUG */
{
  assert(list != NULL);
  assert(node != NULL);

  #ifndef NDEBUG
    debugCheckDuplicateNode(fileName,lineNb,(List*)list,(Node*)node);
  #endif /* not NDEBUG */

  listInsert(list,node,nextNode);
}

#ifdef NDEBUG
void List_append(void *list,
                 void *node
                )
#else /* not NDEBUG */
void __List_append(const char *fileName,
                   ulong      lineNb,
                   void       *list,
                   void       *node
                  )
#endif /* NDEBUG */
{
  assert(list != NULL);
  assert(node != NULL);

  #ifdef NDEBUG
    List_insert(list,node,NULL);
  #else /* not NDEBUG */
    __List_insert(fileName,lineNb,list,node,NULL);
  #endif /* NDEBUG */
}

void *List_remove(void *list,
                  void *node
                 )
{
  void *nextNode;

  assert(list != NULL);
  assert(node != NULL);

  nextNode = ((Node*)node)->next;
  listRemove(list,node);

  return nextNode;
}

void *List_removeAndFree(void *list,
                         void *node
                        )
{
  void *nextNode;

  assert(list != NULL);
  assert(node != NULL);

  nextNode = ((Node*)node)->next;
  listRemove(list,node);
  if (((List*)list)->freeFunction != NULL)
  {
    ((List*)list)->freeFunction(node,((List*)list)->freeUserData);
  }
  LIST_DELETE_NODE(node);

  return nextNode;
}

Node *List_removeFirst(void *list)
{
  Node *node;

  assert(list != NULL);

  node = List_first(list);
  if (node != NULL) listRemove(list,node);

  return node;
}

Node *List_removeLast(void *list)
{
  Node *node;

  assert(list != NULL);

  node = List_last(list);
  if (node != NULL) listRemove(list,node);

  return node;
}

bool List_contains(const void             *list,
                   const void             *node,
                   ListNodeEqualsFunction listNodeEqualsFunction,
                   void                   *listNodeEqualsUserData
                  )
{
  Node *findNode;

  assert(list != NULL);

  findNode = ((List*)list)->head;
  while (   (findNode != NULL)
         && (   ((listNodeEqualsFunction == NULL) && (findNode != node))
             || ((listNodeEqualsFunction != NULL) && !listNodeEqualsFunction(findNode,listNodeEqualsUserData))
            )
        )
  {
    findNode = findNode->next;
  }

  return findNode != NULL;
}

#if 0
void pp(void *list)
{
  void *node;

printf("---\n");
  node = ((List*)list)->head;
  while (node != NULL)
  {
printf("%p\n",node);
node = ((Node*)node)->next;
  }
}
#endif /* 0 */

void *List_findFirst(const void             *list,
                     ListFindModes          listFindMode,
                     ListNodeEqualsFunction listNodeEqualsFunction,
                     void                   *listNodeEqualsUserData
                    )
{
  Node *node;

  assert(list != NULL);
  assert(listNodeEqualsFunction != NULL);

  node = NULL;
  switch (listFindMode)
  {
    case LIST_FIND_FORWARD:
      node = ((List*)list)->head;
      while ((node != NULL) && !listNodeEqualsFunction(node,listNodeEqualsUserData))
      {
        node = node->next;
      }
      break;
    case LIST_FIND_BACKWARD:
      node = ((List*)list)->tail;
      while ((node != NULL) && !listNodeEqualsFunction(node,listNodeEqualsUserData))
      {
        node = node->prev;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }

  return node;
}

void *List_findNext(const void             *list,
                    ListFindModes          listFindMode,
                    void                   *node,
                    ListNodeEqualsFunction listNodeEqualsFunction,
                    void                   *listNodeEqualsUserData
                   )
{
  assert(list != NULL);
  assert(listNodeEqualsFunction != NULL);

  UNUSED_VARIABLE(list);

  if (node != NULL)
  {
    switch (listFindMode)
    {
      case LIST_FIND_FORWARD:
        node = (((Node*)node))->next;
        while ((node != NULL) && !listNodeEqualsFunction(node,listNodeEqualsUserData))
        {
          node = (((Node*)node))->next;
        }
        break;
      case LIST_FIND_BACKWARD:
        node = (((Node*)node))->prev;
        while ((node != NULL) && !listNodeEqualsFunction(node,listNodeEqualsUserData))
        {
          node = (((Node*)node))->prev;
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; // not reached
      #endif /* NDEBUG */
    }
  }

  return node;
}

Node *List_findAndRemove(void                   *list,
                         ListFindModes          listFindMode,
                         ListNodeEqualsFunction listNodeEqualsFunction,
                         void                   *listNodeEqualsUserData
                        )
{
  Node *node;

  assert(list != NULL);
  assert(listNodeEqualsFunction != NULL);

  node = List_findFirst(list,listFindMode,listNodeEqualsFunction,listNodeEqualsUserData);
  if (node != NULL)
  {
    List_remove(list,node);
  }

  return node;
}

void List_sort(void                    *list,
               ListNodeCompareFunction listNodeCompareFunction,
               void                    *listNodeCompareUserData
              )
{
  List  sortedList;
  void  *node1,*node2;
  ulong n;
  bool  mergedFlag;
  ulong i;
  ulong n1,n2;
  void  *node;

  assert(list != NULL);
  assert(listNodeCompareFunction != NULL);

//pp(list);

  /* sort list with merge-sort */
  n = 1;
  do
  {
    sortedList.head = NULL;
    sortedList.tail = NULL;

    mergedFlag = FALSE;
    node1 = ((List*)list)->head;
    while (node1 != NULL)
    {
      /* find start of sub-list 2 */
      node2 = node1;
      for (i = 0; (i < n) && (node2 != NULL); i++)
      {
        node2 = ((Node*)node2)->next;
      }

      /* merge */
      n1 = n;
      n2 = n;
      while (((n1 > 0) && (node1 != NULL)) || ((n2 > 0) && (node2 != NULL)))
      {
        /* select next node to add to sorted list */
        if      ((n1 == 0) || (node1 == NULL))
        {
          /* sub-list 1 is empty -> select node from sub-list 2 */
          node = node2; node2 = ((Node*)node2)->next; n2--;
        }
        else if ((n2 == 0) || (node2 == NULL))
        {
          /* sub-list 2 is empty -> select node from sub-list 1 */
          node = node1; node1 = ((Node*)node1)->next; n1--;
        }
        else
        {
          /* compare nodess from sub-list 1 and 2 */
          if (listNodeCompareFunction(node1,node2,listNodeCompareUserData) < 0)
          {
            /* node1 < node2 -> select node1 */
            node = node1; node1 = ((Node*)node1)->next; n1--;
          }
          else
          {
            /* node1 >= node2 -> select node2 */
            node = node2; node2 = ((Node*)node2)->next; n2--;
          }
          mergedFlag = TRUE;
        }

        /* add to list */
        ((Node*)node)->prev = sortedList.tail;
        ((Node*)node)->next = NULL;
        if (sortedList.head != NULL)
        {
          sortedList.tail->next = node;
          sortedList.tail = node;
        }
        else
        {
          sortedList.head = node;
          sortedList.tail = node;
        }
      }
//pp(&sortedList);

      /* next sub-lists */
      node1 = node2;
    }

    /* next sub-list size */
    ((List*)list)->head = sortedList.head;
    ((List*)list)->tail = sortedList.tail;
    n *= 2;
  }
  while (mergedFlag);
}

#ifndef NDEBUG
void List_debugDone(void)
{
  pthread_once(&debugListInitFlag,debugListInit);

  List_debugCheck();

  pthread_mutex_lock(&debugListLock);
  {
    while (!List_isEmpty(&debugListFreeNodeList))
    {
      free(List_removeFirst(&debugListFreeNodeList));
    }
    while (!List_isEmpty(&debugListFreeNodeList))
    {
      free(List_removeFirst(&debugListFreeNodeList));
    }
  }
  pthread_mutex_unlock(&debugListLock);
}

void List_debugDumpInfo(FILE *handle)
{
  DebugListNode *debugListNode;

  pthread_once(&debugListInitFlag,debugListInit);

  pthread_mutex_lock(&debugListLock);
  {
    LIST_ITERATE(&debugListAllocNodeList,debugListNode)
    {
      fprintf(handle,"DEBUG: list node %p allocated at %s, line %lu\n",
              debugListNode->node,
              debugListNode->fileName,
              debugListNode->lineNb
             );
      #ifdef HAVE_BACKTRACE
        fprintf(stderr,"  allocated at\n");
        debugDumpStackTrace(handle,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugListNode->stackTrace,debugListNode->stackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
    }
  }
  pthread_mutex_unlock(&debugListLock);
}

void List_debugPrintInfo()
{
  List_debugDumpInfo(stderr);
}

void List_debugPrintStatistics(void)
{
  pthread_once(&debugListInitFlag,debugListInit);

  pthread_mutex_lock(&debugListLock);
  {
    fprintf(stderr,"DEBUG: %lu list node(s) allocated\n",
            List_count(&debugListAllocNodeList)
           );
    fprintf(stderr,"DEBUG: %lu list node(s) in deleted list\n",
            List_count(&debugListFreeNodeList)
           );
  }
  pthread_mutex_unlock(&debugListLock);
}

void List_debugCheck()
{
  pthread_once(&debugListInitFlag,debugListInit);

  List_debugPrintInfo();
  List_debugPrintStatistics();

  pthread_mutex_lock(&debugListLock);
  {
    if (!List_isEmpty(&debugListAllocNodeList))
    {
      HALT_INTERNAL_ERROR_LOST_RESOURCE();
    }
  }
  pthread_mutex_unlock(&debugListLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
