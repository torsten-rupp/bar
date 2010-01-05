/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver file system functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
//#include "stringlists.h"
#include "devices.h"
#include "errors.h"

#include "filesystems.h"

#include "filesystems_ext2.c"
#include "filesystems_ext3.c"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
/* file system definition */
typedef struct
{
  FileSystemTypes               type;
  uint                          sizeOfHandle;
  FileSystemInitFunction        initFunction;
  FileSystemDoneFunction        doneFunction;
  FileSystemBlockIsUsedFunction blockIsUsedFunction;
} FileSystem;

/***************************** Variables *******************************/

#define DEFINE_FILE_SYSTEM(name) \
  { \
    FILE_SYSTEM_TYPE_ ## name, \
    sizeof(name ## Handle), \
    (FileSystemInitFunction)name ## _init, \
    (FileSystemDoneFunction)name ## _done, \
    (FileSystemBlockIsUsedFunction)name ## _blockIsUsed, \
  }

/* support file systems */
LOCAL FileSystem FILE_SYSTEMS[] =
{
  DEFINE_FILE_SYSTEM(EXT2),
  DEFINE_FILE_SYSTEM(EXT3),
};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

Errors FileSystem_init(FileSystemHandle *fileSystemHandle,
                       DeviceHandle     *deviceHandle
                      )
{
  void *handle;
  int  z;

  assert(fileSystemHandle != NULL);
  assert(deviceHandle != NULL);

  /* initialize variables */
  fileSystemHandle->deviceHandle        = deviceHandle;
  fileSystemHandle->type                = FILE_SYSTEM_TYPE_UNKNOWN;
  fileSystemHandle->doneFunction        = NULL;
  fileSystemHandle->blockIsUsedFunction = NULL;

  /* detect file system on device */
  z = 0;
  while ((z < SIZE_OF_ARRAY(FILE_SYSTEMS)) && (fileSystemHandle->type == FILE_SYSTEM_TYPE_UNKNOWN))
  {
    handle = malloc(FILE_SYSTEMS[z].sizeOfHandle);
    if (handle == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    if (FILE_SYSTEMS[z].initFunction(deviceHandle,handle))
    {
      fileSystemHandle->type                = FILE_SYSTEMS[z].type;
      fileSystemHandle->handle              = handle;
      fileSystemHandle->doneFunction        = FILE_SYSTEMS[z].doneFunction;
      fileSystemHandle->blockIsUsedFunction = FILE_SYSTEMS[z].blockIsUsedFunction;
    }
    else
    {
      free(handle);
    }
    z++;
  }

  return ERROR_NONE;
}

Errors FileSystem_done(FileSystemHandle *fileSystemHandle)
{
  assert(fileSystemHandle != NULL);

  if (fileSystemHandle->doneFunction != NULL)
  {
    fileSystemHandle->doneFunction(fileSystemHandle->deviceHandle,fileSystemHandle->handle);
  }
  free(fileSystemHandle->handle);

  return ERROR_NONE;
}

bool FileSystem_blockIsUsed(FileSystemHandle *fileSystemHandle, uint64 blockOffset)
{
  assert(fileSystemHandle != NULL);

  if (fileSystemHandle->blockIsUsedFunction != NULL)
  {
    return fileSystemHandle->blockIsUsedFunction(fileSystemHandle->deviceHandle,fileSystemHandle->handle,blockOffset);
  }
  else
  {
    return TRUE;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
