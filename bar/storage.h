/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.h,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

#ifndef __STORAGE__
#define __STORAGE__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "errors.h"
#include "network.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_BAND_WIDTH_MEASUREMENTS 256

/***************************** Datatypes *******************************/

typedef bool(*StorageRequestVolumeFunction)(void *userData,
                                            uint volumeNumber
                                           );

/* status info data */
typedef struct
{
  uint   volumeNumber;                     // current volume number
  double volumeProgress;                   // current volume progress [0..100]
} StorageStatusInfo;

typedef bool(*StorageStatusInfoFunction)(void *userData,
                                         const StorageStatusInfo *storageStatusInfo
                                        );

typedef enum
{
  STORAGE_MODE_READ,
  STORAGE_MODE_WRITE,
} StorageModes;

typedef enum
{
  STORAGE_TYPE_FILESYSTEM,
  STORAGE_TYPE_SSH,
  STORAGE_TYPE_SCP,
  STORAGE_TYPE_SFTP,
  STORAGE_TYPE_DVD,
  STORAGE_TYPE_DEVICE
} StorageTypes;

typedef struct
{
  ulong  max;
  ulong  blockSize;
  ulong  measurements[MAX_BAND_WIDTH_MEASUREMENTS];
  uint   measurementNextIndex;
  ulong  measurementBytes;    // sum of transmitted bytes
  uint64 measurementTime;     // time for transmission [us]
} StorageBandWidth;

typedef struct
{
  StorageModes                 mode;
  StorageTypes                 type;
  const Options                *options;

  StorageRequestVolumeFunction requestVolumeFunction;
  void                         *requestVolumeUserData;
  StorageStatusInfoFunction    storageStatusInfoFunction;
  void                         *storageStatusInfoUserData;
  uint                         volumeNumber;          
  uint                         requestedVolumeNumber; 

  union
  {
    // file storage
    struct
    {
      FileHandle fileHandle;
    } fileSystem;
    // ssh storage (remote BAR)
    struct
    {
      String           hostName;
      uint             hostPort;
      String           sshLoginName;
      String           sshPublicKeyFileName;
      String           sshPrivatKeyFileName;
      Password         *sshPassword;
      SocketHandle     socketHandle;
      LIBSSH2_CHANNEL  *channel;
      StorageBandWidth bandWidth;
    } ssh;
    // scp storage
    struct
    {
      String           hostName;
      uint             hostPort;
      String           sshLoginName;
      String           sshPublicKeyFileName;
      String           sshPrivatKeyFileName;
      Password         *sshPassword;
      SocketHandle     socketHandle;
      LIBSSH2_CHANNEL  *channel;
      StorageBandWidth bandWidth;
    } scp;
    // sftp storage
    struct
    {
      String              hostName;
      uint                hostPort;
      String              sshLoginName;
      String              sshPublicKeyFileName;
      String              sshPrivatKeyFileName;
      Password            *sshPassword;
      SocketHandle        socketHandle;
      LIBSSH2_SFTP        *sftp;
      LIBSSH2_SFTP_HANDLE *sftpHandle;
      uint64              index;
      uint64              size;
      struct
      {
        byte   *data;
        uint64 offset;
        ulong  length;
      } readAheadBuffer;
      StorageBandWidth    bandWidth;
    } sftp;
    // dvd storage
    struct
    {
      Device     device;                               // dvd device name
      String     name;                                 // 
      uint       steps;                                // number of step to create dvd
      String     directory;                            //
      uint64     volumeSize;                           // size of dvd [bytes]

      uint       step;                                 // current step number
      double     progress;                             // progress of current step

      StringList fileNameList;                         // list with file names
 
      String     fileName;                             // current file name
      FileHandle fileHandle;
      uint64     totalSize;                            // current size of dvd [bytes]
    } dvd;
    // device storage
    struct
    {
      Device     device;
      String     name;
      String     directory;
      uint       volumeNumber;
      StringList fileNameList;
      String     fileName;
      FileHandle fileHandle;
      uint64     totalSize;
    } device;
  };

  StorageStatusInfo runningInfo;
} StorageFileHandle;

typedef struct
{
  StorageTypes type;
  union
  {
    struct
    {
      DirectoryHandle directoryHandle;
    } fileSystem;
    struct
    {
      String              pathName;
      SocketHandle        socketHandle;
      LIBSSH2_SESSION     *session;
      LIBSSH2_CHANNEL     *channel;
      LIBSSH2_SFTP        *sftp;
      LIBSSH2_SFTP_HANDLE *sftpHandle;
      char                *buffer;          // buffer for reading file names
      ulong               bufferLength;
      bool                entryReadFlag;    // TRUE if entry read
    } sftp;
    struct
    {
      DirectoryHandle directoryHandle;
    } dvd;
  };
} StorageDirectoryHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Storage_initAll
* Purpose: initialize storage functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_initAll(void);

/***********************************************************************\
* Name   : Storage_doneAll
* Purpose: deinitialize storage functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_doneAll(void);

/***********************************************************************\
* Name   : Storage_getType
* Purpose: get storage type from storage name
* Input  : storageName - storage name
* Output : storageSpecifier - storage specific data (can be NULL)
* Return : storage type
* Notes  : -
\***********************************************************************/

StorageTypes Storage_getType(const String storageName,
                             String       storageSpecifier
                            );

/***********************************************************************\
* Name   : Storage_parseSSHSpecifier
* Purpose: parse ssh specifier: <user name>@<host name>:<file name>
* Input  : sshSpecifier - ssh specifier string
* Output : userName     - user name (can be NULL)
*          hostName     - host name (can be NULL)
*          fileName     - file name (can be NULL)
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       userName,
                               String       hostName,
                               String       fileName
                              );

/***********************************************************************\
* Name   : Storage_parseDevicepecifier
* Purpose: parse device specifier: <device name>:<file name>
* Input  : deviceSpecifier   - device specifier string
*          defaultDeviceName - default device name
* Output : deviceName - device name (can be NULL)
*          fileName   - file name (can be NULL)
* Return : TRUE if DVD specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseDeviceSpecifier(const String deviceSpecifier,
                                  const String defaultDeviceName,
                                  String       deviceName,
                                  String       fileName
                                 );

/***********************************************************************\
* Name   : Storage_prepare
* Purpose: prepare storage: read password, init files
* Input  : storageName - storage name:
*                          <file name>
*                          scp:<user name>@<host name>:<file name>
*                          sftp:<user name>@<host name>:<file name>
*          options     - options
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_prepare(const String  storageName,
                       const Options *options
                      );

/***********************************************************************\
* Name   : Storage_init
* Purpose: init new storage file
* Input  : storageFileHandle            - storage file handle variable
*          storageName                  - storage name:
*                                           <file name>
*                                           scp:<user name>@<host name>:<file name>
*                                           sftp:<user name>@<host name>:<file name>
*          options                      - options
*          storageRequestVolumeFunction - volume request call back
*          storageRequestVolumeUserData - user data for volume request
*                                         call back
* Output : storageFileHandle - initialized storage file handle
*          fileName          - file name (without storage specifier
*                              prefix)
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_init(StorageFileHandle            *storageFileHandle,
                    const String                 storageName,
                    const Options                *options,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    String                       fileName
                   );

/***********************************************************************\
* Name   : Storage_done
* Purpose: deinit storage file
* Input  : storageFileHandle - storage file handle variable
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_done(StorageFileHandle *storageFileHandle);

/***********************************************************************\
* Name   : Storage_preProcess
* Purpose: pre-process storage
* Input  : storageFileHandle - storage file handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_preProcess(StorageFileHandle *storageFileHandle);

/***********************************************************************\
* Name   : Storage_postProcess
* Purpose: post-process storage
* Input  : storageFileHandle - storage file handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_postProcess(StorageFileHandle *storageFileHandle,
                           bool              finalFlag
                          );

uint Storage_getVolumeNumber(const StorageFileHandle *storageFileHandle);
void Storage_setVolumeNumber(StorageFileHandle *storageFileHandle,
                             uint              volumeNumber
                            );

/***********************************************************************\
* Name   : Storage_create
* Purpose: create new storage file
* Input  : storageFileHandle - storage file handle
*          storageName      - storage name:
*                               <file name>
*                               scp:<user name>@<host name>:<file name>
*                               sftp:<user name>@<host name>:<file name>
*          fileSize         - storage file size
*          options          - options
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_create(StorageFileHandle *storageFileHandle,
                      const String      fileName,
                      uint64            fileSize,
                      const Options     *options
                     );

/***********************************************************************\
* Name   : Storage_open
* Purpose: open storage file
* Input  : storageFileHandle - storage handle file
*          storageName       - storage name:
*                                <file name>
*                                sftp:<user name>@<host name>:<file name>
*          options           - options
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_open(StorageFileHandle *storageFileHandle,
                    const String      fileName,
                    const Options     *options
                   );

/***********************************************************************\
* Name   : Storage_close
* Purpose: close storage file
* Input  : storageFileHandle - storage file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_close(StorageFileHandle *storageFileHandle);

/***********************************************************************\
* Name   : Storage_eof
* Purpose: check if end-of-file in storage file
* Input  : storageFileHandle - storage file handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_eof(StorageFileHandle *storageFileHandle);

/***********************************************************************\
* Name   : Storage_read
* Purpose: read from storage file
* Input  : storageFileHandle - storage file handle
*          buffer            - buffer with data to write
*          size              - data size
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_read(StorageFileHandle *storageFileHandle,
                    void              *buffer,
                    ulong             size,
                    ulong             *bytesRead
                   );

/***********************************************************************\
* Name   : Storage_write
* Purpose: write into storage file
* Input  : storageFileHandle - storage file handle
*          buffer            - buffer with data to write
*          size              - data size
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_write(StorageFileHandle *storageFileHandle,
                     const void        *buffer,
                     ulong             size
                    );

/***********************************************************************\
* Name   : Storage_getSize
* Purpose: get storage file size
* Input  : storageFileHandle - storage file handle
* Output : -
* Return : size of storage
* Notes  : -
\***********************************************************************/

uint64 Storage_getSize(StorageFileHandle *storageFileHandle);

/***********************************************************************\
* Name   : Storage_tell
* Purpose: get current position in storage file
* Input  : storageFileHandle - storage file handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_tell(StorageFileHandle *storageFileHandle,
                    uint64            *offset
                   );

/***********************************************************************\
* Name   : Storage_seek
* Purpose: seek in storage file
* Input  : storageFileHandle - storage file handle
*          offset            - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_seek(StorageFileHandle *storageFileHandle,
                    uint64            offset
                   );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Storage_openDirectory
* Purpose: open storage
* Input  : storageDirectoryBandle - storage directory handle variable
*          storageName            - storage name
*          options                - options
* Output : storageDirectoryHandle - initialized storage directory handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_openDirectory(StorageDirectoryHandle *storageDirectoryHandle,
                             const String           storageName,
                             const Options          *options
                            );

/***********************************************************************\
* Name   : Storage_closeDirectory
* Purpose: close storage directory
* Input  : storageDirectoryHandle - storage directory handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_closeDirectory(StorageDirectoryHandle *storageDirectoryHandle);

/***********************************************************************\
* Name   : Storage_endOfDirectory
* Purpose: check if end of directory reached
* Input  : storageDirectoryHandle - storage directory handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_endOfDirectory(StorageDirectoryHandle *storageDirectoryHandle);

/***********************************************************************\
* Name   : Storage_readDirectory
* Purpose: read next directory entry in storage
* Input  : storageDirectoryHandle - storage directory handle
*          fileName               - file name variable
* Output : fileName - next file name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_readDirectory(StorageDirectoryHandle *storageDirectoryHandle,
                             String                 fileName
                            );

#ifdef __cplusplus
  }
#endif

#endif /* __STORAGE__ */

/* end of file */
