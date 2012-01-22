/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/fragmentlists.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver fragment list functions
* Systems: all
*
\***********************************************************************/

#ifndef __FRAGMENT_LISTS__
#define __FRAGMENT_LISTS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct FragmentEntryNode
{
  LIST_NODE_HEADER(struct FragmentEntryNode);

  uint64 offset;                        // fragment offset (0..n)
  uint64 length;                        // length of fragment
} FragmentEntryNode;

typedef struct
{
  LIST_HEADER(FragmentEntryNode);
} FragmentEntryList;

typedef struct FragmentNode
{
  LIST_NODE_HEADER(struct FragmentNode);

  String            name;               // fragment name
  uint64            size;               // size of fragment
  void              *userData;
  uint              userDataSize;
  FragmentEntryList fragmentEntryList;
} FragmentNode;

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
* Notes  : usage:
*            LIST_ITERATE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define FRAGMENTLIST_ITERATE(fragmentList,variable) \
  for ((variable) = (fragmentList)->head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

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

void FragmentList_init(FragmentList *fragmentList);

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
* Name   : FragmentList_add
* Purpose: add new fragment to fragment list
* Input  : fragmentList - fragment list
*          name         - name of fragment
*          size         - size of fragment
*          userData     - user data to store with fragment (will be
*                         copied!)
*          userDataSize - size of user data
* Output : -
* Return : fragment node or NULL on error
* Notes  :
\***********************************************************************/

FragmentNode *FragmentList_add(FragmentList   *fragmentList,
                               const String   name,
                               uint64         size,
                               const void     *userData,
                               uint           userDataSize
                              );

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

FragmentNode *FragmentList_find(FragmentList *fragmentList, const String name);

/***********************************************************************\
* Name   : FragmentList_clearEntry
* Purpose: clear all fragment entries of fragment
* Input  : fragmentNode - fragment node to clear
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_clearEntry(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_addEntry
* Purpose: add a fragment entry to a fragment
* Input  : fragmentNode - fragment node
*          offset       - fragment offset (0..n)
*          length       - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_addEntry(FragmentNode *fragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FragmentList_entryExists
* Purpose: check if fragment entry already exists in fragment
* Input  : fragmentNode - fragment node
*          offset       - fragment offset (0..n)
*          length       - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool FragmentList_entryExists(FragmentNode *fragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FragmentList_isEntryComplete
* Purpose: check if fragment is completed (no fragmentation)
* Input  : fragmentNode - fragment node
* Output : -
* Return : TRUE if fragment completed (no fragmented), FALSE otherwise
* Notes  : -
\***********************************************************************/

bool FragmentList_isEntryComplete(FragmentNode *fragmentNode);

#ifndef NDEBUG
/***********************************************************************\
* Name   : FragmentList_debugPrintInfo
* Purpose: fragment list debug function: output info to fragment entries
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_debugPrintInfo(FragmentNode *fragmentNode, const char *name);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FRAGMENT_LISTS__ */

/* end of file */
