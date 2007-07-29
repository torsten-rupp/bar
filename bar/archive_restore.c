/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive_restore.c,v $
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

LOCAL bool readFile(void *userData, void *buffer, ulong length)
{
  return (read((int)userData,buffer,length) == length);
}

LOCAL bool writeFile(void *userData, const void *buffer, ulong length)
{
  return (write((int)userData,buffer,length) == length);
}

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  off64_t n;

  assert(offset != NULL);

  n = lseek64((int)userData,0,SEEK_CUR);
  if (n == (off_t)-1)
  {
    return FALSE;
  }
  (*offset) = (uint64)n;

  return TRUE;
}

LOCAL bool seekFile(void *userData, uint64 offset)
{
  if (lseek64((int)userData,(off64_t)offset,SEEK_SET) == (off_t)-1)
  {
    return FALSE;
  }

  return TRUE;
}

/*---------------------------------------------------------------------*/

bool archive_test(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList)
{
  return TRUE;
}

/*---------------------------------------------------------------------*/

bool archive_restore(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList, const char *directory)
{
  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
