/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/* Supported storage types:

     ftp://[<login name>[:<login password>]@]<host name>[:<host port>]/<file name>
     ssh://[<login name>@]<host name>[:<host port>]/<file name>
     scp://[<login name>@]<host name>[:<host port>]/<file name>
     sftp://[<login name>@]<host name>[:<host port>]/<file name>
     webdav://[<login name>[:<login password>]@]<host name>/<file name>
     cd://[<device name>:]<file name>
     dvd://[<device name>:]<file name>
     bd://[<device name>:]<file name>
     device://[<device name>:]<file name>
     file://<file name>
     plain file name
*/

#ifndef __STORAGE__
#define __STORAGE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_FTP
  #include <ftplib.h>
#endif /* HAVE_FTP */
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_ISO9660
  #include <cdio/cdio.h>
  #include <cdio/iso9660.h>
#endif /* HAVE_ISO9660 */
#ifdef HAVE_MXML
  #include "mxml.h"
#endif /* HAVE_MXML */
#include <assert.h>


#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "database.h"
#include "errors.h"

#include "crypt.h"
#include "passwords.h"
#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
// unlimited storage band width
#define STORAGE_BAND_WIDTH_UNLIMITED 0L

/***************************** Datatypes *******************************/

/* storage modes */
typedef enum
{
  STORAGE_REQUEST_VOLUME_NONE,

  STORAGE_REQUEST_VOLUME_OK,
  STORAGE_REQUEST_VOLUME_FAIL,
  STORAGE_REQUEST_VOLUME_UNLOAD,
  STORAGE_REQUEST_VOLUME_ABORTED,

  STORAGE_REQUEST_VOLUME_UNKNOWN,
} StorageRequestResults;

/***********************************************************************\
* Name   : StorageRequestVolumeFunction
* Purpose: request new volume call-back
* Input  : userData - user data
*          volumeNumber - requested volume number
* Output : -
* Return : storage request result; see StorageRequestResults
* Notes  : -
\***********************************************************************/

typedef StorageRequestResults(*StorageRequestVolumeFunction)(void *userData,
                                                             uint volumeNumber
                                                            );

/* status info data */
typedef struct
{
  uint   volumeNumber;                     // current volume number
  double volumeProgress;                   // current volume progress [0..100]
} StorageStatusInfo;

/***********************************************************************\
* Name   : StorageStatusInfoFunction
* Purpose: storage status call-back
* Input  : userData          - user data
*          storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*StorageStatusInfoFunction)(void                    *userData,
                                         const StorageStatusInfo *storageStatusInfo
                                        );

// storage modes
typedef enum
{
  STORAGE_MODE_READ,
  STORAGE_MODE_WRITE,

  STORAGE_MODE_UNKNOWN,
} StorageModes;

// storage types
typedef enum
{
  STORAGE_TYPE_NONE,

  STORAGE_TYPE_FILESYSTEM,
  STORAGE_TYPE_FTP,
  STORAGE_TYPE_SSH,
  STORAGE_TYPE_SCP,
  STORAGE_TYPE_SFTP,
  STORAGE_TYPE_WEBDAV,
  STORAGE_TYPE_CD,
  STORAGE_TYPE_DVD,
  STORAGE_TYPE_BD,
  STORAGE_TYPE_DEVICE,

  STORAGE_TYPE_ANY,

  STORAGE_TYPE_UNKNOWN
} StorageTypes;

//#define STORAGE_TYPE_ALL STORAGE_TYPE_NONE

// storage specifier
typedef struct
{
  StorageTypes type;                                       // storage type
  String       hostName;                                   // host name
  uint         hostPort;                                   // host port
  String       loginName;                                  // login name
  Password     *loginPassword;                             // login name
  String       deviceName;                                 // device name
  String       fileName;                                   // file name

  String       storageName;                                // storage name (returned by Storage_getStorageName())
  String       printableStorageName;                       // printable storage name without password (returned by Storage_getPrintableStorageName())
} StorageSpecifier;

// volume states
typedef enum
{
  STORAGE_VOLUME_STATE_UNKNOWN,
  STORAGE_VOLUME_STATE_UNLOADED,
  STORAGE_VOLUME_STATE_WAIT,
  STORAGE_VOLUME_STATE_LOADED,
} StorageVolumeStates;

// bandwidth data
typedef struct
{
  BandWidthList *maxBandWidthList;                         // list with max. band width [bits/s] to use or NULL
  ulong         maxBlockSize;                              // max. block size [bytes]
  ulong         blockSize;                                 // current block size [bytes]
  ulong         measurements[16];                          // measured band width values [bis/s]
  uint          measurementNextIndex;
  uint          measurementCount;
  ulong         measurementBytes;                          // measurement sum of transmitted bytes
  uint64        measurementTime;                           // measurement sum of time for transmission [us]
} StorageBandWidthLimiter;

typedef struct
{
  StorageModes                 mode;                       // storage mode: READ, WRITE
  StorageSpecifier             storageSpecifier;           // storage specifier data
  const JobOptions             *jobOptions;

  StorageRequestVolumeFunction requestVolumeFunction;      // call back for request new volume
  void                         *requestVolumeUserData;
  uint                         volumeNumber;               // current loaded volume number
  uint                         requestedVolumeNumber;      // requested volume number
  StorageVolumeStates          volumeState;                // volume state

  StorageStatusInfoFunction    storageStatusInfoFunction;  // call back new storage status info
  void                         *storageStatusInfoUserData;

  union
  {
    // file storage
    struct
    {
      FileHandle fileHandle;                               // file handle
    } fileSystem;

    #if defined(HAVE_CURL)
      // FTP storage
      struct
      {
        Server                  *server;
        CURLM                   *curlMultiHandle;
        CURL                    *curlHandle;
//        int                     runningHandles;            // curl number of active handles (1 or 0)
        uint64                  index;                     // current read/write index in file [0..n-1]
        uint64                  size;                      // size of file [bytes]
        struct                                             // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
        StorageBandWidthLimiter bandWidthLimiter;          // band width limit data
        void                    *buffer;                   // next data to write/read
        ulong                   length;                    // length of data to write/read
        ulong                   transferedBytes;           // number of data bytes read/written
      } ftp;

      // WebDAV storage
      struct
      {
        Server                  *server;
        CURLM                   *curlMultiHandle;
        CURL                    *curlHandle;
        uint64                  index;                     // current read/write index in file [0..n-1]
        uint64                  size;                      // size of file [bytes]
        struct                                             // receive buffer
        {
          byte   *data;                                    // data received
          ulong  size;                                     // buffer size [bytes]
          uint64 offset;                                   // data offset
          ulong  length;                                   // length of data received
        } receiveBuffer;
        struct                                             // send buffer
        {
          const byte *data;                                // data to send
          ulong      index;                                // data index
          ulong      length;                               // length of data to send
        } sendBuffer;
        StorageBandWidthLimiter bandWidthLimiter;          // band width limit data
      } webdav;
    #elif defined(HAVE_FTP)
      // FTP storage
      struct
      {
        Server                  *server;
        netbuf                  *control;
        netbuf                  *data;
        uint64                  index;                     // current read/write index in file [0..n-1]
        uint64                  size;                      // size of file [bytes]
        struct                                             // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
        StorageBandWidthLimiter bandWidthLimiter;          // band width limit data
      } ftp;
    #endif /* HAVE_CURL || HAVE_FTP */

    #ifdef HAVE_SSH2
      // ssh storage (remote BAR)
      struct
      {
        Server                  *server;
//        String                  hostName;                  // ssh server host name
//        uint                    hostPort;                  // ssh server port number
//        String                  loginName;                 // ssh login name
//        Password                *password;                 // ssh login password
        String                  sshPublicKeyFileName;      // ssh public key file name
        String                  sshPrivateKeyFileName;     // ssh private key file name

        SocketHandle            socketHandle;
        LIBSSH2_CHANNEL         *channel;                  // ssh channel
        LIBSSH2_SEND_FUNC((*oldSendCallback));             // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));          // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;            // total sent bytes
        uint64                  totalReceivedBytes;        // total received bytes
        StorageBandWidthLimiter bandWidthLimiter;          // band width limiter data
      } ssh;

      // scp storage
      struct
      {
        Server                  *server;
        String                  sshPublicKeyFileName;      // ssh public key file name
        String                  sshPrivateKeyFileName;     // ssh private key file name

        SocketHandle            socketHandle;
        LIBSSH2_CHANNEL         *channel;                  // scp channel
        LIBSSH2_SEND_FUNC((*oldSendCallback));             // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));          // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;            // total sent bytes
        uint64                  totalReceivedBytes;        // total received bytes
        uint64                  index;                     // current read/write index in file [0..n-1]
        uint64                  size;                      // size of file [bytes]
        struct                                             // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
        StorageBandWidthLimiter bandWidthLimiter;          // band width limiter data
      } scp;

      // sftp storage
      struct
      {
        Server                  *server;
        String                  sshPublicKeyFileName;      // ssh public key file name
        String                  sshPrivateKeyFileName;     // ssh private key file name

        SocketHandle            socketHandle;
        LIBSSH2_SEND_FUNC((*oldSendCallback));             // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));          // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;            // total sent bytes
        uint64                  totalReceivedBytes;        // total received bytes
        LIBSSH2_SFTP            *sftp;                     // sftp session
        LIBSSH2_SFTP_HANDLE     *sftpHandle;               // sftp handle
        uint64                  index;                     // current read/write index in file [0..n-1]
        uint64                  size;                      // size of file [bytes]
        struct                                             // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
        StorageBandWidthLimiter bandWidthLimiter;          // band width limiter data
      } sftp;
    #endif /* HAVE_SSH2 */

    // cd/dvd/bd storage
    struct
    {
      // read cd/dvd/bd
      struct
      {
        #ifdef HAVE_ISO9660
          iso9660_t      *iso9660Handle;                   // ISO9660 image handle
          iso9660_stat_t *iso9660Stat;                     // ISO9660 file handle
          uint64         index;                            // current read/write index in ISO image [0..n-1]

          struct                                           // read buffer
          {
            byte   *data;
            uint64 blockIndex;                             // ISO9660 block index
            ulong  length;
          } buffer;
        #endif /* HAVE_ISO9660 */
      } read;

      // write cd/dvd/bd
      struct
      {
        String     requestVolumeCommand;                   // command to request new cd/dvd/bd
        String     unloadVolumeCommand;                    // command to unload cd/dvd/bd
        String     loadVolumeCommand;                      // command to load cd/dvd/bd
        uint64     volumeSize;                             // size of cd/dvd/bd [bytes]
        String     imagePreProcessCommand;                 // command to execute before creating image
        String     imagePostProcessCommand;                // command to execute after created image
        String     imageCommand;                           // command to create cd/dvd/bd image
        String     eccPreProcessCommand;                   // command to execute before ECC calculation
        String     eccPostProcessCommand;                  // command to execute after ECC calculation
        String     eccCommand;                             // command for ECC calculation
        String     writePreProcessCommand;                 // command to execute before writing cd/dvd/bd
        String     writePostProcessCommand;                // command to execute after writing cd/dvd/bd
        String     writeCommand;                           // command to write cd/dvd/bd
        String     writeImageCommand;                      // command to write image on cd/dvd/bd
        bool       alwaysCreateImage;                      // TRUE iff always creating image

        uint       steps;                                  // total number of steps to create cd/dvd/bd
        String     directory;                              // temporary directory for cd/dvd/bd files

        uint       step;                                   // current step number
        double     progress;                               // progress of current step

        uint       number;                                 // current cd/dvd/bd number
        bool       newVolumeFlag;                          // TRUE iff new cd/dvd/bd volume needed
        StringList fileNameList;                           // list with file names
        String     fileName;                               // current file name
        FileHandle fileHandle;
        uint64     totalSize;                              // current size of cd/dvd/bd [bytes]
      } write;
    } opticalDisk;

    // device storage
    struct
    {
      String     requestVolumeCommand;                     // command to request new volume
      String     unloadVolumeCommand;                      // command to unload volume
      String     loadVolumeCommand;                        // command to load volume
      uint64     volumeSize;                               // size of volume [bytes]
      String     imagePreProcessCommand;                   // command to execute before creating image
      String     imagePostProcessCommand;                  // command to execute after created image
      String     imageCommand;                             // command to create volume image
      String     eccPreProcessCommand;                     // command to execute before ECC calculation
      String     eccPostProcessCommand;                    // command to execute after ECC calculation
      String     eccCommand;                               // command for ECC calculation
      String     writePreProcessCommand;                   // command to execute before writing volume
      String     writePostProcessCommand;                  // command to execute after writing volume
      String     writeCommand;                             // command to write volume

      String     directory;                                // temporary directory for files

      uint       number;                                   // volume number
      bool       newVolumeFlag;                            // TRUE iff new volume needed
      StringList fileNameList;                             // list with file names
      String     fileName;                                 // current file name
      FileHandle fileHandle;
      uint64     totalSize;                                // current size [bytes]
    } device;
  };

  StorageStatusInfo runningInfo;
} StorageHandle;

typedef struct
{
  StorageTypes type;
  union
  {
    struct
    {
      DirectoryListHandle directoryListHandle;
    } fileSystem;
    #if   defined(HAVE_CURL)
      struct
      {
        Server                  *server;
        String                  pathName;                  // directory name
        StringList              lineList;

        String                  fileName;                  // last parsed entry
        FileTypes               type;
        int64                   size;
        uint64                  timeModified;
        uint32                  userId;
        uint32                  groupId;
        FilePermission          permission;
        bool                    entryReadFlag;             // TRUE if entry read
      } ftp;

      struct
      {
        Server                  *server;
        String                  pathName;                  // directory name

        mxml_node_t             *rootNode;
        mxml_node_t             *lastNode;
        mxml_node_t             *currentNode;

/*
        String                  fileName;                  // last parsed entry
        FileTypes               type;
        int64                   size;
        uint64                  timeModified;
        uint32                  userId;
        uint32                  groupId;
        FilePermission          permission;
        bool                    entryReadFlag;             // TRUE if entry read
*/
      } webdav;
    #elif defined(HAVE_FTP)
      struct
      {
        Server                  *server;
        String                  pathName;                  // directory name

        String                  fileListFileName;
        FileHandle              fileHandle;

        String                  fileName;                  // last parsed entry
        FileTypes               type;
        int64                   size;
        uint64                  timeModified;
        uint32                  userId;
        uint32                  groupId;
        FilePermission          permission;
        bool                    entryReadFlag;             // TRUE if entry read
      } ftp;
    #endif /* HAVE_CURL || HAVE_FTP */
    #ifdef HAVE_SSH2
      struct
      {
        String                  pathName;                  // directory name

        SocketHandle            socketHandle;
        LIBSSH2_SESSION         *session;
        LIBSSH2_CHANNEL         *channel;
        LIBSSH2_SFTP            *sftp;
        LIBSSH2_SFTP_HANDLE     *sftpHandle;
        char                    *buffer;                   // buffer for reading file names
        ulong                   bufferLength;
        LIBSSH2_SFTP_ATTRIBUTES attributes;
        bool                    entryReadFlag;             // TRUE if entry read
      } sftp;
    #endif /* HAVE_SSH2 */
    struct
    {
      #ifdef HAVE_ISO9660
        String                  pathName;                  // directory name

        iso9660_t               *iso9660Handle;            // ISO9660 image handle
        CdioList_t              *cdioList;                 // ISO9660 entry list
        CdioListNode_t          *cdioNextNode;             // next entry in list
      #else /* not HAVE_ISO9660 */
        DirectoryListHandle     directoryListHandle;
      #endif /* HAVE_ISO9660 */
    } opticalDisk;
  };
} StorageDirectoryListHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Storage_initSpecifier(...)      __Storage_initSpecifier(__FILE__,__LINE__,__VA_ARGS__)
  #define Storage_duplicateSpecifier(...) __Storage_duplicateSpecifier(__FILE__,__LINE__,__VA_ARGS__)
  #define Storage_doneSpecifier(...)      __Storage_doneSpecifier(__FILE__,__LINE__,__VA_ARGS__)
  #define Storage_init(...)               __Storage_init(__FILE__,__LINE__,__VA_ARGS__)
  #define Storage_done(...)               __Storage_done(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

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
* Name   : Storage_initSpecifier
* Purpose: initialize storage specifier
* Input  : storageSpecifier - storage specifier variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Storage_initSpecifier(StorageSpecifier *storageSpecifier);
#else /* not NDEBUG */
  void __Storage_initSpecifier(const char       *__fileName__,
                               ulong            __lineNb__,
                               StorageSpecifier *storageSpecifier
                              );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_duplicateSpecifier
* Purpose: duplicate storage specifier structure
* Input  : destinationStorageSpecifier - storage specifier variable
*          sourceStorageSpecifier      - source storage specifier
* Output : destinationStorageSpecifier - duplicated storage specifier
* Return : duplicated storage specifier
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Storage_duplicateSpecifier(StorageSpecifier       *destinationStorageSpecifier,
                                  const StorageSpecifier *sourceStorageSpecifier
                                 );
#else /* not NDEBUG */
  void __Storage_duplicateSpecifier(const char             *__fileName__,
                                    ulong                  __lineNb__,
                                    StorageSpecifier       *destinationStorageSpecifier,
                                    const StorageSpecifier *sourceStorageSpecifier
                                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_doneSpecifier
* Purpose: done storage specifier
* Input  : storageSpecifier - storage specifier variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Storage_doneSpecifier(StorageSpecifier *storageSpecifier);
#else /* not NDEBUG */
  void __Storage_doneSpecifier(const char       *__fileName__,
                               ulong            __lineNb__,
                               StorageSpecifier *storageSpecifier
                              );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_parseFTPSpecifier
* Purpose: parse FTP specifier:
*            [<login name>[:<login password>]@]<host name>[:<host port>]
* Input  : ftpSpecifier  - FTP specifier string
*          hostName      - host name variable (can be NULL)
*          hostPort      - host port variable (can be NULL)
*          loginName     - login name variable (can be NULL)
*          loginPassword - login password variable (can be NULL)
* Output : hostName      - host name
*          hostPort      - host port
*          loginName     - login name
*          loginPassword - login password
* Return : TRUE if FTP specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseFTPSpecifier(const String ftpSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName,
                               Password     *loginPassword
                              );

/***********************************************************************\
* Name   : Storage_parseSSHSpecifier
* Purpose: parse ssh specifier:
*            [<login name>@]<host name>[:<host port>]
* Input  : sshSpecifier - ssh specifier string
*          hostName     - host name variable (can be NULL)
*          hostPort     - host port number variable (can be NULL)
*          loginName    - login name variable (can be NULL)
* Output : hostName     - host name (can be NULL)
*          hostPort     - host port number (can be NULL)
*          loginName    - login name (can be NULL)
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName
                              );

/***********************************************************************\
* Name   : Storage_parseWebDAVSpecifier
* Purpose: parse WebDAV specifier:
*            [<login name>@]<host name>
* Input  : webdavSpecifier - WebDAV specifier string
*          hostName        - host name variable (can be NULL)
*          loginName       - login name variable (can be NULL)
*          loginPassword   - login password variable (can be NULL)
* Output : hostName      - host name (can be NULL)
*          loginName     - login name (can be NULL)
*          loginPassword - login password
* Return : TRUE if WebDAV specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseWebDAVSpecifier(const String webdavSpecifier,
                                  String       hostName,
                                  String       loginName,
                                  Password     *loginPassword
                                 );

/***********************************************************************\
* Name   : Storage_parseDeviceSpecifier
* Purpose: parse device specifier:
*            <device name>:
* Input  : deviceSpecifier   - device specifier string
*          defaultDeviceName - default device name
*          deviceName        - device name variable (can be NULL)
*          fileName          - file name variable (can be NULL)
* Output : deviceName - device name (can be NULL)
*          fileName   - file name (can be NULL)
* Return : TRUE if device specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseDeviceSpecifier(const String deviceSpecifier,
                                  const String defaultDeviceName,
                                  String       deviceName
                                 );

/***********************************************************************\
* Name   : Storage_getType
* Purpose: get storage type from storage name
* Input  : storageName - storage name
* Output : -
* Return : storage type
* Notes  : name structure:
*            <type>://<storage specifier>/<filename>
\***********************************************************************/

StorageTypes Storage_getType(const String storageName);

/***********************************************************************\
* Name   : Storage_parseName
* Purpose: parse storage name and get storage type
* Input  : storageSpecifier - storage specific variable
*          storageName - storage name
* Output : storageSpecifier - storage specific data
* Return : ERROR_NONE or error code
* Notes  : name structure:
*            <type>://<storage specifier>/<filename>
\***********************************************************************/

Errors Storage_parseName(StorageSpecifier *storageSpecifier,
                         const String     storageName
                        );

/***********************************************************************\
* Name   : Storage_equalNames
* Purpose: check if storage names identifty the same archive
* Input  : storageName1,storageName2 - storage names
* Output : -
* Return : TURE iff storage names identifty the same archive
* Notes  : -
\***********************************************************************/

bool Storage_equalNames(const String storageName1,
                        const String storageName2
                       );

/***********************************************************************\
* Name   : Storage_getName, Storage_getNameCString
* Purpose: get storage name
* Input  : storageSpecifierString - storage specifier string
*          fileName               - fileName (can be NULL)
* Output : -
* Return : storage name
* Notes  : if fileName is NULL file name from storageSpecifier is used
\***********************************************************************/

const String Storage_getName(StorageSpecifier *storageSpecifier,
                             const String     fileName
                            );
const char *Storage_getNameCString(StorageSpecifier *storageSpecifier,
                                   const String     fileName
                                  );

/***********************************************************************\
* Name   : Storage_getPrintableName, Storage_getPrintableNameCString
* Purpose: get printable storage name (without password)
* Input  : storageSpecifierString - storage specifier string
*          fileName               - fileName (can be NULL)
* Output : -
* Return : storage name
* Notes  : if fileName is NULL file name from storageSpecifier is used
\***********************************************************************/

const String Storage_getPrintableName(StorageSpecifier *storageSpecifier,
                                      const String     fileName
                                     );
const char *Storage_getPrintableNameCString(StorageSpecifier *storageSpecifier,
                                            const String     fileName
                                           );

/***********************************************************************\
* Name   : Storage_prepare
* Purpose: prepare storage: read password, init files
* Input  : storageName - storage name:
*          options     - options
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_prepare(const String     storageName,
                       const JobOptions *jobOptions
                      );

/***********************************************************************\
* Name   : Storage_init
* Purpose: init new storage
* Input  : storageHandle                - storage handle variable
*          storageSpecifier             - storage specifier structure
*          jobOptions                   - job options
*          maxBandWidthList             - list with max. band width to
*                                         use [bits/s] or NULL
*          serverConnectionPriority     - server connection priority
*          storageRequestVolumeFunction - volume request call back
*          storageRequestVolumeUserData - user data for volume request
*                                         call back
*          storageStatusInfoFunction    - status info call back
*          storageStatusInfoUserData    - user data for status info
*                                         call back
* Output : storageHandle - initialized storage handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_init(StorageHandle                *storageHandle,
                      const StorageSpecifier       *storageSpecifier,
                      const JobOptions             *jobOptions,
                      BandWidthList                *maxBandWidthList,
                      ServerConnectionPriorities   serverConnectionPriority,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      StorageStatusInfoFunction    storageStatusInfoFunction,
                      void                         *storageStatusInfoUserData
                     );
#else /* not NDEBUG */
  Errors __Storage_init(const char                   *__fileName__,
                        ulong                        __lineNb__,
                        StorageHandle                *storageHandle,
                        const StorageSpecifier       *storageSpecifier,
                        const JobOptions             *jobOptions,
                        BandWidthList                *maxBandWidthList,
                        ServerConnectionPriorities   serverConnectionPriority,
                        StorageRequestVolumeFunction storageRequestVolumeFunction,
                        void                         *storageRequestVolumeUserData,
                        StorageStatusInfoFunction    storageStatusInfoFunction,
                        void                         *storageStatusInfoUserData
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_done
* Purpose: deinit storage
* Input  : storageHandle - storage handle variable
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_done(StorageHandle *storageHandle);
#else /* not NDEBUG */
  Errors __Storage_done(const char    *__fileName__,
                        ulong         __lineNb__,
                        StorageHandle *storageHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_isServerAllocationPending
* Purpose: check if a server allocation with high priority is pending
* Input  : storageHandle - storage handle
* Output : -
* Return : TRUE if server allocation with high priority is pending,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_isServerAllocationPending(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_getStorageSpecifier
* Purpose: get storage specifier from from storage handle
* Input  : storageHandle - storage handle
* Output : -
* Return : storage specifier
* Notes  : -
\***********************************************************************/

const StorageSpecifier *Storage_getStorageSpecifier(const StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_preProcess
* Purpose: pre-process storage
* Input  : storageHandle - storage handle
*          initialFlag   - TRUE iff initial call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_preProcess(StorageHandle *storageHandle,
                          bool          initialFlag
                         );

/***********************************************************************\
* Name   : Storage_postProcess
* Purpose: post-process storage
* Input  : storageHandle - storage handle
*          finalFlag     - TRUE iff final call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_postProcess(StorageHandle *storageHandle,
                           bool          finalFlag
                          );

/***********************************************************************\
* Name   : Storage_getVolumeNumber
* Purpose: get current volume number
* Input  : storageHandle - storage handle
* Output : -
* Return : volume number
* Notes  : -
\***********************************************************************/

uint Storage_getVolumeNumber(const StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_setVolumeNumber
* Purpose: set volume number
* Input  : storageHandle - storage handle
*          volumeNumber  - volume number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_setVolumeNumber(StorageHandle *storageHandle,
                             uint          volumeNumber
                            );

/***********************************************************************\
* Name   : Storage_unloadVolume
* Purpose: unload volume
* Input  : storageHandle - storage handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_unloadVolume(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_create
* Purpose: create new storage file
* Input  : storageHandle - storage handle
*          fileName      - archive file name
*          fileSize      - storage file size
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_create(StorageHandle *storageHandle,
                      const String  fileName,
                      uint64        fileSize
                     );

/***********************************************************************\
* Name   : Storage_open
* Purpose: open storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_open(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_close
* Purpose: close storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_close(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_delete
* Purpose: delete storage file
* Input  : storageHandle - storage handle
*          fileName      - archive file name
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_delete(StorageHandle *storageHandle,
                      const String  fileName
                     );

/***********************************************************************\
* Name   : Storage_eof
* Purpose: check if end-of-file in storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_eof(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_read
* Purpose: read from storage file
* Input  : storageHandle - storage file handle
*          buffer        - buffer with data to write
*          size          - data size
*          bytesRead     - number of bytes read or NULL
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_read(StorageHandle *storageHandle,
                    void          *buffer,
                    ulong         size,
                    ulong         *bytesRead
                   );

/***********************************************************************\
* Name   : Storage_write
* Purpose: write into storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_write(StorageHandle *storageHandle,
                     const void    *buffer,
                     ulong         size
                    );

/***********************************************************************\
* Name   : Storage_getSize
* Purpose: get storage file size
* Input  : storageHandle - storage handle
* Output : -
* Return : size of storage
* Notes  : -
\***********************************************************************/

uint64 Storage_getSize(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_tell
* Purpose: get current position in storage file
* Input  : storageHandle - storage handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_tell(StorageHandle *storageHandle,
                    uint64        *offset
                   );

/***********************************************************************\
* Name   : Storage_seek
* Purpose: seek in storage file
* Input  : storageHandle - storage handle
*          offset            - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_seek(StorageHandle *storageHandle,
                    uint64        offset
                   );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Storage_openDirectoryList
* Purpose: open storage directory list for reading directory entries
* Input  : storageDirectoryListHandle - storage directory list handle
*                                       variable
*          storageName                - storage name
*                                       (prefix+specifier+path only)
*          jobOptions                 - job options
*          serverConnectionPriority        - server connection priority
* Output : storageDirectoryListHandle - initialized storage directory
*                                       list handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 const StorageSpecifier     *storageSpecifier,
                                 const JobOptions           *jobOptions,
                                 ServerConnectionPriorities serverConnectionPriority
                                );

/***********************************************************************\
* Name   : Storage_closeDirectoryList
* Purpose: close storage directory list
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle);

/***********************************************************************\
* Name   : Storage_endOfDirectoryList
* Purpose: check if end of storage directory list reached
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle);

/***********************************************************************\
* Name   : Storage_readDirectoryList
* Purpose: read next storage directory list entry in storage
* Input  : storageDirectoryListHandle - storage directory list handle
*          fileName                   - file name variable
*          fileInfo                   - file info (can be NULL)
* Output : fileName - next file name (including path)
*          fileInfo - next file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 String                     fileName,
                                 FileInfo                   *fileInfo
                                );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Storage_copy
* Purpose: copy storage file to local file
* Input  : storageSpecifier             - storage specifier structure
*          jobOptions                   - job options
*          maxBandWidthLIst             - list with max. band width to use [bits/s] or NULL
*          storageRequestVolumeFunction - volume request call back
*          storageRequestVolumeUserData - user data for volume request
*                                         call back
*          storageStatusInfoFunction    - status info call back
*          storageStatusInfoUserData    - user data for status info
*                                         call back
*          localFileName                - local archive file name
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Storage_copy(const StorageSpecifier       *storageSpecifier,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    const String                 localFileName
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __STORAGE__ */

/* end of file */
