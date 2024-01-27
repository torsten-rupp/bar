/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver PAR2 wrapper functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"

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
* Name   : PAR2_create
* Purpose: create PAR2 checksum files
* Input  : dataFileName               - data file name to create
*                                       checksum files for
*          sourceFileName             - source file name
*          checkSumFilesDirectoryPath - path to store checksum files
*                                       (could be NULL)
*          archiveFileMode            - archive file mode
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors PAR2_create(ConstString      dataFileName,
                   ConstString      sourceFileName,
                   const char       *checkSumFilesDirectoryPath,
                   ArchiveFileModes archiveFileMode
                  );

#ifdef __cplusplus
  }
#endif

/* end of file */
