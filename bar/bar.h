/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.h,v $
* $Revision: 1.26 $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems :
*
\***********************************************************************/

#ifndef __BAR__
#define __BAR__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"

#include "patterns.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  EXITCODE_OK=0,
  EXITCODE_FAIL=1,

  EXITCODE_INVALID_ARGUMENT=5,
  EXITCODE_CONFIG_ERROR,

  EXITCODE_INIT_FAIL=125,
  EXITCODE_FATAL_ERROR=126,
  EXITCODE_FUNCTION_NOT_SUPPORTED=127,

  EXITCODE_UNKNOWN=128
} ExitCodes;

typedef enum
{
  LOG_GROUP_IO_ERROR,
  LOG_GROUP_FILE_ACCESS_DENIED,
  LOG_GROUP_FILE_MISSING,

  LOG_GROUP_FILE_OK,
  LOG_GROUP_FILE_SKIPPED,
} LogGroups;

typedef struct
{
  uint     port;
  String   loginName;
  String   publicKeyFileName;
  String   privatKeyFileName;
  Password *password;
} SSHServer;

typedef struct SSHServerNode
{
  NODE_HEADER(struct SSHServerNode);

  String    name;
  SSHServer sshServer;
} SSHServerNode;

typedef struct
{
  LIST_HEADER(SSHServerNode);
} SSHServerList;

typedef struct
{
  String requestVolumeCommand;
  String unloadVolumeCommand;
  String loadVolumeCommand;
  uint64 volumeSize;

  String imagePreProcessCommand;
  String imagePostProcessCommand;
  String imageCommand;
  String eccPreProcessCommand;
  String eccPostProcessCommand;
  String eccCommand;
  String writePreProcessCommand;
  String writePostProcessCommand;
  String writeCommand;
} Device;

typedef struct DeviceNode
{
  NODE_HEADER(struct DeviceNode);

  String name;
  Device device;
} DeviceNode;

typedef struct
{
  LIST_HEADER(DeviceNode);
} DeviceList;

typedef struct
{
  uint64             archivePartSize;
  String             tmpDirectory;
  uint64             maxTmpSize;
  uint               directoryStripCount;
  String             directory;

  ulong              maxBandWidth;

  PatternTypes       patternType;

  CompressAlgorithms compressAlgorithm;
  CryptAlgorithms    cryptAlgorithm;
  ulong              compressMinFileSize;
  Password           *cryptPassword;

  SSHServer          sshServer;
  SSHServerList      *sshServerList;
  SSHServer          defaultSSHServer;

  String             remoteBARExecutable;

  String             deviceName;
  Device             device;
  DeviceList         *deviceList;
  Device             defaultDevice;

  bool               skipUnreadableFlag;
  bool               overwriteArchiveFilesFlag;
  bool               overwriteFilesFlag;
  bool               noDefaultConfigFlag;
  bool               errorCorrectionCodesFlag;
  bool               quietFlag;
  long               verboseLevel;
} Options;

/***************************** Variables *******************************/
extern Options defaultOptions;

/****************************** Macros *********************************/

#define BYTES_SHORT(n) (((n)>(1024LL*1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>       (1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>                1024LL)?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        (double)(n) \
                       )
#define BYTES_UNIT(n) (((n)>(1024LL*1024LL*1024LL))?"GB": \
                       ((n)>       (1024LL*1024LL))?"MB": \
                       ((n)>                1024LL)?"KB": \
                       "bytes" \
                      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getErrorText
* Purpose: get errror text of error code
* Input  : error - error
* Output : -
* Return : error text (read only!)
* Notes  : -
\***********************************************************************/

const char *getErrorText(Errors error);

/***********************************************************************\
* Name   : info, vinfo
* Purpose: output info
* Input  : verboseLevel - verbosity level
*          format       - format string (like printf)
*          ...          - optional arguments (like printf)
*          arguments    - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void info(uint verboseLevel, const char *format, ...);
void vinfo(uint verboseLevel, const char *format, va_list arguments);

/***********************************************************************\
* Name   : printWarning
* Purpose: output warning
* Input  : text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printWarning(const char *text, ...);

/***********************************************************************\
* Name   : printError
* Purpose: print error message
*          text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printError(const char *text, ...);

/***********************************************************************\
* Name   : copyOptions
* Purpose: copy options structure
* Input  : sourceOptions      - source options
*          destinationOptions - destination options variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void copyOptions(const Options *sourceOptions, Options *destinationOptions);

/***********************************************************************\
* Name   : freeOptions
* Purpose: free options
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeOptions(Options *options);

/***********************************************************************\
* Name   : getSSHServer
* Purpose: get SSH server data
* Input  : name    - server name
*          options - options
* Output : sshServer - SSH server data from server list or default
*                      server values
* Return : -
* Notes  : -
\***********************************************************************/

void getSSHServer(const String  name,
                  const Options *options,
                  SSHServer     *sshServer
                 );

/***********************************************************************\
* Name   : getDevice
* Purpose: get device data
* Input  : name    - device name
*          options - options
* Output : device - device data from devie list or default
*                   device values
* Return : -
* Notes  : -
\***********************************************************************/

void getDevice(const String  name,
               const Options *options,
               Device        *device
              );

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
