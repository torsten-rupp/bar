/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/devices.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver device functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"

#include "devices.h"

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

/*---------------------------------------------------------------------*/

Errors Device_open(DeviceHandle    *deviceHandle,
                   const String    deviceName,
                   DeviceOpenModes deviceOpenMode
                  )
{
  off_t  n;
  Errors error;

  assert(deviceHandle != NULL);
  assert(deviceName != NULL);

  /* open device */
  switch (deviceOpenMode)
  {
    case DEVICE_OPENMODE_READ:
      deviceHandle->file = fopen(String_cString(deviceName),"rb");
      if (deviceHandle->file == NULL)
      {
        return ERRORX(OPEN_DEVICE,errno,String_cString(deviceName));
      }
      break;
    case DEVICE_OPENMODE_WRITE:
      deviceHandle->file = fopen(String_cString(deviceName),"r+b");
      if (deviceHandle->file == NULL)
      {
        return ERRORX(OPEN_DEVICE,errno,String_cString(deviceName));
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* get device size */
  if (fseeko(deviceHandle->file,(off_t)0,SEEK_END) == -1)
  {
    error = ERRORX(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  n = ftello(deviceHandle->file);
  if (n == (off_t)(-1))
  {
    error = ERRORX(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  if (fseeko(deviceHandle->file,(off_t)0,SEEK_SET) == -1)
  {
    error = ERRORX(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }

  /* initialize handle */
  deviceHandle->name  = String_duplicate(deviceName);
  deviceHandle->index = 0LL;
  deviceHandle->size  = (int64)n;

  return ERROR_NONE;
}

Errors Device_close(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);
  assert(deviceHandle->name != NULL);
 
  fclose(deviceHandle->file);
  deviceHandle->file = NULL;
  String_delete(deviceHandle->name);

  return ERROR_NONE;
}

bool Device_eof(DeviceHandle *deviceHandle)
{
  int  ch;
  bool eofFlag;

  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);

  ch = getc(deviceHandle->file);
  if (ch != EOF)
  {
    ungetc(ch,deviceHandle->file);
    eofFlag = FALSE;
  }
  else
  {
    eofFlag = TRUE;
  }

  return eofFlag;
}

Errors Device_read(DeviceHandle *deviceHandle,
                   void         *buffer,
                   ulong        bufferLength,
                   ulong        *bytesRead
                  )
{
  ssize_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);
  assert(buffer != NULL);

  n = fread(buffer,1,bufferLength,deviceHandle->file);
  if (   ((n <= 0) && ferror(deviceHandle->file))
      || ((n < bufferLength) && (bytesRead == NULL))
     )
  {
    return ERRORX(IO_ERROR,errno,String_cString(deviceHandle->name));
  }
  deviceHandle->index += n;

  if (bytesRead != NULL) (*bytesRead) = n;

  return ERROR_NONE;
}

Errors Device_write(DeviceHandle *deviceHandle,
                    const void   *buffer,
                    ulong        bufferLength
                   )
{
  ssize_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);
  assert(buffer != NULL);

  n = fwrite(buffer,1,bufferLength,deviceHandle->file);
  if (n > 0) deviceHandle->index += n;
  if (deviceHandle->index > deviceHandle->size) deviceHandle->size = deviceHandle->index;
  if (n != bufferLength)
  {
    return ERRORX(IO_ERROR,errno,String_cString(deviceHandle->name));
  }

  return ERROR_NONE;
}

int64 Device_getSize(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  return deviceHandle->size;
}

Errors Device_tell(DeviceHandle *deviceHandle, uint64 *offset)
{
  off_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);
  assert(offset != NULL);

  n = ftello(deviceHandle->file);
  if (n == (off_t)(-1))
  {
    return ERRORX(IO_ERROR,errno,String_cString(deviceHandle->name));
  }
// NYI
//assert(sizeof(off_t)==8);
assert(n == (off_t)deviceHandle->index);

  (*offset) = deviceHandle->index;

  return ERROR_NONE;
}

Errors Device_seek(DeviceHandle *deviceHandle,
                   uint64       offset
                  )
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->file != NULL);

  if (fseeko(deviceHandle->file,(off_t)offset,SEEK_SET) == -1)
  {
    return ERRORX(IO_ERROR,errno,String_cString(deviceHandle->name));
  }
  deviceHandle->index = offset;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Device_openDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);

  deviceListHandle->dir = opendir("/dev");
  if (deviceListHandle->dir == NULL)
  {
    return ERROR(OPEN_DIRECTORY,errno);
  }
  deviceListHandle->entry = NULL;

  return ERROR_NONE;
}

void Device_closeDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);
  assert(deviceListHandle->dir != NULL);

  closedir(deviceListHandle->dir);
}

bool Device_endOfDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);
  assert(deviceListHandle->dir != NULL);

  /* read entry iff not read */
  if (deviceListHandle->entry == NULL)
  {
    deviceListHandle->entry = readdir(deviceListHandle->dir);
  }

  /* skip "." and ".." entries */
  while (   (deviceListHandle->entry != NULL)
         && (   (strcmp(deviceListHandle->entry->d_name,"." ) == 0)
             || (strcmp(deviceListHandle->entry->d_name,"..") == 0)
            )
        )
  {
    deviceListHandle->entry = readdir(deviceListHandle->dir);
  }

  return deviceListHandle->entry == NULL;
}

Errors Device_readDeviceList(DeviceListHandle *deviceListHandle,
                             String           deviceName
                            )
{
  assert(deviceListHandle != NULL);
  assert(deviceListHandle->dir != NULL);
  assert(deviceName != NULL);

  /* read entry iff not read */
  if (deviceListHandle->entry == NULL)
  {
    deviceListHandle->entry = readdir(deviceListHandle->dir);
  }

  /* skip "." and ".." */
  while (   (deviceListHandle->entry != NULL)
         && (   (strcmp(deviceListHandle->entry->d_name,"." ) == 0)
             || (strcmp(deviceListHandle->entry->d_name,"..") == 0)
            )
        )
  {
    deviceListHandle->entry = readdir(deviceListHandle->dir);
  }
  if (deviceListHandle->entry == NULL)
  {
    return ERROR(IO_ERROR,errno);
  }

  /* get device name */
  String_setCString(deviceName,"/dev");
  File_appendFileNameCString(deviceName,deviceListHandle->entry->d_name);

  /* mark entry read */
  deviceListHandle->entry = NULL;

  return ERROR_NONE;
}

Errors Device_getDeviceInfo(const String deviceName, DeviceInfo *deviceInfo)
{
  struct stat64 fileStat;
  FILE          *file;
  off_t         n;
  int           handle;
  struct statfs fileSystemStat;

  assert(deviceName != NULL);
  assert(deviceInfo != NULL);

  /* initialize variables */
  deviceInfo->type        = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size        = -1LL;
  deviceInfo->blockSize   = 0L;
  deviceInfo->freeBlocks  = 0LL;
  deviceInfo->totalBlocks = 0LL;
  deviceInfo->mountedFlag = FALSE;

  /* get device type */
  if (lstat64(String_cString(deviceName),&fileStat) == 0)
  {
    if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
    else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
  }

  if (deviceInfo->type == DEVICE_TYPE_BLOCK)
  {
    /* get device size */
    file = fopen(String_cString(deviceName),"rb");
    if (file != NULL)
    {
      if (fseeko(file,(off_t)0,SEEK_END) != -1)
      {
        n = ftello(file);
        if (n != (off_t)(-1))
        {
          deviceInfo->size = (int64)n;
        }
else {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,errno,strerror(errno));
}
      }
      fclose(file);
    }

    /* get block size, free/total blocks */
    handle = open(String_cString(deviceName),O_RDONLY);
    if (handle != -1)
    {
      if (fstatfs(handle,&fileSystemStat) == 0)
      {
        deviceInfo->blockSize   = fileSystemStat.f_bsize;
        deviceInfo->freeBlocks  = fileSystemStat.f_bfree;
        deviceInfo->totalBlocks = fileSystemStat.f_blocks;
      }
      close(handle);
    }
    else
    {
      return ERRORX(OPEN_DEVICE,errno,String_cString(deviceName));
    }
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
