/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/* Format storage URL:

   <type>://<url specific values>/<archive name>

   Supported storage types:

     ftp://[<login name>[:<login password>]@]<host name>[:<host port>]/<archive name>
     ssh://[<login name>@]<host name>[:<host port>]/<archive name>
     scp://[<login name>@]<host name>[:<host port>]/<archive name>
     sftp://[<login name>@]<host name>[:<host port>]/<archive name>
     webdav://[<login name>[:<login password>]@]<host name>/<archive name>
     webdavs://[<login name>[:<login password>]@]<host name>/<archive name>
     cd://[<device name>:]<archive name>
     dvd://[<device name>:]<archive name>
     bd://[<device name>:]<archive name>
     device://[<device name>:]<archive name>
     file://<archive name>
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


#include "common/global.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/network.h"
#include "common/semaphores.h"
#include "common/passwords.h"

#include "errors.h"
#include "configuration.h"
#include "crypt.h"
#include "server_io.h"
#include "bar_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// unlimited storage band width
#define STORAGE_BAND_WIDTH_UNLIMITED 0L

/***************************** Datatypes *******************************/

// status info data
typedef struct
{
  uint64 storageDoneBytes;                 // storage done [bytes]
  uint   volumeNumber;                     // current volume number
  double volumeProgress;                   // current volume progress [0..100]
} StorageStatusInfo;

/***********************************************************************\
* Name   : StorageStatusInfoFunction
* Purpose: storage status call-back
* Input  : storageStatusInfo - storage status info
*          userData          - user data
* Output : -
* Return : TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

typedef bool(*StorageUpdateStatusInfoFunction)(const StorageStatusInfo *storageStatusInfo,
                                               void                    *userData
                                              );

// storage request volume types
typedef enum
{
  STORAGE_REQUEST_VOLUME_TYPE_NEW,
  STORAGE_REQUEST_VOLUME_TYPE_RENEW
} StorageRequestVolumeTypes;

// storage request volume results
typedef enum
{
  STORAGE_REQUEST_VOLUME_RESULT_NONE,

  STORAGE_REQUEST_VOLUME_RESULT_OK,
  STORAGE_REQUEST_VOLUME_RESULT_FAIL,
  STORAGE_REQUEST_VOLUME_RESULT_UNLOAD,
  STORAGE_REQUEST_VOLUME_RESULT_ABORTED,

  STORAGE_REQUEST_VOLUME_RESULT_UNKNOWN,
} StorageRequestVolumeResults;

/***********************************************************************\
* Name   : StorageRequestVolumeFunction
* Purpose: request new volume call-back
* Input  : type         - storage request type; see StorageRequestTypes
*          volumeNumber - requested volume number
*          message      - message to show
*          userData     - user data
* Output : -
* Return : storage request result; see StorageRequestResults
* Notes  : -
\***********************************************************************/

typedef StorageRequestVolumeResults(*StorageRequestVolumeFunction)(StorageRequestVolumeTypes type,
                                                                   uint                      volumeNumber,
                                                                   const char                *message,
                                                                   void                      *userData
                                                                  );

// storage modes
typedef enum
{
  STORAGE_MODE_READ,
  STORAGE_MODE_WRITE,
//TODO
//  STORAGE_MODE_APPEND
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

//TODO: remove from enum
  STORAGE_TYPE_ANY,

  STORAGE_TYPE_UNKNOWN
} StorageTypes;

//#define STORAGE_TYPE_ALL STORAGE_TYPE_NONE

// storage specifier
typedef struct
{
  StorageTypes type;                                         // storage type

  String       hostName;                                     // host name
  uint         hostPort;                                     // host port
  bool         sslFlag;                                      // TRUE for SSL
  String       loginName;                                    // login name
  Password     *loginPassword;                               // login name
  String       deviceName;                                   // device name

  String       archiveName;                                  // archive name
  String       archivePatternString;                         // archive pattern string or NULL if no pattern
  Pattern      archivePattern;

  String       storageName;                                  // storage name (returned by Storage_getName())
  String       printableStorageName;                         // printable storage name without password (returned by Storage_getPrintableName())
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
  BandWidthList *maxBandWidthList;                           // list with max. band width [bits/s] to use or NULL
  ulong         maxBlockSize;                                // max. block size [bytes]
  ulong         blockSize;                                   // current block size [bytes]
  ulong         measurements[16];                            // measured band width values [bis/s]
  uint          measurementNextIndex;
  uint          measurementCount;
  ulong         measurementBytes;                            // measurement sum of transmitted bytes
  uint64        measurementTime;                             // measurement sum of time for transmission [us]
} StorageBandWidthLimiter;

// storage info
typedef struct
{
  Semaphore                       lock;

  StorageSpecifier                storageSpecifier;          // storage specifier data
  const JobOptions                *jobOptions;
  ServerIO                        *masterIO;

  LogHandle                       *logHandle;                // log handle

  StorageUpdateStatusInfoFunction updateStatusInfoFunction;  // storage status info update call-back
  void                            *updateStatusInfoUserData;
  GetNamePasswordFunction         getNamePasswordFunction;   // get name/password call-back
  void                            *getNamePasswordUserData;
  StorageRequestVolumeFunction    requestVolumeFunction;     // request new volume call-back
  void                            *requestVolumeUserData;
  IsPauseFunction                 isPauseFunction;           // check if pause call-back
  void                            *isPauseUserData;
  IsAbortedFunction               isAbortedFunction;         // check if aborted call-back
  void                            *isAbortedUserData;

  uint                            volumeNumber;              // current loaded volume number
  uint                            requestedVolumeNumber;     // requested volume number
  StorageVolumeStates             volumeState;               // volume state

  union
  {
    // file storage
    struct
    {
    } fileSystem;

    #ifdef HAVE_CURL
      // FTP storage
      struct
      {
        uint                      serverId;                  // id of allocated server
        StorageBandWidthLimiter   bandWidthLimiter;          // band width limit data
      } ftp;

      // WebDAV storage
      struct
      {
        uint                      serverId;                  // id of allocated server
        StorageBandWidthLimiter   bandWidthLimiter;          // band width limit data
      } webdav;
    #endif /* HAVE_CURL */

    #ifdef HAVE_SSH2
      // ssh storage (remote BAR)
      struct
      {
        uint                      serverId;                  // id of allocated server
//        String                    hostName;                  // ssh server host name
//        uint                      hostPort;                  // ssh server port number
//        String                    loginName;                 // ssh login name
//        Password                  *password;                 // ssh login password
//        String                    sshPublicKeyFileName;      // ssh public key file name
//        String                    sshPrivateKeyFileName;     // ssh private key file name
        Key                       publicKey;                   // ssh public key data (ssh,scp,sftp)
        Key                       privateKey;                  // ssh private key data (ssh,scp,sftp)
        StorageBandWidthLimiter   bandWidthLimiter;          // band width limiter data
      } ssh;

      // scp storage
      struct
      {
        uint                      serverId;                  // id of allocated server
        String                    sshPublicKeyFileName;      // ssh public key file name
        String                    sshPrivateKeyFileName;     // ssh private key file name
        Key                       publicKey;                 // ssh public key data (ssh,scp,sftp)
        Key                       privateKey;                // ssh private key data (ssh,scp,sftp)
        StorageBandWidthLimiter   bandWidthLimiter;          // band width limiter data
      } scp;

      // sftp storage
      struct
      {
        uint                      serverId;                  // id of allocated server
        String                    sshPublicKeyFileName;      // ssh public key file name
        String                    sshPrivateKeyFileName;     // ssh private key file name
        Key                       publicKey;                 // ssh public key data (ssh,scp,sftp)
        Key                       privateKey;                // ssh private key data (ssh,scp,sftp)
        StorageBandWidthLimiter   bandWidthLimiter;          // band width limiter data
      } sftp;
    #endif /* HAVE_SSH2 */

    // cd/dvd/bd storage
    struct
    {
      // read cd/dvd/bd
      struct
      {
        #ifdef HAVE_ISO9660
        #endif /* HAVE_ISO9660 */
      } read;

      // write cd/dvd/bd
      struct
      {
        String     requestVolumeCommand;                     // command to request new cd/dvd/bd
        String     unloadVolumeCommand;                      // command to unload cd/dvd/bd
        String     loadVolumeCommand;                        // command to load cd/dvd/bd
        uint64     volumeSize;                               // size of cd/dvd/bd [bytes]
        String     imagePreProcessCommand;                   // command to execute before creating image
        String     imagePostProcessCommand;                  // command to execute after created image
        String     imageCommand;                             // command to create cd/dvd/bd image
        String     eccPreProcessCommand;                     // command to execute before ECC calculation
        String     eccPostProcessCommand;                    // command to execute after ECC calculation
        String     eccCommand;                               // command for ECC calculation
        String     blankCommand;                             // command to blank medium before writing
        String     writePreProcessCommand;                   // command to execute before writing cd/dvd/bd
        String     writePostProcessCommand;                  // command to execute after writing cd/dvd/bd
        String     writeCommand;                             // command to write cd/dvd/bd
        String     writeImageCommand;                        // command to write image on cd/dvd/bd
        bool       alwaysCreateImage;                        // TRUE iff always creating image

        uint       steps;                                    // total number of steps to create cd/dvd/bd
        String     directory;                                // temporary directory for cd/dvd/bd files

        uint       step;                                     // current step number
        double     progress;                                 // progress of current step

        uint       number;                                   // current cd/dvd/bd number
        bool       newVolumeFlag;                            // TRUE iff new cd/dvd/bd volume needed
        StringList fileNameList;                             // list with file names
        uint64     totalSize;                                // current size of cd/dvd/bd [bytes]
      } write;
    } opticalDisk;

    // device storage
    struct
    {
      // read device
      struct
      {
        #ifdef HAVE_ISO9660
        #endif /* HAVE_ISO9660 */
      } read;

      // write device
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
        String     blankCommand;                             // command to blank medium before writing
        String     writePreProcessCommand;                   // command to execute before writing volume
        String     writePostProcessCommand;                  // command to execute after writing volume
        String     writeCommand;                             // command to write volume

        String     directory;                                // temporary directory for files

        uint       number;                                   // volume number
        bool       newVolumeFlag;                            // TRUE iff new volume needed
        StringList fileNameList;                             // list with file names
        uint64     totalSize;                                // current size [bytes]
      } write;
    } device;
  };

  StorageStatusInfo runningInfo;
} StorageInfo;

// storage handle
typedef struct
{
  StorageInfo                  *storageInfo;
  StorageModes                 mode;                         // storage mode: READ, WRITE
//TODO: use stoageInfo->specifier->name?
  String                       archiveName;                  // archive name

  union
  {
    // file storage
    struct
    {
      FileHandle fileHandle;                                 // file handle
    } fileSystem;

    #if defined(HAVE_CURL)
      // FTP storage
      struct
      {
        CURLM                   *curlMultiHandle;
        CURL                    *curlHandle;
        uint64                  index;                       // current read/write index in file [0..n-1]
        uint64                  size;                        // size of file [bytes]
        struct                                               // read-ahead buffer
        {
          byte   *data;                                      // read ahead data buffer
          uint64 offset;                                     // offset in file
          ulong  length;                                     // length of read ahead data
        } readAheadBuffer;
        void                    *buffer;                     // next data to write/read
        ulong                   length;                      // length of data to write/read
        ulong                   transferedBytes;             // number of data bytes read/written
      } ftp;

      // WebDAV storage
      struct
      {
        CURLM                   *curlMultiHandle;
        CURL                    *curlHandle;
        uint64                  index;                       // current read/write index in file [0..n-1]
        uint64                  size;                        // size of file [bytes]
        struct                                               // receive buffer
        {
          byte   *data;                                      // data received
          ulong  size;                                       // buffer size [bytes]
          uint64 offset;                                     // data offset
          ulong  length;                                     // length of data received
        } receiveBuffer;
        struct                                               // send buffer
        {
          const byte *data;                                  // data to send
          ulong      index;                                  // data index
          ulong      length;                                 // length of data to send
        } sendBuffer;
      } webdav;
    #endif /* HAVE_CURL */

    #ifdef HAVE_SSH2
      // ssh storage (remote BAR)
      struct
      {
        SocketHandle            socketHandle;
        LIBSSH2_CHANNEL         *channel;                    // ssh channel
        LIBSSH2_SEND_FUNC((*oldSendCallback));               // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));            // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;              // total sent bytes
        uint64                  totalReceivedBytes;          // total received bytes
        StorageBandWidthLimiter bandWidthLimiter;            // band width limiter data
      } ssh;

      // scp storage
      struct
      {
        SocketHandle            socketHandle;
        LIBSSH2_CHANNEL         *channel;                    // scp channel
        LIBSSH2_SEND_FUNC((*oldSendCallback));               // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));            // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;              // total sent bytes
        uint64                  totalReceivedBytes;          // total received bytes
        uint64                  index;                       // current read/write index in file [0..n-1]
        uint64                  size;                        // size of file [bytes]
        struct                                               // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
      } scp;

      // sftp storage
      struct
      {
        SocketHandle            socketHandle;
        LIBSSH2_SEND_FUNC((*oldSendCallback));               // libssh2 callback to send data (used to track sent bytes)
        LIBSSH2_RECV_FUNC((*oldReceiveCallback));            // libssh2 callback to receive data (used to track received bytes)
        uint64                  totalSentBytes;              // total sent bytes
        uint64                  totalReceivedBytes;          // total received bytes
        LIBSSH2_SFTP            *sftp;                       // sftp session
        LIBSSH2_SFTP_HANDLE     *sftpHandle;                 // sftp handle
        uint64                  index;                       // current read/write index in file [0..n-1]
        uint64                  size;                        // size of file [bytes]
        struct                                               // read-ahead buffer
        {
          byte   *data;
          uint64 offset;
          ulong  length;
        } readAheadBuffer;
      } sftp;
    #endif /* HAVE_SSH2 */

    // cd/dvd/bd storage
    struct
    {
      // read cd/dvd/bd
      struct
      {
        #ifdef HAVE_ISO9660
          iso9660_t      *iso9660Handle;                     // ISO9660 image handle
          iso9660_stat_t *iso9660Stat;                       // ISO9660 file handle
          uint64         index;                              // current read/write index in ISO image [0..n-1]

          struct                                             // read buffer
          {
            byte   *data;
            uint64 blockIndex;                               // ISO9660 block index
            ulong  length;
          } buffer;
        #endif /* HAVE_ISO9660 */
      } read;

      // write cd/dvd/bd
      struct
      {
        String     fileName;                                 // current file name
        FileHandle fileHandle;
      } write;
    } opticalDisk;

    // device storage
    struct
    {
      // write cd/dvd/bd
      struct
      {
        String     fileName;                                 // current file name
        FileHandle fileHandle;
      } write;
    } device;

    // master storage
//TODO: required?
    struct
    {
      uint64     index;                                      // current read/write index in file [0..n-1]
      uint64     size;                                       // size of file [bytes]
    } master;
  };
} StorageHandle;

// directory list handle
typedef struct
{
  StorageSpecifier storageSpecifier;                         // storage specifier data

  StorageTypes type;
  union
  {
    struct
    {
      DirectoryListHandle directoryListHandle;
    } fileSystem;

    #ifdef HAVE_CURL
      struct
      {
        uint                    serverId;                    // id of allocated server
        String                  pathName;                    // directory name
        StringList              lineList;

        String                  fileName;                    // last parsed entry
        FileTypes               type;
        int64                   size;
        uint64                  timeModified;
        uint32                  userId;
        uint32                  groupId;
        FilePermission          permission;
        bool                    entryReadFlag;               // TRUE if entry read
      } ftp;
    #endif /* HAVE_CURL */

    #if defined(HAVE_CURL) && defined(HAVE_MXML)
      struct
      {
        uint                    serverId;                    // id of allocated server
        String                  pathName;                    // directory name

        mxml_node_t             *rootNode;
        mxml_node_t             *lastNode;
        mxml_node_t             *currentNode;

/*
        String                  fileName;                    // last parsed entry
        FileTypes               type;
        int64                   size;
        uint64                  timeModified;
        uint32                  userId;
        uint32                  groupId;
        FilePermission          permission;
        bool                    entryReadFlag;               // TRUE if entry read
*/
      } webdav;
    #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

    #ifdef HAVE_SSH2
      struct
      {
        String                  pathName;                    // directory name

        SocketHandle            socketHandle;
        LIBSSH2_SESSION         *session;
        LIBSSH2_CHANNEL         *channel;
        LIBSSH2_SFTP            *sftp;
        LIBSSH2_SFTP_HANDLE     *sftpHandle;
        char                    *buffer;                     // buffer for reading file names
        ulong                   bufferLength;
        LIBSSH2_SFTP_ATTRIBUTES attributes;
        bool                    entryReadFlag;               // TRUE if entry read
      } sftp;
    #endif /* HAVE_SSH2 */
    struct
    {
      String                    pathName;                    // directory name
      #ifdef HAVE_ISO9660

        iso9660_t               *iso9660Handle;              // ISO9660 image handle
        CdioList_t              *cdioList;                   // ISO9660 entry list
        CdioListNode_t          *cdioNextNode;               // next entry in list
      #else /* not HAVE_ISO9660 */
        DirectoryListHandle     directoryListHandle;
      #endif /* HAVE_ISO9660 */
    } opticalDisk;
  };
} StorageDirectoryListHandle;

/***********************************************************************\
* Name   : StorageFunction
* Purpose: storage call back
* Input  : storageName - storage name
*          fileInfo    - file info
*          doneCount   - done count
*          totalCount  - total count
*          userData    - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*StorageFunction)(ConstString    storageName,
                                 const FileInfo *fileInfo,
                                 void           *userData
                                );

/***********************************************************************\
* Name   : StorageProgressFunction
* Purpose: storage progress call back
* Input  : doneCount  - done count
*          totalCount - total count
*          userData   - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef void(*StorageProgressFunction)(ulong doneCount,
                                       ulong totalCount,
                                       void  *userData
                                      );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Storage_initSpecifier(...)      __Storage_initSpecifier(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_duplicateSpecifier(...) __Storage_duplicateSpecifier(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_doneSpecifier(...)      __Storage_doneSpecifier(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_init(...)               __Storage_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_done(...)               __Storage_done(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_create(...)             __Storage_create(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_open(...)               __Storage_open(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Storage_close(...)              __Storage_close(__FILE__,__LINE__, ## __VA_ARGS__)
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
* Return : ERROR_NONE or error code
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
* Input  : storageSpecifier - storage specifier
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
* Name   : Storage_isPatternSpecifier
* Purpose: check if specifiers contain pattern
* Input  : storageSpecifier - storage specifier
* Output : -
* Return : TRUE iff pattern
* Notes  : -
\***********************************************************************/

INLINE bool Storage_isPatternSpecifier(const StorageSpecifier *storageSpecifier);
#if defined(NDEBUG) || defined(__STORAGE_IMPLEMENTATION__)
INLINE bool Storage_isPatternSpecifier(const StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  return storageSpecifier->archivePatternString != NULL;
}
#endif /* NDEBUG || __STORAGE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Storage_equalSpecifiers
* Purpose: compare specifiers if equals
* Input  : storageSpecifier1,storageSpecifier2 - specifiers
*          archiveName1,archiveName2           - archive names (can be
*                                                NULL)
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

bool Storage_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                             ConstString            archiveName1,
                             const StorageSpecifier *storageSpecifier2,
                             ConstString            archiveName2
                            );

/***********************************************************************\
* Name   : Storage_isInFileSystem
* Purpose: check if in file system
* Input  : storageSpecifier - storage specifier
* Output : -
* Return : TRUE if in file system, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Storage_isInFileSystem(const StorageSpecifier *storageSpecifier);
#if defined(NDEBUG) || defined(__STORAGE_IMPLEMENTATION__)
INLINE bool Storage_isInFileSystem(const StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  return storageSpecifier->type == STORAGE_TYPE_FILESYSTEM;
}
#endif /* NDEBUG || __STORAGE_IMPLEMENTATION__ */

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

bool Storage_parseFTPSpecifier(ConstString ftpSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName,
                               Password    *loginPassword
                              );

/***********************************************************************\
* Name   : Storage_parseSSHSpecifier
* Purpose: parse ssh specifier:
*            [<login name>@]<host name>[:<host port>]
* Input  : sshSpecifier - ssh specifier string
*          hostName     - host name variable (can be NULL)
*          hostPort     - host port number variable (can be NULL)
*          loginName    - login name variable (can be NULL)
* Output : hostName  - host name (can be NULL)
*          hostPort  - host port number (can be NULL)
*          loginName - login name (can be NULL)
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseSSHSpecifier(ConstString sshSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName
                              );

/***********************************************************************\
* Name   : Storage_parseSCPSpecifier
* Purpose: parse scp specifier:
*            [<login name>@]<host name>[:<host port>]
* Input  : sshSpecifier - ssh specifier string
*          hostName     - host name variable (can be NULL)
*          hostPort     - host port number variable (can be NULL)
*          loginName    - login name variable (can be NULL)
* Output : hostName      - host name (can be NULL)
*          hostPort      - host port number (can be NULL)
*          loginName     - login name (can be NULL)
*          loginPassword - login password
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseSCPSpecifier(ConstString scpSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName,
                               Password    *loginPassword
                              );

/***********************************************************************\
* Name   : Storage_parseSFTPSpecifier
* Purpose: parse sftp specifier:
*            [<login name>@]<host name>[:<host port>]
* Input  : sftpSpecifier - ssh specifier string
*          hostName      - host name variable (can be NULL)
*          hostPort      - host port number variable (can be NULL)
*          loginName     - login name variable (can be NULL)
* Output : hostName      - host name (can be NULL)
*          hostPort      - host port number (can be NULL)
*          loginName     - login name (can be NULL)
*          loginPassword - login password
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseSFTPSpecifier(ConstString sftpSpecifier,
                                String      hostName,
                                uint        *hostPort,
                                String      loginName,
                                Password    *loginPassword
                               );

/***********************************************************************\
* Name   : Storage_parseWebDAVSpecifier
* Purpose: parse WebDAV specifier:
*            [<login name>@]<host name>
* Input  : webdavSpecifier - WebDAV specifier string
*          hostName        - host name variable (can be NULL)
*          hostPort        - host port number variable (can be NULL)
*          loginName       - login name variable (can be NULL)
*          loginPassword   - login password variable (can be NULL)
* Output : hostName      - host name (can be NULL)
*          loginName     - login name (can be NULL)
*          loginPassword - login password
* Return : TRUE if WebDAV specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseWebDAVSpecifier(ConstString webdavSpecifier,
                                  String      hostName,
                                  String      loginName,
                                  Password    *loginPassword
                                 );

/***********************************************************************\
* Name   : Storage_parseOpticalSpecifier
* Purpose: parse device specifier:
*            <device name>:
* Input  : deviceSpecifier   - device specifier string
*          defaultDeviceName - default device name
*          deviceName        - device name variable (can be NULL)
* Output : deviceName - device name (can be NULL)
* Return : TRUE if device specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseOpticalSpecifier(ConstString opticalSpecifier,
                                   ConstString defaultDeviceName,
                                   String      deviceName
                                  );

/***********************************************************************\
* Name   : Storage_parseDeviceSpecifier
* Purpose: parse device specifier:
*            <device name>:
* Input  : deviceSpecifier   - device specifier string
*          defaultDeviceName - default device name
*          deviceName        - device name variable (can be NULL)
* Output : deviceName - device name (can be NULL)
* Return : TRUE if device specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

bool Storage_parseDeviceSpecifier(ConstString deviceSpecifier,
                                  ConstString defaultDeviceName,
                                  String      deviceName
                                 );

/***********************************************************************\
* Name   : Storage_parseType
* Purpose: get storage type from storage name
* Input  : storageName - storage name
* Output : -
* Return : storage type
* Notes  : name structure:
*            <type>://<storage specifier>/<archiveName>
\***********************************************************************/

StorageTypes Storage_parseType(ConstString storageName);

/***********************************************************************\
* Name   : Storage_parseName
* Purpose: parse storage name and get storage specifier
* Input  : storageSpecifier - storage specific variable
*          storageName      - storage name (may include pattern)
* Output : storageSpecifier - storage specific data
* Return : ERROR_NONE or error code
* Notes  : name structure:
*            <type>://<storage specifier>/<archiveName>
\***********************************************************************/

Errors Storage_parseName(StorageSpecifier *storageSpecifier,
                         ConstString      storageName
                        );

/***********************************************************************\
* Name   : Storage_equalNames
* Purpose: check if storage names identify the same archive
* Input  : storageName1,storageName2 - storage names
* Output : -
* Return : TURE iff storage names identifty the same archive
* Notes  : -
\***********************************************************************/

bool Storage_equalNames(ConstString storageName1,
                        ConstString storageName2
                       );

/***********************************************************************\
* Name   : Storage_getName
* Purpose: get storage name
* Input  : string                 - name variable (can be NULL)
*          storageSpecifierString - storage specifier string
*          archiveName            - archive name (can be NULL)
* Output : -
* Return : storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

String Storage_getName(String                 string,
                       const StorageSpecifier *storageSpecifier,
                       ConstString            archiveName
                      );

/***********************************************************************\
* Name   : Storage_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string                 - name variable (can be NULL)
*          storageSpecifierString - storage specifier string
*          archiveName            - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

String Storage_getPrintableName(String                 string,
                                const StorageSpecifier *storageSpecifier,
                                ConstString            archiveName
                               );

/***********************************************************************\
* Name   : Storage_getServerSettings
* Purpose: get server settings
* Input  : server           - server variable
*          storageSpecifier - storage specifier
*          jobOptions       - job options
* Output : server - server
* Return : unique server id
* Notes  : -
\***********************************************************************/

uint Storage_getServerSettings(Server                 *server,
                               const StorageSpecifier *storageSpecifier,
                               const JobOptions       *jobOptions
                              );


/***********************************************************************\
* Name   : Storage_prepare
* Purpose: prepare storage: read password, init files
* Input  : storageName - storage name:
*          options     - options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_prepare(const String     storageName,
                       const JobOptions *jobOptions
                      );

/***********************************************************************\
* Name   : Storage_init
* Purpose: init new storage
* Input  : storageInfo                     - storage info variable
*          masterIO                        - master I/O (can be NULL)
*          storageSpecifier                - storage specifier structure
*          jobOptions                      - job options or NULL
*          maxBandWidthList                - list with max. band width
*                                            to use [bits/s] or NULL
*          storageFlags                    - storage flags; see
*                                            STORAGE_FLAG_...
*          serverConnectionPriority        - server connection priority
*          storageUpdateStatusInfoFunction - update status info call-back
*          storageUpdateStatusInfoUserData - user data for update status
*                                            info call-back
*          getNamePasswordFunction         - get name/password call-back
*                                            (can be NULL)
*          getNamePasswordUserData         - user data for get password
*                                            call-back
*          storageRequestVolumeFunction    - volume request call-back
*          storageRequestVolumeUserData    - user data for volume
*          isPauseFunction                 - is pause check callback (can
*                                            be NULL)
*          isPauseUserData                 - user data for is pause check
*          isAbortedFunction               - is abort check callback (can
*                                            be NULL)
*          isAbortedUserData               - user data for is aborted
*                                            check
*          logHandle                       - log handle (can be NULL)
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_init(StorageInfo                     *storageInfo,
                      ServerIO                        *masterIO,
                      const StorageSpecifier          *storageSpecifier,
                      const JobOptions                *jobOptions,
                      BandWidthList                   *maxBandWidthList,
                      ServerConnectionPriorities      serverConnectionPriority,
                      StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                      void                            *storageUpdateStatusInfoUserData,
                      GetNamePasswordFunction         getNamePasswordFunction,
                      void                            *getNamePasswordUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData,
                      IsPauseFunction                 isPauseFunction,
                      void                            *isPauseUserData,
                      IsAbortedFunction               isAbortedFunction,
                      void                            *isAbortedUserData,
                      LogHandle                       *logHandle
                     );
#else /* not NDEBUG */
  Errors __Storage_init(const char                      *__fileName__,
                        ulong                           __lineNb__,
                        StorageInfo                     *storageInfo,
                        ServerIO                        *masterIO,
                        const StorageSpecifier          *storageSpecifier,
                        const JobOptions                *jobOptions,
                        BandWidthList                   *maxBandWidthList,
                        ServerConnectionPriorities      serverConnectionPriority,
                        StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                        void                            *storageUpdateStatusInfoUserData,
                        GetNamePasswordFunction         getNamePasswordFunction,
                        void                            *getNamePasswordUserData,
                        StorageRequestVolumeFunction    storageRequestVolumeFunction,
                        void                            *storageRequestVolumeUserData,
                        IsPauseFunction                 isPauseFunction,
                        void                            *isPauseUserData,
                        IsAbortedFunction               isAbortedFunction,
                        void                            *isAbortedUserData,
                        LogHandle                       *logHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_done
* Purpose: deinit storage
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_done(StorageInfo *storageInfo);
#else /* not NDEBUG */
  Errors __Storage_done(const char  *__fileName__,
                        ulong       __lineNb__,
                        StorageInfo *storage
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_getType
* Purpose: get storage type
* Input  : storageInfo - storage info
* Output : -
* Return : storage type
* Notes  : -
\***********************************************************************/

INLINE StorageTypes Storage_getType(const StorageInfo *storageInfo);
#if defined(NDEBUG) || defined(__STORAGE_IMPLEMENTATION__)
INLINE StorageTypes Storage_getType(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  return storageInfo->storageSpecifier.type;
}
#endif /* NDEBUG || __STORAGE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Storage_isServerAllocationPending
* Purpose: check if a server allocation with high priority is pending
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE if server allocation with high priority is pending,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_isServerAllocationPending(const StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_getStorageSpecifier
* Purpose: get storage specifier from from storage handle
* Input  : storageInfo - storage info
* Output : -
* Return : storage specifier
* Notes  : -
\***********************************************************************/

const StorageSpecifier *Storage_getStorageSpecifier(const StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_isAborted
* Purpose: check if aborted
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE iff aborted
* Notes  : -
\***********************************************************************/

INLINE bool Storage_isAborted(const StorageInfo *storageInfo);
#if defined(NDEBUG) || defined(__STORAGE_IMPLEMENTATION__)
INLINE bool Storage_isAborted(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  return (storageInfo->isAbortedFunction != NULL) && storageInfo->isAbortedFunction(storageInfo->isAbortedUserData);
}
#endif /* NDEBUG || __STORAGE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Storage_pause
* Purpose: pause storage
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_pause(const StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_preProcess
* Purpose: pre-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          initialFlag - TRUE iff initial call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_preProcess(StorageInfo *storageInfo,
                          ConstString archiveName,
                          time_t      time,
                          bool        initialFlag
                         );

/***********************************************************************\
* Name   : Storage_postProcess
* Purpose: post-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          finalFlag   - TRUE iff final call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_postProcess(StorageInfo *storageInfo,
                           ConstString archiveName,
                           time_t      time,
                           bool        finalFlag
                          );

/***********************************************************************\
* Name   : Storage_getVolumeNumber
* Purpose: get current volume number
* Input  : storageInfo - storage info
* Output : -
* Return : volume number
* Notes  : -
\***********************************************************************/

uint Storage_getVolumeNumber(const StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_setVolumeNumber
* Purpose: set volume number
* Input  : storageInfo  - storage info
*          volumeNumber - volume number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Storage_setVolumeNumber(StorageInfo *storageInfo,
                             uint        volumeNumber
                            );

/***********************************************************************\
* Name   : Storage_unloadVolume
* Purpose: unload volume
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_unloadVolume(StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_exists
* Purpose: check if storage file exists
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE iff storage file exists
* Notes  : -
\***********************************************************************/

bool Storage_exists(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_isFile
* Purpose: check if storage file
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Storage_isFile(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_isDirectory
* Purpose: check if storage directory
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage directory, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Storage_isDirectory(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_isReadable
* Purpose: check if storage file exists and is readable
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file/directory exists and is readable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool Storage_isReadable(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_isWritable
* Purpose: check if storage file exists and is writable
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file/directory exists and is writable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool Storage_isWritable(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_getTmpName
* Purpose: get temporary archive name
* Input  : archiveName - archive name variable
*          storageInfo - storage info
* Output : archiveName - temporary archive name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_getTmpName(String archiveName, StorageInfo *storageInfo);

/***********************************************************************\
* Name   : Storage_create
* Purpose: create new/append to storage
* Input  : storageHandle - storage handle variable
*          storageInfo   - storage info
*          archiveName   - archive name (can be NULL)
*          archiveSize   - archive size [bytes]
*          forceFlag     - TRUE to force overwrite existing storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_create(StorageHandle *storageHandle,
                        StorageInfo   *storageInfo,
                        ConstString   archiveName,
                        uint64        archiveSize,
                        bool          forceFlag
                       );
#else /* not NDEBUG */
  Errors __Storage_create(const char    *__fileName__,
                          ulong         __lineNb__,
                          StorageHandle *storageHandle,
                          StorageInfo   *storageInfo,
                          ConstString   archiveName,
                          uint64        archiveSize,
                          bool          forceFlag
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_open
* Purpose: open storage for reading
* Input  : storageHandle - storage handle variable
*          storageInfo   - storage info
*          archiveName   - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Storage_open(StorageHandle *storageHandle,
                      StorageInfo   *storageInfo,
                      ConstString   archiveName
                     );
#else /* not NDEBUG */
  Errors __Storage_open(const char    *__fileName__,
                        ulong         __lineNb__,
                        StorageHandle *storageHandle,
                        StorageInfo   *storageInfo,
                        ConstString   archiveName
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_close
* Purpose: close storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Storage_close(StorageHandle *storageHandle);
#else /* not NDEBUG */
  void __Storage_close(const char    *__fileName__,
                       ulong         __lineNb__,
                       StorageHandle *storageHandle
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Storage_eof
* Purpose: check if end-of-file in storage
* Input  : storageHandle - storage handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Storage_eof(StorageHandle *storageHandle);

/***********************************************************************\
* Name   : Storage_read
* Purpose: read from storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
*          bytesRead     - number of bytes read or NULL
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or error code
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
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_write(StorageHandle *storageHandle,
                     const void    *buffer,
                     ulong         size
                    );

/***********************************************************************\
* Name   : Storage_transferFromFile
* Purpose: transfer content of file into storage file
* Input  : fileHandle     - file handle
*          storageHandle  - storage handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_transferFromFile(FileHandle    *fromFileHandle,
                                StorageHandle *storageHandle
                               );

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
*          offset        - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_seek(StorageHandle *storageHandle,
                    uint64        offset
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
* Name   : Storage_copyToLocal
* Purpose: copy storage file to local file
* Input  : storageSpecifier                - storage specifier structure
*          localFileName                   - local archive file name
*          jobOptions                      - job options
*          maxBandWidthLIst                - list with max. band width
*                                            to use [bits/s] or NULL
*          storageUpdateStatusInfoFunction - status info call-back
*          storageUpdateStatusInfoUserData - user data for status info
*                                            call-back
*          storageRequestVolumeFunction    - volume request call-back
*          storageRequestVolumeUserData    - user data for volume request
*                                            call-back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

// TODO: storageSpecifier -> storageInfo
Errors Storage_copyToLocal(const StorageSpecifier          *storageSpecifier,
                           ConstString                     localFileName,
                           const JobOptions                *jobOptions,
                           BandWidthList                   *maxBandWidthList,
                           StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                           void                            *storageUpdateStatusInfoUserData,
                           StorageRequestVolumeFunction    storageRequestVolumeFunction,
                           void                            *storageRequestVolumeUserData
                          );

/***********************************************************************\
* Name   : Storage_copy
* Purpose: copy storage
* Input  : fromStorageInfo, toStorageInfo - fromt/to storage info
*          fromArchiveName, toArchiveName - fromt/to archive name (can be NULL)
*          archiveSize                    - archive size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_copy(StorageInfo *fromStorageInfo,
                    ConstString fromArchiveName,
                    StorageInfo *toStorageInfo,
                    ConstString toArchiveName,
                    uint64      archiveSize
                   );

/***********************************************************************\
* Name   : Storage_rename
* Purpose: rename storage file
* Input  : storageInfo    - storage
*          oldArchiveName - archive names (can be NULL)
*          newArchiveName - new archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_rename(const StorageInfo *storageInfo,
                      ConstString       oldArchiveName,
                      ConstString       newArchiveName
                     );

/***********************************************************************\
* Name   : Storage_delete
* Purpose: delete storage file
* Input  : storageInfo - storage
*          archiveName - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_delete(StorageInfo *storageInfo, ConstString archiveName);

/***********************************************************************\
* Name   : Storage_makeDirectory
* Purpose: create directories
* Input  : storageInfo - storage info
*          pathName    - path name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_makeDirectory(StorageInfo *storageInfo, ConstString pathName);

/***********************************************************************\
* Name   : Storage_pruneDirectories
* Purpose: delete empty directories in path
* Input  : storageInfo - storage info
*          pathName    - path name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_pruneDirectories(StorageInfo *storageInfo, ConstString pathName);

#if 0
still not complete
/***********************************************************************\
* Name   : Storage_getFileInfo
* Purpose: get storage file info
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
*          fileInfo    - file info variable
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_getFileInfo(StorageInfo *storageInfo,
                           ConstString archiveName,
                           FileInfo    *fileInfo
                          );
#endif /* 0 */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Storage_openDirectoryList
* Purpose: open storage directory list for reading directory entries
* Input  : storageDirectoryListHandle - storage directory list handle
*                                       variable
*          storageSpecifier           - storage specifier
*          pathName                   - path name
*          jobOptions                 - job options
*          serverConnectionPriority   - server connection priority
* Output : storageDirectoryListHandle - initialized storage directory
*                                       list handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 const StorageSpecifier     *storageSpecifier,
                                 ConstString                pathName,
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
* Name   : Storage_forAll
* Purpose: execute callback for all storage files
* Input  : storageSpecifier        - storage specifier
*          directory               - directory (can be NULL)
*          patternString           - pattern string (can be NULL)
*          storageFunction         - storage callback function (can be
*                                    NULL)
*          storageUserData         - storage callback user data
*          storageProgressFunction - storage progress callback function
*                                    (can be NULL)
*          storageProgressUserData - storage progress callback user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Storage_forAll(StorageSpecifier        *storageSpecifier,
                      ConstString             directory,
                      const char              *patternString,
                      StorageFunction         storageFunction,
                      void                    *storageUserData,
                      StorageProgressFunction storageProgressFunction,
                      void                    *storageProgressUserData
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __STORAGE__ */

/* end of file */
