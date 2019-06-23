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
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysmacros.h>
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
#include <mntent.h>
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
    error = ERRORX_(EXEC_FAIL,errorCode,"%s",String_cString(s));

    // free resources
    String_delete(s);

    return error;
  }

  pid_t  pid;
  Errors error;
  int    status;

  assert(arguments != NULL);
  assert(arguments[0] != NULL);

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
    error = getError(errno);
    return error;
  }

  if (waitpid(pid,&status,0) == -1)
  {
    error = ERRORX_(EXEC_FAIL,errno,"%s",command);
  }
  if      (WIFEXITED(status))
  {
    if (WEXITSTATUS(status) == 0)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = getError(WEXITSTATUS(status));
    }
  }
  else if (WIFSIGNALED(status))
  {
    error = getError(WTERMSIG(status));
  }
  else
  {
    error = ERROR_UNKNOWN;
  }

  return error;
}

/*---------------------------------------------------------------------*/

Errors Device_open(DeviceHandle *deviceHandle,
                   ConstString  deviceName,
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
        return ERRORX_(OPEN_DEVICE,errno,"%s",String_cString(deviceName));
      }
      break;
    case DEVICE_OPEN_WRITE:
      deviceHandle->file = fopen(String_cString(deviceName),"r+b");
      if (deviceHandle->file == NULL)
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

  // get device size
  if (FSEEK(deviceHandle->file,(off_t)0,SEEK_END) == -1)
  {
    error = ERRORX_(IO,errno,"%s",String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  n = FTELL(deviceHandle->file);
  if (n == (off_t)(-1))
  {
    error = ERRORX_(IO,errno,"%s",String_cString(deviceName));
    fclose(deviceHandle->file);
    return error;
  }
  if (FSEEK(deviceHandle->file,(off_t)0,SEEK_SET) == -1)
  {
    error = ERRORX_(IO,errno,"%s",String_cString(deviceName));
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
  assert(deviceHandle->file != NULL);
  assert(buffer != NULL);

  n = fwrite(buffer,1,bufferLength,deviceHandle->file);
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
  assert(deviceHandle->file != NULL);
  assert(offset != NULL);

  n = FTELL(deviceHandle->file);
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
  assert(deviceHandle->file != NULL);

  if (FSEEK(deviceHandle->file,(off_t)offset,SEEK_SET) == -1)
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
  TextMacro  textMacros[2];

  assert(mountPointName != NULL);

  TEXT_MACRO_N_STRING (textMacros[0],"%device",   deviceName,    NULL);
  TEXT_MACRO_N_STRING (textMacros[1],"%directory",mountPointName,NULL);

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
                             textMacros,SIZE_OF_ARRAY(textMacros),
                             CALLBACK_NULL,
                             CALLBACK_NULL
                            );
}

Errors Device_umount(ConstString umountCommand,
                     ConstString mountPointName
                    )
{
  TextMacro textMacros[1];
  Errors    error;

  assert(mountPointName != NULL);

  TEXT_MACRO_N_STRING (textMacros[0],"%directory",mountPointName,NULL);

  if (!String_isEmpty(umountCommand))
  {
    error = Misc_executeCommand(String_cString(umountCommand),
                                textMacros,SIZE_OF_ARRAY(textMacros),
                                CALLBACK_NULL,
                                CALLBACK_NULL
                               );
  }
  else
  {
    error = Misc_executeCommand("/bin/umount %directory",
                                textMacros,SIZE_OF_ARRAY(textMacros),
                                CALLBACK_NULL,
                                CALLBACK_NULL
                               );
    if (error != ERROR_NONE)
    {
      error = Misc_executeCommand("sudo /bin/umount %directory",
                                  textMacros,SIZE_OF_ARRAY(textMacros),
                                  CALLBACK_NULL,
                                  CALLBACK_NULL
                                 );
    }
  }

  return error;
}

bool Device_isMounted(ConstString mountPointName)
{
  bool          mounted;
  String        absoluteName;
  FILE          *mtab;
  struct mntent mountEntry;
  char          buffer[4096];
  #if   defined(PLATFORM_LINUX)
    char *s;
  #elif defined(PLATFORM_WINDOWS)
    char *s;
  #endif /* PLATFORM_... */

  assert(mountPointName != NULL);

  mounted = FALSE;

  // get absolute name
  #if   defined(PLATFORM_LINUX)
    s = realpath(String_cString(mountPointName),NULL);
    absoluteName = String_newCString(s);
    free(s);
  #elif defined(PLATFORM_WINDOWS)
    s = _fullpath(NULL,String_cString(mountPointName),0);
    absoluteName = String_newCString(s);
    free(s);
  #endif /* PLATFORM_... */

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
  String_delete(absoluteName);

  return mounted;
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
      return ERRORX_(IO,0,"invalid format");
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

Errors Device_getInfo(DeviceInfo  *deviceInfo,
                      ConstString deviceName
                     )
{
  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  return Device_getInfoCString(deviceInfo,String_cString(deviceName));
}

Errors Device_getInfoCString(DeviceInfo *deviceInfo,
                             const char *deviceName
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
  struct mntent mountEntry;
  char          buffer[4096];

  assert(deviceInfo != NULL);
  assert(deviceName != NULL);

  // initialize variables
  deviceInfo->type        = DEVICE_TYPE_UNKNOWN;
  deviceInfo->size        = 0LL;
  deviceInfo->blockSize   = 0L;
//  deviceInfo->freeBlocks  = 0LL;
//  deviceInfo->totalBlocks = 0LL;
  deviceInfo->mounted     = FALSE;

  // get device meta data
  if (LSTAT(deviceName,&fileStat) == 0)
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
    // try to get block size, total size
    handle = open(deviceName,O_RDONLY);
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
  }

  // check if mounted
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

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
