/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: files functions
* Systems : all
*
\***********************************************************************/

#ifndef __FILES_H__
#define __FILES_H__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  String name;
  uint64 size;
  uint64 timeLastAccess;
  uint64 timeModified;
  uint64 timeLastChanged;
  uint32 userId;
  uint32 groupId;
  uint32 permission;
} FileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __FILES_H__ */

/* end of file */
