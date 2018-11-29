/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive list functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_LIST__
#define __COMMANDS_LIST__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/stringlists.h"
#include "common/patternlists.h"

#include "bar.h"
#include "entrylists.h"
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
* Name   : Command_list
* Purpose: list content of archive(s)
* Input  : storageNameList         - list with storage names
*          includeEntryList        - include entry list
*          excludePatternList      - exclude pattern list
*          showEntriesFlag         - TRUE to show entries
*          jobOptions              - job options
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password call back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_list(StringList              *storageNameList,
                    const EntryList         *includeEntryList,
                    const PatternList       *excludePatternList,
                    bool                    showEntriesFlag,
                    JobOptions              *jobOptions,
                    GetNamePasswordFunction getNamePasswordFunction,
                    void                    *getNamePasswordUserData,
                    LogHandle               *logHandle
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_LIST__ */

/* end of file */
