/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.h,v $
* $Revision: 1.7 $
* $Author: torsten $
* Contents: Backup ARchiver archive create function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_CREATE__
#define __COMMANDS_CREATE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bar.h"
#include "patterns.h"
#include "compress.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
/* status info data */
typedef struct
{
  ulong  doneFiles;
  uint64 doneBytes;
  ulong  totalFiles;
  uint64 totalBytes;
  ulong  skippedFiles;
  uint64 skippedBytes;
  ulong  errorFiles;
  uint64 errorBytes;
  double compressionRatio;
  String fileName;
  uint64 fileDoneBytes;
  uint64 fileTotalBytes;
  String storageName;
  uint64 storageDoneBytes;
  uint64 storageTotalBytes;
} CreateStatusInfo;

typedef char(*CreateStatusInfoFunction)(Errors                 error,
                                        const CreateStatusInfo *createStatusInfo,
                                        void                   *userData
                                       );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Command_create
* Purpose: create archive
* Input  : archiveFileName          - archive file name
*          includeList              - include list
*          excludeList              - exclude list
*          optiosn                  - options
*          createStatusInfoFunction - status info call back function
*                                     (can be NULL)
*          createStatusInfoUserData - user data for status info function
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_create(const char               *archiveFileName,
                      PatternList              *includePatternList,
                      PatternList              *excludePatternList,
                      const Options            *options,
                      CreateStatusInfoFunction createStatusInfoFunction,
                      void                     *createStatusInfoUserData
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CREATE__ */

/* end of file */
