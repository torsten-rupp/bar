/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif
#include <mntent.h>
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/types.h>
  #include <linux/fs.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

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
#ifdef HAVE_FSEEKO
  #define FSEEK fseeko
#elif HAVE__FSEEKI64
  #define FSEEK _fseeki64
#else
  #define FSEEK fseek
#endif

#ifdef HAVE_FTELLO
  #define FTELL ftello
#elif HAVE__FTELLI64
  #define FTELL _ftelli64
#else
  #define FTELL ftell
#endif

#ifdef HAVE_STAT64
  #define STAT  stat64
  #define LSTAT lstat64
  typedef struct stat64 FileStat;
#elif HAVE___STAT64
  #define STAT  stat64
  #define LSTAT lstat64
  typedef struct __stat64 FileStat;
#elif HAVE__STATI64
  #define STAT  _stati64
  #define LSTAT _stati64
  typedef struct _stati64 FileStat;
#elif HAVE_STAT
  #define STAT  stat
  #define LSTAT lstat
  typedef struct stat FileStat;
#else
  #error No struct stat64 nor struct __stat64
#endif

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

Errors Device_open(DeviceHandle *deviceHandle,
                   const String deviceName,
                   DeviceModes  deviceMode
                  )
{
  off_t  n;
  Errors error;

  assert(deviceHandle != NULL);
  assert(deviceName != NULL);

  // open device
  switch (deviceMode)
  {
    case DEVICE_OPEN_READ:
      deviceHandle->file = fopen(String_cString(deviceName),"rb");
      if (deviceHandle->file == NULL)
      {
        return ERRORX_(OPEN_DEVICE,errno,String_cString(deviceName));
      }
      break;
    case DEVICE_OPEN_WRITE:
      deviceHandle->file = fopen(String_cString(deviceName),"r+b");
      if (deviceHandle->file == NULL)
      {
        return ERRORX_(OPEN_DEVICE,errno,String_cString(deviceName));
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  // get device size
  if (FSEEK(deviceHandle->file,(off_t)0,SEEK_END) == -1)
  {
    error = ERRORX_(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  n = FTELL(deviceHandle->file);
  if (n == (off_t)(-1))
  {
    error = ERRORX_(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  if (FSEEK(deviceHandle->file,(off_t)0,SEEK_SET) == -1)
  {
    error = ERRORX_(IO_ERROR,errno,String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }

  // initialize handle
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
      || ((n < (ssize_t)bufferLength) && (bytesRead == NULL))
     )
  {
    return ERRORX_(IO_ERROR,errno,String_cString(deviceHandle->name));
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
  if (n != (ssize_t)bufferLength)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(deviceHandle->name));
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

  n = FTELL(deviceHandle->file);
  if (n == (off_t)(-1))
  {
    return ERRORX_(IO_ERROR,errno,String_cString(deviceHandle->name));
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

  if (FSEEK(deviceHandle->file,(off_t)offset,SEEK_SET) == -1)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(deviceHandle->name));
  }
  deviceHandle->index = offset;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Device_openDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);

  // open partition list file
  deviceListHandle->file = fopen("/proc/partitions","r");
  if (deviceListHandle->file == NULL)
  {
    return ERROR_(OPEN_FILE,errno);
  }

  // skip first line (header line)
  if (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) == NULL)
  {
    // ignore error
  }

  // no line read yet
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

  // read entry iff not read
  while (   !deviceListHandle->readFlag
         && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
        )
  {
    // skip leading spaces
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

    // skip trailing spaces
    while ((j > 0) && isspace(deviceListHandle->line[j-1]))
    {
      j--;
    }
    deviceListHandle->line[j] = '\0';

    // if line is not empty set read flag
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

  // read entry iff not read
  while (   !deviceListHandle->readFlag
         && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
        )
  {
    // skip leading spaces
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

    // skip trailing spaces
    while ((j > 0) && isspace(deviceListHandle->line[j-1]))
    {
      j--;
    }
    deviceListHandle->line[j] = '\0';

    // if line is not empty set read flag
    if (j > 0) deviceListHandle->readFlag = TRUE;
  }

  if (deviceListHandle->readFlag)
  {
    // parse and get device name
    if (!String_scanCString(deviceListHandle->line,"%* %* %* %256s %*",buffer))
    {
      return ERRORX_(IO_ERROR,0,"invalid format");
    }
    String_setCString(deviceName,"/dev");
    File_appendFileNameCString(deviceName,buffer);

    // mark entry read
    deviceListHandle->readFlag = FALSE;
  }
  else
  {
    String_clear(deviceName);
  }

  return ERROR_NONE;
}

Errors Device_getDeviceInfo(DeviceInfo   *deviceInfo,
                            const String deviceName
                           )
{
  FileStat fileStat;
  int      handle;
  #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
    int      i;
  #endif
  #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
    long     l;
  #endif
  FILE          *mtab;
  struct mntent *mountEntryPointer;
  struct mntent mountEntry;
  char          buffer[4096];

  assert(deviceName != NULL);
  assert(deviceInfo != NULL);

  // initialize variables
  deviceInfo->type        = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size        = -1LL;
  deviceInfo->blockSize   = 0L;
//  deviceInfo->freeBlocks  = 0LL;
//  deviceInfo->totalBlocks = 0LL;
  deviceInfo->mountedFlag = FALSE;

  // get device meta data
  if (LSTAT(String_cString(deviceName),&fileStat) == 0)
  {
    deviceInfo->timeLastAccess  = fileStat.st_atime;
    deviceInfo->timeModified    = fileStat.st_mtime;
    deviceInfo->timeLastChanged = fileStat.st_ctime;
    deviceInfo->userId          = fileStat.st_uid;
    deviceInfo->groupId         = fileStat.st_gid;
    deviceInfo->permission      = (DevicePermission)fileStat.st_mode;
    #ifdef HAVE_MAJOR
      deviceInfo->major         = major(fileStat.st_rdev);
    #else
      deviceInfo->major         = 0;
    #endif
    #ifdef HAVE_MINOR
      deviceInfo->minor         = minor(fileStat.st_rdev);
    #else
      deviceInfo->minor         = 0;
    #endif
    deviceInfo->id              = (uint64)fileStat.st_ino;

    if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
    else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
  }

  if (deviceInfo->type == DEVICE_TYPE_BLOCK)
  {
    // get block size, total size
    handle = open(String_cString(deviceName),O_RDONLY);
    if (handle != -1)
    {
      #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
        if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
      #endif
      #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
        if (ioctl(handle,BLKGETSIZE,&l) == 0) deviceInfo->size      = (int64)l*512;
      #endif
      close(handle);
    }
    else
    {
      return ERRORX_(OPEN_DEVICE,errno,String_cString(deviceName));
    }
  }

  // check if mounted
  mtab = setmntent("/etc/mtab","r");
  if (mtab != NULL)
  {
    while (getmntent_r(mtab,&mountEntry,buffer,sizeof(buffer)) != NULL)
    {
      if (String_equalsCString(deviceName,mountEntry.mnt_fsname))
      {
        deviceInfo->mountedFlag = TRUE;
        break;
      }
    }
    endmntent(mtab);
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
