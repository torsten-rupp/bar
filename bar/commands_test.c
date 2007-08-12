/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "chunks.h"
#include "archive_format.h"
#include "archive.h"

#include "command_test.h"

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

bool command_test(FileNameList *fileNameList,
                  PatternList  *includeList,
                  PatternList  *excludeList
                 )
{
  assert(fileNameList != NULL);
  assert(includeList != NULL);
  assert(excludeList != NULL);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
