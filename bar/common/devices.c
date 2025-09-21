/***********************************************************************\
*
* Contents: device functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT
  #include <sys/wait.h>
#endif
#ifdef HAVE_SYS_SYSMACROS_H
  #include <sys/sysmacros.h>
#endif
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <fcntl.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
#endif
#ifdef HAVE_MNTENT_H
  #include <mntent.h>
#endif
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/types.h>
  #include <linux/fs.h>
  #ifdef HAVE_LIBMOUNT_LIBMOUNT_H
    #include "libmount/libmount.h"
  #endif
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/misc.h"

#include "errors.h"

#include "devices.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#if   defined(PLATFORM_LINUX)
  #define O_BINARY 0
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/
#if HAVE_LSEEK64
  #define LSEEK lseek64
#else
  #define LSEEK lseek
#endif

#ifdef HAVE_STAT64
  #define STAT stat64
  typedef struct stat64 FileStat;
#elif HAVE___STAT64
  #define STAT stat64
  typedef struct __stat64 FileStat;
#elif HAVE__STATI64
  #define STAT _stati64
  typedef struct _stati64 FileStat;
#elif HAVE_STAT
  #define STAT stat
  typedef struct stat FileStat;
#else
  #error No struct stat64 nor struct __stat64
#endif

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugGetEmulateBlockDevice
* Purpose: get emulated block device file name
* Input  : -
* Output : -
* Return : emulated block device file name or NULL
* Notes  : -
\***********************************************************************/

LOCAL_INLINE char *debugGetEmulateBlockDevice(void)
{
  return getenv(DEVICE_DEBUG_EMULATE_BLOCK_DEVICE);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : getDeviceInfo
* Purpose: get device info
* Input  : getDeviceInfo - device info variable
*          deviceName    - device name
*          sizesFlag     - TRUE to detect block size+device size
* Output : getDeviceInfo - device info
* Return : ERROR_NONE or eror code
* Notes  : only in debug version: if environment variable
*          DEBUG_EMULATE_BLOCK_DEVICE is set to a file name a device
*          of that name is emulated
\***********************************************************************/

LOCAL Errors getDeviceInfo(DeviceInfo *deviceInfo,
                           const char *deviceName,
                           bool        sizesFlag
                          )
{
  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  // initialize variables
  deviceInfo->type            = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size            = 0LL;
  deviceInfo->blockSize       = 0L;
//TODO: NYI
//  deviceInfo->freeBlocks  = 0LL;
//  deviceInfo->totalBlocks = 0LL;
  deviceInfo->timeLastAccess  = 0L;
  deviceInfo->timeModified    = 0L;
  deviceInfo->timeLastChanged = 0L;
  deviceInfo->userId          = 0L;
  deviceInfo->groupId         = 0L;
  deviceInfo->permission      = 0L;
  deviceInfo->major           = 0L;
  deviceInfo->minor           = 0L;
  deviceInfo->id              = 0L;
  deviceInfo->mounted         = FALSE;

  #if   defined(PLATFORM_LINUX)
    // check if character or block device
    FileStat fileStat;
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        CStringTokenizer stringTokenizer;
        stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
        const char *emulateDeviceName,*emulateFileName;
        if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
            && stringEquals(deviceName,emulateDeviceName))
        {
          // emulate block device
          if (stringGetNextToken(&stringTokenizer,&emulateFileName))
          {
            if (STAT(emulateFileName,&fileStat) != 0)
            {
              return ERRORX_(IO,errno,"%E",errno);
            }
          }
          else
          {
            if (STAT(emulateDeviceName,&fileStat) != 0)
            {
              return ERRORX_(IO,errno,"%E",errno);
            }
          }
        }
        else
        {
          // use block device
          if (STAT(deviceName,&fileStat) != 0)
          {
            return ERRORX_(IO,errno,"%E",errno);
          }
          if (!S_ISCHR(fileStat.st_mode) && !S_ISBLK(fileStat.st_mode))
          {
            return ERRORX_(NOT_A_DEVICE,0,"%s",deviceName);
          }
        }
        stringTokenizerDone(&stringTokenizer);
      }
      else
      {
        if (STAT(deviceName,&fileStat) != 0)
        {
          return ERRORX_(IO,errno,"%E",errno);
        }
        if (!S_ISCHR(fileStat.st_mode) && !S_ISBLK(fileStat.st_mode))
        {
          return ERRORX_(NOT_A_DEVICE,0,"%s",deviceName);
        }
      }
    #else /* NDEBUG */
      if (STAT(deviceName,&fileStat) != 0)
      {
        return ERRORX_(IO,errno,"%E",errno);
      }
      if (!S_ISCHR(fileStat.st_mode) && !S_ISBLK(fileStat.st_mode))
      {
        return ERRORX_(NOT_A_DEVICE,0,"%s",deviceName);
      }
    #endif /* not NDEBUG */

    // init device info
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

    // get device type
    #ifndef NDEBUG
      debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        CStringTokenizer stringTokenizer;
        stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
        const char *emulateDeviceName;
        if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
            && stringEquals(deviceName,emulateDeviceName)
           )
        {
          // emulate block device
          deviceInfo->type = DEVICE_TYPE_BLOCK;
        }
        else
        {
          // use block device
          if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
          else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
        }
        stringTokenizerDone(&stringTokenizer);
      }
      else
      {
        if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
        else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
      }
    #else /* NDEBUG */
      if      (S_ISCHR(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_CHARACTER;
      else if (S_ISBLK(fileStat.st_mode)) deviceInfo->type = DEVICE_TYPE_BLOCK;
    #endif /* not NDEBUG */

    if (deviceInfo->type == DEVICE_TYPE_BLOCK)
    {
      // get block size, total size
      #ifndef NDEBUG
        const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
        if (debugEmulateBlockDevice != NULL)
        {
          CStringTokenizer stringTokenizer;
          stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
          const char *emulateDeviceName,*emulateFileName;
          if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
              && stringEquals(deviceName,emulateDeviceName)
             )
          {
            // emulate block device
            if (stringGetNextToken(&stringTokenizer,&emulateFileName))
            {
              if      (STAT(emulateFileName,&fileStat) == 0)
              {
                deviceInfo->blockSize = 512;
                deviceInfo->size      = fileStat.st_size;
              }
              else if (!sizesFlag)
              {
                deviceInfo->blockSize = 0;
                deviceInfo->size      = 0LL;
              }
              else
              {
                return ERRORX_(IO,errno,"%E",errno);
              }
            }
            else
            {
              if      (STAT(deviceName,&fileStat) == 0)
              {
                deviceInfo->blockSize = 512;
                deviceInfo->size      = fileStat.st_size;
              }
              else if (!sizesFlag)
              {
                deviceInfo->blockSize = 0;
                deviceInfo->size      = 0LL;
              }
              else
              {
                return ERRORX_(IO,errno,"%E",errno);
              }
            }
          }
          else
          {
            // use block device
            int handle = open(deviceName,O_RDONLY);
            if      (handle != -1)
            {
              #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
                int i;
                if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
              #endif
              #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
                long l;
                if (ioctl(handle,BLKGETSIZE,&l) == 0) deviceInfo->size      = (int64)l*512;
              #endif
              close(handle);
            }
            else if (!sizesFlag)
            {
              deviceInfo->blockSize = 0;
              deviceInfo->size      = 0LL;
            }
            else
            {
              return ERRORX_(IO,errno,"%E",errno);
            }
          }
          stringTokenizerDone(&stringTokenizer);
        }
        else
        {
          int handle = open(deviceName,O_RDONLY);
          if      (handle != -1)
          {
            #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
              int i;
              if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
            #endif
            #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
              long l;
              if (ioctl(handle,BLKGETSIZE,&l) == 0) deviceInfo->size      = (int64)l*512;
            #endif
            close(handle);
          }
          else if (!sizesFlag)
          {
            deviceInfo->blockSize = 0;
            deviceInfo->size      = 0LL;
          }
          else
          {
            return ERRORX_(IO,errno,"%E",errno);
          }
        }
      #else /* NDEBUG */
        int handle = open(deviceName,O_RDONLY);
        if      (handle != -1)
        {
          #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
            int i;
            if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
          #endif
          #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
            long l;
            if (ioctl(handle,BLKGETSIZE,&l) == 0) deviceInfo->size      = (int64)l*512;
          #endif
          close(handle);
        }
        else if (!sizesFlag)
        {
          deviceInfo->blockSize = 0;
          deviceInfo->size      = 0LL;
        }
        else
        {
          return ERRORX_(IO,errno,"%E",errno);
        }
      #endif /* not NDEBUG */
    }
  #elif defined(PLATFORM_WINDOWS)
    deviceInfo->type            = DEVICE_TYPE_BLOCK;
//TODO: NYI
    deviceInfo->timeLastAccess  = 0;
    deviceInfo->timeModified    = 0;
    deviceInfo->timeLastChanged = 0;
    deviceInfo->userId          = 0;
    deviceInfo->groupId         = 0;
    deviceInfo->permission      = 0;
    deviceInfo->major           = 0;
    deviceInfo->minor           = 0;
    deviceInfo->id              = 0;

    ULARGE_INTEGER freeSpace,totalSpace,totalFreeSpace;
    if      (GetDiskFreeSpaceEx(deviceName,&freeSpace,&totalSpace,&totalFreeSpace))
    {
      deviceInfo->blockSize = 0;
      deviceInfo->size      = (uint64)totalSpace.QuadPart;
    }
    else if (!sizesFlag)
    {
      deviceInfo->blockSize = 0;
      deviceInfo->size      = 0LL;
    }
    else
    {
      return ERRORX_(IO,errno,"%E",errno);
    }
  #endif /* PLATFORM_... */

  // check if mounted
  #if   defined(PLATFORM_LINUX)
    FILE *mtab = setmntent("/etc/mtab","r");
    if (mtab != NULL)
    {
      struct mntent mountEntry;
      char          buffer[4096];
      while (getmntent_r(mtab,&mountEntry,buffer,sizeof(buffer)) != NULL)
      {
        if (stringEquals(deviceName,mountEntry.mnt_fsname))
        {
          deviceInfo->mounted = TRUE;
          break;
        }
      }
      endmntent(mtab);
    }
  #elif defined(PLATFORM_WINDOWS)
// TODO: NYI
    #ifndef NDEBUG
    #endif
    deviceInfo->mounted = TRUE;
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

#if 0
//TODO: remove?
/***********************************************************************\
* Name   : findCommandInPath
* Purpose: find command in PATH
* Input  : command - command variable
*          name    - command name
* Output : command - command (if found)
* Return : TRUE if command found
* Notes  : -
\***********************************************************************/

LOCAL bool findCommandInPath(String command, const char *name)
{
  assert(command != NULL);
  assert(name != NULL);

  bool foundFlag = FALSE;

  const char *path = getenv("PATH");
  if (path != NULL)
  {
    StringTokenizer stringTokenizer;
    String_initTokenizerCString(&stringTokenizer,path,":","",FALSE);
    ConstString token;
    while (String_getNextToken(&stringTokenizer,&token,NULL) && !foundFlag)
    {
      File_setFileName(command,token);
      File_appendFileNameCString(command,name);
      foundFlag = File_exists(command);
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return foundFlag;
}

/***********************************************************************\
* Name   : execute
* Purpose: execute command
* Input  : command   - command
*          arguments - arguments
* Output : -
* Return : ERROR_NONE or eror code
* Notes  : -
\***********************************************************************/

LOCAL Errors execute(const char *command, const char *arguments[])
{
  /***********************************************************************\
  * Name   : getError
  * Purpose: get error
  * Input  : errorCode - error number
  * Output : -
  * Return : error
  * Notes  : -
  \***********************************************************************/

  auto Errors getError(int errorCode);
  Errors getError(int errorCode)
  {
    Errors error;

    assert(arguments != NULL);
    assert(arguments[0] != NULL);

    // init variables
    String s = String_new();

    // format command
    String_joinCString(s,command,' ');
    uint i = 1;
    while (arguments[i] != NULL)
    {
      String_joinCString(s,arguments[i],' ');
      i++;
    }

    // get error
    error = ERRORX_(EXEC_FAIL,0,"'%s', error %d",String_cString(s),errorCode);

    // free resources
    String_delete(s);

    return error;
  }

  Errors error;

  assert(arguments != NULL);
  assert(arguments[0] != NULL);

  // init variables
  String text = String_new();

  // format command
  String_joinCString(text,command,' ');
  uint i = 1;
  while (arguments[i] != NULL)
  {
    String_joinCString(text,arguments[i],' ');
    i++;
  }

  pid_t pid = fork();
  if      (pid == 0)
  {
    // suppress output/input on stdout/stderr/stdin
    close(STDERR_FILENO);
    close(STDOUT_FILENO);
    close(STDIN_FILENO);

    // execute command
    execvp(command,(char**)arguments);

    // in case exec() fail, return a default exitcode
    exit(1);
  }
  else if (pid < 0)
  {
    error = ERRORX_(EXEC_FAIL,0,"'%s', error %d",String_cString(text),errno);
    String_delete(text);
    return error;
  }

  int status;
  if (waitpid(pid,&status,0) == -1)
  {
    error = ERRORX_(EXEC_FAIL,0,"'%s', error %d",String_cString(text),errno);
  }
  if      (WIFEXITED(status))
  {
    exitcode = WEXITSTATUS(status);
    if (exitcode == 0)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(EXEC_FAIL,0,"'%s', exitcode %d",String_cString(text),exitcode);
      String_delete(text);
    }
  }
  else if (WIFSIGNALED(status))
  {
    error = ERRORX_(EXEC_TERMINATE,0,"'%s', signal %d",String_cString(text),WTERMSIG(status));
  }
  else
  {
    error = ERROR_UNKNOWN;
  }

  // free resources
  String_delete(text);

  return error;
}
#endif

/*---------------------------------------------------------------------*/

Errors Device_open(DeviceHandle *deviceHandle,
                   ConstString  deviceName,
                   DeviceModes  deviceMode
                  )
{
  assert(deviceHandle != NULL);
  assert(deviceName != NULL);

  return Device_openCString(deviceHandle,String_cString(deviceName),deviceMode);
}

Errors Device_openCString(DeviceHandle *deviceHandle,
                          const char   *deviceName,
                          DeviceModes  deviceMode
                         )
{
  Errors error;

  assert(deviceHandle != NULL);
  assert(deviceName != NULL);

  // open device
  #ifdef HAVE_O_LARGEFILE
    #define FLAGS O_BINARY|O_LARGEFILE
  #else
    #define FLAGS O_BINARY
  #endif
  switch (deviceMode)
  {
    case DEVICE_OPEN_READ:
      {
        #ifndef NDEBUG
          const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
          if (debugEmulateBlockDevice != NULL)
          {
            CStringTokenizer stringTokenizer;
            stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
            const char *emulateDeviceName,*emulateFileName;
            if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
                && stringEquals(deviceName,emulateDeviceName)
               )
            {
              // emulate block device
              if (stringGetNextToken(&stringTokenizer,&emulateFileName))
              {
                deviceHandle->handle = open(emulateFileName,FLAGS|O_RDONLY);
              }
              else
              {
                deviceHandle->handle = open(emulateDeviceName,FLAGS|O_RDONLY);
              }
            }
            else
            {
              // use block device
              deviceHandle->handle = open(deviceName,FLAGS|O_RDONLY);
            }
            stringTokenizerDone(&stringTokenizer);
          }
          else
          {
            // use block device
            deviceHandle->handle = open(deviceName,FLAGS|O_RDONLY);
          }
        #else /* NDEBUG */
          deviceHandle->handle = open(deviceName,FLAGS|O_RDONLY);
        #endif /* not NDEBUG */
        if (deviceHandle->handle == -1)
        {
          return ERRORX_(OPEN_DEVICE,errno,"%s",deviceName);
        }
      }
      break;
    case DEVICE_OPEN_WRITE:
      {
// TODO:
        #ifndef NDEBUG
          const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
          if (debugEmulateBlockDevice != NULL)
          {
            CStringTokenizer stringTokenizer;
            stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
            const char *emulateDeviceName,*emulateFileName;
            if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
                && stringEquals(deviceName,emulateDeviceName)
               )
            {
              // emulate block device
              if (stringGetNextToken(&stringTokenizer,&emulateFileName))
              {
                deviceHandle->handle = open(emulateFileName,FLAGS|O_RDWR);
              }
              else
              {
                deviceHandle->handle = open(emulateDeviceName,FLAGS|O_RDWR);
              }
            }
            else
            {
              // use block device
              deviceHandle->handle = open(deviceName,FLAGS|O_RDWR);
            }
            stringTokenizerDone(&stringTokenizer);
          }
          else
          {
            deviceHandle->handle = open(deviceName,FLAGS|O_RDWR);
          }
        #else /* NDEBUG */
          deviceHandle->handle = open(deviceName,FLAGS|O_RDWR);
        #endif /* not NDEBUG */
        if (deviceHandle->handle == -1)
        {
          return ERRORX_(OPEN_DEVICE,errno,"%s",deviceName);
        }
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  #undef FLAGS

  // get device size
  off_t n = LSEEK(deviceHandle->handle,(off_t)0,SEEK_END);
  if (n == (off_t)(-1))
  {
    error = ERRORX_(IO,errno,"%s",deviceName);
    close(deviceHandle->handle);
    return error;
  }
  if (LSEEK(deviceHandle->handle,(off_t)0,SEEK_SET) == -1)
  {
    error = ERRORX_(IO,errno,"%s",deviceName);
    close(deviceHandle->handle);
    return error;
  }

  // initialize handle
  deviceHandle->name  = String_newCString(deviceName);
  deviceHandle->index = 0LL;
  deviceHandle->size  = (int64)n;

  return ERROR_NONE;
}

Errors Device_close(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->name != NULL);

  close(deviceHandle->handle);
  deviceHandle->handle = -1;
  String_delete(deviceHandle->name);

  return ERROR_NONE;
}

Errors Device_read(DeviceHandle *deviceHandle,
                   void         *buffer,
                   ulong        bufferLength,
                   ulong        *bytesRead
                  )
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(buffer != NULL);

  ssize_t n = read(deviceHandle->handle,buffer,bufferLength);
  if (   (n == (off_t)(-1))
      || ((n < (ssize_t)bufferLength) && (bytesRead == NULL))
     )
  {
    return ERRORX_(IO,errno,"%s",String_cString(deviceHandle->name));
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
  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(buffer != NULL);

  ssize_t n = write(deviceHandle->handle,buffer,bufferLength);
  if (n > 0) deviceHandle->index += n;
  if (deviceHandle->index > deviceHandle->size) deviceHandle->size = deviceHandle->index;
  if (n != (ssize_t)bufferLength)
  {
    return ERRORX_(IO,errno,"%s",String_cString(deviceHandle->name));
  }

  return ERROR_NONE;
}

uint64 Device_getSize(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  return deviceHandle->size;
}

Errors Device_tell(DeviceHandle *deviceHandle, uint64 *offset)
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(offset != NULL);

  off_t n = LSEEK(deviceHandle->handle,(off_t)0,SEEK_CUR);
  if (n == (off_t)(-1))
  {
    return ERRORX_(IO,errno,"%s",String_cString(deviceHandle->name));
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
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);

  if (LSEEK(deviceHandle->handle,(off_t)offset,SEEK_SET) == -1)
  {
    return ERRORX_(IO,errno,"%s",String_cString(deviceHandle->name));
  }
  deviceHandle->index = offset;

  return ERROR_NONE;
}

Errors Device_mount(ConstString mountCommand,
                    ConstString mountPointName,
                    ConstString deviceName
                   )
{
  assert(mountPointName != NULL);

  Errors error;
#ifdef HAVE_LIBMOUNT
  if      (!String_isEmpty(mountCommand))
  {
    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING ("%device",   deviceName,    NULL);
      TEXT_MACRO_X_STRING ("%directory",mountPointName,NULL);
    }
    error = Misc_executeCommand(String_cString(mountCommand),
                                textMacros.data,
                                textMacros.count,
                                NULL, // commandLine
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL)
                               );
  }
  else
  {
    struct libmnt_table *libmountTable = mnt_new_table_from_file("/etc/fstab");
    if (libmountTable != NULL)
    {
      struct libmnt_fs *libmountFileSystem = mnt_table_find_target(libmountTable, String_cString(mountPointName), MNT_ITER_FORWARD);
      if (libmountFileSystem != NULL)
      {
        struct libmnt_context *libmountContext = mnt_new_context();
        if (libmountContext != NULL)
        {
          mnt_context_set_fs(libmountContext, libmountFileSystem);
          mnt_context_set_target(libmountContext, String_cString(mountPointName));

          if (   (mnt_context_mount(libmountContext) == 0)
              && (mnt_context_get_status(libmountContext) == 1)
             )
          {
            error = ERROR_NONE;
          }
          else
          {
            error = ERRORX_(MOUNT,mnt_context_get_status(libmountContext),"'%s'", String_cString(deviceName));
          }

          mnt_free_context(libmountContext);
        }
        else
        {
          error = ERRORX_(MOUNT,0,"create mount context");
        }
      }
      else
      {
        error = ERRORX_(MOUNT,0,"mount point '%s' not found", String_cString(mountPointName));
      }

      mnt_unref_table(libmountTable);
    }
    else
    {
      error = ERRORX_(MOUNT,0,"read /etc/fstab fail");
    }
  }
#else
  const char *command;
  if      (mountCommand != NULL)
  {
    command = String_cString(mountCommand);
  }
  else if (!String_isEmpty(deviceName))
  {
    command = "/bin/mount -p 0 %device %directory";
  }
  else
  {
    command = "/bin/mount -p 0 %directory";
  }

  TextMacros (textMacros,2);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("%device",   deviceName,    NULL);
    TEXT_MACRO_X_STRING ("%directory",mountPointName,NULL);
  }
  error = Misc_executeCommand(command,
                              textMacros.data,
                              textMacros.count,
                              NULL, // commandLine
                              CALLBACK_(NULL,NULL),
                              CALLBACK_(NULL,NULL),
                              WAIT_FOREVER
                             );
#endif

  return error;
}

Errors Device_umount(ConstString umountCommand,
                     ConstString mountPointName
                    )
{
  Errors error;

  assert(mountPointName != NULL);

  TextMacros (textMacros,1);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("%directory",mountPointName,NULL);
  }

  if (!String_isEmpty(umountCommand))
  {
    error = Misc_executeCommand(String_cString(umountCommand),
                                textMacros.data,
                                textMacros.count,
                                NULL, // commandLine
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL),
                                WAIT_FOREVER
                               );
  }
  else
  {
    error = Misc_executeCommand("/bin/umount %directory",
                                textMacros.data,
                                textMacros.count,
                                NULL, // commandLine
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL),
                                WAIT_FOREVER
                               );
    if (error != ERROR_NONE)
    {
      error = Misc_executeCommand("sudo /bin/umount %directory",
                                  textMacros.data,
                                  textMacros.count,
                                  NULL, // commandLine
                                  CALLBACK_(NULL,NULL),
                                  CALLBACK_(NULL,NULL),
                                  WAIT_FOREVER
                                 );
    }
  }

  return error;
}

bool Device_isMounted(ConstString mountPointName)
{
  assert(mountPointName != NULL);

  bool mounted = FALSE;

  // get absolute name
  String absoluteName = String_new();
  #if   defined(PLATFORM_LINUX)
    char *s = realpath(String_cString(mountPointName),NULL);
    String_setCString(absoluteName,s);
    free(s);

    FILE *mtab = setmntent("/etc/mtab","r");
    if (mtab != NULL)
    {
      struct mntent mountEntry;
      char          buffer[4096];
      while (getmntent_r(mtab,&mountEntry,buffer,sizeof(buffer)) != NULL)
      {
        if (   String_equalsCString(mountPointName,mountEntry.mnt_fsname)
            || String_equalsCString(mountPointName,mountEntry.mnt_dir)
            || String_equalsCString(absoluteName,mountEntry.mnt_fsname)
            || String_equalsCString(absoluteName,mountEntry.mnt_dir)
           )
        {
          mounted = TRUE;
          break;
        }
      }
      endmntent(mtab);
    }
  #elif defined(PLATFORM_WINDOWS)
    char *s = _fullpath(NULL,String_cString(mountPointName),0);
    String_setCString(absoluteName,s);
    free(s);
// TODO: NYI
    mounted = TRUE;
  #endif /* PLATFORM_... */

  String_delete(absoluteName);

  return mounted;
}

/*---------------------------------------------------------------------*/

Errors Device_openDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    #ifdef HAVE_O_NOATIME
      // open directory (try first with O_NOATIME)
      int handle = open("/dev",O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY|O_NOATIME,0);
      if (handle == -1)
      {
        handle = open("/dev",O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY,0);
      }
      if (handle != -1)
      {
        // create directory handle
        deviceListHandle->dir = fdopendir(handle);
      }
      else
      {
        deviceListHandle->dir = NULL;
      }
    #else /* not HAVE_O_NOATIME */
      // open directory
      int handle = open("/dev",O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY,0);
      if (handle != -1)
      {
        // create directory handle
        deviceListHandle->dir = fdopendir(handle);
      }
    #endif /* HAVE_O_NOATIME */
    deviceListHandle->entry = NULL;
  #elif defined(PLATFORM_WINDOWS)
    deviceListHandle->logicalDrives = GetLogicalDrives();
    if (deviceListHandle->logicalDrives == 0)
    {
      return ERROR_(OPEN_FILE,GetLastError());
    }
    deviceListHandle->i = 0;
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

void Device_closeDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    assert(deviceListHandle->dir != NULL);

    closedir(deviceListHandle->dir);
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(deviceListHandle);
  #endif /* PLATFORM_... */
}

bool Device_endOfDeviceList(DeviceListHandle *deviceListHandle)
{
  assert(deviceListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    assert(deviceListHandle->dir != NULL);

    // read entry iff not read
    if (deviceListHandle->entry == NULL)
    {
      deviceListHandle->entry = readdir(deviceListHandle->dir);
    }

    // skip non-block devices
    while (   (deviceListHandle->entry != NULL)
           && (deviceListHandle->entry->d_type != DT_BLK)
          )
    {
      deviceListHandle->entry = readdir(deviceListHandle->dir);
    }

    return deviceListHandle->entry == NULL;
  #elif defined(PLATFORM_WINDOWS)
    return ((deviceListHandle->logicalDrives >> deviceListHandle->i) <= 0);
  #endif /* PLATFORM_... */
}

Errors Device_readDeviceList(DeviceListHandle *deviceListHandle,
                             String           deviceName,
                             DeviceInfo       *deviceInfo
                            )
{
  #define DEVICE_PREFIX "/dev/"

  assert(deviceListHandle != NULL);
  assert(deviceName != NULL);

  String_clear(deviceName);

  #if   defined(PLATFORM_LINUX)
    assert(deviceListHandle->dir != NULL);

    // read entry iff not read
    if (deviceListHandle->entry == NULL)
    {
      deviceListHandle->entry = readdir(deviceListHandle->dir);
    }

    // skip non-block devices
    while (   (deviceListHandle->entry != NULL)
           && (deviceListHandle->entry->d_type != DT_BLK)
          )
    {
      deviceListHandle->entry = readdir(deviceListHandle->dir);
    }
    if (deviceListHandle->entry == NULL)
    {
      return ERROR_DEVICE_NOT_FOUND;
    }

    // get entry name
    String_setCString(deviceName,"/dev/");
    File_appendFileNameCString(deviceName,deviceListHandle->entry->d_name);

    // try to get device info
    if (deviceInfo != NULL)
    {
      (void)getDeviceInfo(deviceInfo,String_cString(deviceName),FALSE);
    }

    // mark entry read
    deviceListHandle->entry = NULL;
  #elif defined(PLATFORM_WINDOWS)
    while ((deviceListHandle->logicalDrives >> deviceListHandle->i) > 0)
    {
      if (((1UL << deviceListHandle->i) & deviceListHandle->logicalDrives) != 0)
      {
        String_format(deviceName,"%c:",'A'+deviceListHandle->i);
        deviceListHandle->i++;
        break;
      }
      else
      {
        deviceListHandle->i++;
      }
    }

    // try to get device info
    if (deviceInfo != NULL)
    {
      (void)getDeviceInfo(deviceInfo,String_cString(deviceName),FALSE);
    }
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

bool Device_exists(ConstString deviceName)
{
  assert(deviceName != NULL);

  return Device_existsCString(String_cString(deviceName));
}

bool Device_existsCString(const char *deviceName)
{
  bool existsFlag;

  assert(deviceName != NULL);

  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      CStringTokenizer stringTokenizer;
      stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
      const char *emulateDeviceName,*emulateFileName;
      if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
          && stringEquals(deviceName,emulateDeviceName)
         )
      {
        // emulate block device
        if (stringGetNextToken(&stringTokenizer,&emulateFileName))
        {
          existsFlag = File_existsCString(emulateFileName);
        }
        else
        {
          existsFlag = File_existsCString(emulateDeviceName);
        }
      }
      else
      {
        // use block device
        existsFlag = File_existsCString(deviceName);
      }
      stringTokenizerDone(&stringTokenizer);
    }
    else
    {
      existsFlag = File_existsCString(deviceName);
    }
  #else /* NDEBUG */
    existsFlag = File_existsCString(deviceName);
  #endif /* not NDEBUG */

  return existsFlag;
}

Errors Device_getInfo(DeviceInfo  *deviceInfo,
                      ConstString deviceName,
                      bool        sizesFlag
                     )
{
  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  return getDeviceInfo(deviceInfo,String_cString(deviceName),sizesFlag);
}

Errors Device_getInfoCString(DeviceInfo *deviceInfo,
                             const char *deviceName,
                             bool        sizesFlag
                            )
{
  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  return getDeviceInfo(deviceInfo,deviceName,sizesFlag);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
