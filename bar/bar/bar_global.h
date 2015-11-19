/***********************************************************************\
*
* $Revision: 3438 $
* $Date: 2015-01-02 10:45:04 +0100 (Fri, 02 Jan 2015) $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

#ifndef __BAR_GLOBAL__
#define __BAR_GLOBAL__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "forward.h"         // required for JobOptions

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "files.h"
#include "configvalues.h"
#include "semaphores.h"

#include "compress.h"
#include "passwords.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// run modes
typedef enum
{
  RUN_MODE_INTERACTIVE,
  RUN_MODE_BATCH,
  RUN_MODE_SERVER,
} RunModes;

// archive types
typedef enum
{
  ARCHIVE_TYPE_NORMAL,                  // normal archives; no incremental list file
  ARCHIVE_TYPE_FULL,                    // full archives, create incremental list file
  ARCHIVE_TYPE_INCREMENTAL,             // incremental achives, use and update incremental list file
  ARCHIVE_TYPE_DIFFERENTIAL,            // differential achives, use incremental list file
  ARCHIVE_TYPE_CONTINUOUS,              // continuous archives, use continuous collected file list
  ARCHIVE_TYPE_UNKNOWN,
} ArchiveTypes;

// date/time
/*
#define WEEKDAY_ANY_MONTH \
  (  SET_VALUE(MONTH_JAN) \
   | SET_VALUE(MONTH_FEB) \
   | SET_VALUE(MONTH_MAR) \
   | SET_VALUE(MONTH_APR) \
   | SET_VALUE(MONTH_MAY) \
   | SET_VALUE(MONTH_JUN) \
   | SET_VALUE(MONTH_JUL) \
   | SET_VALUE(MONTH_AUG) \
   | SET_VALUE(MONTH_SEP) \
   | SET_VALUE(MONTH_OCT) \
   | SET_VALUE(MONTH_NOV) \
   | SET_VALUE(MONTH_DEC) \
  )
*/
#define WEEKDAY_SET_ANY \
  (  SET_VALUE(WEEKDAY_MON) \
   | SET_VALUE(WEEKDAY_TUE) \
   | SET_VALUE(WEEKDAY_WED) \
   | SET_VALUE(WEEKDAY_THU) \
   | SET_VALUE(WEEKDAY_FRI) \
   | SET_VALUE(WEEKDAY_SAT) \
   | SET_VALUE(WEEKDAY_SUN) \
  )
#define DATE_ANY -1
#define TIME_ANY -1

// directory strip
#define DIRECTORY_STRIP_ANY -1
#define DIRECTORY_STRIP_NONE 0

// password mode
typedef enum
{
  PASSWORD_MODE_DEFAULT,                // use global password
  PASSWORD_MODE_ASK,                    // ask for password
  PASSWORD_MODE_CONFIG                  // use password from config
} PasswordModes;

// server connection priority
typedef enum
{
  SERVER_CONNECTION_PRIORITY_LOW,
  SERVER_CONNECTION_PRIORITY_HIGH,
} ServerConnectionPriorities;

// archive file modes
typedef enum
{
  ARCHIVE_FILE_MODE_STOP,
  ARCHIVE_FILE_MODE_APPEND,
  ARCHIVE_FILE_MODE_OVERWRITE
} ArchiveFileModes;

/***************************** Datatypes *******************************/

// log types
typedef enum
{
  LOG_TYPE_ALWAYS              = 0,
  LOG_TYPE_ERROR               = (1 <<  0),
  LOG_TYPE_WARNING             = (1 <<  1),
  LOG_TYPE_ENTRY_OK            = (1 <<  2),
  LOG_TYPE_ENTRY_TYPE_UNKNOWN  = (1 <<  3),
  LOG_TYPE_ENTRY_ACCESS_DENIED = (1 <<  4),
  LOG_TYPE_ENTRY_MISSING       = (1 <<  5),
  LOG_TYPE_ENTRY_INCOMPLETE    = (1 <<  6),
  LOG_TYPE_ENTRY_EXCLUDED      = (1 <<  7),
  LOG_TYPE_CONTINUOUS          = (1 <<  8),
  LOG_TYPE_STORAGE             = (1 <<  9),
  LOG_TYPE_INDEX               = (1 << 10),
} LogTypes;

#define LOG_TYPE_NONE 0x00000000
#define LOG_TYPE_ALL  0xFFFFffff

#define MAX_CONNECTION_COUNT_UNLIMITED MAX_INT
#define MAX_STORAGE_SIZE_UNLIMITED     MAX_INT64

// log handle
typedef struct
{
  String logFileName;
  FILE   *logFile;
} LogHandle;

// week day sets
typedef long WeekDaySet;                                      // week days set or WEEKDAY_SET_ANY

// key data
typedef struct
{
  void *data;                                                 // key data
  uint length;                                                // length of key data
} Key;

// band width usage
typedef struct BandWidthNode
{
  LIST_NODE_HEADER(struct BandWidthNode);

  int        year;                                            // valid year or DATE_ANY
  int        month;                                           // valid month or DATE_ANY
  int        day;                                             // valid day or DATE_ANY
  int        hour;                                            // valid hour or TIME_ANY
  int        minute;                                          // valid minute or TIME_ANY
  WeekDaySet weekDaySet;                                      // valid weekday set or WEEKDAY_SET_ANY
  ulong      n;                                               // band with limit [bits/s]
  String     fileName;                                        // file to read band width from
} BandWidthNode;

typedef struct
{
  LIST_HEADER(BandWidthNode);
  ulong  n;
  uint64 lastReadTimestamp;
} BandWidthList;

// File settings
typedef struct
{
} FileServer;

// FTP server settings
typedef struct
{
  String           loginName;                                 // login name
  Password         *password;                                 // login password
} FTPServer;

// SSH server settings
typedef struct
{
  uint             port;                                      // server port (ssh,scp,sftp)
  String           loginName;                                 // login name
  Password         *password;                                 // login password
  Key              publicKey;                                 // public key data (ssh,scp,sftp)
  Key              privateKey;                                // private key data (ssh,scp,sftp)
} SSHServer;

// WebDAV server settings
typedef struct
{
  String           loginName;                                 // login name
  Password         *password;                                 // login password
  Key              publicKey;                                 // public key data
  Key              privateKey;                                // private key data
} WebDAVServer;

// server types
typedef enum
{
  SERVER_TYPE_FTP,
  SERVER_TYPE_SSH,
  SERVER_TYPE_WEBDAV
} ServerTypes;

// server
typedef struct
{
  Semaphore   lock;
  String      name;                                           // server name
  ServerTypes type;                                           // server type
  union
  {
    FTPServer    ftpServer;
    SSHServer    sshServer;
    WebDAVServer webDAVServer;
  };
  uint        maxConnectionCount;                             // max. number of concurrent connections or MAX_CONNECTION_COUNT_UNLIMITED
  uint64      maxStorageSize;                                 // max. number of bytes to store on server
  struct
  {
    uint      lowPriorityRequestCount;                        // number of waiting low priority connection requests
    uint      highPriorityRequestCount;                       // number of waiting high priority connection requests
    uint      count;                                          // number of current connections
  }           connection;
} Server;

// server node
typedef struct ServerNode
{
  LIST_NODE_HEADER(struct ServerNode);

  Server server;
} ServerNode;

// server list
typedef struct
{
  LIST_HEADER(ServerNode);
} ServerList;

// file settings
typedef struct
{
  String writePreProcessCommand;                              // command to execute before writing
  String writePostProcessCommand;                             // command to execute after writing
} File;

// FTP settings
typedef struct
{
  String writePreProcessCommand;                              // command to execute before writing
  String writePostProcessCommand;                             // command to execute after writing
} FTP;

// SCP settings
typedef struct
{
  String writePreProcessCommand;                              // command to execute before writing
  String writePostProcessCommand;                             // command to execute after writing
} SCP;

// SFTP settings
typedef struct
{
  String writePreProcessCommand;                              // command to execute before writing
  String writePostProcessCommand;                             // command to execute after writing
} SFTP;

// WebDAV settings
typedef struct
{
  String writePreProcessCommand;                              // command to execute before writing
  String writePostProcessCommand;                             // command to execute after writing
} WebDAV;

// optical disk settings
typedef struct
{
  String defaultDeviceName;                                   // default device name
  String requestVolumeCommand;                                // command to request new medium
  String unloadVolumeCommand;                                 // command to unload medium
  String loadVolumeCommand;                                   // command to load medium
  uint64 volumeSize;                                          // size of medium [bytes] (0 for default)

  String imagePreProcessCommand;                              // command to execute before creating image
  String imagePostProcessCommand;                             // command to execute after created image
  String imageCommand;                                        // command to create medium image
  String eccPreProcessCommand;                                // command to execute before ECC calculation
  String eccPostProcessCommand;                               // command to execute after ECC calculation
  String eccCommand;                                          // command for ECC calculation
  String writePreProcessCommand;                              // command to execute before writing medium
  String writePostProcessCommand;                             // command to execute after writing medium
  String writeCommand;                                        // command to write medium
  String writeImageCommand;                                   // command to write image on medium
} OpticalDisk;

// device settings
typedef struct
{
  String defaultDeviceName;                                   // default device name
  String requestVolumeCommand;                                // command to request new volume
  String unloadVolumeCommand;                                 // command to unload volume
  String loadVolumeCommand;                                   // command to load volume
  uint64 volumeSize;                                          // size of volume [bytes]

  String imagePreProcessCommand;                              // command to execute before creating image
  String imagePostProcessCommand;                             // command to execute after created image
  String imageCommand;                                        // command to create volume image
  String eccPreProcessCommand;                                // command to execute before ECC calculation
  String eccPostProcessCommand;                               // command to execute after ECC calculation
  String eccCommand;                                          // command for ECC calculation
  String writePreProcessCommand;                              // command to execute before writing volume
  String writePostProcessCommand;                             // command to execute after writing volume
  String writeCommand;                                        // command to write volume
} Device;

typedef struct DeviceNode
{
  LIST_NODE_HEADER(struct DeviceNode);

  String name;                                                // device name
  Device device;
} DeviceNode;

typedef struct
{
  LIST_HEADER(DeviceNode);
} DeviceList;

// global options
typedef struct
{
  RunModes               runMode;

  const char             *barExecutable;                      // name of BAR executable

  uint                   niceLevel;                           // nice level 0..19
  uint                   maxThreads;                          // max. number of concurrent compress/encryption threads or 0

  String                 tmpDirectory;                        // directory for temporary files
  uint64                 maxTmpSize;                          // max. size of temporary files

  BandWidthList          maxBandWidthList;                    // list of max. send/receive bandwidth to use [bits/s]

  ulong                  compressMinFileSize;                 // min. size of file for using compression

  Password               *cryptPassword;                      // default password for encryption/decryption

  Server                 *ftpServer;                          // current selected FTP server
  Server                 *defaultFTPServer;                   // default FTP server

  Server                 *sshServer;                          // current selected SSH server
  Server                 *defaultSSHServer;                   // default SSH server

  Server                 *webDAVServer;                       // current selected WebDAV server
  Server                 *defaultWebDAVServer;                // default WebDAV server

  const ServerList       *serverList;                         // list with FTP/SSH/WebDAV servers

  String                 remoteBARExecutable;

  File                   file;                                // file settings
  FTP                    ftp;                                 // ftp settings
  SCP                    scp;                                 // scp settings
  SFTP                   sftp;                                // sftp settings
  WebDAV                 webdav;                              // WebDAV settings
  OpticalDisk            cd;                                  // CD settings
  OpticalDisk            dvd;                                 // DVD settings
  OpticalDisk            bd;                                  // BD settings

  Device                 *device;                             // current selected device
  const DeviceList       *deviceList;                         // list with devices
  Device                 *defaultDevice;                      // default device

  bool                   indexDatabaseAutoUpdateFlag;         // TRUE for automatic update of index datbase
  BandWidthList          indexDatabaseMaxBandWidthList;       // list of max. band width to use for index updates [bits/s]
  uint                   indexDatabaseKeepTime;               // number of seconds to keep index data of not existing storage

  bool                   groupFlag;                           // TRUE iff entries in list should be grouped
  bool                   allFlag;                             // TRUE iff all entries should be listed/restored
  bool                   longFormatFlag;                      // TRUE iff long format list
  bool                   humanFormatFlag;                     // TRUE iff human format list
  bool                   noHeaderFooterFlag;                  // TRUE iff no header/footer should be printed in list
  bool                   deleteOldArchiveFilesFlag;           // TRUE iff old archive files should be deleted after creating new files
  bool                   ignoreNoBackupFileFlag;              // TRUE iff .nobackup/.NOBACKUP file should be ignored

  bool                   noDefaultConfigFlag;                 // TRUE iff default config should not be read
  bool                   quietFlag;                           // TRUE iff suppress any output
  long                   verboseLevel;                        /* verbosity level
                                                                   0 - none
                                                                   1 - fatal errors
                                                                   2 - processing information
                                                                   3 - external programs
                                                                   4 - stdout+stderr of external programs
                                                                   5 - some SSH debug debug
                                                                   6 - all SSH/FTP/WebDAV debug
                                                              */

  bool                   serverDebugFlag;                     // TRUE iff server debug enabled (for debug only)
} GlobalOptions;

// job options
typedef struct
{
  uint32 userId;                                              // restore user id
  uint32 groupId;                                             // restore group id
} JobOptionsOwner;

// job compress algorithms
typedef struct
{
  CompressAlgorithms delta;                                   // delta compress algorithm to use
  CompressAlgorithms byte;                                    // byte compress algorithm to use
} JobOptionsCompressAlgorithms;

// see forward declaration in forward.h
struct JobOptions
{
  ArchiveTypes                 archiveType;                   // archive type (normal, full, incremental, differential)

  uint64                       archivePartSize;               // archive part size [bytes]

  String                       incrementalListFileName;       // name of incremental list file

  int                          directoryStripCount;           // number of directories to strip in restore or DIRECTORY_STRIP_ANY for all
  String                       destination;                   // destination for restore
  JobOptionsOwner              owner;                         // restore owner

  PatternTypes                 patternType;                   // pattern type

  JobOptionsCompressAlgorithms compressAlgorithms;            // compress algorithms

  CryptTypes                   cryptType;                     // crypt type (symmetric, asymmetric)
  CryptAlgorithms              cryptAlgorithm;                // crypt algorithm to use
  PasswordModes                cryptPasswordMode;             // crypt password mode
  Password                     *cryptPassword;                // crypt password
//TODO: key data instead of file anme
  String                       cryptPublicKeyFileName;
  String                       cryptPrivateKeyFileName;

  String                       preProcessCommand;             // command to execute before start of job
  String                       postProcessCommand;            // command to execute after after termination of job

  String                       mountDeviceName;               // device to mount/unmount
  FTPServer                    ftpServer;                     // job specific FTP server settings
  SSHServer                    sshServer;                     // job specific SSH server settings
  WebDAVServer                 webDAVServer;                  // job specific WebDAV server settings
  OpticalDisk                  opticalDisk;                   // job specific optical disk settings
  String                       deviceName;                    // device name to use
  Device                       device;                        // job specific device settings

  uint64                       maxStorageSize;                // max. number of storage bytes for jobs
  uint64                       volumeSize;                    // volume size or 0LL for default [bytes]

  bool                         skipUnreadableFlag;            // TRUE for skipping unreadable files
  bool                         forceDeltaCompressionFlag;     // TRUE to force delta compression of files
  bool                         ignoreNoDumpAttributeFlag;     // TRUE for ignoring no-dump attribute
  ArchiveFileModes             archiveFileMode;               // archive files write mode
  bool                         overwriteFilesFlag;            // TRUE for overwrite existing files on restore
  bool                         errorCorrectionCodesFlag;      // TRUE iff error correction codes should be added
  bool                         alwaysCreateImageFlag;         // TRUE iff always create image for CD/DVD/BD/device
  bool                         waitFirstVolumeFlag;           // TRUE for wait for first volume
  bool                         rawImagesFlag;                 // TRUE for storing raw images
  bool                         noFragmentsCheckFlag;          // TRUE to skip checking file fragments for completeness
  bool                         noIndexDatabaseFlag;           // TRUE for do not store index database for archives
  bool                         dryRunFlag;                    // TRUE to do a dry-run (do not store, do not create incremental data, do not store in database)
  bool                         noStorageFlag;                 // TRUE to skip storage, only create incremental data file
  bool                         noBAROnMediumFlag;             // TRUE for not storing BAR on medium
  bool                         stopOnErrorFlag;

  // shortcuts
  bool                         archiveFileModeOverwriteFlag;  // TRUE for overwrite existing archive files
};

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __BAR_GLOBAL__ */

/* end of file */
