/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.h,v $
* $Revision: 1.8 $
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
  ulong  doneFiles;                        // number of files processed
  uint64 doneBytes;                        // number of bytes processed
  ulong  totalFiles;                       // total number of files
  uint64 totalBytes;                       // total bytes
  ulong  skippedFiles;                     // number of skipped files
  uint64 skippedBytes;                     // sum of skipped bytes
  ulong  errorFiles;                       // number of files with errors
  uint64 errorBytes;                       // sum of byste in files with errors
  double compressionRatio;                 // compression ratio
  String fileName;                         // current file name
  uint64 fileDoneBytes;                    // number of bytes processed of current file
  uint64 fileTotalBytes;                   // total number of bytes of current file
  String storageName;                      // current storage name
  uint64 storageDoneBytes;                 // number of bytes processed of current storage
  uint64 storageTotalBytes;                // total bytes of current storage
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
