/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/filefragmentlists.h,v $
* $Revision: 1.3 $
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

  uint64 offset;                        // fragment offset (0..n)
  uint64 length;                        // length of fragment
} FragmentNode;

typedef struct
{
  LIST_HEADER(FragmentNode);
} FragmentList;

typedef struct FileFragmentNode
{
  NODE_HEADER(struct FileFragmentNode);

  String       fileName;                // fragment file name
  uint64       size;                    // size of file
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

/***********************************************************************\
* Name   : FileFragmentList_init
* Purpose: init file fragment list
* Input  : fileFragmentList - file fragment list
* Output : fileFragmentList - initialize file fragment list
* Return : -
* Notes  : -
\***********************************************************************/

void FileFragmentList_init(FileFragmentList *fileFragmentList);

/***********************************************************************\
* Name   : FileFragmentList_done
* Purpose: free all nodes and deinitialize file fragment list
* Input  : fileFragmentList - file fragment list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FileFragmentList_done(FileFragmentList *fileFragmentList);

/***********************************************************************\
* Name   : FileFragmentList_addFile
* Purpose: add new file to file fragmen tlist
* Input  : fileFragmentList - file fragment list
*          fileName         - file name
*          size             - size of file
* Output : -
* Return : file fragment node or NULL
* Notes  : -
\***********************************************************************/

FileFragmentNode *FileFragmentList_addFile(FileFragmentList *fileFragmentList, String fileName, uint64 size);

/***********************************************************************\
* Name   : FileFragmentList_removeFile
* Purpose: remove file from file fragment list
* Input  : fileFragmentList - file fragment list
*          fileFragmentNode - file fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FileFragmentList_removeFile(FileFragmentList *fileFragmentList, FileFragmentNode *fileFragmentNode);

/***********************************************************************\
* Name   : FileFragmentList_findFile
* Purpose: find file in file fragment list
* Input  : fileFragmentList - file fragment list
*          fileName         - file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

FileFragmentNode *FileFragmentList_findFile(FileFragmentList *fileFragmentList, String fileName);

/***********************************************************************\
* Name   : FileFragmentList_clear
* Purpose: clear fragments of file
* Input  : fileFragmentNode - file fragment node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FileFragmentList_clear(FileFragmentNode *fileFragmentNode);

/***********************************************************************\
* Name   : FileFragmentList_add
* Purpose: add a fragment to file
* Input  : fileFragmentNode - file fragment node
*          offset           - fragment offset (0..n)
*          length           - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void FileFragmentList_add(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FileFragmentList_checkExists
* Purpose: check if fragment already exists in file
* Input  : fileFragmentNode - file fragment node
*          offset           - fragment offset (0..n)
*          length           - length of fragment
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool FileFragmentList_checkExists(FileFragmentNode *fileFragmentNode, uint64 offset, uint64 length);

/***********************************************************************\
* Name   : FileFragmentList_checkComplete
* Purpose: check if file is completed (no fragmentation)
* Input  : fileFragmentNode - file fragment node
* Output : -
* Return : TRUE if file completed (no fragmented), FALSE otherwise
* Notes  : -
\***********************************************************************/

bool FileFragmentList_checkComplete(FileFragmentNode *fileFragmentNode);

#ifndef NDEBUG
void FileFragmentList_print(FragmentList *fragmentList, const char *name);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FILE_FRAGMENT_LISTS__ */

/* end of file */
