/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// get begin/end index i0,i1
#define I0(offset,length) (offset)
#define I1(offset,length) (((length)>0)?(offset)+(length)-1:(offset))

// begin/end index f0,f1 of fragment
#define F0(fragmentRangeNode) I0(fragmentRangeNode->offset,fragmentRangeNode->length)
#define F1(fragmentRangeNode) I1(fragmentRangeNode->offset,fragmentRangeNode->length)

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
* Purpose: check if fragment node is valid
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : debug only
\***********************************************************************/

LOCAL void fragmentNodeValid(const FragmentNode *fragmentNode)
{
  uint64                  size;
  const FragmentRangeNode *fragmentRangeNode;

  size = 0LL;
  LIST_ITERATE(&fragmentNode->rangeList,fragmentRangeNode)
  {
    size += fragmentRangeNode->length;
  }
  assert(size == fragmentNode->rangeListSum);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : freeFragmentNode
* Purpose: free fragment node
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFragmentNode(FragmentNode *fragmentNode, void *userData)
{
  assert(fragmentNode != NULL);

  UNUSED_VARIABLE(userData);

  List_done(&fragmentNode->rangeList,NULL,NULL);
  if (fragmentNode->userData != NULL)
  {
    free(fragmentNode->userData);
  }
  String_delete(fragmentNode->name);
}

/***********************************************************************\
* Name   : printSpaces
* Purpose: print spaces
* Input  : outputHandle - output file handle
*          n            - number of spaces to print
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpaces(FILE *outputHandle, uint n)
{
  const char *SPACES8 = "        ";

  uint   z;
  size_t bytesWritten;

  assert(outputHandle != NULL);

  z = 0;
  while ((z+8) < n)
  {
    bytesWritten = fwrite(SPACES8,1,8,outputHandle);
    z += 8;
  }
  while (z < n)
  {
    (void)fputc(' ',outputHandle);
    z++;
  }

  UNUSED_VARIABLE(bytesWritten);
}

/*---------------------------------------------------------------------*/

void FragmentList_init(FragmentList *fragmentList)
{
  assert(fragmentList != NULL);

  List_init(fragmentList);
}

void FragmentList_done(FragmentList *fragmentList)
{
  assert(fragmentList != NULL);

  List_done(fragmentList,(ListNodeFreeFunction)freeFragmentNode,NULL);
}

void FragmentList_initNode(FragmentNode *fragmentNode,
                           ConstString  name,
                           uint64       size,
                           const void   *userData,
                           uint         userDataSize
                          )
{
  assert(fragmentNode != NULL);

  fragmentNode->name = String_duplicate(name);
  fragmentNode->size = size;
  if (userData != NULL)
  {
    fragmentNode->userData = malloc(userDataSize);
    if (fragmentNode->userData == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    memcpy(fragmentNode->userData,userData,userDataSize);
    fragmentNode->userDataSize = userDataSize;
  }
  else
  {
    fragmentNode->userData     = NULL;
    fragmentNode->userDataSize = 0;
  }
  List_init(&fragmentNode->rangeList);
  fragmentNode->rangeListSum = 0LL;
}

void FragmentList_doneNode(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  freeFragmentNode(fragmentNode,NULL);
}

FragmentNode *FragmentList_add(FragmentList *fragmentList,
                               ConstString  name,
                               uint64       size,
                               const void   *userData,
                               uint         userDataSize
                              )
{
  FragmentNode *fragmentNode;

  assert(fragmentList != NULL);
  assert(name != NULL);

  fragmentNode = LIST_NEW_NODE(FragmentNode);
  if (fragmentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  FragmentList_initNode(fragmentNode,name,size,userData,userDataSize);

  List_append(fragmentList,fragmentNode);

  return fragmentNode;
}

void FragmentList_discard(FragmentList *fragmentList, FragmentNode *fragmentNode)
{
  assert(fragmentList != NULL);
  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  List_remove(fragmentList,fragmentNode);
  FragmentList_doneNode(fragmentNode);
  LIST_DELETE_NODE(fragmentNode);
}

FragmentNode *FragmentList_find(const FragmentList *fragmentList, ConstString name)
{
  FragmentNode *fragmentNode;

  assert(fragmentList != NULL);
  assert(name != NULL);

  return LIST_FIND(fragmentList,fragmentNode,String_equals(fragmentNode->name,name));
}

void FragmentList_clearRanges(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  List_done(&fragmentNode->rangeList,NULL,NULL);
}

void FragmentList_addRange(FragmentNode *fragmentNode,
                           uint64       offset,
                           uint64       length
                          )
{
  FragmentRangeNode *fragmentRangeNode,*deleteFragmentRangeNode;
  FragmentRangeNode *prevFragmentRangeNode,*nextFragmentRangeNode;

  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  // remove all fragments which are completely covered by new fragment
  fragmentRangeNode = fragmentNode->rangeList.head;
  while (fragmentRangeNode != NULL)
  {
    if ((F0(fragmentRangeNode) >= I0(offset,length)) && (F1(fragmentRangeNode) <= I1(offset,length)))
    {
      deleteFragmentRangeNode = fragmentRangeNode;
      fragmentRangeNode = fragmentRangeNode->next;
      List_remove(&fragmentNode->rangeList,deleteFragmentRangeNode);
      assert(fragmentNode->rangeListSum >= deleteFragmentRangeNode->length);
      fragmentNode->rangeListSum -= deleteFragmentRangeNode->length;
      LIST_DELETE_NODE(deleteFragmentRangeNode);
    }
    else
    {
      fragmentRangeNode = fragmentRangeNode->next;
    }
  }

  // find prev/next fragment
  prevFragmentRangeNode = NULL;
  fragmentRangeNode = fragmentNode->rangeList.head;
  while ((fragmentRangeNode != NULL) && (F1(fragmentRangeNode) <= I1(offset,length)))
  {
    prevFragmentRangeNode = fragmentRangeNode;
    fragmentRangeNode = fragmentRangeNode->next;
  }
  nextFragmentRangeNode = NULL;
  fragmentRangeNode = fragmentNode->rangeList.tail;
  while ((fragmentRangeNode != NULL) && (F0(fragmentRangeNode) >= I0(offset,length)))
  {
    nextFragmentRangeNode = fragmentRangeNode;
    fragmentRangeNode = fragmentRangeNode->prev;
  }

  // check if existing fragment can be extended or new fragment have to be inserted
  if (   ((prevFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= I0(offset,length)))
      || ((nextFragmentRangeNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentRangeNode)))
     )
  {
    if      ((prevFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= I0(offset,length)))
    {
      // combine with previous existing fragment
      prevFragmentRangeNode->length = (offset+length)-prevFragmentRangeNode->offset;
      prevFragmentRangeNode->offset = prevFragmentRangeNode->offset;
      fragmentNode->rangeListSum += length;
    }
    else if ((nextFragmentRangeNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentRangeNode)))
    {
      // combine with next existing fragment
      nextFragmentRangeNode->length = (nextFragmentRangeNode->offset+nextFragmentRangeNode->length)-offset;
      nextFragmentRangeNode->offset = offset;
      fragmentNode->rangeListSum += length;
    }

    if ((prevFragmentRangeNode != NULL) && (nextFragmentRangeNode != NULL) && (F1(prevFragmentRangeNode)+1 >= F0(nextFragmentRangeNode)))
    {
      // combine previous and next fragment
      prevFragmentRangeNode->length += nextFragmentRangeNode->length;
      List_remove(&fragmentNode->rangeList,nextFragmentRangeNode);
      LIST_DELETE_NODE(nextFragmentRangeNode);
    }
  }
  else
//  else if (   ((prevFragmentRangeNode == NULL) || (F1(prevFragmentRangeNode)+1 < I0(offset,length)))
//           && ((nextFragmentRangeNode == NULL) || (F0(nextFragmentRangeNode)-1 > I1(offset,length)))
//          )
  {
    // insert new fragment
    fragmentRangeNode = LIST_NEW_NODE(FragmentRangeNode);
    if (fragmentRangeNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    fragmentRangeNode->offset = offset;
    fragmentRangeNode->length = length;
    List_insert(&fragmentNode->rangeList,fragmentRangeNode,nextFragmentRangeNode);
    fragmentNode->rangeListSum += length;
  }

  FRAGMENTNODE_VALID(fragmentNode);
}

bool FragmentList_rangeExists(const FragmentNode *fragmentNode,
                              uint64             offset,
                              uint64             length
                             )
{
  bool              existsFlag;
  uint64            i0,i1;
  FragmentRangeNode *fragmentRangeNode;

  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  i0 = I0(offset,length);
  i1 = I1(offset,length);

  existsFlag = FALSE;
  for (fragmentRangeNode = fragmentNode->rangeList.head; (fragmentRangeNode != NULL) && !existsFlag; fragmentRangeNode = fragmentRangeNode->next)
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

  return    (fragmentNode->size == 0)
         || (   (List_count(&fragmentNode->rangeList) == 1)
             && (fragmentNode->rangeList.head->offset == 0)
             && (fragmentNode->rangeList.head->length >= fragmentNode->size)
            );
}

void FragmentList_print(FILE               *outputHandle,
                        uint               indent,
                        const FragmentNode *fragmentNode,
                        bool               printMissingFlag
                       )
{
  FragmentRangeNode *fragmentRangeNode;
  uint64            offset0,offset1;
  uint64            lastOffset;

  assert(fragmentNode != NULL);
  FRAGMENTNODE_VALID(fragmentNode);

  lastOffset = 0LL;
  LIST_ITERATE(&fragmentNode->rangeList,fragmentRangeNode)
//  for (fragmentRangeNode = fragmentNode->rangeList.head; fragmentRangeNode != NULL; fragmentRangeNode = fragmentRangeNode->next)
  {
    offset0 = F0(fragmentRangeNode);
    offset1 = F1(fragmentRangeNode);
    if (printMissingFlag)
    {
      if ((lastOffset+1) < offset0)
      {
        printSpaces(outputHandle,indent); fprintf(outputHandle,"%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes: missing\n",lastOffset,offset0-1,offset0-lastOffset+1);
      }
      printSpaces(outputHandle,indent); fprintf(outputHandle,"%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes: OK\n",offset0,offset1,offset1-offset0+1);
      lastOffset = offset1;
    }
    else
    {
      printSpaces(outputHandle,indent); fprintf(outputHandle,"%15"PRIu64"..%15"PRIu64" %15"PRIu64" bytes\n",offset0,offset1,offset1-offset0+1);
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

/* end of file */
