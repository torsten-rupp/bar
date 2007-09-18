/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/filefragmentlists.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver file fragment list functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "filefragmentlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define I0(offset,length) (offset)
#define I1(offset,length) (((length)>0)?(offset)+(length)-1:(offset))

#define F0(fileFragmentNode) I0(fileFragmentNode->offset,fileFragmentNode->length)
#define F1(fileFragmentNode) I1(fileFragmentNode->offset,fileFragmentNode->length)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeFileFragmentNode
* Purpose: free file fragment node
* Input  : fileFragmentNode - file fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileFragmentNode(FileFragmentNode *fileFragmentNode, void *userData)
{
  assert(fileFragmentNode != NULL);

  UNUSED_VARIABLE(userData);

  List_done(&fileFragmentNode->fragmentList,NULL,NULL);
  String_delete(fileFragmentNode->fileName);
}

/*---------------------------------------------------------------------*/

void FileFragmentList_init(FileFragmentList *fileFragmentList)
{
  assert(fileFragmentList != NULL);

  List_init(fileFragmentList);
}

void FileFragmentList_done(FileFragmentList *fileFragmentList)
{
  assert(fileFragmentList != NULL);

  List_done(fileFragmentList,(NodeFreeFunction)freeFileFragmentNode,NULL);
}

FragmentList *FileFragmentList_addFile(FileFragmentList *fileFragmentList, String fileName, uint64 size)
{
  FileFragmentNode *fileFragmentNode;

  assert(fileFragmentList != NULL);
  assert(fileName != NULL);

  fileFragmentNode = LIST_NEW_NODE(FileFragmentNode);
  if (fileFragmentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileFragmentNode->fileName = String_copy(fileName);
  fileFragmentNode->size     = size;
  List_init(&fileFragmentNode->fragmentList);

  List_append(fileFragmentList,fileFragmentNode);

  return &fileFragmentNode->fragmentList;
}

FragmentList *FileFragmentList_findFragmentList(FileFragmentList *fileFragmentList, String fileName)
{
  FileFragmentNode *fileFragmentNode;

  assert(fileFragmentList != NULL);
  assert(fileName != NULL);

  fileFragmentNode = fileFragmentList->head;
  while ((fileFragmentNode != NULL) && (!String_equals(fileFragmentNode->fileName,fileName)))
  {
    fileFragmentNode = fileFragmentNode->next;
  }

  return (fileFragmentNode !=NULL)?&fileFragmentNode->fragmentList:NULL;
}

void FileFragmentList_clear(FragmentList *fragmentList)
{
  assert(fragmentList != NULL);

  List_done(fragmentList,NULL,NULL);
}

void FileFragmentList_add(FragmentList *fragmentList, uint64 offset, uint64 length)
{
  FragmentNode *fragmentNode,*deleteFragmentNode;
  FragmentNode *prevFragmentNode,*nextFragmentNode;

  assert(fragmentList != NULL);

  /* remove all fragments which are completely covered by new fragment */
  fragmentNode = fragmentList->head;
  while (fragmentNode != NULL)
  {
    if ((F0(fragmentNode) >= I0(offset,length)) && (F1(fragmentNode) <= I1(offset,length)))
    {
      deleteFragmentNode = fragmentNode;
      fragmentNode = fragmentNode->next;
      List_remove(fragmentList,deleteFragmentNode);
      LIST_DELETE_NODE(deleteFragmentNode);
    }
    else
    {
      fragmentNode = fragmentNode->next;
    }
  }

  /* find prev/next fragment */
  prevFragmentNode = NULL;
  fragmentNode = fragmentList->head;
  while ((fragmentNode != NULL) && (F1(fragmentNode) < I1(offset,length)))
  {
    prevFragmentNode = fragmentNode;
    fragmentNode = fragmentNode->next;
  }
  nextFragmentNode = NULL;
  fragmentNode = fragmentList->tail;
  while ((fragmentNode != NULL) && (F0(fragmentNode) > I0(offset,length)))
  {
    nextFragmentNode = fragmentNode;
    fragmentNode = fragmentNode->prev;
  }

  /* check if existing Fragment can be extended or new Fragment have to be inserted */
  if      ((prevFragmentNode != NULL) && (F1(prevFragmentNode)+1 >= I0(offset,length)))
  {
    /* combine with previous existing fragment */
    prevFragmentNode->length = (offset+length)-prevFragmentNode->offset;
    prevFragmentNode->offset = prevFragmentNode->offset;
  }
  else if ((nextFragmentNode != NULL) && (I1(offset,length)+1 >= F0(nextFragmentNode)))
  {
    /* combine with next existing fragment */
    nextFragmentNode->length = (nextFragmentNode->offset+nextFragmentNode->length)-offset;
    nextFragmentNode->offset = offset;
  }
  else
  {
    /* insert new Fragment */
    fragmentNode = LIST_NEW_NODE(FragmentNode);
    if (fragmentNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    fragmentNode->offset = offset;
    fragmentNode->length = length;
    List_insert(fragmentList,fragmentNode,nextFragmentNode);
  }
}


bool FileFragmentList_check(FragmentList *fragmentList, uint64 offset, uint64 length)
{
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
return FALSE;
}

bool FileFragmentList_checkComplete(FileFragmentNode *fileFragmentNode)
{
  assert(fileFragmentNode != NULL);

  return    (List_count(&fileFragmentNode->fragmentList) == 1)
         && (fileFragmentNode->fragmentList.tail->offset == 0)
         && (fileFragmentNode->size <= fileFragmentNode->fragmentList.tail->length);
}

#ifndef NDEBUG
void FileFragmentList_print(FragmentList *fragmentList, const char *name)
{
  FragmentNode *fragmentNode;

  printf("Fragments '%s':\n",name);
  fragmentNode = fragmentList->head;
  for (fragmentNode = fragmentList->head; fragmentNode != NULL; fragmentNode = fragmentNode->next)
  {
    printf("  %8llu..%8llu\n",F0(fragmentNode),F1(fragmentNode));
  }
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
