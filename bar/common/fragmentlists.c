/***********************************************************************\
*
* Contents: Fragment list functions
* Systems: all
*
\***********************************************************************/

#define __FRAGMENTLISTS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"

#include "fragmentlists.h"

/****************** Conditional compilation switches *******************/
#define _FRAGMENTLISTS_DEBUG

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// get begin/end index i0,i1 of range
#define I0(offset,length) (offset)
#define I1(offset,length) (((length)>0)?(offset)+(length)-1:(offset))

// begin/end index f0,f1/length of fragment
#define F0(fragmentRangeNode) I0(fragmentRangeNode->offset,fragmentRangeNode->length)
#define F1(fragmentRangeNode) I1(fragmentRangeNode->offset,fragmentRangeNode->length)
#define L(fragmentRangeNode) (fragmentRangeNode->length)

#ifndef NDEBUG
  #define FRAGMENTNODE_VALID(fragmentNode) \
    do \
    { \
      fragmentNodeValid(fragmentNode); \
    } \
    while (0)
#else /* not NDEBUG */
  #define FRAGMENTNODE_VALID(fragmentNode) \
    do \
    { \
    } \
    while (0)
#endif /* NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG

/***********************************************************************\
* Name   : fragmentNodeValid
* Purpose: check if fragment node is valid: sum of fragments
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : debug only
\***********************************************************************/

LOCAL void fragmentNodeValid(const FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  uint64                  size = 0LL;
  const FragmentRangeNode *fragmentRangeNode;
  LIST_ITERATE(&fragmentNode->rangeList,fragmentRangeNode)
  {
    size += fragmentRangeNode->length;
  }
  assert(size == fragmentNode->rangeListSum);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : fragmentNodeDone
* Purpose: done fragment node
* Input  : fragmentNode - fragment node
*          userData     - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fragmentNodeDone(FragmentNode *fragmentNode, void *userData)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  UNUSED_VARIABLE(userData);

  List_done(&fragmentNode->rangeList);
  if (fragmentNode->data != NULL)
  {
    if (fragmentNode->freeDataFunction != NULL)
    {
      fragmentNode->freeDataFunction(fragmentNode->data,
                                     fragmentNode->dataSize,
                                     fragmentNode->freeDataUserData
                                    );
    }
    free(fragmentNode->data);
  }
  String_delete(fragmentNode->name);
}

/***********************************************************************\
* Name   : freeFragmentNode
* Purpose: free fragment node
* Input  : fargmentNode - fragment node to free
*          userData     - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFragmentNode(FragmentNode *fragmentNode, void *userData)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  UNUSED_VARIABLE(userData);

  DEBUG_REMOVE_RESOURCE_TRACE(fragmentNode,FragmentNode);

  fragmentNodeDone(fragmentNode,NULL);
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
void FragmentList_init(FragmentList *fragmentList)
#else /* not NDEBUG */
void __FragmentList_init(const char   *__fileName__,
                         size_t       __lineNb__,
                         FragmentList *fragmentList
                        )
#endif /* NDEBUG */
{
  assert(fragmentList != NULL);

  List_init(fragmentList,
            CALLBACK_(NULL,NULL),
            CALLBACK_((ListNodeFreeFunction)freeFragmentNode,NULL)
           );

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(fragmentList,FragmentList);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,fragmentList,FragmentList);
  #endif /* NDEBUG */
}

void FragmentList_done(FragmentList *fragmentList)
{
  assert(fragmentList != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fragmentList);

  DEBUG_REMOVE_RESOURCE_TRACE(fragmentList,FragmentList);

  List_done(fragmentList);
}

#ifdef NDEBUG
void FragmentList_initNode(FragmentNode                 *fragmentNode,
                           ConstString                  name,
                           uint64                       size,
                           const void                   *data,
                           size_t                       dataSize,
                           size_t                       lockCount,
                           FragmentNodeFreeDataFunction freeDataFunction,
                           void                         *freeDataUserData
                          )
#else /* not NDEBUG */
void __FragmentList_initNode(const char                   *__fileName__,
                             size_t                       __lineNb__,
                             FragmentNode                 *fragmentNode,
                             ConstString                  name,
                             uint64                       size,
                             const void                   *data,
                             size_t                       dataSize,
                             size_t                       lockCount,
                             FragmentNodeFreeDataFunction freeDataFunction,
                             void                         *freeDataUserData
                            )
#endif /* NDEBUG */
{
  assert(fragmentNode != NULL);
  assert((data != NULL) || (freeDataFunction == NULL));

  fragmentNode->name             = String_duplicate(name);
  fragmentNode->size             = size;
  if (data != NULL)
  {
    fragmentNode->data = malloc(dataSize);
    if (fragmentNode->data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    memcpy(fragmentNode->data,data,dataSize);
    fragmentNode->dataSize = dataSize;
  }
  else
  {
    fragmentNode->data     = NULL;
    fragmentNode->dataSize = 0;
  }
  fragmentNode->lockCount        = lockCount;
  fragmentNode->freeDataFunction = freeDataFunction;
  fragmentNode->freeDataUserData = freeDataUserData;
  List_init(&fragmentNode->rangeList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  fragmentNode->rangeListSum = 0LL;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(fragmentNode,FragmentNode);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,fragmentNode,FragmentNode);
  #endif /* NDEBUG */
}

void FragmentList_doneNode(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  DEBUG_REMOVE_RESOURCE_TRACE(fragmentNode,FragmentNode);

  fragmentNodeDone(fragmentNode,NULL);
}

void FragmentList_lockNode(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  ATOMIC_INCREMENT(fragmentNode->lockCount);
}

void FragmentList_unlockNode(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);
  assert(fragmentNode->lockCount > 0);

  ATOMIC_DECREMENT(fragmentNode->lockCount);
}

#ifdef NDEBUG
FragmentNode *FragmentList_add(FragmentList                 *fragmentList,
                               ConstString                  name,
                               uint64                       size,
                               const void                   *data,
                               size_t                       dataSize,
                               size_t                       lockCount,
                               FragmentNodeFreeDataFunction freeDataFunction,
                               void                        *freeDataUserData
                              )
#else /* not NDEBUG */
FragmentNode *__FragmentList_add(const char                   *__fileName__,
                                 size_t                       __lineNb__,
                                 FragmentList                 *fragmentList,
                                 ConstString                  name,
                                 uint64                       size,
                                 const void                   *data,
                                 size_t                       dataSize,
                                 size_t                       lockCount,
                                 FragmentNodeFreeDataFunction freeDataFunction,
                                 void                         *freeDataUserData
                                )
#endif /* NDEBUG */
{
  assert(fragmentList != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fragmentList);
  assert(name != NULL);
  assert((data != NULL) || (freeDataFunction == NULL));

  FragmentNode *fragmentNode = LIST_NEW_NODE(FragmentNode);
  if (fragmentNode != NULL)
  {
    #ifdef NDEBUG
      FragmentList_initNode(fragmentNode,
                            name,
                            size,
                            data,
                            dataSize,
                            lockCount,
                            freeDataFunction,
                            freeDataUserData
                           );
    #else /* not NDEBUG */
      __FragmentList_initNode(__fileName__,__lineNb__,
                              fragmentNode,
                              name,
                              size,
                              data,
                              dataSize,
                              lockCount,
                              freeDataFunction,
                              freeDataUserData
                             );
    #endif /* NDEBUG */
    List_append(fragmentList,fragmentNode);
  }

  return fragmentNode;
}

void FragmentList_discard(FragmentList *fragmentList, FragmentNode *fragmentNode)
{
  assert(fragmentList != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fragmentList);
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  DEBUG_REMOVE_RESOURCE_TRACE(fragmentNode,FragmentNode);

  List_remove(fragmentList,fragmentNode);
  fragmentNodeDone(fragmentNode,NULL);
  LIST_DELETE_NODE(fragmentNode);
}

FragmentNode *FragmentList_find(const FragmentList *fragmentList, ConstString name)
{
  assert(fragmentList != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fragmentList);
  assert(name != NULL);

  FragmentNode *fragmentNode;
  return LIST_FIND(fragmentList,fragmentNode,String_equals(fragmentNode->name,name));
}

void FragmentList_clearRanges(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  List_done(&fragmentNode->rangeList);
  fragmentNode->rangeListSum = 0LL;
}

void FragmentList_addRange(FragmentNode *fragmentNode,
                           uint64       offset,
                           uint64       length
                          )
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  if (length > 0)
  {
    #ifdef FRAGMENTLISTS_DEBUG
      FragmentList_debugPrintInfo(fragmentNode,"before");
    #endif /* FRAGMENTLISTS_DEBUG */

    FragmentRangeNode *fragmentRangeNode;

    // remove all fragments which are completely covered by new fragment
    fragmentRangeNode = fragmentNode->rangeList.head;
    while (fragmentRangeNode != NULL)
    {
      if ((F0(fragmentRangeNode) >= I0(offset,length)) && (F1(fragmentRangeNode) <= I1(offset,length)))
      {
        FragmentRangeNode *deleteFragmentRangeNode = fragmentRangeNode;
        fragmentRangeNode = fragmentRangeNode->next;
        List_remove(&fragmentNode->rangeList,deleteFragmentRangeNode);
        assert(fragmentNode->rangeListSum >= deleteFragmentRangeNode->length);
        fragmentNode->rangeListSum -= deleteFragmentRangeNode->length;
        #ifdef FRAGMENTLISTS_DEBUG
          fprintf(stderr,"%s, %d: removed %lu:%lu\n",__FILE__,__LINE__,deleteFragmentRangeNode->offset,deleteFragmentRangeNode->length);
        #endif /* FRAGMENTLISTS_DEBUG */
        LIST_DELETE_NODE(deleteFragmentRangeNode);
      }
      else
      {
        fragmentRangeNode = fragmentRangeNode->next;
      }
    }

    // find prev/next fragment
    FragmentRangeNode *prevFragmentRangeNode = NULL;
    fragmentRangeNode = fragmentNode->rangeList.head;
    while ((fragmentRangeNode != NULL) && (F1(fragmentRangeNode) <= I1(offset,length)))
    {
      prevFragmentRangeNode = fragmentRangeNode;
      fragmentRangeNode = fragmentRangeNode->next;
    }
    FragmentRangeNode *nextFragmentRangeNode = NULL;
    fragmentRangeNode = fragmentNode->rangeList.tail;
    while ((fragmentRangeNode != NULL) && (F0(fragmentRangeNode) >= I0(offset,length)))
    {
      nextFragmentRangeNode = fragmentRangeNode;
      fragmentRangeNode = fragmentRangeNode->prev;
    }

    // check if existing fragment range can be extended or new fragment range have to be inserted
    if (   ((prevFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= I0(offset,length)))
        || ((nextFragmentRangeNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentRangeNode)))
       )
    {
      if      ((prevFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= I0(offset,length)))
      {
        // combine with previous existing fragment range
        #ifdef FRAGMENTLISTS_DEBUG
          fprintf(stderr,"%s, %d: combine %lu:%lu: prev %lu:%lu: o(p)=%lu I1(p)=%lu\n",__FILE__,__LINE__,
                  offset,length,
                  prevFragmentRangeNode->offset,
                  prevFragmentRangeNode->length,
                  prevFragmentRangeNode->offset,I1(offset,length)
                 );
        #endif /* FRAGMENTLISTS_DEBUG */
        fragmentNode->rangeListSum += I1(offset,length)+1-F0(prevFragmentRangeNode)-L(prevFragmentRangeNode);
        prevFragmentRangeNode->length = I1(offset,length)+1-F0(prevFragmentRangeNode);
        prevFragmentRangeNode->offset = F0(prevFragmentRangeNode);

        assert((F1(prevFragmentRangeNode)-F0(prevFragmentRangeNode)+1) == prevFragmentRangeNode->length);
      }
      else if ((nextFragmentRangeNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentRangeNode)))
      {
        // combine with next existing fragment range
        #ifdef FRAGMENTLISTS_DEBUG
        fprintf(stderr,"%s, %d: combine %lu:%lu: next %lu:%lu: o(n)=%lu I1(n)=%lu\n",__FILE__,__LINE__,
                offset,length,
                nextFragmentRangeNode->offset,
                nextFragmentRangeNode->length,
                nextFragmentRangeNode->offset,I1(offset,length)
               );
        #endif /* FRAGMENTLISTS_DEBUG */
        fragmentNode->rangeListSum += F1(nextFragmentRangeNode)+1-I0(offset,length)-L(nextFragmentRangeNode);
        nextFragmentRangeNode->length = F1(nextFragmentRangeNode)+1-I0(offset,length);
        nextFragmentRangeNode->offset = I0(offset,length);

        assert((F1(nextFragmentRangeNode)-F0(nextFragmentRangeNode)+1) == nextFragmentRangeNode->length);
      }
      #ifdef FRAGMENTLISTS_DEBUG
        FragmentList_debugPrintInfo(fragmentNode,"before combine");
        fprintf(stderr,"%s, %d: fragmentNode->rangeListSum=%lu\n",__FILE__,__LINE__,fragmentNode->rangeListSum);
      #endif /* FRAGMENTLISTS_DEBUG */

      if ((prevFragmentRangeNode != NULL) && (nextFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= F0(nextFragmentRangeNode)))
      {
        // combine previous and next fragment range
        #ifdef FRAGMENTLISTS_DEBUG
          fprintf(stderr,"%s, %d: combine prev+next prev=%lu:%lu next=%lu:%lu F0(p)=%lu F1(p)=%lu F0(n)=%lu F1(n)=%lu L(p)=%lu L(n)=%lu\n",__FILE__,__LINE__,
                  prevFragmentRangeNode->offset,prevFragmentRangeNode->length,
                  nextFragmentRangeNode->offset,nextFragmentRangeNode->length,
                  F0(prevFragmentRangeNode),
                  F1(prevFragmentRangeNode),
                  F0(nextFragmentRangeNode),
                  F1(nextFragmentRangeNode),
                  prevFragmentRangeNode->length,
                  nextFragmentRangeNode->length
                 );
        #endif /* FRAGMENTLISTS_DEBUG */
        #ifdef FRAGMENTLISTS_DEBUG
          fprintf(stderr,"%s, %d: fragmentNode->rangeListSum=%lu\n",__FILE__,__LINE__,fragmentNode->rangeListSum);
        #endif /* FRAGMENTLISTS_DEBUG */
        fragmentNode->rangeListSum += (F1(nextFragmentRangeNode)-F0(prevFragmentRangeNode)+1)-L(prevFragmentRangeNode)-L(nextFragmentRangeNode);
        #ifdef FRAGMENTLISTS_DEBUG
          fprintf(stderr,"%s, %d: fragmentNode->rangeListSum=%lu\n",__FILE__,__LINE__,fragmentNode->rangeListSum);
        #endif /* FRAGMENTLISTS_DEBUG */
        prevFragmentRangeNode->length = F1(nextFragmentRangeNode)-F0(prevFragmentRangeNode)+1;

        List_remove(&fragmentNode->rangeList,nextFragmentRangeNode);
        LIST_DELETE_NODE(nextFragmentRangeNode);

        assert((F1(prevFragmentRangeNode)-F0(prevFragmentRangeNode)+1) == prevFragmentRangeNode->length);
      }
    }
    else /* if (   ((prevFragmentRangeNode == NULL) || (F1(prevFragmentRangeNode)+1 < I0(offset,length)))
                && ((nextFragmentRangeNode == NULL) || (F0(nextFragmentRangeNode)-1 > I1(offset,length)))
               )
         */
    {
      // insert new fragment range
      fragmentRangeNode = LIST_NEW_NODE(FragmentRangeNode);
      if (fragmentRangeNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      fragmentRangeNode->offset = offset;
      fragmentRangeNode->length = length;
      #ifdef FRAGMENTLISTS_DEBUG
        fprintf(stderr,"%s, %d: add %lu:%lu\n",__FILE__,__LINE__,offset,length);
      #endif /* FRAGMENTLISTS_DEBUG */
      assert((F1(fragmentRangeNode)-F0(fragmentRangeNode)+1) == fragmentRangeNode->length);

      List_insert(&fragmentNode->rangeList,fragmentRangeNode,nextFragmentRangeNode);
      fragmentNode->rangeListSum += length;
      #ifdef FRAGMENTLISTS_DEBUG
        fprintf(stderr,"%s, %d: insert %lu:%lu\n",__FILE__,__LINE__,fragmentRangeNode->offset,fragmentRangeNode->length);
      #endif /* FRAGMENTLISTS_DEBUG */
    }

    #ifdef FRAGMENTLISTS_DEBUG
      FragmentList_debugPrintInfo(fragmentNode,"after");
    #endif /* FRAGMENTLISTS_DEBUG */
  }

  FRAGMENTNODE_VALID(fragmentNode);
}

bool FragmentList_rangeExists(const FragmentNode *fragmentNode,
                              uint64             offset,
                              uint64             length
                             )
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  uint64 i0 = I0(offset,length);
  uint64 i1 = I1(offset,length);

  bool existsFlag = FALSE;
  for (FragmentRangeNode *fragmentRangeNode = fragmentNode->rangeList.head; (fragmentRangeNode != NULL) && !existsFlag; fragmentRangeNode = fragmentRangeNode->next)
  {
    if (   ((F0(fragmentRangeNode) <= i0) && (i0 <= F1(fragmentRangeNode)) )
        || ((F0(fragmentRangeNode) <= i1) && (i1 <= F1(fragmentRangeNode)))
       )
    {
      existsFlag = TRUE;
    }
  }

  return existsFlag;
}

bool FragmentList_isComplete(const FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  return    (fragmentNode->lockCount <= 1)
         && (   (fragmentNode->size == 0)
             || (   (List_count(&fragmentNode->rangeList) == 1)
                 && (fragmentNode->rangeList.head->offset == 0)
                 && (fragmentNode->rangeList.head->length >= fragmentNode->size)
                )
            );
}

void FragmentList_print(FILE               *outputHandle,
                        size_t             indent,
                        const FragmentNode *fragmentNode,
                        bool               printMissingFlag
                       )
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  uint64 lastOffset = 0LL;
  FragmentRangeNode *fragmentRangeNode;
  LIST_ITERATE(&fragmentNode->rangeList,fragmentRangeNode)
  {
    uint64 offset0 = F0(fragmentRangeNode);
    uint64 offset1 = F1(fragmentRangeNode);
    if (printMissingFlag)
    {
      if ((lastOffset+1) < offset0)
      {
        fprintf(outputHandle,"%*s%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes: missing\n",(int)indent,"",lastOffset,offset0-1,offset0-1-lastOffset+1);
      }
      fprintf(outputHandle,"%*s%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes: OK\n",(int)indent,"",offset0,offset1,offset1-offset0+1);
      lastOffset = offset1;
    }
    else
    {
      fprintf(outputHandle,"%*s%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes\n",(int)indent,"",offset0,offset1,offset1-offset0+1);
    }
  }
  if ((lastOffset+1) < fragmentNode->size)
  {
    if (printMissingFlag)
    {
      fprintf(outputHandle,"%*s%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes: missing\n",(int)indent,"",lastOffset+1,fragmentNode->size-1,fragmentNode->size-1-lastOffset);
    }
  }
}

#ifndef NDEBUG
void FragmentList_debugPrintInfo(const FragmentNode *fragmentNode, const char *name)
{
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  fprintf(stdout,"Fragments '%s':\n",name);
  FragmentList_print(stdout,0,fragmentNode,FALSE);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#ifdef FRAGMENTLISTS_DEBUG
void FragmentList_unitTests()
{
  StaticString (s,32);
  String_setCString(s,"test");

  FragmentList f;
  FragmentNode *n;

#if 1
  /*  ##____
      __##__
      ____##
  */
  fprintf(stderr,"%s, %d:  1 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,2);
  FragmentList_addRange(n,2,2);
  FragmentList_addRange(n,4,2);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ____##
      __##__
      ##____
  */
  fprintf(stderr,"%s, %d:  2 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,2,2);
  FragmentList_addRange(n,0,2);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif


#if 1
  /*  ##____
      ____##
      __##__
  */
  fprintf(stderr,"%s, %d:  3 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,2);
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,2,2);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ____##
      ##____
      __##__
  */
  fprintf(stderr,"%s, %d:  4 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,0,2);
  FragmentList_addRange(n,2,2);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ###___
      __##__
      ___###
  */
  fprintf(stderr,"%s, %d:  5 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,3);
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,3,3);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ###___
      __####
  */
  fprintf(stderr,"%s, %d:  6 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,3);
  FragmentList_addRange(n,2,4);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  __####
      ###___
  */
  fprintf(stderr,"%s, %d:  7 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,2,4);
  FragmentList_addRange(n,0,3);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ___###
      __##__
      ###___
  */
  fprintf(stderr,"%s, %d:  8 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,3,3);
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,0,3);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  __##__
      ######
  */
  fprintf(stderr,"%s, %d:  9 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,0,6);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ######
      __##__
  */
  fprintf(stderr,"%s, %d: 10 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,6);
  FragmentList_addRange(n,4,2);
  assert(FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ##____
      ____##
  */
  fprintf(stderr,"%s, %d: 11 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,2);
  FragmentList_addRange(n,4,2);
  assert(!FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  ____##
      ##____
  */
  fprintf(stderr,"%s, %d: 12 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,4,2);
  FragmentList_addRange(n,0,2);
  assert(!FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  #_____
      __##__
      _____#
  */
  fprintf(stderr,"%s, %d: 13 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,0,1);
  FragmentList_addRange(n,2,2);
  FragmentList_addRange(n,5,1);
  assert(!FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif

#if 1
  /*  _____#
      __##__
      #_____
  */
  fprintf(stderr,"%s, %d: 14 -------------------------------------------\n",__FILE__,__LINE__);
  FragmentList_init(&f);
  n = FragmentList_add(&f,s,6,NULL,0,0,CALLBACK_(NULL,NULL));
  FragmentList_addRange(n,5,1);
  FragmentList_addRange(n,2,2);
  FragmentList_addRange(n,0,1);
  assert(!FragmentList_isComplete(n));
  FragmentList_done(&f);
#endif
}
#endif /* FRAGMENTLISTS_DEBUG */

/* end of file */
