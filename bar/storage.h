/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

#ifndef __STORAGE__
#define __STORAGE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <libssh2.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "files.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  STORAGE_MODE_READ,
  STORAGE_MODE_WRITE,
} StorageModes;

typedef enum
{
  STORAGE_TYPE_FILE,
  STORAGE_TYPE_SCP,
  STORAGE_TYPE_SFTP
} StorageTypes;

typedef struct
{
  StorageModes mode;
  StorageTypes type;
  union
  {
    struct
    {
      String     fileName;
      FileHandle fileHandle;
    } file;
    struct
    {
      int             socketHandle;
      LIBSSH2_SESSION *session;
      LIBSSH2_CHANNEL *channel;
    } scp;
    struct
    {
    } sftp;
  };
} StorageInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Storage_init(void);
void Storage_done(void);

Errors Storage_create(StorageInfo *storageInfo, String storageName, uint64 fileSize);
Errors Storage_open(StorageInfo *storageInfo, String storageName);
void Storage_close(StorageInfo *storageInfo);

uint64 Storage_getSize(StorageInfo *storageInfo);

Errors Storage_read(StorageInfo *storageInfo, void *buffer, ulong size, ulong *readBytes);
Errors Storage_write(StorageInfo *storageInfo, const void *buffer, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __STORAGE__ */

/* end of file */
