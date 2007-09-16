/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.h,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver archive create function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_CREATE__
#define __COMMAND_CREATE__

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

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : command_create
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool command_create(const char         *archiveFileName,
                    PatternList        *includePatternList,
                    PatternList        *excludePatternList,
                    const char         *tmpDirectory,
                    ulong              partSize,
                    CompressAlgorithms compressAlgorithm,
                    ulong              compressMinFileSize,
                    CryptAlgorithms    cryptAlgorithm,
                    const char         *password
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_CREATE__ */

/* end of file */
