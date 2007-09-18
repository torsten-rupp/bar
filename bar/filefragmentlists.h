/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/filefragmentlists.h,v $
* $Revision: 1.2 $
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

typedef struct FragmentNode
{
  NODE_HEADER(struct FragmentNode);

  uint64 offset;
  uint64 length;
} FragmentNode;

typedef struct
{
  LIST_HEADER(FragmentNode);
} FragmentList;

typedef struct FileFragmentNode
{
  NODE_HEADER(struct FileFragmentNode);

  String       fileName;
  uint64       size;
  FragmentList fragmentList;
} FileFragmentNode;

typedef struct
{
  LIST_HEADER(FileFragmentNode);
} FileFragmentList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

void FileFragmentList_init(FileFragmentList *fileFragmentList);
void FileFragmentList_done(FileFragmentList *fileFragmentList);

FileFragmentNode *FileFragmentList_addFile(FileFragmentList *fileFragmentList, String fileName, uint64 size);
void FileFragmentList_removeFile(FileFragmentList *fileFragmentList, FileFragmentNode *fileFragmentNode);
FileFragmentNode *FileFragmentList_findFile(FileFragmentList *fileFragmentList, String fileName);

void FileFragmentList_clear(FileFragmentNode *fileFragmentNode);

void FileFragmentList_add(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length);

bool FileFragmentList_check(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length);

bool FileFragmentList_checkComplete(FileFragmentNode *fileFragmentNode);

#ifndef NDEBUG
void FileFragmentList_print(FragmentList *fragmentList, const char *name);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FILE_FRAGMENT_LISTS__ */

/* end of file */
