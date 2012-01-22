/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/fragmentlists.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver fragment list functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "fragmentlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define I0(offset,length) (offset)
#define I1(offset,length) (((length)>0)?(offset)+(length)-1:(offset))

#define F0(fragmentEntryNode) I0(fragmentEntryNode->offset,fragmentEntryNode->length)
#define F1(fragmentEntryNode) I1(fragmentEntryNode->offset,fragmentEntryNode->length)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

  List_done(&fragmentNode->fragmentEntryList,NULL,NULL);
  if (fragmentNode->userData != NULL)
  {
    free(fragmentNode->userData);
  }
  String_delete(fragmentNode->name);
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

FragmentNode *FragmentList_add(FragmentList   *fragmentList,
                               const String   name,
                               uint64         size,
                               const void     *userData,
                               uint           userDataSize
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
  List_init(&fragmentNode->fragmentEntryList);

  List_append(fragmentList,fragmentNode);

  return fragmentNode;
}

void FragmentList_discard(FragmentList *fragmentList, FragmentNode *fragmentNode)
{
  assert(fragmentList != NULL);
  assert(fragmentNode != NULL);

  List_remove(fragmentList,fragmentNode);
  freeFragmentNode(fragmentNode,NULL);
  LIST_DELETE_NODE(fragmentNode);
}

FragmentNode *FragmentList_find(FragmentList *fragmentList, const String name)
{
  FragmentNode *fragmentNode;

  assert(fragmentList != NULL);
  assert(name != NULL);

  fragmentNode = fragmentList->head;
  while ((fragmentNode != NULL) && (!String_equals(fragmentNode->name,name)))
  {
    fragmentNode = fragmentNode->next;
  }

  return fragmentNode;
}

void FragmentList_clearEntry(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  List_done(&fragmentNode->fragmentEntryList,NULL,NULL);
}

void FragmentList_addEntry(FragmentNode *fragmentNode, uint64 offset, uint64 length)
{
  FragmentEntryNode *fragmentEntryNode,*deleteFragmentEntryNode;
  FragmentEntryNode *prevFragmentEntryNode,*nextFragmentEntryNode;

  assert(fragmentNode != NULL);

  // remove all fragments which are completely covered by new fragment
  fragmentEntryNode = fragmentNode->fragmentEntryList.head;
  while (fragmentEntryNode != NULL)
  {
    if ((F0(fragmentEntryNode) >= I0(offset,length)) && (F1(fragmentEntryNode) <= I1(offset,length)))
    {
      deleteFragmentEntryNode = fragmentEntryNode;
      fragmentEntryNode = fragmentEntryNode->next;
      List_remove(&fragmentNode->fragmentEntryList,deleteFragmentEntryNode);
      LIST_DELETE_NODE(deleteFragmentEntryNode);
    }
    else
    {
      fragmentEntryNode = fragmentEntryNode->next;
    }
  }

  // find prev/next fragment
  prevFragmentEntryNode = NULL;
  fragmentEntryNode = fragmentNode->fragmentEntryList.head;
  while ((fragmentEntryNode != NULL) && (F1(fragmentEntryNode) < I1(offset,length)))
  {
    prevFragmentEntryNode = fragmentEntryNode;
    fragmentEntryNode = fragmentEntryNode->next;
  }
  nextFragmentEntryNode = NULL;
  fragmentEntryNode = fragmentNode->fragmentEntryList.tail;
  while ((fragmentEntryNode != NULL) && (F0(fragmentEntryNode) > I0(offset,length)))
  {
    nextFragmentEntryNode = fragmentEntryNode;
    fragmentEntryNode = fragmentEntryNode->prev;
  }

  // check if existing fragment can be extended or new fragment have to be inserted
  if      ((prevFragmentEntryNode != NULL) && (F1(prevFragmentEntryNode)+1 >= I0(offset,length)))
  {
    // combine with previous existing fragment
    prevFragmentEntryNode->length = (offset+length)-prevFragmentEntryNode->offset;
    prevFragmentEntryNode->offset = prevFragmentEntryNode->offset;
  }
  else if ((nextFragmentEntryNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentEntryNode)))
  {
    // combine with next existing fragment
    nextFragmentEntryNode->length = (nextFragmentEntryNode->offset+nextFragmentEntryNode->length)-offset;
    nextFragmentEntryNode->offset = offset;
  }
  else
  {
    // insert new fragment
    fragmentEntryNode = LIST_NEW_NODE(FragmentEntryNode);
    if (fragmentEntryNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    fragmentEntryNode->offset = offset;
    fragmentEntryNode->length = length;
    List_insert(&fragmentNode->fragmentEntryList,fragmentEntryNode,nextFragmentEntryNode);
  }
}

bool FragmentList_entryExists(FragmentNode *fragmentNode, uint64 offset, uint64 length)
{
  bool              existsFlag;
  uint64            i0,i1;
  FragmentEntryNode *fragmentEntryNode;

  assert(fragmentNode != NULL);

  i0 = I0(offset,length);
  i1 = I1(offset,length);

  existsFlag = FALSE;
  for (fragmentEntryNode = fragmentNode->fragmentEntryList.head; (fragmentEntryNode != NULL) && !existsFlag; fragmentEntryNode = fragmentEntryNode->next)
  {
    if (   ((F0(fragmentEntryNode) <= i0) && (i0 <= F1(fragmentEntryNode)) )
        || ((F0(fragmentEntryNode) <= i1) && (i1 <= F1(fragmentEntryNode)))
       )
    {
      existsFlag = TRUE;
    }
  }

  return existsFlag;
}

bool FragmentList_isEntryComplete(FragmentNode *fragmentNode)
{
  assert(fragmentNode != NULL);

  return    (fragmentNode->size == 0)
         || (   (List_count(&fragmentNode->fragmentEntryList) == 1)
             && (fragmentNode->fragmentEntryList.head->offset == 0)
             && (fragmentNode->fragmentEntryList.head->length >= fragmentNode->size)
            );
}

#ifndef NDEBUG
void FragmentList_debugPrintInfo(FragmentNode *fragmentNode, const char *name)
{
  FragmentEntryNode *fragmentEntryNode;

  printf("Fragments '%s':\n",name);
  for (fragmentEntryNode = fragmentNode->fragmentEntryList.head; fragmentEntryNode != NULL; fragmentEntryNode = fragmentEntryNode->next)
  {
    printf("  %8llu..%8llu\n",F0(fragmentEntryNode),F1(fragmentEntryNode));
  }
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
