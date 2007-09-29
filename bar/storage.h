/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.h,v $
* $Revision: 1.2 $
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

/***********************************************************************\
* Name   : Storage_init
* Purpose: initialize storage functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_init(void);

/***********************************************************************\
* Name   : Storage_done
* Purpose: deinitialize storage functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_done(void);

/***********************************************************************\
* Name   : Storage_prepare
* Purpose: prepare storage: read password, init files
* Input  : storageName - storage name
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_prepare(const String storageName);

/***********************************************************************\
* Name   : Storage_create
* Purpose: create new storage
* Input  : storageName - storage name
*          fileSize    - storage file size
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_create(StorageInfo *storageInfo, const String storageName, uint64 fileSize);

/***********************************************************************\
* Name   : Storage_open
* Purpose: open storage
* Input  : storageName - storage name
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_open(StorageInfo *storageInfo, const String storageName);

/***********************************************************************\
* Name   : Storage_close
* Purpose: close storage
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_close(StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_getSize
* Purpose: get storage file size
* Input  : storageInfo - storage info block
* Output : -
* Return : size of storage
* Notes  : -
\***********************************************************************/

uint64 Storage_getSize(StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_read
* Purpose: read from storage 
* Input  : storageInfo - storage info block
*          buffer      - buffer with data to write
*          size        - data size
* Output : readBytes - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_read(StorageInfo *storageInfo, void *buffer, ulong size, ulong *readBytes);

/***********************************************************************\
* Name   : Storage_write
* Purpose: write into storage
* Input  : storageInfo - storage info block
*          buffer      - buffer with data to write
*          size        - data size
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_write(StorageInfo *storageInfo, const void *buffer, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __STORAGE__ */

/* end of file */
