/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/filefragmentlists.c,v $
* $Revision: 1.5 $
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

  List_done(fileFragmentList,(ListNodeFreeFunction)freeFileFragmentNode,NULL);
}

FileFragmentNode *FileFragmentList_addFile(FileFragmentList *fileFragmentList, const String fileName, uint64 size)
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

  return fileFragmentNode;
}

void FileFragmentList_removeFile(FileFragmentList *fileFragmentList, FileFragmentNode *fileFragmentNode)
{
  assert(fileFragmentList != NULL);
  assert(fileFragmentNode != NULL);

  List_remove(fileFragmentList,fileFragmentNode);
  freeFileFragmentNode(fileFragmentNode,NULL);
  LIST_DELETE_NODE(fileFragmentNode);
}

FileFragmentNode *FileFragmentList_findFile(FileFragmentList *fileFragmentList, const String fileName)
{
  FileFragmentNode *fileFragmentNode;

  assert(fileFragmentList != NULL);
  assert(fileName != NULL);

  fileFragmentNode = fileFragmentList->head;
  while ((fileFragmentNode != NULL) && (!String_equals(fileFragmentNode->fileName,fileName)))
  {
    fileFragmentNode = fileFragmentNode->next;
  }

  return fileFragmentNode;
}

void FileFragmentList_clear(FileFragmentNode *fileFragmentNode)
{
  assert(fileFragmentNode != NULL);

  List_done(&fileFragmentNode->fragmentList,NULL,NULL);
}

void FileFragmentList_add(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length)
{
  FragmentNode *fragmentNode,*deleteFragmentNode;
  FragmentNode *prevFragmentNode,*nextFragmentNode;

  assert(fileFragmentNode != NULL);

  /* remove all fragments which are completely covered by new fragment */
  fragmentNode = fileFragmentNode->fragmentList.head;
  while (fragmentNode != NULL)
  {
    if ((F0(fragmentNode) >= I0(offset,length)) && (F1(fragmentNode) <= I1(offset,length)))
    {
      deleteFragmentNode = fragmentNode;
      fragmentNode = fragmentNode->next;
      List_remove(&fileFragmentNode->fragmentList,deleteFragmentNode);
      LIST_DELETE_NODE(deleteFragmentNode);
    }
    else
    {
      fragmentNode = fragmentNode->next;
    }
  }

  /* find prev/next fragment */
  prevFragmentNode = NULL;
  fragmentNode = fileFragmentNode->fragmentList.head;
  while ((fragmentNode != NULL) && (F1(fragmentNode) < I1(offset,length)))
  {
    prevFragmentNode = fragmentNode;
    fragmentNode = fragmentNode->next;
  }
  nextFragmentNode = NULL;
  fragmentNode = fileFragmentNode->fragmentList.tail;
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
    List_insert(&fileFragmentNode->fragmentList,fragmentNode,nextFragmentNode);
  }
}

bool FileFragmentList_checkExists(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length)
{
  bool         existsFlag;
  uint64       i0,i1;
  FragmentNode *fragmentNode;

  assert(fileFragmentNode != NULL);

  i0 = I0(offset,length);
  i1 = I1(offset,length);

  existsFlag = FALSE;
  for (fragmentNode = fileFragmentNode->fragmentList.head; (fragmentNode != NULL) && !existsFlag; fragmentNode = fragmentNode->next)
  {
    if (   ((F0(fragmentNode) <= i0) && (i0 <= F1(fragmentNode)) )
        || ((F0(fragmentNode) <= i1) && (i1 <= F1(fragmentNode)))
       )
    {
      existsFlag = TRUE;
    }
  }

  return existsFlag;
}

bool FileFragmentList_checkComplete(FileFragmentNode *fileFragmentNode)
{
  assert(fileFragmentNode != NULL);

  return    (fileFragmentNode->size == 0)
         || (   (List_count(&fileFragmentNode->fragmentList) == 1)
             && (fileFragmentNode->fragmentList.head->offset == 0)
             && (fileFragmentNode->fragmentList.head->length >= fileFragmentNode->size)
            );
}

#ifndef NDEBUG
void FileFragmentList_print(FileFragmentNode *fileFragmentNode, const char *name)
{
  FragmentNode *fragmentNode;

  printf("Fragments '%s':\n",name);
  for (fragmentNode = fileFragmentNode->fragmentList.head; fragmentNode != NULL; fragmentNode = fragmentNode->next)
  {
    printf("  %8llu..%8llu\n",F0(fragmentNode),F1(fragmentNode));
  }
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
