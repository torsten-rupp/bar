/***********************************************************************\
*
* Contents: Backup ARchiver device functions
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
  bool            foundFlag;
  const char      *path;
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(command != NULL);
  assert(name != NULL);

  foundFlag = FALSE;

  path = getenv("PATH");
  if (path != NULL)
  {
    String_initTokenizerCString(&stringTokenizer,path,":","",FALSE);
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
    String s;
    uint   i;

    assert(arguments != NULL);
    assert(arguments[0] != NULL);

    // init variables
    s = String_new();

    // format command
    String_joinCString(s,command,' ');
    i = 1;
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

  String text;
  uint   i;
  pid_t  pid;
  Errors error;
  int    status;

  assert(arguments != NULL);
  assert(arguments[0] != NULL);

  // init variables
  text = String_new();

  // format command
  String_joinCString(text,command,' ');
  i = 1;
  while (arguments[i] != NULL)
  {
    String_joinCString(text,arguments[i],' ');
    i++;
  }

  pid = fork();
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
  off_t  n;
  Errors error;
  #ifndef NDEBUG
    const char       *debugEmulateBlockDevice;
    CStringTokenizer stringTokenizer;
    const char       *emulateDeviceName,*emulateFileName;
  #endif /* not NDEBUG */

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
      #ifndef NDEBUG
        debugEmulateBlockDevice = debugGetEmulateBlockDevice();
        if (debugEmulateBlockDevice != NULL)
        {
          stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
          if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
              && String_equalsCString(deviceName,emulateDeviceName)
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
            deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDONLY);
          }
          stringTokenizerDone(&stringTokenizer);
        }
        else
        {
          // use block device
          deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDONLY);
        }
      #else /* NDEBUG */
        deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDONLY);
      #endif /* not NDEBUG */
      if (deviceHandle->handle == -1)
      {
        return ERRORX_(OPEN_DEVICE,errno,"%s",String_cString(deviceName));
      }
      break;
    case DEVICE_OPEN_WRITE:
// TODO:
      #ifndef NDEBUG
        debugEmulateBlockDevice = debugGetEmulateBlockDevice();
        if (debugEmulateBlockDevice != NULL)
        {
          stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
          if (   stringGetNextToken(&stringTokenizer,&emulateDeviceName)
              && String_equalsCString(deviceName,emulateDeviceName)
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
            deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDWR);
          }
          stringTokenizerDone(&stringTokenizer);
        }
        else
        {
          deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDWR);
        }
      #else /* NDEBUG */
        deviceHandle->handle = open(String_cString(deviceName),FLAGS|O_RDWR);
      #endif /* not NDEBUG */
      if (deviceHandle->handle == -1)
      {
        return ERRORX_(OPEN_DEVICE,errno,"%s",String_cString(deviceName));
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
  n = LSEEK(deviceHandle->handle,(off_t)0,SEEK_END);
  if (n == (off_t)(-1))
  {
    error = ERRORX_(IO,errno,"%s",String_cString(deviceName));
    close(deviceHandle->handle);
    return error;
  }
  if (LSEEK(deviceHandle->handle,(off_t)0,SEEK_SET) == -1)
  {
    error = ERRORX_(IO,errno,"%s",String_cString(deviceName));
    close(deviceHandle->handle);
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
  ssize_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(buffer != NULL);

  n = read(deviceHandle->handle,buffer,bufferLength);
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
  ssize_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(buffer != NULL);

  n = write(deviceHandle->handle,buffer,bufferLength);
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
  off_t n;

  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);
  assert(deviceHandle->index <= deviceHandle->size);
  assert(offset != NULL);

  n = LSEEK(deviceHandle->handle,(off_t)0,SEEK_CUR);
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
  const char *command;
  TextMacros (textMacros,2);

  assert(mountPointName != NULL);

  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("%device",   deviceName,    NULL);
    TEXT_MACRO_X_STRING ("%directory",mountPointName,NULL);
  }

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

  return Misc_executeCommand(command,
                             textMacros.data,
                             textMacros.count,
                             NULL, // commandLine
                             CALLBACK_(NULL,NULL),
                             CALLBACK_(NULL,NULL)
                            );
}

Errors Device_umount(ConstString umountCommand,
                     ConstString mountPointName
                    )
{
  TextMacros (textMacros,1);
  Errors     error;

  assert(mountPointName != NULL);

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
                                CALLBACK_(NULL,NULL)
                               );
  }
  else
  {
    error = Misc_executeCommand("/bin/umount %directory",
                                textMacros.data,
                                textMacros.count,
                                NULL, // commandLine
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL)
                               );
    if (error != ERROR_NONE)
    {
      error = Misc_executeCommand("sudo /bin/umount %directory",
                                  textMacros.data,
                                  textMacros.count,
                                  NULL, // commandLine
                                  CALLBACK_(NULL,NULL),
                                  CALLBACK_(NULL,NULL)
                                 );
    }
  }

  return error;
}

bool Device_isMounted(ConstString mountPointName)
{
  bool          mounted;
  String        absoluteName;
  #if   defined(PLATFORM_LINUX)
    FILE          *mtab;
    char          buffer[4096];
    struct mntent mountEntry;
    char          *s;
  #elif defined(PLATFORM_WINDOWS)
    char *s;
  #endif /* PLATFORM_... */

  assert(mountPointName != NULL);

  mounted = FALSE;

  // get absolute name
  absoluteName = String_new();
  #if   defined(PLATFORM_LINUX)
    s = realpath(String_cString(mountPointName),NULL);
    String_setCString(absoluteName,s);
    free(s);

    mtab = setmntent("/etc/mtab","r");
    if (mtab != NULL)
    {
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
    s = _fullpath(NULL,String_cString(mountPointName),0);
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
    assert(deviceListHandle->file != NULL);

    fclose(deviceListHandle->file);
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(deviceListHandle);
  #endif /* PLATFORM_... */
}

bool Device_endOfDeviceList(DeviceListHandle *deviceListHandle)
{
  #if   defined(PLATFORM_LINUX)
    #define PREFIX "/dev/"
    #define BUFFER_SIZE 256-strlen(PREFIX)

    uint        i,j;
    struct stat fileStat;
    char        buffer[BUFFER_SIZE];
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(deviceListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    assert(deviceListHandle->file != NULL);

    // read entry iff not read
    while (   !deviceListHandle->readFlag
           && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
          )
    {
      // skip leading spaces
      i = 0;
      while ((deviceListHandle->line[i] != '\0') && isspace(deviceListHandle->line[i]))
      {
        i++;
      }

      // skip trailing spaces
      j = i;
      while (deviceListHandle->line[j] != '\0')
      {
        j++;
      }
      while ((j > i) && isspace(deviceListHandle->line[j-1]))
      {
        j--;
      }
      deviceListHandle->line[j] = '\0';

      if (j > i)
      {
        // parse and get device name
        if (String_scanCString(&deviceListHandle->line[i],"%* %* %* %" STRINGIFY(BUFFER_SIZE) "s %*",buffer))
        {
          stringSet(deviceListHandle->deviceName,sizeof(deviceListHandle->deviceName),PREFIX);
          stringAppend(deviceListHandle->deviceName,sizeof(deviceListHandle->deviceName),buffer);
          if (stat(deviceListHandle->deviceName,&fileStat) == 0)
          {
            // mark entry available
            deviceListHandle->readFlag = TRUE;
          }
        }
      }
    }

    return !deviceListHandle->readFlag;
  #elif defined(PLATFORM_WINDOWS)
    return ((deviceListHandle->logicalDrives >> deviceListHandle->i) <= 0);
  #endif /* PLATFORM_... */
}

Errors Device_readDeviceList(DeviceListHandle *deviceListHandle,
                             String           deviceName
                            )
{
  #define DEVICE_PREFIX "/dev/"

  #if   defined(PLATFORM_LINUX)
    uint        i,j;
    struct stat fileStat;
    char        buffer[256-stringLength(DEVICE_PREFIX)];
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(deviceListHandle != NULL);
  assert(deviceName != NULL);

  String_clear(deviceName);

  #if   defined(PLATFORM_LINUX)
    assert(deviceListHandle->file != NULL);

    // read entry iff not read
    while (   !deviceListHandle->readFlag
           && (fgets(deviceListHandle->line,sizeof(deviceListHandle->line),deviceListHandle->file) != NULL)
          )
    {
      // skip leading spaces
      i = 0;
      while ((deviceListHandle->line[i] != '\0') && isspace(deviceListHandle->line[i]))
      {
        i++;
      }

      // skip trailing spaces
      j = i;
      while (deviceListHandle->line[j] != '\0')
      {
        j++;
      }
      while ((j > i) && isspace(deviceListHandle->line[j-1]))
      {
        j--;
      }
      deviceListHandle->line[j] = '\0';

      // if line is not empty set read flag
      if (j > i)
      {
        // parse and get device name
        if (String_scanCString(&deviceListHandle->line[i],"%* %* %* %256s %*",buffer))
        {
          stringSet(deviceListHandle->deviceName,sizeof(deviceListHandle->deviceName),"/dev/");
          stringAppend(deviceListHandle->deviceName,sizeof(deviceListHandle->deviceName),buffer);
          if (stat(deviceListHandle->deviceName,&fileStat) == 0)
          {
            // mark entry available
            deviceListHandle->readFlag = TRUE;
          }
        }
      }
    }

    if (deviceListHandle->readFlag)
    {
      String_setCString(deviceName,deviceListHandle->deviceName);

      // mark entry read
      deviceListHandle->readFlag = FALSE;
    }
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
  #ifndef NDEBUG
    const char       *debugEmulateBlockDevice;
    CStringTokenizer stringTokenizer;
    const char       *emulateDeviceName,*emulateFileName;
  #endif /* not NDEBUG */

  assert(deviceName != NULL);

  #ifndef NDEBUG
    debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
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

  return Device_getInfoCString(deviceInfo,String_cString(deviceName),sizesFlag);
}

Errors Device_getInfoCString(DeviceInfo *deviceInfo,
                             const char *deviceName,
                             bool        sizesFlag
                            )
{
  #if   defined(PLATFORM_LINUX)
    FileStat      fileStat;
    int      handle;
    #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
      int           i;
    #endif
    #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
      long          l;
    #endif
    struct mntent mountEntry;
    char          buffer[4096];
    FILE          *mtab;
  #elif defined(PLATFORM_WINDOWS)
    ULARGE_INTEGER freeSpace,totalSpace,totalFreeSpace;
  #endif /* PLATFORM_... */
  #ifndef NDEBUG
    const char       *debugEmulateBlockDevice;
    CStringTokenizer stringTokenizer;
    const char       *emulateDeviceName,*emulateFileName;
  #endif /* not NDEBUG */

  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  // initialize variables
  deviceInfo->type        = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size        = 0LL;
  deviceInfo->blockSize   = 0L;
//TODO: NYI
//  deviceInfo->freeBlocks  = 0LL;
//  deviceInfo->totalBlocks = 0LL;
  deviceInfo->mounted     = FALSE;

  #if   defined(PLATFORM_LINUX)
    // check if character or block device
    #ifndef NDEBUG
      debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
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
        stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
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
        debugEmulateBlockDevice = debugGetEmulateBlockDevice();
        if (debugEmulateBlockDevice != NULL)
        {
          stringTokenizerInit(&stringTokenizer,debugEmulateBlockDevice,",");
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
            handle = open(deviceName,O_RDONLY);
            if      (handle != -1)
            {
              #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
                if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
              #endif
              #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
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
        }
        else
        {
          handle = open(deviceName,O_RDONLY);
          if      (handle != -1)
          {
            #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
              if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
            #endif
            #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
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
        handle = open(deviceName,O_RDONLY);
        if      (handle != -1)
        {
          #if defined(HAVE_IOCTL) && defined(HAVE_BLKSSZGET)
            if (ioctl(handle,BLKSSZGET, &i) == 0) deviceInfo->blockSize = (ulong)i;
          #endif
          #if defined(HAVE_IOCTL) && defined(HAVE_BLKGETSIZE)
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
    mtab = setmntent("/etc/mtab","r");
    if (mtab != NULL)
    {
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
UNUSED_VARIABLE(debugEmulateBlockDevice);
UNUSED_VARIABLE(stringTokenizer);
UNUSED_VARIABLE(emulateDeviceName);
UNUSED_VARIABLE(emulateFileName);
    #endif
    deviceInfo->mounted = TRUE;
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
