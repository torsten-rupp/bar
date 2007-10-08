/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.h,v $
* $Revision: 1.6 $
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
typedef struct
{
  ulong  doneFiles;
  uint64 doneBytes;
  ulong  totalFiles;
  uint64 totalBytes;
  double compressionRatio;
  String fileName;
  String storageName;
} CreateStatusInfo;

typedef char(*CreateStatusInfoFunction)(const CreateStatusInfo *createStatusInfo, void *userData);

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
* Input  : archiveFileName     - archive file name
*          includeList         - include list
*          excludeList         - exclude list
*          archivePartSize     - archive part size or 0
*          compressAlgorithm   - compression algorithm to use
*          compressMinFileSize - min. file size for compression
*          cryptAlgorithm      - crypt algorithm to use
*          password            - crypt password
* Output : -
* Return : TRUE if archive created, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Command_create(const char               *archiveFileName,
                    PatternList              *includePatternList,
                    PatternList              *excludePatternList,
                    ulong                    archivePartSize,
                    CompressAlgorithms       compressAlgorithm,
                    ulong                    compressMinFileSize,
                    CryptAlgorithms          cryptAlgorithm,
                    const char               *password,
                    CreateStatusInfoFunction createStatusInfoFunction,
                    void                     *createStatusInfoUserData
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CREATE__ */

/* end of file */
