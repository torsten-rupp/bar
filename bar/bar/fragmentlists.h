/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/fragmentlists.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver file fragment list functions
* Systems: all
*
\***********************************************************************/

#ifndef __FILE_FRAGMENT_LISTS__
#define __FILE_FRAGMENT_LISTS__

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
  uint64            size;               // size of file
  FragmentEntryList fragmentEntryList;
} FragmentNode;

typedef struct
{
  LIST_HEADER(FragmentNode);
} FragmentList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : FragmentList_init
* Purpose: init file fragment list
* Input  : fragmentList - fragment list
* Output : fragmentList - initialize fragment list
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_init(FragmentList *fragmentList);

/***********************************************************************\
* Name   : FragmentList_done
* Purpose: free all nodes and deinitialize file fragment list
* Input  : fragmentList - fragment list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_done(FragmentList *fragmentList);

/***********************************************************************\
* Name   : FragmentList_addFile
* Purpose: add new file to file fragmen tlist
* Input  : fragmentList - fragment list
*          fileName     - name
*          size         - size of file
* Output : -
* Return : file fragment node or NULL
* Notes  : -
\***********************************************************************/

FragmentNode *FragmentList_add(FragmentList *fragmentList, const String name, uint64 size);

/***********************************************************************\
* Name   : FragmentList_removeFile
* Purpose: remove file from file fragment list
* Input  : fragmentList - fragment list
*          fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_remove(FragmentList *fragmentList, FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_findFile
* Purpose: find file in file fragment list
* Input  : fragmentList - fragment list
*          name         - name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

FragmentNode *FragmentList_find(FragmentList *fragmentList, const String name);

/***********************************************************************\
* Name   : FragmentList_clear
* Purpose: clear fragments of file
* Input  : fragmentNode - fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_clearEntry(FragmentNode *fragmentNode);

/***********************************************************************\
* Name   : FragmentList_add
* Purpose: add a fragment entry
* Input  : fragmentNode - fragment node
*          offset       - fragment offset (0..n)
*          length       - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FragmentList_addEntry(FragmentNode *fragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FragmentList_checkEntryExists
* Purpose: check if fragment entry already exists in file
* Input  : fragmentNode - fragment node
*          offset       - fragment offset (0..n)
*          length       - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool FragmentList_checkEntryExists(FragmentNode *fragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FragmentList_checkEntryComplete
* Purpose: check if entry is completed (no fragmentation)
* Input  : fragmentNode - fragment node
* Output : -
* Return : TRUE if file completed (no fragmented), FALSE otherwise
* Notes  : -
\***********************************************************************/

bool FragmentList_checkEntryComplete(FragmentNode *fragmentNode);

#ifndef NDEBUG
void FragmentList_print(FragmentNode *fragmentNode, const char *name);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FILE_FRAGMENT_LISTS__ */

/* end of file */
