/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.h,v $
* $Revision: 1.36 $
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

#include "patterns.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

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

/* log types */
typedef enum
{
  LOG_TYPE_ALWAYS             = 0,
  LOG_TYPE_ERROR              = (1 <<  0),
  LOG_TYPE_WARNING            = (1 <<  1),
  LOG_TYPE_FILE_OK            = (1 <<  2),
  LOG_TYPE_FILE_TYPE_UNKNOWN  = (1 <<  3),
  LOG_TYPE_FILE_ACCESS_DENIED = (1 <<  4),
  LOG_TYPE_FILE_MISSING       = (1 <<  5),
  LOG_TYPE_FILE_INCOMPLETE    = (1 <<  6),
  LOG_TYPE_FILE_EXCLUDED      = (1 <<  7),
  LOG_TYPE_STORAGE            = (1 <<  8),
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

/***************************** Datatypes *******************************/

/* password mode */
typedef enum
{
  PASSWORD_MODE_DEFAULT,                // use global password
  PASSWORD_MODE_ASK,                    // ask for password
  PASSWORD_MODE_CONFIG,                 // use password from config
  PASSWORD_MODE_UNKNOWN,
} PasswordModes;

/* ssh server */
typedef struct
{
  uint     port;
  String   loginName;
  String   publicKeyFileName;
  String   privateKeyFileName;
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

/* dvd */
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
  String writeImageCommand;
} DVD;

/* device */
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

/* global options */
typedef struct
{
  const char          *barExecutable;                  // name of BAR executable

  uint                niceLevel;
  
  String              tmpDirectory;
  uint64              maxTmpSize;

  ulong               maxBandWidth;

  ulong               compressMinFileSize;

  Password            *cryptPassword;

  SSHServer           *sshServer;                      // SSH server
  const SSHServerList *sshServerList;                  // list with SSH servers
  SSHServer           defaultSSHServer;                // default SSH server

  String              remoteBARExecutable;

  DVD                 dvd;

  String              defaultDeviceName;
  Device              *device;                         // device
  const DeviceList    *deviceList;                     // list with devices
  Device              defaultDevice;                   // default device

  bool                noDefaultConfigFlag;             // do not read default config
  bool                quietFlag;
  long                verboseLevel;
} GlobalOptions;

/* schedule */
typedef struct ScheduleNode
{
  NODE_HEADER(struct ScheduleNode);

  int          year;
  int          month;
  int          day;
  int          hour;
  int          minute;
  int          weekDay;
  ArchiveTypes archiveType;
//  String       comment;
} ScheduleNode;

typedef struct
{
  LIST_HEADER(ScheduleNode);
} ScheduleList;

/* job options */
typedef struct
{
  ArchiveTypes        archiveType;                     // archive type (normal, full, incremental)

  uint64              archivePartSize;                 // archive part size [bytes]

  String              incrementalListFileName;         // name of incremental list file

  uint                directoryStripCount;             // number of directories to strip in restore
  String              directory;                       // restore destination directory

  PatternTypes        patternType;

  CompressAlgorithms  compressAlgorithm;
  CryptAlgorithms     cryptAlgorithm;
  PasswordModes       cryptPasswordMode;
  Password            *cryptPassword;

  SSHServer           sshServer;

  String              deviceName;
  Device              device;

  bool                skipUnreadableFlag;
  bool                overwriteArchiveFilesFlag;
  bool                overwriteFilesFlag;
  bool                errorCorrectionCodesFlag;
  bool                waitFirstVolumeFlag;
  bool                noStorageFlag;
  bool                noBAROnDVDFlag;                  // TRUE for not storing BAR on DVDs
  bool                stopOnErrorFlag;
} JobOptions;

/***************************** Variables *******************************/
extern GlobalOptions globalOptions;

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

#define DAY_ADD(days,day)    ((days) |=  (1 << (day)))
#define DAY_REM(days,day)    ((days) &= ~(1 << (day)))
#define DAY_TEST(days,day)   (((days) & (1 << (day))) != 0)

#define MONTH_ADD(months,month)  ((months) |=  (1 << (month)))
#define MONTH_REM(months,month)  ((months) &= ~(1 << (month)))
#define MONTH_TEST(months,month) (((months) &  (1 << (month))) != 0)

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
*          prefix    - prefix text
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
* Input  : sourceJobOptions      - source job options
*          destinationJobOptions - destination job options variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void copyJobOptions(const JobOptions *sourceJobOptions, JobOptions *destinationJobOptions);

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
* Name   : getSSHServer
* Purpose: get SSH server data
* Input  : name       - server name
*          jobOptions - job options
* Output : sshServer - SSH server data from server list or default
*                      server values
* Return : -
* Notes  : -
\***********************************************************************/

void getSSHServer(const String     name,
                  const JobOptions *jobOptions,
                  SSHServer        *sshServer
                 );

/***********************************************************************\
* Name   : getDevice
* Purpose: get device data
* Input  : name       - device name
*          jobOptions - job options
* Output : device - device data from devie list or default
*                   device values
* Return : -
* Notes  : -
\***********************************************************************/

void getDevice(const String     name,
               const JobOptions *jobOptions,
               Device           *device
              );

/***********************************************************************\
* Name   : inputCryptPassword
* Purpose: input crypt password
* Input  : cryptPassword - variable for crypt password (will be
*                          initialized if needed)
* Output : -
* Return : TRUE if passwort input ok, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool inputCryptPassword(Password **cryptPassword);

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

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
