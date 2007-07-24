/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.h,v $
* $Revision: 1.1.1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool archive_create(const char *fileName, PatternList *includeList, PatternList *excludeList, const char *tmpDirectory, ulong size);
bool archive_list(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList);
bool archive_test(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList);
bool archive_restore(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList, const char *directory);

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE_H__ */

/* end of file */
