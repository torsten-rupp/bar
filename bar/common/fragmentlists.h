/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Fragment list functions
* Systems: all
*
\***********************************************************************/

#ifndef __FRAGMENT_LISTS__
#define __FRAGMENT_LISTS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

// fragment list range node
typedef struct FragmentRangeNode
{
  LIST_NODE_HEADER(struct FragmentRangeNode);

  uint64 offset;                        // fragment range offset (0..n)
  uint64 length;                        // length of fragment range
} FragmentRangeNode;

// fragment list entry
typedef struct
{
  LIST_HEADER(FragmentRangeNode);
} FragmentRangeList;

// fragment list node
typedef struct FragmentNode
{
  LIST_NODE_HEADER(struct FragmentNode);

  String            name;               // fragment name
  uint64            size;               // size of fragment
  void              *userData;
  uint              userDataSize;
  uint              lockCount;

  FragmentRangeList rangeList;
  uint64            rangeListSum;
} FragmentNode;

// sorted fragment list
typedef struct
{
  LIST_HEADER(FragmentNode);
} FragmentList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : FRAGMENTLIST_ITERATE
* Purpose: iterated over fragment list and execute block
* Input  : fragmentList - fragment list
*          variable     - iterator variable (type FragmentNode)
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*            FRAGMENTLIST_ITERATE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define FRAGMENTLIST_ITERATE(fragmentList,variable) \
  for ((variable) = (fragmentList)->head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : FRAGMENTLIST_ITERATEX
* Purpose: iterated over fragment list and execute block
* Input  : fragmentList - fragment list
*          variable     - iterator variable (type FragmentNode)
*          condition    - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*            FRAGMENTLIST_ITERATEX(list,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define FRAGMENTLIST_ITERATEX(fragmentList,variable,condition) \
  for ((variable) = (fragmentList)->head; \
       ((variable) != NULL) && (condition); \
       (variable) = (variable)->next \
      )

#ifndef NDEBUG
  #define FragmentList_init(...)     __FragmentList_init    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define FragmentList_initNode(...) __FragmentList_initNode(__FILE__,__LINE__, ## __VA_ARGS__)
  #define FragmentList_add(...)      __FragmentList_add     (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : FragmentList_init
* Purpose: init fragment list
* Input  : fragmentList - fragment list
* Output : fragmentList - initialize fragment list
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void FragmentList_init(FragmentList *fragmentList);
#else /* not NDEBUG */
void __FragmentList_init(const char   *__fileName__,
                         ulong        __lineNb__,
                         FragmentList *fragmentList
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : FragmentList_done
* Purpose: free all nodes and deinitialize fragment list
* Input  : fragmentList - fragment list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_done(FragmentList *fragmentList);

/***********************************************************************\
* Name   : FragmentList_initNode
* Purpose: init fragment node
* Input  : name         - name of fragment
*          size         - size of fragment
*          userData     - user data to store with fragment (will be
*                         copied!)
*          userDataSize - size of user data
*          lockCount    - lock count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void FragmentList_initNode(FragmentNode *fragmentNode,
                           ConstString  name,
                           uint64       size,
                           const void   *userData,
                           uint         userDataSize,
                           uint         lockCount
                          );
#else /* not NDEBUG */
void __FragmentList_initNode(const char   *__fileName__,
                             ulong        __lineNb__,
                             FragmentNode *fragmentNode,
                             ConstString  name,
                             uint64       size,
                             const void   *userData,
                             uint         userDataSize,
                             uint         lockCount
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : FragmentList_doneNode
* Purpose: done fragment node
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_doneNode(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_lockNode
* Purpose: lock fragment node
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_lockNode(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_unlockNode
* Purpose: unlock fragment node
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_unlockNode(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_add
* Purpose: add new fragment to fragment list
* Input  : fragmentList - fragment list
*          name         - name of fragment
*          size         - size of fragment
*          userData     - user data to store with fragment (will be
*                         copied!)
*          userDataSize - size of user data
*          lockCount    - lock count
* Output : -
* Return : fragment node or NULL on error
* Notes  :
\***********************************************************************/

#ifdef NDEBUG
FragmentNode *FragmentList_add(FragmentList *fragmentList,
                               ConstString  name,
                               uint64       size,
                               const void   *userData,
                               uint         userDataSize,
                               uint         lockCount
                              );
#else /* not NDEBUG */
FragmentNode *__FragmentList_add(const char   *__fileName__,
                                 ulong        __lineNb__,
                                 FragmentList *fragmentList,
                                 ConstString  name,
                                 uint64       size,
                                 const void   *userData,
                                 uint         userDataSize,
                                 uint         lockCount
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : FragmentList_discard
* Purpose: remove fragment from fragment list and free all resources
* Input  : fragmentList - fragment list
*          fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_discard(FragmentList *fragmentList, FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_find
* Purpose: find fragment in fragment list
* Input  : fragmentList - fragment list
*          name         - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE FragmentNode *FragmentList_find(const FragmentList *fragmentList, ConstString name);
#if defined(NDEBUG) || defined(__FRAGMENTLISTS_IMPLEMENTATION__)
INLINE FragmentNode *FragmentList_find(const FragmentList *fragmentList, ConstString name)
{
  FragmentNode *fragmentNode;

  assert(fragmentList != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fragmentList);
  assert(name != NULL);

  return LIST_FIND(fragmentList,fragmentNode,String_equals(fragmentNode->name,name));
}
#endif /* NDEBUG || __FRAGMENTLISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : FragmentList_clearRanges
* Purpose: clear all ranges of fragment
* Input  : fragmentNode - fragment node to clear
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_clearRanges(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_addRange
* Purpose: add a range to a fragment
* Input  : fragmentNode - fragment node
*          offset       - range offset (0..n)
*          length       - range length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_addRange(FragmentNode *fragmentNode,
                           uint64       offset,
                           uint64       length
                          );

/***********************************************************************\
* Name   : FragmentList_rangeExists
* Purpose: check if range already exists in fragment
* Input  : fragmentNode - fragment node
*          offset       - range offset (0..n)
*          length       - range length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool FragmentList_rangeExists(const FragmentNode *fragmentNode,
                              uint64             offset,
                              uint64             length
                             );

/***********************************************************************\
* Name   : FragmentList_isComplete
* Purpose: check if fragment is completed (no fragmentation)
* Input  : fragmentNode - fragment node
* Output : -
* Return : TRUE if fragment completed (no fragmented), FALSE otherwise
* Notes  : -
\***********************************************************************/

bool FragmentList_isComplete(const FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_getSize
* Purpose: get fragment ranges size
* Input  : fragmentNode - fragment node
* Output : -
* Return : range size
* Notes  : -
\***********************************************************************/

INLINE uint64 FragmentList_getSize(const FragmentNode *fragmentNode);
#if defined(NDEBUG) || defined(__FRAGMENTLISTS_IMPLEMENTATION__)
INLINE uint64 FragmentList_getSize(const FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  return fragmentNode->rangeListSum;
}
#endif /* NDEBUG || __FRAGMENTLISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : FragmentList_getTotalSize
* Purpose: get fragment total size
* Input  : fragmentNode - fragment node
* Output : -
* Return : fragment total size
* Notes  : -
\***********************************************************************/

INLINE uint64 FragmentList_getTotalSize(const FragmentNode *fragmentNode);
#if defined(NDEBUG) || defined(__FRAGMENTLISTS_IMPLEMENTATION__)
INLINE uint64 FragmentList_getTotalSize(const FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  return fragmentNode->size;
}
#endif /* NDEBUG || __FRAGMENTLISTS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : FragmentList_print
* Purpose: print fragment entries of fragment
* Input  : outputHandle - output handle
*          indent       - indention
*          fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_print(FILE               *outputHandle,
                        uint               indent,
                        const FragmentNode *fragmentNode,
                        bool               printMissingFlag
                       );

#ifndef NDEBUG
/***********************************************************************\
* Name   : FragmentList_debugPrintInfo
* Purpose: fragment list debug function: output info to fragment entries
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_debugPrintInfo(const FragmentNode *fragmentNode, const char *name);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FRAGMENT_LISTS__ */

/* end of file */
