/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/devices.c,v $
* $Revision: 1.4 $
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
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>

#include <linux/types.h>
#include <linux/fs.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "errors.h"

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

  /* open partition list file */
  deviceListHandle->file = fopen("/proc/partitions","r");
  if (deviceListHandle->file == NULL)
  {
    return ERROR(OPEN_FILE,errno);
  }

  /* skip first line (header line) */
  if (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) == NULL)
  {
    // ignore error
  }

  /* no line read jet */
  deviceListHandle->readFlag = FALSE;

  return ERROR_NONE;
}

void Device_closeDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);
  assert(deviceListHandle->file != NULL);

  fclose(deviceListHandle->file);
}

bool Device_endOfDeviceList(DeviceListHandle *deviceListHandle)
{
  uint i,j;

  assert(deviceListHandle != NULL);
  assert(deviceListHandle->file != NULL);

  /* read entry iff not read */
  while (   !deviceListHandle->readFlag
         && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
        )
  {
    /* skip leading spaces */
    i = 0;
    while ((i < sizeof(deviceListHandle->line)) && isspace(deviceListHandle->line[i]))
    {
      i++;
    }
    j = 0;
    while ((i < sizeof(deviceListHandle->line)) && (deviceListHandle->line[i] != '\0'))
    {
      deviceListHandle->line[j] = deviceListHandle->line[i];
      i++;
      j++;
    }

    /* skip trailing spaces */
    while ((j > 0) && isspace(deviceListHandle->line[j-1]))
    {
      j--;
    }
    deviceListHandle->line[j] = '\0';

    /* if line is not empty set read flag */
    if (j > 0) deviceListHandle->readFlag = TRUE;
  }

  return !deviceListHandle->readFlag;
}

Errors Device_readDeviceList(DeviceListHandle *deviceListHandle,
                             String           deviceName
                            )
{
  uint i,j;
  char buffer[256];

  assert(deviceListHandle != NULL);
  assert(deviceListHandle->file != NULL);
  assert(deviceName != NULL);

  /* read entry iff not read */
  while (   !deviceListHandle->readFlag
         && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
        )
  {
    /* skip leading spaces */
    i = 0;
    while ((i < sizeof(deviceListHandle->line)) && isspace(deviceListHandle->line[i]))
    {
      i++;
    }
    j = 0;
    while ((i < sizeof(deviceListHandle->line)) && (deviceListHandle->line[i] != '\0'))
    {
      deviceListHandle->line[j] = deviceListHandle->line[i];
      i++;
      j++;
    }

    /* skip trailing spaces */
    while ((j > 0) && isspace(deviceListHandle->line[j-1]))
    {
      j--;
    }
    deviceListHandle->line[j] = '\0';

    /* if line is not empty set read flag */
    if (j > 0) deviceListHandle->readFlag = TRUE;
  }

  if (deviceListHandle->readFlag)
  {
    /* parse and get device name */
    if (!String_scanCString(deviceListHandle->line,"%* %* %* %256s %*",buffer))
    {
      return ERRORX(IO_ERROR,0,"invalid format");
    }
    String_setCString(deviceName,"/dev");
    File_appendFileNameCString(deviceName,buffer);

    /* mark entry read */
    deviceListHandle->readFlag = FALSE;
  }
  else
  {
    String_clear(deviceName);
  }

  return ERROR_NONE;
}

Errors Device_getDeviceInfo(const String deviceName, DeviceInfo *deviceInfo)
{
  struct stat64 fileStat;
  int           handle;
  long          n;

  assert(deviceName != NULL);
  assert(deviceInfo != NULL);

  /* initialize variables */
  deviceInfo->type        = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size        = -1LL;
  deviceInfo->blockSize   = 0L;
//  deviceInfo->freeBlocks  = 0LL;
//  deviceInfo->totalBlocks = 0LL;
//  deviceInfo->mountedFlag = FALSE;

  /* get device type */
  if (lstat64(String_cString(deviceName),&fileStat) == 0)
  {
    if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
    else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
  }

  if (deviceInfo->type == DEVICE_TYPE_BLOCK)
  {
    /* get block size, total size */
    handle = open(String_cString(deviceName),O_RDONLY);
    if (handle != -1)
    {
      if (ioctl(handle,BLKSSZGET, &n) == 0) deviceInfo->blockSize = (ulong)n;
      if (ioctl(handle,BLKGETSIZE,&n) == 0) deviceInfo->size      = (int64)n*512;
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
