/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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

#include "global.h"
#include "strings.h"
#include "bitmaps.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// device open modes
typedef enum
{
  DEVICE_OPEN_READ,
  DEVICE_OPEN_WRITE
} DeviceModes;

/***************************** Datatypes *******************************/

// device i/o handle
typedef struct
{
  String name;
  FILE   *file;
  uint64 index;
  uint64 size;
} DeviceHandle;

// device list read handle
typedef struct
{
  FILE *file;
  char line[256];
  bool readFlag;
} DeviceListHandle;

// device types
typedef enum
{
  DEVICE_TYPE_NONE,

  DEVICE_TYPE_CHARACTER,
  DEVICE_TYPE_BLOCK,

  DEVICE_TYPE_UNKNOWN
} DeviceTypes;

// device system info data
typedef struct
{
  DeviceTypes type;
  int64       size;             // total size [bytes]
  ulong       blockSize;        // size of a block [bytes]
// NYI
//  int64       freeBlocks;       // number of free blocks
//  int64       totalBlocks;      // total number of blocks
//  bool        mountedFlag;      // TRUE iff device is currently mounted
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
                   const String deviceName,
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

bool Device_eof(DeviceHandle *deviceHandle);

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
* Return : size of device (in bytes) or -1 when size cannot be
*          determined
* Notes  : -
\***********************************************************************/

int64 Device_getSize(DeviceHandle *deviceHandle);

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

bool Device_getUsedBlocks(DeviceHandle *deviceHandle,
                          uint64       blockOffset,
                          uint64       blockCount,
                          Bitmap       *usedBlocksBitmap
                         );

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
* Name   : Device_getDeviceInfo
* Purpose: get device info
* Input  : deviceInfo - device info variable to fill
*          deviceName - device name
* Output : deviceInfo - device info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Device_getDeviceInfo(DeviceInfo   *deviceInfo,
                            const String deviceName
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __DEVICES__ */

/* end of file */
