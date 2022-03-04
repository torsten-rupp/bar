/***********************************************************************\
*
* $Revision: 3438 $
* $Date: 2015-01-02 10:45:04 +0100 (Fri, 02 Jan 2015) $
* $Author: torsten $
* Contents: Backup ARchiver common functions
* Systems: all
*
\***********************************************************************/

#ifndef __BAR_COMMON__
#define __BAR_COMMON__/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "forward.h"         // required for JobOptions

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/configvalues.h"
#include "common/semaphores.h"
#include "common/passwords.h"
#include "common/patternlists.h"
#include "common/misc.h"

#include "entrylists.h"
#include "compress.h"
#include "crypt.h"
#include "archive_format_const.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define DEFAULT_CONFIG_FILE_NAME                  "bar.cfg"
#define DEFAULT_LOG_FORMAT                        "%Y-%m-%d %H:%M:%S"
#define DEFAULT_FRAGMENT_SIZE                     (64LL*MB)
#define DEFAULT_COMPRESS_MIN_FILE_SIZE            32
#define DEFAULT_SERVER_PORT                       38523
#ifdef HAVE_GNU_TLS
  #define DEFAULT_TLS_SERVER_PORT                 38524
  #define DEFAULT_TLS_SERVER_CA_FILE              "certs/bar-ca.pem"
  #define DEFAULT_TLS_SERVER_CERTIFICATE_FILE     "certs/bar-server-cert.pem"
  #define DEFAULT_TLS_SERVER_KEY_FILE             "private/bar-server-key.pem"
#else /* not HAVE_GNU_TLS */
  #define DEFAULT_TLS_SERVER_PORT                 0
  #define DEFAULT_TLS_SERVER_CA_FILE              ""
  #define DEFAULT_TLS_SERVER_CERTIFICATE_FILE     ""
  #define DEFAULT_TLS_SERVER_KEY_FILE             ""
#endif /* HAVE_GNU_TLS */
#define DEFAULT_MAX_SERVER_CONNECTIONS            8

#define DEFAULT_JOBS_SUB_DIRECTORY                "jobs"
#define DEFAULT_INCREMENTAL_DATA_SUB_DIRECTORY    ""
#define DEFAULT_PAIRING_MASTER_FILE_NAME          "pairing"
#define DEFAULT_PID_FILE_NAME                     "/run/bar.pid"

#define DEFAULT_CD_DEVICE_NAME                    "/dev/cdrw"
#define DEFAULT_DVD_DEVICE_NAME                   "/dev/dvd"
#define DEFAULT_BD_DEVICE_NAME                    "/dev/bd"
#define DEFAULT_DEVICE_NAME                       "/dev/raw"

#define DEFAULT_CD_VOLUME_SIZE                    0LL
#define DEFAULT_DVD_VOLUME_SIZE                   0LL
#define DEFAULT_BD_VOLUME_SIZE                    0LL
#define DEFAULT_DEVICE_VOLUME_SIZE                0LL

#define DEFAULT_VERBOSE_LEVEL                     1
#define DEFAULT_VERBOSE_LEVEL_INTERACTIVE         1
#define DEFAULT_SERVER_DEBUG_LEVEL                0

#define MOUNT_COMMAND                             "mount -p 0 %directory"
#define MOUNT_DEVICE_COMMAND                      "mount -p 0 %device %directory"
#define UNMOUNT_COMMAND                           "umount %directory"

#define CD_UNLOAD_VOLUME_COMMAND                  "eject %device"
#define CD_LOAD_VOLUME_COMMAND                    "eject -t %device"
#define CD_IMAGE_COMMAND                          "nice mkisofs -V Backup -volset %number -J -r -o %image %directory"
#define CD_ECC_COMMAND                            "nice dvdisaster -mRS03 -x %j1 -c -i %image -v"
#define CD_BLANK_COMMAND                          "nice dvd+rw-format -force %device"
#define CD_WRITE_COMMAND                          "nice sh -c 'mkisofs -V Backup -volset %number -J -r -o %image %directory && cdrecord dev=%device %image'"
#define CD_WRITE_IMAGE_COMMAND                    "nice cdrecord dev=%device %image"

#define DVD_UNLOAD_VOLUME_COMMAND                 "eject %device"
#define DVD_LOAD_VOLUME_COMMAND                   "eject -t %device"
#define DVD_IMAGE_COMMAND                         "nice mkisofs -V Backup -volset %number -J -r -o %image %directory"
#define DVD_ECC_COMMAND                           "nice dvdisaster -mRS03 -x %j1 -c -i %image -v"
#define DVD_BLANK_COMMAND                         "nice dvd+rw-format -force %device"
#define DVD_WRITE_COMMAND                         "nice growisofs -Z %device -A BAR -V Backup -volset %number -J -r %directory -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -use-the-force-luke=tty"
//#warning todo remove -dry-run
//#define DVD_WRITE_COMMAND                         "nice growisofs -Z %device -A BAR -V Backup -volset %number -dry-run -J -r  %directory"
#define DVD_WRITE_IMAGE_COMMAND                   "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -use-the-force-luke=tty"
//#warning todo remove -dry-run
//#define DVD_WRITE_IMAGE_COMMAND                   "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -dry-run"

#define BD_UNLOAD_VOLUME_COMMAND                  "eject %device"
#define BD_LOAD_VOLUME_COMMAND                    "eject -t %device"
#define BD_IMAGE_COMMAND                          "nice mkisofs -V Backup -volset %number -J -r -o %image %directory"
#define BD_ECC_COMMAND                            "nice dvdisaster -mRS03 -x %j1 -c -i %image -v"
#define BD_BLANK_COMMAND                          "nice dvd+rw-format -force %device"
#define BD_WRITE_COMMAND                          "nice growisofs -Z %device -A BAR -V Backup -volset %number -J -r %directory -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -use-the-force-luke=tty"
//#warning todo remove -dry-run
//#define BD_WRITE_COMMAND                          "nice growisofs -Z %device -A BAR -V Backup -volset %number -dry-run -J -r %directory"
#define BD_WRITE_IMAGE_COMMAND                    "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -use-the-force-luke=tty"
//#define BD_WRITE_IMAGE_COMMAND                    "nice growisofs -Z %device=%image -use-the-force-luke=dao -dvd-compat -use-the-force-luke=noload -use-the-force-luke=tty -dry-run"

#define MIN_PASSWORD_QUALITY_LEVEL                0.6

// file name extensions
#define FILE_NAME_EXTENSION_ARCHIVE_FILE          ".bar"
#define FILE_NAME_EXTENSION_INCREMENTAL_FILE      ".bid"

// program exit codes
typedef enum
{
  EXITCODE_OK                     =   0,
  EXITCODE_FAIL                   =   1,

  EXITCODE_INVALID_ARGUMENT       =   5,
  EXITCODE_CONFIG_ERROR,

  EXITCODE_TESTCODE               = 124,
  EXITCODE_INIT_FAIL              = 125,
  EXITCODE_FATAL_ERROR            = 126,
  EXITCODE_FUNCTION_NOT_SUPPORTED = 127,

  EXITCODE_UNKNOWN                = 128
} ExitCodes;

// run modes
typedef enum
{
  RUN_MODE_INTERACTIVE,
  RUN_MODE_BATCH,
  RUN_MODE_SERVER
} RunModes;

// server modes
typedef enum
{
  SERVER_MODE_MASTER,
  SERVER_MODE_SLAVE
} ServerModes;

// archive types
typedef enum
{
//TODO: remove?
  ARCHIVE_TYPE_NONE,

  ARCHIVE_TYPE_NORMAL       = CHUNK_CONST_ARCHIVE_TYPE_NORMAL,        // normal archives; no incremental list file
  ARCHIVE_TYPE_FULL         = CHUNK_CONST_ARCHIVE_TYPE_FULL,          // full archives, create incremental list file
  ARCHIVE_TYPE_INCREMENTAL  = CHUNK_CONST_ARCHIVE_TYPE_INCREMENTAL,   // incremental achives, use and update incremental list file
  ARCHIVE_TYPE_DIFFERENTIAL = CHUNK_CONST_ARCHIVE_TYPE_DIFFERENTIAL,  // differential achives, use incremental list file
  ARCHIVE_TYPE_CONTINUOUS   = CHUNK_CONST_ARCHIVE_TYPE_CONTINUOUS,    // continuous archives, use continuous collected file list

//TODO: separate?
  ARCHIVE_TYPE_ANY,

  ARCHIVE_TYPE_UNKNOWN
} ArchiveTypes;

#define ARCHIVE_TYPE_MIN ARCHIVE_TYPE_NORMAL
#define ARCHIVE_TYPE_MAX ARCHIVE_TYPE_CONTINUOUS

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
  PASSWORD_MODE_NONE,                   // no more passwords
  PASSWORD_MODE_DEFAULT,                // use global password
  PASSWORD_MODE_CONFIG,                 // use password from config
  PASSWORD_MODE_ASK                     // ask for password
} PasswordModes;

// password types
typedef enum
{
  PASSWORD_TYPE_CRYPT,
  PASSWORD_TYPE_FTP,
  PASSWORD_TYPE_SSH,
  PASSWORD_TYPE_WEBDAV
} PasswordTypes;

// server connection priority
typedef enum
{
  SERVER_CONNECTION_PRIORITY_LOW,
  SERVER_CONNECTION_PRIORITY_HIGH
} ServerConnectionPriorities;

// archive file modes
typedef enum
{
  ARCHIVE_FILE_MODE_STOP,
  ARCHIVE_FILE_MODE_RENAME,
  ARCHIVE_FILE_MODE_APPEND,
  ARCHIVE_FILE_MODE_OVERWRITE
} ArchiveFileModes;

// restore entry modes
typedef enum
{
  RESTORE_ENTRY_MODE_STOP,
  RESTORE_ENTRY_MODE_RENAME,
  RESTORE_ENTRY_MODE_OVERWRITE,
  RESTORE_ENTRY_MODE_SKIP_EXISTING
} RestoreEntryModes;

#define INDEX_TIMEOUT       (10L*60L*MS_PER_SECOND)  // index timeout [ms]
#define INDEX_PURGE_TIMEOUT (30L*MS_PER_SECOND)      // index purge timeout [ms]

// log types
typedef enum
{
  LOG_TYPE_ALWAYS              = 0,
  LOG_TYPE_ERROR               = (1 <<  0),
  LOG_TYPE_WARNING             = (1 <<  1),
  LOG_TYPE_INFO                = (1 <<  2),
  LOG_TYPE_ENTRY_OK            = (1 <<  3),
  LOG_TYPE_ENTRY_TYPE_UNKNOWN  = (1 <<  4),
  LOG_TYPE_ENTRY_ACCESS_DENIED = (1 <<  5),
  LOG_TYPE_ENTRY_MISSING       = (1 <<  6),
  LOG_TYPE_ENTRY_INCOMPLETE    = (1 <<  7),
  LOG_TYPE_ENTRY_EXCLUDED      = (1 <<  8),
  LOG_TYPE_CONTINUOUS          = (1 <<  9),
  LOG_TYPE_STORAGE             = (1 << 10),
  LOG_TYPE_INDEX               = (1 << 11),
} LogTypes;

#define LOG_TYPE_NONE 0x00000000
#define LOG_TYPE_ALL  0xFFFFffff

// hash algorithm used for passwords
#ifdef HAVE_GCRYPT
  #define PASSWORD_HASH_ALGORITHM CRYPT_HASH_ALGORITHM_SHA2_256
#else /* not HAVE_GCRYPT */
  #define PASSWORD_HASH_ALGORITHM CRYPT_HASH_ALGORITHM_NONE
#endif /* HAVE_GCRYPT */

// id none
#define ID_NONE                                  0

// keep all
#define KEEP_ALL                                 -1

// forever age
#define AGE_FOREVER                              -1

#define PROGRESS_FLAG_WHEEL      (1 << 0)
#define PROGRESS_FLAG_PERCENTAGE (1 << 1)

/***************************** Datatypes *******************************/

// transform
typedef struct
{
  PatternTypes patternType;
  String       patternString;
  String       replace;
} Transform;

// owner
typedef struct
{
  uint32 userId;                                              // user id
  uint32 groupId;                                             // group id
} Owner;

// compress algorithms
typedef struct
{
  CompressAlgorithms delta;                                   // delta compress algorithm to use
  CompressAlgorithms byte;                                    // byte compress algorithm to use
} CompressAlgorithmsDeltaByte;

// log handle
typedef struct
{
  String logFileName;
  FILE   *logFile;
} LogHandle;

// week day sets
//typedef long WeekDaySet;                                      // week days set or WEEKDAY_SET_ANY
typedef Set WeekDaySet;

// certificate data
typedef struct
{
  enum
  {
    CERTIFICATE_TYPE_NONE,
    CERTIFICATE_TYPE_FILE,
    CERTIFICATE_TYPE_CONFIG
  }      type;
  String fileName;

  void *data;                                                 // data
  uint length;                                                // length of data
} Certificate;

// private/public key data
typedef struct
{
  enum
  {
    KEY_TYPE_NONE,
    KEY_TYPE_FILE,
    KEY_TYPE_CONFIG
  }      type;
  String fileName;

  void *data;                                                 // data
  uint length;                                                // length of data
} Key;
extern const Key KEY_NONE;

// hash data
typedef struct
{
  CryptHashAlgorithms cryptHashAlgorithm;
  void                *data;                                  // data
  uint                length;                                 // length of data
} Hash;
extern const Hash HASH_NONE;

// mount
typedef struct MountNode
{
  LIST_NODE_HEADER(struct MountNode);

  uint   id;                                                  // unique mount id
  String name;                                                // mount point
  String device;                                              // mount device (optional)
  bool   mounted;                                             // TRUE iff mounted by BAR
  uint   mountCount;                                          // mount count (0 to unmount/not mounted)
} MountNode;

typedef struct
{
  LIST_HEADER(MountNode);
} MountList;

// schedule date/time
typedef struct
{
  int year;                                                   // year or DATE_ANY
  int month;                                                  // month or DATE_ANY
  int day;                                                    // day or DATE_ANY
} ScheduleDate;

typedef WeekDaySet ScheduleWeekDaySet;

typedef struct
{
  int hour;                                                   // hour or TIME_ANY
  int minute;                                                 // minute or TIME_ANY
} ScheduleTime;

typedef struct ScheduleNode
{
  LIST_NODE_HEADER(struct ScheduleNode);

  // settings
  String             uuid;                                    // unique schedule id
  String             parentUUID;                              // unique parent id or NULL
//TODO: begin/end date/time
  ScheduleDate       date;
  ScheduleWeekDaySet weekDaySet;
  ScheduleTime       time;
  ArchiveTypes       archiveType;                             // archive type to create
  uint               interval;                                // continuous interval [min]
  String             customText;                              // custom text
  ScheduleTime       beginTime,endTime;                       // begin/end time
  bool               testCreatedArchives;                     // TRUE for simple test created archives
  bool               noStorage;                               // TRUE to skip storage, only create incremental data
  bool               enabled;                                 // TRUE iff enabled

  // running info
  uint64             lastExecutedDateTime;                    // last execution date/time (timestamp; read from file <jobs directory>/.<jobname>)

  // cached statistics info
  ulong              totalEntityCount;                        // total number of entities of last execution
  ulong              totalStorageCount;                       // total number of storage files of last execution
  uint64             totalStorageSize;                        // total size of storage files of last execution
  ulong              totalEntryCount;                         // total number of entries of last execution
  uint64             totalEntrySize;                          // total size of entities of last execution

  // deprecated
  bool               deprecatedPersistenceFlag;               // TRUE iff deprecated persistance data is set
  int                minKeep,maxKeep;                         // min./max keep count
  int                maxAge;                                  // max. age [days]
} ScheduleNode;

typedef struct
{
  LIST_HEADER(ScheduleNode);
} ScheduleList;

// persistence
typedef struct PersistenceNode
{
  LIST_NODE_HEADER(struct PersistenceNode);

  uint         id;
  ArchiveTypes archiveType;                                   // archive type to create
  int          minKeep,maxKeep;                               // min./max keep count
  int          maxAge;                                        // max. age [days]
} PersistenceNode;

typedef struct
{
  LIST_HEADER(PersistenceNode);
  uint64 lastModificationDateTime;                            // last modification date/time
} PersistenceList;

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
  Password         password;                                  // login password
} FTPServer;

// SSH server settings
typedef struct
{
  uint             port;                                      // server port (ssh,scp,sftp)
  String           loginName;                                 // login name
  Password         password;                                  // login password
  Key              publicKey;                                 // public key data (ssh,scp,sftp)
  Key              privateKey;                                // private key data (ssh,scp,sftp)
} SSHServer;

// WebDAV server settings
typedef struct
{
  String           loginName;                                 // login name
  Password         password;                                  // login password
  Key              publicKey;                                 // public key data
  Key              privateKey;                                // private key data
} WebDAVServer;

// server types
typedef enum
{
  SERVER_TYPE_NONE,

  SERVER_TYPE_FILE,
  SERVER_TYPE_FTP,
  SERVER_TYPE_SSH,
  SERVER_TYPE_WEBDAV
} ServerTypes;

// server node
typedef struct ServerNode
{
  LIST_NODE_HEADER(struct ServerNode);

  uint        id;                                             // unique server id
  String      name;                                           // server file name or URL
  ServerTypes type;                                           // server type
  union
  {
    FileServer   file;
    FTPServer    ftp;
    SSHServer    ssh;
    WebDAVServer webDAV;
  };
  uint        maxConnectionCount;                             // max. number of concurrent connections or MAX_CONNECTION_COUNT_UNLIMITED
  uint64      maxStorageSize;                                 // max. number of bytes to store on server
  String      writePreProcessCommand;                         // command to execute before writing
  String      writePostProcessCommand;                        // command to execute after writing

  StringList  commentList;

  struct
  {
    uint lowPriorityRequestCount;                             // number of waiting low priority connection requests
    uint highPriorityRequestCount;                            // number of waiting high priority connection requests
    uint count;                                               // number of current connections
  }           connection;
} ServerNode;

// server list
typedef struct
{
  LIST_HEADER(ServerNode);

  Semaphore lock;
} ServerList;

// server (alias)
typedef ServerNode Server;

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
  String deviceName;                                          // device name
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
  String blankCommand;                                        // command to balnk medium
  String writePreProcessCommand;                              // command to execute before writing medium
  String writePostProcessCommand;                             // command to execute after writing medium
  String writeCommand;                                        // command to write medium
  String writeImageCommand;                                   // command to write image on medium
} OpticalDisk;

// device node
typedef struct DeviceNode
{
  LIST_NODE_HEADER(struct DeviceNode);

  uint        id;                                             // unique device id
  String      name;                                           // device name
  String      requestVolumeCommand;                           // command to request new volume
  String      unloadVolumeCommand;                            // command to unload volume
  String      loadVolumeCommand;                              // command to load volume
  uint64      volumeSize;                                     // size of volume [bytes]

  String      imagePreProcessCommand;                         // command to execute before creating image
  String      imagePostProcessCommand;                        // command to execute after created image
  String      imageCommand;                                   // command to create volume image
  String      eccPreProcessCommand;                           // command to execute before ECC calculation
  String      eccPostProcessCommand;                          // command to execute after ECC calculation
  String      eccCommand;                                     // command for ECC calculation
  String      blankCommand;                                   // command to balnk medium
  String      writePreProcessCommand;                         // command to execute before writing volume
  String      writePostProcessCommand;                        // command to execute after writing volume
  String      writeCommand;                                   // command to write volume

  StringList  commentList;
} DeviceNode;

// device list
typedef struct
{
  LIST_HEADER(DeviceNode);

  Semaphore lock;
} DeviceList;

// device (alias)
typedef struct DeviceNode Device;

// comment
typedef struct
{
  String                       value;                         // comment
  bool                         isSet;                         // TRUE if comment command line option is set
} Comment;

// master info
typedef struct
{
  String     pairingFileName;                                 // name of file to start/clear pairing
  String     name;                                            // name of paired master
  Hash       uuidHash;                                        // master UUID hash
//TODO: required?
  Key        publicKey;
} MasterInfo;

// maintenance date/time
typedef struct
{
  int year;                                                   // year or DATE_ANY
  int month;                                                  // month or DATE_ANY
  int day;                                                    // day or DATE_ANY
} MaintenanceDate;

typedef WeekDaySet MaintenanceWeekDaySet;

typedef struct
{
  int hour;                                                   // hour or TIME_ANY
  int minute;                                                 // minute or TIME_ANY
} MaintenanceTime;

// maintenance node
typedef struct MaintenanceNode
{
  LIST_NODE_HEADER(struct MaintenanceNode);

  uint id;                                                    // unique maintenance id
  MaintenanceDate       date;
  MaintenanceWeekDaySet weekDaySet;
  MaintenanceTime       beginTime,endTime;

  StringList            commentList;
} MaintenanceNode;

// maintenance list
typedef struct
{
  LIST_HEADER(MaintenanceNode);

  Semaphore lock;
} MaintenanceList;

// template expand handle
typedef struct
{
  const char       *templateString;
  ExpandMacroModes expandMacroMode;
  uint64           dateTime;
  const TextMacro  *textMacros;
  uint             textMacroCount;
} TemplateHandle;

// commands
typedef enum
{
  COMMAND_NONE,

  COMMAND_CREATE_FILES,
  COMMAND_CREATE_IMAGES,
  COMMAND_LIST,
  COMMAND_TEST,
  COMMAND_COMPARE,
  COMMAND_RESTORE,
  COMMAND_CONVERT,

  COMMAND_GENERATE_ENCRYPTION_KEYS,
  COMMAND_GENERATE_SIGNATURE_KEYS,
  COMMAND_NEW_KEY_PASSWORD,

  COMMAND_UNKNOWN,
} Commands;

// global options
typedef struct
{
  // --- program options
  RunModes                    runMode;

  String                      barExecutable;                  // name of BAR executable (absolute)

  uint                        niceLevel;                      // nice level 0..19
  uint                        maxThreads;                     // max. number of concurrent compress/encryption threads or 0

  String                      tmpDirectory;                   // directory for temporary files
  uint64                      maxTmpSize;                     // max. size of temporary files

  String                      jobsDirectory;                  // jobs directory
  String                      incrementalDataDirectory;       // incremental data directory

  MasterInfo                  masterInfo;                     // master info

  BandWidthList               maxBandWidthList;               // list of max. send/receive bandwidth to use [bits/s]

  ServerList                  serverList;                     // list with FTP/SSH/WebDAV servers
  DeviceList                  deviceList;                     // list with devices

  Server                      defaultFileServer;
  Server                      defaultFTPServer;
  Server                      defaultSSHServer;
  Server                      defaultWebDAVServer;
  Device                      defaultDevice;

  const char                  *indexDatabaseURI;
  bool                        indexDatabaseUpdateFlag;        // TRUE for update of index database
  bool                        indexDatabaseAutoUpdateFlag;    // TRUE for automatic update of index database
  BandWidthList               indexDatabaseMaxBandWidthList;  // list of max. band width to use for index updates [bits/s]
  uint                        indexDatabaseKeepTime;          // number of seconds to keep index data of not existing storage

  const char                  *continuousDatabaseFileName;

  MaintenanceList             maintenanceList;                // maintenance list

  bool                        metaInfoFlag;                   // TRUE iff meta info should be print
  bool                        groupFlag;                      // TRUE iff entries in list should be grouped
  bool                        allFlag;                        // TRUE iff all entries should be listed/restored
  bool                        longFormatFlag;                 // TRUE iff long format list
  bool                        humanFormatFlag;                // TRUE iff human format list
  bool                        numericUIDGIDFlag;              // TRUE for printing numeric user/group ids
  bool                        numericPermissionsFlag;         // TRUE for printing numeric permissions
  bool                        noHeaderFooterFlag;             // TRUE iff no header/footer should be printed in list
  bool                        deleteOldArchiveFilesFlag;      // TRUE iff old archive files should be deleted after creating new files
  bool                        ignoreNoBackupFileFlag;         // TRUE iff .nobackup/.NOBACKUP file should be ignored
  bool                        noDefaultConfigFlag;            // TRUE iff default config should not be read
  bool                        forceDeltaCompressionFlag;      // TRUE to force delta compression of files
  bool                        ignoreNoDumpAttributeFlag;      // TRUE for ignoring no-dump attribute
  bool                        alwaysCreateImageFlag;          // TRUE iff always create image for CD/DVD/BD/device
  bool                        blankFlag;                      // TRUE to blank medium before writing
  bool                        rawImagesFlag;                  // TRUE for storing raw images
  bool                        noFragmentsCheckFlag;           // TRUE to skip checking file fragments for completeness
  bool                        noIndexDatabaseFlag;            // TRUE for do not store index database for archives
  bool                        forceVerifySignaturesFlag;      // TRUE to force verify signatures of archives
  bool                        skipVerifySignaturesFlag;       // TRUE to not verify signatures of archives
  bool                        noStorage;                      // TRUE to skip create storages
  bool                        noSignatureFlag;                // TRUE for not appending signatures
  bool                        noBAROnMediumFlag;              // TRUE for not storing BAR on medium
  bool                        noStopOnErrorFlag;              // TRUE for not stopping immediately on error
  bool                        noStopOnAttributeErrorFlag;     // TRUE for not stopping immediately on attribute error
  bool                        dryRun;                         // TRUE for dry-run only

  bool                        quietFlag;                      // TRUE iff suppress any output
  long                        verboseLevel;                   /* verbosity level
                                                                   0 - none
                                                                   1 - fatal errors
                                                                   2 - processing information
                                                                   3 - external programs
                                                                   4 - stdout+stderr of external programs
                                                                   5 - some SSH debug debug
                                                                   6 - all SSH/FTP/WebDAV debug
                                                              */

  // --- run options
  String                      jobUUIDOrName;                  // job to execute (name or UUID)

  String                      storageName;
  EntryList                   includeEntryList;               // included entries
  PatternList                 excludePatternList;             // excluded entry patterns

  const char                  *changeToDirectory;
  const char                  *importFileName;

  Commands                    command;
  uint                        generateKeyBits;
  uint                        generateKeyMode;

  ulong                       logTypes;
  const char                  *logFileName;
  const char                  *logFormat;
  const char                  *logPostCommand;

  const char                  *pidFileName;

  bool                        daemonFlag;
  bool                        noDetachFlag;
  bool                        batchFlag;
  bool                        versionFlag;
  bool                        helpFlag;
  bool                        xhelpFlag;
  bool                        helpInternalFlag;

  ServerModes                 serverMode;
  uint                        serverPort;
  uint                        serverTLSPort;
  Certificate                 serverCA;
  Certificate                 serverCert;
  Key                         serverKey;
  Hash                        serverPasswordHash;
  uint                        serverMaxConnections;

  // --- job options default values
  ArchiveTypes                archiveType;                    // archive type for create

  bool                        storageNameListStdin;           // read storage names from stdin
  String                      storageNameListFileName;        // storage names list file name
  String                      storageNameCommand;             // storage names command

  String                      includeFileListFileName;        // include files list file name
  String                      includeFileCommand;             // include files command
  String                      includeImageListFileName;       // include images list file name
  String                      includeImageCommand;            // include images command
  String                      excludeListFileName;            // exclude entries list file name
  String                      excludeCommand;                 // exclude entries command

  MountList                   mountList;                      // mount list
  String                      mountCommand;                   // mount command
  String                      mountDeviceCommand;             // mount device command
  String                      unmountCommand;                 // unmount command

  PatternList                 compressExcludePatternList;     // excluded compression patterns

  DeltaSourceList             deltaSourceList;                // delta sources

  uint64                      archivePartSize;                // archive part size [bytes]

  String                      incrementalListFileName;        // name of incremental list file

  Transform                   transform;                      // transform pattern+string
  int                         directoryStripCount;            // number of directories to strip in restore
  String                      destination;                    // destination for restore
  Owner                       owner;                          // restore owner
  FilePermission              permissions;                    // restore permissions

  PatternTypes                patternType;                    // pattern type

  uint64                      fragmentSize;                   // fragment size [bytes]
  uint64                      maxStorageSize;                 // max. storage size [bytes]
  uint64                      volumeSize;                     // volume size or 0LL for default [bytes]

  ulong                       compressMinFileSize;            // min. size of file for using compression
  uint64                      continuousMaxSize;              // max. entry size for continuous backup
  uint                        continuousMinTimeDelta;         // min. time between consequtive continuous backup of an entry [s]

  CompressAlgorithmsDeltaByte compressAlgorithms;             // compress algorithms delta/byte

  CryptTypes                  cryptType;                      // crypt type (symmetric, asymmetric)
  CryptAlgorithms             cryptAlgorithms[4];             // crypt algorithms to use
  PasswordModes               cryptPasswordMode;              // crypt password mode
  Password                    cryptPassword;                  // default crypt password if none set in job options
  Password                    cryptNewPassword;               // new crypt password
  Key                         cryptPublicKey;                 // crypt public key
  Key                         cryptPrivateKey;                // crypt private key (possible encryped with pass phrase, thus no CryptKey here)

#if 0
  CryptKey                    signaturePublicKey;             // signature public key (not encrypted)
  CryptKey                    signaturePrivateKey;            // signature private key (not encrypted)
#else
  Key                         signaturePublicKey;             // signature public key (not encrypted)
  Key                         signaturePrivateKey;            // signature private key (not encrypted)
#endif
  Server                      *fileServer;                    // current selected file server
  Server                      *ftpServer;                     // current selected FTP server
  Server                      *sshServer;                     // current selected SSH server
  Server                      *webDAVServer;                  // current selected WebDAV server
  Device                      *device;                        // current selected device

  String                      comment;                        // comment

  String                      remoteBARExecutable;

  String                      preProcessScript;               // script to execute before start of job
  String                      postProcessScript;              // script to execute after after termination of

  File                        file;                           // file settings
  FTP                         ftp;                            // ftp settings
  SCP                         scp;                            // scp settings
  SFTP                        sftp;                           // sftp settings
  WebDAV                      webdav;                         // WebDAV settings
  OpticalDisk                 cd;                             // CD settings
  OpticalDisk                 dvd;                            // DVD settings
  OpticalDisk                 bd;                             // BD settings

  ArchiveFileModes            archiveFileMode;                // archive files write mode
  RestoreEntryModes           restoreEntryMode;               // overwrite existing entry mode on restore
  bool                        sparseFlag;                     // TRUE to create sparse files

  bool                        testCreatedArchivesFlag;        // TRUE to simple test archives after creation

  bool                        skipUnreadableFlag;             // TRUE for skipping unreadable files
  bool                        errorCorrectionCodesFlag;       // TRUE iff error correction codes should be added
  bool                        waitFirstVolumeFlag;            // TRUE for wait for first volume

  const char                  *saveConfigurationFileName;     // configuration save file name

  // debug/test only
  #ifndef NDEBUG
  struct
  {
    uint                      createArchiveErrors;            // number of errors in created archive

    uint                      serverLevel;                    // server debug level (for debug only)
    bool                      serverFixedIdsFlag;             // always generate id=1
    String                    indexUUID;                      // index UUID
// TODO:
//    DatabaseId                indexEntityId;
    int64                     indexEntityId;
    bool                      indexWaitOperationsFlag;        // TRUE to wait for index operation
    bool                      indexPurgeDeletedStoragesFlag;  // TRUE to purge deleted storages
    String                    indexAddStorage;                // add storage to index
    String                    indexRemoveStorage;             // remove storage from index
    String                    indexRefreshStorage;            // refresh storage index

    bool                      printConfigurationSHA256;
  } debug;
  #endif /* NDEBUG */
} GlobalOptions;

// status info data
typedef struct
{
  struct
  {
    ulong     count;                                          // number of entries processed
    uint64    size;                                           // number of bytes processed
  } done;
  struct
  {
    ulong     count;                                          // total number of entries
    uint64    size;                                           // total size of entries [bytes]
  } total;
  bool   collectTotalSumDone;                                 // TRUE iff all file sums are collected
  struct
  {
    ulong     count;                                          // number of skipped entries
    uint64    size;                                           // sum of skipped bytes
  } skipped;
  struct
  {
    ulong     count;                                          // number of entries with errors
    uint64    size;                                           // sum of bytes of entries with errors
  } error;
  uint64 archiveSize;                                         // number of bytes stored in archive
  double compressionRatio;                                    // compression ratio
  struct
  {
    String    name;                                           // current entry name
    uint64    doneSize;                                       // number of bytes processed of current entry
    uint64    totalSize;                                      // total number of bytes of current entry
  } entry;
  struct
  {
    String    name;                                           // current storage name
    uint64    doneSize;                                       // number of bytes processed of current archive
    uint64    totalSize;                                      // total bytes of current archive
  } storage;
  struct
  {
    uint      number;                                         // current volume number
    double    progress;                                       // current volume progress [0..100]
  } volume;
  String message;                                             // last message
} StatusInfo;

/***********************************************************************\
* Name   : GetNamePasswordFunction
* Purpose: call-back to get name and password
* Input  : name          - name variable (can be NULL)
*          password      - password variable
*          passwordType  - password type
*          text          - text (file name, host name, etc.)
*          validateFlag  - TRUE to validate input, FALSE otherwise
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password)
*          userData      - user data
* Output : name     - name
*          password - password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*GetNamePasswordFunction)(String        name,
                                         Password      *password,
                                         PasswordTypes passwordType,
                                         const char    *text,
                                         bool          validateFlag,
                                         bool          weakCheckFlag,
                                         void          *userData
                                        );

/***********************************************************************\
* Name   : StatusInfoFunction
* Purpose: status info call-back
* Input  : error      - error code
*          statusInfo - create status info
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*StatusInfoFunction)(Errors           error,
                                  const StatusInfo *statusInfo,
                                  void             *userData
                                 );

/***********************************************************************\
* Name   : IsPauseFunction
* Purpose: call-back to check for pause
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef bool(*IsPauseFunction)(void *userData);

/***********************************************************************\
* Name   : IsAbortedFunction
* Purpose: call-back to check for aborted
* Input  : userData - user data
* Output : -
* Return : TRUE iff abort requested
* Notes  : -
\***********************************************************************/

typedef bool(*IsAbortedFunction)(void *userData);

/***************************** Variables *******************************/
extern String tmpDirectory;           // temporary directory

/****************************** Macros *********************************/

// get time from hour/minute
#define TIME(hour,minute) ((hour)*60+(minute))
// get begin time from hour/minute or 00:00
#define TIME_BEGIN(hour,minute) ((((hour)   != TIME_ANY) ? (uint)(hour  ) :  0)*60+ \
                                 (((minute) != TIME_ANY) ? (uint)(minute) :  0) \
                                )
// get end time from hour/minute or 24:00
#define TIME_END(hour,minute) ((((hour)   != TIME_ANY) ? (uint)(hour  ) : 23)*60+ \
                               (((minute) != TIME_ANY) ? (uint)(minute) : 60) \
                              )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Common_initAll
* Purpose: initialize common functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Common_initAll(void);

/***********************************************************************\
* Name   : Common_doneAll
* Purpose: deinitialize common functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Common_doneAll(void);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : templateInit
* Purpose: init template
* Input  : templateHandle  - template handle variable
*          templateString  - template string
*          expandMacroMode - expand macro mode
*          dateTime        - date/time [s] or 0
* Output : templateHandle  - template handle
* Return : -
* Notes  : -
\***********************************************************************/

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  uint64           dateTime
                 );

/***********************************************************************\
* Name   : templateMacros
* Purpose: add template macros
* Input  : templateHandle - template handle
*          textMacros     - macros array
*          textMacroCount - number of macros
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void templateMacros(TemplateHandle   *templateHandle,
                    const TextMacro  textMacros[],
                    uint             textMacroCount
                   );

/***********************************************************************\
* Name   : templateDone
* Purpose: template done
* Input  : templateHandle - template handle
*          string         - string variable (can be NULL)
* Output : -
* Return : expanded templated string
* Notes  : if string variable is NULL, new string is allocated and must
*          be freed!
\***********************************************************************/

String templateDone(TemplateHandle *templateHandle,
                    String         string
                   );

/***********************************************************************\
* Name   : expandTemplate
* Purpose: expand template
* Input  : templateString  - template string
*          expandMacroMode - expand macro mode
*          timestamp       - timestamp [s] or 0
*          textMacros      - macros array
*          textMacroCount  - number of macros
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

String expandTemplate(const char       *templateString,
                      ExpandMacroModes expandMacroMode,
                      time_t           timestamp,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     );

/***********************************************************************\
* Name   : executeTemplate
* Purpose: execute template as script
* Input  : templateString    - template string
*          timestamp         - timestamp [s] or 0
*          textMacros        - macros array
*          textMacroCount    - number of macros
*          executeIOFunction - execute I/O function
*          executeIOUserData - execute I/O function user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors executeTemplate(const char        *templateString,
                       time_t            timestamp,
                       const TextMacro   textMacros[],
                       uint              textMacroCount,
                       ExecuteIOFunction executeIOFunction,
                       void              *executeIOUserData
                      );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : parseBandWidthNumber
* Purpose: parse band width number
* Input  : s - string to parse
*          commandLineUnits
* Output : value - number variable
* Return : TRUE iff number parsed
* Notes  : -
\***********************************************************************/

bool parseBandWidthNumber(ConstString s, ulong *n);

/***********************************************************************\
* Name   : getBandWidth
* Purpose: get band width from value or external file
* Input  : bandWidthList - band width list settings or NULL
* Output : -
* Return : return band width [bits/s] or 0
* Notes  : -
\***********************************************************************/

ulong getBandWidth(BandWidthList *bandWidthList);

/***********************************************************************\
* Name   : inTimeRange
* Purpose: check if time is in time range
* Input  : hour,minute           - time
*          beginHour,beginMinute - begin time range
*          endHour,endMinute     - end time range
* Output : -
* Return : TRUE iff in time range
* Notes  : handle begin* > end*
\***********************************************************************/

bool inTimeRange(uint hour, uint minute, int beginHour, int beginMinute, int endHour, int endMinute);

#ifdef __cplusplus
  }
#endif

#endif /* __BAR_COMMON__ */

/* end of file */
