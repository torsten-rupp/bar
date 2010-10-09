/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/bar.h,v $
* $Revision: 1.17 $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems: all
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
#include "configvalues.h"

#include "patterns.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "misc.h"
#include "database.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* file name extensions */
#define FILE_NAME_EXTENSION_ARCHIVE_FILE     ".bar"
#define FILE_NAME_EXTENSION_INCREMENTAL_FILE ".bid"

/* program exit codes */
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

/* run modes */
typedef enum
{
  RUN_MODE_INTERACTIVE,
  RUN_MODE_BATCH,
  RUN_MODE_SERVER,
} RunModes;

/* log types */
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
  LOG_TYPE_STORAGE             = (1 <<  8),
} LogTypes;

#define LOG_TYPE_NONE 0x00000000
#define LOG_TYPE_ALL  0xFFFFffff

/* archive types */
typedef enum
{
  ARCHIVE_TYPE_NORMAL,                  // normal archives; no incremental list file
  ARCHIVE_TYPE_FULL,                    // full archives, create incremental list file
  ARCHIVE_TYPE_INCREMENTAL,             // incremental achives
  ARCHIVE_TYPE_UNKNOWN,
} ArchiveTypes;

#define SCHEDULE_ANY -1
/*
#define SCHEDULE_ANY_MONTH \
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
#define SCHEDULE_ANY_DAY \
  (  SET_VALUE(WEEKDAY_MON) \
   | SET_VALUE(WEEKDAY_TUE) \
   | SET_VALUE(WEEKDAY_WED) \
   | SET_VALUE(WEEKDAY_THU) \
   | SET_VALUE(WEEKDAY_FRI) \
   | SET_VALUE(WEEKDAY_SAT) \
   | SET_VALUE(WEEKDAY_SUN) \
  )

/***************************** Datatypes *******************************/

/* password mode */
typedef enum
{
  PASSWORD_MODE_DEFAULT,                // use global password
  PASSWORD_MODE_ASK,                    // ask for password
  PASSWORD_MODE_CONFIG,                 // use password from config
  PASSWORD_MODE_UNKNOWN,
} PasswordModes;

/* FTP server settings */
typedef struct
{
  String   loginName;                   // login name
  Password *password;                   // login password
} FTPServer;

typedef struct FTPServerNode
{
  LIST_NODE_HEADER(struct FTPServerNode);

  String    name;                      // ftp server name
  FTPServer ftpServer;
} FTPServerNode;

typedef struct
{
  LIST_HEADER(FTPServerNode);
} FTPServerList;

/* SSH server settings */
typedef struct
{
  uint     port;                        // server port (ssh,scp,sftp)
  String   loginName;                   // login name
  Password *password;                   // login password
  String   publicKeyFileName;           // public key file name (ssh,scp,sftp)
  String   privateKeyFileName;          // private key file name (ssh,scp,sftp)
} SSHServer;

typedef struct SSHServerNode
{
  LIST_NODE_HEADER(struct SSHServerNode);

  String    name;                       // ssh server name
  SSHServer sshServer;
} SSHServerNode;

typedef struct
{
  LIST_HEADER(SSHServerNode);
} SSHServerList;

/* dvd settings */
typedef struct
{
  String requestVolumeCommand;          // command to request new dvd
  String unloadVolumeCommand;           // command to unload dvd
  String loadVolumeCommand;             // command to load dvd
  uint64 volumeSize;                    // size of dvd [bytes] (0 for default)

  String imagePreProcessCommand;        // command to execute before creating image
  String imagePostProcessCommand;       // command to execute after created image
  String imageCommand;                  // command to create dvd image
  String eccPreProcessCommand;          // command to execute before ECC calculation
  String eccPostProcessCommand;         // command to execute after ECC calculation
  String eccCommand;                    // command for ECC calculation
  String writePreProcessCommand;        // command to execute before writing dvd
  String writePostProcessCommand;       // command to execute after writing dvd
  String writeCommand;                  // command to write dvd
  String writeImageCommand;             // command to write image on dvd
} DVD;

/* device settings */
typedef struct
{
  String requestVolumeCommand;          // command to request new volume
  String unloadVolumeCommand;           // command to unload volume
  String loadVolumeCommand;             // command to load volume
  uint64 volumeSize;                    // size of volume [bytes]

  String imagePreProcessCommand;        // command to execute before creating image
  String imagePostProcessCommand;       // command to execute after created image
  String imageCommand;                  // command to create volume image
  String eccPreProcessCommand;          // command to execute before ECC calculation
  String eccPostProcessCommand;         // command to execute after ECC calculation
  String eccCommand;                    // command for ECC calculation
  String writePreProcessCommand;        // command to execute before writing volume
  String writePostProcessCommand;       // command to execute after writing volume
  String writeCommand;                  // command to write volume
} Device;

typedef struct DeviceNode
{
  LIST_NODE_HEADER(struct DeviceNode);

  String name;                          // device name
  Device device;
} DeviceNode;

typedef struct
{
  LIST_HEADER(DeviceNode);
} DeviceList;

/* global options */
typedef struct
{
  RunModes               runMode;

  const char             *barExecutable;                 // name of BAR executable

  uint                   niceLevel;
  
  String                 tmpDirectory;                   // directory for temporary files
  uint64                 maxTmpSize;                     // max. size of temporary files

  ulong                  maxBandWidth;                   // max. bandwidth to use [bytes/s]

  ulong                  compressMinFileSize;            // min. size of file for using compression

  Password               *cryptPassword;                 // default password for encryption/decryption

  FTPServer              *ftpServer;                     // current selected FTP server
  const FTPServerList    *ftpServerList;                 // list with remote servers
  FTPServer              *defaultFTPServer;              // default FTP server

  SSHServer              *sshServer;                     // current selected SSH server
  const SSHServerList    *sshServerList;                 // list with remote servers
  SSHServer              *defaultSSHServer;              // default SSH server

  String                 remoteBARExecutable;

  DVD                    dvd;                            // DVD settings

  String                 defaultDeviceName;              // default device name
  Device                 *device;                        // current selected device
  const DeviceList       *deviceList;                    // list with devices
  Device                 *defaultDevice;                 // default device

  bool                   noAutoUpdateDatabaseIndexFlag;  // TRUE for no automatic update of datbase indizes

  bool                   groupFlag;                      // TRUE iff entries in list should be grouped
  bool                   allFlag;                        // TRUE iff all entries should be listed/restored
  bool                   longFormatFlag;                 // TRUE iff long format list
  bool                   humanFormatFlag;                // TRUE iff human format list
  bool                   noHeaderFooterFlag;             // TRUE iff no header/footer should be printed in list
  bool                   deleteOldArchiveFilesFlag;      // TRUE iff old archive files should be deleted after creating new files

  bool                   noDefaultConfigFlag;            // TRUE iff default config should not be read 
  bool                   quietFlag;                      // TRUE iff suppress any output
  long                   verboseLevel;
} GlobalOptions;

/* schedule */
typedef struct ScheduleNode
{
  LIST_NODE_HEADER(struct ScheduleNode);

  int          year;
  int          month;
  int          day;
  int          hour;
  int          minute;
  ulong        weekDays;
  ArchiveTypes archiveType;
  bool         enabled;
} ScheduleNode;

typedef struct
{
  LIST_HEADER(ScheduleNode);
} ScheduleList;

/* job options */
typedef struct
{
  uint32              userId;                            // user id
  uint32              groupId;                           // group id 
} Owner;

typedef struct
{
  ArchiveTypes        archiveType;                       // archive type (normal, full, incremental)

  uint64              archivePartSize;                   // archive part size [bytes]

  String              incrementalListFileName;           // name of incremental list file

  uint                directoryStripCount;               // number of directories to strip in restore
  String              destination     ;                  // destination for restore
  Owner               owner;                             // restore owner

  PatternTypes        patternType;

  CompressAlgorithms  compressAlgorithm;                 // compress algorithm to use
  CryptTypes          cryptType;                         // crypt type (symmetric, asymmetric)
  CryptAlgorithms     cryptAlgorithm;                    // crypt algorithm to use
  PasswordModes       cryptPasswordMode;                 // crypt password mode
  Password            *cryptPassword;                    // crypt password
  String              cryptPublicKeyFileName;
  String              cryptPrivateKeyFileName;

  FTPServer           ftpServer;                         // job specific FTP server settings
  SSHServer           sshServer;                         // job specific SSH server settings

  DVD                 dvd;                               // job specific DVD settings

  String              deviceName;                        // device name to use
  Device              device;                            // job specific device settings

  uint64              volumeSize;                        // volume size or 0LL for default [bytes]

  bool                skipUnreadableFlag;                // TRUE for skipping unreadable files
  bool                overwriteArchiveFilesFlag;
  bool                overwriteFilesFlag;
  bool                errorCorrectionCodesFlag;
  bool                waitFirstVolumeFlag;
  bool                rawImagesFlag;                     // TRUE for storing raw images
  bool                noStorageFlag;
  bool                noBAROnMediumFlag;                 // TRUE for not storing BAR on medium
  bool                stopOnErrorFlag;
} JobOptions;

/***************************** Variables *******************************/
extern GlobalOptions  globalOptions;
extern String         tmpDirectory;
extern DatabaseHandle *indexDatabaseHandle;

/****************************** Macros *********************************/

/* return short number of bytes */
#define BYTES_SHORT(n) (((n)>(1024LL*1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>       (1024LL*1024LL))?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        ((n)>                1024LL)?(double)(n)/(double)(1024LL*1024LL*1024LL): \
                        (double)(n) \
                       )
/* return unit for short number of bytes */
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
* Name   : vprintInfo, printInfo
* Purpose: output info
* Input  : verboseLevel - verbosity level
*          prefix       - prefix text
*          format       - format string (like printf)
*          arguments    - arguments
*          ...          - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments);
void printInfo(uint verboseLevel, const char *format, ...);

/***********************************************************************\
* Name   : vlogMessage, logMessage
* Purpose: log message
*          logType   - log type; see LOG_TYPES_*
*          prefix    - prefix text
*          text      - format string (like printf)
*          arguments - arguments
*          ...       - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void vlogMessage(ulong logType, const char *prefix, const char *text, va_list arguments);
void logMessage(ulong logType, const char *text, ...);

/***********************************************************************\
* Name   : outputConsole
* Purpose: output to console
* Input  : format       - format string (like printf)
*          ...          - optional arguments (like printf)
*          arguments    - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printConsole(const char *format, ...);

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
* Name   : logPostProcess
* Purpose: log post processing
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void logPostProcess(void);

/***********************************************************************\
* Name   : initJobOptions
* Purpose: init job options structure
* Input  : jobOptions - job options variable
* Output : jobOptions - initialized job options variable
* Return : -
* Notes  : -
\***********************************************************************/

void initJobOptions(JobOptions *jobOptions);

/***********************************************************************\
* Name   : copyJobOptions
* Purpose: copy job options structure
* Input  : fromJobOptions - source job options
*          toJobOptions   - destination job options variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void copyJobOptions(const JobOptions *fromJobOptions, JobOptions *toJobOptions);

/***********************************************************************\
* Name   : freeJobOptions
* Purpose: free job options
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeJobOptions(JobOptions *jobOptions);

/***********************************************************************\
* Name   : getFTPServerSettings
* Purpose: get FTP server settings
* Input  : name       - FTP server name
*          jobOptions - job options
* Output : ftperver   - FTP server settings from job options, server
*                       list or default FTP server values
* Return : -
* Notes  : -
\***********************************************************************/

void getFTPServerSettings(const String     name,
                          const JobOptions *jobOptions,
                          FTPServer        *ftpServer
                         );

/***********************************************************************\
* Name   : getSSHServerSettings
* Purpose: get SSH server settings
* Input  : name       - SSH server name
*          jobOptions - job options
* Output : sshServer  - SSH server settings from job options, server
*                       list or default SSH server values
* Return : -
* Notes  : -
\***********************************************************************/

void getSSHServerSettings(const String     name,
                          const JobOptions *jobOptions,
                          SSHServer        *sshServer
                         );

/***********************************************************************\
* Name   : getDVDSettings
* Purpose: get DVD settings
* Input  : jobOptions - job options
* Output : dvd - dvd settings from job options or default DVD values
* Return : -
* Notes  : -
\***********************************************************************/

void getDVDSettings(const JobOptions *jobOptions,
                    DVD              *dvd
                   );

/***********************************************************************\
* Name   : getDeviceSettings
* Purpose: get device settings
* Input  : name       - device name
*          jobOptions - job options
* Output : device - device settings from job options, device list or
*                   default device values
* Return : -
* Notes  : -
\***********************************************************************/

void getDeviceSettings(const String     name,
                       const JobOptions *jobOptions,
                       Device           *device
                      );

/***********************************************************************\
* Name   : inputCryptPassword
* Purpose: input crypt password
* Input  : userData      - (not used)
*          password      - crypt password variable
*          fileName      - file name
*          validateFlag  - TRUE to validate input, FALSE otherwise
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password)
* Output : password - crypt password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors inputCryptPassword(void         *userData,
                          Password     *password,
                          const String fileName,
                          bool         validateFlag,
                          bool         weakCheckFlag
                         );

/***********************************************************************\
* Name   : configValueParseOwner
* Purpose: config value option call back for parsing owner
*          patterns
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseOwner(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitOwner
* Purpose: init format of config owner statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitOwner(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneOwner
* Purpose: done format of config owner statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneOwner(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatOwner
* Purpose: format next config owner statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatOwner(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseFileEntry, configValueParseImageEntry
* Purpose: config value option call back for parsing include/exclude
*          patterns
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseFileEntry(void *userData, void *variable, const char *name, const char *value);
bool configValueParseImageEntry(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitEntry
* Purpose: init format of config include statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitEntry(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneEntry
* Purpose: done format of config include statements
* Input  : formatUserData - format user data
*          userData       - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneEntry(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatFileEntry, configValueFormatImageEntry
* Purpose: format next config include statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatFileEntry(void **formatUserData, void *userData, String line);
bool configValueFormatImageEntry(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseIncludeExclude
* Purpose: config value option call back for parsing include/exclude
*          patterns
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseIncludeExclude(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitIncludeExclude
* Purpose: init format of config include/exclude statements
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitIncludeExclude(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneIncludeExclude
* Purpose: done format of config include/exclude statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneIncludeExclude(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatIncludeExclude
* Purpose: format next config include/exclude statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Notes  : -
\***********************************************************************/

bool configValueFormatIncludeExclude(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : configValueParseString
* Purpose: config value option call back for parsing string
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseString(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueParsePassword
* Purpose: config value option call back for parsing password
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParsePassword(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitPassord
* Purpose: init format config password
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitPassord(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatPassword
* Purpose: format password config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatPassword(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : parseSchedule
* Purpose: parse schedule
* Input  : s            - string
* Output : 
* Return : scheduleNode or NULL on error
* Notes  : -
\***********************************************************************/

ScheduleNode *parseSchedule(const String s);

/***********************************************************************\
* Name   : configValueParseSchedule
* Purpose: config value option call back for parsing schedule
* Input  : userData - user data
*          variable - config variable
*          name     - config name
*          value    - config value
* Output : -
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueParseSchedule(void *userData, void *variable, const char *name, const char *value);

/***********************************************************************\
* Name   : configValueFormatInitSchedule
* Purpose: init format config schedule
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatInitSchedule(void **formatUserData, void *userData, void *variable);

/***********************************************************************\
* Name   : configValueFormatDoneSchedule
* Purpose: done format of config schedule statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void configValueFormatDoneSchedule(void **formatUserData, void *userData);

/***********************************************************************\
* Name   : configValueFormatSchedule
* Purpose: format schedule config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

bool configValueFormatSchedule(void **formatUserData, void *userData, String line);

/***********************************************************************\
* Name   : readJobFile
* Purpose: read job from file
* Input  : fileName         - file name
*          configValues     - job configuration values
*          configValueCount - number of job configuration values
*          configData       - configuration data
* Output : -
* Return : TRUE iff job read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

bool readJobFile(const String      fileName,
                 const ConfigValue *configValues,
                 uint              configValueCount,
                 void              *configData
                );

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
