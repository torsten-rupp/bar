/***********************************************************************\
*
* Contents: Backup ARchiver device functions
* Systems: all
*
\***********************************************************************/

#ifndef __DEVICES__
#define __DEVICES__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// device open modes
typedef enum
{
  DEVICE_OPEN_READ,
  DEVICE_OPEN_WRITE
} DeviceModes;

#ifndef NDEBUG
  #define DEVICE_DEBUG_EMULATE_BLOCK_DEVICE "DEBUG_EMULATE_BLOCK_DEVICE"
#endif

/***************************** Datatypes *******************************/

// device i/o handle
typedef struct
{
  String name;
  int    handle;
  uint64 index;
  uint64 size;
} DeviceHandle;

// device list read handle
typedef struct
{
  #if   defined(PLATFORM_LINUX)
    FILE *file;
    char line[256];
    char deviceName[256];
    bool readFlag;
  #elif defined(PLATFORM_WINDOWS)
    DWORD logicalDrives;
    uint  i;
  #endif /* PLATFORM_... */
} DeviceListHandle;

// device types
typedef enum
{
  DEVICE_TYPE_NONE,

  DEVICE_TYPE_CHARACTER,
  DEVICE_TYPE_BLOCK,

  DEVICE_TYPE_UNKNOWN
} DeviceTypes;

// device permission
typedef uint32 DevicePermission;

// device system info data
typedef struct
{
  DeviceTypes      type;
  uint64           size;                     // total size [bytes]
  ulong            blockSize;                // size of a block [bytes]
// NYI
//  int64       freeBlocks;       // number of free blocks
//  int64       totalBlocks;      // total number of blocks
  uint64           timeLastAccess;           // timestamp of last access
  uint64           timeModified;             // timestamp of last modification
  uint64           timeLastChanged;          // timestamp of last changed
  uint32           userId;                   // user id
  uint32           groupId;                  // group id
  DevicePermission permission;               // permission flags
  uint32           major,minor;              // special type major/minor number
  uint64           id;                       // unique id (e. g. inode number)
  bool             mounted;                  // TRUE iff device is currently mounted
} DeviceInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Device_open
* Purpose: open device
* Input  : deviceHandle - device handle
*          deviceName   - device name
*          deviceMode   - device open mode; see DEVICE_OPEN_*
* Output : deviceHandle - device handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_open(DeviceHandle *deviceHandle,
                   ConstString  deviceName,
                   DeviceModes  deviceMode
                  );

/***********************************************************************\
* Name   : Device_close
* Purpose: close device
* Input  : deviceHandle - device handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_close(DeviceHandle *deviceHandle);

/***********************************************************************\
* Name   : Device_eof
* Purpose: check if end-of-device
* Input  : deviceHandle - device handle
* Output : -
* Return : TRUE if end-of-device, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Device_eof(DeviceHandle *deviceHandle);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE bool Device_eof(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);
  assert(deviceHandle->handle != -1);

  return deviceHandle->index >= deviceHandle->size;
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Device_read
* Purpose: read data from device
* Input  : deviceHandle - device handle
*          buffer       - buffer for data to read
*          bufferLength - length of data to read
* Output : bytesRead - bytes read (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : if bytesRead is not given (NULL) reading less than
*          bufferLength bytes result in an i/o error
\***********************************************************************/

Errors Device_read(DeviceHandle *deviceHandle,
                   void         *buffer,
                   ulong        bufferLength,
                   ulong        *bytesRead
                  );

/***********************************************************************\
* Name   : Device_write
* Purpose: write data into device
* Input  : deviceHandle - device handle
*          buffer       - buffer for data to write
*          bufferLength - length of data to write
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_write(DeviceHandle *deviceHandle,
                    const void   *buffer,
                    ulong        bufferLength
                   );

/***********************************************************************\
* Name   : Device_size
* Purpose: get device size
* Input  : deviceHandle - device handle
* Output : -
* Return : size of device (in bytes) or 0 when size cannot be
*          determined
* Notes  : -
\***********************************************************************/

uint64 Device_getSize(DeviceHandle *deviceHandle);

/***********************************************************************\
* Name   : Device_tell
* Purpose: get current position in device
* Input  : deviceHandle - device handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_tell(DeviceHandle *deviceHandle,
                   uint64       *offset
                  );

/***********************************************************************\
* Name   : Device_seek
* Purpose: seek in device
* Input  : deviceHandle - device handle
*          offset       - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_seek(DeviceHandle *deviceHandle,
                   uint64       offset
                  );

/***********************************************************************\
* Name   : Device_getUsedBlockBitmap
* Purpose: get device type
* Input  : deviceName - device name
* Output : -
* Return : device type; see DeviceTypes
* Notes  : -
\***********************************************************************/

// TODO: implement
#if 0
bool Device_getUsedBlocks(DeviceHandle *deviceHandle,
                          uint64       blockOffset,
                          uint64       blockCount,
                          Bitmap       *usedBlocksBitmap
                         );
#endif

/***********************************************************************\
* Name   : Device_mount
* Purpose: mount device
* Input  : mountCommand   - mount command or NULL
*          mountPointName - mount point name
*          deviceName     - device name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_mount(ConstString mountCommand,
                    ConstString mountPointName,
                    ConstString deviceName
                   );

/***********************************************************************\
* Name   : Device_umount
* Purpose: unmount device
* Input  : umountCommand  - umount command or NULL
*          mountPointName - mount point name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_umount(ConstString umountCommand,
                     ConstString mountPointName
                    );

/***********************************************************************\
* Name   : Device_isMounted
* Purpose: check if device is mounted
* Input  : mountPointName - mount point name
* Output : -
* Return : TRUE iff device is currently mounted
* Notes  : -
\***********************************************************************/

bool Device_isMounted(ConstString mountPointName);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Device_openDeviceList
* Purpose: open device list for reading
* Input  : deviceListHandle - device list handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_openDeviceList(DeviceListHandle *deviceListHandle);

/***********************************************************************\
* Name   : Device_closeDeviceList
* Purpose: close device list
* Input  : deviceListHandle - device list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Device_closeDeviceList(DeviceListHandle *deviceListHandle);

/***********************************************************************\
* Name   : Device_endOfDeviceList
* Purpose: check if end of devices reached
* Input  : deviceListHandle - device list handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Device_endOfDeviceList(DeviceListHandle *deviceListHandle);

/***********************************************************************\
* Name   : Device_readDeviceList
* Purpose: read next (block) device list entry
* Input  : deviceListHandle - device list handle
*          deviceName       - device name variable
* Output : deviceName - next device name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_readDeviceList(DeviceListHandle *deviceListHandle,
                             String           deviceName
                            );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Device_exists, Device_existsCString
* Purpose: check device exists
* Input  : deviceName - device name
* Output : -
* Return : TRUE iff device exists
* Notes  : -
\***********************************************************************/

bool Device_exists(ConstString deviceName);
bool Device_existsCString(const char *deviceName);

/***********************************************************************\
* Name   : Device_getInfo, Device_getInfoCString
* Purpose: get device info
* Input  : deviceInfo - device info variable to fill
*          deviceName - device name
*          sizesFlag  - TRUE to detect block size+device size
* Output : deviceInfo - device info
* Return : ERROR_NONE or error code
* Notes  : only in debug version: if environment variable
*          DEBUG_EMULATE_BLOCK_DEVICE is set to a file name a device of
*          that name is emulated
\***********************************************************************/

Errors Device_getInfo(DeviceInfo  *deviceInfo,
                      ConstString deviceName,
                      bool        sizesFlag
                     );
Errors Device_getInfoCString(DeviceInfo *deviceInfo,
                             const char *deviceName,
                             bool        sizesFlag
                            );

#ifdef __cplusplus
  }
#endif

#endif /* __DEVICES__ */

/* end of file */
